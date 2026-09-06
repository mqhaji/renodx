#pragma once

/* Method-neutral temporal candidate validation, dispatch, and completion. */

#include <windows.h>

#include <intrin.h>
#include <tlhelp32.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <d3d11_1.h>
#include <detours.h>
#include <wrl/client.h>

#include "../analytical/runtime.hpp"
#include "../dlss/runtime.hpp"
#include "../fsr3/runtime.hpp"
#include "./camera_state.hpp"
#include "./d3d11_compute_state.hpp"
#include "./descriptor_tracker.hpp"
#include "./input_capture.hpp"
#include "./logging.hpp"
#include "./state.hpp"

namespace taa::coordinator {

inline std::atomic_flag execution_lock = ATOMIC_FLAG_INIT;
inline reshade::api::device* runtime_device = nullptr;

using BeginQuery = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Asynchronous*);
using EndQuery = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Asynchronous*);

struct DeferredDlssDispatch {
  reshade::api::command_list* deferred_context = nullptr;
  Microsoft::WRL::ComPtr<ID3D11CommandList> prefix;
  Microsoft::WRL::ComPtr<ID3D11CommandList> post;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> color_srv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> velocity_srv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth_srv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> object_velocity_srv;
  ValidatedFrameInputs inputs;
  uint64_t temporal_generation = 0u;
  bool dispatch_consumed = false;
};

inline BeginQuery begin_query = nullptr;
inline BeginQuery deferred_begin_query = nullptr;
inline EndQuery end_query = nullptr;
inline EndQuery deferred_end_query = nullptr;
inline bool deferred_dlss_bridge_installed = false;
inline std::atomic<reshade::api::device*> dlss_bridge_device = nullptr;
inline std::atomic<uint32_t> execution_hook_calls_in_flight = 0u;
inline std::atomic<bool> logged_deferred_dlss_executed = false;
inline std::mutex deferred_dlss_mutex;
inline std::vector<DeferredDlssDispatch> deferred_dlss_dispatches;
inline std::unordered_map<ID3D11DeviceContext*, std::unordered_set<ID3D11Asynchronous*>> active_queries;

inline void UninstallDeferredDlssBridge();

inline bool EnlistProcessThreads(std::vector<HANDLE>& handles) {
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
        handles.push_back(thread);
      }
    }
    has_entry = Thread32Next(snapshot, &entry);
  }
  CloseHandle(snapshot);
  // DetourUpdateThread suspends immediately. Finish enumeration/allocation
  // before suspending threads that may own allocator or loader locks.
  if (DetourUpdateThread(GetCurrentThread()) != NO_ERROR) return false;
  for (HANDLE thread : handles) {
    if (DetourUpdateThread(thread) != NO_ERROR) return false;
  }
  return true;
}

inline void CloseThreadHandles(std::vector<HANDLE>& handles) {
  for (HANDLE thread : handles) CloseHandle(thread);
  handles.clear();
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
  const auto previous_compute_state = d3d11_compute_state::Capture(
      inputs.cmd_list,
      state::GetTemporalMode() == state::TemporalMode::AMD_FSR3
          && state::runtime_fsr_legacy_compute_state.load(std::memory_order_acquire));
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
  d3d11_compute_state::Restore(inputs.cmd_list, previous_compute_state);
  if (!succeeded) return false;

  if (!camera_state::Commit(inputs.camera)) {
    ResetTemporalState("native camera matrix commit failed");
    logging::Warn("temporal native camera matrix commit failed");
    state::frame_state.reconstruction_completed = true;
    return true;
  }
  state::CommitTemporalFrame();
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
    const std::unique_lock lock(deferred_dlss_mutex);
    const auto found = active_queries.find(context);
    if (found != active_queries.end()) found->second.insert(asynchronous);
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
  const std::unique_lock lock(deferred_dlss_mutex);
  const auto found = active_queries.find(context);
  if (found != active_queries.end()) {
    found->second.erase(asynchronous);
  }
}

inline bool InstallDeferredDlssBridge(ID3D11DeviceContext* immediate_context) {
  if (deferred_dlss_bridge_installed) return true;
  if (immediate_context == nullptr) return false;

  constexpr size_t begin_query_vtable_index = 27u;
  constexpr size_t end_query_vtable_index = 28u;
  auto** vtable = *reinterpret_cast<void***>(immediate_context);  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11Device> device;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> deferred_context;
  immediate_context->GetDevice(&device);
  if (device == nullptr || FAILED(device->CreateDeferredContext(0u, &deferred_context))) return false;
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
  if (!EnlistProcessThreads(thread_handles)
      || DetourAttach(reinterpret_cast<void**>(&begin_query), reinterpret_cast<void*>(HookBeginQuery)) != NO_ERROR
      || (deferred_begin_query != nullptr
        && DetourAttach(reinterpret_cast<void**>(&deferred_begin_query), reinterpret_cast<void*>(HookBeginQuery)) != NO_ERROR)
      || DetourAttach(reinterpret_cast<void**>(&end_query), reinterpret_cast<void*>(HookEndQuery)) != NO_ERROR
      || (deferred_end_query != nullptr
        && DetourAttach(reinterpret_cast<void**>(&deferred_end_query), reinterpret_cast<void*>(HookEndQuery)) != NO_ERROR)
      || DetourTransactionCommit() != NO_ERROR) {
    DetourTransactionAbort();
    CloseThreadHandles(thread_handles);
    begin_query = nullptr;
    deferred_begin_query = nullptr;
    end_query = nullptr;
    deferred_end_query = nullptr;
    return false;
  }
  CloseThreadHandles(thread_handles);
  deferred_dlss_bridge_installed = true;
  logging::Info("installed D3D11 deferred DLSS execution bridge");
  return true;
}

inline void UninstallDeferredDlssBridge() {
  if (!deferred_dlss_bridge_installed) return;
  if (DetourTransactionBegin() != NO_ERROR) return;
  std::vector<HANDLE> thread_handles;
  if (!EnlistProcessThreads(thread_handles)
      || DetourDetach(reinterpret_cast<void**>(&begin_query), reinterpret_cast<void*>(HookBeginQuery)) != NO_ERROR
      || (deferred_begin_query != nullptr
        && DetourDetach(reinterpret_cast<void**>(&deferred_begin_query), reinterpret_cast<void*>(HookBeginQuery)) != NO_ERROR)
      || DetourDetach(reinterpret_cast<void**>(&end_query), reinterpret_cast<void*>(HookEndQuery)) != NO_ERROR
      || (deferred_end_query != nullptr
        && DetourDetach(reinterpret_cast<void**>(&deferred_end_query), reinterpret_cast<void*>(HookEndQuery)) != NO_ERROR)
      || DetourTransactionCommit() != NO_ERROR) {
    DetourTransactionAbort();
    CloseThreadHandles(thread_handles);
    logging::Warn("failed to uninstall D3D11 deferred DLSS execution bridge");
    return;
  }
  CloseThreadHandles(thread_handles);
  while (execution_hook_calls_in_flight.load(std::memory_order_acquire) != 0u) {
    _mm_pause();
  }
  dlss_bridge_device.store(nullptr, std::memory_order_release);
  std::vector<DeferredDlssDispatch> retired_dispatches;
  {
    const std::unique_lock lock(deferred_dlss_mutex);
    retired_dispatches.swap(deferred_dlss_dispatches);
    active_queries.clear();
  }
  deferred_dlss_bridge_installed = false;
  begin_query = nullptr;
  deferred_begin_query = nullptr;
  end_query = nullptr;
  deferred_end_query = nullptr;
  logging::Info("uninstalled D3D11 deferred DLSS execution bridge");
}

inline bool EnsureDeferredDlssBridge(reshade::api::device* device) {
  if (deferred_dlss_bridge_installed) return dlss_bridge_device.load(std::memory_order_acquire) == device;
  if (device == nullptr || device->get_api() != reshade::api::device_api::d3d11) return false;
  auto* native_device = reinterpret_cast<ID3D11Device*>(device->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (native_device == nullptr) return false;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;
  native_device->GetImmediateContext(&immediate_context);
  if (!InstallDeferredDlssBridge(immediate_context.Get())) return false;
  dlss_bridge_device.store(device, std::memory_order_release);
  return true;
}

inline void OnInitCommandList(reshade::api::command_list* command_list) {
  const auto* device = dlss_bridge_device.load(std::memory_order_acquire);
  if (device == nullptr || command_list == nullptr || command_list->get_device() != device) return;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  auto* native = reinterpret_cast<IUnknown*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (native == nullptr || FAILED(native->QueryInterface(IID_PPV_ARGS(&context)))
      || context->GetType() != D3D11_DEVICE_CONTEXT_DEFERRED) return;
  const std::unique_lock lock(deferred_dlss_mutex);
  active_queries.try_emplace(context.Get());
}

enum class DeferredScheduleResult : uint8_t {
  SCHEDULED,
  RETRYABLE_SKIP,
  FATAL,
};

inline DeferredScheduleResult ScheduleDeferredDlssLocked(ValidatedFrameInputs& inputs) {
  auto* deferred_context = reinterpret_cast<ID3D11DeviceContext*>(inputs.cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  auto* native_device = reinterpret_cast<ID3D11Device*>(inputs.device->get_native());              // NOLINT(performance-no-int-to-ptr)
  if (deferred_context == nullptr
      || deferred_context->GetType() != D3D11_DEVICE_CONTEXT_DEFERRED
      || native_device == nullptr) {
    return DeferredScheduleResult::FATAL;
  }

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;
  native_device->GetImmediateContext(&immediate_context);
  if (!deferred_dlss_bridge_installed || immediate_context == nullptr) return DeferredScheduleResult::FATAL;

  {
    const std::unique_lock lock(deferred_dlss_mutex);
    if (deferred_dlss_dispatches.size() >= 64u) return DeferredScheduleResult::RETRYABLE_SKIP;
    const auto queries = active_queries.find(deferred_context);
    // A context already recording when hooks were installed may contain an
    // unseen Begin. Absence means unknown, NOT query-free. Wait for natural
    // successful FinishCommandList (or a fresh context) before splitting it.
    if (queries == active_queries.end() || !queries->second.empty()) {
      return DeferredScheduleResult::RETRYABLE_SKIP;
    }
    const bool duplicate = std::any_of(
        deferred_dlss_dispatches.begin(),
        deferred_dlss_dispatches.end(),
        [&inputs](const DeferredDlssDispatch& candidate) {
          return candidate.deferred_context == inputs.cmd_list && candidate.post == nullptr;
        });
    if (duplicate) return DeferredScheduleResult::RETRYABLE_SKIP;
  }

  DeferredDlssDispatch pending = {
      .deferred_context = inputs.cmd_list,
      .color_srv = reinterpret_cast<ID3D11ShaderResourceView*>(inputs.color_srv.handle),              // NOLINT(performance-no-int-to-ptr)
      .velocity_srv = reinterpret_cast<ID3D11ShaderResourceView*>(inputs.velocity_srv.handle),        // NOLINT(performance-no-int-to-ptr)
      .depth_srv = reinterpret_cast<ID3D11ShaderResourceView*>(inputs.depth_srv.handle),              // NOLINT(performance-no-int-to-ptr)
      .object_velocity_srv = reinterpret_cast<ID3D11ShaderResourceView*>(inputs.object_velocity_srv.handle),  // NOLINT(performance-no-int-to-ptr)
      .inputs = inputs,
        .temporal_generation = state::frame_state.temporal_generation,
  };
  ID3D11CommandList* prefix = nullptr;
  const HRESULT finish_result = deferred_context->FinishCommandList(TRUE, &prefix);
  if (FAILED(finish_result) || prefix == nullptr) {
    if (finish_result == DXGI_ERROR_INVALID_CALL) return DeferredScheduleResult::RETRYABLE_SKIP;
    return DeferredScheduleResult::FATAL;
  }
  pending.prefix.Attach(prefix);

  const std::unique_lock lock(deferred_dlss_mutex);
  deferred_dlss_dispatches.emplace_back(std::move(pending));
  state::frame_state.reconstruction_scheduled = true;
  if (!logged_deferred_dlss_executed.load(std::memory_order_relaxed)) {
    logging::Info("scheduled deferred NVIDIA DLSS dispatch frame=", inputs.frame_token);
  }
  return DeferredScheduleResult::SCHEDULED;
}

inline void OnExecuteSecondaryCommandList(
    reshade::api::command_list* command_list,
    reshade::api::command_list* secondary_command_list) {
  const auto* device = dlss_bridge_device.load(std::memory_order_acquire);
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
    const std::unique_lock lock(deferred_dlss_mutex);
    active_queries[recording_context.Get()].clear();
    const auto recorded = std::find_if(
        deferred_dlss_dispatches.begin(),
        deferred_dlss_dispatches.end(),
        [secondary_command_list](const DeferredDlssDispatch& candidate) {
          return candidate.deferred_context == secondary_command_list && candidate.post == nullptr;
        });
    if (recorded != deferred_dlss_dispatches.end()) {
      recorded->deferred_context = nullptr;
      recorded->post = reinterpret_cast<ID3D11CommandList*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
      if (!logged_deferred_dlss_executed.load(std::memory_order_relaxed)) {
        logging::Info("mapped deferred NVIDIA DLSS post command list frame=", recorded->inputs.frame_token);
      }
    }
    return;
  }

  auto* post = reinterpret_cast<ID3D11CommandList*>(secondary_command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  DeferredDlssDispatch pending = {};
  bool dispatch_dlss = false;
  {
    const std::unique_lock lock(deferred_dlss_mutex);
    const auto found = std::find_if(
        deferred_dlss_dispatches.begin(),
        deferred_dlss_dispatches.end(),
        [post](const DeferredDlssDispatch& candidate) { return candidate.post.Get() == post; });
    if (found == deferred_dlss_dispatches.end()) return;
    pending = *found;
    dispatch_dlss = !found->dispatch_consumed;
    found->dispatch_consumed = true;
  }

  auto* context = reinterpret_cast<ID3D11DeviceContext*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (context == nullptr) return;
  if (context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) {
    context->ExecuteCommandList(pending.prefix.Get(), TRUE);
    if (dispatch_dlss && state::GetTemporalMode() == state::TemporalMode::NVIDIA_DLSS) {
      ExecutionGuard execution_guard;
      state::frame_state.reconstruction_scheduled = false;
      logging::Warn("deferred NVIDIA DLSS post list was merged into another deferred context");
      dlss::QueueFatalFailure();
    }
    return;
  }

  ExecutionGuard execution_guard;
  ImmediateContextStateGuard state_guard(context);
  if (!state_guard.IsActive()) {
    context->ExecuteCommandList(pending.prefix.Get(), TRUE);
    if (dispatch_dlss && state::GetTemporalMode() == state::TemporalMode::NVIDIA_DLSS) {
      state::frame_state.reconstruction_scheduled = false;
      logging::Warn("unable to preserve immediate D3D11 state for deferred NVIDIA DLSS");
      dlss::QueueFatalFailure();
    }
    return;
  }

  context->ExecuteCommandList(pending.prefix.Get(), FALSE);
  if (!dispatch_dlss) {
    if (state::GetTemporalMode() == state::TemporalMode::NVIDIA_DLSS) {
      logging::Warn("repeated execution of a deferred NVIDIA DLSS command list is unsupported");
      dlss::QueueFatalFailure();
    }
    return;
  }
  if (pending.temporal_generation != state::frame_state.temporal_generation) return;
  if (state::GetTemporalMode() != state::TemporalMode::NVIDIA_DLSS) return;

  const uint64_t current_frame = state::CurrentFrameToken();
  const bool current_dispatch = pending.inputs.device == runtime_device
                                && pending.inputs.sample_index == state::CurrentSampleIndex();
  if (!current_dispatch) {
    state::frame_state.reconstruction_scheduled = false;
    logging::Warn("rejecting deferred NVIDIA DLSS execution pending_frame=", pending.inputs.frame_token,
                  " submission_frame=", current_frame,
                  " pending_sample=", pending.inputs.sample_index,
                  " current_sample=", state::CurrentSampleIndex(),
                  " device_match=", logging::Bool{pending.inputs.device == runtime_device});
    dlss::QueueFatalFailure();
    return;
  }

  if (!logged_deferred_dlss_executed.exchange(true, std::memory_order_relaxed)) {
    logging::Info("executing deferred NVIDIA DLSS dispatch on immediate context frame=",
                  pending.inputs.frame_token,
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
  const auto* device = dlss_bridge_device.load(std::memory_order_acquire);
  if (device == nullptr || command_list == nullptr || command_list->get_device() != device) return;
  auto* native_command_list = reinterpret_cast<ID3D11CommandList*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  auto* native_context = reinterpret_cast<ID3D11DeviceContext*>(command_list->get_native());      // NOLINT(performance-no-int-to-ptr)
  bool abandoned = false;
  uint64_t abandoned_generation = 0u;
  {
    const std::unique_lock lock(deferred_dlss_mutex);
    active_queries.erase(native_context);
    for (const auto& pending : deferred_dlss_dispatches) {
      if ((pending.deferred_context == command_list || pending.post.Get() == native_command_list)
          && !pending.dispatch_consumed) {
        abandoned = true;
        abandoned_generation = pending.temporal_generation;
      }
    }
    std::erase_if(deferred_dlss_dispatches, [command_list, native_command_list](const DeferredDlssDispatch& pending) {
      return pending.deferred_context == command_list || pending.post.Get() == native_command_list;
    });
  }
  if (abandoned && state::GetTemporalMode() == state::TemporalMode::NVIDIA_DLSS) {
    ExecutionGuard execution_guard;
    if (abandoned_generation == state::frame_state.temporal_generation
        && state::frame_state.reconstruction_scheduled) {
      state::frame_state.reconstruction_scheduled = false;
      logging::Info("deferred NVIDIA DLSS command list abandoned; retrying a later candidate");
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

  if (state::GetTemporalMode() == state::TemporalMode::NVIDIA_DLSS) {
    if (!dlss::runtime_ready.load(std::memory_order_acquire)) return;

    auto* native_context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
    if (native_context != nullptr && native_context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED) {
      const auto schedule_result = ScheduleDeferredDlssLocked(inputs);
      if (schedule_result == DeferredScheduleResult::FATAL) {
        logging::Warn("failed to schedule deferred NVIDIA DLSS dispatch");
        dlss::QueueFatalFailure();
        state::frame_state.reconstruction_scheduled = true;
      }
      return;
    }
    ImmediateContextStateGuard state_guard(native_context);
    if (!state_guard.IsActive()) {
      logging::Warn("unable to preserve immediate D3D11 state for NVIDIA DLSS");
      dlss::QueueFatalFailure();
      return;
    }
    DispatchValidatedLocked(inputs);
    return;
  }
  DispatchValidatedLocked(inputs);
}

}  // namespace taa::coordinator
