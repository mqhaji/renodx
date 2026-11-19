// SPDX-License-Identifier: MIT
#ifndef RENODX_GAMES_MGSV_RENDERING_NATIVE_VELOCITY_HPP_
#define RENODX_GAMES_MGSV_RENDERING_NATIVE_VELOCITY_HPP_

#include <windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <system_error>

#include <include/reshade.hpp>

#include "../../../utils/detour.hpp"
#include "native_context.hpp"

// The engine draws object and camera motion vectors in
// PluginDeferredGeometry::PostExec, and only when its motion blur plugin has
// set a per-build flag (motion blur enabled with a non-zero amount). These
// hooks keep the vectors drawn whenever a feature of the mod needs them, and
// tell whether a view build drew them.
namespace mgsv::rendering::native_velocity {

// Present sets this while temporal reconstruction or enhanced indirect
// lighting is on.
inline std::atomic<bool> wanted = false;

namespace internal {

inline constexpr uintptr_t POST_EXEC_RVA = 0x1C54D0u;
inline constexpr uintptr_t END_SCREEN_SPACE_VELOCITY_RVA = 0x2BF7A0u;
// Render keeps a pointer to the build's frame flags at +0xD0. Bit 0 of the
// first word asks for motion vectors: PluginMotionBlur::InitRender is its only
// writer and PostExec its only reader. The motion blur draw has its own test.
inline constexpr size_t RENDER_FRAME_FLAGS_OFFSET = 0xD0u;
inline constexpr uint64_t MOTION_VECTORS_FLAG = 1u;

using PostExec = void(__fastcall*)(void* plugin, void* render, void* viewport);
using EndScreenSpaceVelocity = void(__fastcall*)(void* dg);

struct HookState {
  std::shared_mutex mutex;
  PostExec post_exec = nullptr;
  EndScreenSpaceVelocity end_screen_space_velocity = nullptr;
  bool installed = false;
  bool logged_failure = false;
};
static HookState hooks;
static std::atomic<uint32_t> calls_in_flight = 0u;

// The view build whose camera-velocity pass this thread recorded last.
struct Scene {
  uint64_t scene = 0u;
  uint64_t generation = 0u;
};
static thread_local Scene recorded;

static void __fastcall HookPostExec(void* plugin, void* render, void* viewport) {
  calls_in_flight.fetch_add(1u, std::memory_order_acq_rel);
  auto* flags = *reinterpret_cast<uint64_t* const*>(static_cast<const uint8_t*>(render) + RENDER_FRAME_FLAGS_OFFSET);
  // Every view of the build shares the flags, so the bit is set only for the
  // presented main view's call and cleared again after it.
  const bool keep = flags != nullptr && (*flags & MOTION_VECTORS_FLAG) == 0u && wanted.load(std::memory_order_acquire)
                    && native_context::IsPresentedMainView(native_context::GetScope().owner);
  if (keep) {
    *flags |= MOTION_VECTORS_FLAG;
    static std::atomic_flag reported;
    if (!reported.test_and_set(std::memory_order_relaxed)) {
      reshade::log::message(reshade::log::level::info, "MgsvVelocity: the game's motion blur is off; the mod keeps the engine's motion vectors on.");
    }
  }
  hooks.post_exec(plugin, render, viewport);
  if (keep) {
    *flags &= ~MOTION_VECTORS_FLAG;
  }
  calls_in_flight.fetch_sub(1u, std::memory_order_acq_rel);
}

// The engine calls this once its camera-velocity draw has been recorded, on the
// thread building the view.
static void __fastcall HookEndScreenSpaceVelocity(void* dg) {
  calls_in_flight.fetch_add(1u, std::memory_order_acq_rel);
  const auto owner = native_context::GetScope().owner;
  recorded = {.scene = owner.scene, .generation = owner.generation};
  hooks.end_screen_space_velocity(dg);
  calls_in_flight.fetch_sub(1u, std::memory_order_acq_rel);
}

// The replaced functions, for installing and removing them together.
inline auto HookedFunctions() {
  return std::array{
      renodx::utils::detour::Function(&hooks.post_exec, HookPostExec),
      renodx::utils::detour::Function(&hooks.end_screen_space_velocity, HookEndScreenSpaceVelocity),
  };
}

}  // namespace internal

// Whether the engine recorded this view build's camera-velocity pass. Ask on
// the thread that builds the view.
inline bool WasRecorded(const native_context::Owner& owner) {
  return owner.scene != 0u && internal::recorded.scene == owner.scene && internal::recorded.generation == owner.generation;
}

// Balance a successful call with Detach.
inline bool Install() {
  const std::unique_lock lock(internal::hooks.mutex);
  if (internal::hooks.installed) return true;
  const auto fail = [](const char* message) {
    if (!internal::hooks.logged_failure) {
      reshade::log::message(reshade::log::level::warning, message);
      internal::hooks.logged_failure = true;
    }
    return false;
  };
  auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
  if (base == nullptr) return fail("MgsvVelocity: native executable is unavailable; motion vectors follow the game's motion blur.");
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) return fail("MgsvVelocity: native executable header validation failed; motion vectors follow the game's motion blur.");
  const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
  if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.TimeDateStamp != native_context::IMAGE_TIMESTAMP
      || pe->OptionalHeader.SizeOfImage != native_context::IMAGE_SIZE) {
    return fail("MgsvVelocity: unsupported native executable; motion vectors follow the game's motion blur.");
  }
  // Installed-executable bytes. The second function begins like its neighbors,
  // so its signature includes the operand that addresses the G-buffer.
  constexpr std::array<uint8_t, 25> post_exec = {0x48, 0x8B, 0xC4, 0x53, 0x55, 0x56, 0x57, 0x41, 0x56, 0x48, 0x81, 0xEC, 0x40,
                                                 0x01, 0x00, 0x00, 0x48, 0xC7, 0x44, 0x24, 0x40, 0xFE, 0xFF, 0xFF, 0xFF};
  constexpr std::array<uint8_t, 16> end_screen_space_velocity = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B,
                                                                 0xD9, 0x48, 0x8B, 0x0D, 0x10, 0x49, 0x8D, 0x02};
  if (std::memcmp(base + internal::POST_EXEC_RVA, post_exec.data(), post_exec.size()) != 0
      || std::memcmp(base + internal::END_SCREEN_SPACE_VELOCITY_RVA, end_screen_space_velocity.data(), end_screen_space_velocity.size()) != 0) {
    return fail("MgsvVelocity: native velocity pass validation failed; motion vectors follow the game's motion blur.");
  }
  internal::hooks.post_exec = reinterpret_cast<internal::PostExec>(base + internal::POST_EXEC_RVA);
  internal::hooks.end_screen_space_velocity = reinterpret_cast<internal::EndScreenSpaceVelocity>(base + internal::END_SCREEN_SPACE_VELOCITY_RVA);
  try {
    renodx::utils::detour::Install(internal::HookedFunctions());
  } catch (const std::system_error&) {
    internal::hooks.post_exec = nullptr;
    internal::hooks.end_screen_space_velocity = nullptr;
    return fail("MgsvVelocity: native velocity hook installation failed; motion vectors follow the game's motion blur.");
  }
  internal::hooks.installed = true;
  internal::hooks.logged_failure = false;
  reshade::log::message(reshade::log::level::info, "MgsvVelocity: installed the native velocity hooks.");
  return true;
}

inline bool Detach() {
  const std::unique_lock lock(internal::hooks.mutex);
  if (!internal::hooks.installed) return true;
  try {
    renodx::utils::detour::Uninstall(internal::HookedFunctions());
  } catch (const std::system_error&) {
    reshade::log::message(reshade::log::level::warning, "MgsvVelocity: native velocity hook removal failed.");
    return false;
  }
  while (internal::calls_in_flight.load(std::memory_order_acquire) != 0u) {
    SwitchToThread();
  }
  internal::hooks.installed = false;
  internal::hooks.post_exec = nullptr;
  internal::hooks.end_screen_space_velocity = nullptr;
  return true;
}

}  // namespace mgsv::rendering::native_velocity

#endif  // RENODX_GAMES_MGSV_RENDERING_NATIVE_VELOCITY_HPP_
