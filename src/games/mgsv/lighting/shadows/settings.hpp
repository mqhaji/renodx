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

  const auto is_shadows_enhanced = [shader_injection] { return shader_injection->shadows_mode >= 0.5f; };
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
              .tooltip = "Vanilla keeps the game's shadows. Enhanced softens shadow-map edges by how far each shadow falls from its caster and adds screen-space contact shadows, for the sun, the moon and spot and point lights.",
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
              .tooltip = "Scales the size of the lights, which sets how soft shadow edges get as they fall farther from their caster. 100% gives the sun and moon their real size and a lamp a bulb 0.1 game units across. 0% keeps the game's shadow-map filtering and sharp contact shadows.",
              .max = 100.f,
              .format = "%.0f%%",
              .parse = [](float value) { return value * 0.01f; },
              .is_visible = is_shadows_enhanced,
          },
          new renodx::utils::settings::Setting{
              .key = "DirectLightingContactShadowLength",
              .binding = &shader_injection->contact_shadow_length,
              .default_value = 0.2f,
              .label = "Contact Shadow Length",
              .section = "Direct Lighting (Advanced)",
              .tooltip = "How far toward the light, in game units, each pixel looks for something on screen that shades it. Longer finds the shadows of casters farther away, at more cost when the camera is close. Each pixel's search is kept between 16 and 704 render pixels. It does not change softness.",
              .min = 0.05f,
              .max = 2.f,
              .format = "%.2f",
              .is_visible = is_shadows_enhanced,
          },
          new renodx::utils::settings::Setting{
              .key = "DirectLightingContactShadowSpotlightLimit",
              .binding = &shader_injection->contact_shadow_spotlight_limit,
              .value_type = renodx::utils::settings::SettingValueType::INTEGER,
              .default_value = 34.f,
              .label = "Contact Shadow Spotlight Limit",
              .section = "Direct Lighting (Advanced)",
              .tooltip = "How many spotlights get contact shadows in each lighting pass. They go to the spotlights that put the most light on what is in view, so close-ups keep their key lights; the others keep only their shadow-map shadows, if they have one. Point lights and the sun do not count toward it. The value also sizes the memory for contact shadows: 0 removes the count but keeps room for 13 screens of them, fewer than the default.",
              .max = 64.f,
              .format = "%d",
              .is_visible = is_shadows_enhanced,
          },
          new renodx::utils::settings::Setting{
              .key = "DirectLightingShadowMapDebugView",
              .binding = &shader_injection->shadow_map_debug_view,
              .value_type = renodx::utils::settings::SettingValueType::INTEGER,
              .default_value = 0.f,
              .label = "Shadow Map Debug View",
              .section = "Direct Lighting (Advanced)",
              .tooltip = "Replaces the Debug Light's direct light with a step of its shadow-map filter. Surface color, other lights, GI and fog still show. Blocker Distance shows distance / (1 + distance) in game units; Filter Radius is relative to its 0.25-unit cap; Map Visibility is the value handed to the game's shader. Magenta marks pixels the filter could not project, and for the sun every pixel when Shadow Softness is 0%.",
              .labels = {"Off", "Blocker Distance", "Filter Radius", "Map Visibility", "Cascade / Hemisphere"},
              .is_visible = is_shadows_enhanced,
          },
          new renodx::utils::settings::Setting{
              .key = "DirectLightingContactShadowDebugView",
              .binding = &shader_injection->contact_shadow_debug_view,
              .value_type = renodx::utils::settings::SettingValueType::INTEGER,
              .default_value = 0.f,
              .label = "Contact Shadow Debug View",
              .section = "Direct Lighting (Advanced)",
              .tooltip = "Replaces the Debug Light's direct light with contact-shadow information; other lights stay visible. Without Contact Shadows removes them from every light while their passes keep running, for comparisons. Traced Pixels marks the pixels the trace evaluated. Toward Light draws arrows pointing at the light; shadows fall the other way. Magenta marks missing inputs.",
              .labels = {"Off", "Visibility", "Depth Edges", "Ray Layout", "Without Contact Shadows", "Traced Pixels", "Toward Light"},
              .is_visible = is_shadows_enhanced,
          },
          new renodx::utils::settings::Setting{
              .key = "DirectLightingShadowDebugLight",
              .binding = &shader_injection->shadow_debug_light,
              .value_type = renodx::utils::settings::SettingValueType::INTEGER,
              .default_value = 0.f,
              .label = "Debug Light",
              .section = "Direct Lighting (Advanced)",
              .tooltip = "The light type the shadow debug views show.",
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
