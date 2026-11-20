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
#include <numeric>
#include <shared_mutex>
#include <span>
#include <utility>
#include <vector>

#include <embed/shaders.h>
#include <include/reshade.hpp>

#include "../../../../utils/mutex.hpp"
#include "../../engine/local_lights.hpp"
#include "../../engine/views.hpp"
#include "../../rendering/d3d11_state.hpp"
#include "../../rendering/submission.hpp"
#include "../../shared.h"

// Contact shadows (Bend screen-space shadows) run as one batch of lights: one
// setup, one clear, and one trace write every light's mask into a shared atlas,
// and each light draw reads its own rectangle. All local lights of a lighting
// pass share the frame batch, which the engine executes before any of their
// draws. A light that batch cannot take, and the sun, record a batch of their
// own beside their draw.
namespace mgsv::shadows::batch {

// These sizes match contact/bend_common.hlsli. A light's record holds its
// projected position, bounds, sweeps, shadow-map projection, atlas location and
// work totals. Its inputs are eleven native vectors, a kind vector, and the
// four vectors of its shadow-map projection.
inline constexpr UINT RECORD_BYTES = 400u;
inline constexpr UINT MAX_LIGHTS = 1024u;
inline constexpr UINT LIGHT_VECTORS = 16u;
inline constexpr UINT LIGHT_KIND = 11u;
inline constexpr UINT LIGHT_SHADOW = 12u;
// Scene constants c0 to c29 of the light draws' b2.
inline constexpr UINT SCENE_BYTES = 480u;
inline constexpr UINT LIGHT_BYTES = 176u;
// Bound a single atlas allocation, including any padding on its final row.
inline constexpr uint64_t MAX_ATLAS_BYTES = 256ull * 1024ull * 1024ull;
inline constexpr uint64_t MAX_ATLAS_DIMENSION = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
// Atlas space in full-screen masks when the spot budget is unlimited.
inline constexpr UINT DEFAULT_ATLAS_SCREENS = 13u;
inline constexpr UINT SETTINGS_BYTES = (sizeof(ShaderInjectData) + 15u) & ~15u;

using LightVector = std::array<float, 4>;

// The user's settings, read under the global settings lock. Set once at startup.
inline const ShaderInjectData* controls = nullptr;

// GPU storage for one batch at a time. Batches reuse it in GPU order: a batch's
// light draws finish reading it before the next batch overwrites it.
struct Storage {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> atlas;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> atlas_srv;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> atlas_uav;
  Microsoft::WRL::ComPtr<ID3D11Buffer> dispatch_data;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> dispatch_srv;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> dispatch_uav;
  std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> light_dispatch_views;
  Microsoft::WRL::ComPtr<ID3D11Buffer> light_buffer;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> light_srv;
  Microsoft::WRL::ComPtr<ID3D11Buffer> scene_buffer;
  Microsoft::WRL::ComPtr<ID3D11Buffer> settings_buffer;
  // Bound as the spot budget when no light of the batch uses one.
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> idle_budget;
  uint32_t width = 0u;
  uint32_t height = 0u;
  uint32_t requested_screens = 0u;
  uint32_t atlas_width = 0u;
  uint32_t atlas_height = 0u;
};

// A 16-byte counter for the setup pass, zeroed at creation.
inline Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> CreateBudget(ID3D11Device* device) {
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
  if (FAILED(device->CreateUnorderedAccessView(buffer.Get(), &output, &view))) return {};
  return view;
}

// The atlas holds up to the requested number of full-screen masks. Setup hands
// that space to lights by their actual bounds, so it serves far more lights.
inline std::shared_ptr<Storage> CreateStorage(ID3D11Device* device, uint32_t width, uint32_t height,
                                              uint32_t requested_screens, uint32_t max_lights) {
  auto storage = std::make_shared<Storage>();
  UINT support = 0u;
  // Compact RG8 stores enough precision for visibility and the bounded radius.
  const bool compact = SUCCEEDED(device->CheckFormatSupport(DXGI_FORMAT_R8G8_UNORM, &support))
                       && (support & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW) != 0u;
  const uint64_t pixels = static_cast<uint64_t>(width) * height;
  const uint64_t screens = std::min({static_cast<uint64_t>(requested_screens),
                                     (MAX_ATLAS_BYTES - (MAX_ATLAS_DIMENSION * 4u)) / (pixels * (compact ? 2u : 4u)),
                                     MAX_ATLAS_DIMENSION * MAX_ATLAS_DIMENSION / pixels});
  if (screens == 0u) return nullptr;
  const uint64_t texels = pixels * screens;
  storage->atlas_width = static_cast<UINT>(std::max<uint64_t>(width, (texels + MAX_ATLAS_DIMENSION - 1u) / MAX_ATLAS_DIMENSION));
  storage->atlas_height = static_cast<UINT>((texels + storage->atlas_width - 1u) / storage->atlas_width);
  storage->width = width;
  storage->height = height;
  storage->requested_screens = requested_screens;
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
      || FAILED(device->CreateUnorderedAccessView(storage->atlas.Get(), nullptr, &storage->atlas_uav))) return nullptr;
  // The records, then the indirect arguments of the clear and the trace.
  const D3D11_BUFFER_DESC dispatch_data = {
      .ByteWidth = (max_lights * RECORD_BYTES) + 32u,
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
      .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS | D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS,
  };
  if (FAILED(device->CreateBuffer(&dispatch_data, nullptr, &storage->dispatch_data))) return nullptr;
  D3D11_SHADER_RESOURCE_VIEW_DESC input = {};
  input.Format = DXGI_FORMAT_R32_TYPELESS;
  input.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
  input.BufferEx.NumElements = dispatch_data.ByteWidth / 4u;
  input.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
  if (FAILED(device->CreateShaderResourceView(storage->dispatch_data.Get(), &input, &storage->dispatch_srv))) return nullptr;
  // Each light draw reads its own record through a view of just that record.
  storage->light_dispatch_views.resize(max_lights);
  for (UINT index = 0u; index < max_lights; ++index) {
    input.BufferEx.FirstElement = index * RECORD_BYTES / 4u;
    input.BufferEx.NumElements = RECORD_BYTES / 4u;
    if (FAILED(device->CreateShaderResourceView(storage->dispatch_data.Get(), &input, &storage->light_dispatch_views[index]))) return nullptr;
  }
  D3D11_UNORDERED_ACCESS_VIEW_DESC output = {};
  output.Format = DXGI_FORMAT_R32_TYPELESS;
  output.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
  output.Buffer.NumElements = dispatch_data.ByteWidth / 4u;
  output.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
  if (FAILED(device->CreateUnorderedAccessView(storage->dispatch_data.Get(), &output, &storage->dispatch_uav))) return nullptr;
  const D3D11_BUFFER_DESC light_inputs = {
      .ByteWidth = (1u + (max_lights * LIGHT_VECTORS)) * 16u,
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_SHADER_RESOURCE,
  };
  if (FAILED(device->CreateBuffer(&light_inputs, nullptr, &storage->light_buffer))) return nullptr;
  input = {};
  input.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  input.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
  input.Buffer.NumElements = light_inputs.ByteWidth / 16u;
  if (FAILED(device->CreateShaderResourceView(storage->light_buffer.Get(), &input, &storage->light_srv))) return nullptr;
  D3D11_BUFFER_DESC constants = {
      .ByteWidth = SCENE_BYTES,
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
  };
  if (FAILED(device->CreateBuffer(&constants, nullptr, &storage->scene_buffer))) return nullptr;
  constants.ByteWidth = SETTINGS_BYTES;
  if (FAILED(device->CreateBuffer(&constants, nullptr, &storage->settings_buffer))) return nullptr;
  storage->idle_budget = CreateBudget(device);
  if (storage->idle_budget == nullptr) return nullptr;
  return storage;
}

struct Batch;

struct __declspec(uuid("DFA07B80-EFCF-4D15-83EE-98B7DA01A5E2")) DeviceState {
  std::shared_mutex mutex;
  std::array<Microsoft::WRL::ComPtr<ID3D11ComputeShader>, 3> shaders;
  Microsoft::WRL::ComPtr<ID3D11SamplerState> border;
  // The frame batch's storage, and one-light storage for batches beside a draw.
  std::shared_ptr<Storage> frame_storage;
  std::shared_ptr<Storage> local_storage;
  // Frame batches whose lighting pass is still recording.
  std::vector<std::shared_ptr<Batch>> open;
  bool native_hook = false;
};

// Records setup, clear, and trace. The caller has uploaded the inputs and
// preserves the compute state these calls change.
inline void RecordPasses(ID3D11DeviceContext* context, const DeviceState& programs, const Storage& storage,
                         ID3D11ShaderResourceView* depth, ID3D11UnorderedAccessView* budget, UINT light_count) {
  constexpr std::array<ID3D11ShaderResourceView*, 1> no_input = {};
  constexpr std::array<ID3D11UnorderedAccessView*, 3> no_outputs = {};
  context->CSSetConstantBuffers(2u, 1u, storage.scene_buffer.GetAddressOf());
  context->CSSetConstantBuffers(13u, 1u, storage.settings_buffer.GetAddressOf());
  context->CSSetShaderResources(0u, 1u, &depth);
  context->CSSetShaderResources(3u, 1u, no_input.data());
  context->CSSetShaderResources(4u, 1u, storage.light_srv.GetAddressOf());
  const std::array<ID3D11UnorderedAccessView*, 3> outputs = {
      storage.dispatch_uav.Get(), storage.atlas_uav.Get(), budget != nullptr ? budget : storage.idle_budget.Get()};
  context->CSSetUnorderedAccessViews(0u, static_cast<UINT>(outputs.size()), outputs.data(), nullptr);
  context->CSSetSamplers(0u, 1u, programs.border.GetAddressOf());
  context->CSSetShader(programs.shaders[0].Get(), nullptr, 0u);
  // Setup admits lights in draw order and publishes the indirect arguments.
  // Clear and trace then process all admitted rectangles in parallel.
  context->Dispatch(1u, 1u, 1u);
  context->CSSetUnorderedAccessViews(0u, 1u, no_outputs.data(), nullptr);
  context->CSSetShaderResources(3u, 1u, storage.dispatch_srv.GetAddressOf());
  context->CSSetShader(programs.shaders[1].Get(), nullptr, 0u);
  context->DispatchIndirect(storage.dispatch_data.Get(), light_count * RECORD_BYTES);
  context->CSSetShader(programs.shaders[2].Get(), nullptr, 0u);
  context->DispatchIndirect(storage.dispatch_data.Get(), (light_count * RECORD_BYTES) + 16u);
  // Unbind the writable views before a light draw reads them.
  context->CSSetUnorderedAccessViews(0u, static_cast<UINT>(no_outputs.size()), no_outputs.data(), nullptr);
}

// The local lights of one lighting pass. Light draws on any recording thread
// append their CPU constants; the dispatch reserved at the start of the pass
// uploads them when it executes, which is after all of that recording.
struct Batch {
  const DeviceState* programs = nullptr;
  std::shared_ptr<Storage> storage;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> budget;
  ShaderInjectData settings = {};
  std::array<float, SCENE_BYTES / 4u> scene = {};
  // The header vector, then LIGHT_VECTORS per light.
  std::vector<LightVector> lights;
  // Per light: the node its draw was recorded in, then its record index. One
  // thread records a node and nodes execute in serial order, so sorting by this
  // gives draw order.
  std::vector<std::pair<uint64_t, uint32_t>> draw_order;
  uint64_t scene_id = 0u;
  uint64_t owner_generation = 0u;
  uint64_t input_generation = 0u;
  std::mutex mutex;
  // Set under the mutex when the dispatch first executes. Later lights cannot join.
  std::atomic<bool> sealed = false;

  void Dispatch(reshade::api::command_list* cmd_list) {
    UINT light_count = 0u;
    {
      const std::scoped_lock lock(mutex);
      sealed = true;
      light_count = static_cast<UINT>(draw_order.size());
      if (light_count == 0u) return;
      std::vector<uint32_t> order(light_count);
      std::iota(order.begin(), order.end(), 0u);
      std::ranges::stable_sort(order, {}, [this](uint32_t index) { return draw_order[index]; });
      for (UINT position = 0u; position < light_count; ++position) {
        lights[1u + (position * LIGHT_VECTORS) + LIGHT_KIND][2] = std::bit_cast<float>(order[position]);
      }
      lights[0] = {std::bit_cast<float>(light_count), std::bit_cast<float>(storage->atlas_width),
                   std::bit_cast<float>(storage->atlas_height), std::bit_cast<float>(RECORD_BYTES)};
    }
    // A sealed batch no longer changes, so a replayed list uploads the same lights.
    auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
    const auto saved = rendering::d3d11_compute_state::Capture(cmd_list);
    // Light draws read the atlas and records at PS t18-t19. Unbind them before
    // the passes write those resources.
    constexpr std::array<ID3D11ShaderResourceView*, 2> empty_views = {};
    context->PSSetShaderResources(18u, static_cast<UINT>(empty_views.size()), empty_views.data());
    // DEFAULT storage supports both immediate execution and command lists
    // merged into another deferred context.
    const D3D11_BOX used = {.left = 0u, .top = 0u, .front = 0u, .right = static_cast<UINT>(lights.size() * sizeof(LightVector)), .bottom = 1u, .back = 1u};
    context->UpdateSubresource(storage->light_buffer.Get(), 0u, &used, lights.data(), 0u, 0u);
    context->UpdateSubresource(storage->scene_buffer.Get(), 0u, nullptr, scene.data(), 0u, 0u);
    std::array<std::byte, SETTINGS_BYTES> settings_bytes = {};
    std::memcpy(settings_bytes.data(), &settings, sizeof(settings));
    context->UpdateSubresource(storage->settings_buffer.Get(), 0u, nullptr, settings_bytes.data(), 0u, 0u);
    RecordPasses(context, *programs, *storage, depth.Get(), budget.Get(), light_count);
    rendering::d3d11_compute_state::Restore(cmd_list, saved);
    static std::atomic_flag reported;
    if (!reported.test_and_set(std::memory_order_relaxed)) {
      const auto message = std::format("MgsvShadows: one batch recorded for {} local light draws; atlas {} x {}",
                                       light_count, storage->atlas_width, storage->atlas_height);
      reshade::log::message(reshade::log::level::info, message.c_str());
    }
  }
};

// Creates the three programs and the border sampler once per device.
inline bool PreparePrograms(ID3D11Device* device, DeviceState* device_state) {
  {
    const std::shared_lock lock(device_state->mutex);
    if (device_state->shaders[0] != nullptr) return true;
  }
  std::array<Microsoft::WRL::ComPtr<ID3D11ComputeShader>, 3> shaders;
  Microsoft::WRL::ComPtr<ID3D11SamplerState> border;
  const std::array<std::span<const uint8_t>, 3> bytecode = {__bend_setup, __bend_clear, __bend_trace};
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
  if (FAILED(device->CreateSamplerState(&sampler, &border))) return false;
  const std::unique_lock lock(device_state->mutex);
  if (device_state->shaders[0] == nullptr) {
    device_state->shaders = std::move(shaders);
    device_state->border = std::move(border);
  }
  return true;
}

// Runs on the build thread when the engine's local-light plugin begins, before
// it branches the jobs that draw the lights. Reserves the frame batch's
// dispatch at this point of the build thread's command list: the engine
// executes that list before every job list, and only after all of them have
// been recorded.
inline void BeginLocalLights() {
  try {
    if (controls == nullptr) return;
    {
      const std::shared_lock lock(renodx::utils::mutex::global_mutex);
      if (controls->shadows_mode < 0.5f) return;
    }
    rendering::submission::install_requested.store(true, std::memory_order_release);
    const auto scope = engine::views::GetScope();
    if (scope.owner.scene == 0u) return;
    const auto context = engine::views::Unwrap(engine::views::GetRecording().d3d_context);
    // On an immediate context the dispatch would run now, before any light exists.
    if (context == nullptr || context->GetType() != D3D11_DEVICE_CONTEXT_DEFERRED) return;
    auto* cmd_list = rendering::submission::FindRecordingList(context.Get());
    if (cmd_list == nullptr) return;
    auto* device_state = cmd_list->get_device()->get_private_data<DeviceState>();
    if (device_state == nullptr
        || !PreparePrograms(reinterpret_cast<ID3D11Device*>(cmd_list->get_device()->get_native()), device_state)) return;  // NOLINT(performance-no-int-to-ptr)
    auto batch = std::make_shared<Batch>();
    batch->programs = device_state;
    batch->scene_id = scope.owner.scene;
    batch->owner_generation = scope.owner.generation;
    {
      const std::unique_lock lock(device_state->mutex);
      // A sealed batch has executed; a pass that never executed leaves its batch
      // here until newer passes push it out.
      std::erase_if(device_state->open, [](const std::shared_ptr<Batch>& candidate) {
        return candidate->sealed.load(std::memory_order_acquire);
      });
      if (device_state->open.size() >= 8u) {
        device_state->open.erase(device_state->open.begin());
      }
      device_state->open.push_back(batch);
    }
    // The batch runs on every execution. Inside another deferred list it is
    // recorded between its native prefix and remainder, so visibility is
    // computed again on every replay from that list's current depth.
    const auto dispatch = [batch](reshade::api::command_list* submission, const auto&) { batch->Dispatch(submission); };
    if (!rendering::submission::Schedule(cmd_list, {.run = dispatch, .run_merged = dispatch, .leaves_room = true})) {
      const std::unique_lock lock(device_state->mutex);
      std::erase(device_state->open, batch);
      static std::atomic_flag reported;
      if (!reported.test_and_set(std::memory_order_relaxed)) {
        reshade::log::message(reshade::log::level::warning, "MgsvShadows: could not reserve the local-light batch; lights run their own contact-shadow passes");
      }
    }
  } catch (...) {
    // The native plugin continues; its lights record separate batches.
  }
}

// Adds a local light draw to its lighting pass's frame batch and returns the
// views that draw reads. Fails when no batch is open for this pass, the light's
// CPU constants cannot be read, or the batch cannot take it.
inline bool CollectLight(ID3D11DeviceContext* context, DeviceState* device_state, const ShaderInjectData& settings,
                         uint32_t light_type, bool has_shadow_map, uint32_t width, uint32_t height, uint64_t input_generation,
                         ID3D11ShaderResourceView* depth, ID3D11RenderTargetView* target, ID3D11UnorderedAccessView* budget,
                         std::array<ID3D11ShaderResourceView*, 2>* views) {
  if (light_type == 0u) return false;
  const auto scope = engine::views::GetScope();
  if (scope.owner.scene == 0u) return false;
  std::shared_ptr<Batch> batch;
  {
    const std::shared_lock lock(device_state->mutex);
    const auto found = std::ranges::find_if(device_state->open, [&scope](const std::shared_ptr<Batch>& candidate) {
      return candidate->scene_id == scope.owner.scene && candidate->owner_generation == scope.owner.generation;
    });
    if (found == device_state->open.end()) return false;
    batch = *found;
  }
  // A light whose pass has a batch should be in it. Report each cause once.
  const auto rejected = [](uint32_t reason, const char* cause) {
    static std::array<std::atomic_flag, 5> reported = {};
    if (!reported[reason].test_and_set(std::memory_order_relaxed)) {
      const auto message = std::format("MgsvShadows: a local light runs its own contact-shadow passes because {}", cause);
      reshade::log::message(reshade::log::level::warning, message.c_str());
    }
    return false;
  };
  engine::local_lights::Constants constants;
  if (!engine::local_lights::ReadConstants(engine::views::GetRecording().command_buffer, context, &constants)) {
    return rejected(0u, "its CPU constants could not be read");
  }

  // Light draws only test depth. A draw that could write it would make the
  // depth the batch traced stale for the lights after it.
  Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_state;
  context->OMGetDepthStencilState(&depth_state, nullptr);
  if (depth_state == nullptr) return rejected(1u, "its draw can write depth");
  D3D11_DEPTH_STENCIL_DESC depth_description = {};
  depth_state->GetDesc(&depth_description);
  if (depth_description.DepthEnable != FALSE && depth_description.DepthWriteMask != D3D11_DEPTH_WRITE_MASK_ZERO) {
    return rejected(1u, "its draw can write depth");
  }

  // Jobs can bind their own views of the same depth and lighting textures.
  const auto same_texture = [](ID3D11View* first, ID3D11View* second) {
    if (first == second) return true;
    Microsoft::WRL::ComPtr<ID3D11Resource> first_resource;
    Microsoft::WRL::ComPtr<ID3D11Resource> second_resource;
    first->GetResource(&first_resource);
    second->GetResource(&second_resource);
    return first_resource == second_resource;
  };
  constexpr const char* different_inputs = "its depth, target, settings, camera or spot budget differ from the batch's";
  const std::scoped_lock lock(batch->mutex);
  if (batch->sealed || batch->draw_order.size() == MAX_LIGHTS) return rejected(2u, "its lighting pass's batch has already run or is full");
  if (batch->draw_order.empty()) {
    // The first light fixes what the whole batch traces against.
    const auto requested_screens = (std::isfinite(settings.spot_contact_shadow_budget) && settings.spot_contact_shadow_budget >= 1.f
                                        ? static_cast<UINT>(std::min(settings.spot_contact_shadow_budget, 256.f))
                                        : DEFAULT_ATLAS_SCREENS);
    {
      const std::unique_lock storage_lock(device_state->mutex);
      if (device_state->frame_storage == nullptr || device_state->frame_storage->width != width
          || device_state->frame_storage->height != height
          || device_state->frame_storage->requested_screens != requested_screens) {
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        context->GetDevice(&device);
        device_state->frame_storage = CreateStorage(device.Get(), width, height, requested_screens, MAX_LIGHTS);
        if (device_state->frame_storage == nullptr) return rejected(4u, "the batch's storage could not be created");
      }
      batch->storage = device_state->frame_storage;
    }
    batch->depth = depth;
    batch->target = target;
    batch->settings = settings;
    batch->scene = constants.scene;
    batch->input_generation = input_generation;
    batch->lights.assign(1u, LightVector{});
  } else {
    // The batch traces every light with one camera. Each light keeps its own
    // shadow-map projection (c12 to c15), so those vectors may differ.
    const auto& scene = batch->scene;
    const bool same_camera = std::memcmp(&scene[4u * 4u], &constants.scene[4u * 4u], 4u * 16u) == 0
                             && std::memcmp(&scene[21u * 4u], &constants.scene[21u * 4u], 2u * 16u) == 0
                             && std::memcmp(&scene[28u * 4u], &constants.scene[28u * 4u], 16u) == 0;
    // Only the settings the three passes read. Per-frame values elsewhere in the
    // layout can change while a lighting pass is still recording.
    const auto& traced = batch->settings;
    const bool same_settings = traced.shadow_softness == settings.shadow_softness
                               && traced.contact_shadow_reach == settings.contact_shadow_reach
                               && traced.contact_shadow_render_scale == settings.contact_shadow_render_scale
                               && traced.contact_shadow_debug_view == settings.contact_shadow_debug_view
                               && traced.shadow_debug_light == settings.shadow_debug_light
                               && traced.spot_contact_shadow_budget == settings.spot_contact_shadow_budget;
    if (!same_texture(batch->depth.Get(), depth) || !same_texture(batch->target.Get(), target)
        || batch->input_generation != input_generation || batch->storage->width != width || batch->storage->height != height
        || !same_settings || !same_camera) {
      return rejected(3u, different_inputs);
    }
  }
  if (budget != nullptr) {
    // Every budgeted spot of a lighting pass counts against the same counter.
    if (batch->budget != nullptr && batch->budget.Get() != budget) return rejected(3u, different_inputs);
    batch->budget = budget;
  }
  const auto index = static_cast<uint32_t>(batch->draw_order.size());
  batch->draw_order.emplace_back(scope.node, index);
  batch->lights.resize(1u + ((index + 1u) * LIGHT_VECTORS));
  auto* light = &batch->lights[1u + (index * LIGHT_VECTORS)];
  std::memcpy(light, constants.light.data(), sizeof(constants.light));
  light[LIGHT_KIND] = {std::bit_cast<float>(light_type), std::bit_cast<float>(has_shadow_map ? 1u : 0u), 0.f, 0.f};
  if (has_shadow_map) {
    std::memcpy(&light[LIGHT_SHADOW], &constants.scene[12u * 4u], 4u * sizeof(LightVector));
  }
  *views = {batch->storage->atlas_srv.Get(), batch->storage->light_dispatch_views[index].Get()};
  return true;
}

// Records a one-light batch beside its draw: the sun, or a local light the
// frame batch cannot take. Its constants are copied on the GPU from the draw's
// own bound buffers, so this path needs nothing from the engine.
inline bool RecordLight(ID3D11DeviceContext* context, DeviceState* device_state, const ShaderInjectData& settings,
                        uint32_t light_type, bool has_shadow_map, uint32_t width, uint32_t height,
                        ID3D11ShaderResourceView* depth, ID3D11UnorderedAccessView* budget,
                        const std::array<ID3D11Buffer*, 2>& native_constants, const std::array<UINT, 2>& first_constants,
                        std::array<ID3D11ShaderResourceView*, 2>* views) {
  Microsoft::WRL::ComPtr<ID3D11Device> device;
  context->GetDevice(&device);
  if (!PreparePrograms(device.Get(), device_state)) return false;
  std::shared_ptr<Storage> storage;
  {
    const std::unique_lock lock(device_state->mutex);
    if (device_state->local_storage == nullptr || device_state->local_storage->width != width
        || device_state->local_storage->height != height) {
      device_state->local_storage = CreateStorage(device.Get(), width, height, 1u, 1u);
      if (device_state->local_storage == nullptr) return false;
    }
    storage = device_state->local_storage;
  }
  // The header and the kind come from here; the light's vectors are copied
  // from its native buffers below. A box starting at zero avoids the deferred
  // context's offset error for partial updates.
  std::array<LightVector, 1u + LIGHT_VECTORS> inputs = {};
  inputs[0] = {std::bit_cast<float>(1u), std::bit_cast<float>(storage->atlas_width),
               std::bit_cast<float>(storage->atlas_height), std::bit_cast<float>(RECORD_BYTES)};
  inputs[1u + LIGHT_KIND] = {std::bit_cast<float>(light_type), std::bit_cast<float>(has_shadow_map ? 1u : 0u), 0.f, 0.f};
  const D3D11_BOX all_inputs = {.left = 0u, .top = 0u, .front = 0u, .right = sizeof(inputs), .bottom = 1u, .back = 1u};
  context->UpdateSubresource(storage->light_buffer.Get(), 0u, &all_inputs, inputs.data(), 0u, 0u);
  const UINT scene_start = first_constants[0] * 16u;
  const UINT light_start = first_constants[1] * 16u;
  const D3D11_BOX light_box = {.left = light_start, .top = 0u, .front = 0u, .right = light_start + LIGHT_BYTES, .bottom = 1u, .back = 1u};
  context->CopySubresourceRegion(storage->light_buffer.Get(), 0u, 16u, 0u, 0u, native_constants[1], 0u, &light_box);
  if (has_shadow_map) {
    const D3D11_BOX shadow_box = {.left = scene_start + (12u * 16u), .top = 0u, .front = 0u, .right = scene_start + (16u * 16u), .bottom = 1u, .back = 1u};
    context->CopySubresourceRegion(storage->light_buffer.Get(), 0u, (1u + LIGHT_SHADOW) * 16u, 0u, 0u, native_constants[0], 0u, &shadow_box);
  }
  const D3D11_BOX scene_box = {.left = scene_start, .top = 0u, .front = 0u, .right = scene_start + SCENE_BYTES, .bottom = 1u, .back = 1u};
  context->CopySubresourceRegion(storage->scene_buffer.Get(), 0u, 0u, 0u, 0u, native_constants[0], 0u, &scene_box);
  std::array<std::byte, SETTINGS_BYTES> settings_bytes = {};
  std::memcpy(settings_bytes.data(), &settings, sizeof(settings));
  context->UpdateSubresource(storage->settings_buffer.Get(), 0u, nullptr, settings_bytes.data(), 0u, 0u);

  // The light draw's graphics state stays as the game set it: native graphics
  // never binds these outputs, and the caller has unbound PS t18/t19. Only the
  // compute bindings the passes use are saved and put back.
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
  // Slots t0, t3 and t4.
  std::array<ID3D11ShaderResourceView*, 3> saved_inputs = {};
  std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 3> saved_input_refs;
  context->CSGetShaderResources(0u, 1u, saved_inputs.data());
  context->CSGetShaderResources(3u, 2u, saved_inputs.data() + 1u);
  for (size_t index = 0u; index < saved_inputs.size(); ++index) {
    saved_input_refs[index].Attach(saved_inputs[index]);
  }
  std::array<ID3D11UnorderedAccessView*, 3> saved_outputs = {};
  std::array<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>, 3> saved_output_refs;
  context->CSGetUnorderedAccessViews(0u, static_cast<UINT>(saved_outputs.size()), saved_outputs.data());
  for (size_t index = 0u; index < saved_outputs.size(); ++index) {
    saved_output_refs[index].Attach(saved_outputs[index]);
  }
  std::array<ID3D11Buffer*, 2> saved_buffers = {};
  std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, 2> saved_buffer_refs;
  std::array<UINT, 2> saved_first = {};
  std::array<UINT, 2> saved_counts = {};
  Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context1;
  const bool ranged = SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&context1)));
  if (ranged) {
    context1->CSGetConstantBuffers1(2u, 1u, saved_buffers.data(), saved_first.data(), saved_counts.data());
    context1->CSGetConstantBuffers1(13u, 1u, saved_buffers.data() + 1u, saved_first.data() + 1u, saved_counts.data() + 1u);
  } else {
    context->CSGetConstantBuffers(2u, 1u, saved_buffers.data());
    context->CSGetConstantBuffers(13u, 1u, saved_buffers.data() + 1u);
  }
  for (size_t index = 0u; index < saved_buffers.size(); ++index) {
    saved_buffer_refs[index].Attach(saved_buffers[index]);
  }
  RecordPasses(context, *device_state, *storage, depth, budget, 1u);
  context->CSSetShaderResources(0u, 1u, saved_inputs.data());
  context->CSSetShaderResources(3u, 2u, saved_inputs.data() + 1u);
  context->CSSetUnorderedAccessViews(0u, static_cast<UINT>(saved_outputs.size()), saved_outputs.data(), nullptr);
  if (ranged) {
    context1->CSSetConstantBuffers1(2u, 1u, saved_buffers.data(), saved_first.data(), saved_counts.data());
    context1->CSSetConstantBuffers1(13u, 1u, saved_buffers.data() + 1u, saved_first.data() + 1u, saved_counts.data() + 1u);
  } else {
    context->CSSetConstantBuffers(2u, 1u, saved_buffers.data());
    context->CSSetConstantBuffers(13u, 1u, saved_buffers.data() + 1u);
  }
  context->CSSetSamplers(0u, 1u, saved_sampler.GetAddressOf());
  context->CSSetShader(saved_shader.Get(), instances.data(), instance_count);
  *views = {storage->atlas_srv.Get(), storage->light_dispatch_views[0].Get()};
  return true;
}

}  // namespace mgsv::shadows::batch

#endif  // MGSV_SHADOWS_BATCH_HPP_
