/*
 * Copyright (C) 2026 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Shadow controls. Bound shader data must outlive the registry callbacks.

#include <algorithm>

#include <include/reshade.hpp>

#include "../../../../utils/settings.hpp"
#include "../../shared.h"

namespace mgsv::shadows {

inline void AppendSettings(renodx::utils::settings::Settings* registry, ShaderInjectData* shader_injection) {
  if (std::ranges::any_of(*registry, [](auto* setting) {
        return setting->key == "DirectLightingShadowsMode";
      })) return;

  const auto is_shadow_visible = [shader_injection] { return shader_injection->shadows_mode >= 0.5f; };
  const auto position = std::ranges::find_if(*registry, [](auto* setting) {
    return setting->section == "Options";
  });
  registry->insert(
      position,
      {
          new renodx::utils::settings::Setting{
              .key = "DirectLightingShadowsMode",
              .binding = &shader_injection->shadows_mode,
              .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
              .default_value = 1.f,
              .label = "Shadows",
              .section = "Direct Lighting",
              .tooltip = "Vanilla keeps the game's original shadows. Enhanced adds soft shadow-map edges and screen-space contact detail for sun, moon, spot, and point lights.",
              .labels = {"Vanilla", "Enhanced"},
              .on_change_value = [](float, float value) {
                reshade::log::message(reshade::log::level::info, value >= 0.5f ? "Shadows = Enhanced" : "Shadows = Vanilla");
              },
          },
          new renodx::utils::settings::Setting{
              .key = "DirectLightingShadowSoftness",
              .binding = &shader_injection->shadow_softness,
              .default_value = 100.f,
              .label = "Shadow Softness",
              .section = "Direct Lighting (Advanced)",
              .tooltip = "Scales the light size for shadow-map and contact-edge filtering. 100% keeps the tuned source sizes. 0% uses native map filtering and sharp contact detail. Contact rays keep the native light position.",
              .max = 100.f,
              .format = "%.0f%%",
              .parse = [](float value) { return value * 0.01f; },
              .is_visible = is_shadow_visible,
          },
          new renodx::utils::settings::Setting{
              .key = "DirectLightingContactShadowReach",
              .binding = &shader_injection->contact_shadow_reach,
              .default_value = 0.1f,
              .label = "Contact Shadow Reach",
              .section = "Direct Lighting (Advanced)",
              .tooltip = "Search length in game units toward the light, shared by all lights. Each pixel converts it to screen pixels at its own depth, so a contact shadow keeps its length as the camera moves, between 16 and 256 render pixels. Higher values find farther visible casters and cost more up close. This changes reach, not sample density.",
              .min = 0.05f,
              .max = 2.f,
              .format = "%.2f",
              .is_visible = is_shadow_visible,
          },
          new renodx::utils::settings::Setting{
              .key = "DirectLightingSpotContactShadowBudget",
              .binding = &shader_injection->spot_contact_shadow_budget,
              .value_type = renodx::utils::settings::SettingValueType::INTEGER,
              .default_value = 34.f,
              .label = "Spot Contact Shadow Budget",
              .section = "Direct Lighting (Advanced)",
              .tooltip = "Limits added spotlight contact-shadow work. Lower values reduce cost; 0 is unlimited. Lights beyond the budget retain shadow-map shadows.",
              .max = 64.f,
              .format = "%d",
              .is_visible = is_shadow_visible,
          },
          new renodx::utils::settings::Setting{
              .key = "DirectLightingShadowMapDebugView",
              .binding = &shader_injection->shadow_map_debug_view,
              .value_type = renodx::utils::settings::SettingValueType::INTEGER,
              .default_value = 0.f,
              .label = "Shadow Map Debug View",
              .section = "Direct Lighting (Advanced)",
              .tooltip = "Shows the selected light's map filter in diffuse RGB. Other lighting, surface color, GI, and fog still affect it. Magenta marks unavailable coordinates. Distance uses distance / (1 + distance); radius is relative to the fixed maximum. Region identifies a sun cascade or point hemisphere.",
              .labels = {"Off", "Blocker Distance", "Filter Radius", "Map Visibility", "Region"},
              .is_visible = is_shadow_visible,
          },
          new renodx::utils::settings::Setting{
              .key = "DirectLightingContactShadowDebugView",
              .binding = &shader_injection->contact_shadow_debug_view,
              .value_type = renodx::utils::settings::SettingValueType::INTEGER,
              .default_value = 0.f,
              .label = "Contact Shadow Debug View",
              .section = "Direct Lighting (Advanced)",
              .tooltip = "Shows contact-shadow information for the selected light type. Other lighting remains visible. Without Contact Shadows hides their effect while keeping the calculations running. Shadow Detail shows evaluated areas. Toward Light draws arrows pointing toward the light; cast shadows extend away from it. Magenta marks missing inputs.",
              .labels = {"Off", "Visibility", "Depth Edges", "Ray Layout", "Without Contact Shadows", "Shadow Detail", "Toward Light"},
              .is_visible = is_shadow_visible,
          },
          new renodx::utils::settings::Setting{
              .key = "DirectLightingShadowDebugLight",
              .binding = &shader_injection->shadow_debug_light,
              .value_type = renodx::utils::settings::SettingValueType::INTEGER,
              .default_value = 0.f,
              .label = "Debug Light",
              .section = "Direct Lighting (Advanced)",
              .labels = {"Sun / Moon", "Spot", "Point"},
              .is_visible = [shader_injection] {
                return shader_injection->shadows_mode >= 0.5f
                       && (shader_injection->shadow_map_debug_view >= 1.f
                           || (shader_injection->contact_shadow_debug_view >= 1.f && shader_injection->contact_shadow_debug_view <= 3.f)
                           || shader_injection->contact_shadow_debug_view == 5.f
                           || shader_injection->contact_shadow_debug_view == 6.f);
              },
          },
      });
}

}  // namespace mgsv::shadows
