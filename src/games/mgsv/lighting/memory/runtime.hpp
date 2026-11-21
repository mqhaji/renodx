// SPDX-License-Identifier: MIT
#ifndef MGSV_LIGHTING_MEMORY_RUNTIME_HPP_
#define MGSV_LIGHTING_MEMORY_RUNTIME_HPP_

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <shared_mutex>
#include <span>

#include <d3d11_1.h>
#include <d3d12.h>
#include <embed/shaders.h>
#include <wrl/client.h>

#include <include/reshade.hpp>

#include "../../engine/pause.hpp"
#include "../../rendering/d3d12_bridge.hpp"
#include "../../rendering/d3d12_compute.hpp"
#include "probe_api.hpp"

// Scene memory: a world-space record of the surfaces the camera has seen, in
// four nested grids that follow the camera. It runs inside the reflection
// round trip. At the reflection composite, after reflection preparation, a
// D3D11 pass turns one pixel per tile into a world-space sample
// (sample_memory). On D3D12, after the reflection resolve, the state pass
// counts the samples, keeps the light direction and places the grids
// (memory_state), the scroll pass forgets cells that wrapped into a moved
// window (memory_scroll), the carve pass empties cells camera rays see through
// (memory_carve), the mark pass stores the samples with their color and sun
// visibility (memory_mark), and for the scene memory debug views the inspect
// pass draws the grids (memory_inspect). Where the game draws terrain, the
// engine's terrain textures are kept from a terrain draw (CaptureTerrain), a
// second D3D11 pass copies the heights around the eye out of them
// (memory_window), and a D3D12 pass before carving fills the grids from those
// (memory_terrain). The host reads the state back when the command slot comes
// around again. Upkeep runs while one of those views is selected or the probe
// asks for it.
namespace mgsv::lighting::memory {

namespace bridge_api = rendering::d3d12_bridge;
namespace compute = rendering::d3d12_compute;
using compute::Check;
using compute::ImageView;

inline constexpr UINT SAMPLE_WIDTH = 256u;
inline constexpr UINT SAMPLE_HEIGHT = 64u;
// One carving point per tile of this grid over the render, as in memory_carve.
inline constexpr UINT CARVE_WIDTH = 256u;
inline constexpr UINT CARVE_HEIGHT = 128u;
// Four levels of 128 x 64 x 128 cells stacked along z, as in common.hlsli.
inline constexpr UINT GRID_WIDTH = 128u;
inline constexpr UINT GRID_HEIGHT = 64u;
inline constexpr UINT16 GRID_DEPTH = 512u;
// Surface color and sun visibility, at half that resolution.
inline constexpr UINT HALF_WIDTH = GRID_WIDTH / 2u;
inline constexpr UINT HALF_HEIGHT = GRID_HEIGHT / 2u;
inline constexpr UINT16 HALF_DEPTH = GRID_DEPTH / 2u;
inline constexpr size_t LEVELS = 4u;
inline constexpr UINT STATE_WORDS = 64u;
// Field positions in the state image, as in common.hlsli.
inline constexpr size_t STATE_VALID = 0u;
inline constexpr size_t STATE_MOVING = 1u;
inline constexpr size_t STATE_ATLAS = 2u;
inline constexpr size_t STATE_COVERED = 3u;
inline constexpr size_t STATE_FACING = 4u;
inline constexpr size_t STATE_SUN_EPOCH = 5u;
inline constexpr size_t STATE_SUN_DIRECTION = 6u;
inline constexpr size_t STATE_EYE = 9u;
inline constexpr size_t STATE_EYE_MISMATCH = 15u;
inline constexpr size_t STATE_FRAMES = 16u;
inline constexpr size_t STATE_NEAR_CELLS = 17u;
inline constexpr size_t STATE_MARKED = 21u;
inline constexpr size_t STATE_MARKING = 22u;
inline constexpr size_t STATE_CARVED = 23u;
inline constexpr size_t STATE_ORIGIN = 24u;
inline constexpr size_t STATE_KNOWN_CELLS = 48u;
inline constexpr size_t STATE_ALBEDO_WRITTEN = 52u;
inline constexpr size_t STATE_SUN_WRITTEN = 53u;
inline constexpr size_t STATE_TERRAIN_CELLS = 54u;
inline constexpr size_t STATE_FORWARD = 55u;
inline constexpr size_t STATE_EXPOSURE = 58u;
// The terrain window's side in lattice points, as in common.hlsli.
inline constexpr UINT TERRAIN_WINDOW = 256u;
// The terrain pass takes one z slice in this many per update, as in
// memory_terrain.
inline constexpr UINT TERRAIN_SLICE_STRIDE = 8u;
// A terrain draw's cVSMaterial and cVSObject.
inline constexpr UINT TERRAIN_MATERIAL_BYTES = 128u;
inline constexpr UINT TERRAIN_OBJECT_BYTES = 208u;
// Terrain draws refresh the kept textures and constants this often.
inline constexpr auto TERRAIN_CAPTURE_INTERVAL = std::chrono::milliseconds(250);
// Pass flags, as in common.hlsli.
inline constexpr uint32_t FLAG_CLEAR = 1u;
inline constexpr uint32_t FLAG_PAUSED = 2u;
inline constexpr uint32_t FLAG_PAUSE_UNKNOWN = 4u;
inline constexpr uint32_t FLAG_CENSUS = 8u;
// Lighting debug views the inspect pass draws: Scene Memory, Behind Camera,
// Agreement and Sun Visibility.
inline constexpr int FIRST_VIEW = 18;
inline constexpr int LAST_VIEW = 21;
// The frame drawn can lag the game frame the pause word belongs to, so the
// first frames drawn after a pause can still be frozen ones, whose moving
// objects the engine has not marked. Marking resumes with the update this
// many after the last paused one.
inline constexpr uint32_t RESUME_UPDATES = 4u;
// An update this long after the previous one (a load, a menu without the
// scene, upkeep switched off) starts over from empty grids.
inline constexpr auto CLEAR_AFTER = std::chrono::seconds(1);

// b2 of every D3D12 pass and of the sample pass.
struct Constants {
  uint32_t flags = 0u;
  uint32_t frame = 0u;
  uint32_t view = 0u;
  uint32_t padding = 0u;
};

// The engine's terrain textures and copies of a terrain draw's constants.
struct TerrainInputs {
  // VS t0: each tile's page in the height atlas and its level.
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tiles;
  // VS t1: the height atlas.
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> heights;
  Microsoft::WRL::ComPtr<ID3D11Buffer> material;
  Microsoft::WRL::ComPtr<ID3D11Buffer> object;
};

// A copy of the state, recorded into one command slot. The bridge reuses a
// slot only after its work completed, so the copy is ready when the slot
// records again. It takes the first free slot, so a slot used once while the
// GPU was behind can keep its copy for many submissions.
struct Readback {
  Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
  // The update the copy belongs to; 0 while no copy waits.
  uint64_t submission = 0u;
  std::optional<std::array<double, 3>> host_eye;
};

struct Runtime {
  Microsoft::WRL::ComPtr<ID3D12Device> device;
  compute::SharedInput samples;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> sample;
  Microsoft::WRL::ComPtr<ID3D11Buffer> constants;
  compute::Pipeline state_pass;
  compute::Pipeline scroll_pass;
  compute::Pipeline carve_pass;
  compute::Pipeline mark_pass;
  compute::Pipeline inspect_pass;
  compute::Pipeline terrain_pass;
  compute::SharedInput window;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> window_shader;
  Microsoft::WRL::ComPtr<ID3D12Resource> state;
  Microsoft::WRL::ComPtr<ID3D12Resource> grid;
  Microsoft::WRL::ComPtr<ID3D12Resource> albedo;
  Microsoft::WRL::ComPtr<ID3D12Resource> sun;
  std::array<Readback, bridge_api::Bridge::MAX_IN_FLIGHT> readbacks;
  uint32_t frame = 0u;
  uint64_t submissions = 0u;
  uint64_t paused_submissions = 0u;
  // The update whose copy was read last; older copies are dropped.
  uint64_t newest_read = 0u;
  // New grids hold no data; they stay pending until a clearing update is
  // submitted.
  bool clear_pending = true;
  uint64_t clears = 0u;
  std::chrono::steady_clock::time_point last_submitted;
  // Updates recorded since game time last stopped.
  uint32_t updates_since_pause = RESUME_UPDATES;
  // What the update being recorded used, for Sample and Submitted.
  bool recorded_clear = false;
  std::optional<bool> recorded_paused;
  int recorded_view = 0;
  // Empty where the game has drawn no terrain within CLEAR_AFTER.
  TerrainInputs recorded_terrain;
};

// Used only on the thread that holds the submission execution lock.
static Runtime runtime;
static std::atomic_bool enabled = false;
struct Reported {
  std::shared_mutex mutex;
  probe::Status status;
};
static Reported reported;
// Written by the threads that record terrain draws.
struct Terrain {
  std::shared_mutex mutex;
  TerrainInputs inputs;
  ID3D11Device* device = nullptr;
  std::chrono::steady_clock::time_point captured;
};
static Terrain terrain;
// When the last update was submitted, as steady_clock ticks, for those threads.
static std::atomic<std::chrono::steady_clock::rep> last_update = 0;

inline bool IsMemoryView(int debug_view) { return debug_view >= FIRST_VIEW && debug_view <= LAST_VIEW; }

inline void ReadProbeStatus(probe::Status* status) {
  const std::shared_lock lock(reported.mutex);
  *status = reported.status;
  status->enabled = enabled.load(std::memory_order_relaxed);
}

inline void UseProbe(bool attach) {
  if (attach) {
    probe::shared.RegisterModule();
    if (probe::shared.data == nullptr) return;
    const std::unique_lock lock(probe::shared.data->mutex);
    probe::shared.data->enable = [](bool enable) { enabled.store(enable, std::memory_order_relaxed); };
    probe::shared.data->read = ReadProbeStatus;
  } else if (probe::shared.data != nullptr) {
    {
      const std::unique_lock lock(probe::shared.data->mutex);
      probe::shared.data->enable = nullptr;
      probe::shared.data->read = nullptr;
    }
    probe::shared.UnregisterModule();
  }
}

// The caller drains the bridge first.
inline void Destroy() {
  runtime = {};
  {
    const std::unique_lock lock(terrain.mutex);
    terrain.inputs = {};
    terrain.device = nullptr;
    terrain.captured = {};
  }
  const std::unique_lock lock(reported.mutex);
  reported.status = {};
}

// Draw callback of the terrain depth and shadow shaders. Every terrain draw
// binds the same tile map (VS t0) and height atlas (VS t1), and map-wide
// values in its cVSMaterial (b4) and cVSObject (b5). While scene memory is
// updating, keep the views and copy the constants on the GPU a few times a
// second; the copies take effect when this command list runs.
inline bool CaptureTerrain(reshade::api::command_list* cmd_list) {
  const auto now = std::chrono::steady_clock::now();
  if (now.time_since_epoch().count() - last_update.load(std::memory_order_relaxed) > std::chrono::steady_clock::duration(CLEAR_AFTER).count()) return true;
  {
    const std::shared_lock lock(terrain.mutex);
    if (now - terrain.captured < TERRAIN_CAPTURE_INTERVAL) return true;
  }
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11DeviceContext1> ranged;
  if (context == nullptr || FAILED(context->QueryInterface(IID_PPV_ARGS(&ranged)))) return true;
  std::array<ID3D11ShaderResourceView*, 2> views = {};
  context->VSGetShaderResources(0u, 2u, views.data());
  TerrainInputs bound;
  bound.tiles.Attach(views[0]);
  bound.heights.Attach(views[1]);
  std::array<ID3D11Buffer*, 2> buffers = {};
  std::array<UINT, 2> first = {};
  std::array<UINT, 2> count = {};
  ranged->VSGetConstantBuffers1(4u, 2u, buffers.data(), first.data(), count.data());
  bound.material.Attach(buffers[0]);
  bound.object.Attach(buffers[1]);
  if (bound.tiles == nullptr || bound.heights == nullptr || bound.material == nullptr || bound.object == nullptr) return true;
  constexpr std::array<UINT, 2> bytes = {TERRAIN_MATERIAL_BYTES, TERRAIN_OBJECT_BYTES};
  const std::array<ID3D11Buffer*, 2> sources = {bound.material.Get(), bound.object.Get()};
  for (size_t index = 0u; index < sources.size(); ++index) {
    D3D11_BUFFER_DESC description = {};
    sources[index]->GetDesc(&description);
    if ((static_cast<uint64_t>(first[index]) * 16u) + bytes[index] > description.ByteWidth) return true;
  }
  auto* device = reinterpret_cast<ID3D11Device*>(cmd_list->get_device()->get_native());  // NOLINT(performance-no-int-to-ptr)
  const std::unique_lock lock(terrain.mutex);
  if (terrain.device != device) {
    TerrainInputs created;
    D3D11_BUFFER_DESC description = {.ByteWidth = TERRAIN_MATERIAL_BYTES, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_CONSTANT_BUFFER};
    if (FAILED(device->CreateBuffer(&description, nullptr, &created.material))) return true;
    description.ByteWidth = TERRAIN_OBJECT_BYTES;
    if (FAILED(device->CreateBuffer(&description, nullptr, &created.object))) return true;
    terrain.inputs = std::move(created);
    terrain.device = device;
  }
  const std::array<ID3D11Buffer*, 2> copies = {terrain.inputs.material.Get(), terrain.inputs.object.Get()};
  for (size_t index = 0u; index < sources.size(); ++index) {
    const D3D11_BOX box = {.left = first[index] * 16u, .top = 0u, .front = 0u, .right = (first[index] * 16u) + bytes[index], .bottom = 1u, .back = 1u};
    context->CopySubresourceRegion(copies[index], 0u, 0u, 0u, 0u, sources[index], 0u, &box);
  }
  terrain.inputs.tiles = std::move(bound.tiles);
  terrain.inputs.heights = std::move(bound.heights);
  terrain.captured = now;
  return true;
}

// Creates the resources on first use and after a device change.
inline void Prepare(bridge_api::Bridge* bridge) {
  if (runtime.device == bridge->device12) return;
  runtime = {};
  auto* device = bridge->device12.Get();
  runtime.samples = compute::CreateSharedInput(bridge, SAMPLE_WIDTH, SAMPLE_HEIGHT, DXGI_FORMAT_R32G32B32A32_UINT);
  Check(bridge->device11->CreateComputeShader(__sample_memory.data(), __sample_memory.size(), nullptr, &runtime.sample), "Create scene memory samples");
  const D3D11_BUFFER_DESC constants = {.ByteWidth = sizeof(Constants), .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_CONSTANT_BUFFER};
  Check(bridge->device11->CreateBuffer(&constants, nullptr, &runtime.constants), "Create scene memory constants");
  // Every D3D12 pass binds the reflection constants, the scene and Constants.
  runtime.state_pass.Create(device, __memory_state, 1u, 1u, 3u);
  runtime.scroll_pass.Create(device, __memory_scroll, 0u, 4u, 3u);
  runtime.carve_pass.Create(device, __memory_carve, 1u, 2u, 3u);
  runtime.mark_pass.Create(device, __memory_mark, 1u, 4u, 3u);
  runtime.inspect_pass.Create(device, __memory_inspect, 5u, 2u, 3u);
  runtime.terrain_pass.Create(device, __memory_terrain, 1u, 2u, 3u);
  runtime.window = compute::CreateSharedInput(bridge, TERRAIN_WINDOW, TERRAIN_WINDOW, DXGI_FORMAT_R32_FLOAT);
  Check(bridge->device11->CreateComputeShader(__memory_window.data(), __memory_window.size(), nullptr, &runtime.window_shader), "Create scene memory terrain window");
  runtime.state = compute::CreateTexture(device, STATE_WORDS, 1u, DXGI_FORMAT_R32_UINT);
  runtime.grid = compute::CreateTexture(device, GRID_WIDTH, GRID_HEIGHT, DXGI_FORMAT_R32_UINT, 1u, GRID_DEPTH);
  runtime.albedo = compute::CreateTexture(device, HALF_WIDTH, HALF_HEIGHT, DXGI_FORMAT_R32_UINT, 1u, HALF_DEPTH);
  runtime.sun = compute::CreateTexture(device, HALF_WIDTH, HALF_HEIGHT, DXGI_FORMAT_R32_UINT, 1u, HALF_DEPTH);
  for (auto& readback : runtime.readbacks) {
    readback.buffer = compute::CreateBuffer(device, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, D3D12_HEAP_TYPE_READBACK,
                                            D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
  }
  runtime.device = bridge->device12;
}

// Runs right after reflection preparation, whose inputs stay bound; the
// sun's cascade atlas is at t12 and the scene constants at b1. The caller
// unbinds t0 to t12 and u0 to u7 and restores state.
inline void Sample(ID3D11DeviceContext* context) {
  const Constants constants = {.frame = runtime.frame};
  context->UpdateSubresource(runtime.constants.Get(), 0u, nullptr, &constants, 0u, 0u);
  context->CSSetShader(runtime.sample.Get(), nullptr, 0u);
  context->CSSetConstantBuffers(2u, 1u, runtime.constants.GetAddressOf());
  context->CSSetUnorderedAccessViews(0u, 1u, runtime.samples.uav.GetAddressOf(), nullptr);
  context->Dispatch(SAMPLE_WIDTH / 2u / 8u, SAMPLE_HEIGHT / 8u, 1u);
  const auto& kept = runtime.recorded_terrain;
  if (kept.tiles == nullptr) return;
  const std::array<ID3D11ShaderResourceView*, 2> views = {kept.tiles.Get(), kept.heights.Get()};
  const std::array<ID3D11Buffer*, 2> buffers = {kept.material.Get(), kept.object.Get()};
  context->CSSetShader(runtime.window_shader.Get(), nullptr, 0u);
  context->CSSetShaderResources(0u, static_cast<UINT>(views.size()), views.data());
  context->CSSetConstantBuffers(4u, static_cast<UINT>(buffers.size()), buffers.data());
  context->CSSetUnorderedAccessViews(0u, 1u, runtime.window.uav.GetAddressOf(), nullptr);
  context->Dispatch(TERRAIN_WINDOW / 8u, TERRAIN_WINDOW / 8u, 1u);
}

// Records the scene memory passes into the reflection command list, after
// reading the state this slot copied last time if no newer copy was read
// since. reflection holds the addresses of the reflection constants and the
// scene; depth is the reflection depth with its nearest-depth mips and normal
// the reflection normal image. For the scene memory debug views the inspect
// pass draws into view_output, the image the composite shows.
inline void Record(ID3D12Device* device, const bridge_api::Dispatch& dispatch, compute::Frame* frame,
                   const std::array<D3D12_GPU_VIRTUAL_ADDRESS, 2>& reflection, const std::array<uint32_t, 2>& size,
                   const ImageView& depth, const ImageView& normal, int debug_view, ID3D12Resource* view_output) {
  auto& readback = runtime.readbacks[dispatch.slot_index];
  if (readback.submission > runtime.newest_read) {
    std::array<uint32_t, STATE_WORDS> words = {};
    const D3D12_RANGE range = {.Begin = 0u, .End = sizeof(words)};
    void* mapped = nullptr;
    Check(readback.buffer->Map(0u, &range, &mapped), "Map scene memory readback");
    std::memcpy(words.data(), mapped, sizeof(words));
    const D3D12_RANGE no_writes = {};
    readback.buffer->Unmap(0u, &no_writes);
    const auto read_float = [&words](size_t index) {
      float value = 0.f;
      std::memcpy(&value, &words[index], sizeof(value));
      return value;
    };
    const std::unique_lock lock(reported.mutex);
    auto& status = reported.status;
    ++status.readbacks;
    status.readback_lag = runtime.submissions + 1u - readback.submission;
    status.valid_samples = words[STATE_VALID];
    status.moving_samples = words[STATE_MOVING];
    status.sun_atlas = words[STATE_ATLAS] != 0u;
    status.sun_covered_samples = words[STATE_COVERED];
    status.sun_facing_samples = words[STATE_FACING];
    status.sun_epoch = words[STATE_SUN_EPOCH];
    double error = 0.0;
    for (size_t axis = 0u; axis < 3u; ++axis) {
      status.sun_direction[axis] = read_float(STATE_SUN_DIRECTION + axis);
      status.eye[axis] = read_float(STATE_EYE + axis);
      if (readback.host_eye) {
        const double difference = static_cast<double>(status.eye[axis]) - (*readback.host_eye)[axis];
        error += difference * difference;
      }
    }
    status.eye_error = readback.host_eye ? static_cast<float>(std::sqrt(error)) : -1.f;
    status.eye_mismatch = read_float(STATE_EYE_MISMATCH);
    status.gpu_frames = words[STATE_FRAMES];
    for (size_t level = 0u; level < LEVELS; ++level) {
      status.near_cells[level] = words[STATE_NEAR_CELLS + level];
      for (size_t axis = 0u; axis < 3u; ++axis) {
        status.origins[level][axis] = static_cast<int32_t>(words[STATE_ORIGIN + (3u * level) + axis]);
      }
    }
    status.marked_samples = words[STATE_MARKED];
    status.marking = words[STATE_MARKING] != 0u;
    status.carved_cells = words[STATE_CARVED];
    for (size_t level = 0u; level < LEVELS; ++level) {
      status.known_cells[level] = words[STATE_KNOWN_CELLS + level];
    }
    status.color_samples = words[STATE_ALBEDO_WRITTEN];
    status.sun_samples = words[STATE_SUN_WRITTEN];
    status.terrain_cells = words[STATE_TERRAIN_CELLS];
    for (size_t axis = 0u; axis < 3u; ++axis) {
      status.forward[axis] = read_float(STATE_FORWARD + axis);
    }
    status.exposure = read_float(STATE_EXPOSURE);
    runtime.newest_read = readback.submission;
  }
  readback.submission = 0u;

  ++runtime.frame;
  runtime.recorded_clear = runtime.clear_pending || std::chrono::steady_clock::now() - runtime.last_submitted > CLEAR_AFTER;
  runtime.recorded_paused = engine::pause::IsGameTimeStopped();
  runtime.recorded_view = debug_view;
  {
    // Textures no terrain draw has bound for a while belong to a map that may
    // be gone; let them go.
    const std::unique_lock lock(terrain.mutex);
    if (std::chrono::steady_clock::now() - terrain.captured >= CLEAR_AFTER) {
      terrain.inputs.tiles = nullptr;
      terrain.inputs.heights = nullptr;
    }
    runtime.recorded_terrain = (terrain.inputs.tiles != nullptr ? terrain.inputs : TerrainInputs{});
  }
  if (runtime.recorded_paused.value_or(false)) {
    runtime.updates_since_pause = 0u;
  } else if (runtime.updates_since_pause < RESUME_UPDATES) {
    ++runtime.updates_since_pause;
  }
  uint32_t flags = (runtime.recorded_clear ? FLAG_CLEAR : 0u) | (enabled.load(std::memory_order_relaxed) ? FLAG_CENSUS : 0u);
  if (!runtime.recorded_paused) {
    flags |= FLAG_PAUSE_UNKNOWN;
  } else if (runtime.updates_since_pause < RESUME_UPDATES) {
    flags |= FLAG_PAUSED;
  }
  const Constants constants = {.flags = flags, .frame = runtime.frame, .view = static_cast<uint32_t>(debug_view)};
  const std::array addresses = {reflection[0], reflection[1], frame->Upload(&constants, sizeof(constants))};
  const ImageView samples = {.resource = runtime.samples.image.resource.Get()};
  const ImageView state = {.resource = runtime.state.Get()};
  const ImageView grid = {.resource = runtime.grid.Get()};
  const ImageView albedo = {.resource = runtime.albedo.Get()};
  const ImageView sun = {.resource = runtime.sun.Get()};
  compute::DispatchPass(device, dispatch.commands, frame, runtime.state_pass, addresses, std::array{samples}, std::array{state}, 1u, 1u);
  dispatch.Mark("scene memory state");
  compute::DispatchPass(device, dispatch.commands, frame, runtime.scroll_pass, addresses, {}, std::array{state, grid, albedo, sun},
                        GRID_WIDTH, GRID_HEIGHT, GRID_DEPTH);
  dispatch.Mark("scene memory scroll");
  if (runtime.recorded_terrain.tiles != nullptr) {
    compute::DispatchPass(device, dispatch.commands, frame, runtime.terrain_pass, addresses,
                          std::array{ImageView{.resource = runtime.window.image.resource.Get()}}, std::array{state, grid},
                          GRID_WIDTH, GRID_HEIGHT, GRID_DEPTH / TERRAIN_SLICE_STRIDE);
    dispatch.Mark("scene memory terrain");
  }
  compute::DispatchPass(device, dispatch.commands, frame, runtime.carve_pass, addresses, std::array{depth}, std::array{state, grid},
                        CARVE_WIDTH, CARVE_HEIGHT);
  dispatch.Mark("scene memory carve");
  compute::DispatchPass(device, dispatch.commands, frame, runtime.mark_pass, addresses, std::array{samples},
                        std::array{state, grid, albedo, sun},
                        SAMPLE_WIDTH / 2u, SAMPLE_HEIGHT);
  dispatch.Mark("scene memory mark");
  if (IsMemoryView(debug_view)) {
    compute::DispatchPass(device, dispatch.commands, frame, runtime.inspect_pass, addresses,
                          std::array{grid, depth, normal, albedo, sun},
                          std::array{state, ImageView{.resource = view_output}}, size[0], size[1]);
    dispatch.Mark("scene memory view");
  }
  compute::Transition(dispatch.commands, runtime.state.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);
  const D3D12_TEXTURE_COPY_LOCATION from = {.pResource = runtime.state.Get(), .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, .SubresourceIndex = 0u};
  const D3D12_TEXTURE_COPY_LOCATION to = {.pResource = readback.buffer.Get(), .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, .PlacedFootprint = {.Offset = 0u, .Footprint = {.Format = DXGI_FORMAT_R32_UINT, .Width = STATE_WORDS, .Height = 1u, .Depth = 1u, .RowPitch = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT}}};
  dispatch.commands->CopyTextureRegion(&to, 0u, 0u, 0u, &from, nullptr);
  compute::Transition(dispatch.commands, runtime.state.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
}

// After the reflection submission recorded into slot succeeded. host_eye is
// the camera position the host knew for this frame.
inline void Submitted(size_t slot, const std::optional<std::array<double, 3>>& host_eye) {
  const bool paused = runtime.recorded_paused.value_or(false);
  ++runtime.submissions;
  if (paused) ++runtime.paused_submissions;
  runtime.readbacks[slot].submission = runtime.submissions;
  runtime.readbacks[slot].host_eye = host_eye;
  runtime.last_submitted = std::chrono::steady_clock::now();
  last_update.store(runtime.last_submitted.time_since_epoch().count(), std::memory_order_relaxed);
  if (runtime.recorded_clear) {
    runtime.clear_pending = false;
    ++runtime.clears;
  }
  const std::unique_lock lock(reported.mutex);
  reported.status.submissions = runtime.submissions;
  reported.status.pause_known = runtime.recorded_paused.has_value();
  reported.status.paused = paused;
  reported.status.paused_submissions = runtime.paused_submissions;
  reported.status.clears = runtime.clears;
  reported.status.debug_view = runtime.recorded_view;
  reported.status.terrain = runtime.recorded_terrain.tiles != nullptr;
}

}  // namespace mgsv::lighting::memory

#endif  // MGSV_LIGHTING_MEMORY_RUNTIME_HPP_
