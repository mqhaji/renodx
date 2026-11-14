#ifndef RENODX_GAMES_MGSV_RENDERING_D3D12_BRIDGE_TEST_HPP_
#define RENODX_GAMES_MGSV_RENDERING_D3D12_BRIDGE_TEST_HPP_

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <utility>

#include <embed/bridge_copy.h>

#include "d3d12_bridge.hpp"

namespace mgsv::rendering::d3d12_bridge_test {

class CopyPass {
 public:
  Microsoft::WRL::ComPtr<ID3D12RootSignature> root;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;

  explicit CopyPass(ID3D12Device* device) {
    const std::array ranges = {
        D3D12_DESCRIPTOR_RANGE{.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV, .NumDescriptors = 1u, .BaseShaderRegister = 0u, .OffsetInDescriptorsFromTableStart = 0u},
        D3D12_DESCRIPTOR_RANGE{.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV, .NumDescriptors = 1u, .BaseShaderRegister = 0u, .OffsetInDescriptorsFromTableStart = 1u},
    };
    D3D12_ROOT_PARAMETER parameter = {};
    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameter.DescriptorTable = {.NumDescriptorRanges = static_cast<UINT>(ranges.size()), .pDescriptorRanges = ranges.data()};
    const D3D12_ROOT_SIGNATURE_DESC signature = {.NumParameters = 1u, .pParameters = &parameter};
    Microsoft::WRL::ComPtr<ID3DBlob> serialized;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    d3d12_bridge::Check(renodx::utils::directx::pD3D12SerializeRootSignature(&signature, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors), "Serialize bridge test root signature");
    d3d12_bridge::Check(device->CreateRootSignature(0u, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&root)), "Create bridge test root signature");
    const D3D12_COMPUTE_PIPELINE_STATE_DESC description = {
        .pRootSignature = root.Get(),
        .CS = {.pShaderBytecode = __bridge_copy.data(), .BytecodeLength = __bridge_copy.size()},
    };
    d3d12_bridge::Check(device->CreateComputePipelineState(&description, IID_PPV_ARGS(&pipeline)), "Create bridge test pipeline");
  }

  void Record(const d3d12_bridge::Dispatch& dispatch) const {
    // Make each API handoff explicit. COMMON is the shared boundary state.
    std::array<D3D12_RESOURCE_BARRIER, 2> barriers = {};
    for (auto& barrier : barriers) {
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    }
    barriers[0].Transition.pResource = dispatch.input;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    barriers[1].Transition.pResource = dispatch.output;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    dispatch.commands->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    dispatch.commands->SetPipelineState(pipeline.Get());
    dispatch.commands->SetComputeRootSignature(root.Get());
    const std::array descriptors = {dispatch.descriptors};
    dispatch.commands->SetDescriptorHeaps(static_cast<UINT>(descriptors.size()), descriptors.data());
    dispatch.commands->SetComputeRootDescriptorTable(0u, dispatch.input_view);
    dispatch.commands->Dispatch((dispatch.width + 7u) / 8u, (dispatch.height + 7u) / 8u, 1u);
    for (auto& barrier : barriers) {
      std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    }
    dispatch.commands->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
  }
};

enum class Mode : uint8_t {
  OFF,
  COPY,
  VERIFY,
  PROFILE,
};
static std::atomic<Mode> mode = Mode::OFF;

struct Validation {
  std::array<Microsoft::WRL::ComPtr<ID3D11Texture2D>, 2> staging;
  D3D11_TEXTURE2D_DESC description = {};
  d3d12_bridge::Scene scene;
  uint64_t completion = 0u;
};

// One sample is allowed in flight. Results are read on a later frame, then the
// same queries are reused. Profiling stops after a short, explicit request.
struct Timing {
  static constexpr uint32_t SAMPLE_COUNT = 64u;
  std::array<Microsoft::WRL::ComPtr<ID3D11Query>, 2> timestamps;
  Microsoft::WRL::ComPtr<ID3D11Query> disjoint;
  Microsoft::WRL::ComPtr<ID3D12QueryHeap> queries;
  Microsoft::WRL::ComPtr<ID3D12Resource> readback;
  uint64_t frequency = 0u;
  uint32_t samples = 0u;
  uint32_t warmup = 32u;
  double submission_us = 0.;
  double total_us = 0.;
  double compute_us = 0.;
  double pending_submission_us = 0.;
  d3d12_bridge::SubmissionTiming pending_cpu;
  d3d12_bridge::SubmissionTiming cpu;
  uint64_t initial_imports = 0u;
  bool pending = false;

  explicit Timing(const d3d12_bridge::Bridge& bridge) {
    initial_imports = bridge.statistics.image_imports;
    const D3D11_QUERY_DESC timestamp = {.Query = D3D11_QUERY_TIMESTAMP};
    for (auto& query : timestamps) {
      d3d12_bridge::Check(bridge.device11->CreateQuery(&timestamp, &query), "Create bridge timestamp");
    }
    const D3D11_QUERY_DESC clock = {.Query = D3D11_QUERY_TIMESTAMP_DISJOINT};
    d3d12_bridge::Check(bridge.device11->CreateQuery(&clock, &disjoint), "Create bridge timestamp clock");
    const D3D12_QUERY_HEAP_DESC heap = {.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP, .Count = 2u};
    d3d12_bridge::Check(bridge.device12->CreateQueryHeap(&heap, IID_PPV_ARGS(&queries)), "Create D3D12 timestamps");
    const D3D12_HEAP_PROPERTIES properties = {.Type = D3D12_HEAP_TYPE_READBACK};
    const D3D12_RESOURCE_DESC description = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Width = 2u * sizeof(uint64_t),
        .Height = 1u,
        .DepthOrArraySize = 1u,
        .MipLevels = 1u,
        .SampleDesc = {.Count = 1u},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
    };
    d3d12_bridge::Check(bridge.device12->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &description,
                                                                 D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)),
                        "Create bridge timestamp readback");
    d3d12_bridge::Check(bridge.queue12->GetTimestampFrequency(&frequency), "Read D3D12 timestamp frequency");
  }
};

struct Status {
  d3d12_bridge::Statistics statistics;
  uint64_t verified = 0u;
  uint64_t mismatched_bytes = 0u;
  uint32_t slots = 0u;
  bool pending = false;
  uint32_t timed_samples = 0u;
  double submission_us = 0.;
  double total_us = 0.;
  double compute_us = 0.;
  std::string error;
};

struct Runtime {
  std::shared_mutex mutex;
  std::unique_ptr<d3d12_bridge::Bridge> bridge;
  std::unique_ptr<CopyPass> pass;
  std::optional<Validation> validation;
  std::unique_ptr<Timing> timing;
  Status status;
  uint32_t verified_width = 0u;
  uint32_t verified_height = 0u;
  uint64_t verified_generation = 0u;
  bool verification_requested = true;
  bool profiling_requested = false;
};
static Runtime runtime;

inline void SetMode(Mode selected) {
  const std::unique_lock lock(runtime.mutex);
  if (mode.exchange(selected, std::memory_order_relaxed) != selected) {
    runtime.verification_requested = true;
    runtime.profiling_requested = selected == Mode::PROFILE;
  }
}

inline Status GetStatus() {
  const std::shared_lock lock(runtime.mutex);
  return runtime.status;
}

inline void ReadTiming() {
  if (runtime.timing == nullptr || !runtime.timing->pending) return;
  auto& timing = *runtime.timing;
  auto* context = runtime.bridge->context11.Get();
  D3D11_QUERY_DATA_TIMESTAMP_DISJOINT clock = {};
  const auto ready = context->GetData(timing.disjoint.Get(), &clock, sizeof(clock), D3D11_ASYNC_GETDATA_DONOTFLUSH);
  if (ready == S_FALSE) return;
  d3d12_bridge::Check(ready, "Read bridge timestamp clock");
  std::array<uint64_t, 2> times = {};
  for (size_t index = 0u; index < times.size(); ++index) {
    const auto result = context->GetData(timing.timestamps[index].Get(), &times[index], sizeof(times[index]), D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if (result == S_FALSE) return;
    d3d12_bridge::Check(result, "Read bridge timestamp");
  }
  timing.pending = false;
  if (clock.Disjoint != FALSE || clock.Frequency == 0u) return;
  const D3D12_RANGE range = {.Begin = 0u, .End = 2u * sizeof(uint64_t)};
  void* data = nullptr;
  d3d12_bridge::Check(timing.readback->Map(0u, &range, &data), "Read D3D12 timestamps");
  std::array<uint64_t, 2> compute = {};
  std::memcpy(compute.data(), data, sizeof(compute));
  const D3D12_RANGE written = {};
  timing.readback->Unmap(0u, &written);
  timing.compute_us += static_cast<double>(compute[1] - compute[0]) * 1e6 / static_cast<double>(timing.frequency);
  timing.total_us += static_cast<double>(times[1] - times[0]) * 1e6 / static_cast<double>(clock.Frequency);
  timing.submission_us += timing.pending_submission_us;
  timing.cpu.preparation_us += timing.pending_cpu.preparation_us;
  timing.cpu.recording_us += timing.pending_cpu.recording_us;
  timing.cpu.queue_us += timing.pending_cpu.queue_us;
  timing.cpu.input_copy_us += timing.pending_cpu.input_copy_us;
  timing.cpu.signal11_us += timing.pending_cpu.signal11_us;
  timing.cpu.wait12_us += timing.pending_cpu.wait12_us;
  timing.cpu.execute12_us += timing.pending_cpu.execute12_us;
  timing.cpu.signal12_us += timing.pending_cpu.signal12_us;
  timing.cpu.wait11_us += timing.pending_cpu.wait11_us;
  timing.cpu.return_copy_us += timing.pending_cpu.return_copy_us;
  ++timing.samples;
  runtime.status.timed_samples = timing.samples;
  runtime.status.submission_us = timing.submission_us / timing.samples;
  runtime.status.total_us = timing.total_us / timing.samples;
  runtime.status.compute_us = timing.compute_us / timing.samples;
  if (timing.samples == Timing::SAMPLE_COUNT) {
    const std::string message = "MGSV D3D12 bridge profile samples=" + std::to_string(timing.samples)
                                + " cpu_submission_us=" + std::to_string(runtime.status.submission_us)
                                + " cpu_preparation_us=" + std::to_string(timing.cpu.preparation_us / timing.samples)
                                + " cpu_recording_us=" + std::to_string(timing.cpu.recording_us / timing.samples)
                                + " cpu_queue_us=" + std::to_string(timing.cpu.queue_us / timing.samples)
                                + " cpu_input_copy_us=" + std::to_string(timing.cpu.input_copy_us / timing.samples)
                                + " cpu_signal11_us=" + std::to_string(timing.cpu.signal11_us / timing.samples)
                                + " cpu_wait12_us=" + std::to_string(timing.cpu.wait12_us / timing.samples)
                                + " cpu_execute12_us=" + std::to_string(timing.cpu.execute12_us / timing.samples)
                                + " cpu_signal12_us=" + std::to_string(timing.cpu.signal12_us / timing.samples)
                                + " cpu_wait11_us=" + std::to_string(timing.cpu.wait11_us / timing.samples)
                                + " cpu_return_copy_us=" + std::to_string(timing.cpu.return_copy_us / timing.samples)
                                + " gpu_total_us=" + std::to_string(runtime.status.total_us)
                                + " gpu_dispatch_us=" + std::to_string(runtime.status.compute_us)
                                + " passes=" + std::to_string(runtime.status.statistics.submitted)
                                + " image_imports=" + std::to_string(runtime.status.statistics.image_imports)
                                + " profile_image_imports=" + std::to_string(runtime.status.statistics.image_imports - timing.initial_imports)
                                + " input_copies=" + std::to_string(runtime.status.statistics.input_copies)
                                + " output_copies=" + std::to_string(runtime.status.statistics.output_copies)
                                + " busy_skips=" + std::to_string(runtime.status.statistics.busy);
    reshade::log::message(reshade::log::level::info, message.c_str());
  }
}

// Called under runtime.mutex on the serialized immediate-context path. The
// staging images read the actual native destination after its GPU fence wait.
inline void ReadValidation() {
  if (!runtime.validation.has_value()) return;
  auto& validation = *runtime.validation;
  if (!runtime.bridge->d3d11_fence->IsComplete(validation.completion)) return;
  if (runtime.bridge->DeviceRemoved()) throw std::runtime_error("Device removed during bridge validation");
  std::array<D3D11_MAPPED_SUBRESOURCE, 2> mapped = {};
  auto* context = runtime.bridge->context11.Get();
  const auto first = context->Map(validation.staging[0].Get(), 0u, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, mapped.data());
  if (first == DXGI_ERROR_WAS_STILL_DRAWING) return;
  d3d12_bridge::Check(first, "Map bridge reference");
  const auto second = context->Map(validation.staging[1].Get(), 0u, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped[1]);
  if (FAILED(second)) {
    context->Unmap(validation.staging[0].Get(), 0u);
    if (second == DXGI_ERROR_WAS_STILL_DRAWING) return;
    d3d12_bridge::Check(second, "Map bridge result");
  }
  const uint32_t row_bytes = validation.description.Width * (validation.description.Format == DXGI_FORMAT_R16G16B16A16_FLOAT ? 8u : 16u);
  uint64_t mismatches = 0u;
  for (uint32_t y = 0u; y < validation.description.Height; ++y) {
    const auto* reference = static_cast<const uint8_t*>(mapped[0].pData) + (static_cast<size_t>(y) * mapped[0].RowPitch);
    const auto* result = static_cast<const uint8_t*>(mapped[1].pData) + (static_cast<size_t>(y) * mapped[1].RowPitch);
    if (std::memcmp(reference, result, row_bytes) == 0) continue;
    for (uint32_t x = 0u; x < row_bytes; ++x) {
      mismatches += (reference[x] != result[x] ? 1u : 0u);
    }
  }
  context->Unmap(validation.staging[1].Get(), 0u);
  context->Unmap(validation.staging[0].Get(), 0u);
  ++runtime.status.verified;
  runtime.status.mismatched_bytes += mismatches;
  runtime.status.pending = false;
  runtime.verified_width = validation.description.Width;
  runtime.verified_height = validation.description.Height;
  runtime.verified_generation = validation.scene.generation;
  const std::string message = "MGSV D3D12 bridge image check " + std::to_string(validation.description.Width) + "x" + std::to_string(validation.description.Height)
                              + " scene=" + std::to_string(validation.scene.id) + " generation=" + std::to_string(validation.scene.generation)
                              + " mismatched_bytes=" + std::to_string(mismatches);
  reshade::log::message(mismatches == 0u ? reshade::log::level::info : reshade::log::level::error, message.c_str());
  runtime.validation.reset();
  if (mismatches != 0u) throw std::runtime_error("D3D12 bridge returned different pixels");
}

inline bool Run(ID3D11DeviceContext* context, ID3D11Resource* source, ID3D11Resource* destination,
                const d3d12_bridge::Scene& scene, SharedTexture* shared_source = nullptr, SharedTexture* shared_destination = nullptr) {
  const auto requested = mode.load(std::memory_order_relaxed);
  // Off does not create a D3D12 device, take the runtime lock, or poll queries.
  // Existing resources stay alive until the normal device teardown.
  if (requested == Mode::OFF) return false;
  const std::unique_lock lock(runtime.mutex);
  if (!runtime.status.error.empty()) return false;
  Timing* active_timing = nullptr;
  try {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> source_texture;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> destination_texture;
    if (source == nullptr || destination == nullptr || context == nullptr
        || FAILED(source->QueryInterface(IID_PPV_ARGS(&source_texture)))
        || FAILED(destination->QueryInterface(IID_PPV_ARGS(&destination_texture)))) return false;
    if (runtime.bridge == nullptr) {
      Microsoft::WRL::ComPtr<ID3D11Device> device;
      context->GetDevice(&device);
      runtime.bridge = std::make_unique<d3d12_bridge::Bridge>(device.Get());
      runtime.pass = std::make_unique<CopyPass>(runtime.bridge->device12.Get());
      reshade::log::message(reshade::log::level::info,
                            (runtime.bridge->device12.Get() != runtime.bridge->device12_owner.Get()
                                 ? "MGSV D3D12 bridge created on the game's adapter; private commands bypass ReShade callbacks"
                                 : "MGSV D3D12 bridge created on the game's adapter; using the returned device interface"));
    }
    ReadValidation();
    ReadTiming();
    if (requested == Mode::PROFILE && runtime.profiling_requested
        && (runtime.timing == nullptr || !runtime.timing->pending)) {
      runtime.timing = std::make_unique<Timing>(*runtime.bridge);
      runtime.profiling_requested = false;
    }
    auto* timing = runtime.timing.get();
    D3D11_TEXTURE2D_DESC description;
    source_texture->GetDesc(&description);
    std::optional<Validation> validation;
    const bool verify = requested == Mode::VERIFY
                        && (runtime.verification_requested || runtime.verified_generation != scene.generation || runtime.verified_width != description.Width || runtime.verified_height != description.Height);
    if (verify && !runtime.validation.has_value()) {
      validation.emplace();
      validation->description = description;
      validation->scene = scene;
      auto staging = description;
      staging.Usage = D3D11_USAGE_STAGING;
      staging.BindFlags = 0u;
      staging.MiscFlags = 0u;
      staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      for (auto& texture : validation->staging) {
        d3d12_bridge::Check(runtime.bridge->device11->CreateTexture2D(&staging, nullptr, &texture), "Create bridge readback");
      }
      context->CopyResource(validation->staging[0].Get(), source);
    }
    const auto allocations = runtime.bridge->statistics.allocations;
    const bool profile = requested == Mode::PROFILE && timing != nullptr && timing->samples < Timing::SAMPLE_COUNT
                         && timing->warmup == 0u && !timing->pending && !runtime.validation.has_value();
    if (profile) {
      active_timing = timing;
      context->Begin(timing->disjoint.Get());
      context->End(timing->timestamps[0].Get());
    }
    const auto submission_start = (profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
    const auto record = [profile, timing](const d3d12_bridge::Dispatch& dispatch) {
      if (profile) {
        dispatch.commands->EndQuery(timing->queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0u);
      }
      runtime.pass->Record(dispatch);
      if (profile) {
        dispatch.commands->EndQuery(timing->queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1u);
        dispatch.commands->ResolveQueryData(timing->queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0u, 2u, timing->readback.Get(), 0u);
      }
    };
    const bool executed = runtime.bridge->Execute(context, source_texture.Get(), destination_texture.Get(), scene, record,
                                                  shared_source, shared_destination, profile ? &timing->pending_cpu : nullptr);
    if (profile) {
      context->End(timing->timestamps[1].Get());
      context->End(timing->disjoint.Get());
      active_timing = nullptr;
      timing->pending_submission_us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - submission_start).count();
      timing->pending = executed;
    }
    if (executed && requested == Mode::PROFILE && timing != nullptr && timing->warmup != 0u) {
      --timing->warmup;
    }
    if (!executed && runtime.bridge->rejection != nullptr) {
      D3D11_TEXTURE2D_DESC target;
      destination_texture->GetDesc(&target);
      throw std::runtime_error(std::string(runtime.bridge->rejection)
                               + "; source=" + std::to_string(description.Width) + "x" + std::to_string(description.Height)
                               + " format=" + std::to_string(description.Format)
                               + "; destination=" + std::to_string(target.Width) + "x" + std::to_string(target.Height)
                               + " format=" + std::to_string(target.Format));
    }
    if (executed && validation.has_value()) {
      context->CopyResource(validation->staging[1].Get(), destination);
      validation->completion = renodx::utils::gpu_fence::QueueSignal(context, runtime.bridge->d3d11_fence.get());
      if (validation->completion == renodx::utils::gpu_fence::NO_SIGNAL) throw std::runtime_error("Bridge readback signal rejected");
      runtime.validation = std::move(validation);
      runtime.status.pending = true;
      runtime.verification_requested = false;
    }
    runtime.status.statistics = runtime.bridge->statistics;
    runtime.status.slots = static_cast<uint32_t>(runtime.bridge->slots.size());
    if (runtime.bridge->statistics.allocations != allocations) {
      const std::string message = "MGSV D3D12 bridge command slot " + std::to_string(description.Width) + "x" + std::to_string(description.Height)
                                  + " slots=" + std::to_string(runtime.status.slots) + " image_imports=" + std::to_string(runtime.status.statistics.image_imports)
                                  + " input_copies=" + std::to_string(runtime.status.statistics.input_copies) + " output_copies=" + std::to_string(runtime.status.statistics.output_copies);
      reshade::log::message(reshade::log::level::info, message.c_str());
    }
    return executed;
  } catch (const std::exception& error) {
    if (active_timing != nullptr) {
      context->End(active_timing->timestamps[1].Get());
      context->End(active_timing->disjoint.Get());
    }
    runtime.status.error = error.what();
    reshade::log::message(reshade::log::level::error, ("MGSV D3D12 bridge disabled after failure - " + runtime.status.error).c_str());
    return false;
  }
}

inline void Reset() {
  const std::unique_lock lock(runtime.mutex);
  if (runtime.bridge != nullptr && !runtime.bridge->Drain()) {
    reshade::log::message(reshade::log::level::error, "MGSV D3D12 bridge still has GPU work; retaining its resources");
    return;
  }
  if (runtime.bridge != nullptr && !runtime.bridge->DeviceRemoved()) {
    try {
      ReadValidation();
      ReadTiming();
    } catch (const std::exception& error) {
      reshade::log::message(reshade::log::level::error, error.what());
    }
  }
  runtime.validation.reset();
  runtime.timing.reset();
  runtime.pass.reset();
  runtime.bridge.reset();
  runtime.status = {};
  runtime.verified_generation = 0u;
  runtime.verification_requested = true;
  runtime.profiling_requested = mode.load(std::memory_order_relaxed) == Mode::PROFILE;
}

}  // namespace mgsv::rendering::d3d12_bridge_test

#endif  // RENODX_GAMES_MGSV_RENDERING_D3D12_BRIDGE_TEST_HPP_
