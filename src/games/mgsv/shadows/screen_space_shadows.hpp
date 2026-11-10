// SPDX-License-Identifier: MIT
#pragma once

#include <d3d11_1.h>
#include <wrl/client.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <type_traits>
#include <utility>

#include <embed/shaders.h>
#include <include/reshade.hpp>

#include "../inputs.hpp"
#include "../reconstruction/resolution.hpp"
#include "../shared.h"
#include "./batch.hpp"
#include "./native_groups.hpp"

namespace mgsv::shadows::screen_space {

inline constexpr GUID SPOT_BUDGET_ID = {
    .Data1 = 0xcdb408ed, .Data2 = 0xcba8, .Data3 = 0x44e8, .Data4 = {0xa6, 0xc8, 0xfb, 0x38, 0x81, 0xd4, 0xd5, 0xa1}};

struct __declspec(uuid("857D66A1-1F7E-4C6F-BDE8-A21BC0135537")) DeviceState {
  std::shared_mutex mutex;
  std::array<Microsoft::WRL::ComPtr<ID3D11ComputeShader>, 3> shaders;
  Microsoft::WRL::ComPtr<ID3D11SamplerState> border;
};

// A view's resource description never changes. Cache it on the view itself to
// avoid keeping retired textures alive or maintaining a separate global map.
struct __declspec(uuid("A079A897-0A5A-4823-BBC7-080019D8A9B8")) ViewDescription {
  D3D11_TEXTURE2D_DESC texture = {};
  bool compatible = false;
};

struct DispatchStorage {
  Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
};

// Each recording context owns its scratch images. Commands clear and overwrite
// the current light's bounds before its draw, including when a list is replayed.
struct __declspec(uuid("F5591171-16BF-4AC9-B341-D74C1F56BDE2")) DrawState {
  std::array<Microsoft::WRL::ComPtr<ID3D11ComputeShader>, 3> shaders;
  Microsoft::WRL::ComPtr<ID3D11SamplerState> border;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> mask;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mask_srv;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> mask_uav;
  Microsoft::WRL::ComPtr<ID3D11Buffer> dispatch;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> dispatch_srv;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> dispatch_uav;
  Microsoft::WRL::ComPtr<ID3D11Buffer> constants;
  std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2> displaced;
  std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, 2> qualified_buffers;
  std::array<UINT, 2> buffer_sizes = {};
  uint32_t width = 0u;
  uint32_t height = 0u;
  uint64_t generation = 0u;
  bool active = false;
  bool replacement_safe = false;
  uint32_t resolution_divisor = 0u;  // Reserved private-data layout slot.
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> fallback_budget;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> budget_target;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> budget;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> displaced_pcss_noise;
  bool pcss_noise_active = false;
  // Keep four argument allocations in rotation, including the active one above.
  // A later setup can write another buffer while earlier light draws still read.
  std::array<DispatchStorage, 3> spare_dispatches;
  size_t next_dispatch = 0u;
};

inline void OnInitDevice(reshade::api::device* device) {
  if (device->get_api() == reshade::api::device_api::d3d11) {
    device->create_private_data<DeviceState>();
    device->create_private_data<batch::DeviceState>();
    device->get_private_data<batch::DeviceState>()->native_hooks = native_groups::Install(batch::FinishGroup);
  }
}

inline void OnDestroyDevice(reshade::api::device* device) {
  if (device->get_api() == reshade::api::device_api::d3d11) {
    if (const auto* state = device->get_private_data<batch::DeviceState>(); state != nullptr && state->native_hooks) {
      native_groups::Detach();
    }
    device->destroy_private_data<batch::DeviceState>();
    device->destroy_private_data<DeviceState>();
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

inline void Begin(reshade::api::command_list* cmd_list, const ShaderInjectData& settings, uint32_t light_type) {
  if (cmd_list == nullptr || light_type >= 5u || cmd_list->get_device()->get_api() != reshade::api::device_api::d3d11) return;
  const uint32_t light_group = (light_type >= 3u ? light_type - 2u : light_type);
  if (settings.enhanced_shadows_enabled < 0.5f) return;
  auto* state = cmd_list->get_private_data<DrawState>();
  auto* device_state = cmd_list->get_device()->get_private_data<DeviceState>();
  if (state == nullptr || state->active) return;
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
  if (device_state == nullptr || selected.device == nullptr) return;

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

  // Forward the actual native buffer ranges. A 64 KiB allocation can contain
  // many draws' constants, so binding the whole allocation would read wrong data.
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
    const UINT needed = (index == 0u ? 480u : 176u);
    if (state->buffer_sizes[index] < needed
        || (ranged && (counts[index] * 16u < needed || (static_cast<uint64_t>(first[index]) * 16u) + needed > state->buffer_sizes[index]))) {
      unavailable(2u, "MgsvShadows: native constant-buffer range is incompatible; keeping native shadows");
      return;
    }
  }

  auto* device = reinterpret_cast<ID3D11Device*>(cmd_list->get_device()->get_native());  // NOLINT(performance-no-int-to-ptr)
  const auto create_budget = [device]() {
    const D3D11_BUFFER_DESC description = {
        .ByteWidth = 16u,
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_UNORDERED_ACCESS,
        .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS,
    };
    const std::array<uint32_t, 4> zero = {};
    const D3D11_SUBRESOURCE_DATA data = {.pSysMem = zero.data()};
    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> view;
    if (FAILED(device->CreateBuffer(&description, &data, &buffer))) return view;
    D3D11_UNORDERED_ACCESS_VIEW_DESC output = {};
    output.Format = DXGI_FORMAT_R32_TYPELESS;
    output.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    output.Buffer.NumElements = 4u;
    output.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
    if (FAILED(device->CreateUnorderedAccessView(buffer.Get(), &output, &view))) return Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>{};
    return view;
  };
  if (state->fallback_budget == nullptr) {
    state->fallback_budget = create_budget();
  }
  if (state->fallback_budget == nullptr) {
    unavailable(7u, "MgsvShadows: spotlight budget allocation failed; keeping native shadows");
    return;
  }
  auto* budget = state->fallback_budget.Get();
  if (light_group == 1u && settings.ss_spot_budget >= 1.f) {
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
        const auto candidate = create_budget();
        if (candidate == nullptr) return;
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
  // Local-light groups supply owned CPU records. Their shared work is inserted
  // before the first native consumer; unsupported contexts keep the same
  // per-light implementation below, including unshadowed native lights.
  // Convert reach in a private copy; other effects keep the user's output-pixel value.
  auto constants = settings;
  const auto plan = reconstruction::resolution::Read();
  if (plan.IsScaled() && plan.render == reconstruction::resolution::Size{.width = depth_description.Width, .height = depth_description.Height}) {
    constants.ss_shadow_reach *= static_cast<float>(depth_description.Width) / static_cast<float>(plan.output.width);
  }
  if (batch::TryCollectLight(cmd_list, constants, light_type, depth_description.Width, depth_description.Height,
                             selected.generation, depth.Get(), targets[0].Get(), budget, buffers[0], first[0], counts[0])) {
    state->replacement_safe = targets[2] == nullptr;
    return;
  }
  if (auto* group = native_groups::GetCurrentGroup(context); group != nullptr) {
    batch::FinishGroup(group);
    static std::atomic_flag reported;
    if (!reported.test_and_set(std::memory_order_relaxed)) {
      reshade::log::message(reshade::log::level::warning,
                            "MgsvShadows: local-light group could not be batched; using per-light contact shadows");
    }
  }

  // Share immutable shaders and the border sampler across contexts. Resizing
  // the depth image replaces only scratch textures, not these programs.
  if (state->shaders[0] == nullptr) {
    const std::shared_lock lock(device_state->mutex);
    state->shaders = device_state->shaders;
    state->border = device_state->border;
  }
  if (state->shaders[0] == nullptr) {
    std::array<Microsoft::WRL::ComPtr<ID3D11ComputeShader>, 3> shaders;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> border;
    const std::array<std::span<const uint8_t>, 3> bytecode = {__bend_setup, __bend_clear, __bend_trace};
    for (size_t index = 0u; index < shaders.size(); ++index) {
      if (FAILED(device->CreateComputeShader(bytecode[index].data(), bytecode[index].size(), nullptr, &shaders[index]))) {
        unavailable(3u, "MgsvShadows: Bend shader creation failed; keeping native shadows");
        return;
      }
    }
    const D3D11_SAMPLER_DESC description = {
        .Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
        .AddressU = D3D11_TEXTURE_ADDRESS_BORDER,
        .AddressV = D3D11_TEXTURE_ADDRESS_BORDER,
        .AddressW = D3D11_TEXTURE_ADDRESS_BORDER,
        .MaxAnisotropy = 1u,
        .ComparisonFunc = D3D11_COMPARISON_ALWAYS,
        .MaxLOD = D3D11_FLOAT32_MAX,
    };
    if (FAILED(device->CreateSamplerState(&description, &border))) {
      unavailable(3u, "MgsvShadows: Bend sampler creation failed; keeping native shadows");
      return;
    }
    const std::unique_lock lock(device_state->mutex);
    if (device_state->shaders[0] == nullptr) {
      device_state->shaders = shaders;
      device_state->border = border;
    }
    state->shaders = std::move(shaders);
    state->border = std::move(border);
  }
  const uint32_t mask_width = depth_description.Width;
  const uint32_t mask_height = depth_description.Height;
  // Each context records writes into its own mask. Compact RG8 stores enough
  // precision for visibility and the bounded radius; unsupported devices use RG16F.
  if (state->mask == nullptr || state->width != mask_width || state->height != mask_height
      || state->generation != selected.generation) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> mask;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
    UINT format_support = 0u;
    const bool compact = SUCCEEDED(device->CheckFormatSupport(DXGI_FORMAT_R8G8_UNORM, &format_support))
                         && (format_support & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW) != 0u;
    const D3D11_TEXTURE2D_DESC description = {
        .Width = mask_width,
        .Height = mask_height,
        .MipLevels = 1u,
        .ArraySize = 1u,
        .Format = (compact ? DXGI_FORMAT_R8G8_UNORM : DXGI_FORMAT_R16G16_FLOAT),
        .SampleDesc = {.Count = 1u},
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
    };
    if (FAILED(device->CreateTexture2D(&description, nullptr, &mask))
        || FAILED(device->CreateShaderResourceView(mask.Get(), nullptr, &srv))
        || FAILED(device->CreateUnorderedAccessView(mask.Get(), nullptr, &uav))) {
      unavailable(4u, "MgsvShadows: Bend visibility texture creation failed; keeping native shadows");
      return;
    }
    state->mask = std::move(mask);
    state->mask_srv = std::move(srv);
    state->mask_uav = std::move(uav);
    state->width = description.Width;
    state->height = description.Height;
    state->generation = selected.generation;
  }
  auto& spare_dispatch = state->spare_dispatches[state->next_dispatch];
  state->next_dispatch = (state->next_dispatch + 1u) % state->spare_dispatches.size();
  std::swap(state->dispatch, spare_dispatch.buffer);
  std::swap(state->dispatch_srv, spare_dispatch.srv);
  std::swap(state->dispatch_uav, spare_dispatch.uav);
  if (state->dispatch == nullptr) {
    // The header describes the light; eight sweep records precede the clear
    // and trace arguments. Setup writes all values consumed by this invocation.
    constexpr UINT dispatch_bytes = batch::RECORD_BYTES;
    const D3D11_BUFFER_DESC description = {
        .ByteWidth = dispatch_bytes,
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
        .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS | D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS,
    };
    D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Format = DXGI_FORMAT_R32_TYPELESS;
    srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    srv.BufferEx.NumElements = dispatch_bytes / 4u;
    srv.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
    D3D11_UNORDERED_ACCESS_VIEW_DESC uav = {};
    uav.Format = DXGI_FORMAT_R32_TYPELESS;
    uav.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uav.Buffer.NumElements = dispatch_bytes / 4u;
    uav.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> input;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> output;
    if (FAILED(device->CreateBuffer(&description, nullptr, &buffer))
        || FAILED(device->CreateShaderResourceView(buffer.Get(), &srv, &input))
        || FAILED(device->CreateUnorderedAccessView(buffer.Get(), &uav, &output))) {
      unavailable(5u, "MgsvShadows: Bend dispatch storage creation failed; keeping native shadows");
      return;
    }
    state->dispatch = std::move(buffer);
    state->dispatch_srv = std::move(input);
    state->dispatch_uav = std::move(output);
  }
  constexpr UINT constant_bytes = (sizeof(ShaderInjectData) + 15u) & ~15u;
  if (state->constants == nullptr) {
    const D3D11_BUFFER_DESC description = {
        .ByteWidth = constant_bytes,
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    if (FAILED(device->CreateBuffer(&description, nullptr, &state->constants))) {
      unavailable(6u, "MgsvShadows: Bend settings upload allocation failed; keeping native shadows");
      return;
    }
  }

  // Batch records carry their own types. The per-light path uses this local field.
  constants.ss_shadow_light_type = static_cast<float>(light_type);
  // Discard gives this draw fresh upload storage instead of copying over a
  // default buffer that earlier queued dispatches can still be reading.
  D3D11_MAPPED_SUBRESOURCE mapped = {};
  if (FAILED(context->Map(state->constants.Get(), 0u, D3D11_MAP_WRITE_DISCARD, 0u, &mapped))) {
    unavailable(6u, "MgsvShadows: Bend settings mapping failed; keeping native shadows");
    return;
  }
  std::memset(mapped.pData, 0, constant_bytes);
  std::memcpy(mapped.pData, &constants, sizeof(constants));
  context->Unmap(state->constants.Get(), 0u);

  // Mask and dispatch storage belong to this context; the spotlight counter
  // can be shared. Native graphics never binds their UAVs, and PS t18/t19 were
  // unbound above, so recording these writes can leave graphics state intact.
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> saved_shader;
  Microsoft::WRL::ComPtr<ID3D11SamplerState> saved_sampler;
  std::array<ID3D11ClassInstance*, 256> instances = {};
  std::array<Microsoft::WRL::ComPtr<ID3D11ClassInstance>, 256> saved_instances;
  UINT instance_count = static_cast<UINT>(instances.size());
  context->CSGetShader(&saved_shader, instances.data(), &instance_count);
  for (UINT index = 0u; index < instance_count; ++index) {
    saved_instances[index].Attach(instances[index]);
  }
  context->CSGetSamplers(0u, 1u, &saved_sampler);
  std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2> saved_inputs;
  context->CSGetShaderResources(0u, 1u, &saved_inputs[0]);
  context->CSGetShaderResources(3u, 1u, &saved_inputs[1]);
  std::array<ID3D11UnorderedAccessView*, 3> saved_outputs = {};
  std::array<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>, 3> saved_output_refs;
  context->CSGetUnorderedAccessViews(0u, static_cast<UINT>(saved_outputs.size()), saved_outputs.data());
  for (size_t index = 0u; index < saved_outputs.size(); ++index) {
    saved_output_refs[index].Attach(saved_outputs[index]);
  }
  std::array<ID3D11Buffer*, 3> saved_buffers = {};
  std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, 3> saved_buffer_refs;
  std::array<UINT, 3> saved_first = {};
  std::array<UINT, 3> saved_counts = {};
  if (ranged) {
    context1->CSGetConstantBuffers1(2u, 2u, saved_buffers.data(), saved_first.data(), saved_counts.data());
    context1->CSGetConstantBuffers1(13u, 1u, saved_buffers.data() + 2u, saved_first.data() + 2u, saved_counts.data() + 2u);
  } else {
    context->CSGetConstantBuffers(2u, 2u, saved_buffers.data());
    context->CSGetConstantBuffers(13u, 1u, saved_buffers.data() + 2u);
  }
  for (size_t index = 0u; index < saved_buffers.size(); ++index) {
    saved_buffer_refs[index].Attach(saved_buffers[index]);
  }
  // Setup publishes bounds and indirect arguments on the GPU. Clear and trace
  // consume them in order, without a CPU readback or per-sector dispatch loop.
  if (ranged) {
    context1->CSSetConstantBuffers1(2u, static_cast<UINT>(buffers.size()), buffers.data(), first.data(), counts.data());
  } else {
    context->CSSetConstantBuffers(2u, static_cast<UINT>(buffers.size()), buffers.data());
  }
  context->CSSetConstantBuffers(13u, 1u, state->constants.GetAddressOf());
  context->CSSetShaderResources(0u, 1u, depth.GetAddressOf());
  context->CSSetShaderResources(3u, 1u, empty.data());
  const std::array<ID3D11UnorderedAccessView*, 3> outputs = {state->dispatch_uav.Get(), state->mask_uav.Get(), budget};
  context->CSSetUnorderedAccessViews(0u, static_cast<UINT>(outputs.size()), outputs.data(), nullptr);
  context->CSSetShader(state->shaders[0].Get(), nullptr, 0u);
  context->Dispatch(1u, 1u, 1u);
  constexpr std::array<ID3D11UnorderedAccessView*, 3> no_outputs = {};
  context->CSSetUnorderedAccessViews(0u, 1u, no_outputs.data(), nullptr);
  context->CSSetShaderResources(3u, 1u, state->dispatch_srv.GetAddressOf());
  context->CSSetSamplers(0u, 1u, state->border.GetAddressOf());
  context->CSSetShader(state->shaders[1].Get(), nullptr, 0u);
  context->DispatchIndirect(state->dispatch.Get(), 48u + (8u * 32u));
  context->CSSetShader(state->shaders[2].Get(), nullptr, 0u);
  context->DispatchIndirect(state->dispatch.Get(), 48u + (9u * 32u));

  // Unbind private writable views before restoring compute state and exposing
  // their read-only views to the native light shader. The draw runs afterward.
  context->CSSetUnorderedAccessViews(0u, static_cast<UINT>(no_outputs.size()), no_outputs.data(), nullptr);
  context->CSSetShaderResources(0u, 1u, saved_inputs[0].GetAddressOf());
  context->CSSetShaderResources(3u, 1u, saved_inputs[1].GetAddressOf());
  context->CSSetUnorderedAccessViews(0u, static_cast<UINT>(saved_outputs.size()), saved_outputs.data(), nullptr);
  if (ranged) {
    context1->CSSetConstantBuffers1(2u, 2u, saved_buffers.data(), saved_first.data(), saved_counts.data());
    context1->CSSetConstantBuffers1(13u, 1u, saved_buffers.data() + 2u, saved_first.data() + 2u, saved_counts.data() + 2u);
  } else {
    context->CSSetConstantBuffers(2u, 2u, saved_buffers.data());
    context->CSSetConstantBuffers(13u, 1u, saved_buffers.data() + 2u);
  }
  context->CSSetSamplers(0u, 1u, saved_sampler.GetAddressOf());
  context->CSSetShader(saved_shader.Get(), instances.data(), instance_count);
  const std::array<ID3D11ShaderResourceView*, 2> result = {state->mask_srv.Get(), state->dispatch_srv.Get()};
  context->PSSetShaderResources(18u, static_cast<UINT>(result.size()), result.data());
  // The extended sun output is safe only when the game has no third target.
  // Failed or stencil-only draws keep their original shader and skip injection.
  state->replacement_safe = targets[2] == nullptr;
  static std::array<std::atomic_flag, 3> reported;
  if (!reported[light_group].test_and_set(std::memory_order_relaxed)) {
    constexpr std::array<const char*, 3> messages = {
        "MgsvShadows: Bend directional commands recorded; use Contact Shadow Debug View to inspect output",
        "MgsvShadows: Bend spotlight commands recorded; use Contact Shadow Debug View to inspect output",
        "MgsvShadows: Bend point-light commands recorded; use Contact Shadow Debug View to inspect output",
    };
    reshade::log::message(reshade::log::level::info, messages[light_group]);
  }
}

}  // namespace mgsv::shadows::screen_space
