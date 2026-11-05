/*
 * Copyright (C) 2026 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#ifndef RENODX_GAMES_MGSV_RECONSTRUCTION_RESOLUTION_HPP_
#define RENODX_GAMES_MGSV_RECONSTRUCTION_RESOLUTION_HPP_

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <shared_mutex>

#include <detours.h>
#include <include/reshade.hpp>

#include "./logging.hpp"
#include "./state.hpp"

namespace mgsv::reconstruction::resolution {

enum class Quality : uint8_t {
  AUTO,
  NATIVE_AA,
  QUALITY,
  BALANCED,
  PERFORMANCE,
  ULTRA_PERFORMANCE,
};

inline constexpr std::array QUALITY_LABELS = {"Auto", "Native", "Quality", "Balance", "Performance", "Ultra Performance"};

struct Size {
  uint32_t width = 0u;
  uint32_t height = 0u;
  bool operator==(const Size&) const = default;
};
static_assert(sizeof(Size) == 8u);

struct Plan {
  Size render;
  Size output;
  Quality quality = Quality::NATIVE_AA;
  state::TemporalMode mode = state::TemporalMode::OFF;
  bool operator==(const Plan&) const = default;
  [[nodiscard]] bool IsScaled() const { return render.width != 0u && output.width != 0u && render != output; }
  [[nodiscard]] float MipBias(Size actual) const {
    if ((mode != state::TemporalMode::AMD_FSR3 && mode != state::TemporalMode::NVIDIA_DLSS)
        || actual.width == 0u || output.width == 0u) return 0.f;
    return std::log2(static_cast<float>(actual.width) / static_cast<float>(output.width)) - 1.f;
  }
  [[nodiscard]] uint32_t JitterPhaseCount(Size actual) const {
    if (!IsScaled() || actual.width == 0u) return state::BASE_JITTER_PHASE_COUNT;
    const double ratio = static_cast<double>(output.width) / static_cast<double>(actual.width);
    // Each render pixel covers more output pixels. Round up to give that area enough samples.
    return std::max(state::BASE_JITTER_PHASE_COUNT,
                    static_cast<uint32_t>(std::ceil(static_cast<double>(state::BASE_JITTER_PHASE_COUNT) * ratio * ratio)));
  }
};

struct State {
  std::shared_mutex mutex;
  Plan requested;
  Size actual;
  uint64_t generation = 0u;
  uint8_t* renderer = nullptr;  // Borrowed identity; dereferenced only during its own native step.
  Size engine_wanted;
  Size last_written;
};
inline State sizing_state;
inline std::atomic<Quality> quality = Quality::AUTO;
inline std::atomic<bool> installed = false;
inline std::atomic<bool> failed = false;
inline std::atomic<float> mip_bias = 0.f;

inline Plan Read() {
  const std::shared_lock lock(sizing_state.mutex);
  return sizing_state.requested;
}

inline Size ReadActual() {
  const std::shared_lock lock(sizing_state.mutex);
  return sizing_state.actual;
}

inline Plan MakePlan(Size output, state::TemporalMode mode, Quality selected) {
  Plan result = {.render = output, .output = output, .mode = mode};
  if ((mode != state::TemporalMode::AMD_FSR3 && mode != state::TemporalMode::NVIDIA_DLSS)
      || selected == Quality::NATIVE_AA || output.width == 0u || output.height == 0u) return result;
  if (selected == Quality::AUTO) {
    // NVIDIA's UI guidelines choose a preset by total output pixels, not frame rate.
    // Resolve it here so backend queries and resource caches receive a concrete mode.
    const uint64_t pixels = static_cast<uint64_t>(output.width) * output.height;
    if (pixels < 1920u * 1080u) return result;
    if (pixels <= 2560u * 1440u) {
      selected = Quality::QUALITY;
    } else if (pixels <= 3840u * 2160u) {
      selected = Quality::PERFORMANCE;
    } else {
      selected = Quality::ULTRA_PERFORMANCE;
    }
  }
  constexpr std::array divisors = {1.f, 1.f, 1.5f, 1.7f, 2.f, 3.f};
  const auto index = static_cast<size_t>(selected);
  if (index >= divisors.size()) return result;
  result.quality = selected;
  result.render.width = std::min(output.width, (static_cast<uint32_t>(static_cast<float>(output.width) / divisors[index]) + 1u) & ~1u);
  result.render.height = std::min(output.height, (static_cast<uint32_t>(static_cast<float>(output.height) / divisors[index]) + 1u) & ~1u);
  return result;
}

// A failure restores native sizing at the renderer's next update. It never
// rebuilds resources from inside a draw or a settings callback.
inline void Fail(const char* reason) {
  if (!failed.exchange(true, std::memory_order_acq_rel)) {
    logging::Warn("upscaling disabled: ", reason);
  }
}

using RendererStep = void(__fastcall*)(uint8_t* renderer);
using DofDraw = void(__fastcall*)(void* effect, const float* parameters);
using DofDimensions = void(__fastcall*)(void* effect, uint32_t width, uint32_t height);
using DofSprites = void(__fastcall*)(void* effect, uint32_t width, uint32_t height, uint8_t level, char near_layer,
                                     float parameter6, float parameter7, float parameter8, float parameter9,
                                     float parameter10, float parameter11);
inline RendererStep renderer_step = nullptr;
inline DofDraw dof_draw = nullptr;
inline DofDimensions dof_dimensions = nullptr;
inline DofSprites dof_sprites = nullptr;
inline thread_local Plan dof_plan;
inline thread_local uint64_t dof_token = 0u;
inline thread_local uint64_t dof_sequence = 0u;
inline uintptr_t renderer_step_entry = 0u;

inline void __fastcall HookRendererStep(uint8_t* renderer) {
  // Preserve the selected renderer's own size requests, including display
  // changes. Unrelated renderer instances must not replace its actual extent.
  auto* wanted = reinterpret_cast<Size*>(renderer + 0x188u);
  auto* flags = reinterpret_cast<uint32_t*>(renderer + 0x198u);
  const auto display = *reinterpret_cast<const Size*>(renderer + 0x180u);
  const auto* table = *reinterpret_cast<const uintptr_t* const*>(renderer);
  bool selected = false;
  {
    const std::unique_lock lock(sizing_state.mutex);
    const auto& plan = sizing_state.requested;
    if (sizing_state.renderer == nullptr && table != nullptr && table[6] == renderer_step_entry
        && display.width != 0u && display == plan.output) {
      sizing_state.renderer = renderer;
      sizing_state.engine_wanted = *wanted;
      sizing_state.last_written = *wanted;
    }
    selected = sizing_state.renderer == renderer;
    if (selected) {
      if (*wanted != sizing_state.last_written) {
        sizing_state.engine_wanted = *wanted;
      }
      const bool scaled = installed.load(std::memory_order_acquire) && !failed.load(std::memory_order_acquire)
                          && state::IsEnabled() && plan.IsScaled() && plan.output == display;
      const auto desired = (scaled ? plan.render : sizing_state.engine_wanted);
      if ((*flags & 4u) == 0u && desired.width != 0u && desired.height != 0u && *wanted != desired) {
        *wanted = desired;
        *flags |= 2u;
      }
      sizing_state.last_written = *wanted;
    }
  }
  renderer_step(renderer);
  if (!selected) return;
  const auto actual = *reinterpret_cast<const Size*>(renderer + 0x190u);
  const std::unique_lock lock(sizing_state.mutex);
  if (sizing_state.renderer == renderer && sizing_state.actual != actual) {
    sizing_state.actual = actual;
    ++sizing_state.generation;
  }
  mip_bias.store(failed.load(std::memory_order_acquire) ? 0.f : sizing_state.requested.MipBias(actual), std::memory_order_release);
  // Publish the new phase count before camera setup uses the resized scene.
  state::runtime_jitter_phases.store(sizing_state.requested.JitterPhaseCount(actual), std::memory_order_release);
}

inline void __fastcall HookDofDraw(void* effect, const float* parameters) {
  Plan plan;
  Size actual;
  {
    const std::shared_lock lock(sizing_state.mutex);
    plan = sizing_state.requested;
    actual = sizing_state.actual;
  }
  const auto previous = dof_plan;
  const uint64_t previous_token = dof_token;
  dof_token = ++dof_sequence;
  dof_plan = {};
  if (plan.IsScaled() && plan.render == actual && !failed.load(std::memory_order_acquire)) {
    const auto flags = std::bit_cast<uint32_t>(parameters[8]);
    if ((flags & 8u) != 0u) {
      dof_plan = plan;
      std::array<float, 9> copied;
      std::memcpy(copied.data(), parameters, sizeof(copied));
      const float scale = static_cast<float>(plan.output.width) / static_cast<float>(plan.render.width);
      // The native scatter converts these physical blur terms into pixels
      // using render width. Supply output-pixel terms without changing its view.
      copied[0] *= scale;
      copied[5] *= scale;
      copied[6] *= scale;
      if ((flags & 1u) != 0u) {
        // Bit 1 only enables the native near-radius cap of width/1280*6.
        // Apply that cap in output units and prevent a second render-size cap.
        copied[5] = std::min(parameters[5], 6.f) * scale;
        copied[8] = std::bit_cast<float>(flags & ~1u);
      }
      dof_draw(effect, copied.data());
      dof_plan = previous;
      dof_token = previous_token;
      return;
    }
    Fail("unsupported native depth-of-field path");
  }
  dof_draw(effect, parameters);
  dof_plan = previous;
  dof_token = previous_token;
}

inline void __fastcall HookDofDimensions(void* effect, uint32_t width, uint32_t height) {
  if (dof_plan.IsScaled() && Size{.width = width, .height = height} == dof_plan.render) {
    width = dof_plan.output.width;
    height = dof_plan.output.height;
  }
  dof_dimensions(effect, width, height);
}

inline void __fastcall HookDofSprites(void* effect, uint32_t width, uint32_t height, uint8_t level, char near_layer,
                                      float parameter6, float parameter7, float parameter8, float parameter9,
                                      float parameter10, float parameter11) {
  if (dof_plan.IsScaled() && Size{.width = width, .height = height} == dof_plan.render) {
    width = dof_plan.output.width;
    height = dof_plan.output.height;
  }
  dof_sprites(effect, width, height, level, near_layer, parameter6, parameter7, parameter8, parameter9, parameter10, parameter11);
}

inline void OnInitDevice(reshade::api::device* device) {
  if (device->get_api() != reshade::api::device_api::d3d11 || installed.load(std::memory_order_acquire)) return;
  auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
  const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
  if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.TimeDateStamp != 0x6A4CB898u
      || pe->OptionalHeader.SizeOfImage != 0xA01D000u) return;
  constexpr std::array<uint8_t, 15> step = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xD9, 0x8B, 0x89, 0x98, 0x01, 0x00, 0x00};
  constexpr std::array<uint8_t, 16> draw = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48};
  constexpr std::array<uint8_t, 16> dimensions = {0x48, 0x89, 0x5C, 0x24, 0x20, 0x48, 0x89, 0x4C, 0x24, 0x08, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41};
  constexpr std::array<uint8_t, 16> sprites = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56};
  if (std::memcmp(base + 0x24C220u, step.data(), step.size()) != 0
      || std::memcmp(base + 0x2F1780u, draw.data(), draw.size()) != 0
      || std::memcmp(base + 0x2F64A0u, dimensions.data(), dimensions.size()) != 0
      || std::memcmp(base + 0x2F5D70u, sprites.data(), sprites.size()) != 0
      || *reinterpret_cast<const float*>(base + 0x20AAE50u) != 6.f
      || *reinterpret_cast<const float*>(base + 0x211E04Cu) != 0.85f
      || *reinterpret_cast<const float*>(base + 0x211E048u) != (1.f / 1920.f)) {
    logging::Warn("native upscaling executable validation failed");
    return;
  }
  renderer_step = reinterpret_cast<RendererStep>(base + 0x24C220u);
  renderer_step_entry = reinterpret_cast<uintptr_t>(base + 0x24C220u);
  dof_draw = reinterpret_cast<DofDraw>(base + 0x2F1780u);
  dof_dimensions = reinterpret_cast<DofDimensions>(base + 0x2F64A0u);
  dof_sprites = reinterpret_cast<DofSprites>(base + 0x2F5D70u);
  HMODULE pinned = nullptr;
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                         reinterpret_cast<LPCWSTR>(&HookRendererStep), &pinned)
      == FALSE) return;
  if (DetourTransactionBegin() != NO_ERROR) return;
  if (DetourUpdateThread(GetCurrentThread()) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&renderer_step), reinterpret_cast<void*>(HookRendererStep)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&dof_draw), reinterpret_cast<void*>(HookDofDraw)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&dof_dimensions), reinterpret_cast<void*>(HookDofDimensions)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&dof_sprites), reinterpret_cast<void*>(HookDofSprites)) != NO_ERROR) {
    DetourTransactionAbort();
    return;
  }
  if (DetourTransactionCommit() != NO_ERROR) return;
  installed.store(true, std::memory_order_release);
  logging::Info("installed native render-size and output-size DoF hooks");
}

inline void Use(DWORD reason) {
  if (reason == DLL_PROCESS_ATTACH) {
    reshade::register_event<reshade::addon_event::init_device>(OnInitDevice);
  } else if (reason == DLL_PROCESS_DETACH) {
    reshade::unregister_event<reshade::addon_event::init_device>(OnInitDevice);
    // Pinned native code and its trampolines remain valid until process exit.
  }
}

}  // namespace mgsv::reconstruction::resolution

#endif  // RENODX_GAMES_MGSV_RECONSTRUCTION_RESOLUTION_HPP_
