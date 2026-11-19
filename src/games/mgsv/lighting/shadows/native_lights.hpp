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

#include "../../../../utils/detour.hpp"
#include "../../rendering/native_context.hpp"

// The engine side of contact shadows: where local lighting begins on the build
// thread, and the CPU constants a worker has prepared for the light it is about
// to draw.
namespace mgsv::shadows::native_lights {

// The scene and light constant banks of one light draw, as shader b2 and b3.
struct Constants {
  std::array<float, 120> scene = {};
  std::array<float, 44> light = {};
};

// Runs on the build thread when PluginLocalLight::MainExec begins, before that
// plugin branches its light jobs. It must not throw.
using LocalLightsCallback = void (*)();

namespace internal {

inline constexpr uintptr_t MAIN_EXEC_RVA = 0x20AA00u;
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
  if (native_context == nullptr || rendering::native_context::Unwrap(*reinterpret_cast<const uintptr_t*>(native_context + 8u)).Get() != context) return false;
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
  auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
  if (base == nullptr) return fail("MgsvShadows: native executable is unavailable; contact shadows are off.");
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) return fail("MgsvShadows: native executable header validation failed; contact shadows are off.");
  const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
  if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.TimeDateStamp != rendering::native_context::IMAGE_TIMESTAMP
      || pe->OptionalHeader.SizeOfImage != rendering::native_context::IMAGE_SIZE) {
    return fail("MgsvShadows: unsupported native executable; contact shadows are off.");
  }

  // These installed-executable signatures contain no relative operands, so all
  // bytes are significant. Validate the CPU constant layout as well as the hook.
  constexpr std::array<uint8_t, 28> main_exec = {0x48, 0x8B, 0xC4, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
                                                 0x48, 0x8D, 0xA8, 0x18, 0xFF, 0xFF, 0xFF, 0x48, 0x81, 0xEC, 0xB0, 0x01, 0x00, 0x00};
  constexpr std::array<uint8_t, 16> set_vector = {0x48, 0x83, 0xEC, 0x38, 0x48, 0x8B, 0x49, 0x28, 0x4D, 0x8B, 0xD0, 0x8B, 0xC2, 0xC1, 0xF8, 0x04};
  constexpr std::array<uint8_t, 18> set_bank_vector = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xF9, 0xBB, 0x01, 0x00, 0x00, 0x00};
  constexpr std::array<uint8_t, 29> flush_bank = {0x48, 0x89, 0x4C, 0x24, 0x08, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x81, 0xEC, 0xB0, 0x00, 0x00, 0x00, 0x8B, 0x81, 0x14, 0x01, 0x00, 0x00};
  if (std::memcmp(base + internal::MAIN_EXEC_RVA, main_exec.data(), main_exec.size()) != 0
      || std::memcmp(base + 0x2D8B70u, set_vector.data(), set_vector.size()) != 0
      || std::memcmp(base + 0x2C5F30u, set_bank_vector.data(), set_bank_vector.size()) != 0
      || std::memcmp(base + 0x2C5A10u, flush_bank.data(), flush_bank.size()) != 0) {
    return fail("MgsvShadows: native local-light validation failed; contact shadows are off.");
  }

  internal::hooks.main_exec = reinterpret_cast<internal::MainExec>(base + internal::MAIN_EXEC_RVA);
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

}  // namespace mgsv::shadows::native_lights

#endif  // MGSV_SHADOWS_NATIVE_LIGHTS_HPP_
