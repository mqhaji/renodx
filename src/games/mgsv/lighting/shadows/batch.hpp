// SPDX-License-Identifier: MIT
#ifndef MGSV_SHADOWS_BATCH_HPP_
#define MGSV_SHADOWS_BATCH_HPP_

#include <d3d11_1.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <utility>

#include <embed/shaders.h>
#include <include/reshade.hpp>

#include "../../reconstruction/coordinator.hpp"
#include "../../rendering/d3d11_state.hpp"
#include "../../shared.h"
#include "native_groups.hpp"

namespace mgsv::shadows::batch {

// Each light keeps the original 368-byte dispatch record followed by its atlas
// location and batch work totals. These sizes match contact/bend_common.hlsli.
inline constexpr UINT RECORD_BYTES = 400u;
inline constexpr UINT MAX_LIGHTS = 256u;
// One header vector, then eleven native vectors and one light-kind vector per light.
inline constexpr UINT LIGHT_VECTORS = 12u;
inline constexpr UINT INPUT_VECTORS = 1u + (MAX_LIGHTS * LIGHT_VECTORS);
// Bound a single atlas allocation, including any padding on its final row.
inline constexpr uint64_t MAX_ATLAS_BYTES = 256ull * 1024ull * 1024ull;
inline constexpr uint64_t MAX_ATLAS_DIMENSION = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
inline constexpr UINT DEFAULT_MASK_CAPACITY = 13u;
inline constexpr UINT SETTINGS_BYTES = (sizeof(ShaderInjectData) + 15u) & ~15u;

// Batches reuse this storage in GPU order. The preceding batch's light draws
// finish reading it before the next batch overwrites its inputs and masks.
struct Storage {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> atlas;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> atlas_srv;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> atlas_uav;
  Microsoft::WRL::ComPtr<ID3D11Buffer> dispatch_data;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> dispatch_srv;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> dispatch_uav;
  std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, MAX_LIGHTS> light_dispatch_views;
  Microsoft::WRL::ComPtr<ID3D11Buffer> light_buffer;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> light_srv;
  uint32_t width = 0u;
  uint32_t height = 0u;
  uint32_t mask_capacity = 0u;
  uint32_t requested_capacity = 0u;
  uint32_t atlas_width = 0u;
  uint32_t atlas_height = 0u;
};

struct __declspec(uuid("DFA07B80-EFCF-4D15-83EE-98B7DA01A5E2")) DeviceState {
  std::shared_mutex mutex;
  std::array<Microsoft::WRL::ComPtr<ID3D11ComputeShader>, 3> shaders;
  Microsoft::WRL::ComPtr<ID3D11SamplerState> border;
  std::shared_ptr<Storage> storage;
  bool native_hooks = false;
};

struct Batch {
  std::array<Microsoft::WRL::ComPtr<ID3D11ComputeShader>, 3> shaders;
  Microsoft::WRL::ComPtr<ID3D11SamplerState> border;
  std::shared_ptr<Storage> storage;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> budget;
  Microsoft::WRL::ComPtr<ID3D11Buffer> scene_snapshot;
  Microsoft::WRL::ComPtr<ID3D11Buffer> source_scene;
  Microsoft::WRL::ComPtr<ID3D11Buffer> settings_buffer;
  std::array<float, 120> native_scene_values = {};
  std::array<std::array<float, 4>, INPUT_VECTORS> light_data = {};
  ShaderInjectData settings = {};
  UINT scene_first_constant = 0u;
  UINT scene_constant_count = 0u;
  uint64_t generation = 0u;
  uint32_t light_count = 0u;
  uint32_t light_capacity = 0u;
  uint32_t light_family = 0u;
  std::atomic<bool> collection_complete = false;

  void Dispatch(reshade::api::command_list* cmd_list) const {
    // The group and submission callback own these copied inputs. Finishing
    // collection publishes them before this execution or a later list replay.
    if (!collection_complete.load(std::memory_order_acquire) || light_count == 0u) return;
    auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
    const auto saved = rendering::d3d11_compute_state::Capture(cmd_list);
    constexpr std::array<ID3D11ShaderResourceView*, 2> empty_views = {};
    context->PSSetShaderResources(18u, static_cast<UINT>(empty_views.size()), empty_views.data());
    // Upload all lights together. DEFAULT storage supports both immediate
    // execution and command lists merged into another deferred context.
    context->UpdateSubresource(storage->light_buffer.Get(), 0u, nullptr, light_data.data(), 0u, 0u);
    Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context1;
    if (scene_constant_count != 0u && SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&context1)))) {
      context1->CSSetConstantBuffers1(2u, 1u, scene_snapshot.GetAddressOf(), &scene_first_constant, &scene_constant_count);
    } else {
      context->CSSetConstantBuffers(2u, 1u, scene_snapshot.GetAddressOf());
    }
    context->CSSetConstantBuffers(13u, 1u, settings_buffer.GetAddressOf());
    context->CSSetShaderResources(0u, 1u, depth.GetAddressOf());
    context->CSSetShaderResources(3u, 1u, empty_views.data());
    context->CSSetShaderResources(4u, 1u, storage->light_srv.GetAddressOf());
    const std::array<ID3D11UnorderedAccessView*, 3> outputs = {
        storage->dispatch_uav.Get(), storage->atlas_uav.Get(), budget.Get()};
    context->CSSetUnorderedAccessViews(0u, static_cast<UINT>(outputs.size()), outputs.data(), nullptr);
    context->CSSetSamplers(0u, 1u, border.GetAddressOf());
    context->CSSetShader(shaders[0].Get(), nullptr, 0u);
    // Setup preserves native light order and the shared spotlight budget.
    // Clear and trace then process all admitted rectangles in parallel.
    context->Dispatch(1u, 1u, 1u);
    constexpr std::array<ID3D11UnorderedAccessView*, 1> empty_outputs = {};
    context->CSSetUnorderedAccessViews(0u, 1u, empty_outputs.data(), nullptr);
    context->CSSetShaderResources(3u, 1u, storage->dispatch_srv.GetAddressOf());
    context->CSSetShader(shaders[1].Get(), nullptr, 0u);
    context->DispatchIndirect(storage->dispatch_data.Get(), light_count * RECORD_BYTES);
    context->CSSetShader(shaders[2].Get(), nullptr, 0u);
    context->DispatchIndirect(storage->dispatch_data.Get(), (light_count * RECORD_BYTES) + 16u);
    rendering::d3d11_compute_state::Restore(cmd_list, saved);
    static std::array<std::atomic_flag, 3> reported;
    if (!reported[light_family].test_and_set(std::memory_order_relaxed)) {
      const auto message = std::format("MgsvShadows: batch commands recorded for {} native {} light draws; atlas {} x {}, capacity {} masks",
                                       light_count, (light_family == 1u ? "spot" : "point"),
                                       storage->atlas_width, storage->atlas_height, storage->mask_capacity);
      reshade::log::message(reshade::log::level::info, message.c_str());
    }
  }
};

inline void FinishGroup(native_groups::Group* group) noexcept {
  if (group->collected_batch != nullptr) {
    auto* recorded = static_cast<Batch*>(group->collected_batch.get());
    recorded->collection_complete.store(true, std::memory_order_release);
    group->collected_batch.reset();
  }
}

inline bool TryCollectLight(reshade::api::command_list* cmd_list, const ShaderInjectData& settings,
                            uint32_t light_type, uint32_t width, uint32_t height, uint64_t generation,
                            ID3D11ShaderResourceView* depth, ID3D11RenderTargetView* target,
                            ID3D11UnorderedAccessView* budget, ID3D11Buffer* source_scene, UINT scene_first, UINT scene_count) {
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  auto* group = native_groups::GetCurrentGroup(context);
  auto* device_state = cmd_list->get_device()->get_private_data<DeviceState>();
  if (group == nullptr || device_state == nullptr || light_type == 0u
      || context->GetType() != D3D11_DEVICE_CONTEXT_DEFERRED) return false;
  const auto light_family = (light_type >= 3u ? light_type - 2u : light_type);
  if (light_family != group->light_type) return false;
  native_groups::Constants native_constants;
  if (!native_groups::CopyConstants(context, &native_constants)) return false;

  // Only the native unshadowed volume groups are collected. These groups do not
  // render shadow maps or change scene depth between their light draws.
  Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_state;
  context->OMGetDepthStencilState(&depth_state, nullptr);
  if (depth_state == nullptr) return false;
  D3D11_DEPTH_STENCIL_DESC depth_description = {};
  depth_state->GetDesc(&depth_description);
  if (depth_description.DepthEnable != FALSE && depth_description.DepthWriteMask != D3D11_DEPTH_WRITE_MASK_ZERO) return false;

  // A changed scene, output, settings snapshot, or full input buffer starts a
  // new batch. Its insertion also finishes the preceding native draw sequence.
  auto* active_batch = static_cast<Batch*>(group->collected_batch.get());
  if (active_batch != nullptr && !active_batch->collection_complete.load(std::memory_order_acquire)
      && (active_batch->depth.Get() != depth || active_batch->target.Get() != target || active_batch->budget.Get() != budget
          || active_batch->source_scene.Get() != source_scene || active_batch->scene_first_constant != scene_first || active_batch->scene_constant_count != scene_count
          || active_batch->generation != generation || active_batch->storage->width != width || active_batch->storage->height != height
          || std::memcmp(active_batch->native_scene_values.data(), native_constants.scene.data(), sizeof(native_constants.scene)) != 0
          || std::memcmp(&active_batch->settings, &settings, sizeof(settings)) != 0 || active_batch->light_count == active_batch->light_capacity)) {
    active_batch->collection_complete.store(true, std::memory_order_release);
  }
  if (active_batch == nullptr || active_batch->collection_complete.load(std::memory_order_acquire)) {
    group->collected_batch.reset();
    const auto new_batch = std::make_shared<Batch>();
    new_batch->settings = settings;
    new_batch->native_scene_values = native_constants.scene;
    new_batch->source_scene = source_scene;
    new_batch->depth = depth;
    new_batch->target = target;
    new_batch->budget = budget;
    new_batch->scene_first_constant = scene_first;
    new_batch->scene_constant_count = scene_count;
    new_batch->generation = generation;
    new_batch->light_family = light_family;
    auto* device = reinterpret_cast<ID3D11Device*>(cmd_list->get_device()->get_native());  // NOLINT(performance-no-int-to-ptr)
    {
      const std::unique_lock lock(device_state->mutex);
      if (device_state->shaders[0] == nullptr) {
        const std::array<std::span<const uint8_t>, 3> bytecode = {
            __bend_batch_setup, __bend_batch_clear, __bend_batch_trace};
        std::array<Microsoft::WRL::ComPtr<ID3D11ComputeShader>, 3> shaders;
        for (size_t index = 0u; index < shaders.size(); ++index) {
          if (FAILED(device->CreateComputeShader(bytecode[index].data(), bytecode[index].size(), nullptr, &shaders[index]))) return false;
        }
        const D3D11_SAMPLER_DESC sampler = {
            .Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
            .AddressU = D3D11_TEXTURE_ADDRESS_BORDER,
            .AddressV = D3D11_TEXTURE_ADDRESS_BORDER,
            .AddressW = D3D11_TEXTURE_ADDRESS_BORDER,
            .MaxAnisotropy = 1u,
            .ComparisonFunc = D3D11_COMPARISON_ALWAYS,
            .MaxLOD = D3D11_FLOAT32_MAX,
        };
        if (FAILED(device->CreateSamplerState(&sampler, &device_state->border))) return false;
        device_state->shaders = std::move(shaders);
      }
      new_batch->shaders = device_state->shaders;
      new_batch->border = device_state->border;
      // Reserve full-screen space for each possible mask. Point lights and
      // unlimited spotlights split at this limit. A finite spotlight budget
      // lets one batch collect more lights because only that many can be traced.
      const auto requested_capacity = (std::isfinite(settings.spot_contact_shadow_budget) && settings.spot_contact_shadow_budget >= 1.f
                                           ? static_cast<UINT>(std::min(settings.spot_contact_shadow_budget, static_cast<float>(MAX_LIGHTS)))
                                           : DEFAULT_MASK_CAPACITY);
      if (device_state->storage == nullptr || device_state->storage->width != width
          || device_state->storage->height != height
          || device_state->storage->requested_capacity != requested_capacity) {
        auto storage = std::make_shared<Storage>();
        UINT support = 0u;
        const bool compact = SUCCEEDED(device->CheckFormatSupport(DXGI_FORMAT_R8G8_UNORM, &support))
                             && (support & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW) != 0u;
        const uint64_t pixels = static_cast<uint64_t>(width) * height;
        const auto mask_capacity = static_cast<UINT>(std::min({static_cast<uint64_t>(requested_capacity),
                                                               (MAX_ATLAS_BYTES - (MAX_ATLAS_DIMENSION * 4u)) / (pixels * (compact ? 2u : 4u)),
                                                               MAX_ATLAS_DIMENSION * MAX_ATLAS_DIMENSION / pixels}));
        if (mask_capacity == 0u) return false;
        const uint64_t texels = pixels * mask_capacity;
        storage->atlas_width = static_cast<UINT>(std::max<uint64_t>(width, (texels + MAX_ATLAS_DIMENSION - 1u) / MAX_ATLAS_DIMENSION));
        storage->atlas_height = static_cast<UINT>((texels + storage->atlas_width - 1u) / storage->atlas_width);
        storage->width = width;
        storage->height = height;
        storage->mask_capacity = mask_capacity;
        storage->requested_capacity = requested_capacity;
        const D3D11_TEXTURE2D_DESC atlas = {
            .Width = storage->atlas_width,
            .Height = storage->atlas_height,
            .MipLevels = 1u,
            .ArraySize = 1u,
            .Format = (compact ? DXGI_FORMAT_R8G8_UNORM : DXGI_FORMAT_R16G16_FLOAT),
            .SampleDesc = {.Count = 1u},
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
        };
        if (FAILED(device->CreateTexture2D(&atlas, nullptr, &storage->atlas))
            || FAILED(device->CreateShaderResourceView(storage->atlas.Get(), nullptr, &storage->atlas_srv))
            || FAILED(device->CreateUnorderedAccessView(storage->atlas.Get(), nullptr, &storage->atlas_uav))) return false;
        const D3D11_BUFFER_DESC dispatch_data = {
            .ByteWidth = (MAX_LIGHTS * RECORD_BYTES) + 32u,
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
            .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS | D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS,
        };
        if (FAILED(device->CreateBuffer(&dispatch_data, nullptr, &storage->dispatch_data))) return false;
        D3D11_SHADER_RESOURCE_VIEW_DESC input = {};
        input.Format = DXGI_FORMAT_R32_TYPELESS;
        input.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
        input.BufferEx.NumElements = dispatch_data.ByteWidth / 4u;
        input.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
        if (FAILED(device->CreateShaderResourceView(storage->dispatch_data.Get(), &input, &storage->dispatch_srv))) return false;
        for (UINT index = 0u; index < MAX_LIGHTS; ++index) {
          input.BufferEx.FirstElement = index * RECORD_BYTES / 4u;
          input.BufferEx.NumElements = RECORD_BYTES / 4u;
          if (FAILED(device->CreateShaderResourceView(storage->dispatch_data.Get(), &input, &storage->light_dispatch_views[index]))) return false;
        }
        D3D11_UNORDERED_ACCESS_VIEW_DESC output = {};
        output.Format = DXGI_FORMAT_R32_TYPELESS;
        output.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        output.Buffer.NumElements = dispatch_data.ByteWidth / 4u;
        output.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
        if (FAILED(device->CreateUnorderedAccessView(storage->dispatch_data.Get(), &output, &storage->dispatch_uav))) return false;
        const D3D11_BUFFER_DESC light_inputs = {
            .ByteWidth = INPUT_VECTORS * 16u,
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_SHADER_RESOURCE,
        };
        if (FAILED(device->CreateBuffer(&light_inputs, nullptr, &storage->light_buffer))) return false;
        input = {};
        input.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        input.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        input.Buffer.NumElements = INPUT_VECTORS;
        if (FAILED(device->CreateShaderResourceView(storage->light_buffer.Get(), &input, &storage->light_srv))) return false;
        device_state->storage = std::move(storage);
      }
      new_batch->storage = device_state->storage;
    }
    new_batch->light_capacity = new_batch->storage->mask_capacity;
    if ((light_type == 1u || light_type == 3u) && std::isfinite(settings.spot_contact_shadow_budget)
        && settings.spot_contact_shadow_budget >= 1.f && settings.spot_contact_shadow_budget <= static_cast<float>(new_batch->light_capacity)) {
      new_batch->light_capacity = MAX_LIGHTS;
    }
    D3D11_BUFFER_DESC scene_description = {};
    source_scene->GetDesc(&scene_description);
    scene_description.Usage = D3D11_USAGE_DEFAULT;
    scene_description.CPUAccessFlags = 0u;
    if (FAILED(device->CreateBuffer(&scene_description, nullptr, &new_batch->scene_snapshot))) return false;
    std::array<std::byte, SETTINGS_BYTES> settings_bytes = {};
    std::memcpy(settings_bytes.data(), &settings, sizeof(settings));
    const D3D11_BUFFER_DESC controls = {
        .ByteWidth = SETTINGS_BYTES,
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
    };
    const D3D11_SUBRESOURCE_DATA initial = {.pSysMem = settings_bytes.data()};
    if (FAILED(device->CreateBuffer(&controls, &initial, &new_batch->settings_buffer))) return false;
    // Copy the bound GPU scene buffer here to retain its camera jitter and
    // buffer range. Run the batch after this copy and before its first light.
    context->CopyResource(new_batch->scene_snapshot.Get(), source_scene);
    if (!reconstruction::coordinator::ScheduleLighting(
            cmd_list, [new_batch](reshade::api::command_list* submission, bool) { new_batch->Dispatch(submission); },
            reconstruction::coordinator::LightingPolicy::SHADOW_BATCH,
            [new_batch]() { new_batch->collection_complete.store(true, std::memory_order_release); })) return false;
    active_batch = new_batch.get();
    group->collected_batch = new_batch;
  }
  const auto light_index = active_batch->light_count++;
  std::memcpy(active_batch->light_data[1u + (light_index * LIGHT_VECTORS)].data(), native_constants.light.data(), sizeof(native_constants.light));
  active_batch->light_data[LIGHT_VECTORS + (light_index * LIGHT_VECTORS)][0] = std::bit_cast<float>(light_type);
  active_batch->light_data[0] = {std::bit_cast<float>(active_batch->light_count), std::bit_cast<float>(active_batch->storage->atlas_width),
                                 std::bit_cast<float>(active_batch->storage->atlas_height), std::bit_cast<float>(RECORD_BYTES)};
  const std::array<ID3D11ShaderResourceView*, 2> result = {
      active_batch->storage->atlas_srv.Get(), active_batch->storage->light_dispatch_views[light_index].Get()};
  context->PSSetShaderResources(18u, static_cast<UINT>(result.size()), result.data());
  return true;
}

}  // namespace mgsv::shadows::batch

#endif  // MGSV_SHADOWS_BATCH_HPP_
