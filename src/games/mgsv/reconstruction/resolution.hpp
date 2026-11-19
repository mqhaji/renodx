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
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <shared_mutex>

#include <detours.h>
#include <include/reshade.hpp>

#include "../engine/build.hpp"
#include "../engine/image.hpp"
#include "../engine/views.hpp"
#include "logging.hpp"
#include "state.hpp"

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
    if ((mode != state::TemporalMode::AMD_FSR && mode != state::TemporalMode::NVIDIA_DLSS)
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
  uintptr_t backend = 0u;  // Borrowed identity; read only during its native Start call.
  uint64_t presentation_generation = 0u;
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
  if ((mode != state::TemporalMode::AMD_FSR && mode != state::TemporalMode::NVIDIA_DLSS)
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

using BackendStep = void(__fastcall*)(uint8_t* backend);
using DofDraw = void(__fastcall*)(void* effect, const float* parameters);
using DofDimensions = void(__fastcall*)(void* resources, uint32_t width, uint32_t height);
using DofSprites = void(__fastcall*)(void* effect, uint32_t width, uint32_t height, uint8_t level, char near_layer,
                                     float parameter6, float parameter7, float parameter8, float parameter9,
                                     float parameter10, float parameter11);
inline BackendStep backend_step = nullptr;
inline DofDraw dof_draw = nullptr;
inline DofDimensions dof_dimensions = nullptr;
inline DofSprites dof_sprites = nullptr;
inline uintptr_t backend_step_entry = 0u;

// The hooks, and the constants the native depth-of-field scatter is
// rescaled around at output size.
inline constexpr std::array REQUIRED_ENTRIES = {
    engine::Entry::DG_START,
    engine::Entry::DEPTH_OF_FIELD_DRAW,
    engine::Entry::DEPTH_OF_FIELD_SET_RENDER_AREA,
    engine::Entry::DEPTH_OF_FIELD_DRAW_SPRITE_SUB,
    engine::Entry::DEPTH_OF_FIELD_NEAR_RADIUS_CAP,
    engine::Entry::DEPTH_OF_FIELD_SCATTER_SCALE,
    engine::Entry::DEPTH_OF_FIELD_REFERENCE_WIDTH,
};
inline constexpr engine::Requirement REQUIREMENT = {.feature = "Upscaling (DLSS and FSR below native resolution)", .entries = REQUIRED_ENTRIES};

// These fields belong to the shared Dg backend. Its Start call applies size
// requests before the game begins building views for the next render.
inline constexpr size_t BACKEND_WANTED_SIZE_OFFSET = 0x188u;
inline constexpr size_t BACKEND_FLAGS_OFFSET = 0x198u;
inline constexpr uint32_t BACKEND_RESIZE_FLAG = 2u;
inline constexpr uint32_t BACKEND_BUSY_FLAG = 4u;
inline constexpr size_t BACKEND_START_SLOT = 6u;

struct DofDrawState {
  Plan plan;
  uintptr_t effect = 0u;
  uint64_t invocation = 0u;
  uint64_t scene = 0u;
  uint64_t generation = 0u;

  [[nodiscard]] bool IsScaled() const { return plan.IsScaled(); }
  [[nodiscard]] bool Matches(const engine::views::Owner& owner) const {
    return IsScaled() && scene != 0u && scene == owner.scene && generation == owner.generation;
  }
};
static thread_local DofDrawState active_dof;

inline void __fastcall HookBackendStep(uint8_t* backend) {
  const auto presentation = engine::views::GetPresentationOwner();
  const auto backend_address = reinterpret_cast<uintptr_t>(backend);
  auto* wanted = reinterpret_cast<Size*>(backend + BACKEND_WANTED_SIZE_OFFSET);
  auto* flags = reinterpret_cast<uint32_t*>(backend + BACKEND_FLAGS_OFFSET);
  const auto display = *reinterpret_cast<const Size*>(backend + engine::views::OUTPUT_SIZE_OFFSET);
  const auto* table = *reinterpret_cast<const uintptr_t* const*>(backend);
  bool managed = false;
  bool acquired = false;
  {
    const std::unique_lock lock(sizing_state.mutex);
    const auto& plan = sizing_state.requested;
    // A native-size scene establishes the swapchain/backend association during
    // startup. Apply its scale at the next Start, without a settings toggle.
    const bool selected = presentation.backend == backend_address && presentation.generation != 0u
                          && table != nullptr && table[BACKEND_START_SLOT] == backend_step_entry;
    if (selected && sizing_state.backend != backend_address) {
      sizing_state.backend = backend_address;
      sizing_state.engine_wanted = *wanted;
      sizing_state.last_written = *wanted;
    }
    managed = sizing_state.backend == backend_address;
    if (managed) {
      acquired = selected && sizing_state.presentation_generation != presentation.generation;
      sizing_state.presentation_generation = (selected ? presentation.generation : 0u);
      if (*wanted != sizing_state.last_written) {
        sizing_state.engine_wanted = *wanted;
      }
      const bool scaled = selected && installed.load(std::memory_order_acquire) && !failed.load(std::memory_order_acquire)
                          && state::IsEnabled() && plan.IsScaled() && plan.output == display;
      // Preserve the game's own request across resize, recovery, and a lost
      // presentation association. Only a backend we already own is restored.
      const auto desired = (scaled ? plan.render : sizing_state.engine_wanted);
      if ((*flags & BACKEND_BUSY_FLAG) == 0u && desired.width != 0u && desired.height != 0u && *wanted != desired) {
        *wanted = desired;
        *flags |= BACKEND_RESIZE_FLAG;
      }
      sizing_state.last_written = *wanted;
    }
  }
  if (acquired) {
    logging::Info("native scaling backend matched presentation generation=", presentation.generation,
                  " render=", presentation.render_id, " view=", presentation.view_id);
  }
  backend_step(backend);
  if (!managed) return;
  const auto actual = *reinterpret_cast<const Size*>(backend + engine::views::RENDER_SIZE_OFFSET);
  const std::unique_lock lock(sizing_state.mutex);
  if (sizing_state.backend != backend_address) return;
  if (sizing_state.actual != actual) {
    sizing_state.actual = actual;
    ++sizing_state.generation;
  }
  mip_bias.store(failed.load(std::memory_order_acquire) ? 0.f : sizing_state.requested.MipBias(actual), std::memory_order_release);
  // Publish the new phase count before camera setup uses the resized scene.
  state::runtime_jitter_phases.store(sizing_state.requested.JitterPhaseCount(actual), std::memory_order_release);
}

inline void __fastcall HookDofDraw(void* effect, const float* parameters) {
  const auto scope = engine::views::GetDofScope();
  const auto& owner = scope.scope.owner;
  Plan plan;
  {
    const std::shared_lock lock(sizing_state.mutex);
    // MainExec supplies the viewport and camera. Dimensions then check that
    // its scene was built with the plan currently applied to this backend.
    if (scope.matches_view && scope.effect == reinterpret_cast<uintptr_t>(effect)
        && owner.main_view && owner.active_member && owner.presentation_matches
        && owner.backend == sizing_state.backend && owner.generation == sizing_state.presentation_generation
        && sizing_state.requested.render == sizing_state.actual
        && owner.render_size == std::array{sizing_state.actual.width, sizing_state.actual.height}
        && owner.output_size == std::array{sizing_state.requested.output.width, sizing_state.requested.output.height}) {
      plan = sizing_state.requested;
    }
  }
  const auto previous = active_dof;
  active_dof = {};
  std::array<float, 9> copied;
  const float* draw_parameters = parameters;
  if (plan.IsScaled() && !failed.load(std::memory_order_acquire)) {
    constexpr size_t FLAGS_INDEX = 8u;
    constexpr uint32_t SCATTER_FLAG = 8u;
    constexpr uint32_t NEAR_RADIUS_CAP_FLAG = 1u;
    constexpr float NEAR_RADIUS_CAP = 6.f;
    const auto flags = std::bit_cast<uint32_t>(parameters[FLAGS_INDEX]);
    if ((flags & SCATTER_FLAG) != 0u) {
      active_dof = {.plan = plan, .effect = scope.effect, .invocation = scope.invocation, .scene = owner.scene, .generation = owner.generation};
      std::memcpy(copied.data(), parameters, sizeof(copied));
      const float scale = static_cast<float>(plan.output.width) / static_cast<float>(plan.render.width);
      // The native scatter converts these physical blur terms into pixels
      // using render width. Supply output-pixel terms without changing its view.
      copied[0] *= scale;
      copied[5] *= scale;
      copied[6] *= scale;
      if ((flags & NEAR_RADIUS_CAP_FLAG) != 0u) {
        // The near-radius flag enables the native cap of width/1280*6.
        // Apply that cap in output units and prevent a second render-size cap.
        copied[5] = std::min(parameters[5], NEAR_RADIUS_CAP) * scale;
        copied[FLAGS_INDEX] = std::bit_cast<float>(flags & ~NEAR_RADIUS_CAP_FLAG);
      }
      draw_parameters = copied.data();
    } else {
      Fail("unsupported native depth-of-field path");
    }
  }
  dof_draw(effect, draw_parameters);
  active_dof = previous;
}

inline void __fastcall HookDofDimensions(void* resources, uint32_t width, uint32_t height) {
  // DrawScatterDof passes its shared DgResources here. The enclosing admitted
  // draw identifies the scene whose DoF rectangles need output dimensions.
  if (active_dof.IsScaled() && Size{.width = width, .height = height} == active_dof.plan.render) {
    width = active_dof.plan.output.width;
    height = active_dof.plan.output.height;
  }
  dof_dimensions(resources, width, height);
}

inline void __fastcall HookDofSprites(void* effect, uint32_t width, uint32_t height, uint8_t level, char near_layer,
                                      float parameter6, float parameter7, float parameter8, float parameter9,
                                      float parameter10, float parameter11) {
  if (active_dof.IsScaled() && active_dof.effect == reinterpret_cast<uintptr_t>(effect)
      && Size{.width = width, .height = height} == active_dof.plan.render) {
    width = active_dof.plan.output.width;
    height = active_dof.plan.output.height;
  }
  dof_sprites(effect, width, height, level, near_layer, parameter6, parameter7, parameter8, parameter9, parameter10, parameter11);
}

inline void OnInitDevice(reshade::api::device* device) {
  if (device->get_api() != reshade::api::device_api::d3d11 || installed.load(std::memory_order_acquire)) return;
  if (!engine::Verified(REQUIRED_ENTRIES)) {
    logging::Warn("the upscaling entries did not verify; upscaling is off");
    return;
  }
  backend_step = reinterpret_cast<BackendStep>(engine::Address(engine::Entry::DG_START));
  backend_step_entry = reinterpret_cast<uintptr_t>(engine::Address(engine::Entry::DG_START));
  dof_draw = reinterpret_cast<DofDraw>(engine::Address(engine::Entry::DEPTH_OF_FIELD_DRAW));
  dof_dimensions = reinterpret_cast<DofDimensions>(engine::Address(engine::Entry::DEPTH_OF_FIELD_SET_RENDER_AREA));
  dof_sprites = reinterpret_cast<DofSprites>(engine::Address(engine::Entry::DEPTH_OF_FIELD_DRAW_SPRITE_SUB));
  HMODULE pinned = nullptr;
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                         reinterpret_cast<LPCWSTR>(&HookBackendStep), &pinned)
      == FALSE) return;
  if (DetourTransactionBegin() != NO_ERROR) return;
  if (DetourUpdateThread(GetCurrentThread()) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&backend_step), reinterpret_cast<void*>(HookBackendStep)) != NO_ERROR
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
