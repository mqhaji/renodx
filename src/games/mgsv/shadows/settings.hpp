/*
 * Copyright (C) 2026 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Shadow controls. Bound shader data must outlive the registry callbacks.

#include <algorithm>

#include <include/reshade.hpp>

#include "../../../utils/settings.hpp"
#include "../shared.h"

namespace mgsv::shadows {

inline void AppendSettings(renodx::utils::settings::Settings* registry, ShaderInjectData* shader_injection) {
  if (std::ranges::any_of(*registry, [](auto* setting) {
        return setting->key == "FxEnhancedShadowsEnabled";
      })) return;

  const auto is_shadow_visible = [shader_injection] { return shader_injection->enhanced_shadows_enabled >= 0.5f; };
  const auto position = std::ranges::find_if(*registry, [](auto* setting) {
    return setting->section == "Options";
  });
  registry->insert(
      position,
      {
          new renodx::utils::settings::Setting{
              .key = "FxEnhancedShadowsEnabled",
              .binding = &shader_injection->enhanced_shadows_enabled,
              .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
              .default_value = 1.f,
              .label = "Enhanced Shadows",
              .section = "Shadows",
              .tooltip = "Adds soft shadow-map edges and screen-space contact detail for sun, moon, spot, and point lights. Off retains native shadows.",
              .labels = {"Off", "On"},
              .on_change_value = [](float, float value) {
                reshade::log::message(reshade::log::level::info, value >= 0.5f ? "Enhanced Shadows = On" : "Enhanced Shadows = Off");
              },
          },
          new renodx::utils::settings::Setting{
              .key = "ShadowSoftness",
              .binding = &shader_injection->shadow_softness,
              .default_value = 100.f,
              .label = "Shadow Softness",
              .section = "Shadows",
              .tooltip = "Scales shadow-map softness for all lights. 100% keeps the tuned source sizes. 0% uses native map filtering. Contact detail stays active.",
              .max = 100.f,
              .format = "%.0f%%",
              .parse = [](float value) { return value * 0.01f; },
              .is_visible = is_shadow_visible,
          },
          new renodx::utils::settings::Setting{
              .key = "FxSpotScreenSpaceShadowBudget",
              .binding = &shader_injection->ss_spot_budget,
              .value_type = renodx::utils::settings::SettingValueType::INTEGER,
              .default_value = 13.f,
              .label = "Spot Contact Shadow Budget",
              .section = "Shadows (Advanced)",
              .tooltip = "Limits added spotlight contact-shadow work. Lower values reduce cost; 0 is unlimited. Lights beyond the budget retain shadow-map shadows.",
              .max = 64.f,
              .format = "%d",
              .is_visible = is_shadow_visible,
          },
          new renodx::utils::settings::Setting{
              .key = "FxLocalScreenSpaceShadowCulling",
              .binding = &shader_injection->ss_local_culling,
              .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
              .default_value = 1.f,
              .label = "Skip Unlit Areas",
              .section = "Shadows (Advanced)",
              .tooltip = "Skips contact-shadow calculations in areas the light cannot reach, based on its cone and range.",
              .is_visible = is_shadow_visible,
          },
          new renodx::utils::settings::Setting{
              .key = "ShadowMapDebugView",
              .binding = &shader_injection->shadow_map_debug_view,
              .value_type = renodx::utils::settings::SettingValueType::INTEGER,
              .default_value = 0.f,
              .label = "Shadow Map Debug View",
              .section = "Shadows (Advanced)",
              .tooltip = "Shows the selected light's map filter in diffuse RGB. Other lighting, surface color, GI, and fog still affect it. Magenta marks unavailable coordinates. Distance uses distance / (1 + distance); radius is relative to the fixed maximum. Region identifies a sun cascade or point hemisphere.",
              .labels = {"Off", "Blocker Distance", "Filter Radius", "Map Visibility", "Region"},
              .is_visible = is_shadow_visible,
          },
          new renodx::utils::settings::Setting{
              .key = "FxScreenSpaceShadowDebugView",
              .binding = &shader_injection->ss_shadow_debug_view,
              .value_type = renodx::utils::settings::SettingValueType::INTEGER,
              .default_value = 0.f,
              .label = "Contact Shadow Debug View",
              .section = "Shadows (Advanced)",
              .tooltip = "Shows contact-shadow information for the selected light type. Other lighting remains visible. Without Contact Shadows hides their effect while keeping the calculations running. Shadow Detail shows full detail in white and skipped detail in black. Magenta marks missing inputs.",
              .labels = {"Off", "Visibility", "Depth Edges", "Ray Layout", "Without Contact Shadows", "Shadow Detail"},
              .is_visible = is_shadow_visible,
          },
          new renodx::utils::settings::Setting{
              .key = "FxScreenSpaceShadowDebugLight",
              .binding = &shader_injection->ss_shadow_debug_light,
              .value_type = renodx::utils::settings::SettingValueType::INTEGER,
              .default_value = 0.f,
              .label = "Debug Light",
              .section = "Shadows (Advanced)",
              .labels = {"Sun / Moon", "Spot", "Point"},
              .is_visible = [shader_injection] { return shader_injection->enhanced_shadows_enabled >= 0.5f && (shader_injection->shadow_map_debug_view >= 1.f || (shader_injection->ss_shadow_debug_view >= 1.f && shader_injection->ss_shadow_debug_view <= 3.f) || shader_injection->ss_shadow_debug_view == 5.f); },
          },
      });
}

}  // namespace mgsv::shadows
