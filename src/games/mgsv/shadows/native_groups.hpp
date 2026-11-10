// SPDX-License-Identifier: MIT
#ifndef MGSV_SHADOWS_NATIVE_GROUPS_HPP_
#define MGSV_SHADOWS_NATIVE_GROUPS_HPP_

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <system_error>

#include <include/reshade.hpp>

#include "../../../utils/detour.hpp"

namespace mgsv::shadows::native_groups {

// This scope exists while native point/spot lighting records its D3D11 draws.
// It does not exist when the finished command list is executed or replayed.
struct Group {
  void* command_buffer = nullptr;
  ID3D11DeviceContext* context = nullptr;
  uint32_t light_type = 0u;  // Shared shadow convention: spot = 1, point = 2.
  // Keep the collected batch alive until both this scope and its GPU callback
  // release it. The callback may finish before the native group unwinds.
  std::shared_ptr<void> collected_batch;
};

struct Constants {
  std::array<float, 120> scene = {};
  std::array<float, 44> light = {};
};

using CompletionCallback = void (*)(Group*);

namespace internal {

// Both native methods have the same Win64 ABI. The final two pointers are the
// visible-light bucket and the list already selected for native shadow maps.
using DrawGroup = void(__fastcall*)(void*, uint32_t, void*, void*, void*, void*);

struct HookState {
  std::shared_mutex mutex;
  DrawGroup point = nullptr;
  DrawGroup spot = nullptr;
  size_t registrations = 0u;
  bool logged_failure = false;
};
static HookState hooks;
static thread_local Group* current = nullptr;
static std::atomic<CompletionCallback> completion_callback = nullptr;
static std::atomic<uint32_t> calls_in_flight = 0u;

// ReShade exposes the original COM object through this interface ID. Its
// get_native() returns that same object; the game's stored proxy is different.
inline constexpr GUID UNWRAPPED_OBJECT = {
    .Data1 = 0x7f2c9a11, .Data2 = 0x3b4e, .Data3 = 0x4d6a, .Data4 = {0x81, 0x2f, 0x5e, 0x9c, 0xd3, 0x7a, 0x1b, 0x42}};

struct GroupScope {
  mutable Group group;  // Draw callbacks update this thread's collected batch.
  Group* previous = nullptr;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;

  GroupScope(void* command_buffer, uint32_t light_type)
      : group{.command_buffer = command_buffer,
              .light_type = light_type},
        previous(current) {
    calls_in_flight.fetch_add(1u, std::memory_order_acq_rel);
    if (command_buffer != nullptr) {
      // These are borrowed native objects. The executable signatures checked at
      // installation establish the CommandBuffer and native-context layouts.
      const auto* native_context = *reinterpret_cast<const std::byte* const*>(static_cast<const std::byte*>(command_buffer) + 8u);
      if (native_context != nullptr) {
        auto* proxy = *reinterpret_cast<ID3D11DeviceContext* const*>(native_context + 8u);
        if (proxy != nullptr) {
          if (FAILED(proxy->QueryInterface(UNWRAPPED_OBJECT, reinterpret_cast<void**>(context.GetAddressOf())))) {
            context = proxy;
          }
          group.context = context.Get();
        }
      }
    }
    current = &group;
  }

  GroupScope(const GroupScope&) = delete;
  GroupScope& operator=(const GroupScope&) = delete;
  GroupScope(GroupScope&&) = delete;
  GroupScope& operator=(GroupScope&&) = delete;

  ~GroupScope() {
    // Restore the enclosing group before finishing this one. Nested native
    // calls must not append lights to the group that just returned.
    current = previous;
    if (const auto callback = completion_callback.load(std::memory_order_acquire); callback != nullptr) {
      callback(&group);
    }
    calls_in_flight.fetch_sub(1u, std::memory_order_acq_rel);
  }
};

static void __fastcall HookPointGroup(void* instance, uint32_t draw_type, void* command_buffer,
                                      void* lighting_context, void* visible_lights, void* selected_shadow_lights) {
  const GroupScope scope(command_buffer, 2u);
  hooks.point(instance, draw_type, command_buffer, lighting_context, visible_lights, selected_shadow_lights);
}

static void __fastcall HookSpotGroup(void* instance, uint32_t draw_type, void* command_buffer,
                                     void* lighting_context, void* visible_lights, void* selected_shadow_lights) {
  const GroupScope scope(command_buffer, 1u);
  hooks.spot(instance, draw_type, command_buffer, lighting_context, visible_lights, selected_shadow_lights);
}

}  // namespace internal

inline Group* GetCurrentGroup(ID3D11DeviceContext* context) {
  auto* group = internal::current;
  if (group == nullptr || context == nullptr || group->context != context) return nullptr;
  return group;
}

// Copy at the shaded draw. Native Flush has uploaded these CPU constant banks
// without clearing them; the next light can overwrite them as soon as we return.
inline bool CopyConstants(ID3D11DeviceContext* context, Constants* output) {
  const auto* group = GetCurrentGroup(context);
  if (group == nullptr || output == nullptr || group->command_buffer == nullptr) return false;
  // CommandBuffer+0x28 points to eight 32-byte bank entries. Each entry holds
  // its CPU data at +8 and float4 count at +0x18. Banks 2/3 match shader b2/b3.
  const auto* shadow = *reinterpret_cast<const std::byte* const*>(static_cast<const std::byte*>(group->command_buffer) + 0x28u);
  if (shadow == nullptr) return false;
  const auto bank_count = *reinterpret_cast<const uint32_t*>(shadow + 0x108u);
  const auto active_banks = *reinterpret_cast<const uint32_t*>(shadow + 0x114u);
  if (bank_count != 8u || (active_banks & 0xCu) != 0xCu) return false;
  const auto copy_bank = [shadow](uint32_t bank, std::span<float> destination) {
    const auto* entry = shadow + (bank * 0x20u);
    const auto count = *reinterpret_cast<const uint32_t*>(entry + 0x18u);
    const auto* data = *reinterpret_cast<const float* const*>(entry + 8u);
    if (data == nullptr || count != destination.size() / 4u) return false;
    std::memcpy(destination.data(), data, destination.size_bytes());
    return true;
  };
  return copy_bank(2u, output->scene) && copy_bank(3u, output->light);
}

// Register each device once and balance successful calls with Detach. The
// callback finishes collection when a native group returns; it must not throw.
inline bool Install(CompletionCallback on_complete) {
  const std::unique_lock lock(internal::hooks.mutex);
  internal::completion_callback.store(on_complete, std::memory_order_release);
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
  if (base == nullptr) return fail("MgsvShadows: native executable is unavailable; keeping per-light shadows.");
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) return fail("MgsvShadows: native executable header validation failed; keeping per-light shadows.");
  const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
  if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.TimeDateStamp != 0x6A4CB898u
      || pe->OptionalHeader.SizeOfImage != 0xA01D000u) {
    return fail("MgsvShadows: unsupported native executable; keeping per-light shadows.");
  }

  // These installed-executable signatures contain no relative operands, so all
  // bytes are significant. Validate the CPU constant layout as well as the hooks.
  constexpr std::array<uint8_t, 26> point = {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x4C, 0x24, 0x08, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0x6C, 0x24, 0xE9};
  constexpr std::array<uint8_t, 22> spot = {0x4C, 0x8B, 0xDC, 0x49, 0x89, 0x5B, 0x10, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x49, 0x8D, 0x6B, 0x98};
  constexpr std::array<uint8_t, 16> set_vector = {0x48, 0x83, 0xEC, 0x38, 0x48, 0x8B, 0x49, 0x28, 0x4D, 0x8B, 0xD0, 0x8B, 0xC2, 0xC1, 0xF8, 0x04};
  constexpr std::array<uint8_t, 18> set_bank_vector = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xF9, 0xBB, 0x01, 0x00, 0x00, 0x00};
  constexpr std::array<uint8_t, 29> flush_bank = {0x48, 0x89, 0x4C, 0x24, 0x08, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x81, 0xEC, 0xB0, 0x00, 0x00, 0x00, 0x8B, 0x81, 0x14, 0x01, 0x00, 0x00};
  if (std::memcmp(base + 0x2FA6A0u, point.data(), point.size()) != 0
      || std::memcmp(base + 0x2FAB60u, spot.data(), spot.size()) != 0
      || std::memcmp(base + 0x2D8B70u, set_vector.data(), set_vector.size()) != 0
      || std::memcmp(base + 0x2C5F30u, set_bank_vector.data(), set_bank_vector.size()) != 0
      || std::memcmp(base + 0x2C5A10u, flush_bank.data(), flush_bank.size()) != 0) {
    return fail("MgsvShadows: native light-group validation failed; keeping per-light shadows.");
  }

  internal::hooks.point = reinterpret_cast<internal::DrawGroup>(base + 0x2FA6A0u);
  internal::hooks.spot = reinterpret_cast<internal::DrawGroup>(base + 0x2FAB60u);
  try {
    renodx::utils::detour::Install(std::array{
        renodx::utils::detour::Function(&internal::hooks.point, internal::HookPointGroup),
        renodx::utils::detour::Function(&internal::hooks.spot, internal::HookSpotGroup),
    });
  } catch (const std::system_error&) {
    internal::hooks.point = nullptr;
    internal::hooks.spot = nullptr;
    return fail("MgsvShadows: native light-group hook installation failed; keeping per-light shadows.");
  }
  internal::hooks.registrations = 1u;
  internal::hooks.logged_failure = false;
  reshade::log::message(reshade::log::level::info, "MgsvShadows: installed native point/spot group hooks.");
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
    renodx::utils::detour::Uninstall(std::array{
        renodx::utils::detour::Function(&internal::hooks.point, internal::HookPointGroup),
        renodx::utils::detour::Function(&internal::hooks.spot, internal::HookSpotGroup),
    });
  } catch (const std::system_error&) {
    reshade::log::message(reshade::log::level::warning, "MgsvShadows: native light-group hook removal failed.");
    return false;
  }
  while (internal::calls_in_flight.load(std::memory_order_acquire) != 0u) {
    SwitchToThread();
  }
  internal::hooks.registrations = 0u;
  internal::hooks.point = nullptr;
  internal::hooks.spot = nullptr;
  return true;
}

}  // namespace mgsv::shadows::native_groups

#endif  // MGSV_SHADOWS_NATIVE_GROUPS_HPP_
