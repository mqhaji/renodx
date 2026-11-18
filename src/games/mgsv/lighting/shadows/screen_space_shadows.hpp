// SPDX-License-Identifier: MIT
#pragma once

#include <d3d11_1.h>
#include <wrl/client.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <type_traits>

#include <include/reshade.hpp>

#include "../../reconstruction/projection_jitter.hpp"
#include "../../reconstruction/resolution.hpp"
#include "../../rendering/inputs.hpp"
#include "../../shared.h"
#include "batch.hpp"
#include "native_lights.hpp"

namespace mgsv::shadows::screen_space {

inline constexpr GUID SPOT_BUDGET_ID = {
    .Data1 = 0xcdb408ed, .Data2 = 0xcba8, .Data3 = 0x44e8, .Data4 = {0xa6, 0xc8, 0xfb, 0x38, 0x81, 0xd4, 0xd5, 0xa1}};

// A view's resource description never changes. Cache it on the view itself to
// avoid keeping retired textures alive or maintaining a separate global map.
struct __declspec(uuid("A079A897-0A5A-4823-BBC7-080019D8A9B8")) ViewDescription {
  D3D11_TEXTURE2D_DESC texture = {};
  bool compatible = false;
};

// Per recording context: the native inputs a light draw displaced, and what
// the context has already qualified.
struct __declspec(uuid("F5591171-16BF-4AC9-B341-D74C1F56BDE2")) DrawState {
  std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2> displaced;
  std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, 2> qualified_buffers;
  std::array<UINT, 2> buffer_sizes = {};
  bool active = false;
  bool replacement_safe = false;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> budget_target;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> budget;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> displaced_pcss_noise;
  bool pcss_noise_active = false;
  // The sun pass and depth view whose mask and record the views below hold. A
  // pass draws its one light up to four times; later draws reuse them.
  uint64_t sun_pass = 0u;
  const ID3D11ShaderResourceView* sun_depth = nullptr;
  std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2> sun_views;
};

inline void OnInitDevice(reshade::api::device* device) {
  if (device->get_api() == reshade::api::device_api::d3d11) {
    device->create_private_data<batch::DeviceState>()->native_hook = native_lights::Install(batch::BeginLocalLights);
  }
}

inline void OnDestroyDevice(reshade::api::device* device) {
  if (device->get_api() == reshade::api::device_api::d3d11) {
    if (const auto* state = device->get_private_data<batch::DeviceState>(); state != nullptr && state->native_hook) {
      native_lights::Detach();
    }
    device->destroy_private_data<batch::DeviceState>();
  }
}

inline void OnInitCommandList(reshade::api::command_list* cmd_list) {
  if (cmd_list->get_device()->get_api() == reshade::api::device_api::d3d11) {
    cmd_list->create_private_data<DrawState>();
  }
}

inline void OnDestroyCommandList(reshade::api::command_list* cmd_list) {
  if (cmd_list->get_device()->get_api() == reshade::api::device_api::d3d11) {
    cmd_list->destroy_private_data<DrawState>();
  }
}

inline void OnResetCommandList(reshade::api::command_list* cmd_list) {
  if (auto* state = cmd_list->get_private_data<DrawState>(); state != nullptr) {
    state->displaced = {};
    state->active = false;
    state->replacement_safe = false;
    state->displaced_pcss_noise.Reset();
    state->pcss_noise_active = false;
  }
}

inline void End(reshade::api::command_list* cmd_list) {
  if (cmd_list == nullptr) return;
  auto* state = cmd_list->get_private_data<DrawState>();
  if (state == nullptr) return;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  // Restore the native draw's displaced inputs, including intentional null
  // bindings. The retained views keep objects alive, not their old contents.
  if (state->pcss_noise_active) {
    context->PSSetShaderResources(20u, 1u, state->displaced_pcss_noise.GetAddressOf());
    state->displaced_pcss_noise.Reset();
    state->pcss_noise_active = false;
  }
  if (!state->active) return;
  const std::array<ID3D11ShaderResourceView*, 2> original = {state->displaced[0].Get(), state->displaced[1].Get()};
  context->PSSetShaderResources(18u, static_cast<UINT>(original.size()), original.data());
  state->displaced = {};
  state->active = false;
  state->replacement_safe = false;
}

inline bool OnClearRenderTarget(reshade::api::command_list* cmd_list, reshade::api::resource_view rtv,
                                const float* /*color*/, uint32_t rect_count, const reshade::api::rect* /*rects*/) {
  if (rtv.handle == 0u || rect_count != 0u || cmd_list->get_device()->get_api() != reshade::api::device_api::d3d11) return false;
  auto* view = reinterpret_cast<ID3D11RenderTargetView*>(rtv.handle);  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11Resource> resource;
  view->GetResource(&resource);
  Microsoft::WRL::ComPtr<IUnknown> stored;
  UINT bytes = sizeof(IUnknown*);
  if (FAILED(resource->GetPrivateData(SPOT_BUDGET_ID, &bytes, stored.GetAddressOf()))) return false;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> budget;
  if (FAILED(stored.As(&budget))) return false;
  const std::array<UINT, 4> zero = {};
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  // Record the reset beside the native clear. Deferred recording and replay
  // therefore use GPU order instead of a CPU Present counter.
  context->ClearUnorderedAccessViewUint(budget.Get(), zero.data());
  return false;
}

// Qualifies one native light draw and gives it its contact-shadow inputs: a
// place in its lighting pass's batch, or a batch of its own recorded here.
inline void Begin(reshade::api::command_list* cmd_list, const ShaderInjectData& settings, uint32_t light_type, bool has_shadow_map) {
  if (cmd_list == nullptr || light_type >= 5u || cmd_list->get_device()->get_api() != reshade::api::device_api::d3d11) return;
  if (settings.shadows_mode < 0.5f) return;
  auto* state = cmd_list->get_private_data<DrawState>();
  auto* device_state = cmd_list->get_device()->get_private_data<batch::DeviceState>();
  // Without the verified executable the native light shaders stay in place.
  if (state == nullptr || state->active || device_state == nullptr || !device_state->native_hook) return;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  // Read-only checks come first. Stencil-only draws need no displaced inputs,
  // shader replacement, settings injection, or noise binding.
  state->replacement_safe = false;
  const auto unavailable = [](uint32_t reason, const char* message) {
    static std::array<std::atomic_flag, 8> reported = {};
    if (!reported[reason].test_and_set(std::memory_order_relaxed)) {
      reshade::log::message(reshade::log::level::warning, message);
    }
  };
  const auto selected = inputs::ReadSelection(cmd_list->get_device());
  if (selected.device == nullptr) return;

  // Read the depth and outputs from this draw, rather than choosing textures
  // by size or by the order in which worker threads recorded their commands.
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth;
  std::array<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>, 3> targets;
  std::array<ID3D11RenderTargetView*, 3> native_targets = {};
  context->PSGetShaderResources(2u, 1u, &depth);
  context->OMGetRenderTargets(static_cast<UINT>(native_targets.size()), native_targets.data(), nullptr);
  for (size_t index = 0u; index < targets.size(); ++index) {
    targets[index].Attach(native_targets[index]);
  }
  if (depth == nullptr || targets[0] == nullptr) {
    unavailable(0u, "MgsvShadows: native depth or lighting output unavailable; keeping native shadows");
    return;
  }
  Microsoft::WRL::ComPtr<ID3D11BlendState> blend;
  UINT sample_mask = 0u;
  context->OMGetBlendState(&blend, nullptr, &sample_mask);
  if (sample_mask == 0u) return;
  if (blend != nullptr) {
    D3D11_BLEND_DESC description = {};
    blend->GetDesc(&description);
    bool writes_lighting = false;
    for (size_t index = 0u; index < 2u; ++index) {
      const auto& target = description.RenderTarget[description.IndependentBlendEnable != FALSE ? index : 0u];
      writes_lighting |= targets[index] != nullptr && (target.RenderTargetWriteMask & 0x7u) != 0u;
    }
    // Depth/stencil-only draws can retain a lighting shader. The native draw
    // still runs; its unchanged alpha does not need a screen-space shadow mask.
    if (!writes_lighting) return;
  }
  // Once a lighting draw qualifies, reserve its inputs until End. Any later
  // failure leaves null contact inputs and restores the displaced native views.
  std::array<ID3D11ShaderResourceView*, 2> previous = {};
  context->PSGetShaderResources(18u, static_cast<UINT>(previous.size()), previous.data());
  for (size_t index = 0u; index < previous.size(); ++index) {
    state->displaced[index].Attach(previous[index]);
  }
  state->active = true;
  constexpr std::array<ID3D11ShaderResourceView*, 2> empty = {};
  context->PSSetShaderResources(18u, static_cast<UINT>(empty.size()), empty.data());
  const auto describe = []<typename View>(View* view, ViewDescription* result) {
    UINT bytes = sizeof(*result);
    if (SUCCEEDED(view->GetPrivateData(__uuidof(ViewDescription), &bytes, result)) && bytes == sizeof(*result)) return true;
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    view->GetResource(&resource);
    if (FAILED(resource.As(&texture))) return false;
    texture->GetDesc(&result->texture);
    if constexpr (std::is_same_v<View, ID3D11ShaderResourceView>) {
      D3D11_SHADER_RESOURCE_VIEW_DESC description = {};
      view->GetDesc(&description);
      result->compatible = description.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D
                           && description.Texture2D.MostDetailedMip == 0u
                           && result->texture.ArraySize == 1u;
    } else {
      D3D11_RENDER_TARGET_VIEW_DESC description = {};
      view->GetDesc(&description);
      result->compatible = description.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D
                           && description.Texture2D.MipSlice == 0u;
    }
    result->compatible = result->compatible && result->texture.SampleDesc.Count == 1u;
    view->SetPrivateData(__uuidof(ViewDescription), sizeof(*result), result);
    return true;
  };
  ViewDescription depth_info;
  ViewDescription target_info;
  if (!describe(depth.Get(), &depth_info) || !describe(targets[0].Get(), &target_info)) return;
  const auto& depth_description = depth_info.texture;
  UINT viewport_count = 1u;
  D3D11_VIEWPORT viewport = {};
  context->RSGetViewports(&viewport_count, &viewport);
  if (viewport_count != 1u || viewport.TopLeftX != 0.f || viewport.TopLeftY != 0.f
      || viewport.Width != static_cast<float>(depth_description.Width) || viewport.Height != static_cast<float>(depth_description.Height)
      || target_info.texture.Width != depth_description.Width || target_info.texture.Height != depth_description.Height
      || !depth_info.compatible || !target_info.compatible) {
    unavailable(1u, "MgsvShadows: unsupported depth/viewport; keeping native shadows");
    return;
  }

  // The draw's scene and light constants. A 64 KiB allocation can contain many
  // draws' constants, so only the bound ranges belong to this light.
  std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, 2> native_constants;
  std::array<ID3D11Buffer*, 2> buffers = {};
  std::array<UINT, 2> first = {};
  std::array<UINT, 2> counts = {};
  Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context1;
  const bool ranged = SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&context1)));
  if (ranged) {
    context1->PSGetConstantBuffers1(2u, static_cast<UINT>(buffers.size()), buffers.data(), first.data(), counts.data());
  } else {
    context->PSGetConstantBuffers(2u, static_cast<UINT>(buffers.size()), buffers.data());
  }
  for (size_t index = 0u; index < buffers.size(); ++index) {
    native_constants[index].Attach(buffers[index]);
  }
  if (buffers[0] == nullptr || buffers[1] == nullptr) {
    unavailable(2u, "MgsvShadows: native scene/light constants unavailable; keeping native shadows");
    return;
  }
  for (size_t index = 0u; index < buffers.size(); ++index) {
    if (state->qualified_buffers[index].Get() != buffers[index]) {
      D3D11_BUFFER_DESC description = {};
      buffers[index]->GetDesc(&description);
      state->qualified_buffers[index] = native_constants[index];
      state->buffer_sizes[index] = description.ByteWidth;
    }
    const UINT needed = (index == 0u ? batch::SCENE_BYTES : batch::LIGHT_BYTES);
    if (state->buffer_sizes[index] < needed
        || (ranged && (counts[index] * 16u < needed || (static_cast<uint64_t>(first[index]) * 16u) + needed > state->buffer_sizes[index]))) {
      unavailable(2u, "MgsvShadows: native constant-buffer range is incompatible; keeping native shadows");
      return;
    }
  }

  ID3D11UnorderedAccessView* budget = nullptr;
  if ((light_type == 1u || light_type == 3u) && settings.spot_contact_shadow_budget >= 1.f) {
    // Contexts writing the same native lighting allocation share its counter.
    // Resetting it beside the native clear makes replay follow GPU order.
    if (state->budget_target.Get() != targets[0].Get()) {
      Microsoft::WRL::ComPtr<ID3D11Resource> resource;
      targets[0]->GetResource(&resource);
      Microsoft::WRL::ComPtr<IUnknown> stored;
      UINT bytes = sizeof(IUnknown*);
      {
        const std::shared_lock lock(device_state->mutex);
        resource->GetPrivateData(SPOT_BUDGET_ID, &bytes, stored.GetAddressOf());
      }
      if (stored == nullptr) {
        const auto candidate = batch::CreateBudget(reinterpret_cast<ID3D11Device*>(cmd_list->get_device()->get_native()));  // NOLINT(performance-no-int-to-ptr)
        if (candidate == nullptr) {
          unavailable(7u, "MgsvShadows: spotlight budget allocation failed; keeping native shadows");
          return;
        }
        const std::unique_lock lock(device_state->mutex);
        bytes = sizeof(IUnknown*);
        resource->GetPrivateData(SPOT_BUDGET_ID, &bytes, stored.GetAddressOf());
        if (stored == nullptr) {
          if (FAILED(resource->SetPrivateDataInterface(SPOT_BUDGET_ID, candidate.Get()))) return;
          stored = candidate;
        }
      }
      if (FAILED(stored.As(&state->budget))) return;
      state->budget_target = targets[0];
    }
    budget = state->budget.Get();
  }

  // Convert reach in a private copy; other effects keep the user's output-pixel value.
  // The trace scales its remaining output-pixel distances by the same factor.
  auto constants = settings;
  constants.contact_shadow_render_scale = 1.f;
  const auto plan = reconstruction::resolution::Read();
  if (plan.IsScaled() && plan.render == reconstruction::resolution::Size{.width = depth_description.Width, .height = depth_description.Height}) {
    constants.contact_shadow_render_scale = static_cast<float>(depth_description.Width) / static_cast<float>(plan.output.width);
    constants.contact_shadow_reach_pixels *= constants.contact_shadow_render_scale;
  }

  // A sun pass shades one directional light several times against the same
  // depth, so its later draws reuse the first draw's mask and record. A local
  // light joins its lighting pass's batch. Anything else records its own.
  std::array<ID3D11ShaderResourceView*, 2> result = {};
  const uint64_t sun_pass = (light_type == 0u ? reconstruction::projection_jitter::active_sun_pass : 0u);
  if (sun_pass != 0u && state->sun_pass == sun_pass && state->sun_depth == depth.Get()) {
    result = {state->sun_views[0].Get(), state->sun_views[1].Get()};
  } else if (!batch::CollectLight(context, device_state, constants, light_type, has_shadow_map, depth_description.Width, depth_description.Height,
                                  selected.generation, depth.Get(), targets[0].Get(), budget, &result)) {
    if (!batch::RecordLight(context, device_state, constants, light_type, has_shadow_map, depth_description.Width, depth_description.Height,
                            depth.Get(), budget, buffers, first, &result)) {
      unavailable(3u, "MgsvShadows: contact-shadow storage or programs unavailable; keeping native shadows");
      return;
    }
    if (light_type == 0u) {
      state->sun_pass = sun_pass;
      state->sun_depth = depth.Get();
      state->sun_views = {result[0], result[1]};
    }
  }
  context->PSSetShaderResources(18u, static_cast<UINT>(result.size()), result.data());
  // The extended sun output is safe only when the game has no third target.
  // Failed or stencil-only draws keep their original shader and skip injection.
  state->replacement_safe = targets[2] == nullptr;
}

}  // namespace mgsv::shadows::screen_space
