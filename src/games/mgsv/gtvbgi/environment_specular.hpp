// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

#ifndef SRC_GAMES_MGSV_GTVBGI_ENVIRONMENT_SPECULAR_HPP_
#define SRC_GAMES_MGSV_GTVBGI_ENVIRONMENT_SPECULAR_HPP_

#include <d3d11.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <utility>
#include <vector>

#include <include/reshade.hpp>

#include "../../../utils/data.hpp"
#include "../inputs.hpp"

namespace mgsv::lighting::environment_specular {

// Keep captured environment color and resolved visibility paired with one
// native specular target. Deferred composition must use that pair, rather than
// whichever camera or filter output was most recently observed on the CPU.
struct Target {
  uint64_t generation = 0u;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> visibility;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> visibility_srv;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> visibility_uav;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> material;
  Microsoft::WRL::ComPtr<ID3D11Resource> depth;
};

// The key is the native specular accumulation allocation, not a frame or context
// ID. Native clears reset the extra attachment in the same GPU command stream.
inline renodx::utils::data::ParallelFlatHashMap<ID3D11Resource*, std::shared_ptr<Target>, std::shared_mutex> targets;

struct __declspec(uuid("E86B6354-AF43-4D25-9CDE-C72A331B8D91")) DrawState {
  std::array<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> render_targets;
  Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth;
  Microsoft::WRL::ComPtr<ID3D11BlendState> blend;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> original_shader;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> capture_shader;
  std::array<float, 4> blend_factor = {};
  UINT sample_mask = UINT32_MAX;
  bool active = false;
  std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2> composite_inputs;
  bool composite_active = false;
};

struct CaptureBlend {
  Microsoft::WRL::ComPtr<ID3D11BlendState> original;
  Microsoft::WRL::ComPtr<ID3D11BlendState> capture;
};

struct __declspec(uuid("A71B9530-42A6-4E39-8635-19ECA852BD70")) DeviceState {
  std::shared_mutex mutex;
  std::vector<CaptureBlend> blends;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> capture_shader;
};

inline void OnInitDevice(reshade::api::device* device) {
  if (device->get_api() == reshade::api::device_api::d3d11) {
    device->create_private_data<DeviceState>();
  }
}

inline void OnDestroyDevice(reshade::api::device* device) {
  if (device->get_api() == reshade::api::device_api::d3d11) {
    device->destroy_private_data<DeviceState>();
  }
}

inline std::shared_ptr<Target> Read(reshade::api::device* device, ID3D11Resource* specular) {
  const auto selected = inputs::Read(device);
  if (selected.device == nullptr || specular == nullptr) return {};
  std::shared_ptr<Target> result;
  targets.if_contains(specular, [&](const auto& entry) {
    const auto& [native_specular, capture] = entry;
    if (capture != nullptr && capture->generation == selected.generation) {
      result = capture;
    }
  });
  return result;
}

inline void OnInitCommandList(reshade::api::command_list* cmd_list) {
  cmd_list->create_private_data<DrawState>();
}

inline void OnDestroyCommandList(reshade::api::command_list* cmd_list) {
  cmd_list->destroy_private_data<DrawState>();
}

inline void OnResetCommandList(reshade::api::command_list* cmd_list) {
  auto* saved = cmd_list->get_private_data<DrawState>();
  if (saved != nullptr) {
    *saved = {};
  }
}

inline void BeginCapture(reshade::api::command_list* cmd_list, bool lighting_enabled) {
  // 1. Qualify the existing light draw before adding an attachment. Native
  // diffuse/specular outputs and their additive blend must remain unchanged.
  if (!lighting_enabled) return;
  const auto selected = inputs::Read(cmd_list->get_device());
  if (selected.device == nullptr) return;
  auto* device_state = selected.device->get_private_data<DeviceState>();
  if (device_state == nullptr) return;
  auto* saved = cmd_list->get_private_data<DrawState>();
  if (saved == nullptr || saved->active) return;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> render_targets = {};
  DrawState state;
  UINT class_instance_count = 0u;
  context->PSGetShader(&state.original_shader, nullptr, &class_instance_count);
  if (state.original_shader == nullptr || class_instance_count != 0u) return;
  context->OMGetRenderTargets(static_cast<UINT>(render_targets.size()), render_targets.data(), &state.depth);
  for (uint32_t index = 0u; index < render_targets.size(); ++index) {
    state.render_targets[index].Attach(render_targets[index]);
  }
  if (render_targets[0] == nullptr || render_targets[1] == nullptr
      || std::ranges::any_of(render_targets.begin() + 2, render_targets.end(), [](const auto* view) { return view != nullptr; })) return;
  std::array<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>, D3D11_PS_CS_UAV_REGISTER_COUNT> output_uavs;
  std::array<ID3D11UnorderedAccessView*, D3D11_PS_CS_UAV_REGISTER_COUNT> native_uavs = {};
  context->OMGetRenderTargetsAndUnorderedAccessViews(0u, nullptr, nullptr, 0u, static_cast<UINT>(native_uavs.size()), native_uavs.data());
  for (uint32_t index = 0u; index < native_uavs.size(); ++index) {
    output_uavs[index].Attach(native_uavs[index]);
  }
  if (std::ranges::any_of(native_uavs, [](const auto* view) { return view != nullptr; })) return;

  context->OMGetBlendState(&state.blend, state.blend_factor.data(), &state.sample_mask);
  if (state.blend == nullptr) return;
  Microsoft::WRL::ComPtr<ID3D11BlendState> capture_blend;
  {
    const std::shared_lock lock(device_state->mutex);
    for (const auto& blend : device_state->blends) {
      if (blend.original == state.blend) {
        capture_blend = blend.capture;
        break;
      }
    }
  }
  D3D11_BLEND_DESC blend_description = {};
  if (capture_blend == nullptr) {
    state.blend->GetDesc(&blend_description);
    for (uint32_t index = 0u; index < 2u; ++index) {
      const auto& blend = blend_description.RenderTarget[blend_description.IndependentBlendEnable != FALSE ? index : 0u];
      if (blend.BlendEnable == FALSE || blend.SrcBlend != D3D11_BLEND_ONE
          || blend.DestBlend != D3D11_BLEND_ONE || blend.BlendOp != D3D11_BLEND_OP_ADD
          || blend.RenderTargetWriteMask != D3D11_COLOR_WRITE_ENABLE_ALL) return;
    }
  }
  Microsoft::WRL::ComPtr<ID3D11Resource> specular;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> specular_texture;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> material;
  render_targets[1]->GetResource(&specular);
  if (FAILED(specular.As(&specular_texture))) return;
  D3D11_TEXTURE2D_DESC description = {};
  D3D11_RENDER_TARGET_VIEW_DESC specular_view = {};
  specular_texture->GetDesc(&description);
  render_targets[1]->GetDesc(&specular_view);
  // MGSV uses typeless storage with typed floating-point lighting views.
  if ((description.Format != DXGI_FORMAT_R16G16B16A16_FLOAT && description.Format != DXGI_FORMAT_R16G16B16A16_TYPELESS)
      || specular_view.Format != DXGI_FORMAT_R16G16B16A16_FLOAT
      || specular_view.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D || specular_view.Texture2D.MipSlice != 0u
      || description.ArraySize != 1u
      || description.MipLevels != 1u || description.SampleDesc.Count != 1u) return;
  std::array<ID3D11ShaderResourceView*, 2> native_inputs = {};
  context->PSGetShaderResources(1u, static_cast<UINT>(native_inputs.size()), native_inputs.data());
  material.Attach(native_inputs[0]);
  depth.Attach(native_inputs[1]);
  if (material == nullptr || depth == nullptr) return;
  Microsoft::WRL::ComPtr<ID3D11Resource> depth_resource;
  depth->GetResource(&depth_resource);
  // 2. Reuse a full-size capture tied to this native specular allocation.
  // Capture RGB is linear pre-exposed environment light; alpha marks validity.
  // The paired RG16F image will hold resolved visibility and confidence.
  auto target = Read(selected.device, specular.Get());
  if (target == nullptr) {
    auto created = std::make_shared<Target>();
    created->generation = selected.generation;
    created->material = material;
    created->depth = depth_resource;
    description.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    description.CPUAccessFlags = 0u;
    description.MiscFlags = 0u;
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    context->GetDevice(&device);
    if (FAILED(device->CreateTexture2D(&description, nullptr, &created->texture))
        || FAILED(device->CreateRenderTargetView(created->texture.Get(), nullptr, &created->rtv))
        || FAILED(device->CreateShaderResourceView(created->texture.Get(), nullptr, &created->srv))) {
      static std::atomic_flag reported = {};
      if (!reported.test_and_set(std::memory_order_relaxed)) {
        reshade::log::message(reshade::log::level::warning, "MgsvLighting: environment specular capture allocation failed; keeping native lighting");
      }
      return;
    }
    description.Format = DXGI_FORMAT_R16G16_FLOAT;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    if (FAILED(device->CreateTexture2D(&description, nullptr, &created->visibility))
        || FAILED(device->CreateShaderResourceView(created->visibility.Get(), nullptr, &created->visibility_srv))
        || FAILED(device->CreateUnorderedAccessView(created->visibility.Get(), nullptr, &created->visibility_uav))) {
      static std::atomic_flag reported = {};
      if (!reported.test_and_set(std::memory_order_relaxed)) {
        reshade::log::message(reshade::log::level::warning, "MgsvLighting: specular visibility allocation failed; keeping native occlusion");
      }
      return;
    }
    const std::array<float, 4> empty = {};
    context->ClearRenderTargetView(created->rtv.Get(), empty.data());
    context->ClearUnorderedAccessViewFloat(created->visibility_uav.Get(), empty.data());
    std::shared_ptr<Target> retired;
    targets.lazy_emplace_l(
        specular.Get(),
        [&](auto& entry) {
          auto& [native_specular, capture] = entry;
          if (capture == nullptr || capture->generation != selected.generation) {
            // Release retired COM views after leaving the map's lock.
            retired = std::exchange(capture, created);
          }
          target = capture;
        },
        [&](const auto& insert) {
          insert(specular.Get(), created);
          target = created;
        });
  }
  if (target->generation != selected.generation || target->depth.Get() != depth_resource.Get()
      || target->material.Get() != material.Get()) return;

  // 3. Add environment RGB at target 2 while replacing its validity alpha.
  // Targets 0/1 retain their native blend. EndCapture restores every displaced
  // render target, depth view, blend state, and pixel shader after this draw.
  if (capture_blend == nullptr) {
    if (blend_description.IndependentBlendEnable == FALSE) {
      for (auto& blend : blend_description.RenderTarget) {
        blend = blend_description.RenderTarget[0];
      }
    }
    blend_description.IndependentBlendEnable = TRUE;
    blend_description.RenderTarget[2] = {
        .BlendEnable = TRUE,
        .SrcBlend = D3D11_BLEND_ONE,
        .DestBlend = D3D11_BLEND_ONE,
        .BlendOp = D3D11_BLEND_OP_ADD,
        .SrcBlendAlpha = D3D11_BLEND_ONE,
        .DestBlendAlpha = D3D11_BLEND_ZERO,
        .BlendOpAlpha = D3D11_BLEND_OP_ADD,
        .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
    };
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    context->GetDevice(&device);
    if (FAILED(device->CreateBlendState(&blend_description, &capture_blend))) return;
    Microsoft::WRL::ComPtr<ID3D11BlendState> existing;
    {
      const std::unique_lock lock(device_state->mutex);
      for (const auto& blend : device_state->blends) {
        if (blend.original == state.blend) {
          existing = blend.capture;
          break;
        }
      }
      if (existing == nullptr) {
        device_state->blends.push_back({.original = state.blend, .capture = capture_blend});
      }
    }
    if (existing != nullptr) {
      capture_blend = std::move(existing);
    }
  }
  render_targets[2] = target->rtv.Get();
  state.active = true;
  *saved = std::move(state);
  context->OMSetRenderTargets(3u, render_targets.data(), saved->depth.Get());
  context->OMSetBlendState(capture_blend.Get(), saved->blend_factor.data(), saved->sample_mask);
  static std::atomic_flag reported = {};
  if (!reported.test_and_set(std::memory_order_relaxed)) {
    reshade::log::message(reshade::log::level::info, "MgsvLighting: environment specular capture commands recorded");
  }
}

inline void EndCapture(reshade::api::command_list* cmd_list) {
  auto* saved = cmd_list->get_private_data<DrawState>();
  if (saved == nullptr || !saved->active) return;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> render_targets = {};
  for (uint32_t index = 0u; index < render_targets.size(); ++index) {
    render_targets[index] = saved->render_targets[index].Get();
  }
  context->OMSetRenderTargets(static_cast<UINT>(render_targets.size()), render_targets.data(), saved->depth.Get());
  context->OMSetBlendState(saved->blend.Get(), saved->blend_factor.data(), saved->sample_mask);
  // The next draw can reject capture without rebinding its native pixel shader.
  context->PSSetShader(saved->original_shader.Get(), nullptr, 0u);
  *saved = {};
}

inline reshade::api::pipeline GetShadowedCapturePipeline(reshade::api::command_list* cmd_list, std::span<const uint8_t> bytecode) {
  auto* saved = cmd_list->get_private_data<DrawState>();
  if (saved == nullptr || !saved->active) return {0u};
  auto* device_state = cmd_list->get_device()->get_private_data<DeviceState>();
  if (device_state == nullptr) {
    EndCapture(cmd_list);
    return {0u};
  }
  {
    const std::shared_lock lock(device_state->mutex);
    saved->capture_shader = device_state->capture_shader;
  }
  if (saved->capture_shader == nullptr) {
    auto* device = reinterpret_cast<ID3D11Device*>(cmd_list->get_device()->get_native());  // NOLINT(performance-no-int-to-ptr)
    Microsoft::WRL::ComPtr<ID3D11PixelShader> created;
    if (FAILED(device->CreatePixelShader(bytecode.data(), bytecode.size(), nullptr, &created))) {
      EndCapture(cmd_list);
      return {0u};
    }
    {
      const std::unique_lock lock(device_state->mutex);
      if (device_state->capture_shader == nullptr) {
        device_state->capture_shader = created;
      }
      saved->capture_shader = device_state->capture_shader;
    }
  }
  return {reinterpret_cast<uintptr_t>(saved->capture_shader.Get())};
}

inline void BeginComposite(reshade::api::command_list* cmd_list) {
  // 4. Match deferred composition's native specular/depth inputs, then bind
  // only their capture and visibility pair. EndComposite restores t16/t17.
  auto* saved = cmd_list->get_private_data<DrawState>();
  if (saved == nullptr || saved->composite_active) return;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  std::array<ID3D11ShaderResourceView*, 2> previous = {};
  context->PSGetShaderResources(16u, static_cast<UINT>(previous.size()), previous.data());
  for (uint32_t index = 0u; index < previous.size(); ++index) {
    saved->composite_inputs[index].Attach(previous[index]);
  }
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> specular;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth;
  Microsoft::WRL::ComPtr<ID3D11Resource> specular_resource;
  Microsoft::WRL::ComPtr<ID3D11Resource> depth_resource;
  std::array<ID3D11ShaderResourceView*, 2> native_inputs = {};
  context->PSGetShaderResources(10u, static_cast<UINT>(native_inputs.size()), native_inputs.data());
  specular.Attach(native_inputs[0]);
  depth.Attach(native_inputs[1]);
  if (specular != nullptr) {
    specular->GetResource(&specular_resource);
  }
  if (depth != nullptr) {
    depth->GetResource(&depth_resource);
  }
  const auto target = Read(cmd_list->get_device(), specular_resource.Get());
  std::array<ID3D11ShaderResourceView*, 2> inputs = {};
  if (target != nullptr && target->depth.Get() == depth_resource.Get()) {
    inputs = {target->srv.Get(), target->visibility_srv.Get()};
    static std::atomic_flag reported = {};
    if (!reported.test_and_set(std::memory_order_relaxed)) {
      reshade::log::message(reshade::log::level::info, "MgsvLighting: environment specular composition inputs recorded; zero confidence retains native occlusion");
    }
  }
  // Missing capture binds null inputs, never arbitrary displaced game textures.
  context->PSSetShaderResources(16u, static_cast<UINT>(inputs.size()), inputs.data());
  saved->composite_active = true;
}

inline void EndComposite(reshade::api::command_list* cmd_list) {
  auto* saved = cmd_list->get_private_data<DrawState>();
  if (saved == nullptr || !saved->composite_active) return;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  const std::array<ID3D11ShaderResourceView*, 2> previous = {saved->composite_inputs[0].Get(), saved->composite_inputs[1].Get()};
  context->PSSetShaderResources(16u, static_cast<UINT>(previous.size()), previous.data());
  saved->composite_inputs = {};
  saved->composite_active = false;
}

inline bool OnClearRenderTarget(reshade::api::command_list* cmd_list, reshade::api::resource_view rtv,
                                const float* color, uint32_t rect_count, const reshade::api::rect* rects) {
  (void)color;
  (void)rects;
  (void)rect_count;
  if (rtv.handle == 0u || cmd_list->get_device()->get_api() != reshade::api::device_api::d3d11) return false;
  auto* view = reinterpret_cast<ID3D11RenderTargetView*>(rtv.handle);  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11Resource> resource;
  view->GetResource(&resource);
  const auto target = Read(cmd_list->get_device(), resource.Get());
  if (target != nullptr) {
    // Invalidate color and visibility beside the native clear in GPU order.
    // An old capture must not survive a new lighting accumulation or replay.
    const std::array<float, 4> empty = {};
    auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
    context->ClearRenderTargetView(target->rtv.Get(), empty.data());
    context->ClearUnorderedAccessViewFloat(target->visibility_uav.Get(), empty.data());
  }
  return false;
}

inline void OnDestroyResource(reshade::api::device* device, reshade::api::resource resource) {
  (void)device;
  std::shared_ptr<Target> retired;
  targets.erase_if(reinterpret_cast<ID3D11Resource*>(resource.handle), [&](auto& entry) {  // NOLINT(performance-no-int-to-ptr)
    auto& [native_specular, capture] = entry;
    retired = std::move(capture);
    return true;
  });
}

inline void Reset() {
  std::vector<std::shared_ptr<Target>> retired;
  targets.for_each_m([&](auto& entry) {
    auto& [native_specular, capture] = entry;
    retired.push_back(std::move(capture));
  });
  targets.clear();
}

}  // namespace mgsv::lighting::environment_specular

#endif  // SRC_GAMES_MGSV_GTVBGI_ENVIRONMENT_SPECULAR_HPP_
