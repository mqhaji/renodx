/*
 * Copyright (C) 2026 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <include/reshade.hpp>

#include "../../../mods/swapchain_v2.hpp"
#include "../../../utils/settings.hpp"
#include "../../../utils/swapchain.hpp"
#include "../shared.h"

namespace mgsv::output {

// Output mode (SDR or HDR) and the brightness settings it controls. addon.cpp owns
// this state; settings.hpp fills in the setting pointers when it builds the menu.
struct State {
  float mode = 1.f;
  renodx::utils::settings::Setting* peak_nits_setting = nullptr;
  renodx::utils::settings::Setting* game_nits_setting = nullptr;
  renodx::utils::settings::Setting* ui_nits_setting = nullptr;
  bool peak_detected = false;
};

inline constexpr float SDR_WHITE_NITS = 80.f;

inline bool IsHDROutputSelected(const State& state) { return state.mode != 0.f; }

inline void SyncOutputMode(ShaderInjectData* shader_injection, const State& state) {
  if (IsHDROutputSelected(state)) {
    shader_injection->peak_white_nits = state.peak_nits_setting->GetValue();
    shader_injection->diffuse_white_nits = state.game_nits_setting->GetValue();
    shader_injection->graphics_white_nits = state.ui_nits_setting->GetValue();
    shader_injection->swap_chain_output_preset = 1.f;
  } else {
    shader_injection->peak_white_nits = SDR_WHITE_NITS;
    shader_injection->diffuse_white_nits = SDR_WHITE_NITS;
    shader_injection->graphics_white_nits = SDR_WHITE_NITS;
    shader_injection->swap_chain_output_preset = 0.f;
  }
}

// The first swapchain sets the Peak Brightness default: the display's reported
// peak, or 100 nits when RenoDX upgraded the swapchain.
inline void OnInitSwapchain(State* state, reshade::api::swapchain* swapchain) {
  if (state->peak_detected) return;
  state->peak_detected = true;
  auto peak = renodx::utils::swapchain::GetPeakNits(swapchain);
  if (peak.has_value()) {
    state->peak_nits_setting->default_value = peak.value();
    state->peak_nits_setting->can_reset = true;
  }
  if (renodx::mods::swapchain::v2::IsUpgraded(swapchain)) {
    state->peak_nits_setting->default_value = 100.f;
  }
}

}  // namespace mgsv::output
