// Separate native material color from the cubemap approximation without
// changing the game's G-buffer. Native outputs remain the complete fallback.
#ifndef MGSV_MATERIAL_INPUTS_HPP_
#define MGSV_MATERIAL_INPUTS_HPP_

#include <d3d11.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

#include <include/reshade.hpp>

#include "../../../utils/data.hpp"
#include "../../../utils/mutex.hpp"
#include "../gtvbgi/specular_result.hpp"
#include "../inputs.hpp"
#include "../shared.h"

namespace mgsv::lighting::material_inputs {

inline const ShaderInjectData* controls = nullptr;

struct Target {
  uint64_t generation = 0u;
  uint32_t width = 0u;
  uint32_t height = 0u;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> color;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> color_rtv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> color_srv;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> owner;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> owner_rtv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> owner_srv;
};

// The key is weak. The extra images retain neither the native albedo allocation
// nor its RTV, so destroying that allocation can retire its captured inputs.
inline renodx::utils::data::ParallelFlatHashMap<ID3D11Resource*, std::shared_ptr<Target>, std::shared_mutex> targets;

struct Blend {
  Microsoft::WRL::ComPtr<ID3D11BlendState> original;
  Microsoft::WRL::ComPtr<ID3D11BlendState> capture;
  bool opaque = false;
  uint32_t native_count = 0u;
};

struct __declspec(uuid("45157F10-3393-4BC7-ADB1-197E2C1781AF")) DeviceState {
  std::shared_mutex mutex;
  std::vector<Blend> blends;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> parameter_view;
};

struct __declspec(uuid("48FB7998-7A69-4399-98F5-429824095270")) DrawState {
  std::array<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> targets;
  Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth;
  Microsoft::WRL::ComPtr<ID3D11BlendState> blend;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;
  std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 3> parameters;
  std::array<float, 4> blend_factor = {};
  UINT sample_mask = UINT32_MAX;
  bool active = false;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> reflection_input;
  bool reflection_active = false;
};

inline std::shared_ptr<Target> Read(reshade::api::device* device, ID3D11Resource* albedo) {
  const auto selected = inputs::ReadSelection(device);
  std::shared_ptr<Target> result;
  if (selected.device == nullptr || albedo == nullptr) return result;
  targets.if_contains(albedo, [&](const auto& entry) {
    const auto& [native_albedo, capture] = entry;
    if (capture != nullptr && capture->generation == selected.generation) {
      result = capture;
    }
  });
  return result;
}

inline void OnInitDevice(reshade::api::device* device) {
  if (device->get_api() != reshade::api::device_api::d3d11) return;
  auto* state = device->create_private_data<DeviceState>();
  const D3D11_TEXTURE2D_DESC description = {
      .Width = 1u,
      .Height = 1u,
      .MipLevels = 1u,
      .ArraySize = 1u,
      .Format = DXGI_FORMAT_R32_UINT,
      .SampleDesc = {.Count = 1u},
      .Usage = D3D11_USAGE_IMMUTABLE,
      .BindFlags = D3D11_BIND_SHADER_RESOURCE,
  };
  auto* native = reinterpret_cast<ID3D11Device*>(device->get_native());  // NOLINT(performance-no-int-to-ptr)
  const uint32_t value = 1u;
  const D3D11_SUBRESOURCE_DATA data = {.pSysMem = &value, .SysMemPitch = sizeof(value)};
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  if (FAILED(native->CreateTexture2D(&description, &data, &texture))
      || FAILED(native->CreateShaderResourceView(texture.Get(), nullptr, &state->parameter_view))) {
    reshade::log::message(reshade::log::level::error, "MgsvMaterials: failed to create capture parameters; keeping native inputs");
  }
}

inline void OnDestroyDevice(reshade::api::device* device) {
  if (device->get_api() == reshade::api::device_api::d3d11) {
    device->destroy_private_data<DeviceState>();
  }
}

inline void OnInitCommandList(reshade::api::command_list* cmd_list) {
  cmd_list->create_private_data<DrawState>();
}

inline void OnDestroyCommandList(reshade::api::command_list* cmd_list) {
  cmd_list->destroy_private_data<DrawState>();
}

inline void OnResetCommandList(reshade::api::command_list* cmd_list) {
  auto* state = cmd_list->get_private_data<DrawState>();
  if (state != nullptr) {
    *state = {};
  }
}

// Called only for registered G-buffer or wet-material draws.
inline bool BeginCapture(reshade::api::command_list* cmd_list, bool rain) {
  if (controls == nullptr || cmd_list->get_device()->get_api() != reshade::api::device_api::d3d11) return true;
  {
    const std::shared_lock lock(renodx::utils::mutex::global_mutex);
    if (controls->gt_enabled < 0.5f) return true;
  }
  const auto selected = inputs::ReadSelection(cmd_list->get_device());
  auto* saved = cmd_list->get_private_data<DrawState>();
  auto* device_state = cmd_list->get_device()->get_private_data<DeviceState>();
  if (selected.device == nullptr || saved == nullptr || saved->active || device_state == nullptr
      || device_state->parameter_view == nullptr) return true;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());        // NOLINT(performance-no-int-to-ptr)
  auto* device = reinterpret_cast<ID3D11Device*>(cmd_list->get_device()->get_native());  // NOLINT(performance-no-int-to-ptr)

  // 1. Qualify the real G-buffer layout. A shader hash alone cannot establish
  // which scene, dimensions, output formats, or depth target it is drawing into.
  const uint32_t native_count = (rain ? 2u : 3u);
  DrawState state;
  std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> bound = {};
  context->OMGetRenderTargets(static_cast<UINT>(bound.size()), bound.data(), &state.depth);
  for (size_t index = 0u; index < bound.size(); ++index) {
    state.targets[index].Attach(bound[index]);
  }
  if ((!rain && state.depth == nullptr)
      || std::ranges::any_of(bound.begin(), bound.begin() + native_count, [](const auto* view) { return view == nullptr; })
      || std::ranges::any_of(bound.begin() + native_count, bound.end(), [](const auto* view) { return view != nullptr; })) return true;
  std::array<Microsoft::WRL::ComPtr<ID3D11Resource>, 3> resources;
  D3D11_TEXTURE2D_DESC albedo_desc = {};
  for (size_t index = 0u; index < native_count; ++index) {
    D3D11_RENDER_TARGET_VIEW_DESC view = {};
    bound[index]->GetDesc(&view);
    if (view.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D || view.Texture2D.MipSlice != 0u
        || (view.Format != DXGI_FORMAT_B8G8R8A8_UNORM && view.Format != DXGI_FORMAT_R8G8B8A8_UNORM)) return true;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    bound[index]->GetResource(&resources[index]);
    if (FAILED(resources[index].As(&texture))) return true;
    D3D11_TEXTURE2D_DESC description = {};
    texture->GetDesc(&description);
    if (description.ArraySize != 1u || description.SampleDesc.Count != 1u || description.MipLevels != 1u) return true;
    if (index == 0u) {
      albedo_desc = description;
    } else if (description.Width != albedo_desc.Width || description.Height != albedo_desc.Height) {
      return true;
    }
  }
  if (state.depth != nullptr) {
    D3D11_DEPTH_STENCIL_VIEW_DESC depth_view = {};
    state.depth->GetDesc(&depth_view);
    if (depth_view.Format != DXGI_FORMAT_D32_FLOAT_S8X24_UINT && depth_view.Format != DXGI_FORMAT_D32_FLOAT) return true;
  }
  std::shared_ptr<Target> rain_source;
  if (rain) {
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> source;
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    context->PSGetShaderResources(8u, 1u, &source);
    if (source == nullptr) return true;
    source->GetResource(&resource);
    if (resource == resources[0]) return true;
    rain_source = Read(cmd_list->get_device(), resource.Get());
    if (rain_source == nullptr || rain_source->width != albedo_desc.Width || rain_source->height != albedo_desc.Height) return true;
  }
  std::array<ID3D11UnorderedAccessView*, D3D11_PS_CS_UAV_REGISTER_COUNT> native_uavs = {};
  context->OMGetRenderTargetsAndUnorderedAccessViews(0u, nullptr, nullptr, 0u, static_cast<UINT>(native_uavs.size()), native_uavs.data());
  bool has_uav = false;
  for (auto* view : native_uavs) {
    if (view != nullptr) {
      has_uav = true;
      view->Release();
    }
  }
  if (has_uav) return true;
  UINT class_count = 0u;
  context->PSGetShader(&state.shader, nullptr, &class_count);
  if (state.shader == nullptr || class_count != 0u) return true;

  // 2. Preserve native blending. Only a full opaque write can publish clean
  // material data; decals and partial writes explicitly invalidate their pixels.
  context->OMGetBlendState(&state.blend, state.blend_factor.data(), &state.sample_mask);
  Blend blend;
  {
    const std::shared_lock lock(device_state->mutex);
    for (const auto& entry : device_state->blends) {
      if (entry.original == state.blend && entry.native_count == native_count) {
        blend = entry;
        break;
      }
    }
  }
  if (blend.capture == nullptr) {
    D3D11_BLEND_DESC description = {};
    if (state.blend != nullptr) {
      state.blend->GetDesc(&description);
    } else {
      description.RenderTarget[0] = {.BlendEnable = FALSE, .SrcBlend = D3D11_BLEND_ONE, .DestBlend = D3D11_BLEND_ZERO, .BlendOp = D3D11_BLEND_OP_ADD, .SrcBlendAlpha = D3D11_BLEND_ONE, .DestBlendAlpha = D3D11_BLEND_ZERO, .BlendOpAlpha = D3D11_BLEND_OP_ADD, .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL};
    }
    if (description.IndependentBlendEnable == FALSE) {
      std::fill(description.RenderTarget + 1, description.RenderTarget + D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
                description.RenderTarget[0]);
    }
    blend.original = state.blend;
    blend.native_count = native_count;
    blend.opaque = std::all_of(description.RenderTarget, description.RenderTarget + native_count, [](const auto& entry) {
      return entry.BlendEnable == FALSE && entry.RenderTargetWriteMask == D3D11_COLOR_WRITE_ENABLE_ALL;
    });
    description.IndependentBlendEnable = TRUE;
    for (size_t index = native_count; index < native_count + 4u; ++index) {
      description.RenderTarget[index] = {.BlendEnable = FALSE, .SrcBlend = D3D11_BLEND_ONE, .DestBlend = D3D11_BLEND_ZERO, .BlendOp = D3D11_BLEND_OP_ADD, .SrcBlendAlpha = D3D11_BLEND_ONE, .DestBlendAlpha = D3D11_BLEND_ZERO, .BlendOpAlpha = D3D11_BLEND_OP_ADD, .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL};
    }
    if (FAILED(device->CreateBlendState(&description, &blend.capture))) return true;
    const std::unique_lock lock(device_state->mutex);
    device_state->blends.push_back(blend);
  }

  const auto report_failure = [] {
    static std::atomic_flag reported = {};
    if (!reported.test_and_set(std::memory_order_relaxed)) {
      reshade::log::message(reshade::log::level::warning, "MgsvMaterials: capture attachment creation failed; keeping native reflections");
    }
  };

  // 3. Reuse two attachments per native albedo allocation.
  // Native color/normal/material codes and exact depth reject stale data from an
  // unsupported foreground writer instead of guessing from material ID or alpha.
  auto target = Read(cmd_list->get_device(), resources[0].Get());
  if (target == nullptr) {
    auto created = std::make_shared<Target>();
    created->generation = selected.generation;
    created->width = albedo_desc.Width;
    created->height = albedo_desc.Height;
    D3D11_TEXTURE2D_DESC description = {
        .Width = albedo_desc.Width,
        .Height = albedo_desc.Height,
        .MipLevels = 1u,
        .ArraySize = 1u,
        .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
        .SampleDesc = {.Count = 1u},
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
    };
    if (FAILED(device->CreateTexture2D(&description, nullptr, &created->color))
        || FAILED(device->CreateRenderTargetView(created->color.Get(), nullptr, &created->color_rtv))
        || FAILED(device->CreateShaderResourceView(created->color.Get(), nullptr, &created->color_srv))) {
      report_failure();
      return true;
    }
    description.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    if (FAILED(device->CreateTexture2D(&description, nullptr, &created->owner))
        || FAILED(device->CreateRenderTargetView(created->owner.Get(), nullptr, &created->owner_rtv))
        || FAILED(device->CreateShaderResourceView(created->owner.Get(), nullptr, &created->owner_srv))) {
      report_failure();
      return true;
    }
    const std::array<float, 4> invalid = {0.f, 0.f, 0.f, -1.f};
    const std::array<float, 4> empty = {};
    context->ClearRenderTargetView(created->color_rtv.Get(), invalid.data());
    context->ClearRenderTargetView(created->owner_rtv.Get(), empty.data());
    std::shared_ptr<Target> retired;
    targets.lazy_emplace_l(resources[0].Get(), [&](auto& entry) {
      auto& [native_albedo, capture] = entry;
      if (capture == nullptr || capture->generation != selected.generation) {
        retired = std::exchange(capture, created);
      }
      target = capture; }, [&](const auto& insert) { insert(resources[0].Get(), created); target = created; });
    static std::atomic_flag reported = {};
    if (!reported.test_and_set(std::memory_order_relaxed)) {
      reshade::log::message(reshade::log::level::info, "MgsvMaterials: separate linear material color and native owner guides created");
    }
  }
  if (target->width != albedo_desc.Width || target->height != albedo_desc.Height) return true;

  auto result = specular_result::Read(cmd_list->get_device(), resources[native_count - 1u].Get());
  if (result == nullptr) {
    auto created = std::make_shared<specular_result::Target>();
    created->generation = selected.generation;
    created->width = albedo_desc.Width;
    created->height = albedo_desc.Height;
    D3D11_TEXTURE2D_DESC description = {
        .Width = albedo_desc.Width,
        .Height = albedo_desc.Height,
        .MipLevels = 1u,
        .ArraySize = 1u,
        .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
        .SampleDesc = {.Count = 1u},
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
    };
    const std::array<float, 4> empty = {};
    for (auto& image : created->images) {
      if (FAILED(device->CreateTexture2D(&description, nullptr, &image.texture))
          || FAILED(device->CreateRenderTargetView(image.texture.Get(), nullptr, &image.rtv))
          || FAILED(device->CreateShaderResourceView(image.texture.Get(), nullptr, &image.srv))
          || FAILED(device->CreateUnorderedAccessView(image.texture.Get(), nullptr, &image.uav))) {
        report_failure();
        return true;
      }
      context->ClearRenderTargetView(image.rtv.Get(), empty.data());
    }
    description.Format = DXGI_FORMAT_R8_UNORM;
    description.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device->CreateTexture2D(&description, nullptr, &created->composed.texture))
        || FAILED(device->CreateRenderTargetView(created->composed.texture.Get(), nullptr, &created->composed.rtv))
        || FAILED(device->CreateShaderResourceView(created->composed.texture.Get(), nullptr, &created->composed.srv))) {
      report_failure();
      return true;
    }
    context->ClearRenderTargetView(created->composed.rtv.Get(), empty.data());
    std::shared_ptr<specular_result::Target> retired;
    specular_result::targets.lazy_emplace_l(resources[native_count - 1u].Get(), [&](auto& entry) {
      auto& [native_material, capture] = entry;
      if (capture == nullptr || capture->generation != selected.generation) {
        retired = std::exchange(capture, created);
      }
      result = capture; }, [&](const auto& insert) { insert(resources[native_count - 1u].Get(), created); result = created; });
  }

  // 4. Blended decals invalidate their captured inputs instead of treating
  // their unblended source as the final material. Native blending stays intact.
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> parameters = device_state->parameter_view;
  if (!blend.opaque) {
    parameters.Reset();  // Null reads zero: publish invalid guides for this draw.
  }

  // 5. Attach the extra outputs without disturbing native RGB, alpha, blending,
  // depth, or shader resources. Restore every displaced binding after the draw.
  std::array<ID3D11ShaderResourceView*, 3> previous = {};
  context->PSGetShaderResources(18u, static_cast<UINT>(previous.size()), previous.data());
  for (size_t index = 0u; index < previous.size(); ++index) {
    state.parameters[index].Attach(previous[index]);
  }
  state.active = true;
  *saved = std::move(state);
  bound[native_count] = target->color_rtv.Get();
  bound[native_count + 1u] = target->owner_rtv.Get();
  bound[native_count + 2u] = result->images[0].rtv.Get();
  bound[native_count + 3u] = result->composed.rtv.Get();
  // Every native G-buffer write resets reflected-color readiness in GPU order.
  // If the later lighting chain fails, the original SSR remains enabled.
  context->OMSetRenderTargets(native_count + 4u, bound.data(), saved->depth.Get());
  context->OMSetBlendState(blend.capture.Get(), saved->blend_factor.data(), saved->sample_mask);
  const std::array<ID3D11ShaderResourceView*, 3> inputs = {
      rain ? rain_source->color_srv.Get() : nullptr,
      rain ? rain_source->owner_srv.Get() : nullptr, parameters.Get()};
  context->PSSetShaderResources(18u, static_cast<UINT>(inputs.size()), inputs.data());
  return true;
}

inline bool OnMaterialDraw(reshade::api::command_list* cmd_list) {
  return BeginCapture(cmd_list, false);
}

inline bool OnRainDraw(reshade::api::command_list* cmd_list) {
  return BeginCapture(cmd_list, true);
}

inline bool IsCaptureActive(reshade::api::command_list* cmd_list) {
  const auto* state = cmd_list->get_private_data<DrawState>();
  return state != nullptr && state->active;
}

inline void EndCapture(reshade::api::command_list* cmd_list) {
  auto* saved = cmd_list->get_private_data<DrawState>();
  if (saved == nullptr || !saved->active) return;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> bound = {};
  for (size_t index = 0u; index < bound.size(); ++index) {
    bound[index] = saved->targets[index].Get();
  }
  context->OMSetRenderTargets(static_cast<UINT>(bound.size()), bound.data(), saved->depth.Get());
  context->OMSetBlendState(saved->blend.Get(), saved->blend_factor.data(), saved->sample_mask);
  std::array<ID3D11ShaderResourceView*, 3> previous = {};
  for (size_t index = 0u; index < previous.size(); ++index) {
    previous[index] = saved->parameters[index].Get();
  }
  context->PSSetShaderResources(18u, static_cast<UINT>(previous.size()), previous.data());
  context->PSSetShader(saved->shader.Get(), nullptr, 0u);
  *saved = {};
}

inline bool OnReflectionComposite(reshade::api::command_list* cmd_list) {
  auto* saved = cmd_list->get_private_data<DrawState>();
  if (saved == nullptr || saved->reflection_active) return true;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> material;
  Microsoft::WRL::ComPtr<ID3D11Resource> resource;
  context->PSGetShaderResources(2u, 1u, &material);
  if (material != nullptr) {
    material->GetResource(&resource);
  }
  const auto result = specular_result::Read(cmd_list->get_device(), resource.Get());
  auto* view = (result != nullptr ? result->composed.srv.Get() : nullptr);
  context->PSGetShaderResources(20u, 1u, &saved->reflection_input);
  context->PSSetShaderResources(20u, 1u, &view);
  saved->reflection_active = true;
  return true;
}

inline void EndReflections(reshade::api::command_list* cmd_list) {
  auto* saved = cmd_list->get_private_data<DrawState>();
  if (saved == nullptr || !saved->reflection_active) return;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  context->PSSetShaderResources(20u, 1u, saved->reflection_input.GetAddressOf());
  saved->reflection_input.Reset();
  saved->reflection_active = false;
}

inline bool OnClearRenderTarget(reshade::api::command_list* cmd_list, reshade::api::resource_view rtv,
                                const float* color, uint32_t rect_count, const reshade::api::rect* rects) {
  if (rtv.handle == 0u || cmd_list->get_device()->get_api() != reshade::api::device_api::d3d11) return false;
  Microsoft::WRL::ComPtr<ID3D11Resource> resource;
  reinterpret_cast<ID3D11RenderTargetView*>(rtv.handle)->GetResource(&resource);  // NOLINT(performance-no-int-to-ptr)
  // Material clears retire prepared and composed reflection readiness beside
  // the native command. Albedo clears independently invalidate clean color.
  if (const auto result = specular_result::Read(cmd_list->get_device(), resource.Get())) {
    const std::array<float, 4> empty = {};
    auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
    context->ClearRenderTargetView(result->images[0].rtv.Get(), empty.data());
    context->ClearRenderTargetView(result->composed.rtv.Get(), empty.data());
  }
  const auto target = Read(cmd_list->get_device(), resource.Get());
  if (target != nullptr) {
    const std::array<float, 4> invalid = {0.f, 0.f, 0.f, -1.f};
    const std::array<float, 4> empty = {};
    auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
    context->ClearRenderTargetView(target->color_rtv.Get(), invalid.data());
    context->ClearRenderTargetView(target->owner_rtv.Get(), empty.data());
  }
  return false;
}

inline void OnDestroyResource(reshade::api::device* device, reshade::api::resource resource) {
  std::shared_ptr<Target> retired;
  targets.erase_if(reinterpret_cast<ID3D11Resource*>(resource.handle), [&](auto& entry) {  // NOLINT(performance-no-int-to-ptr)
    auto& [native_albedo, target] = entry;
    retired = std::move(target);
    return true;
  });
  std::shared_ptr<specular_result::Target> retired_result;
  specular_result::targets.erase_if(reinterpret_cast<ID3D11Resource*>(resource.handle), [&](auto& entry) {  // NOLINT(performance-no-int-to-ptr)
    auto& [native_material, target] = entry;
    retired_result = std::move(target);
    return true;
  });
}

inline void Reset() {
  std::vector<std::shared_ptr<Target>> retired;
  targets.for_each_m([&](auto& entry) {
    auto& [native_albedo, target] = entry;
    retired.push_back(std::move(target));
  });
  targets.clear();
  std::vector<std::shared_ptr<specular_result::Target>> retired_results;
  specular_result::targets.for_each_m([&](auto& entry) {
    auto& [native_material, target] = entry;
    retired_results.push_back(std::move(target));
  });
  specular_result::targets.clear();
}

}  // namespace mgsv::lighting::material_inputs

#endif  // MGSV_MATERIAL_INPUTS_HPP_
