// SPDX-License-Identifier: MIT
#ifndef MGSV_SHADOWS_NATIVE_LIGHTS_HPP_
#define MGSV_SHADOWS_NATIVE_LIGHTS_HPP_

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <system_error>

#include <include/reshade.hpp>

#include "../../../utils/detour.hpp"
#include "build.hpp"
#include "image.hpp"
#include "views.hpp"

// The engine side of contact shadows: where local lighting begins on the build
// thread, and the CPU constants a worker has prepared for the light it is about
// to draw.
namespace mgsv::engine::local_lights {

// The scene and light constant banks of one light draw, as shader b2 and b3.
struct Constants {
  std::array<float, 120> scene = {};
  std::array<float, 44> light = {};
};

// Runs on the build thread when PluginLocalLight::MainExec begins, before that
// plugin branches its light jobs. It must not throw.
using LocalLightsCallback = void (*)();

// The hook, and the engine's CPU constant banks the batch reads light records from.
inline constexpr std::array REQUIRED_ENTRIES = {
    engine::Entry::LOCAL_LIGHT_MAIN_EXEC,
    engine::Entry::COMMAND_BUFFER_SET_VECTOR,
    engine::Entry::SHADOW_CONSTANT_REGISTER_SET_VECTOR,
    engine::Entry::SHADOW_CONSTANT_REGISTER_FLUSH,
};
inline constexpr engine::Requirement REQUIREMENT = {.feature = "Contact shadows", .entries = REQUIRED_ENTRIES};

namespace internal {

using MainExec = void(__fastcall*)(void*, void*, void*);

struct HookState {
  std::shared_mutex mutex;
  MainExec main_exec = nullptr;
  size_t registrations = 0u;
  bool logged_failure = false;
};
static HookState hooks;
static std::atomic<LocalLightsCallback> local_lights_callback = nullptr;
static std::atomic<uint32_t> calls_in_flight = 0u;

static void __fastcall HookMainExec(void* plugin, void* render, void* viewport) {
  calls_in_flight.fetch_add(1u, std::memory_order_acq_rel);
  if (const auto callback = local_lights_callback.load(std::memory_order_acquire); callback != nullptr) {
    callback();
  }
  hooks.main_exec(plugin, render, viewport);
  calls_in_flight.fetch_sub(1u, std::memory_order_acq_rel);
}

}  // namespace internal

// Copy at the shaded draw. Native Flush has uploaded these CPU constant banks
// without clearing them; the next light can overwrite them as soon as we return.
// The CommandBuffer must be the one recording into the draw's own context.
inline bool ReadConstants(uintptr_t command_buffer, ID3D11DeviceContext* context, Constants* output) {
  if (command_buffer == 0u || context == nullptr) return false;
  // The executable signatures checked at installation establish these layouts:
  // CommandBuffer+8 is its native context, whose +8 is the D3D context, and
  // CommandBuffer+0x28 points to eight 32-byte bank entries. Each entry holds
  // its CPU data at +8 and float4 count at +0x18. Banks 2/3 match shader b2/b3.
  const auto* buffer = reinterpret_cast<const std::byte*>(command_buffer);  // NOLINT(performance-no-int-to-ptr)
  const auto* native_context = *reinterpret_cast<const std::byte* const*>(buffer + 8u);
  if (native_context == nullptr || engine::views::Unwrap(*reinterpret_cast<const uintptr_t*>(native_context + 8u)).Get() != context) return false;
  const auto* banks = *reinterpret_cast<const std::byte* const*>(buffer + 0x28u);
  if (banks == nullptr) return false;
  const auto bank_count = *reinterpret_cast<const uint32_t*>(banks + 0x108u);
  const auto active_banks = *reinterpret_cast<const uint32_t*>(banks + 0x114u);
  if (bank_count != 8u || (active_banks & 0xCu) != 0xCu) return false;
  const auto copy_bank = [banks](uint32_t bank, std::span<float> destination) {
    const auto* entry = banks + (bank * 0x20u);
    const auto count = *reinterpret_cast<const uint32_t*>(entry + 0x18u);
    const auto* data = *reinterpret_cast<const float* const*>(entry + 8u);
    if (data == nullptr || count != destination.size() / 4u) return false;
    std::memcpy(destination.data(), data, destination.size_bytes());
    return true;
  };
  return copy_bank(2u, output->scene) && copy_bank(3u, output->light);
}

// Register each device once and balance successful calls with Detach.
inline bool Install(LocalLightsCallback on_local_lights) {
  const std::unique_lock lock(internal::hooks.mutex);
  internal::local_lights_callback.store(on_local_lights, std::memory_order_release);
  if (internal::hooks.registrations != 0u) {
    ++internal::hooks.registrations;
    return true;
  }
  const auto fail = [](const char* message) {
    if (!internal::hooks.logged_failure) {
      reshade::log::message(reshade::log::level::warning, message);
      internal::hooks.logged_failure = true;
    }
    return false;
  };
  if (!engine::Verified(REQUIRED_ENTRIES)) {
    return fail("MgsvShadows: the local-light entries did not verify; contact shadows are off.");
  }
  internal::hooks.main_exec = reinterpret_cast<internal::MainExec>(engine::Address(engine::Entry::LOCAL_LIGHT_MAIN_EXEC));
  try {
    renodx::utils::detour::Install(std::array{renodx::utils::detour::Function(&internal::hooks.main_exec, internal::HookMainExec)});
  } catch (const std::system_error&) {
    internal::hooks.main_exec = nullptr;
    return fail("MgsvShadows: native local-light hook installation failed; contact shadows are off.");
  }
  internal::hooks.registrations = 1u;
  internal::hooks.logged_failure = false;
  reshade::log::message(reshade::log::level::info, "MgsvShadows: installed the native local-light hook.");
  return true;
}

inline bool Detach() {
  const std::unique_lock lock(internal::hooks.mutex);
  if (internal::hooks.registrations == 0u) return true;
  if (internal::hooks.registrations > 1u) {
    --internal::hooks.registrations;
    return true;
  }
  try {
    renodx::utils::detour::Uninstall(std::array{renodx::utils::detour::Function(&internal::hooks.main_exec, internal::HookMainExec)});
  } catch (const std::system_error&) {
    reshade::log::message(reshade::log::level::warning, "MgsvShadows: native local-light hook removal failed.");
    return false;
  }
  while (internal::calls_in_flight.load(std::memory_order_acquire) != 0u) {
    SwitchToThread();
  }
  internal::hooks.registrations = 0u;
  internal::hooks.main_exec = nullptr;
  internal::local_lights_callback.store(nullptr, std::memory_order_release);
  return true;
}

}  // namespace mgsv::engine::local_lights

#endif  // MGSV_SHADOWS_NATIVE_LIGHTS_HPP_
