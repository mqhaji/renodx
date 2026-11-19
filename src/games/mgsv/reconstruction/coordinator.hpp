#pragma once

/* Runs added GPU work at a chosen point inside the game's command lists. The
 * engine records with D3D11 deferred contexts, so the work cannot run when it
 * is scheduled. Instead the list is cut: FinishCommandList ends it there (the
 * prefix), and the game keeps recording into a new list (the remainder,
 * `post`). When the game executes the remainder on the immediate context, the
 * prefixes run first, each followed by its added work, in recorded order.
 * Reconstruction is cut in where the engine starts a scene's post-processing;
 * the shadow batch and the indirect lighting passes use the same cuts. */

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
#include <optional>
#include <shared_mutex>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <d3d11_1.h>
#include <detours.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "../engine/render_targets.hpp"
#include "../engine/views.hpp"
#include "../rendering/d3d11_state.hpp"
#include "../rendering/d3d12_timing.hpp"
#include "../rendering/inputs.hpp"
#include "../rendering/producer_records.hpp"
#include "analytical/runtime.hpp"
#include "camera_state.hpp"
#include "dlss/runtime.hpp"
#include "fsr/runtime.hpp"
#include "input_capture.hpp"
#include "logging.hpp"
#include "output_size/resources.hpp"
#include "state.hpp"

namespace mgsv::reconstruction::coordinator {

// Limits how many cuts can wait for execution, in case the game delays or
// drops a submission.
inline constexpr size_t MAX_PENDING_DISPATCHES = 64u;
// Shadow batches must leave room for GI and the later scene reconstruction.
inline constexpr size_t RESERVED_LIGHTING_DISPATCHES = 8u;
inline auto& execution_lock = state::execution_lock;
inline reshade::api::device* runtime_device = nullptr;
// Protected by ExecutionGuard. After a failed teardown, no new work may use
// the old device's resources, even if a new device reuses its address.
inline bool device_teardown_pending = false;

using BeginQuery = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Asynchronous*);
using EndQuery = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Asynchronous*);

enum class LightingPolicy : uint8_t {
  INDIRECT,
  SHADOW_BATCH,
};

// One cut: the native list before it (`prefix`), the added work, and the
// native list after it (`post`), set once the game finishes that list. The
// COM references keep a scene's inputs and output alive until the work runs.
struct PendingTemporalDispatch {
  engine::views::Scope scope;
  reshade::api::command_list* deferred_context = nullptr;
  Microsoft::WRL::ComPtr<ID3D11CommandList> prefix;
  Microsoft::WRL::ComPtr<ID3D11CommandList> post;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> color_srv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> velocity_srv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth_srv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> object_velocity_srv;
  Microsoft::WRL::ComPtr<ID3D11Resource> output_resource;
  std::shared_ptr<output_size::Output> post_output;
  ValidatedFrameInputs inputs;
  uint64_t temporal_generation = 0u;
  state::TemporalMode mode = state::TemporalMode::OFF;
  // The scene had no motion vectors or camera: its job only fills the output
  // from the native image.
  bool fill_only = false;
  std::function<void(reshade::api::command_list*, bool)> lighting_callback;
  bool is_lighting = false;
  // Set the first time the list is executed. A replay runs the native commands
  // again but does not add the same scene to history twice.
  bool dispatch_consumed = false;
  LightingPolicy lighting_policy = LightingPolicy::INDIRECT;
  std::function<void()> on_recording_complete;
};

// `mutex` protects these members. The query hook pointers below are not
// covered; they change only while the bridge is installed or removed.
struct SubmissionState {
  std::shared_mutex mutex;
  std::vector<PendingTemporalDispatch> pending_dispatches;
  // Queries begun but not yet ended on each tracked deferred context. A
  // context with an open query is not cut.
  std::unordered_map<ID3D11DeviceContext*, std::unordered_set<ID3D11Asynchronous*>> active_queries;
  // The ReShade command list of each deferred context above.
  std::unordered_map<ID3D11DeviceContext*, reshade::api::command_list*> recording_lists;
};
inline SubmissionState submission;

// The D3D11 submission bridge: hooks on ID3D11DeviceContext::Begin and End
// (immediate and, when different, deferred) that track open queries, plus the
// command-list event handlers in this file. Lighting and every temporal method
// share it.
inline BeginQuery begin_query = nullptr;
inline BeginQuery deferred_begin_query = nullptr;
inline EndQuery end_query = nullptr;
inline EndQuery deferred_end_query = nullptr;
inline bool submission_bridge_installed = false;
inline std::atomic<reshade::api::device*> submission_bridge_device = nullptr;
inline std::atomic<uint32_t> execution_hook_calls_in_flight = 0u;
inline std::atomic<bool> logged_immediate_dispatch = false;
// Set when scheduled work could not run as planned. The next Present turns
// temporal reconstruction off.
inline std::atomic<bool> submission_failure_pending = false;
// Set once contact shadows have asked for a shadow batch. Present then
// installs the bridge even when no temporal feature needs it.
inline std::atomic<bool> shadow_batch_requested = false;

// Adds every other thread of the process to the open Detours transaction, so
// no thread is running the bytes being patched.
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
  // DetourUpdateThread suspends the thread at once. Finish enumerating and
  // allocating first, since a suspended thread may hold the heap or loader lock.
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

// Counts calls inside the query hooks, so removing them can wait until no call
// is still running.
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

// The first device seen becomes the reconstruction device; later calls succeed
// only for that device.
inline bool BindRuntimeDevice(reshade::api::device* device) {
  if (device == nullptr || device_teardown_pending) return false;
  if (runtime_device == nullptr) {
    runtime_device = device;
  }
  return runtime_device == device;
}

inline bool IsForeignDevice(reshade::api::device* device) {
  return runtime_device != nullptr && runtime_device != device;
}

inline void ResetTemporalStateWithPublicationLocked(const char* reason) {
  analytical::InvalidateHistory(reason);
  dlss::InvalidateHistory();
  fsr::InvalidateHistory();
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
  dx12::Drain();
  analytical::Destroy(device);
  dlss::Destroy(device);
  fsr::Destroy();
  dx12::Destroy();
  rendering::d3d12_bridge::Destroy();
  output_size::Reset(true);
  submission_failure_pending.store(false, std::memory_order_release);
  runtime_device = nullptr;
  lighting::temporal_inputs::Reset();
  device_teardown_pending = false;
}

// Called under ExecutionGuard at Present and before each evaluation; either can
// come first after a method change. Frees the other methods' GPU allocations
// but keeps their SDKs loaded, so switching back neither passes through Off nor
// resizes the game while a runtime loads again.
inline void ReleaseInactiveResources(reshade::api::device* device, state::TemporalMode mode) {
  if (mode != state::TemporalMode::ANALYTICAL_TAA) {
    analytical::Destroy(device);
  }
  if (mode != state::TemporalMode::AMD_FSR) {
    fsr::ReleaseContext();
  }
  if (mode != state::TemporalMode::NVIDIA_DLSS) {
    if (!dlss::ReleaseFeatureResources()) {
      throw std::runtime_error("NVIDIA DLSS feature release failed");
    }
  }
}

// Runs one scene's reconstruction on the immediate context, between the native
// prefix and remainder. Returns true when the method wrote the output.
inline bool DispatchValidatedLocked(ValidatedFrameInputs& inputs) {
  // A recorded scene can reach submission after device teardown has started.
  if (inputs.device == nullptr || device_teardown_pending) return false;
  const auto destination = (inputs.output.resource.handle != 0u ? inputs.device->get_resource_desc(inputs.output.resource) : reshade::api::resource_desc{});
  if ((state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA && !inputs.HasNativeOutputSize())
      || !IsReconstructionColorTexture(destination)
      || destination.texture.format != inputs.output.format
      || inputs.output.width < inputs.render_width || inputs.output.height < inputs.render_height
      || destination.texture.width != inputs.output.width || destination.texture.height != inputs.output.height
      || inputs.output.resource.handle == 0u
      || inputs.output.format != inputs.color_format) {
    static uint64_t last_output_failure_log = std::numeric_limits<uint64_t>::max();
    if (logging::ShouldLogFrame(state::CurrentFrameToken(), last_output_failure_log)) {
      logging::Warn("rejecting incompatible reconstruction output scene=", inputs.camera.scene,
                    " format=", static_cast<uint32_t>(destination.texture.format),
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

    // The engine's motion vectors lead back to the scene just before this one.
    // If a scene was skipped, start fresh history: the motion cannot span it.
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
  // Apply a preparation shader the lighting probe has queued.
  if (dx12::probe_pending.load(std::memory_order_acquire)) {
    dx12::ProbeUpdate update;
    {
      const std::unique_lock lock(dx12::probe_state.mutex);
      update = std::move(dx12::probe_state.pending);
      dx12::probe_state.pending = {};
      dx12::probe_pending.store(false, std::memory_order_release);
    }
    try {
      const std::span<const uint8_t> code = (update.restore ? std::span<const uint8_t>(__prepare_reconstruction) : std::span<const uint8_t>(update.code));
      Microsoft::WRL::ComPtr<ID3D11ComputeShader> replacement;
      auto* native = reinterpret_cast<ID3D11Device*>(inputs.device->get_native());  // NOLINT(performance-no-int-to-ptr)
      dx12::Check(native->CreateComputeShader(code.data(), code.size(), nullptr, &replacement), "Create live reconstruction preparation");
      // Commands already recorded keep their own reference to the old shader.
      // Only the preparation shader changes; D3D12 resources stay as they are.
      if (dx12::runtime.prepare != nullptr) {
        dx12::runtime.prepare = std::move(replacement);
      }
      dx12::probe_state.active_code = std::move(update.code);
      // Different inputs start fresh method history. The scene, camera sample
      // and resources are kept.
      analytical::InvalidateHistory("probe reconstruction inputs changed");
      dlss::InvalidateHistory();
      fsr::InvalidateHistory();
      {
        const std::unique_lock lock(dx12::probe_state.mutex);
        auto& status = dx12::probe_state.status;
        status.active_revision = update.revision;
        status.live = !dx12::probe_state.active_code.empty();
        status.pending = dx12::probe_pending.load(std::memory_order_acquire);
        if (status.revision == update.revision) {
          status.error = {};
        }
      }
      dx12::probe_state.report_submission = true;
      logging::Info("probe reconstruction inputs changed; SDK history restarted");
    } catch (const std::exception& error) {
      {
        const std::unique_lock lock(dx12::probe_state.mutex);
        auto& status = dx12::probe_state.status;
        if (status.revision == update.revision) {
          const size_t message_size = std::min(std::strlen(error.what()), status.error.size() - 1u);
          std::copy_n(error.what(), message_size, status.error.data());
          status.error[message_size] = '\0';
        }
        status.pending = dx12::probe_pending.load(std::memory_order_acquire);
      }
      logging::Warn("probe reconstruction update rejected: ", error.what());
    }
  }
  if (!inputs.camera.camera_reprojection_valid) {
    analytical::InvalidateHistory("scene history is not consecutive");
    dlss::InvalidateHistory();
    fsr::InvalidateHistory();
  }
  const auto previous_compute_state = mgsv::rendering::d3d11_compute_state::Capture(inputs.cmd_list);
  inputs.cmd_list->barrier(
      inputs.velocity_resource,
      reshade::api::resource_usage::render_target,
      reshade::api::resource_usage::shader_resource);

  bool succeeded = false;
  try {
    ReleaseInactiveResources(inputs.device, state::GetTemporalMode());
    // Each method writes inputs.output through the shared D3D12 transport
    // (d3d12.hpp). It queues a D3D11 wait, so native post-processing reads
    // that image only after D3D12 has written it.
    switch (state::GetTemporalMode()) {
      case state::TemporalMode::ANALYTICAL_TAA: succeeded = analytical::Dispatch(inputs); break;
      case state::TemporalMode::AMD_FSR:        succeeded = fsr::Dispatch(inputs); break;
      case state::TemporalMode::NVIDIA_DLSS:    succeeded = dlss::Dispatch(inputs); break;
      case state::TemporalMode::OFF:            break;
    }
  } catch (const std::exception& error) {
    static uint64_t last_backend_failure = std::numeric_limits<uint64_t>::max();
    if (logging::ShouldLogFrame(state::CurrentFrameToken(), last_backend_failure)) {
      logging::Warn("D3D12 reconstruction failed: ", error.what());
    }
    // An SDK may have changed its internal state while recording the discarded
    // work, so its history cannot be trusted. DLSS and FSR are turned off at
    // the next Present instead of reusing it.
    if (state::GetTemporalMode() == state::TemporalMode::NVIDIA_DLSS) {
      dlss::QueueFatalFailure();
    } else if (state::GetTemporalMode() == state::TemporalMode::AMD_FSR) {
      // FSR then reports itself unavailable, and the next settings
      // transition selects Off and shows the reason.
      fsr::runtime_failed.store(true, std::memory_order_release);
    }
    succeeded = false;
  }
  inputs.cmd_list->barrier(
      inputs.velocity_resource,
      reshade::api::resource_usage::shader_resource,
      reshade::api::resource_usage::render_target);
  mgsv::rendering::d3d11_compute_state::Restore(inputs.cmd_list, previous_compute_state);
  if (!succeeded) {
    // An SDK evaluation may have changed its history before output validation
    // failed. Its next evaluation must start fresh history.
    analytical::InvalidateHistory("reconstruction output was not installed");
    dlss::InvalidateHistory();
    fsr::InvalidateHistory();
    return false;
  }

  if (dx12::probe_state.report_submission) {
    const std::unique_lock lock(dx12::probe_state.mutex);
    dx12::probe_state.status.submitted_revision = dx12::probe_state.status.active_revision;
    dx12::probe_state.report_submission = false;
  }
  if (!camera_state::Commit(inputs.camera)) {
    ResetTemporalState("native camera matrix commit failed");
    logging::Warn("temporal native camera matrix commit failed");
    state::frame_state.reconstruction_completed = true;
  }
  return true;
}

// Saves the immediate context's whole state by swapping in an empty context
// state, and swaps the game's state back when destroyed. The prefixes and the
// added work run in between without disturbing what the game had bound.
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

// Installs the query hooks once, for the first D3D11 device. Deferred contexts
// can use different Begin and End functions, so those are hooked as well.
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
    submission.recording_lists.clear();
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

// Starts tracking each new deferred context; only tracked contexts are cut.
inline void OnInitCommandList(reshade::api::command_list* command_list) {
  const auto* device = submission_bridge_device.load(std::memory_order_acquire);
  if (device == nullptr || command_list == nullptr || command_list->get_device() != device) return;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  auto* native = reinterpret_cast<IUnknown*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (native == nullptr || FAILED(native->QueryInterface(IID_PPV_ARGS(&context)))
      || context->GetType() != D3D11_DEVICE_CONTEXT_DEFERRED) return;
  const std::unique_lock lock(submission.mutex);
  submission.active_queries.try_emplace(context.Get());
  submission.recording_lists.insert_or_assign(context.Get(), command_list);
}

// Work scheduled from an engine hook knows only the native deferred context.
inline reshade::api::command_list* FindRecordingList(ID3D11DeviceContext* context) {
  const std::shared_lock lock(submission.mutex);
  const auto found = submission.recording_lists.find(context);
  return found != submission.recording_lists.end() ? found->second : nullptr;
}

// Called whenever a deferred context finishes a list, whether the game or a
// cut finished it. Moves the context's output references onto the finished
// list and links a waiting cut to its remainder.
inline void CompleteRecording(reshade::api::command_list* recording, ID3D11CommandList* finished) {
  if (auto* route = recording->get_private_data<output_size::Recording>(); route != nullptr) {
    output_size::RetainOutput(recording, route->output);
    if (!route->retained_outputs.empty()) {
      auto retained = Microsoft::WRL::Make<output_size::RetainedOutputs>();
      retained->outputs = std::move(route->retained_outputs);
      if (FAILED(finished->SetPrivateDataInterface(output_size::OUTPUT_LEASE_GUID, retained.Get()))) {
        // Stop reusing these allocations. The native list still holds its own
        // D3D references, and the pool releases its references at reset.
        const std::unique_lock lock(output_size::state.mutex);
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
    // A context created before the bridge was installed is first seen here.
    submission.recording_lists.insert_or_assign(context, recording);
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
  // A successful FinishCommandList ends input collection for this cut. Run the
  // callback outside our lock, because feature callbacks take their own locks.
  if (on_recording_complete) {
    on_recording_complete();
  }
}

// Cuts `cmd_list` here so `callback` runs when the game executes the list; its
// bool argument is true on the list's first execution. On the immediate
// context the callback runs at once. Returns false when the list cannot be cut.
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
  // If this list already had a cut, this prefix is that cut's remainder.
  // Linking them keeps GI and the later scene reconstruction in recorded order.
  CompleteRecording(cmd_list, prefix.Get());
  const std::unique_lock lock(submission.mutex);
  submission.pending_dispatches.push_back({
      .scope = engine::views::GetScope(),
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

  // ReShade sends this event as (finished list, deferred context) only after
  // FinishCommandList succeeds, whatever its restore flag. reset_command_list
  // is not enough, because it also follows a failed FinishCommandList.
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> recording_context;
  auto* secondary_native = reinterpret_cast<IUnknown*>(secondary_command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (secondary_native == nullptr) return;
  if (SUCCEEDED(secondary_native->QueryInterface(IID_PPV_ARGS(&recording_context)))) {
    if (recording_context->GetType() != D3D11_DEVICE_CONTEXT_DEFERRED) return;
    CompleteRecording(secondary_command_list,
                      reinterpret_cast<ID3D11CommandList*>(command_list->get_native()));  // NOLINT(performance-no-int-to-ptr)
    return;
  }

  // Otherwise the game is executing a finished list. Collect the cuts that lead
  // to it by following each cut's prefix back to the cut before it.
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
    // A list executed into another deferred list passes on every output it
    // uses, including UI lists that have no cut.
    Microsoft::WRL::ComPtr<IUnknown> retained;
    UINT retained_size = sizeof(IUnknown*);
    if (SUCCEEDED(native_list->GetPrivateData(output_size::OUTPUT_LEASE_GUID, &retained_size, retained.GetAddressOf())) && retained != nullptr) {
      const auto* leases = static_cast<output_size::RetainedOutputs*>(retained.Get());
      for (const auto& output : leases->outputs) {
        output_size::RetainOutput(command_list, output);
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
  // Executed into another deferred list: replay the prefixes into it. Only a
  // shadow batch can be recorded there; a reconstruction cannot, and its
  // scene is reported as a submission failure.
  if (context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) {
    for (const auto& pending : chain) {
      const engine::views::ScopedOwner owner_scope(pending.scope);
      retain_list_outputs(pending.prefix.Get());
      context->ExecuteCommandList(pending.prefix.Get(), TRUE);
      if (pending.post_output != nullptr) {
        output_size::RetainOutput(command_list, pending.post_output);
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

  // Declared before the guards so its total includes their release.
  std::optional<rendering::d3d12_timing::CpuSubmission> cpu_timing;
  if (std::ranges::any_of(chain, [](const PendingTemporalDispatch& pending) { return !pending.is_lighting; })) {
    cpu_timing.emplace(rendering::d3d12_timing::Stream::RECONSTRUCTION);
  }
  const ExecutionGuard execution_guard;
  const ImmediateContextStateGuard state_guard(context);
  rendering::d3d12_timing::CpuSubmission::EndPart("D3D11 state save");
  for (auto& pending : chain) {
    const engine::views::ScopedOwner owner_scope(pending.scope);
    context->ExecuteCommandList(pending.prefix.Get(), state_guard.IsActive() ? FALSE : TRUE);
    lighting::temporal_inputs::Submit(pending.prefix.Get());
    rendering::d3d12_timing::CpuSubmission::EndPart("native prefix");
    if (pending.is_lighting) {
      if (pending.lighting_callback) {
        // Lighting runs on every execution, not only the first: unlike temporal
        // history, shadow visibility belongs to this execution of the light draws.
        pending.lighting_callback(command_list, !pending.dispatch_consumed);
      }
      continue;
    }
    // Every path below reads output-size depth, and the fill uses constants
    // this preparation writes, so it runs first.
    const bool depth_ready = pending.post_output == nullptr
                             || pending.post_output->PrepareDepth(context, pending.inputs.camera.jitter_uv_x, pending.inputs.camera.jitter_uv_y);
    rendering::d3d12_timing::CpuSubmission::EndPart("output depth");
    // Every native prefix runs even when its added work was canceled. A
    // replayed scene is not added to history a second time; while upscaling,
    // its output is refreshed from the native image instead.
    const bool reconstructs = !pending.fill_only && !pending.dispatch_consumed
                              && pending.temporal_generation == state::frame_state.temporal_generation
                              && state::GetTemporalMode() == pending.mode;
    bool reconstructed = false;
    if (reconstructs && depth_ready && state_guard.IsActive() && pending.inputs.device == runtime_device) {
      if (!logged_immediate_dispatch.exchange(true, std::memory_order_relaxed)) {
        logging::Info("executing temporal dispatch on immediate context mode=", static_cast<uint32_t>(pending.mode),
                      " frame=", pending.inputs.frame_token, " submission_frame=", state::CurrentFrameToken());
      }
      pending.inputs.cmd_list = command_list;
      reconstructed = DispatchValidatedLocked(pending.inputs);
      rendering::d3d12_timing::CpuSubmission::EndPart("compute state restore, commit");
    } else if (reconstructs) {
      logging::Warn("rejected scene reconstruction submission because its device or saved state is unavailable");
      submission_failure_pending.store(true, std::memory_order_release);
    }
    if (reconstructed) continue;
    if (reconstructs) {
      state::frame_state.reconstruction_scheduled = false;
    }
    if (pending.fill_only) {
      // The next reconstruction must not continue history across this scene.
      analytical::InvalidateHistory("a scene was filled without reconstruction");
      dlss::InvalidateHistory();
      fsr::InvalidateHistory();
    }
    if (pending.post_output != nullptr) {
      pending.post_output->CopyNative(context);
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
    submission.recording_lists.erase(native_context);
    std::vector<ID3D11CommandList*> orphaned_lists = {native_command_list};
    // Prefixes from cuts were finished directly on the native context, so
    // ReShade never reports their destruction. Remove the whole chain of cuts
    // that leads to this list.
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
    if (abandoned_generation == state::frame_state.temporal_generation
        && state::frame_state.reconstruction_scheduled) {
      state::frame_state.reconstruction_scheduled = false;
      logging::Info("the command list holding a scene's reconstruction was abandoned");
    }
  }
}

// Decides what the scene gets where its post-processing starts, and schedules
// it in the list the engine has just begun. Called on the build thread with
// the execution lock held.
//   Reconstruct: the record is complete.
//   Fill only: upscaling without motion vectors or a camera. The job scales the
//     native image into the output, so the output-size passes read this
//     frame's pixels, and the next reconstruction starts fresh history.
//   Nothing: there is no usable scene image, or the list cannot be cut. At
//     native size the engine's image stands. While upscaling the engine's
//     render-size path does not present a correct image, so these frames must
//     stay rare. If complete records keep going unscheduled, the check at
//     Present ends upscaling.
inline void ScheduleSceneLocked(reshade::api::command_list* cmd_list) {
  if (!state::IsEnabled()) return;
  const auto scope = engine::views::GetScope();
  // Retained here until the job has taken its own references.
  const auto images = engine::render_targets::ReadFrameImages();
  ValidatedFrameInputs inputs = {};
  const auto record = input_capture::BuildFrameInputsLocked(cmd_list, images, &inputs);
  const bool fill_only = record == input_capture::Record::SCENE_ONLY;
  // At native size the engine's image already stands in for a missing reconstruction.
  if (record == input_capture::Record::UNUSABLE || (fill_only && inputs.HasNativeOutputSize())) return;
  if (!BindRuntimeDevice(inputs.device)) {
    logging::Warn("rejecting temporal dispatch from an unavailable or different D3D11 device");
    return;
  }
  if (!submission_bridge_installed
      || (state::GetTemporalMode() == state::TemporalMode::NVIDIA_DLSS && !dlss::runtime_ready.load(std::memory_order_acquire))) return;

  // The engine records with deferred contexts; the job runs when the list is executed.
  auto* native_context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  const auto fail_submission = [] {
    logging::Warn("failed to schedule temporal dispatch");
    submission_failure_pending.store(true, std::memory_order_release);
    state::frame_state.reconstruction_scheduled = true;
  };
  if (native_context == nullptr || native_context->GetType() != D3D11_DEVICE_CONTEXT_DEFERRED
      || submission_bridge_device.load(std::memory_order_acquire) != inputs.device) {
    fail_submission();
    return;
  }
  {
    const std::unique_lock lock(submission.mutex);
    const auto queries = submission.active_queries.find(native_context);
    // A list with an open query cannot be split.
    if (submission.pending_dispatches.size() >= MAX_PENDING_DISPATCHES
        || queries == submission.active_queries.end() || !queries->second.empty()) return;
  }

  std::shared_ptr<output_size::Output> post_output;
  std::shared_ptr<output_size::OutputProducer> output_producer;
  if (!inputs.HasNativeOutputSize()) {
    post_output = output_size::GetActive(cmd_list);
    if (post_output != nullptr && !post_output->MatchesInputs(inputs)) {
      resolution::Fail("scene reconstruction inputs changed after output recording began");
      return;
    }
    if (post_output == nullptr) {
      try {
        post_output = output_size::AcquireOutput(inputs);
      } catch (const std::exception& error) {
        logging::Warn(error.what());
        resolution::Fail("scaled post-processing resources unavailable");
      }
    }
    if (post_output == nullptr) {
      logging::Warn("scaled output route unavailable scene=", scope.owner.scene, " generation=", scope.owner.generation,
                    " render=", inputs.render_width, "x", inputs.render_height,
                    " output=", inputs.output.width, "x", inputs.output.height,
                    " color_format=", static_cast<uint32_t>(inputs.color_format));
      resolution::Fail("scaled reconstruction has no compatible output route");
      return;
    }
    inputs.output = post_output->target;
    output_size::Recording route;
    output_size::GetActive(cmd_list, &route, true);
    output_producer = route.producer;
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
      .fill_only = fill_only,
  };
  // At native size the output is the scene image itself, which color_srv
  // already keeps alive. A separate output needs its own reference until the
  // job runs.
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
  // The output-size route redirects the rest of this list's post-processing
  // into this output, so make it the active output now.
  if (post_output != nullptr) {
    const std::unique_lock lock(output_size::state.mutex);
    output_size::Recording route = {.output = post_output, .producer = output_producer, .frame = inputs.frame_token, .generation = output_size::state.generation, .owner = scope.owner};
    output_producer->reconstruction_recorded = true;
    output_size::state.active = post_output;
    output_size::state.producer = output_producer;
    output_size::state.frame = inputs.frame_token;
    output_size::state.active_owner = scope.owner;
    if (const auto pooled = std::ranges::find(output_size::state.pool, post_output, &output_size::Recording::output); pooled != output_size::state.pool.end()) {
      *pooled = route;
    }
    const auto existing = std::ranges::find_if(output_size::state.sequences, [&](const output_size::Recording& candidate) {
      return candidate.owner.scene == scope.owner.scene && candidate.owner.generation == scope.owner.generation;
    });
    if (existing == output_size::state.sequences.end()) {
      output_size::state.sequences.push_back(route);
    } else {
      *existing = route;
    }
    if (auto* recording = cmd_list->get_private_data<output_size::Recording>(); recording != nullptr) {
      output_size::RetainOutput(cmd_list, recording->output);
      route.retained_outputs = std::move(recording->retained_outputs);
      *recording = std::move(route);
    }
  }
  if (fill_only) return;
  state::frame_state.reconstruction_scheduled = true;
  if (!logged_immediate_dispatch.load(std::memory_order_relaxed)) {
    logging::Info("scheduled temporal dispatch frame=", inputs.frame_token);
  }
}

}  // namespace mgsv::reconstruction::coordinator
