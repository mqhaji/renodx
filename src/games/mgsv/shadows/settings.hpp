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
              .tooltip = "Scales the light size for shadow-map and contact-edge filtering. 100% keeps the tuned source sizes. 0% uses native map filtering and sharp contact detail. Contact rays keep the native light position.",
              .max = 100.f,
              .format = "%.0f%%",
              .parse = [](float value) { return value * 0.01f; },
              .is_visible = is_shadow_visible,
          },
          new renodx::utils::settings::Setting{
              .key = "FxScreenSpaceShadowReach",
              .binding = &shader_injection->ss_shadow_reach,
              .value_type = renodx::utils::settings::SettingValueType::INTEGER,
              .default_value = 60.f,
              .label = "Contact Shadow Reach",
              .section = "Shadows (Advanced)",
              .tooltip = "Search length in output pixels along the ray's main screen axis, shared by all lights. Converted to render pixels when upscaling, with a minimum of 16 render pixels. Higher values find farther visible casters and increase cost. This changes reach, not sample density.",
              .min = 16.f,
              .max = 256.f,
              .format = "%d px",
              .is_visible = is_shadow_visible,
          },
          new renodx::utils::settings::Setting{
              .key = "FxSpotScreenSpaceShadowBudget",
              .binding = &shader_injection->ss_spot_budget,
              .value_type = renodx::utils::settings::SettingValueType::INTEGER,
              .default_value = 34.f,
              .label = "Spot Contact Shadow Budget",
              .section = "Shadows (Advanced)",
              .tooltip = "Limits added spotlight contact-shadow work. Lower values reduce cost; 0 is unlimited. Lights beyond the budget retain shadow-map shadows.",
              .max = 64.f,
              .format = "%d",
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
              .tooltip = "Shows contact-shadow information for the selected light type. Other lighting remains visible. Without Contact Shadows hides their effect while keeping the calculations running. Shadow Detail shows evaluated areas. Toward Light draws arrows pointing toward the light; cast shadows extend away from it. Magenta marks missing inputs.",
              .labels = {"Off", "Visibility", "Depth Edges", "Ray Layout", "Without Contact Shadows", "Shadow Detail", "Toward Light"},
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
              .is_visible = [shader_injection] {
                return shader_injection->enhanced_shadows_enabled >= 0.5f
                       && (shader_injection->shadow_map_debug_view >= 1.f
                           || (shader_injection->ss_shadow_debug_view >= 1.f && shader_injection->ss_shadow_debug_view <= 3.f)
                           || shader_injection->ss_shadow_debug_view == 5.f
                           || shader_injection->ss_shadow_debug_view == 6.f);
              },
          },
      });
}

}  // namespace mgsv::shadows
