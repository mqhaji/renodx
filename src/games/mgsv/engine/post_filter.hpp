// SPDX-License-Identifier: MIT
#ifndef RENODX_GAMES_MGSV_ENGINE_POST_FILTER_HPP_
#define RENODX_GAMES_MGSV_ENGINE_POST_FILTER_HPP_

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <system_error>

#include <include/reshade.hpp>

#include "../../../utils/detour.hpp"
#include "build.hpp"
#include "image.hpp"
#include "views.hpp"

// The engine starts a view's post-processing in DgDx11::StartPostFilter: once
// per view, on the thread that builds it (the build thread), after the last draw that
// writes the scene and before the first post-processing pass. Usually no
// command list is open then; the engine opens the next one at the first
// post-processing draw. So the hook marks the scene as waiting, and the next
// list that thread begins is the one to cut: the callback ends it there and
// schedules reconstruction between the two halves. A list that is already
// open is cut at once.
namespace mgsv::engine::post_filter {

// Runs on the build thread at the cut of the presented main view, with the
// deferred context post-processing is recorded into. The list is empty unless
// the engine already had it open when post-processing started. It must not
// throw, and it may take the submission execution lock.
using CutCallback = void (*)(ID3D11DeviceContext* context);

// Counts for the presented main view. Normally starts equals cuts and the
// other three stay zero.
struct Counts {
  uint64_t starts = 0u;
  uint64_t cuts = 0u;
  uint64_t lists_open_at_start = 0u;
  uint64_t unused_starts = 0u;
  uint64_t repeated_starts = 0u;
};

inline constexpr std::array REQUIRED_ENTRIES = {engine::Entry::DG_START_POST_FILTER};
inline constexpr engine::Requirement REQUIREMENT = {.feature = "Temporal reconstruction (DLSS, FSR and Analytical TAA)", .entries = REQUIRED_ENTRIES};

namespace internal {

using StartPostFilter = void(__fastcall*)(void* dg);

struct HookState {
  std::shared_mutex mutex;
  StartPostFilter start_post_filter = nullptr;
  bool installed = false;
  bool listening = false;
  bool logged_failure = false;
};
static HookState hooks;
static std::atomic<CutCallback> cut_callback = nullptr;
static std::atomic<uint32_t> calls_in_flight = 0u;

struct Statistics {
  std::shared_mutex mutex;
  Counts counts;
};
static Statistics statistics;

struct Scene {
  uint64_t scene = 0u;
  uint64_t generation = 0u;
  bool operator==(const Scene&) const = default;
};
// armed: the scene whose post-processing has started on this thread but whose
// next command list has not begun yet. last_cut: the scene this thread cut last.
static thread_local Scene armed;
static thread_local Scene last_cut;

// The build thread can run a worker job between its own lists. That job's
// command list is not the one post-processing continues in.
inline bool IsBuildThreadList(const engine::views::Recording& recording) {
  return recording.d3d_context != 0u && recording.command_buffer == 0u;
}

// The messages belong to temporal reconstruction, the only feature that cuts here.
inline void Log(reshade::log::level level, const std::string& message) {
  reshade::log::message(level, ("MgsvTaa: " + message).c_str());
}

inline void Cut(const engine::views::Recording& recording, bool list_was_open) {
  last_cut = armed;
  armed = {};
  uint64_t cuts = 0u;
  {
    const std::unique_lock lock(statistics.mutex);
    cuts = ++statistics.counts.cuts;
    if (list_was_open) {
      ++statistics.counts.lists_open_at_start;
    }
  }
  if (cuts == 1u) {
    Log(reshade::log::level::info, "first cut at the engine's post-processing start scene=" + std::to_string(last_cut.scene));
  }
  if (list_was_open) {
    static std::atomic_flag reported;
    if (!reported.test_and_set(std::memory_order_relaxed)) {
      Log(reshade::log::level::info, "a command list was already open at the post-processing start scene=" + std::to_string(last_cut.scene) + "; it is cut there");
    }
  }
  if (const auto callback = cut_callback.load(std::memory_order_acquire); callback != nullptr) {
    callback(engine::views::Unwrap(recording.d3d_context).Get());
  }
}

inline void ReportUnusedStart(const Scene& scene) {
  {
    const std::unique_lock lock(statistics.mutex);
    ++statistics.counts.unused_starts;
  }
  static std::atomic_flag reported;
  if (!reported.test_and_set(std::memory_order_relaxed)) {
    Log(reshade::log::level::warning, "a view recorded nothing after its post-processing start scene=" + std::to_string(scene.scene) + "; it was not reconstructed");
  }
}

static void __fastcall HookStartPostFilter(void* dg) {
  calls_in_flight.fetch_add(1u, std::memory_order_acq_rel);
  if (const auto owner = engine::views::GetScope().owner; engine::views::IsPresentedMainView(owner)) {
    const Scene scene = {.scene = owner.scene, .generation = owner.generation};
    const bool repeated = scene == armed || scene == last_cut;
    {
      const std::unique_lock lock(statistics.mutex);
      ++statistics.counts.starts;
      if (repeated) {
        ++statistics.counts.repeated_starts;
      }
    }
    if (repeated) {
      static std::atomic_flag reported;
      if (!reported.test_and_set(std::memory_order_relaxed)) {
        Log(reshade::log::level::warning, "post-processing started twice in one view build scene=" + std::to_string(scene.scene) + "; the second start is ignored");
      }
    } else {
      if (armed.scene != 0u) {
        ReportUnusedStart(armed);
      }
      armed = scene;
      // A list the build thread already has open is cut now.
      if (const auto recording = engine::views::GetRecording(); IsBuildThreadList(recording)) {
        Cut(recording, true);
      }
    }
  }
  hooks.start_post_filter(dg);
  calls_in_flight.fetch_sub(1u, std::memory_order_acq_rel);
}

// Runs on every recording thread right after the engine begins a list. A list
// for another scene means the waiting scene recorded nothing after its start.
static void OnRecordingBegan() {
  if (armed.scene == 0u) return;
  const auto recording = engine::views::GetRecording();
  if (!IsBuildThreadList(recording)) return;
  const auto owner = engine::views::GetScope().owner;
  if (owner.scene == armed.scene && owner.generation == armed.generation) {
    Cut(recording, false);
    return;
  }
  ReportUnusedStart(armed);
  armed = {};
}

}  // namespace internal

inline Counts ReadCounts() {
  const std::shared_lock lock(internal::statistics.mutex);
  return internal::statistics.counts;
}

// Call after engine::views::Install has succeeded; repeated calls are
// harmless. Balance with Detach before engine::views::Shutdown.
inline bool Install(CutCallback on_cut) {
  const std::unique_lock lock(internal::hooks.mutex);
  internal::cut_callback.store(on_cut, std::memory_order_release);
  if (!internal::hooks.installed) {
    const auto fail = [](const char* message) {
      if (!internal::hooks.logged_failure) {
        internal::Log(reshade::log::level::warning, message);
        internal::hooks.logged_failure = true;
      }
      return false;
    };
    if (!engine::Verified(REQUIREMENT)) {
      return fail("the post-processing start entry did not verify; reconstruction cannot be scheduled");
    }
    internal::hooks.start_post_filter = reinterpret_cast<internal::StartPostFilter>(engine::Address(engine::Entry::DG_START_POST_FILTER));
    try {
      renodx::utils::detour::Install(std::array{renodx::utils::detour::Function(&internal::hooks.start_post_filter, internal::HookStartPostFilter)});
    } catch (const std::system_error&) {
      internal::hooks.start_post_filter = nullptr;
      return fail("native post-processing start hook installation failed; reconstruction cannot be scheduled");
    }
    internal::hooks.installed = true;
    internal::hooks.logged_failure = false;
    internal::Log(reshade::log::level::info, "installed the native post-processing start hook");
  }
  if (!internal::hooks.listening) {
    internal::hooks.listening = engine::views::SetRecordingBegan(nullptr, internal::OnRecordingBegan);
  }
  return internal::hooks.listening;
}

// Waits for a cut in progress, which can be waiting for the submission
// execution lock: do not call this while holding that lock.
inline bool Detach() {
  const std::unique_lock lock(internal::hooks.mutex);
  if (internal::hooks.listening) {
    engine::views::SetRecordingBegan(internal::OnRecordingBegan, nullptr);
    internal::hooks.listening = false;
  }
  if (!internal::hooks.installed) return true;
  try {
    renodx::utils::detour::Uninstall(std::array{renodx::utils::detour::Function(&internal::hooks.start_post_filter, internal::HookStartPostFilter)});
  } catch (const std::system_error&) {
    internal::Log(reshade::log::level::warning, "native post-processing start hook removal failed");
    return false;
  }
  while (internal::calls_in_flight.load(std::memory_order_acquire) != 0u) {
    SwitchToThread();
  }
  internal::hooks.installed = false;
  internal::hooks.start_post_filter = nullptr;
  internal::cut_callback.store(nullptr, std::memory_order_release);
  return true;
}

}  // namespace mgsv::engine::post_filter

#endif  // RENODX_GAMES_MGSV_ENGINE_POST_FILTER_HPP_
