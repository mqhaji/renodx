#ifndef MGSV_SOURCE_PREPARATION_HPP_
#define MGSV_SOURCE_PREPARATION_HPP_

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

#include "../../../../utils/data.hpp"
#include "../../materials/capture.hpp"
#include "../../rendering/inputs.hpp"
#include "environment_specular.hpp"

namespace mgsv::lighting::source_preparation {

struct Target {
  uint64_t generation = 0u;
  uint32_t width = 0u;
  uint32_t height = 0u;
  // Original diffuse, material, albedo, normal, depth, clean color, owner.
  // These are immutable binding identities, not snapshots of their contents.
  std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 7> inputs;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
};

// The copy allocation is a weak key. Targets do not retain that allocation,
// so native destruction can retire preparation without an ownership cycle.
inline renodx::utils::data::ParallelFlatHashMap<ID3D11Resource*, std::shared_ptr<Target>, std::shared_mutex> targets;

struct __declspec(uuid("2F9198D3-B6CC-42D7-8DD2-9048B09840F8")) DrawState {
  std::array<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> outputs;
  std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 5> displaced;
  Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;
  Microsoft::WRL::ComPtr<ID3D11BlendState> blend;
  std::array<float, 4> blend_factor = {};
  UINT sample_mask = UINT32_MAX;
  std::shared_ptr<Target> target;
};

inline std::shared_ptr<Target> Read(reshade::api::device* device, ID3D11Resource* copied) {
  const auto selected = inputs::ReadSelection(device);
  std::shared_ptr<Target> result;
  if (selected.device == nullptr || copied == nullptr) return result;
  targets.if_contains(copied, [&](const auto& entry) {
    const auto& [native_copy, target] = entry;
    if (target->generation == selected.generation) {
      result = target;
    }
  });
  return result;
}

inline void OnInitCommandList(reshade::api::command_list* cmd_list) {
  cmd_list->create_private_data<DrawState>();
}

inline void OnResetCommandList(reshade::api::command_list* cmd_list) {
  if (auto* state = cmd_list->get_private_data<DrawState>()) {
    *state = {};
  }
}

inline void OnDestroyCommandList(reshade::api::command_list* cmd_list) {
  cmd_list->destroy_private_data<DrawState>();
}

inline bool IsActive(reshade::api::command_list* cmd_list) {
  const auto* saved = cmd_list->get_private_data<DrawState>();
  return saved != nullptr && saved->target != nullptr;
}

inline bool Begin(reshade::api::command_list* cmd_list) {
  if (cmd_list->get_device()->get_api() != reshade::api::device_api::d3d11) return true;
  const auto selected = inputs::Read(cmd_list->get_device());
  auto* saved = cmd_list->get_private_data<DrawState>();
  if (selected.device == nullptr || saved == nullptr || saved->target != nullptr
      || selected.materials.generation != selected.generation) return true;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  auto* device = reinterpret_cast<ID3D11Device*>(selected.device->get_native());   // NOLINT(performance-no-int-to-ptr)

  // 1. Match the native copy to its material producer. Unsupported layouts
  // keep the original shader and publish no prepared source for this copy.
  DrawState state;
  std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> outputs = {};
  context->OMGetRenderTargets(static_cast<UINT>(outputs.size()), outputs.data(), &state.depth);
  for (size_t index = 0u; index < outputs.size(); ++index) {
    state.outputs[index].Attach(outputs[index]);
  }
  if (outputs[0] == nullptr || state.depth != nullptr
      || std::ranges::any_of(outputs.begin() + 1, outputs.end(), [](auto* view) { return view != nullptr; })) return true;
  std::array<ID3D11UnorderedAccessView*, D3D11_PS_CS_UAV_REGISTER_COUNT> uavs = {};
  context->OMGetRenderTargetsAndUnorderedAccessViews(0u, nullptr, nullptr, 0u, static_cast<UINT>(uavs.size()), uavs.data());
  bool has_uav = false;
  for (auto* view : uavs) {
    if (view != nullptr) {
      has_uav = true;
      view->Release();
    }
  }
  if (has_uav) return true;
  Microsoft::WRL::ComPtr<ID3D11Resource> copied;
  outputs[0]->GetResource(&copied);
  std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 7> sources;
  std::array<ID3D11ShaderResourceView*, 2> native_sources = {};
  context->PSGetShaderResources(0u, 2u, native_sources.data());
  sources[0].Attach(native_sources[0]);
  sources[1].Attach(native_sources[1]);
  if (sources[0] == nullptr || sources[1] == nullptr || selected.materials.albedo == nullptr || selected.materials.specular == nullptr) return true;
  Microsoft::WRL::ComPtr<ID3D11Resource> albedo;
  Microsoft::WRL::ComPtr<ID3D11Resource> specular;
  selected.materials.albedo->GetResource(&albedo);
  selected.materials.specular->GetResource(&specular);
  const auto clean = material_inputs::Read(selected.device, albedo.Get());
  const auto environment = environment_specular::Read(selected.device, specular.Get());
  if (clean == nullptr || environment == nullptr || environment->material == nullptr
      || environment->normal == nullptr || environment->depth_view == nullptr) return true;
  if (environment->material != sources[1]) {
    Microsoft::WRL::ComPtr<ID3D11Resource> captured;
    Microsoft::WRL::ComPtr<ID3D11Resource> current;
    environment->material->GetResource(&captured);
    sources[1]->GetResource(&current);
    D3D11_SHADER_RESOURCE_VIEW_DESC captured_view = {};
    D3D11_SHADER_RESOURCE_VIEW_DESC current_view = {};
    environment->material->GetDesc(&captured_view);
    sources[1]->GetDesc(&current_view);
    if (captured != current || captured_view.Format != current_view.Format) return true;
  }
  sources[2] = selected.materials.albedo;
  sources[3] = environment->normal;
  sources[4] = environment->depth_view;
  sources[5] = clean->color_srv;
  sources[6] = clean->owner_srv;
  auto target = Read(selected.device, copied.Get());
  const bool same_inputs = target != nullptr && target->inputs == sources;
  D3D11_TEXTURE2D_DESC description = {
      .Width = clean->width, .Height = clean->height, .MipLevels = 1u, .ArraySize = 1u, .Format = DXGI_FORMAT_R32G32B32A32_FLOAT, .SampleDesc = {.Count = 1u}, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE};
  D3D11_RENDER_TARGET_VIEW_DESC output_view = {};
  outputs[0]->GetDesc(&output_view);
  if (output_view.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D || output_view.Texture2D.MipSlice != 0u
      || output_view.Format != DXGI_FORMAT_R16G16B16A16_FLOAT) return true;
  // Native view descriptors cannot change. Qualify the seven-input set only
  // when a binding changes; draw coverage and submission still need live checks.
  if (!same_inputs) {
    D3D11_SHADER_RESOURCE_VIEW_DESC albedo_view = {};
    sources[2]->GetDesc(&albedo_view);
    if (albedo_view.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
        && albedo_view.Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) return true;
    D3D11_TEXTURE2D_DESC output_desc = {};
    const auto output_texture = inputs::internal::ReadTexture(outputs[0], device, &output_desc);
    if (output_texture == nullptr || output_desc.Width != description.Width || output_desc.Height != description.Height
        || output_desc.ArraySize != 1u || output_desc.MipLevels != 1u || output_desc.SampleDesc.Count != 1u) return true;
    for (const auto& source : sources) {
      D3D11_TEXTURE2D_DESC source_desc = {};
      const auto texture = inputs::internal::ReadTexture(source.Get(), device, &source_desc);
      if (texture == nullptr || texture == output_texture || source_desc.Width != description.Width || source_desc.Height != description.Height
          || source_desc.ArraySize != 1u || source_desc.MipLevels != 1u || source_desc.SampleDesc.Count != 1u) return true;
      D3D11_SHADER_RESOURCE_VIEW_DESC view = {};
      source->GetDesc(&view);
      if (view.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D || view.Texture2D.MostDetailedMip != 0u) return true;
    }
  }
  UINT viewport_count = 1u;
  D3D11_VIEWPORT viewport = {};
  context->RSGetViewports(&viewport_count, &viewport);
  if (viewport_count != 1u || viewport.TopLeftX != 0.f || viewport.TopLeftY != 0.f
      || viewport.Width != static_cast<float>(description.Width) || viewport.Height != static_cast<float>(description.Height)) return true;
  Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer;
  context->RSGetState(&rasterizer);
  if (rasterizer != nullptr) {
    D3D11_RASTERIZER_DESC rasterizer_desc = {};
    rasterizer->GetDesc(&rasterizer_desc);
    if (rasterizer_desc.ScissorEnable != FALSE) {
      UINT count = 1u;
      D3D11_RECT scissor = {};
      context->RSGetScissorRects(&count, &scissor);
      if (count != 1u || scissor.left > 0 || scissor.top > 0
          || scissor.right < static_cast<LONG>(description.Width) || scissor.bottom < static_cast<LONG>(description.Height)) return true;
    }
  }
  // The native copy must replace both outputs. Do not reinterpret additive or
  // partial writes as a complete source-light image.
  context->OMGetBlendState(&state.blend, state.blend_factor.data(), &state.sample_mask);
  Microsoft::WRL::ComPtr<ID3D11BlendState> capture_blend;
  if (state.blend != nullptr) {
    // Reuse the material-capture blend cache. Only RT0 belongs to this native
    // draw; the extra attachment always receives a complete replacement.
    auto* device_state = selected.device->get_private_data<material_inputs::DeviceState>();
    if (device_state == nullptr) return true;
    {
      const std::shared_lock lock(device_state->mutex);
      for (const auto& entry : device_state->blends) {
        if (entry.original == state.blend && entry.native_count == 1u) {
          capture_blend = entry.capture;
          break;
        }
      }
    }
    if (capture_blend == nullptr) {
      D3D11_BLEND_DESC blend_desc = {};
      state.blend->GetDesc(&blend_desc);
      if (blend_desc.RenderTarget[0].BlendEnable != FALSE
          || blend_desc.RenderTarget[0].RenderTargetWriteMask != D3D11_COLOR_WRITE_ENABLE_ALL) return true;
      blend_desc.IndependentBlendEnable = TRUE;
      blend_desc.RenderTarget[1] = blend_desc.RenderTarget[0];
      if (FAILED(device->CreateBlendState(&blend_desc, &capture_blend))) return true;
      const std::unique_lock lock(device_state->mutex);
      device_state->blends.push_back({.original = state.blend, .capture = capture_blend, .opaque = true, .native_count = 1u});
    }
  }
  UINT class_count = 0u;
  context->PSGetShader(&state.shader, nullptr, &class_count);
  if (state.shader == nullptr || class_count != 0u) return true;

  // 2. Reuse one FP32 image per native copy and binding set. Keeping the
  // multiply in FP32 avoids adding an FP16 clamp or rounding to source light.
  if (!same_inputs) {
    auto created = std::make_shared<Target>();
    created->generation = selected.generation;
    created->width = description.Width;
    created->height = description.Height;
    created->inputs = sources;
    if (FAILED(device->CreateTexture2D(&description, nullptr, &created->texture))
        || FAILED(device->CreateRenderTargetView(created->texture.Get(), nullptr, &created->rtv))
        || FAILED(device->CreateShaderResourceView(created->texture.Get(), nullptr, &created->srv))) {
      static std::atomic_flag reported = {};
      if (!reported.test_and_set(std::memory_order_relaxed)) {
        reshade::log::message(reshade::log::level::warning, "MgsvLighting: source preparation allocation failed; using trace source loads");
      }
      return true;
    }
    std::shared_ptr<Target> retired;
    targets.lazy_emplace_l(copied.Get(), [&](auto& entry) {
      auto& [native_copy, previous] = entry;
      retired = std::exchange(previous, created); }, [&](const auto& insert) { insert(copied.Get(), created); });
    target = std::move(created);
    static std::atomic_flag reported = {};
    if (!reported.test_and_set(std::memory_order_relaxed)) {
      reshade::log::message(reshade::log::level::info, "MgsvLighting: FP32 source radiance attached to native CopyBuffer; no added dispatch");
    }
  }

  // 3. Add one attachment to the existing draw and restore its bindings after
  // replacement. The native output, geometry, depth, and blend state stay intact.
  std::array<ID3D11ShaderResourceView*, 5> displaced = {};
  context->PSGetShaderResources(18u, static_cast<UINT>(displaced.size()), displaced.data());
  for (size_t index = 0u; index < displaced.size(); ++index) {
    state.displaced[index].Attach(displaced[index]);
  }
  const std::array<ID3D11ShaderResourceView*, 5> extra = {
      sources[2].Get(), sources[3].Get(), sources[4].Get(), sources[5].Get(), sources[6].Get()};
  state.target = target;
  *saved = std::move(state);
  outputs[1] = target->rtv.Get();
  context->OMSetRenderTargets(2u, outputs.data(), saved->depth.Get());
  context->OMSetBlendState(capture_blend.Get(), saved->blend_factor.data(), saved->sample_mask);
  context->PSSetShaderResources(18u, static_cast<UINT>(extra.size()), extra.data());
  return true;
}

inline Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> End(reshade::api::command_list* cmd_list) {
  auto* saved = cmd_list->get_private_data<DrawState>();
  if (saved == nullptr || saved->target == nullptr) return {};
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> outputs = {};
  std::array<ID3D11ShaderResourceView*, 5> displaced = {};
  for (size_t index = 0u; index < outputs.size(); ++index) {
    outputs[index] = saved->outputs[index].Get();
  }
  for (size_t index = 0u; index < displaced.size(); ++index) {
    displaced[index] = saved->displaced[index].Get();
  }
  context->OMSetRenderTargets(static_cast<UINT>(outputs.size()), outputs.data(), saved->depth.Get());
  context->OMSetBlendState(saved->blend.Get(), saved->blend_factor.data(), saved->sample_mask);
  context->PSSetShaderResources(18u, static_cast<UINT>(displaced.size()), displaced.data());
  context->PSSetShader(saved->shader.Get(), nullptr, 0u);
  auto prepared = saved->target->srv;
  *saved = {};
  return prepared;
}

inline void OnDestroyResource(reshade::api::device* /*device*/, reshade::api::resource resource) {
  std::shared_ptr<Target> retired;
  targets.erase_if(reinterpret_cast<ID3D11Resource*>(resource.handle), [&](auto& entry) {  // NOLINT(performance-no-int-to-ptr)
    auto& [native_copy, target] = entry;
    retired = std::move(target);
    return true;
  });
}

inline void Reset() {
  std::vector<std::shared_ptr<Target>> retired;
  targets.for_each_m([&](auto& entry) {
    auto& [native_copy, target] = entry;
    retired.push_back(std::move(target));
  });
  targets.clear();
}

}  // namespace mgsv::lighting::source_preparation

#endif  // MGSV_SOURCE_PREPARATION_HPP_
