// SPDX-License-Identifier: MIT
#ifndef RENODX_GAMES_MGSV_RENDERING_SUBMISSION_HPP_
#define RENODX_GAMES_MGSV_RENDERING_SUBMISSION_HPP_

/* Runs added GPU work at a chosen point inside the game's command lists. The
 * engine records with D3D11 deferred contexts, so the work cannot run when it
 * is scheduled. Instead the list is cut: FinishCommandList ends it there (the
 * prefix), and the game keeps recording into a new list (the remainder,
 * `post`). When the game executes the remainder on the immediate context, the
 * prefixes run first, each followed by its job, in recorded order. Contact
 * shadows, the diffuse gather, reflections and temporal reconstruction
 * schedule their work here. */

#include <windows.h>

#include <intrin.h>
#include <tlhelp32.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <d3d11_1.h>
#include <detours.h>
#include <wrl/client.h>

#include <include/reshade.hpp>

#include "../engine/views.hpp"
#include "d3d12_timing.hpp"
#include "producer_records.hpp"

namespace mgsv::rendering::submission {

// Limits how many cuts can wait for execution, in case the game delays or
// drops a submission.
inline constexpr size_t MAX_PENDING_JOBS = 64u;
// A job that leaves room keeps this many slots free for the jobs recorded
// after it: contact shadows leave them for GI and the scene's reconstruction.
inline constexpr size_t RESERVED_JOBS = 8u;
// Reconstruction's Present installs the bridge for the temporal features. A
// feature it does not know sets this to have the bridge installed as well;
// contact shadows do.
inline std::atomic<bool> install_requested = false;

// Serializes scheduling and the execution of jobs with the work that changes
// what jobs read, such as reconstruction's method changes and its Present
// checks. Take it through ExecutionGuard.
inline std::atomic_flag execution_lock = ATOMIC_FLAG_INIT;

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

// How a job is run on the immediate context.
struct Execution {
  // The list's first execution. A replay runs the native commands again.
  bool first = false;
  // The immediate context's whole state was saved and is restored afterward.
  bool state_saved = false;
};

// Work cut into a recorded list. run and run_merged are called with the
// execution lock held.
struct Job {
  // Runs on the immediate context after the native commands before the cut,
  // every time the list is executed there.
  std::function<void(reshade::api::command_list* immediate, const Execution& execution)> run;
  // Runs when the list is executed into another deferred list instead, after
  // the native commands before the cut are replayed into it.
  std::function<void(reshade::api::command_list* deferred, bool first)> run_merged;
  // Runs, without the execution lock, when the list holding the job is
  // destroyed before it was ever executed.
  std::function<void()> on_abandoned;
  // Runs from CancelJobs with the execution lock held. A job that has it stays
  // scheduled and decides what it still does; a job without it is dropped.
  std::function<void()> cancel;
  // The CPU time of an execution that includes this job is measured in this stream.
  std::optional<d3d12_timing::Stream> timing;
  // The job is scheduled only while RESERVED_JOBS slots stay free after it.
  bool leaves_room = false;
};

enum class Result : uint8_t {
  SCHEDULED,
  // The list was the immediate context, so the job ran at once.
  RAN_AT_ONCE,
  // The list cannot be cut now: its context is not tracked yet, a query is
  // open on it, too many cuts wait, or the context refused to finish the list.
  BUSY,
  FAILED,
};

// The output-size redirection keeps its output images alive while a list that
// uses them waits to execute, through these two observers.
using ListFinished = void (*)(reshade::api::command_list* recording, ID3D11CommandList* finished);
using ListExecutedInto = void (*)(reshade::api::command_list* deferred, ID3D11CommandList* executed);

namespace internal {

using BeginQuery = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Asynchronous*);
using EndQuery = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Asynchronous*);

// One cut: the native list before it (`prefix`), the job, and the native list
// after it (`post`), set once the game finishes that list.
struct Pending {
  Job job;
  engine::views::Scope scope;
  reshade::api::command_list* recording = nullptr;
  Microsoft::WRL::ComPtr<ID3D11CommandList> prefix;
  Microsoft::WRL::ComPtr<ID3D11CommandList> post;
  // Set the first time the list is executed.
  bool executed = false;
};

// `mutex` protects these members. The query hook pointers below are not
// covered; they change only while the bridge is installed or removed.
struct State {
  std::shared_mutex mutex;
  std::vector<Pending> pending;
  // Queries begun but not yet ended on each tracked deferred context. A
  // context with an open query is not cut.
  std::unordered_map<ID3D11DeviceContext*, std::unordered_set<ID3D11Asynchronous*>> active_queries;
  // The ReShade command list of each deferred context above.
  std::unordered_map<ID3D11DeviceContext*, reshade::api::command_list*> recording_lists;
};
inline State state;

// The bridge: hooks on ID3D11DeviceContext::Begin and End (immediate and, when
// different, deferred) that track open queries, plus the command-list event
// handlers below.
inline BeginQuery begin_query = nullptr;
inline BeginQuery deferred_begin_query = nullptr;
inline EndQuery end_query = nullptr;
inline EndQuery deferred_end_query = nullptr;
inline bool installed = false;
inline std::atomic<reshade::api::device*> bridge_device = nullptr;
inline std::atomic<uint32_t> hook_calls_in_flight = 0u;
inline std::atomic<ListFinished> list_finished = nullptr;
inline std::atomic<ListExecutedInto> list_executed_into = nullptr;

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
struct HookCallGuard {
  HookCallGuard() { hook_calls_in_flight.fetch_add(1u, std::memory_order_acq_rel); }
  HookCallGuard(const HookCallGuard&) = delete;
  HookCallGuard& operator=(const HookCallGuard&) = delete;
  ~HookCallGuard() { hook_calls_in_flight.fetch_sub(1u, std::memory_order_acq_rel); }
};

// Saves the immediate context's whole state by swapping in an empty context
// state, and swaps the game's state back when destroyed. The prefixes and the
// jobs run in between without disturbing what the game had bound.
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
  const HookCallGuard hook_call_guard;
  {
    const std::unique_lock lock(state.mutex);
    const auto found = state.active_queries.find(context);
    if (found != state.active_queries.end()) {
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
  const HookCallGuard hook_call_guard;
  const auto end = context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED
                           && deferred_end_query != nullptr
                       ? deferred_end_query
                       : end_query;
  end(context, asynchronous);
  const std::unique_lock lock(state.mutex);
  const auto found = state.active_queries.find(context);
  if (found != state.active_queries.end()) {
    found->second.erase(asynchronous);
  }
}

// Called whenever a deferred context finishes a list, whether the game or a
// cut finished it. Links a waiting cut to its remainder.
inline void CompleteRecording(reshade::api::command_list* recording, ID3D11CommandList* finished) {
  if (const auto observer = list_finished.load(std::memory_order_acquire); observer != nullptr) {
    observer(recording, finished);
  }
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(recording->get_native());  // NOLINT(performance-no-int-to-ptr)
  lighting::temporal_inputs::Finish(context, finished);
  const std::unique_lock lock(state.mutex);
  state.active_queries[context].clear();
  // A context created before the bridge was installed is first seen here.
  state.recording_lists.insert_or_assign(context, recording);
  const auto pending = std::ranges::find_if(state.pending, [recording](const Pending& candidate) {
    return candidate.recording == recording && candidate.post == nullptr;
  });
  if (pending != state.pending.end()) {
    pending->recording = nullptr;
    pending->post = finished;
  }
}

}  // namespace internal

// The device the bridge serves; null while it is not installed.
inline reshade::api::device* Device() {
  return internal::bridge_device.load(std::memory_order_acquire);
}

inline void SetListObservers(ListFinished on_finished, ListExecutedInto on_executed_into) {
  internal::list_finished.store(on_finished, std::memory_order_release);
  internal::list_executed_into.store(on_executed_into, std::memory_order_release);
}

// Installs the query hooks once, for the first D3D11 device. Deferred contexts
// can use different Begin and End functions, so those are hooked as well. Call
// with the execution lock held.
inline bool Install(reshade::api::device* device) {
  if (internal::installed) return internal::bridge_device.load(std::memory_order_acquire) == device;
  if (device == nullptr || device->get_api() != reshade::api::device_api::d3d11) return false;
  auto* native_device = reinterpret_cast<ID3D11Device*>(device->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (native_device == nullptr) return false;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;
  native_device->GetImmediateContext(&immediate_context);
  if (immediate_context == nullptr) return false;

  constexpr size_t begin_query_vtable_index = 27u;
  constexpr size_t end_query_vtable_index = 28u;
  const auto* vtable = *reinterpret_cast<void***>(immediate_context.Get());  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11Device> context_device;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> deferred_context;
  immediate_context->GetDevice(&context_device);
  if (context_device == nullptr || FAILED(context_device->CreateDeferredContext(0u, &deferred_context))) return false;
  const auto* deferred_vtable = *reinterpret_cast<void***>(deferred_context.Get());  // NOLINT(performance-no-int-to-ptr)
  internal::begin_query = reinterpret_cast<internal::BeginQuery>(vtable[begin_query_vtable_index]);
  internal::end_query = reinterpret_cast<internal::EndQuery>(vtable[end_query_vtable_index]);
  if (vtable[begin_query_vtable_index] != deferred_vtable[begin_query_vtable_index]) {
    internal::deferred_begin_query = reinterpret_cast<internal::BeginQuery>(deferred_vtable[begin_query_vtable_index]);
  }
  if (vtable[end_query_vtable_index] != deferred_vtable[end_query_vtable_index]) {
    internal::deferred_end_query = reinterpret_cast<internal::EndQuery>(deferred_vtable[end_query_vtable_index]);
  }
  if (internal::begin_query == nullptr
      || internal::end_query == nullptr
      || DetourTransactionBegin() != NO_ERROR) {
    return false;
  }
  std::vector<HANDLE> thread_handles;
  if (!internal::EnlistProcessThreads(&thread_handles)
      || DetourAttach(reinterpret_cast<void**>(&internal::begin_query), reinterpret_cast<void*>(internal::HookBeginQuery)) != NO_ERROR
      || (internal::deferred_begin_query != nullptr
          && DetourAttach(reinterpret_cast<void**>(&internal::deferred_begin_query), reinterpret_cast<void*>(internal::HookBeginQuery)) != NO_ERROR)
      || DetourAttach(reinterpret_cast<void**>(&internal::end_query), reinterpret_cast<void*>(internal::HookEndQuery)) != NO_ERROR
      || (internal::deferred_end_query != nullptr
          && DetourAttach(reinterpret_cast<void**>(&internal::deferred_end_query), reinterpret_cast<void*>(internal::HookEndQuery)) != NO_ERROR)
      || DetourTransactionCommit() != NO_ERROR) {
    DetourTransactionAbort();
    internal::CloseThreadHandles(&thread_handles);
    internal::begin_query = nullptr;
    internal::deferred_begin_query = nullptr;
    internal::end_query = nullptr;
    internal::deferred_end_query = nullptr;
    return false;
  }
  internal::CloseThreadHandles(&thread_handles);
  internal::installed = true;
  reshade::log::message(reshade::log::level::info, "MgsvSubmission: installed the D3D11 submission bridge");
  internal::bridge_device.store(device, std::memory_order_release);
  return true;
}

// Removes the bridge and drops every pending job without running it.
inline void Uninstall() {
  if (!internal::installed) return;
  if (DetourTransactionBegin() != NO_ERROR) return;
  std::vector<HANDLE> thread_handles;
  if (!internal::EnlistProcessThreads(&thread_handles)
      || DetourDetach(reinterpret_cast<void**>(&internal::begin_query), reinterpret_cast<void*>(internal::HookBeginQuery)) != NO_ERROR
      || (internal::deferred_begin_query != nullptr
          && DetourDetach(reinterpret_cast<void**>(&internal::deferred_begin_query), reinterpret_cast<void*>(internal::HookBeginQuery)) != NO_ERROR)
      || DetourDetach(reinterpret_cast<void**>(&internal::end_query), reinterpret_cast<void*>(internal::HookEndQuery)) != NO_ERROR
      || (internal::deferred_end_query != nullptr
          && DetourDetach(reinterpret_cast<void**>(&internal::deferred_end_query), reinterpret_cast<void*>(internal::HookEndQuery)) != NO_ERROR)
      || DetourTransactionCommit() != NO_ERROR) {
    DetourTransactionAbort();
    internal::CloseThreadHandles(&thread_handles);
    reshade::log::message(reshade::log::level::warning, "MgsvSubmission: failed to uninstall the D3D11 submission bridge");
    return;
  }
  internal::CloseThreadHandles(&thread_handles);
  while (internal::hook_calls_in_flight.load(std::memory_order_acquire) != 0u) {
    _mm_pause();
  }
  internal::bridge_device.store(nullptr, std::memory_order_release);
  std::vector<internal::Pending> retired;
  {
    const std::unique_lock lock(internal::state.mutex);
    retired.swap(internal::state.pending);
    internal::state.active_queries.clear();
    internal::state.recording_lists.clear();
  }
  for (const auto& pending : retired) {
    lighting::temporal_inputs::Forget(pending.prefix.Get());
  }
  internal::installed = false;
  internal::begin_query = nullptr;
  internal::deferred_begin_query = nullptr;
  internal::end_query = nullptr;
  internal::deferred_end_query = nullptr;
  reshade::log::message(reshade::log::level::info, "MgsvSubmission: uninstalled the D3D11 submission bridge");
}

// Starts tracking each new deferred context; only tracked contexts are cut.
inline void OnInitCommandList(reshade::api::command_list* command_list) {
  const auto* device = Device();
  if (device == nullptr || command_list == nullptr || command_list->get_device() != device) return;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  auto* native = reinterpret_cast<IUnknown*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (native == nullptr || FAILED(native->QueryInterface(IID_PPV_ARGS(&context)))
      || context->GetType() != D3D11_DEVICE_CONTEXT_DEFERRED) return;
  const std::unique_lock lock(internal::state.mutex);
  internal::state.active_queries.try_emplace(context.Get());
  internal::state.recording_lists.insert_or_assign(context.Get(), command_list);
}

// Work scheduled from an engine hook knows only the native deferred context.
inline reshade::api::command_list* FindRecordingList(ID3D11DeviceContext* context) {
  const std::shared_lock lock(internal::state.mutex);
  const auto found = internal::state.recording_lists.find(context);
  return found != internal::state.recording_lists.end() ? found->second : nullptr;
}

// Whether a job can be cut into the list `context` records now: the context
// is tracked, has no open query and a slot is free.
inline bool CanCut(ID3D11DeviceContext* context, bool leaves_room) {
  const std::shared_lock lock(internal::state.mutex);
  const auto queries = internal::state.active_queries.find(context);
  const size_t limit = (leaves_room ? MAX_PENDING_JOBS - RESERVED_JOBS : MAX_PENDING_JOBS);
  return queries != internal::state.active_queries.end() && queries->second.empty() && internal::state.pending.size() < limit;
}

// Cuts `cmd_list` here so the job runs when the game executes the list. On the
// immediate context the job runs at once, as a first execution with the state
// not saved. Call with the execution lock held.
inline Result ScheduleLocked(reshade::api::command_list* cmd_list, Job job) {
  if (cmd_list == nullptr || !job.run) return Result::FAILED;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (context == nullptr) return Result::FAILED;
  if (context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE) {
    job.run(cmd_list, {.first = true});
    return Result::RAN_AT_ONCE;
  }
  if (Device() != cmd_list->get_device()) return Result::FAILED;
  if (!CanCut(context, job.leaves_room)) return Result::BUSY;
  Microsoft::WRL::ComPtr<ID3D11CommandList> prefix;
  const HRESULT finished = context->FinishCommandList(TRUE, &prefix);
  if (FAILED(finished) || prefix == nullptr) {
    return (finished == DXGI_ERROR_INVALID_CALL ? Result::BUSY : Result::FAILED);
  }
  // If this list already had a cut, this prefix is that cut's remainder.
  // Linking them keeps earlier jobs and later ones in recorded order.
  internal::CompleteRecording(cmd_list, prefix.Get());
  const std::unique_lock lock(internal::state.mutex);
  internal::state.pending.push_back({
      .job = std::move(job),
      .scope = engine::views::GetScope(),
      .recording = cmd_list,
      .prefix = std::move(prefix),
  });
  return Result::SCHEDULED;
}

// ScheduleLocked under its own execution lock. Returns false when the job was
// neither scheduled nor run.
inline bool Schedule(reshade::api::command_list* cmd_list, Job job) {
  const ExecutionGuard execution_guard;
  const auto result = ScheduleLocked(cmd_list, std::move(job));
  return result == Result::SCHEDULED || result == Result::RAN_AT_ONCE;
}

// Called when what the pending jobs were recorded for is gone (the swapchain
// was destroyed or resized). The cuts stay, so the game's native lists still
// run. A job with a cancel function is told and stays scheduled; any other job
// is dropped. Call with the execution lock held. Cancel functions run, and
// dropped jobs are released, outside the scheduler's own lock, because both
// can release objects whose destruction calls back into the handlers below.
inline void CancelJobs() {
  std::vector<Job> dropped;
  std::vector<std::function<void()>> cancels;
  {
    const std::unique_lock lock(internal::state.mutex);
    for (auto& pending : internal::state.pending) {
      if (pending.job.cancel) {
        cancels.push_back(pending.job.cancel);
      } else {
        dropped.push_back(std::exchange(pending.job, {}));
      }
    }
  }
  for (const auto& cancel : cancels) {
    cancel();
  }
}

inline void OnExecuteSecondaryCommandList(
    reshade::api::command_list* command_list,
    reshade::api::command_list* secondary_command_list) {
  const auto* device = Device();
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
    internal::CompleteRecording(secondary_command_list,
                                reinterpret_cast<ID3D11CommandList*>(command_list->get_native()));  // NOLINT(performance-no-int-to-ptr)
    return;
  }

  // Otherwise the game is executing a finished list. Collect the cuts that lead
  // to it by following each cut's prefix back to the cut before it. The copies
  // keep each cut's executed flag from before this execution.
  auto* post = reinterpret_cast<ID3D11CommandList*>(secondary_command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  std::vector<internal::Pending> chain;
  {
    const std::unique_lock lock(internal::state.mutex);
    auto* remainder = post;
    for (size_t index = 0u; index < internal::state.pending.size(); ++index) {
      const auto found = std::ranges::find_if(internal::state.pending, [remainder](const internal::Pending& candidate) {
        return candidate.post.Get() == remainder;
      });
      if (found == internal::state.pending.end()) break;
      chain.push_back(*found);
      found->executed = true;
      remainder = found->prefix.Get();
    }
  }

  auto* context = reinterpret_cast<ID3D11DeviceContext*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (context == nullptr) return;
  // A list executed into another deferred list passes on every output it
  // uses, including lists that have no cut.
  const auto notify_executed_into = [command_list](ID3D11CommandList* executed) {
    if (const auto observer = internal::list_executed_into.load(std::memory_order_acquire); observer != nullptr) {
      observer(command_list, executed);
    }
  };
  if (context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) {
    notify_executed_into(post);
  }
  if (chain.empty()) {
    if (context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE) {
      lighting::temporal_inputs::Submit(post);
    }
    return;
  }
  std::ranges::reverse(chain);
  // Executed into another deferred list: replay the prefixes into it.
  if (context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) {
    for (const auto& pending : chain) {
      const engine::views::ScopedOwner owner_scope(pending.scope);
      notify_executed_into(pending.prefix.Get());
      context->ExecuteCommandList(pending.prefix.Get(), TRUE);
      if (pending.job.run_merged) {
        const ExecutionGuard execution_guard;
        pending.job.run_merged(command_list, !pending.executed);
      }
    }
    return;
  }

  // Declared before the guards so its total includes their release.
  std::optional<d3d12_timing::CpuSubmission> cpu_timing;
  if (const auto timed = std::ranges::find_if(chain, [](const internal::Pending& pending) { return pending.job.timing.has_value(); });
      timed != chain.end()) {
    cpu_timing.emplace(*timed->job.timing);
  }
  const ExecutionGuard execution_guard;
  const internal::ImmediateContextStateGuard state_guard(context);
  d3d12_timing::CpuSubmission::EndPart("D3D11 state save");
  for (const auto& pending : chain) {
    const engine::views::ScopedOwner owner_scope(pending.scope);
    context->ExecuteCommandList(pending.prefix.Get(), state_guard.IsActive() ? FALSE : TRUE);
    lighting::temporal_inputs::Submit(pending.prefix.Get());
    d3d12_timing::CpuSubmission::EndPart("native prefix");
    // Every native prefix runs even when its job was canceled.
    if (pending.job.run) {
      pending.job.run(command_list, {.first = !pending.executed, .state_saved = state_guard.IsActive()});
    }
  }
  // ReShade submits this final native remainder immediately after this event.
  lighting::temporal_inputs::Submit(post);
}

inline void OnDestroyCommandList(reshade::api::command_list* command_list) {
  lighting::temporal_inputs::Destroy(command_list);
  const auto* device = Device();
  if (device == nullptr || command_list == nullptr || command_list->get_device() != device) return;
  auto* native_command_list = reinterpret_cast<ID3D11CommandList*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  auto* native_context = reinterpret_cast<ID3D11DeviceContext*>(command_list->get_native());     // NOLINT(performance-no-int-to-ptr)
  std::vector<internal::Pending> retired;
  {
    const std::unique_lock lock(internal::state.mutex);
    internal::state.active_queries.erase(native_context);
    internal::state.recording_lists.erase(native_context);
    std::vector<ID3D11CommandList*> orphaned_lists = {native_command_list};
    // Prefixes from cuts were finished directly on the native context, so
    // ReShade never reports their destruction. Remove the whole chain of cuts
    // that leads to this list.
    bool removed = true;
    while (removed) {
      removed = false;
      for (auto pending = internal::state.pending.begin(); pending != internal::state.pending.end();) {
        const bool matches = pending->recording == command_list
                             || std::ranges::find(orphaned_lists, pending->post.Get()) != orphaned_lists.end();
        if (!matches) {
          ++pending;
          continue;
        }
        orphaned_lists.push_back(pending->prefix.Get());
        retired.push_back(std::move(*pending));
        pending = internal::state.pending.erase(pending);
        removed = true;
      }
    }
  }
  for (const auto& pending : retired) {
    lighting::temporal_inputs::Forget(pending.prefix.Get());
    if (!pending.executed && pending.job.on_abandoned) {
      pending.job.on_abandoned();
    }
  }
}

}  // namespace mgsv::rendering::submission

#endif  // RENODX_GAMES_MGSV_RENDERING_SUBMISSION_HPP_
