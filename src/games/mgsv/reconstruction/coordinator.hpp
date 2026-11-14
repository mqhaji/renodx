#pragma once

/* Method-neutral temporal candidate validation, dispatch, and completion. */

#include <windows.h>

#include <intrin.h>
#include <tlhelp32.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <d3d11_1.h>
#include <detours.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "../rendering/d3d11_state.hpp"
#include "../rendering/d3d12_bridge_test.hpp"
#include "../rendering/inputs.hpp"
#include "../rendering/native_context.hpp"
#include "../rendering/producer_records.hpp"
#include "analytical/runtime.hpp"
#include "camera_state.hpp"
#include "dlss/runtime.hpp"
#include "fsr3/runtime.hpp"
#include "input_capture.hpp"
#include "logging.hpp"
#include "postprocess/resources.hpp"
#include "state.hpp"

namespace mgsv::reconstruction::coordinator {

// Bound retained split recordings if the game delays or abandons submission.
inline constexpr size_t MAX_PENDING_DISPATCHES = 64u;
// Shadow batches must leave room for GI and the later scene reconstruction.
inline constexpr size_t RESERVED_LIGHTING_DISPATCHES = 8u;
inline auto& execution_lock = state::execution_lock;
inline reshade::api::device* runtime_device = nullptr;

// Admission follows native scene construction. Several scenes can be recorded
// before Present, and a later insertion can occur on another worker.
struct SceneAdmission {
  uint64_t scene = 0u;
  uint64_t owner_generation = 0u;
};
inline std::vector<SceneAdmission> admitted_scenes;

using BeginQuery = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Asynchronous*);
using EndQuery = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Asynchronous*);

enum class LightingPolicy : uint8_t {
  INDIRECT,
  SHADOW_BATCH,
};

struct PendingTemporalDispatch {
  rendering::native_context::Scope scope;
  reshade::api::command_list* deferred_context = nullptr;
  Microsoft::WRL::ComPtr<ID3D11CommandList> prefix;
  Microsoft::WRL::ComPtr<ID3D11CommandList> post;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> color_srv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> velocity_srv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth_srv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> object_velocity_srv;
  Microsoft::WRL::ComPtr<ID3D11Resource> output_resource;
  std::shared_ptr<post_process::Output> post_output;
  ValidatedFrameInputs inputs;
  uint64_t temporal_generation = 0u;
  state::TemporalMode mode = state::TemporalMode::OFF;
  std::function<void(reshade::api::command_list*, bool)> lighting_callback;
  bool is_lighting = false;
  bool dispatch_consumed = false;
  LightingPolicy lighting_policy = LightingPolicy::INDIRECT;
  std::function<void()> on_recording_complete;
};

// The lock owns query/list transactions; hook pointers use lifecycle serialization instead.
struct SubmissionState {
  std::shared_mutex mutex;
  std::vector<PendingTemporalDispatch> pending_dispatches;
  std::unordered_map<ID3D11DeviceContext*, std::unordered_set<ID3D11Asynchronous*>> active_queries;
};
inline SubmissionState submission;

inline BeginQuery begin_query = nullptr;
inline BeginQuery deferred_begin_query = nullptr;
inline EndQuery end_query = nullptr;
inline EndQuery deferred_end_query = nullptr;
inline bool submission_bridge_installed = false;
inline std::atomic<reshade::api::device*> submission_bridge_device = nullptr;
inline std::atomic<uint32_t> execution_hook_calls_in_flight = 0u;
inline std::atomic<bool> logged_immediate_dispatch = false;
inline std::atomic<bool> submission_failure_pending = false;

inline bool EnlistProcessThreads(std::vector<HANDLE>* handles) {
  const DWORD current_process = GetCurrentProcessId();
  const DWORD current_thread = GetCurrentThreadId();
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0u);
  if (snapshot == INVALID_HANDLE_VALUE) return false;

  THREADENTRY32 entry = {.dwSize = sizeof(entry)};
  BOOL has_entry = Thread32First(snapshot, &entry);
  while (has_entry != FALSE) {
    if (entry.th32OwnerProcessID == current_process && entry.th32ThreadID != current_thread) {
      HANDLE thread = OpenThread(
          THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
          FALSE,
          entry.th32ThreadID);
      if (thread == nullptr) {
        if (GetLastError() != ERROR_INVALID_PARAMETER) {
          CloseHandle(snapshot);
          return false;
        }
      } else {
        handles->push_back(thread);
      }
    }
    has_entry = Thread32Next(snapshot, &entry);
  }
  CloseHandle(snapshot);
  // DetourUpdateThread suspends immediately. Finish enumeration/allocation
  // before suspending threads that may own allocator or loader locks.
  if (DetourUpdateThread(GetCurrentThread()) != NO_ERROR) return false;
  return std::ranges::all_of(*handles, [](HANDLE thread) {
    return DetourUpdateThread(thread) == NO_ERROR;
  });
}

inline void CloseThreadHandles(std::vector<HANDLE>* handles) {
  for (HANDLE thread : *handles) {
    CloseHandle(thread);
  }
  handles->clear();
}

struct ExecutionHookCallGuard {
  ExecutionHookCallGuard() { execution_hook_calls_in_flight.fetch_add(1u, std::memory_order_acq_rel); }
  ExecutionHookCallGuard(const ExecutionHookCallGuard&) = delete;
  ExecutionHookCallGuard& operator=(const ExecutionHookCallGuard&) = delete;
  ~ExecutionHookCallGuard() { execution_hook_calls_in_flight.fetch_sub(1u, std::memory_order_acq_rel); }
};

struct ExecutionGuard {
  ExecutionGuard() {
    while (execution_lock.test_and_set(std::memory_order_acquire)) {
      _mm_pause();
    }
  }
  ExecutionGuard(const ExecutionGuard&) = delete;
  ExecutionGuard& operator=(const ExecutionGuard&) = delete;
  ExecutionGuard(ExecutionGuard&&) = delete;
  ExecutionGuard& operator=(ExecutionGuard&&) = delete;
  ~ExecutionGuard() { execution_lock.clear(std::memory_order_release); }
};

inline bool BindRuntimeDevice(reshade::api::device* device) {
  if (device == nullptr) return false;
  if (runtime_device == nullptr) {
    runtime_device = device;
  }
  return runtime_device == device;
}

inline bool IsForeignDevice(reshade::api::device* device) {
  return runtime_device != nullptr && runtime_device != device;
}

inline void ResetTemporalStateWithPublicationLocked(const char* reason) {
  admitted_scenes.clear();
  input_capture::InvalidateCapturedFrame();
  analytical::InvalidateHistory(reason);
  dlss::InvalidateHistory();
  fsr3::InvalidateHistory();
  camera_state::InvalidateLocked();
  camera_state::ResetMatrixHistoryLocked();
  state::ResetTemporalState();
}

inline void ResetTemporalState(const char* reason) {
  camera_state::PublicationWriterGuard publication_guard;
  ResetTemporalStateWithPublicationLocked(reason);
}

inline void Destroy(reshade::api::device* device) {
  if (runtime_device != nullptr && device != runtime_device) return;
  rendering::d3d12_bridge_test::Reset();
  input_capture::Destroy(device);
  analytical::Destroy(device);
  dlss::Destroy(device);
  fsr3::Destroy();
  post_process::Reset(true);
  submission_failure_pending.store(false, std::memory_order_release);
  runtime_device = nullptr;
  lighting::temporal_inputs::Reset();
}

inline void ReleaseInactiveResources(
    reshade::api::device* device,
    state::TemporalMode mode) {
  if (mode != state::TemporalMode::ANALYTICAL_TAA) {
    analytical::Release(device);
  }
  if (mode != state::TemporalMode::AMD_FSR3) {
    fsr3::ReleaseTemporalResources(device);
  }
  if (mode != state::TemporalMode::NVIDIA_DLSS) {
    dlss::SuspendTemporalResources(device);
  }
}

inline bool DispatchValidatedLocked(ValidatedFrameInputs& inputs) {
  if (inputs.device == nullptr) return false;
  const auto destination = (inputs.output.resource.handle != 0u ? inputs.device->get_resource_desc(inputs.output.resource) : reshade::api::resource_desc{});
  if ((state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA && !inputs.HasNativeOutputSize())
      || inputs.output.width < inputs.render_width || inputs.output.height < inputs.render_height
      || destination.texture.width != inputs.output.width || destination.texture.height != inputs.output.height
      || inputs.output.resource.handle == 0u
      || inputs.output.format != inputs.color_format) {
    static uint64_t last_output_failure_log = std::numeric_limits<uint64_t>::max();
    if (logging::ShouldLogFrame(state::CurrentFrameToken(), last_output_failure_log)) {
      logging::Warn("rejecting incompatible reconstruction output insertion=", inputs.insertion_name,
                    " render=", inputs.render_width, "x", inputs.render_height,
                    " output=", inputs.output.width, "x", inputs.output.height);
    }
    return false;
  }
  {
    const camera_state::PublicationWriterGuard guard;
    auto& camera = inputs.camera;
    const auto& previous = camera_state::committed_camera;
    if (!camera.valid || !camera.camera_matrix_valid || camera.scene == 0u || camera.reservation == 0u
        || camera.reset_generation != camera_state::reset_generation.load(std::memory_order_relaxed)
        || (previous.valid && camera.reservation <= previous.reservation)) return false;

    // Native motion describes the preceding scene. A skipped scene requires
    // fresh history because its object motion cannot span the missing update.
    camera.camera_reprojection_valid = previous.valid
                                       && previous.owner_generation == camera.owner_generation
                                       && previous.reservation + 1u == camera.reservation
                                       && previous.viewport_identity == camera.viewport_identity
                                       && previous.camera_identity == camera.camera_identity
                                       && previous.width == camera.width && previous.height == camera.height;
    camera.previous_jitter_uv_x = (camera.camera_reprojection_valid ? previous.jitter_uv_x : 0.f);
    camera.previous_jitter_uv_y = (camera.camera_reprojection_valid ? previous.jitter_uv_y : 0.f);
    camera.current_to_previous_clip = (camera.camera_reprojection_valid
                                           ? camera_state::ToRowMajorFloatArray(camera_state::Multiply(
                                                 previous.current_view_projection, camera.inverse_view_projection))
                                           : std::array<float, 16>{});
  }
  if (!inputs.camera.camera_reprojection_valid) {
    analytical::InvalidateHistory("scene history is not consecutive");
    dlss::InvalidateHistory();
    fsr3::InvalidateHistory();
  }
  const auto previous_compute_state = mgsv::rendering::d3d11_compute_state::Capture(inputs.cmd_list);
  inputs.cmd_list->barrier(
      inputs.velocity_resource,
      reshade::api::resource_usage::render_target,
      reshade::api::resource_usage::shader_resource);

  ColorTarget output = {};
  bool succeeded = false;
  switch (state::GetTemporalMode()) {
    case state::TemporalMode::ANALYTICAL_TAA:
      succeeded = analytical::Dispatch(inputs, &output);
      break;
    case state::TemporalMode::AMD_FSR3:
      succeeded = fsr3::Dispatch(inputs, &output);
      break;
    case state::TemporalMode::NVIDIA_DLSS:
      succeeded = dlss::Dispatch(inputs, &output);
      break;
    case state::TemporalMode::OFF:
      break;
  }
  if (succeeded && output.resource.handle != 0u
      && output.resource.handle != inputs.output.resource.handle
      && output.width == inputs.output.width && output.height == inputs.output.height
      && output.format == inputs.output.format) {
    inputs.cmd_list->barrier(
        output.resource,
        reshade::api::resource_usage::unordered_access,
        reshade::api::resource_usage::copy_source);
    inputs.cmd_list->barrier(
        inputs.output.resource,
        inputs.output.usage,
        reshade::api::resource_usage::copy_dest);
    // Run at execution time after the native prefix and SDK evaluation. The
    // bridge returns the same scene before its post-processing remainder runs.
    // ReShade exposes native D3D11 pointers through integer handles.
    // NOLINTBEGIN(performance-no-int-to-ptr)
    const bool bridged = rendering::d3d12_bridge_test::Run(
        reinterpret_cast<ID3D11DeviceContext*>(inputs.cmd_list->get_native()),
        reinterpret_cast<ID3D11Resource*>(output.resource.handle),
        reinterpret_cast<ID3D11Resource*>(inputs.output.resource.handle),
        {.id = inputs.camera.scene, .generation = inputs.camera.owner_generation},
        output.shared_texture.get(), inputs.output.shared_texture.get());
    // NOLINTEND(performance-no-int-to-ptr)
    if (!bridged) {
      inputs.cmd_list->copy_resource(output.resource, inputs.output.resource);
    }
    inputs.cmd_list->barrier(
        inputs.output.resource,
        reshade::api::resource_usage::copy_dest,
        inputs.output.usage);
    inputs.cmd_list->barrier(output.resource, reshade::api::resource_usage::copy_source, output.usage);
  } else {
    succeeded = false;
  }
  inputs.cmd_list->barrier(
      inputs.velocity_resource,
      reshade::api::resource_usage::shader_resource,
      reshade::api::resource_usage::render_target);
  mgsv::rendering::d3d11_compute_state::Restore(inputs.cmd_list, previous_compute_state);
  if (!succeeded) {
    // An SDK evaluation may have changed its history before output validation
    // failed. Its next evaluation must seed history again.
    analytical::InvalidateHistory("reconstruction output was not installed");
    dlss::InvalidateHistory();
    fsr3::InvalidateHistory();
    return false;
  }

  if (!camera_state::Commit(inputs.camera)) {
    ResetTemporalState("native camera matrix commit failed");
    logging::Warn("temporal native camera matrix commit failed");
    state::frame_state.reconstruction_completed = true;
  }
  return true;
}

struct ImmediateContextStateGuard {
  Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context;
  Microsoft::WRL::ComPtr<ID3DDeviceContextState> captured_state;

  explicit ImmediateContextStateGuard(ID3D11DeviceContext* device_context) {
    if (device_context == nullptr) return;
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11Device1> device1;
    Microsoft::WRL::ComPtr<ID3DDeviceContextState> empty_state;
    device_context->GetDevice(&device);
    if (device == nullptr
        || FAILED(device.As(&device1))
        || FAILED(device_context->QueryInterface(IID_PPV_ARGS(&context)))) {
      return;
    }

    const D3D_FEATURE_LEVEL feature_level = device->GetFeatureLevel();
    const UINT flags = (device->GetCreationFlags() & D3D11_CREATE_DEVICE_SINGLETHREADED) != 0u
                           ? D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED
                           : 0u;
    if (FAILED(device1->CreateDeviceContextState(
            flags,
            &feature_level,
            1u,
            D3D11_SDK_VERSION,
            __uuidof(ID3D11Device1),
            nullptr,
            &empty_state))) {
      context.Reset();
      return;
    }
    context->SwapDeviceContextState(empty_state.Get(), &captured_state);
  }

  ImmediateContextStateGuard(const ImmediateContextStateGuard&) = delete;
  ImmediateContextStateGuard& operator=(const ImmediateContextStateGuard&) = delete;

  ~ImmediateContextStateGuard() {
    if (context != nullptr && captured_state != nullptr) {
      context->SwapDeviceContextState(captured_state.Get(), nullptr);
    }
  }

  [[nodiscard]] bool IsActive() const { return captured_state != nullptr; }
};

inline void STDMETHODCALLTYPE HookBeginQuery(
    ID3D11DeviceContext* context,
    ID3D11Asynchronous* asynchronous) {
  ExecutionHookCallGuard hook_call_guard;
  {
    const std::unique_lock lock(submission.mutex);
    const auto found = submission.active_queries.find(context);
    if (found != submission.active_queries.end()) {
      found->second.insert(asynchronous);
    }
  }
  const auto begin = context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED
                             && deferred_begin_query != nullptr
                         ? deferred_begin_query
                         : begin_query;
  begin(context, asynchronous);
}

inline void STDMETHODCALLTYPE HookEndQuery(
    ID3D11DeviceContext* context,
    ID3D11Asynchronous* asynchronous) {
  ExecutionHookCallGuard hook_call_guard;
  const auto end = context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED
                           && deferred_end_query != nullptr
                       ? deferred_end_query
                       : end_query;
  end(context, asynchronous);
  const std::unique_lock lock(submission.mutex);
  const auto found = submission.active_queries.find(context);
  if (found != submission.active_queries.end()) {
    found->second.erase(asynchronous);
  }
}

inline bool EnsureTemporalSubmissionBridge(reshade::api::device* device) {
  if (submission_bridge_installed) return submission_bridge_device.load(std::memory_order_acquire) == device;
  if (device == nullptr || device->get_api() != reshade::api::device_api::d3d11) return false;
  auto* native_device = reinterpret_cast<ID3D11Device*>(device->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (native_device == nullptr) return false;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;
  native_device->GetImmediateContext(&immediate_context);
  if (immediate_context == nullptr) return false;

  constexpr size_t begin_query_vtable_index = 27u;
  constexpr size_t end_query_vtable_index = 28u;
  auto** vtable = *reinterpret_cast<void***>(immediate_context.Get());  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11Device> context_device;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> deferred_context;
  immediate_context->GetDevice(&context_device);
  if (context_device == nullptr || FAILED(context_device->CreateDeferredContext(0u, &deferred_context))) return false;
  auto** deferred_vtable = *reinterpret_cast<void***>(deferred_context.Get());  // NOLINT(performance-no-int-to-ptr)
  begin_query = reinterpret_cast<BeginQuery>(vtable[begin_query_vtable_index]);
  end_query = reinterpret_cast<EndQuery>(vtable[end_query_vtable_index]);
  if (vtable[begin_query_vtable_index] != deferred_vtable[begin_query_vtable_index]) {
    deferred_begin_query = reinterpret_cast<BeginQuery>(deferred_vtable[begin_query_vtable_index]);
  }
  if (vtable[end_query_vtable_index] != deferred_vtable[end_query_vtable_index]) {
    deferred_end_query = reinterpret_cast<EndQuery>(deferred_vtable[end_query_vtable_index]);
  }
  if (begin_query == nullptr
      || end_query == nullptr
      || DetourTransactionBegin() != NO_ERROR) {
    return false;
  }
  std::vector<HANDLE> thread_handles;
  if (!EnlistProcessThreads(&thread_handles)
      || DetourAttach(reinterpret_cast<void**>(&begin_query), reinterpret_cast<void*>(HookBeginQuery)) != NO_ERROR
      || (deferred_begin_query != nullptr
          && DetourAttach(reinterpret_cast<void**>(&deferred_begin_query), reinterpret_cast<void*>(HookBeginQuery)) != NO_ERROR)
      || DetourAttach(reinterpret_cast<void**>(&end_query), reinterpret_cast<void*>(HookEndQuery)) != NO_ERROR
      || (deferred_end_query != nullptr
          && DetourAttach(reinterpret_cast<void**>(&deferred_end_query), reinterpret_cast<void*>(HookEndQuery)) != NO_ERROR)
      || DetourTransactionCommit() != NO_ERROR) {
    DetourTransactionAbort();
    CloseThreadHandles(&thread_handles);
    begin_query = nullptr;
    deferred_begin_query = nullptr;
    end_query = nullptr;
    deferred_end_query = nullptr;
    return false;
  }
  CloseThreadHandles(&thread_handles);
  submission_bridge_installed = true;
  logging::Info("installed D3D11 temporal submission bridge");
  submission_bridge_device.store(device, std::memory_order_release);
  return true;
}

inline void UninstallTemporalSubmissionBridge() {
  if (!submission_bridge_installed) return;
  if (DetourTransactionBegin() != NO_ERROR) return;
  std::vector<HANDLE> thread_handles;
  if (!EnlistProcessThreads(&thread_handles)
      || DetourDetach(reinterpret_cast<void**>(&begin_query), reinterpret_cast<void*>(HookBeginQuery)) != NO_ERROR
      || (deferred_begin_query != nullptr
          && DetourDetach(reinterpret_cast<void**>(&deferred_begin_query), reinterpret_cast<void*>(HookBeginQuery)) != NO_ERROR)
      || DetourDetach(reinterpret_cast<void**>(&end_query), reinterpret_cast<void*>(HookEndQuery)) != NO_ERROR
      || (deferred_end_query != nullptr
          && DetourDetach(reinterpret_cast<void**>(&deferred_end_query), reinterpret_cast<void*>(HookEndQuery)) != NO_ERROR)
      || DetourTransactionCommit() != NO_ERROR) {
    DetourTransactionAbort();
    CloseThreadHandles(&thread_handles);
    logging::Warn("failed to uninstall D3D11 temporal submission bridge");
    return;
  }
  CloseThreadHandles(&thread_handles);
  while (execution_hook_calls_in_flight.load(std::memory_order_acquire) != 0u) {
    _mm_pause();
  }
  submission_bridge_device.store(nullptr, std::memory_order_release);
  std::vector<PendingTemporalDispatch> retired_dispatches;
  {
    const std::unique_lock lock(submission.mutex);
    retired_dispatches.swap(submission.pending_dispatches);
    submission.active_queries.clear();
  }
  for (const auto& pending : retired_dispatches) {
    lighting::temporal_inputs::Forget(pending.prefix.Get());
  }
  submission_bridge_installed = false;
  begin_query = nullptr;
  deferred_begin_query = nullptr;
  end_query = nullptr;
  deferred_end_query = nullptr;
  logging::Info("uninstalled D3D11 temporal submission bridge");
}

inline void OnInitCommandList(reshade::api::command_list* command_list) {
  const auto* device = submission_bridge_device.load(std::memory_order_acquire);
  if (device == nullptr || command_list == nullptr || command_list->get_device() != device) return;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  auto* native = reinterpret_cast<IUnknown*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (native == nullptr || FAILED(native->QueryInterface(IID_PPV_ARGS(&context)))
      || context->GetType() != D3D11_DEVICE_CONTEXT_DEFERRED) return;
  const std::unique_lock lock(submission.mutex);
  submission.active_queries.try_emplace(context.Get());
}

inline void CompleteRecording(reshade::api::command_list* recording, ID3D11CommandList* finished) {
  if (auto* route = recording->get_private_data<post_process::Recording>(); route != nullptr) {
    post_process::RetainOutput(recording, route->output);
    if (!route->retained_outputs.empty()) {
      auto retained = Microsoft::WRL::Make<post_process::RetainedOutputs>();
      retained->outputs = std::move(route->retained_outputs);
      if (FAILED(finished->SetPrivateDataInterface(post_process::OUTPUT_LEASE_GUID, retained.Get()))) {
        // Retire these allocations from reuse. The native list still owns its
        // D3D resources, and the pool releases its references at reset.
        const std::unique_lock lock(post_process::state.mutex);
        for (const auto& output : retained->outputs) {
          output->reusable = false;
        }
        resolution::Fail("cannot retain recorded post-processing output");
      }
    }
    // The native list finished even if attachment failed, so its producer
    // records and pending insertions must still be completed.
    route->output.reset();
    route->producer.reset();
    route->owner = {};
  }
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(recording->get_native());  // NOLINT(performance-no-int-to-ptr)
  lighting::temporal_inputs::Finish(context, finished);
  std::function<void()> on_recording_complete;
  {
    const std::unique_lock lock(submission.mutex);
    submission.active_queries[context].clear();
    const auto pending = std::ranges::find_if(
        submission.pending_dispatches,
        [recording](const PendingTemporalDispatch& candidate) {
          return candidate.deferred_context == recording && candidate.post == nullptr;
        });
    if (pending != submission.pending_dispatches.end()) {
      pending->deferred_context = nullptr;
      pending->post = finished;
      on_recording_complete = std::move(pending->on_recording_complete);
    }
  }
  // A successful FinishCommandList stops input collection for this insertion.
  // Run completion outside our lock because feature callbacks can take theirs.
  if (on_recording_complete) {
    on_recording_complete();
  }
}

inline bool ScheduleLighting(
    reshade::api::command_list* cmd_list,
    std::function<void(reshade::api::command_list*, bool)> callback,
    LightingPolicy policy = LightingPolicy::INDIRECT,
    std::function<void()> on_recording_complete = {}) {
  if (cmd_list == nullptr || !callback) return false;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (context == nullptr) return false;
  const ExecutionGuard execution_guard;
  if (context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE) {
    if (on_recording_complete) {
      on_recording_complete();
    }
    callback(cmd_list, true);
    return true;
  }
  if (submission_bridge_device.load(std::memory_order_acquire) != cmd_list->get_device()) return false;
  {
    const std::unique_lock lock(submission.mutex);
    const auto queries = submission.active_queries.find(context);
    const size_t pending_limit = (policy == LightingPolicy::SHADOW_BATCH
                                      ? MAX_PENDING_DISPATCHES - RESERVED_LIGHTING_DISPATCHES
                                      : MAX_PENDING_DISPATCHES);
    if (queries == submission.active_queries.end() || !queries->second.empty()
        || submission.pending_dispatches.size() >= pending_limit) return false;
  }
  Microsoft::WRL::ComPtr<ID3D11CommandList> prefix;
  if (FAILED(context->FinishCommandList(TRUE, &prefix)) || prefix == nullptr) return false;
  // A second split completes the preceding insertion's remainder. Keep that
  // link so GI and the later scene reconstruction execute in recorded order.
  CompleteRecording(cmd_list, prefix.Get());
  const std::unique_lock lock(submission.mutex);
  submission.pending_dispatches.push_back({
      .scope = rendering::native_context::GetScope(),
      .deferred_context = cmd_list,
      .prefix = std::move(prefix),
      .lighting_callback = std::move(callback),
      .is_lighting = true,
      .lighting_policy = policy,
      .on_recording_complete = std::move(on_recording_complete),
  });
  return true;
}

inline void OnExecuteSecondaryCommandList(
    reshade::api::command_list* command_list,
    reshade::api::command_list* secondary_command_list) {
  const auto* device = submission_bridge_device.load(std::memory_order_acquire);
  if (device == nullptr || command_list == nullptr || secondary_command_list == nullptr
      || command_list->get_device() != device || secondary_command_list->get_device() != device) return;

  // ReShade emits (finished list, deferred context) only AFTER successful
  // native FinishCommandList, with either restore flag. reset_command_list
  // alone is insufficient: it can also be emitted after a failed Finish.
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> recording_context;
  auto* secondary_native = reinterpret_cast<IUnknown*>(secondary_command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (secondary_native == nullptr) return;
  if (SUCCEEDED(secondary_native->QueryInterface(IID_PPV_ARGS(&recording_context)))) {
    if (recording_context->GetType() != D3D11_DEVICE_CONTEXT_DEFERRED) return;
    CompleteRecording(secondary_command_list,
                      reinterpret_cast<ID3D11CommandList*>(command_list->get_native()));  // NOLINT(performance-no-int-to-ptr)
    return;
  }

  auto* post = reinterpret_cast<ID3D11CommandList*>(secondary_command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  std::vector<PendingTemporalDispatch> chain;
  {
    const std::unique_lock lock(submission.mutex);
    auto* remainder = post;
    for (size_t index = 0u; index < submission.pending_dispatches.size(); ++index) {
      const auto found = std::ranges::find_if(
          submission.pending_dispatches,
          [remainder](const PendingTemporalDispatch& candidate) { return candidate.post.Get() == remainder; });
      if (found == submission.pending_dispatches.end()) break;
      chain.push_back(*found);
      found->dispatch_consumed = true;
      remainder = found->prefix.Get();
    }
  }

  auto* context = reinterpret_cast<ID3D11DeviceContext*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (context == nullptr) return;
  const auto retain_list_outputs = [&](ID3D11CommandList* native_list) {
    // A merged list keeps every output used by the nested list, including UI
    // lists that contain no reconstruction insertion of their own.
    Microsoft::WRL::ComPtr<IUnknown> retained;
    UINT retained_size = sizeof(IUnknown*);
    if (SUCCEEDED(native_list->GetPrivateData(post_process::OUTPUT_LEASE_GUID, &retained_size, retained.GetAddressOf())) && retained != nullptr) {
      const auto* leases = static_cast<post_process::RetainedOutputs*>(retained.Get());
      for (const auto& output : leases->outputs) {
        post_process::RetainOutput(command_list, output);
      }
    }
  };
  if (context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) {
    retain_list_outputs(post);
  }
  if (chain.empty()) {
    if (context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE) {
      lighting::temporal_inputs::Submit(post);
    }
    return;
  }
  std::ranges::reverse(chain);
  if (context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) {
    for (const auto& pending : chain) {
      const rendering::native_context::ScopedOwner owner_scope(pending.scope);
      retain_list_outputs(pending.prefix.Get());
      context->ExecuteCommandList(pending.prefix.Get(), TRUE);
      if (pending.post_output != nullptr) {
        post_process::RetainOutput(command_list, pending.post_output);
      }
      if (pending.is_lighting && pending.lighting_policy == LightingPolicy::SHADOW_BATCH
          && pending.lighting_callback) {
        const ExecutionGuard execution_guard;
        // Record the batch between its native prefix and remainder. The outer
        // list must recompute visibility on every replay from its current depth.
        // The callback preserves all compute and graphics state it changes.
        pending.lighting_callback(command_list, false);
      }
      if (!pending.is_lighting && !pending.dispatch_consumed && state::GetTemporalMode() == pending.mode) {
        const ExecutionGuard execution_guard;
        if (pending.temporal_generation != state::frame_state.temporal_generation) continue;
        state::frame_state.reconstruction_scheduled = false;
        logging::Warn("temporal post list was merged into another deferred context");
        submission_failure_pending.store(true, std::memory_order_release);
      }
    }
    return;
  }

  const ExecutionGuard execution_guard;
  const ImmediateContextStateGuard state_guard(context);
  for (auto& pending : chain) {
    const rendering::native_context::ScopedOwner owner_scope(pending.scope);
    context->ExecuteCommandList(pending.prefix.Get(), state_guard.IsActive() ? FALSE : TRUE);
    lighting::temporal_inputs::Submit(pending.prefix.Get());
    if (pending.is_lighting) {
      if (pending.lighting_callback) {
        // Shadows also run after the first submission; unlike temporal history,
        // their visibility belongs to this execution of the native light draws.
        pending.lighting_callback(command_list, !pending.dispatch_consumed);
      }
      continue;
    }
    const bool depth_ready = pending.post_output == nullptr
                             || pending.post_output->PrepareDepth(context, pending.inputs.camera.jitter_uv_x, pending.inputs.camera.jitter_uv_y);
    // Every native prefix runs even when its added operation was canceled.
    if (pending.temporal_generation != state::frame_state.temporal_generation
        || state::GetTemporalMode() != pending.mode) {
      if (pending.post_output != nullptr) {
        pending.post_output->CopyNative(context);
      }
      continue;
    }
    if (pending.dispatch_consumed) {
      // Replaying a cached scene refreshes its output from the native prefix.
      // It does not add the same observation to temporal history a second time.
      if (pending.post_output != nullptr) {
        pending.post_output->CopyNative(context);
      }
      continue;
    }
    if (!depth_ready || !state_guard.IsActive() || pending.inputs.device != runtime_device) {
      state::frame_state.reconstruction_scheduled = false;
      logging::Warn("rejected scene reconstruction submission because its device or saved state is unavailable");
      submission_failure_pending.store(true, std::memory_order_release);
      if (pending.post_output != nullptr) {
        pending.post_output->CopyNative(context);
      }
      continue;
    }
    const uint64_t current_frame = state::CurrentFrameToken();
    if (!logged_immediate_dispatch.exchange(true, std::memory_order_relaxed)) {
      logging::Info("executing temporal dispatch on immediate context mode=", static_cast<uint32_t>(pending.mode),
                    " frame=", pending.inputs.frame_token, " submission_frame=", current_frame);
    }
    pending.inputs.cmd_list = command_list;
    if (!DispatchValidatedLocked(pending.inputs)) {
      state::frame_state.reconstruction_scheduled = false;
      if (pending.post_output != nullptr) {
        pending.post_output->CopyNative(context);
      }
    }
  }
  // ReShade submits this final native remainder immediately after this event.
  lighting::temporal_inputs::Submit(post);
}

inline void OnDestroyCommandList(reshade::api::command_list* command_list) {
  lighting::temporal_inputs::Destroy(command_list);
  const auto* device = submission_bridge_device.load(std::memory_order_acquire);
  if (device == nullptr || command_list == nullptr || command_list->get_device() != device) return;
  auto* native_command_list = reinterpret_cast<ID3D11CommandList*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  auto* native_context = reinterpret_cast<ID3D11DeviceContext*>(command_list->get_native());     // NOLINT(performance-no-int-to-ptr)
  bool abandoned = false;
  uint64_t abandoned_generation = 0u;
  std::vector<PendingTemporalDispatch> retired;
  {
    const std::unique_lock lock(submission.mutex);
    submission.active_queries.erase(native_context);
    std::vector<ID3D11CommandList*> orphaned_lists = {native_command_list};
    // Intermediate prefixes were finished through the native context, so they
    // have no ReShade wrapper destruction event. Retire their whole chain.
    bool removed = true;
    while (removed) {
      removed = false;
      for (auto pending = submission.pending_dispatches.begin(); pending != submission.pending_dispatches.end();) {
        const bool matches = pending->deferred_context == command_list
                             || std::ranges::find(orphaned_lists, pending->post.Get()) != orphaned_lists.end();
        if (!matches) {
          ++pending;
          continue;
        }
        if (!pending->is_lighting && !pending->dispatch_consumed) {
          abandoned = true;
          abandoned_generation = pending->temporal_generation;
        }
        orphaned_lists.push_back(pending->prefix.Get());
        retired.push_back(std::move(*pending));
        pending = submission.pending_dispatches.erase(pending);
        removed = true;
      }
    }
  }
  for (const auto& pending : retired) {
    lighting::temporal_inputs::Forget(pending.prefix.Get());
  }
  if (abandoned && state::IsEnabled()) {
    ExecutionGuard execution_guard;
    for (const auto& pending : retired) {
      if (pending.is_lighting || pending.dispatch_consumed) continue;
      std::erase_if(admitted_scenes, [&](const SceneAdmission& candidate) {
        return candidate.scene == pending.scope.owner.scene && candidate.owner_generation == pending.scope.owner.generation;
      });
    }
    if (abandoned_generation == state::frame_state.temporal_generation
        && state::frame_state.reconstruction_scheduled) {
      state::frame_state.reconstruction_scheduled = false;
      logging::Info("temporal command list abandoned; retrying a later candidate");
    }
  }
}

inline void MaybeRunLocked(
    reshade::api::command_list* cmd_list,
    const inputs::descriptor_tracker::CommandListData& command_data,
    const char* insertion_name) {
  if (!state::IsEnabled()) return;
  const auto scope = rendering::native_context::GetScope();
  // Utility views keep their native size and do not schedule reconstruction.
  if (scope.owner.scene != 0u && !scope.owner.main_view) return;
  static bool missing_owner = false;
  if (scope.owner.scene == 0u || !scope.owner.main_view || !scope.owner.active_member || !scope.owner.presentation_matches) {
    if (!missing_owner) {
      logging::Warn("reconstruction waiting for main-view ownership at ", insertion_name,
                    " scene=", scope.owner.scene, " generation=", scope.owner.generation,
                    " main=", scope.owner.main_view, " member=", scope.owner.active_member,
                    " presentation=", scope.owner.presentation_matches);
      missing_owner = true;
    }
    return;
  }
  if (missing_owner) {
    logging::Info("reconstruction main-view ownership recovered scene=", scope.owner.scene,
                  " generation=", scope.owner.generation);
    missing_owner = false;
  }
  if (std::ranges::any_of(admitted_scenes, [&](const SceneAdmission& candidate) {
        return candidate.scene == scope.owner.scene && candidate.owner_generation == scope.owner.generation;
      })) return;

  ValidatedFrameInputs inputs = {};
  if (!input_capture::BuildValidatedFrameInputsLocked(
          cmd_list,
          command_data.pixel_srv_t0,
          insertion_name,
          inputs)) {
    return;
  }
  if (!BindRuntimeDevice(inputs.device)) {
    logging::Warn("rejecting temporal dispatch from a second D3D11 device");
    return;
  }

  if (state::GetTemporalMode() == state::TemporalMode::NVIDIA_DLSS
      && !dlss::runtime_ready.load(std::memory_order_acquire)) return;
  if (!submission_bridge_installed) return;

  std::shared_ptr<post_process::Output> post_output;
  if (!inputs.HasNativeOutputSize()
      && inputs.color_format == reshade::api::format::r16g16b16a16_float
      && (std::strcmp(insertion_name, "BeforeBloom") == 0
          || std::strcmp(insertion_name, "DOF_ScatterBakeFirst") == 0)) {
    post_output = post_process::GetActive(cmd_list);
    if (post_output != nullptr && !post_output->MatchesInputs(inputs)) {
      resolution::Fail("scene reconstruction inputs changed after output recording began");
      return;
    }
    if (post_output == nullptr) {
      try {
        post_output = post_process::AcquireOutput(inputs);
      } catch (const std::exception& error) {
        logging::Warn(error.what());
        resolution::Fail("scaled post-processing resources unavailable");
      }
    }
    if (post_output != nullptr) {
      inputs.output = post_output->target;
    }
  }
  if (!inputs.HasNativeOutputSize() && post_output == nullptr) {
    resolution::Fail("scaled reconstruction has no compatible output route");
    return;
  }

  auto* native_context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  std::shared_ptr<post_process::OutputProducer> output_producer;
  if (post_output != nullptr) {
    post_process::Recording route;
    post_process::GetActive(cmd_list, &route, true);
    output_producer = route.producer;
  }
  const auto publish_output = [&] {
    if (post_output == nullptr) return;
    const std::unique_lock lock(post_process::state.mutex);
    post_process::Recording route = {.output = post_output, .producer = output_producer, .frame = inputs.frame_token, .generation = post_process::state.generation, .owner = scope.owner};
    output_producer->reconstruction_recorded = true;
    post_process::state.active = post_output;
    post_process::state.producer = output_producer;
    post_process::state.frame = inputs.frame_token;
    post_process::state.active_owner = scope.owner;
    if (const auto pooled = std::ranges::find(post_process::state.pool, post_output, &post_process::Recording::output); pooled != post_process::state.pool.end()) {
      *pooled = route;
    }
    const auto existing = std::ranges::find_if(post_process::state.sequences, [&](const post_process::Recording& candidate) {
      return candidate.owner.scene == scope.owner.scene && candidate.owner.generation == scope.owner.generation;
    });
    if (existing == post_process::state.sequences.end()) {
      post_process::state.sequences.push_back(route);
    } else {
      *existing = route;
    }
    if (native_context != nullptr && native_context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED) {
      if (auto* recording = cmd_list->get_private_data<post_process::Recording>(); recording != nullptr) {
        post_process::RetainOutput(cmd_list, recording->output);
        route.retained_outputs = std::move(recording->retained_outputs);
        *recording = std::move(route);
      }
    }
  };
  const auto admit_scene = [&] {
    if (admitted_scenes.size() == MAX_PENDING_DISPATCHES) {
      admitted_scenes.erase(admitted_scenes.begin());
    }
    admitted_scenes.push_back({.scene = scope.owner.scene, .owner_generation = scope.owner.generation});
  };
  if (native_context != nullptr && native_context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED) {
    const auto fail_submission = [] {
      logging::Warn("failed to schedule temporal dispatch");
      submission_failure_pending.store(true, std::memory_order_release);
      state::frame_state.reconstruction_scheduled = true;
    };
    if (submission_bridge_device.load(std::memory_order_acquire) != inputs.device) {
      fail_submission();
      return;
    }
    {
      const std::unique_lock lock(submission.mutex);
      if (submission.pending_dispatches.size() >= MAX_PENDING_DISPATCHES) return;
      const auto queries = submission.active_queries.find(native_context);
      // Existing recordings may contain an unseen Begin. Wait for a natural
      // successful FinishCommandList or a fresh context before splitting them.
      if (queries == submission.active_queries.end() || !queries->second.empty()) return;
    }
    PendingTemporalDispatch pending = {
        .scope = scope,
        .deferred_context = inputs.cmd_list,
        .color_srv = reinterpret_cast<ID3D11ShaderResourceView*>(inputs.color_srv.handle),                      // NOLINT(performance-no-int-to-ptr)
        .velocity_srv = reinterpret_cast<ID3D11ShaderResourceView*>(inputs.velocity_srv.handle),                // NOLINT(performance-no-int-to-ptr)
        .depth_srv = reinterpret_cast<ID3D11ShaderResourceView*>(inputs.depth_srv.handle),                      // NOLINT(performance-no-int-to-ptr)
        .object_velocity_srv = reinterpret_cast<ID3D11ShaderResourceView*>(inputs.object_velocity_srv.handle),  // NOLINT(performance-no-int-to-ptr)
        .post_output = post_output,
        .inputs = inputs,
        .temporal_generation = state::frame_state.temporal_generation,
        .mode = state::GetTemporalMode(),
    };
    // The source SRV already retains the destination in the usual in-place
    // Native AA case. A distinct target needs its own lifetime through replay.
    if (inputs.output.resource.handle != inputs.color_resource.handle) {
      pending.output_resource = reinterpret_cast<ID3D11Resource*>(inputs.output.resource.handle);  // NOLINT(performance-no-int-to-ptr)
    }
    ID3D11CommandList* prefix = nullptr;
    const HRESULT finish_result = native_context->FinishCommandList(TRUE, &prefix);
    if (FAILED(finish_result) || prefix == nullptr) {
      if (finish_result != DXGI_ERROR_INVALID_CALL) {
        fail_submission();
      }
      return;
    }
    pending.prefix.Attach(prefix);
    CompleteRecording(cmd_list, prefix);
    {
      const std::unique_lock lock(submission.mutex);
      submission.pending_dispatches.emplace_back(std::move(pending));
    }
    publish_output();
    admit_scene();
    state::frame_state.reconstruction_scheduled = true;
    if (!logged_immediate_dispatch.load(std::memory_order_relaxed)) {
      logging::Info("scheduled temporal dispatch frame=", inputs.frame_token);
    }
    return;
  }
  ImmediateContextStateGuard state_guard(native_context);
  if (!state_guard.IsActive()) {
    logging::Warn("unable to preserve immediate D3D11 state for temporal dispatch");
    submission_failure_pending.store(true, std::memory_order_release);
    return;
  }
  const bool depth_ready = post_output == nullptr || post_output->PrepareDepth(native_context, inputs.camera.jitter_uv_x, inputs.camera.jitter_uv_y);
  const bool succeeded = depth_ready && DispatchValidatedLocked(inputs);
  admit_scene();
  if (post_output != nullptr) {
    if (!succeeded) {
      post_output->CopyNative(native_context);
    }
    publish_output();
  }
}

}  // namespace mgsv::reconstruction::coordinator
