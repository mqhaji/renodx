/*
 * Copyright (C) 2026 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#ifndef RENODX_GAMES_MGSV_BLOOM_HPP_
#define RENODX_GAMES_MGSV_BLOOM_HPP_

#include <algorithm>

#include <include/reshade.hpp>

#include "../../../../utils/settings.hpp"
#include "../../shared.h"

namespace mgsv::bloom {

inline void AppendSettings(renodx::utils::settings::Settings* registry, ShaderInjectData* injection) {
  if (std::ranges::any_of(*registry, [](auto* setting) { return setting->key == "FxBloomType"; })) return;
  const auto position = std::ranges::find_if(*registry, [](auto* setting) { return setting->key == "FxSunBrightnessBoostEnabled"; });
  registry->insert(
      position, {
                    new renodx::utils::settings::Setting{
                        .key = "FxBloomType",
                        .binding = &injection->bloom_type,
                        .value_type = renodx::utils::settings::SettingValueType::INTEGER,
                        .default_value = 1.f,
                        .label = "Bloom Type",
                        .section = "Effects",
                        .tooltip = "Vanilla preserves the game's bloom. Enhanced spreads linear scene light into a tighter glow.",
                        .labels = {"Vanilla", "Enhanced"},
                        .on_change_value = [](float, float current) { reshade::log::message(reshade::log::level::info, current >= 0.5f ? "Bloom: Enhanced" : "Bloom: Vanilla"); },
                    },
                    new renodx::utils::settings::Setting{
                        .key = "FxBloomStrength",
                        .binding = &injection->bloom_strength,
                        .default_value = 100.f,
                        .label = "Bloom",
                        .section = "Effects",
                        .tooltip = "Controls bloom and fullscreen lens flares. In Enhanced mode, 100 blends in 10% blurred light.",
                        .max = 100.f,
                        .parse = [](float value) { return value * 0.01f; },
                    },
                });
}

}  // namespace mgsv::bloom

#endif
