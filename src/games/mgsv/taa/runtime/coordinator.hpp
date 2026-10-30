#pragma once

/* Method-neutral temporal candidate validation, dispatch, and completion. */

#include <windows.h>

#include <intrin.h>
#include <tlhelp32.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <d3d11_1.h>
#include <detours.h>
#include <wrl/client.h>

#include "../../rendering/d3d11_compute_state.hpp"
#include "../analytical/runtime.hpp"
#include "../dlss/runtime.hpp"
#include "../fsr3/runtime.hpp"
#include "./camera_state.hpp"
#include "./input_capture.hpp"
#include "./logging.hpp"
#include "./state.hpp"

namespace mgsv::reconstruction::coordinator {

inline auto& execution_lock = state::execution_lock;
inline reshade::api::device* runtime_device = nullptr;

using BeginQuery = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Asynchronous*);
using EndQuery = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Asynchronous*);

struct PendingTemporalDispatch {
  reshade::api::command_list* deferred_context = nullptr;
  Microsoft::WRL::ComPtr<ID3D11CommandList> prefix;
  Microsoft::WRL::ComPtr<ID3D11CommandList> post;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> color_srv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> velocity_srv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth_srv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> object_velocity_srv;
  ValidatedFrameInputs inputs;
  uint64_t temporal_generation = 0u;
  state::TemporalMode mode = state::TemporalMode::OFF;
  bool dispatch_consumed = false;
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
  if (runtime_device == nullptr) runtime_device = device;
  return runtime_device == device;
}

inline bool IsForeignDevice(reshade::api::device* device) {
  return runtime_device != nullptr && runtime_device != device;
}

inline void ResetTemporalStateWithPublicationLocked(const char* reason) {
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
  input_capture::Destroy(device);
  analytical::Destroy(device);
  dlss::Destroy(device);
  fsr3::Destroy();
  submission_failure_pending.store(false, std::memory_order_release);
  runtime_device = nullptr;
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
  const auto previous_compute_state = mgsv::rendering::d3d11_compute_state::Capture(inputs.cmd_list);
  inputs.cmd_list->barrier(
      inputs.velocity_resource,
      reshade::api::resource_usage::render_target,
      reshade::api::resource_usage::shader_resource);

  MethodOutput output = {};
  bool succeeded = false;
  switch (state::GetTemporalMode()) {
    case state::TemporalMode::ANALYTICAL_TAA:
      succeeded = analytical::Dispatch(inputs, output);
      break;
    case state::TemporalMode::AMD_FSR3:
      succeeded = fsr3::Dispatch(inputs, output);
      break;
    case state::TemporalMode::NVIDIA_DLSS:
      succeeded = dlss::Dispatch(inputs, output);
      break;
    case state::TemporalMode::OFF:
      break;
  }
  if (succeeded && output.resource.handle != 0u) {
    inputs.cmd_list->barrier(
        output.resource,
        reshade::api::resource_usage::unordered_access,
        reshade::api::resource_usage::copy_source);
    inputs.cmd_list->barrier(
        inputs.color_resource,
        reshade::api::resource_usage::shader_resource,
        reshade::api::resource_usage::copy_dest);
    inputs.cmd_list->copy_resource(output.resource, inputs.color_resource);
    inputs.cmd_list->barrier(
        inputs.color_resource,
        reshade::api::resource_usage::copy_dest,
        reshade::api::resource_usage::shader_resource);
    inputs.cmd_list->barrier(output.resource, reshade::api::resource_usage::copy_source, output.final_usage);
  } else {
    succeeded = false;
  }
  inputs.cmd_list->barrier(
      inputs.velocity_resource,
      reshade::api::resource_usage::shader_resource,
      reshade::api::resource_usage::render_target);
  mgsv::rendering::d3d11_compute_state::Restore(inputs.cmd_list, previous_compute_state);
  if (!succeeded) return false;

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
    if (found != submission.active_queries.end()) found->second.insert(asynchronous);
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
    const std::unique_lock lock(submission.mutex);
    submission.active_queries[recording_context.Get()].clear();
    const auto recorded = std::find_if(
        submission.pending_dispatches.begin(),
        submission.pending_dispatches.end(),
        [secondary_command_list](const PendingTemporalDispatch& candidate) {
          return candidate.deferred_context == secondary_command_list && candidate.post == nullptr;
        });
    if (recorded != submission.pending_dispatches.end()) {
      recorded->deferred_context = nullptr;
      recorded->post = reinterpret_cast<ID3D11CommandList*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
      if (!logged_immediate_dispatch.load(std::memory_order_relaxed)) {
        logging::Info("mapped temporal post command list frame=", recorded->inputs.frame_token);
      }
    }
    return;
  }

  auto* post = reinterpret_cast<ID3D11CommandList*>(secondary_command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  PendingTemporalDispatch pending = {};
  bool dispatch_pending = false;
  {
    const std::unique_lock lock(submission.mutex);
    const auto found = std::find_if(
        submission.pending_dispatches.begin(),
        submission.pending_dispatches.end(),
        [post](const PendingTemporalDispatch& candidate) { return candidate.post.Get() == post; });
    if (found == submission.pending_dispatches.end()) return;
    pending = *found;
    dispatch_pending = !found->dispatch_consumed;
    found->dispatch_consumed = true;
  }

  auto* context = reinterpret_cast<ID3D11DeviceContext*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (context == nullptr) return;
  if (context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) {
    context->ExecuteCommandList(pending.prefix.Get(), TRUE);
    if (dispatch_pending && state::GetTemporalMode() == pending.mode) {
      ExecutionGuard execution_guard;
      if (pending.temporal_generation != state::frame_state.temporal_generation) return;
      state::frame_state.reconstruction_scheduled = false;
      logging::Warn("temporal post list was merged into another deferred context");
      submission_failure_pending.store(true, std::memory_order_release);
    }
    return;
  }

  ExecutionGuard execution_guard;
  ImmediateContextStateGuard state_guard(context);
  if (!state_guard.IsActive()) {
    context->ExecuteCommandList(pending.prefix.Get(), TRUE);
    if (dispatch_pending && state::GetTemporalMode() == pending.mode
        && pending.temporal_generation == state::frame_state.temporal_generation) {
      state::frame_state.reconstruction_scheduled = false;
      logging::Warn("unable to preserve immediate D3D11 state for temporal submission");
      submission_failure_pending.store(true, std::memory_order_release);
    }
    return;
  }

  context->ExecuteCommandList(pending.prefix.Get(), FALSE);
  // Prefix execution is unconditional: mode changes cancel reconstruction, never original game work.
  if (pending.temporal_generation != state::frame_state.temporal_generation
      || state::GetTemporalMode() != pending.mode) return;
  if (!dispatch_pending) {
    logging::Warn("repeated execution of a temporal command list is unsupported");
    submission_failure_pending.store(true, std::memory_order_release);
    return;
  }

  const uint64_t current_frame = state::CurrentFrameToken();
  const bool current_dispatch = pending.inputs.device == runtime_device
                                && pending.inputs.sample_index == state::CurrentSampleIndex();
  if (!current_dispatch) {
    state::frame_state.reconstruction_scheduled = false;
    logging::Warn("rejecting temporal execution pending_frame=", pending.inputs.frame_token,
                  " submission_frame=", current_frame,
                  " pending_sample=", pending.inputs.sample_index,
                  " current_sample=", state::CurrentSampleIndex(),
                  " device_match=", logging::Bool{pending.inputs.device == runtime_device});
    submission_failure_pending.store(true, std::memory_order_release);
    return;
  }

  if (!logged_immediate_dispatch.exchange(true, std::memory_order_relaxed)) {
    logging::Info("executing temporal dispatch on immediate context mode=", static_cast<uint32_t>(pending.mode),
                  " frame=", pending.inputs.frame_token,
                  " submission_frame=", current_frame);
  }
  pending.inputs.cmd_list = command_list;
  const bool completed = DispatchValidatedLocked(pending.inputs);
  if (!completed) {
    state::frame_state.reconstruction_scheduled = false;
  } else if (pending.inputs.frame_token != current_frame) {
    state::frame_state.reconstruction_completed = false;
  }
}

inline void OnDestroyCommandList(reshade::api::command_list* command_list) {
  const auto* device = submission_bridge_device.load(std::memory_order_acquire);
  if (device == nullptr || command_list == nullptr || command_list->get_device() != device) return;
  auto* native_command_list = reinterpret_cast<ID3D11CommandList*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  auto* native_context = reinterpret_cast<ID3D11DeviceContext*>(command_list->get_native());     // NOLINT(performance-no-int-to-ptr)
  bool abandoned = false;
  uint64_t abandoned_generation = 0u;
  {
    const std::unique_lock lock(submission.mutex);
    submission.active_queries.erase(native_context);
    std::erase_if(submission.pending_dispatches, [&](const PendingTemporalDispatch& pending) {
      const bool matches = pending.deferred_context == command_list || pending.post.Get() == native_command_list;
      if (matches && !pending.dispatch_consumed) {
        abandoned = true;
        abandoned_generation = pending.temporal_generation;
      }
      return matches;
    });
  }
  if (abandoned && state::IsEnabled()) {
    ExecutionGuard execution_guard;
    if (abandoned_generation == state::frame_state.temporal_generation
        && state::frame_state.reconstruction_scheduled) {
      state::frame_state.reconstruction_scheduled = false;
      logging::Info("temporal command list abandoned; retrying a later candidate");
    }
  }
}

inline void MaybeRunLocked(
    reshade::api::command_list* cmd_list,
    const descriptor_tracker::CommandListData& command_data,
    const char* insertion_name) {
  if (!state::IsEnabled()
      || state::frame_state.reconstruction_scheduled
      || state::frame_state.reconstruction_completed) {
    return;
  }

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

  auto* native_context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
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
      if (submission.pending_dispatches.size() >= 64u) return;
      const auto queries = submission.active_queries.find(native_context);
      // Existing recordings may contain an unseen Begin. Wait for a natural
      // successful FinishCommandList or a fresh context before splitting them.
      if (queries == submission.active_queries.end() || !queries->second.empty()) return;
      if (std::any_of(
              submission.pending_dispatches.begin(), submission.pending_dispatches.end(),
              [&inputs](const PendingTemporalDispatch& candidate) {
                return candidate.deferred_context == inputs.cmd_list && candidate.post == nullptr;
              })) return;
    }
    PendingTemporalDispatch pending = {
        .deferred_context = inputs.cmd_list,
        .color_srv = reinterpret_cast<ID3D11ShaderResourceView*>(inputs.color_srv.handle),                      // NOLINT(performance-no-int-to-ptr)
        .velocity_srv = reinterpret_cast<ID3D11ShaderResourceView*>(inputs.velocity_srv.handle),                // NOLINT(performance-no-int-to-ptr)
        .depth_srv = reinterpret_cast<ID3D11ShaderResourceView*>(inputs.depth_srv.handle),                      // NOLINT(performance-no-int-to-ptr)
        .object_velocity_srv = reinterpret_cast<ID3D11ShaderResourceView*>(inputs.object_velocity_srv.handle),  // NOLINT(performance-no-int-to-ptr)
        .inputs = inputs,
        .temporal_generation = state::frame_state.temporal_generation,
        .mode = state::GetTemporalMode(),
    };
    ID3D11CommandList* prefix = nullptr;
    const HRESULT finish_result = native_context->FinishCommandList(TRUE, &prefix);
    if (FAILED(finish_result) || prefix == nullptr) {
      if (finish_result != DXGI_ERROR_INVALID_CALL) {
        fail_submission();
      }
      return;
    }
    pending.prefix.Attach(prefix);
    const std::unique_lock lock(submission.mutex);
    submission.pending_dispatches.emplace_back(std::move(pending));
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
  DispatchValidatedLocked(inputs);
}

}  // namespace mgsv::reconstruction::coordinator
