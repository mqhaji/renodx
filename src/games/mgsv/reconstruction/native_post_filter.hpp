// SPDX-License-Identifier: MIT
#ifndef MGSV_RECONSTRUCTION_NATIVE_POST_FILTER_HPP_
#define MGSV_RECONSTRUCTION_NATIVE_POST_FILTER_HPP_

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <system_error>

#include "../../../utils/detour.hpp"
#include "../rendering/native_context.hpp"
#include "logging.hpp"
#include "probe_api.hpp"

// The engine starts a view's post-processing in DgDx11::StartPostFilter: once
// per view, on the build thread, after the last scene writer and before the
// first post pass. It opens that thread's next command list only at the first
// post draw, so the hook arms the scene and the list the build thread begins
// next is the one to cut. Reconstruction runs at that cut.
namespace mgsv::reconstruction::native_post_filter {

// Runs on the build thread at the cut of the presented main view, with the
// deferred context post-processing is recorded into. The list is empty unless
// the engine already had it open when post-processing started. It must not
// throw, and it may take the coordinator's execution lock.
using CutCallback = void (*)(ID3D11DeviceContext* context);

namespace internal {

inline constexpr uintptr_t START_POST_FILTER_RVA = 0x2C2D90u;
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
  probe::PostFilterStatus status;
};
static Statistics statistics;

struct Scene {
  uint64_t scene = 0u;
  uint64_t generation = 0u;
  bool operator==(const Scene&) const = default;
};
// The scene whose post-processing has started on this thread and whose next
// command list has not begun, and the scene this thread cut last.
static thread_local Scene armed;
static thread_local Scene last_cut;

// The build thread can run a worker job between its own lists. That job's
// command list is not the one post-processing continues in.
inline bool IsBuildThreadList(const rendering::native_context::Recording& recording) {
  return recording.d3d_context != 0u && recording.command_buffer == 0u;
}

inline void Cut(const rendering::native_context::Recording& recording, bool list_was_open) {
  last_cut = armed;
  armed = {};
  uint64_t cuts = 0u;
  {
    const std::unique_lock lock(statistics.mutex);
    cuts = ++statistics.status.cuts;
    if (list_was_open) {
      ++statistics.status.lists_open_at_start;
    }
  }
  if (cuts == 1u) {
    logging::Info("first cut at the engine's post-processing start scene=", last_cut.scene);
  }
  if (list_was_open) {
    static std::atomic_flag reported;
    if (!reported.test_and_set(std::memory_order_relaxed)) {
      logging::Info("a command list was already open at the post-processing start scene=", last_cut.scene, "; it is cut there");
    }
  }
  if (const auto callback = cut_callback.load(std::memory_order_acquire); callback != nullptr) {
    callback(rendering::native_context::Unwrap(recording.d3d_context).Get());
  }
}

inline void ReportUnusedStart(const Scene& scene) {
  {
    const std::unique_lock lock(statistics.mutex);
    ++statistics.status.unused_starts;
  }
  static std::atomic_flag reported;
  if (!reported.test_and_set(std::memory_order_relaxed)) {
    logging::Warn("a view recorded nothing after its post-processing start scene=", scene.scene, "; it was not reconstructed");
  }
}

static void __fastcall HookStartPostFilter(void* dg) {
  calls_in_flight.fetch_add(1u, std::memory_order_acq_rel);
  if (const auto owner = rendering::native_context::GetScope().owner; rendering::native_context::IsPresentedMainView(owner)) {
    const Scene scene = {.scene = owner.scene, .generation = owner.generation};
    const bool repeated = scene == armed || scene == last_cut;
    {
      const std::unique_lock lock(statistics.mutex);
      ++statistics.status.starts;
      if (repeated) {
        ++statistics.status.repeated_starts;
      }
    }
    if (repeated) {
      static std::atomic_flag reported;
      if (!reported.test_and_set(std::memory_order_relaxed)) {
        logging::Warn("post-processing started twice in one view build scene=", scene.scene, "; the second start is ignored");
      }
    } else {
      if (armed.scene != 0u) {
        ReportUnusedStart(armed);
      }
      armed = scene;
      if (const auto recording = rendering::native_context::GetRecording(); IsBuildThreadList(recording)) {
        Cut(recording, true);
      }
    }
  }
  hooks.start_post_filter(dg);
  calls_in_flight.fetch_sub(1u, std::memory_order_acq_rel);
}

// Runs on every recording thread right after the engine begins a list.
static void OnRecordingBegan() {
  if (armed.scene == 0u) return;
  const auto recording = rendering::native_context::GetRecording();
  if (!IsBuildThreadList(recording)) return;
  const auto owner = rendering::native_context::GetScope().owner;
  if (owner.scene == armed.scene && owner.generation == armed.generation) {
    Cut(recording, false);
    return;
  }
  ReportUnusedStart(armed);
  armed = {};
}

}  // namespace internal

inline probe::PostFilterStatus ReadStatus() {
  const std::shared_lock lock(internal::statistics.mutex);
  return internal::statistics.status;
}

// Call after native_context::Install has succeeded; repeated calls are
// harmless. Balance with Detach before native_context::Shutdown.
inline bool Install(CutCallback on_cut) {
  const std::unique_lock lock(internal::hooks.mutex);
  internal::cut_callback.store(on_cut, std::memory_order_release);
  if (!internal::hooks.installed) {
    const auto fail = [](const char* message) {
      if (!internal::hooks.logged_failure) {
        logging::Warn(message);
        internal::hooks.logged_failure = true;
      }
      return false;
    };
    auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    if (base == nullptr) return fail("native executable is unavailable; reconstruction cannot be scheduled");
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return fail("native executable header validation failed; reconstruction cannot be scheduled");
    const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.TimeDateStamp != rendering::native_context::IMAGE_TIMESTAMP
        || pe->OptionalHeader.SizeOfImage != rendering::native_context::IMAGE_SIZE) {
      return fail("unsupported native executable; reconstruction cannot be scheduled");
    }
    // Installed-executable bytes up to the function's first relative operand.
    constexpr std::array<uint8_t, 13> start_post_filter = {0x48, 0x89, 0x5C, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x8B, 0x05};
    if (std::memcmp(base + internal::START_POST_FILTER_RVA, start_post_filter.data(), start_post_filter.size()) != 0) {
      return fail("native post-processing start validation failed; reconstruction cannot be scheduled");
    }
    internal::hooks.start_post_filter = reinterpret_cast<internal::StartPostFilter>(base + internal::START_POST_FILTER_RVA);
    try {
      renodx::utils::detour::Install(std::array{renodx::utils::detour::Function(&internal::hooks.start_post_filter, internal::HookStartPostFilter)});
    } catch (const std::system_error&) {
      internal::hooks.start_post_filter = nullptr;
      return fail("native post-processing start hook installation failed; reconstruction cannot be scheduled");
    }
    internal::hooks.installed = true;
    internal::hooks.logged_failure = false;
    logging::Info("installed the native post-processing start hook");
  }
  if (!internal::hooks.listening) {
    internal::hooks.listening = rendering::native_context::SetRecordingBegan(nullptr, internal::OnRecordingBegan);
  }
  return internal::hooks.listening;
}

// Waits for a cut in progress, which can be waiting for the coordinator's
// execution lock: do not call this while holding that lock.
inline bool Detach() {
  const std::unique_lock lock(internal::hooks.mutex);
  if (internal::hooks.listening) {
    rendering::native_context::SetRecordingBegan(internal::OnRecordingBegan, nullptr);
    internal::hooks.listening = false;
  }
  if (!internal::hooks.installed) return true;
  try {
    renodx::utils::detour::Uninstall(std::array{renodx::utils::detour::Function(&internal::hooks.start_post_filter, internal::HookStartPostFilter)});
  } catch (const std::system_error&) {
    logging::Warn("native post-processing start hook removal failed");
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

}  // namespace mgsv::reconstruction::native_post_filter

#endif  // MGSV_RECONSTRUCTION_NATIVE_POST_FILTER_HPP_
