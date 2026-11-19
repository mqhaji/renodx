// SPDX-License-Identifier: MIT
#ifndef MGSV_REFLECTIONS_RUNTIME_HPP_
#define MGSV_REFLECTIONS_RUNTIME_HPP_

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <d3d11_1.h>
#include <d3d12.h>
#include <embed/shaders.h>
#include <wrl/client.h>
#include <include/reshade.hpp>

#include "../../assets/noise/fast_noise_gr8.h"
#include "../../reconstruction/coordinator.hpp"
#include "../../rendering/d3d11_state.hpp"
#include "../../rendering/d3d12_compute.hpp"
#include "../../rendering/nrd.hpp"
#include "../../rendering/producer_records.hpp"
#include "../environment_specular.hpp"
#include "../indirect_lighting_mode.hpp"
#include "probe_api.hpp"

namespace mgsv::lighting::reflections {

namespace bridge_api = rendering::d3d12_bridge;
namespace compute = rendering::d3d12_compute;
using compute::Check;

inline constexpr UINT SCENE_BYTES = 480u;
inline constexpr UINT SCENE_ROW_BYTES = 512u;

struct alignas(16) Constants {
  std::array<uint32_t, 2> size = {};
  uint32_t sample = 0u;
  uint32_t maximum_mip = 0u;
  std::array<float, 2> jitter = {};
  float environment_occlusion_strength = 1.f;
  uint32_t flags = 0u;
  std::array<float, 16> current_to_previous_clip = {};
  std::array<float, 3> hit_distance_parameters = {};  // Filled from the SDK's REBLUR defaults.
  float velocity_projection_jitter_scale = 0.f;
  uint32_t rays_per_pixel = 1u;
};
static_assert(sizeof(Constants) == probe::CONSTANT_BYTES);

struct SharedInput {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
  std::shared_ptr<rendering::SharedTexture> allocation;
  bridge_api::SharedImage image;
};

struct ImageView {
  ID3D12Resource* resource = nullptr;
  UINT mip = 0u;
  UINT mips = 1u;
};

struct Frame {
  compute::Frame arena;
  // Descriptors do not retain COM objects. Keep shared outputs alive until the
  // bridge retires this slot, including outputs superseded by a native resize.
  std::shared_ptr<specular_result::Target> result;
  std::array<bridge_api::SharedImage, 2> outputs;
  bridge_api::SharedImage environment;
};

// Keep preparation and every DX12 stage in one set. A reload either creates
// this entire set successfully or leaves the current set intact.
struct Pipelines {
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> prepare;
  compute::Pipeline mips;
  compute::Pipeline trace;
  compute::Pipeline resolve;
  compute::Pipeline integrate;

  void Create(ID3D11Device* device11, ID3D12Device* device12,
              const std::array<std::vector<uint8_t>, probe::PASS_COUNT>& shaders = {}) {
    const std::array<std::span<const uint8_t>, probe::PASS_COUNT> embedded = {
        __prepare_reflections, __reflection_mips, __sssr_trace, __sssr_resolve, __sssr_brdf};
    std::array<std::span<const uint8_t>, probe::PASS_COUNT> code;
    for (size_t index = 0u; index < code.size(); ++index) {
      code[index] = (shaders[index].empty() ? embedded[index] : std::span<const uint8_t>(shaders[index]));
    }
    Check(device11->CreateComputeShader(code[0].data(), code[0].size(), nullptr, &prepare), "Create reflection preparation");
    mips.Create(device12, code[1], 1u, 1u, 2u);
    trace.Create(device12, code[2], 7u, 2u, 2u);
    resolve.Create(device12, code[3], 6u, 2u, 2u);
    integrate.Create(device12, code[4], 0u, 1u, 0u);
  }
};

struct ReloadState {
  std::shared_mutex mutex;
  std::array<std::vector<uint8_t>, probe::PASS_COUNT> shaders;
  probe::Status status;
};
static ReloadState reload;
static std::atomic_bool reload_pending = false;
static std::atomic_bool denoiser_settings_pending = false;
static std::atomic_bool camera_capture_pending = false;
static std::atomic<uint64_t> nrd_submissions = 0u;
static std::atomic<uint64_t> nrd_restarts = 0u;

inline void ReadProbeStatus(probe::Status* status) {
  const std::shared_lock lock(reload.mutex);
  *status = reload.status;
  status->nrd_submissions = nrd_submissions.load(std::memory_order_relaxed);
  status->nrd_restarts = nrd_restarts.load(std::memory_order_relaxed);
}

inline bool QueueProbeReload(const probe::Request* request, probe::Status* status) {
  const std::unique_lock lock(reload.mutex);
  *status = reload.status;
  if (request->expected_revision != reload.status.revision || reload.status.pending) return false;
  try {
    std::array<std::vector<uint8_t>, probe::PASS_COUNT> shaders;
    if (!request->restore) {
      for (size_t index = 0u; index < shaders.size(); ++index) {
        const auto& code = request->shaders[index];
        if (code.data == nullptr || code.size < 32u || code.size > 16u * 1024u * 1024u) return false;
        shaders[index].assign(code.data, code.data + code.size);
      }
    }
    reload.shaders = std::move(shaders);
    ++reload.status.revision;
    reload.status.pending = true;
    reload.status.error = {};
    *status = reload.status;
    reload_pending.store(true, std::memory_order_release);
    return true;
  } catch (...) {
    // Allocation failure must not unwind through another addon's C++ runtime.
    return false;
  }
}

inline bool QueueDenoiserSettings(probe::DenoiserMode mode, probe::Status* status) {
  if (mode >= probe::DenoiserMode::COUNT) return false;
  const std::unique_lock lock(reload.mutex);
  reload.status.denoiser_mode = mode;
  ++reload.status.denoiser_revision;
  *status = reload.status;
  denoiser_settings_pending.store(true, std::memory_order_release);
  return true;
}

inline bool QueueCameraCapture(uint32_t frames, probe::Status* status) {
  if (frames == 0u || frames > probe::MAX_CAMERA_SNAPSHOTS) return false;
  const std::unique_lock lock(reload.mutex);
  if (camera_capture_pending.load(std::memory_order_relaxed)) return false;
  ++reload.status.camera_revision;
  reload.status.camera_requested = frames;
  reload.status.camera_count = 0u;
  *status = reload.status;
  camera_capture_pending.store(true, std::memory_order_release);
  return true;
}

inline void UseProbe(bool attach) {
  if (attach) {
    probe::shared.RegisterModule();
    if (probe::shared.data == nullptr) return;
    const std::unique_lock lock(probe::shared.data->mutex);
    probe::shared.data->request = QueueProbeReload;
    probe::shared.data->read = ReadProbeStatus;
    probe::shared.data->configure_denoiser = QueueDenoiserSettings;
    probe::shared.data->capture_camera = QueueCameraCapture;
  } else if (probe::shared.data != nullptr) {
    {
      const std::unique_lock lock(probe::shared.data->mutex);
      probe::shared.data->request = nullptr;
      probe::shared.data->read = nullptr;
      probe::shared.data->configure_denoiser = nullptr;
      probe::shared.data->capture_camera = nullptr;
    }
    probe::shared.UnregisterModule();
  }
}

struct Runtime {
  Microsoft::WRL::ComPtr<ID3D12Device> device;
  bridge_api::Bridge::WorkPool commands;
  std::array<Frame, bridge_api::Bridge::MAX_IN_FLIGHT> frames;
  std::array<SharedInput, 8> inputs;
  SharedInput sampling_noise;
  Pipelines pipelines;
  Microsoft::WRL::ComPtr<ID3D11Buffer> constants;
  Microsoft::WRL::ComPtr<ID3D12Resource> scene;
  Microsoft::WRL::ComPtr<ID3D12Resource> brdf;
  Microsoft::WRL::ComPtr<ID3D12Resource> depth;
  Microsoft::WRL::ComPtr<ID3D12Resource> color;
  std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> raw;
  Microsoft::WRL::ComPtr<ID3D12Resource> filtered;
  std::unique_ptr<rendering::denoising::Runtime> denoiser;
  reconstruction::camera_state::CameraFrame previous_camera;
  std::weak_ptr<specular_result::Target> previous_result;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> previous_source;
  uint64_t previous_submission = 0u;
  uint32_t denoised_frame = 0u;
  uint64_t denoiser_revision = 0u;
  bool report_denoiser_submission = false;
  float previous_occlusion_strength = -1.f;
  uint32_t previous_rays_per_pixel = 0u;
  bool denoiser_attempted = false;
  bool denoiser_usable = false;
  bool history_valid = false;
  int previous_debug_view = 0;
  std::array<uint32_t, 2> size = {};
  UINT mip_count = 0u;
  bool pipelines_ready = false;
  bool brdf_ready = false;
  uint64_t probe_revision = 0u;
  bool report_probe_submission = false;
};

// The existing coordinator serializes this runtime with reconstruction. Both
// consumers share one device, queue and fence pair, but have separate slots.
static Runtime runtime;

// Report changes, including missing native inputs, without writing every frame.
inline void Report(const char* message, reshade::log::level level = reshade::log::level::warning) {
  static std::atomic<const char*> previous = nullptr;
  static std::atomic<uint32_t> reports = 0u;
  if (previous.exchange(message) != message && reports.fetch_add(1u) < 32u) {
    reshade::log::message(level, message);
  }
}

inline void Destroy() {
  bridge_api::Drain();
  runtime = {};
  nrd_submissions.store(0u, std::memory_order_relaxed);
  nrd_restarts.store(0u, std::memory_order_relaxed);
  const std::unique_lock lock(reload.mutex);
  reload.status.active_revision = 0u;
  reload.status.submitted_revision = 0u;
  reload.status.live = false;
  reload.status.denoiser_submitted_revision = 0u;
  camera_capture_pending.store(false, std::memory_order_release);
  reload.status.camera_requested = reload.status.camera_count = 0u;
  denoiser_settings_pending.store(reload.status.denoiser_revision != 0u, std::memory_order_release);
}

inline void Execute(reshade::api::command_list* cmd_list, const Inputs& inputs) {
  const auto selected = mgsv::inputs::ReadSelection(cmd_list->get_device());
  if (selected.generation != inputs.generation || selected.device == nullptr) return;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());                         // NOLINT(performance-no-int-to-ptr)
  auto& bridge = bridge_api::Get(reinterpret_cast<ID3D11Device*>(cmd_list->get_device()->get_native()));  // NOLINT(performance-no-int-to-ptr)
  const auto owner = rendering::native_context::GetScope().owner;
  if (owner.scene == 0u || owner.generation == 0u) return;
  auto views = inputs.views;
  uint64_t palette_generation = 0u;
  UINT bytes = sizeof(palette_generation);
  if (FAILED(inputs.specular->GetPrivateData(temporal_inputs::MATERIAL_PALETTE_GENERATION_GUID, &bytes, &palette_generation))
      || palette_generation != inputs.generation) {
    Report("MgsvReflections: material palette does not match the scene; retaining environment lighting");
    return;
  }
  bytes = sizeof(ID3D11ShaderResourceView*);
  inputs.specular->GetPrivateData(temporal_inputs::MATERIAL_PALETTE_GUID, &bytes, views[7].ReleaseAndGetAddressOf());
  if (views[7] == nullptr) {
    Report("MgsvReflections: missing material palette; retaining environment lighting");
    return;
  }
  // The submitted camera owns the noise phase, motion and history. A worker's
  // latest publication or a Present counter cannot identify this scene.
  const auto scene = temporal_inputs::Read(cmd_list->get_device(), {reinterpret_cast<uintptr_t>(views[4].Get())});
  const auto camera = reconstruction::camera_state::GetForCapture();
  views[8] = scene.motion.velocity;
  views[11] = scene.motion.object_velocity;
  // Allocate on the first scene or a resize; steady frames reuse pipelines,
  // shared inputs, descriptors, and private ray outputs.
  const UINT width = inputs.result->width;
  const UINT height = inputs.result->height;
  auto create_input = [&](UINT width, UINT height, DXGI_FORMAT format, UINT mips = 1u,
                          const D3D11_SUBRESOURCE_DATA* initial_data = nullptr) {
    SharedInput input;
    const D3D11_TEXTURE2D_DESC description = {
        .Width = width,
        .Height = height,
        .MipLevels = mips,
        .ArraySize = 1u,
        .Format = format,
        .SampleDesc = {.Count = 1u},
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE | (initial_data == nullptr ? D3D11_BIND_UNORDERED_ACCESS : 0u),
        .MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE,
    };
    Check(bridge.device11->CreateTexture2D(&description, initial_data, &input.texture), "Create reflection input");
    if (initial_data == nullptr) {
      Check(bridge.device11->CreateUnorderedAccessView(input.texture.Get(), nullptr, &input.uav), "Create reflection input view");
    }
    input.allocation = rendering::ExportTexture(input.texture.Get());
    if (input.allocation == nullptr) throw std::runtime_error("Cannot export reflection input");
    input.image = bridge_api::SharedImage(bridge.device11.Get(), bridge.device12.Get(), input.texture.Get(), description,
                                          D3D11_BIND_SHADER_RESOURCE, input.allocation.get(), &bridge.statistics.image_imports);
    if (!input.image.direct) throw std::runtime_error("Reflection input must be directly shared");
    return input;
  };
  if (runtime.device != bridge.device12) {
    // Backend teardown drains its queue before releasing it. A replacement
    // device cannot consume descriptors or images from the previous one.
    runtime = {};
    runtime.device = bridge.device12;
    const std::unique_lock lock(reload.mutex);
    reload.status.active_revision = 0u;
    reload.status.submitted_revision = 0u;
    reload.status.live = false;
    reload.status.denoiser_submitted_revision = 0u;
    denoiser_settings_pending.store(reload.status.denoiser_revision != 0u, std::memory_order_release);
  }
  if (!runtime.pipelines_ready) {
    runtime.pipelines.Create(bridge.device11.Get(), bridge.device12.Get());
    const D3D11_BUFFER_DESC description = {.ByteWidth = sizeof(Constants), .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_CONSTANT_BUFFER};
    Check(bridge.device11->CreateBuffer(&description, nullptr, &runtime.constants), "Create reflection controls");
    runtime.brdf = compute::CreateTexture(bridge.device12.Get(), 128u, 128u, DXGI_FORMAT_R16G16B16A16_FLOAT);
    // Upload the existing sampling data once, then read the same allocation
    // directly from DX12. This small atlas needs no per-frame copy or dispatch.
    // Flattening its layers keeps all reflection inputs ordinary 2D views.
    const D3D11_SUBRESOURCE_DATA noise_data = {.pSysMem = FAST_NOISE_RG8.data(), .SysMemPitch = 128u * 2u};
    runtime.sampling_noise = create_input(128u, 128u * 32u, DXGI_FORMAT_R8G8_UNORM, 1u, &noise_data);
    const D3D12_HEAP_PROPERTIES heap = {.Type = D3D12_HEAP_TYPE_DEFAULT};
    const D3D12_RESOURCE_DESC buffer = {.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER, .Width = SCENE_ROW_BYTES, .Height = 1u, .DepthOrArraySize = 1u, .MipLevels = 1u, .SampleDesc = {.Count = 1u}, .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR};
    Check(bridge.device12->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COMMON,
                                                   nullptr, IID_PPV_ARGS(&runtime.scene)),
          "Create reflection scene constants");
    runtime.pipelines_ready = true;
  }
  if (reload_pending.exchange(false, std::memory_order_acquire)) {
    const std::unique_lock lock(reload.mutex);
    try {
      Pipelines replacement;
      replacement.Create(bridge.device11.Get(), bridge.device12.Get(), reload.shaders);
      // This explicit development operation may stall once. Drain before
      // releasing old PSOs or rebuilding the lookup table; steady frames do
      // not wait on the CPU, inspect files, or compile shaders.
      bridge_api::Drain();
      runtime.pipelines = std::move(replacement);
      runtime.brdf_ready = false;
      runtime.history_valid = false;
      runtime.probe_revision = reload.status.revision;
      runtime.report_probe_submission = true;
      reload.status.active_revision = reload.status.revision;
      reload.status.live = !reload.shaders[0].empty();
      reshade::log::message(reshade::log::level::info, "MgsvReflections: probe shader set installed");
    } catch (const std::exception& error) {
      std::copy_n(error.what(), std::min(std::char_traits<char>::length(error.what()), reload.status.error.size() - 1u), reload.status.error.data());
      reshade::log::message(reshade::log::level::warning, "MgsvReflections: probe reload failed; current shaders retained");
    }
    reload.status.pending = false;
    reload.shaders = {};
  }
  if (runtime.size != std::array<uint32_t, 2>{width, height}) {
    bridge_api::Drain();
    runtime.mip_count = std::min(8u, static_cast<UINT>(std::bit_width(std::min(width, height))));
    constexpr std::array formats = {DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,
                                    DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R32_FLOAT};
    for (size_t index = 0u; index < formats.size(); ++index) {
      runtime.inputs[index] = create_input(width, height, formats[index], index == 3u ? runtime.mip_count : 1u);
    }
    runtime.inputs[4] = create_input(SCENE_ROW_BYTES / 16u, 1u, DXGI_FORMAT_R32G32B32A32_UINT);
    runtime.inputs[5] = create_input(width, height, DXGI_FORMAT_R16G16_FLOAT);
    runtime.inputs[6] = create_input(width, height, DXGI_FORMAT_R10G10B10A2_UNORM);
    runtime.inputs[7] = create_input(width, height, DXGI_FORMAT_R32_FLOAT);
    // Preparation writes directly into the shared source and depth. Only depth
    // needs a hierarchy; source radiance keeps its original pixel footprint.
    runtime.depth = runtime.inputs[3].image.resource;
    runtime.color = runtime.inputs[0].image.resource;
    for (auto& texture : runtime.raw) {
      texture = compute::CreateTexture(bridge.device12.Get(), width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
    }
    runtime.size = {width, height};
    runtime.filtered = compute::CreateTexture(bridge.device12.Get(), width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
    runtime.history_valid = false;
  }
  // Load the SDK once. Missing or incompatible DLLs keep the complete raw
  // estimator available and produce a useful warning without retrying per frame.
  if (!runtime.denoiser_attempted) {
    runtime.denoiser_attempted = true;
    try {
      auto denoiser = std::make_unique<rendering::denoising::Runtime>();
      denoiser->Create(bridge.device12.Get());
      runtime.denoiser = std::move(denoiser);
      runtime.denoiser_usable = true;
    } catch (const std::exception& error) {
      reshade::log::message(reshade::log::level::warning, error.what());
    }
  }
  if (runtime.denoiser_usable && runtime.denoiser->size != runtime.size) {
    try {
      runtime.denoiser->Resize(bridge.device12.Get(), width, height);
    } catch (const std::exception& error) {
      runtime.denoiser_usable = false;
      reshade::log::message(reshade::log::level::warning, error.what());
    }
  }
  if (runtime.denoiser_usable && denoiser_settings_pending.exchange(false, std::memory_order_acquire)) {
    probe::Status requested;
    ReadProbeStatus(&requested);
    nrd::ReblurSettings settings;
    switch (requested.denoiser_mode) {
      case probe::DenoiserMode::DEFAULT:
        break;
      case probe::DenoiserMode::NO_STABILIZATION:
        settings.maxStabilizedFrameNum = 0u;
        break;
      case probe::DenoiserMode::SPATIAL_ONLY:
        settings.maxAccumulatedFrameNum = 0u;
        settings.maxFastAccumulatedFrameNum = 0u;
        settings.maxStabilizedFrameNum = 0u;
        settings.historyFixFrameNum = 0u;
        break;
      case probe::DenoiserMode::TRACKING_PREPASS_ONLY:
        settings.usePrepassOnlyForSpecularMotionEstimation = true;
        break;
      case probe::DenoiserMode::SHORT_HISTORY:
        settings.maxAccumulatedFrameNum = 6u;
        settings.maxFastAccumulatedFrameNum = 1u;
        settings.maxStabilizedFrameNum = 0u;
        settings.historyFixFrameNum = 0u;
        break;
      default:
        throw std::runtime_error("Invalid probe denoiser mode");
    }
    try {
      runtime.denoiser->Configure(settings);
      runtime.denoiser_revision = requested.denoiser_revision;
      runtime.report_denoiser_submission = true;
      reshade::log::message(reshade::log::level::info, "MgsvReflections: probe denoiser settings applied; history restarted");
    } catch (const std::exception& error) {
      runtime.denoiser_usable = false;
      reshade::log::message(reshade::log::level::warning, error.what());
    }
    runtime.history_valid = false;
    // These are explicit diagnostic requests, not saved gameplay settings.
    // Constants are copied per submission, so changing them needs no queue drain.
  }
  // Every private pass begins and ends in COMMON. The views name exact mip
  // subresources, so a reduction can read one level while writing the next.
  auto dispatch_pass = [&](const bridge_api::Dispatch& dispatch, compute::Frame* frame,
                           const compute::Pipeline& pipeline, D3D12_GPU_VIRTUAL_ADDRESS constants,
                           std::span<const ImageView> inputs, std::span<const ImageView> outputs, UINT width, UINT height,
                           bool scene_constants = true) {
    const UINT first = frame->Allocate(static_cast<UINT>(inputs.size() + outputs.size()));
    for (size_t index = 0u; index < inputs.size(); ++index) {
      const auto& input = inputs[index];
      const auto description = input.resource->GetDesc();
      const D3D12_SHADER_RESOURCE_VIEW_DESC view = {.Format = description.Format, .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D, .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING, .Texture2D = {.MostDetailedMip = input.mip, .MipLevels = input.mips}};
      bridge.device12->CreateShaderResourceView(input.resource, &view, frame->Cpu(first + static_cast<UINT>(index)));
      for (UINT mip = input.mip; mip < input.mip + input.mips; ++mip) {
        compute::Transition(dispatch.commands, input.resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, mip);
      }
    }
    for (size_t index = 0u; index < outputs.size(); ++index) {
      const auto& output = outputs[index];
      const D3D12_UNORDERED_ACCESS_VIEW_DESC view = {.Format = output.resource->GetDesc().Format,
                                                     .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
                                                     .Texture2D = {.MipSlice = output.mip}};
      bridge.device12->CreateUnorderedAccessView(output.resource, nullptr, &view, frame->Cpu(first + static_cast<UINT>(inputs.size() + index)));
      compute::Transition(dispatch.commands, output.resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, output.mip);
    }
    pipeline.Bind(dispatch.commands, *frame, first);
    if (scene_constants) {
      dispatch.commands->SetComputeRootConstantBufferView(1u, constants);
      dispatch.commands->SetComputeRootConstantBufferView(2u, runtime.scene->GetGPUVirtualAddress());
    }
    dispatch.commands->Dispatch((width + 7u) / 8u, (height + 7u) / 8u, 1u);
    for (const auto& output : outputs) {
      compute::Transition(dispatch.commands, output.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, output.mip);
    }
    for (const auto& input : inputs) {
      for (UINT mip = input.mip; mip < input.mip + input.mips; ++mip) {
        compute::Transition(dispatch.commands, input.resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON, mip);
      }
    }
  };
  const float occlusion_strength = material_inputs::controls->specular_occlusion_strength;
  const auto rays_per_pixel = static_cast<uint32_t>(std::clamp(material_inputs::controls->reflection_rays_per_pixel,
                                                               1.f, static_cast<float>(MGSV_REFLECTION_MAX_RAYS_PER_PIXEL)));
  const int debug_view = static_cast<int>(material_inputs::controls->lighting_debug_view);
  const bool camera_valid = scene.valid && camera.valid && camera.camera_matrix_valid
                            && camera.scene == owner.scene && camera.owner_generation == owner.generation
                            && camera.publication_sequence == scene.camera_publication && camera.reservation != 0u;
  const bool same_scene = camera_valid && runtime.history_valid
                          && camera.reservation == runtime.previous_camera.reservation
                          && camera.owner_generation == runtime.previous_camera.owner_generation
                          && camera.reset_generation == runtime.previous_camera.reset_generation
                          && runtime.previous_result.lock() == inputs.result
                          && runtime.previous_source == views[1]
                          && runtime.previous_submission == scene.submission
                          && debug_view == runtime.previous_debug_view
                          && occlusion_strength == runtime.previous_occlusion_strength
                          && rays_per_pixel == runtime.previous_rays_per_pixel;
  if (same_scene) {
    // This output is still the last submitted result for this exact view build.
    // Its bridge wait is already in the immediate stream; replay needs neither
    // another denoiser update nor another cross-API handoff.
    const std::array<float, 4> ready = {1.f, 0.f, 0.f, 0.f};
    context->ClearUnorderedAccessViewFloat(inputs.result->ready.uav.Get(), ready.data());
    return;
  }
  const bool consecutive = camera_valid && runtime.history_valid
                           && camera.reservation == runtime.previous_camera.reservation + 1u
                           && camera.reset_generation == runtime.previous_camera.reset_generation
                           && camera.owner_generation == runtime.previous_camera.owner_generation
                           && camera.viewport_identity == runtime.previous_camera.viewport_identity
                           && camera.camera_identity == runtime.previous_camera.camera_identity
                           && occlusion_strength == runtime.previous_occlusion_strength
                           && rays_per_pixel == runtime.previous_rays_per_pixel;
  Constants constants = {.size = runtime.size, .sample = scene.camera_sample, .maximum_mip = runtime.mip_count - 1u, .jitter = scene.current_jitter_uv, .environment_occlusion_strength = occlusion_strength, .flags = (consecutive ? 1u : 0u) | (debug_view == 17 ? 2u : 0u), .velocity_projection_jitter_scale = scene.velocity_projection_jitter_scale, .rays_per_pixel = rays_per_pixel};
  if (consecutive) {
    constants.current_to_previous_clip = reconstruction::camera_state::ToRowMajorFloatArray(
        reconstruction::camera_state::Multiply(runtime.previous_camera.current_view_projection, camera.inverse_view_projection));
  }
  const nrd::ReblurSettings denoiser_settings;
  constants.hit_distance_parameters = {denoiser_settings.hitDistanceParameters.A, denoiser_settings.hitDistanceParameters.B, denoiser_settings.hitDistanceParameters.C};
  nrd::CommonSettings denoising;
  if (camera_valid) {
    const auto view_to_world = reconstruction::camera_state::Multiply(
        camera.inverse_view_projection, reconstruction::camera_state::LoadColumnMajorMatrix(camera.projection.data()));
    const std::array<double, 3> origin = {view_to_world.m[0][3], view_to_world.m[1][3], view_to_world.m[2][3]};
    const auto set_camera = [&origin](const reconstruction::camera_state::CameraFrame& source, float* projection, float* view) {
      std::ranges::copy(source.projection, projection);
      std::ranges::copy(source.view, view);
      // Express both cameras relative to the current camera origin in double
      // precision. The previous view keeps its real translation delta; removing
      // both translations would erase motion. NRD never needs large world offsets.
      for (uint32_t row = 0u; row < 3u; ++row) {
        auto translation = static_cast<double>(source.view[12u + row]);
        for (uint32_t column = 0u; column < 3u; ++column) {
          translation += static_cast<double>(source.view[(column * 4u) + row]) * origin[column];
        }
        view[12u + row] = static_cast<float>(translation);
      }
      // MGSV mirrors screen X in its projection. NRD uses that sign when
      // detecting handedness, which otherwise reverses reconstructed positions.
      // Move the X reflection into the view instead: P * V and positive view Z
      // stay identical, and world-space guides need no coordinate conversion.
      if (projection[0] < 0.f) {
        for (uint32_t axis = 0u; axis < 4u; ++axis) {
          projection[axis] = -projection[axis];  // First column: P * reflectX.
          view[axis * 4u] = -view[axis * 4u];    // First row: reflectX * V.
        }
      }
    };
    set_camera(camera, denoising.viewToClipMatrix, denoising.worldToViewMatrix);
    const auto& previous = consecutive ? runtime.previous_camera : camera;
    set_camera(previous, denoising.viewToClipMatrixPrev, denoising.worldToViewMatrixPrev);
    // Our jitter moves projected geometry. NRD wants the opposite sampling
    // offset: sample = pixel + jitter, measured in input pixels.
    denoising.cameraJitter[0] = -camera.jitter_uv_x * static_cast<float>(width);
    denoising.cameraJitter[1] = -camera.jitter_uv_y * static_cast<float>(height);
    denoising.cameraJitterPrev[0] = -previous.jitter_uv_x * static_cast<float>(width);
    denoising.cameraJitterPrev[1] = -previous.jitter_uv_y * static_cast<float>(height);
  }
  for (size_t axis = 0u; axis < 2u; ++axis) {
    denoising.resourceSize[axis] = denoising.resourceSizePrev[axis] = denoising.rectSize[axis] = denoising.rectSizePrev[axis] = static_cast<uint16_t>(runtime.size[axis]);
  }
  denoising.frameIndex = runtime.denoised_frame;
  denoising.enableValidation = debug_view == 17;
  denoising.accumulationMode = consecutive ? nrd::AccumulationMode::CONTINUE : nrd::AccumulationMode::CLEAR_AND_RESTART;
  const bool use_denoiser = runtime.denoiser_usable && camera_valid;
  if (!use_denoiser) constants.flags &= ~2u;
  std::optional<probe::CameraSnapshot> camera_capture;
  if (use_denoiser && camera_capture_pending.load(std::memory_order_acquire)) {
    // This is a bounded CPU snapshot, with no GPU readback or desktop capture.
    // Its scene sample joins it to preparation's GPU-captured native constants.
    camera_capture.emplace();
    camera_capture->scene = owner.scene;
    camera_capture->owner_generation = owner.generation;
    camera_capture->reservation = camera.reservation;
    camera_capture->publication = camera.publication_sequence;
    camera_capture->sample = scene.camera_sample;
    camera_capture->size = runtime.size;
    camera_capture->jitter = scene.current_jitter_uv;
    camera_capture->source_projection = camera.projection;
    camera_capture->source_view = camera.view;
    const auto& previous = (consecutive ? runtime.previous_camera : camera);
    camera_capture->source_previous_projection = previous.projection;
    camera_capture->source_previous_view = previous.view;
    camera_capture->native_previous_projection = camera.native_previous_projection;
    camera_capture->native_previous_view = camera.native_previous_view;
    camera_capture->native_history_available = camera.native_history_available;
    std::ranges::copy(denoising.viewToClipMatrix, camera_capture->nrd_projection.begin());
    std::ranges::copy(denoising.worldToViewMatrix, camera_capture->nrd_view.begin());
    std::ranges::copy(denoising.viewToClipMatrixPrev, camera_capture->nrd_previous_projection.begin());
    std::ranges::copy(denoising.worldToViewMatrixPrev, camera_capture->nrd_previous_view.begin());
    camera_capture->current_to_previous_clip = constants.current_to_previous_clip;
  }
  const auto saved = rendering::d3d11_compute_state::Capture(cmd_list);
  bool succeeded = false;
  try {
    const auto record_reflections = [&](const bridge_api::Dispatch& dispatch) {
      auto& frame = runtime.frames[dispatch.slot_index];
      frame.arena.Reset(bridge.device12.Get());
      const auto constant_address = frame.arena.Upload(&constants, sizeof(constants));
      if (frame.result != inputs.result) {
        for (size_t index = 0u; index < frame.outputs.size(); ++index) {
          const auto& image = inputs.result->images[index];
          D3D11_TEXTURE2D_DESC description = {};
          image.texture->GetDesc(&description);
          frame.outputs[index] = bridge_api::SharedImage(bridge.device11.Get(), bridge.device12.Get(), image.texture.Get(), description,
                                                         D3D11_BIND_UNORDERED_ACCESS, image.shared.get(), &bridge.statistics.image_imports);
          if (!frame.outputs[index].direct) throw std::runtime_error("Reflection output must be directly shared");
        }
        frame.result = inputs.result;
        const auto& environment = inputs.result->environment;
        D3D11_TEXTURE2D_DESC description = {};
        environment.texture->GetDesc(&description);
        frame.environment = bridge_api::SharedImage(bridge.device11.Get(), bridge.device12.Get(), environment.texture.Get(), description,
                                                    D3D11_BIND_SHADER_RESOURCE, environment.shared.get(), &bridge.statistics.image_imports);
        if (!frame.environment.direct) throw std::runtime_error("Reflection environment must be directly shared");
      }
      // Import the prepared source and exact native scene bytes. All later
      // intermediate images stay private to DX12 until the final resolve.
      auto* scene_texture = runtime.inputs[4].image.resource.Get();
      compute::Transition(dispatch.commands, scene_texture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);
      compute::Transition(dispatch.commands, runtime.scene.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
      const D3D12_TEXTURE_COPY_LOCATION from = {.pResource = scene_texture, .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, .SubresourceIndex = 0u};
      const D3D12_TEXTURE_COPY_LOCATION to = {.pResource = runtime.scene.Get(), .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, .PlacedFootprint = {.Offset = 0u, .Footprint = {.Format = DXGI_FORMAT_R32G32B32A32_UINT, .Width = 32u, .Height = 1u, .Depth = 1u, .RowPitch = SCENE_ROW_BYTES}}};
      dispatch.commands->CopyTextureRegion(&to, 0u, 0u, 0u, &from, nullptr);
      compute::Transition(dispatch.commands, scene_texture, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
      compute::Transition(dispatch.commands, runtime.scene.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
      if (!runtime.brdf_ready) {
        const std::array<ImageView, 1> outputs = {{{.resource = runtime.brdf.Get()}}};
        dispatch_pass(dispatch, &frame.arena, runtime.pipelines.integrate, constant_address, {}, outputs, 128u, 128u, false);
      }
      dispatch.Mark("scene constants");
      for (UINT mip = 1u; mip < runtime.mip_count; ++mip) {
        const std::array<ImageView, 1> srvs = {{{.resource = runtime.depth.Get(), .mip = mip - 1u}}};
        const std::array<ImageView, 1> uavs = {{{.resource = runtime.depth.Get(), .mip = mip}}};
        dispatch_pass(dispatch, &frame.arena, runtime.pipelines.mips, constant_address, srvs, uavs,
                      std::max(runtime.size[0] >> mip, 1u), std::max(runtime.size[1] >> mip, 1u));
      }
      dispatch.Mark("depth mips");
      const std::array<ImageView, 7> trace_inputs = {{{.resource = runtime.depth.Get(), .mip = 0u, .mips = runtime.mip_count}, {.resource = runtime.color.Get()}, {.resource = runtime.inputs[1].image.resource.Get()}, {.resource = runtime.inputs[2].image.resource.Get()}, {.resource = runtime.brdf.Get()}, {.resource = runtime.sampling_noise.image.resource.Get()}, {.resource = frame.environment.resource.Get()}}};
      const std::array<ImageView, 2> trace_outputs = {{{.resource = runtime.raw[0].Get()}, {.resource = runtime.raw[1].Get()}}};
      dispatch_pass(dispatch, &frame.arena, runtime.pipelines.trace, constant_address, trace_inputs, trace_outputs, runtime.size[0], runtime.size[1]);
      dispatch.Mark("trace");
      if (use_denoiser) {
        std::array<ID3D12Resource*, rendering::denoising::Runtime::RESOURCE_COUNT> resources = {};
        resources[static_cast<size_t>(nrd::ResourceType::IN_MV)] = runtime.inputs[5].image.resource.Get();
        resources[static_cast<size_t>(nrd::ResourceType::IN_NORMAL_ROUGHNESS)] = runtime.inputs[6].image.resource.Get();
        resources[static_cast<size_t>(nrd::ResourceType::IN_VIEWZ)] = runtime.inputs[7].image.resource.Get();
        resources[static_cast<size_t>(nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST)] = runtime.raw[0].Get();
        resources[static_cast<size_t>(nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST)] = runtime.filtered.Get();
        if (denoising.enableValidation) resources[static_cast<size_t>(nrd::ResourceType::OUT_VALIDATION)] = frame.outputs[1].resource.Get();
        const auto captured_bytes = runtime.denoiser->Record(bridge.device12.Get(), dispatch.commands, &frame.arena, denoising, resources,
                                                             camera_capture ? std::as_writable_bytes(std::span(camera_capture->sdk_constants)) : std::span<std::byte>{},
                                                             dispatch.timing);
        if (camera_capture) {
          camera_capture->sdk_constant_bytes = captured_bytes;
        }
      }
      const bool show_raw = debug_view == 16;
      const std::array<ImageView, 6> resolve_inputs = {{{.resource = use_denoiser && !show_raw ? runtime.filtered.Get() : runtime.raw[0].Get()}, {.resource = runtime.raw[1].Get()}, {.resource = runtime.inputs[1].image.resource.Get()}, {.resource = runtime.inputs[2].image.resource.Get()}, {.resource = runtime.depth.Get()}, {.resource = runtime.brdf.Get()}}};
      const std::array<ImageView, 2> resolve_outputs = {{{.resource = frame.outputs[0].resource.Get()}, {.resource = frame.outputs[1].resource.Get()}}};
      dispatch_pass(dispatch, &frame.arena, runtime.pipelines.resolve, constant_address, resolve_inputs, resolve_outputs, runtime.size[0], runtime.size[1]);
      dispatch.Mark("resolve");
      compute::Transition(dispatch.commands, runtime.scene.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COMMON);
      return true;
    };
    const auto prepare_inputs = [&]() {
      context->UpdateSubresource(runtime.constants.Get(), 0u, nullptr, &constants, 0u, 0u);
      const std::array<ID3D11Buffer*, 4> buffers = {runtime.constants.Get(), inputs.scene.Get(), nullptr, inputs.scene.Get()};
      std::array<ID3D11ShaderResourceView*, INPUT_COUNT> srvs = {};
      for (size_t index = 0u; index < srvs.size(); ++index) {
        srvs[index] = views[index].Get();
      }
      std::array<ID3D11UnorderedAccessView*, 8> outputs = {};
      for (size_t index = 0u; index < outputs.size(); ++index) {
        outputs[index] = runtime.inputs[index].uav.Get();
      }
      context->CSSetShader(runtime.pipelines.prepare.Get(), nullptr, 0u);
      context->CSSetConstantBuffers(0u, static_cast<UINT>(buffers.size()), buffers.data());
      context->CSSetShaderResources(0u, static_cast<UINT>(srvs.size()), srvs.data());
      context->CSSetUnorderedAccessViews(0u, static_cast<UINT>(outputs.size()), outputs.data(), nullptr);
      context->Dispatch((runtime.size[0] + 7u) / 8u, (runtime.size[1] + 7u) / 8u, 1u);
      srvs.fill(nullptr);
      outputs.fill(nullptr);
      context->CSSetShaderResources(0u, static_cast<UINT>(srvs.size()), srvs.data());
      context->CSSetUnorderedAccessViews(0u, static_cast<UINT>(outputs.size()), outputs.data(), nullptr);
    };
    const auto publish_output = [&]() {
      const std::array<float, 4> ready = {1.f, 0.f, 0.f, 0.f};
      context->ClearUnorderedAccessViewFloat(inputs.result->ready.uav.Get(), ready.data());
    };
    succeeded = bridge.ExecuteWork(context, {.id = owner.scene, .generation = owner.generation},
                                   &runtime.commands, record_reflections, prepare_inputs, publish_output, false);
  } catch (...) {
    // A partially recorded SDK sequence must not reuse its CPU-side pool
    // states. Retain allocations until retirement, but use raw light afterward.
    runtime.denoiser_usable = false;
    runtime.history_valid = false;
    rendering::d3d11_compute_state::Restore(cmd_list, saved);
    throw;
  }
  rendering::d3d11_compute_state::Restore(cmd_list, saved);
  if (succeeded) {
    runtime.history_valid = use_denoiser;
    runtime.previous_camera = camera;
    runtime.previous_result = inputs.result;
    runtime.previous_source = views[1];
    runtime.previous_submission = scene.submission;
    runtime.previous_occlusion_strength = occlusion_strength;
    runtime.previous_rays_per_pixel = rays_per_pixel;
    runtime.previous_debug_view = debug_view;
    if (camera_capture) {
      const std::unique_lock lock(reload.mutex);
      if (reload.status.camera_count < reload.status.camera_requested) {
        reload.status.cameras[reload.status.camera_count++] = *camera_capture;
        camera_capture_pending.store(reload.status.camera_count < reload.status.camera_requested, std::memory_order_release);
      }
    }
    if (use_denoiser) {
      ++runtime.denoised_frame;
      nrd_submissions.fetch_add(1u, std::memory_order_relaxed);
      if (!consecutive) nrd_restarts.fetch_add(1u, std::memory_order_relaxed);
      if (runtime.report_denoiser_submission) {
        const std::unique_lock lock(reload.mutex);
        reload.status.denoiser_submitted_revision = runtime.denoiser_revision;
        runtime.report_denoiser_submission = false;
      }
    }
    if (runtime.report_probe_submission) {
      const std::unique_lock lock(reload.mutex);
      reload.status.submitted_revision = runtime.probe_revision;
      runtime.report_probe_submission = false;
    }
    runtime.brdf_ready = true;
    Report(use_denoiser ? "MgsvReflections: GGX reflections denoised with NRD at native reflection composition"
                        : "MgsvReflections: raw GGX reflections submitted; NRD or matching camera guides unavailable",
           reshade::log::level::info);
  } else {
    runtime.history_valid = false;
    static std::atomic_flag reported = {};
    if (!reported.test_and_set()) {
      reshade::log::message(reshade::log::level::warning, "MgsvReflections: submission unavailable; using native environment lighting");
    }
  }
}

struct __declspec(uuid("1EE8B040-9DA4-48C3-B2F7-072AC1B4E522")) CompositeState {
  std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 4> previous;
  bool active = false;
};

inline void OnInitCommandList(reshade::api::command_list* cmd_list) { cmd_list->create_private_data<CompositeState>(); }
inline void OnDestroyCommandList(reshade::api::command_list* cmd_list) { cmd_list->destroy_private_data<CompositeState>(); }
inline void OnResetCommandList(reshade::api::command_list* cmd_list) {
  if (auto* state = cmd_list->get_private_data<CompositeState>()) *state = {};
}

inline void Capture(reshade::api::command_list* cmd_list) {
  if (!UsesEnhancedIndirectLighting() || cmd_list->get_device()->get_api() != reshade::api::device_api::d3d11) return;
  // A failed capture must invalidate an earlier association for this owner.
  // Publish this beside the draw, so worker recording order cannot select it.
  temporal_inputs::Record(cmd_list, {
                                        .device = cmd_list->get_device(),
                                        .generation = mgsv::inputs::ReadSelection(cmd_list->get_device()).generation,
                                        .kind = temporal_inputs::ProducerKind::REFLECTION_INPUTS,
                                    });
  const auto* composite = cmd_list->get_private_data<environment_specular::DrawState>();
  if (composite == nullptr || !composite->composite_output_active) return;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  Inputs inputs;
  std::array<ID3D11ShaderResourceView*, 4> native = {};
  context->PSGetShaderResources(8u, 4u, native.data());
  inputs.views[0].Attach(native[0]);
  inputs.views[1].Attach(native[1]);
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> specular;
  specular.Attach(native[2]);
  inputs.views[4].Attach(native[3]);
  if (std::ranges::any_of(native, [](auto* view) { return view == nullptr; })) return;
  specular->GetResource(&inputs.specular);
  const auto environment = environment_specular::Read(cmd_list->get_device(), inputs.specular.Get());
  Microsoft::WRL::ComPtr<ID3D11Resource> albedo_resource;
  Microsoft::WRL::ComPtr<ID3D11Resource> depth_resource;
  inputs.views[0]->GetResource(&albedo_resource);
  inputs.views[4]->GetResource(&depth_resource);
  if (environment == nullptr || environment->depth != depth_resource || environment->normal == nullptr || environment->material == nullptr) {
    Report("MgsvReflections: missing matching environment, depth, or material inputs");
    return;
  }
  const auto material = material_inputs::Read(cmd_list->get_device(), albedo_resource.Get());
  Microsoft::WRL::ComPtr<ID3D11Resource> material_resource;
  environment->material->GetResource(&material_resource);
  inputs.material = material_resource;
  inputs.result = specular_result::Read(cmd_list->get_device(), material_resource.Get());
  if (inputs.result == nullptr) {
    Report("MgsvReflections: missing shared output");
    return;
  }
  inputs.views[2] = environment->normal;
  inputs.views[3] = environment->material;
  // Records exist only where a cube material drew into this G-buffer. Without
  // any, preparation reads every surface as free of a baked cube.
  if (material != nullptr) {
    inputs.views[5] = material->color_srv;
    inputs.views[6] = material->owner_srv;
  }
  inputs.views[9] = environment->srv;
  inputs.generation = environment->generation;

  // Freeze the bound range, not the worker's mutable backing buffer. Copying
  // it here places its bytes before the insertion in the same native list.
  Microsoft::WRL::ComPtr<ID3D11DeviceContext1> ranged;
  Microsoft::WRL::ComPtr<ID3D11Buffer> scene;
  UINT first = 0u;
  UINT count = 0u;
  if (SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&ranged)))) {
    ranged->PSGetConstantBuffers1(2u, 1u, &scene, &first, &count);
  } else {
    context->PSGetConstantBuffers(2u, 1u, &scene);
  }
  if (scene == nullptr) return;
  D3D11_BUFFER_DESC source = {};
  scene->GetDesc(&source);
  if (first > source.ByteWidth / 16u || source.ByteWidth - (first * 16u) < SCENE_BYTES || (count != 0u && count < SCENE_BYTES / 16u)) return;
  auto* device = reinterpret_cast<ID3D11Device*>(cmd_list->get_device()->get_native());  // NOLINT(performance-no-int-to-ptr)
  const D3D11_BUFFER_DESC description = {.ByteWidth = SCENE_BYTES, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_CONSTANT_BUFFER};
  if (FAILED(device->CreateBuffer(&description, nullptr, &inputs.scene))) return;
  const D3D11_BOX box = {.left = first * 16u, .top = 0u, .front = 0u, .right = (first * 16u) + SCENE_BYTES, .bottom = 1u, .back = 1u};
  context->CopySubresourceRegion(inputs.scene.Get(), 0u, 0u, 0u, 0u, scene.Get(), 0u, &box);
  temporal_inputs::Record(cmd_list, {
                                        .device = cmd_list->get_device(),
                                        .generation = inputs.generation,
                                        .kind = temporal_inputs::ProducerKind::REFLECTION_INPUTS,
                                        .reflection_inputs = std::make_shared<Inputs>(std::move(inputs)),
                                    });
}

// The native compositor still performs the scene-target change. Insert the
// trace immediately before it, after sky and forward lighting have contributed.
inline bool BeginComposite(reshade::api::command_list* cmd_list) {
  if (!UsesEnhancedIndirectLighting()) return true;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> source;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> material;
  context->PSGetShaderResources(0u, 1u, &source);
  context->PSGetShaderResources(2u, 1u, &material);
  Microsoft::WRL::ComPtr<ID3D11Resource> material_resource;
  if (material != nullptr) material->GetResource(&material_resource);
  auto result = specular_result::Read(cmd_list->get_device(), material_resource.Get());
  // The late scene must cover the same pixel grid as the captured G-buffer.
  // A format or mip mismatch cannot be repaired by reusing last frame's data.
  if (source != nullptr && result != nullptr) {
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    D3D11_TEXTURE2D_DESC description = {};
    D3D11_SHADER_RESOURCE_VIEW_DESC view = {};
    source->GetResource(&resource);
    source->GetDesc(&view);
    if (SUCCEEDED(resource.As(&texture))) texture->GetDesc(&description);
    if (view.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D || view.Texture2D.MostDetailedMip != 0u
        || description.Width != result->width || description.Height != result->height
        || description.SampleDesc.Count != 1u || description.ArraySize != 1u
        || (view.Format != DXGI_FORMAT_R16G16B16A16_FLOAT && view.Format != DXGI_FORMAT_R32G32B32A32_FLOAT)) {
      source.Reset();
      Report("MgsvReflections: late scene format or pixel grid does not match the captured material");
    }
  }
  auto* state = cmd_list->get_private_data<CompositeState>();
  if (state == nullptr) return true;
  std::array<ID3D11ShaderResourceView*, 4> outputs = {};
  if (result != nullptr) {
    // Bound readiness tells the compositor this view's native trace was
    // canceled. If the late scene is unusable, readiness stays unavailable and
    // the scene is left unchanged instead of exposing the canceled trace.
    outputs[3] = result->ready.srv.Get();
  }
  if (source != nullptr && result != nullptr) {
    outputs = {result->images[0].srv.Get(), result->images[1].srv.Get(), result->environment.srv.Get(), result->ready.srv.Get()};
    const auto trace_scene = [source, material_resource, result](auto* submitted, bool) {
      try {
        // Read the association at submission, not worker recording time.
        // Different scenes can record into the same pooled material target.
        const auto owner = rendering::native_context::GetScope().owner;
        std::shared_ptr<const Inputs> captured;
        {
          const std::shared_lock lock(temporal_inputs::state.mutex);
          for (const auto& scene : temporal_inputs::state.submitted) {
            if (scene.owner.scene == owner.scene && scene.owner.generation == owner.generation) {
              if (scene.reflection_inputs != nullptr && scene.reflection_inputs->material == material_resource) {
                captured = scene.reflection_inputs;
              }
              break;
            }
          }
        }
        auto* context = reinterpret_cast<ID3D11DeviceContext*>(submitted->get_native());  // NOLINT(performance-no-int-to-ptr)
        // Failed or unavailable work keeps native environment light. The
        // canceled native trace must never expose its old pooled texture.
        const std::array<float, 4> fallback = {};
        context->ClearUnorderedAccessViewFloat(result->ready.uav.Get(), fallback.data());
        if (captured == nullptr || captured->result != result) {
          Report("MgsvReflections: late scene has no matching deferred inputs; retaining native fallback");
          return;
        }
        Inputs inputs = *captured;
        inputs.views[1] = source;
        inputs.views[10] = result->environment.srv;
        Execute(submitted, inputs);
      } catch (const std::exception& error) {
        static std::atomic_flag reported = {};
        if (!reported.test_and_set()) reshade::log::message(reshade::log::level::error, error.what());
      }
    };
    if (!reconstruction::coordinator::ScheduleLighting(cmd_list, trace_scene)) {
      // Native trace selection already wrote the fallback marker in order.
      // Preserve it when the late insertion cannot be scheduled.
      Report("MgsvReflections: could not insert work at native reflection composition");
    }
  } else {
    Report("MgsvReflections: missing late scene or reflection material target");
  }
  // Scheduling can finish the prefix and reset recording-local private data.
  // Save displaced inputs afterward, so EndComposite can always restore them.
  std::array<ID3D11ShaderResourceView*, 4> previous = {};
  context->PSGetShaderResources(18u, static_cast<UINT>(previous.size()), previous.data());
  for (size_t index = 0u; index < previous.size(); ++index) state->previous[index].Attach(previous[index]);
  state->active = true;
  context->PSSetShaderResources(18u, static_cast<UINT>(outputs.size()), outputs.data());
  return true;
}

inline void EndComposite(reshade::api::command_list* cmd_list) {
  auto* state = cmd_list->get_private_data<CompositeState>();
  if (state == nullptr || !state->active) return;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  std::array<ID3D11ShaderResourceView*, 4> previous = {};
  for (size_t index = 0u; index < previous.size(); ++index) previous[index] = state->previous[index].Get();
  context->PSSetShaderResources(18u, static_cast<UINT>(previous.size()), previous.data());
  *state = {};
}

}  // namespace mgsv::lighting::reflections
#endif
