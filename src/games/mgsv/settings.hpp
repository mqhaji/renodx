/*
 * Copyright (C) 2024 Musa Haji
 * Copyright (C) 2026 ZioPao
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Core controls and cross-feature preset policy. Each registry stays bound to its original state.
// Bound state and the registry must outlive the callbacks.

#include <algorithm>
#include <mutex>
#include <string>
#include <utility>

#include "../../utils/date.hpp"
#include "../../utils/mutex.hpp"
#include "../../utils/platform.hpp"
#include "../../utils/settings.hpp"
#include "./output/output.hpp"
#include "./shared.h"

namespace mgsv::settings {

inline void AppendSettings(renodx::utils::settings::Settings* registry, ShaderInjectData* shader_injection,
                           mgsv::output::State* output_state) {
  if (std::ranges::any_of(*registry, [](auto* setting) {
        return setting->key == "OutputMode";
      })) return;

  registry->insert(
      registry->end(),
      {
          new renodx::utils::settings::Setting{
              .key = "OutputMode",
              .binding = &output_state->mode,
              .value_type = renodx::utils::settings::SettingValueType::INTEGER,
              .default_value = 1.f,
              .can_reset = false,
              .label = "Output Mode",
              .section = "Output",
              .tooltip = "Selects SDR gamma 2.2 or HDR10 PQ output.",
              .labels = {"SDR", "HDR"},
              .on_change_value = [shader_injection, output_state](float, float) {
                mgsv::output::SyncOutputMode(shader_injection, *output_state);
              },
          },
          output_state->peak_nits_setting = new renodx::utils::settings::Setting{
              .key = "ToneMapPeakNits",
              .binding = &shader_injection->peak_white_nits,
              .default_value = 1000.f,
              .can_reset = false,
              .label = "Peak Brightness",
              .section = "Output",
              .tooltip = "Sets the value of peak white in nits",
              .min = 48.f,
              .max = 4000.f,
              .is_enabled = [shader_injection, output_state]() {
                return mgsv::output::IsHDROutputSelected(*output_state) && shader_injection->tone_map_type != 0.f;
              },
          },
          output_state->game_nits_setting = new renodx::utils::settings::Setting{
              .key = "ToneMapGameNits",
              .binding = &shader_injection->diffuse_white_nits,
              .default_value = 203.f,
              .label = "Game Brightness",
              .section = "Output",
              .tooltip = "Sets the value of 100% white in nits",
              .min = 48.f,
              .max = 500.f,
              .is_enabled = [output_state]() { return mgsv::output::IsHDROutputSelected(*output_state); },
          },
          output_state->ui_nits_setting = new renodx::utils::settings::Setting{
              .key = "ToneMapUINits",
              .binding = &shader_injection->graphics_white_nits,
              .default_value = 203.f,
              .label = "UI Brightness",
              .section = "Output",
              .tooltip = "Sets the brightness of UI and HUD elements in nits",
              .min = 48.f,
              .max = 500.f,
              .is_enabled = [output_state]() { return mgsv::output::IsHDROutputSelected(*output_state); },
          },
          new renodx::utils::settings::Setting{
              .key = "GammaCorrection",
              .binding = &shader_injection->gamma_correction,
              .value_type = renodx::utils::settings::SettingValueType::INTEGER,
              .default_value = 1.f,
              .label = "SDR EOTF Emulation",
              .section = "Output",
              .tooltip = "Emulates a 2.2 EOTF (use with HDR or sRGB)",
              .labels = {"Off", "2.2"},
          },
          new renodx::utils::settings::Setting{
              .key = "ToneMapType",
              .binding = &shader_injection->tone_map_type,
              .value_type = renodx::utils::settings::SettingValueType::INTEGER,
              .default_value = 1.f,
              .label = "Tone Mapper",
              .section = "Tone Mapping",
              .tooltip = "Sets the tone mapper type",
              .labels = {"Vanilla", "RenoDX"},
          },
          new renodx::utils::settings::Setting{
              .key = "ColorGradeExposure",
              .binding = &shader_injection->tone_map_exposure,
              .default_value = 1.f,
              .label = "Exposure",
              .section = "Color Grading",
              .max = 2.f,
              .format = "%.2f",
              .is_enabled = [shader_injection]() { return shader_injection->tone_map_type != 0.f; },
          },
          new renodx::utils::settings::Setting{
              .key = "ColorGradeHighlights",
              .binding = &shader_injection->tone_map_highlights,
              .default_value = 50.f,
              .label = "Highlights",
              .section = "Color Grading",
              .max = 100.f,
              .is_enabled = [shader_injection]() { return shader_injection->tone_map_type != 0.f; },
              .parse = [](float value) { return value * 0.02f; },
          },
          new renodx::utils::settings::Setting{
              .key = "ColorGradeHighlightContrast",
              .binding = &shader_injection->tone_map_contrast_highlights,
              .default_value = 55.f,
              .label = "Highlight Contrast",
              .section = "Color Grading",
              .max = 100.f,
              .is_enabled = [shader_injection]() { return shader_injection->tone_map_type != 0; },
              .parse = [](float value) { return value * 0.02f; },
          },
          new renodx::utils::settings::Setting{
              .key = "ColorGradeShadows",
              .binding = &shader_injection->tone_map_shadows,
              .default_value = 50.f,
              .label = "Shadows",
              .section = "Color Grading",
              .max = 100.f,
              .is_enabled = [shader_injection]() { return shader_injection->tone_map_type != 0.f; },
              .parse = [](float value) { return value * 0.02f; },
          },
          new renodx::utils::settings::Setting{
              .key = "ColorGradeShadowContrast",
              .binding = &shader_injection->tone_map_contrast_shadows,
              .default_value = 50.f,
              .label = "Shadow Contrast",
              .section = "Color Grading",
              .max = 100.f,
              .is_enabled = [shader_injection]() { return shader_injection->tone_map_type != 0; },
              .parse = [](float value) { return value * 0.02f; },
          },
          new renodx::utils::settings::Setting{
              .key = "ColorGradeContrast",
              .binding = &shader_injection->tone_map_contrast,
              .default_value = 50.f,
              .label = "Contrast",
              .section = "Color Grading",
              .max = 100.f,
              .is_enabled = [shader_injection]() { return shader_injection->tone_map_type != 0.f; },
              .parse = [](float value) { return value * 0.02f; },
          },
          new renodx::utils::settings::Setting{
              .key = "ColorGradeSaturation",
              .binding = &shader_injection->tone_map_saturation,
              .default_value = 50.f,
              .label = "Saturation",
              .section = "Color Grading",
              .max = 100.f,
              .is_enabled = [shader_injection]() { return shader_injection->tone_map_type != 0.f; },
              .parse = [](float value) { return value * 0.02f; },
          },
          new renodx::utils::settings::Setting{
              .key = "ColorGradeHighlightSaturation",
              .binding = &shader_injection->tone_map_highlight_saturation,
              .default_value = 50.f,
              .label = "Highlight Saturation",
              .section = "Color Grading",
              .tooltip = "Adds or removes highlight color.",
              .max = 100.f,
              .is_enabled = [shader_injection]() { return shader_injection->tone_map_type != 0.f; },
              .parse = [](float value) { return value * 0.02f; },
          },
          new renodx::utils::settings::Setting{
              .key = "ColorGradeDechroma",
              .binding = &shader_injection->tone_map_dechroma,
              .default_value = 0.f,
              .label = "Dechroma",
              .section = "Color Grading",
              .tooltip = "Controls highlight desaturation due to overexposure.",
              .max = 100.f,
              .is_enabled = [shader_injection]() { return shader_injection->tone_map_type != 0.f; },
              .parse = [](float value) { return value * 0.01f; },
          },
          new renodx::utils::settings::Setting{
              .key = "ColorGradeFlare",
              .binding = &shader_injection->tone_map_flare,
              .default_value = 0.f,
              .label = "Flare",
              .section = "Color Grading",
              .tooltip = "Flare/Glare Compensation",
              .max = 100.f,
              .is_enabled = [shader_injection]() { return shader_injection->tone_map_type != 0.f; },
              .parse = [](float value) { return value * 0.01f; },
          },
          new renodx::utils::settings::Setting{
              .key = "ColorGradeScene",
              .binding = &shader_injection->scene_grade_strength,
              .default_value = 100.f,
              .label = "Scene Grading",
              .section = "Color Grading",
              .tooltip = "Selects the strength of the game's custom scene grading.",
              .max = 100.f,
              .parse = [](float value) { return value * 0.01f; },
          },
          new renodx::utils::settings::Setting{
              .key = "ColorGradeSceneScaling",
              .binding = &shader_injection->scene_grade_scaling,
              .default_value = 100.f,
              .label = "Scene Grading Scaling",
              .section = "Color Grading",
              .tooltip = "Scales the scene grading to full range when size is clamped.",
              .max = 100.f,
              .parse = [](float value) { return value * 0.01f; },
          },
          // new renodx::utils::settings::Setting{
          //     .key = "FxBloomType",
          //     .binding = &shader_injection->custom_bloom_type,
          //     .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          //     .default_value = 1.f,
          //     .label = "Bloom Type",
          //     .section = "Effects",
          //     .labels = {"Vanilla", "Enhanced"},
          // },
          new renodx::utils::settings::Setting{
              .key = "FxBloom",
              .binding = &shader_injection->custom_bloom,
              .default_value = 100.f,
              .label = "Bloom",
              .section = "Effects",
              .max = 100.f,
              .parse = [](float value) { return value * 0.01f; },
          },
          new renodx::utils::settings::Setting{
              .key = "FxBoostSun",
              .binding = &shader_injection->custom_boost_sun,
              .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
              .default_value = 1.f,
              .label = "Boost Sun Brightness",
              .section = "Effects",
              .tooltip = "Boosts the brightness of the sun.",
          },
          new renodx::utils::settings::Setting{
              .key = "FxMotionBlur",
              .binding = &shader_injection->custom_motion_blur,
              .default_value = 100.f,
              .label = "Motion Blur",
              .section = "Effects",
              .min = 0.f,
              .max = 100.f,
              .parse = [](float value) { return value * 0.01f; },
          },
          new renodx::utils::settings::Setting{
              .key = "FxFilmGrain",
              .binding = &shader_injection->custom_grain_strength,
              .default_value = 0.f,
              .label = "Film Grain",
              .section = "Effects",
              .max = 100.f,
              .parse = [](float value) { return value * 0.02f; },
          },
          new renodx::utils::settings::Setting{
              .value_type = renodx::utils::settings::SettingValueType::BUTTON,
              .label = "Reset All",
              .section = "Options",
              .group = "button-line-1",
              .on_change = [registry, shader_injection, output_state]() {
                for (auto* setting : *registry) {
                  if (setting->key.empty()) continue;
                  if (setting->is_global) continue;
                  if (!setting->can_reset) continue;
                  if (!setting->HasNumericValue()) continue;
                  const std::unique_lock lock(renodx::utils::mutex::global_mutex);
                  setting->Set(setting->default_value)->Write();
                }
                mgsv::output::SyncOutputMode(shader_injection, *output_state);
              },
          },
          new renodx::utils::settings::Setting{
              .value_type = renodx::utils::settings::SettingValueType::BUTTON,
              .label = "RenoDX Discord",
              .section = "Links",
              .group = "button-line-2",
              .tint = 0x5865F2,
              .on_change = []() {
                renodx::utils::platform::LaunchURL("https://discord.gg/", "Ce9bQHQrSV");
              },
          },
          new renodx::utils::settings::Setting{
              .value_type = renodx::utils::settings::SettingValueType::BUTTON,
              .label = "HDR Den Discord",
              .section = "Links",
              .group = "button-line-2",
              .tint = 0x5865F2,
              .on_change = []() {
                renodx::utils::platform::LaunchURL("https://discord.gg/", "5WZXDpmbpP");
              },
          },
          new renodx::utils::settings::Setting{
              .value_type = renodx::utils::settings::SettingValueType::BUTTON,
              .label = "More Mods",
              .section = "Links",
              .group = "button-line-2",
              .tint = 0x2B3137,
              .on_change = []() {
                renodx::utils::platform::LaunchURL("https://github.com/clshortfuse/renodx/wiki/Mods");
              },
          },
          new renodx::utils::settings::Setting{
              .value_type = renodx::utils::settings::SettingValueType::BUTTON,
              .label = "Github",
              .section = "Links",
              .group = "button-line-2",
              .tint = 0x2B3137,
              .on_change = []() {
                renodx::utils::platform::LaunchURL("https://github.com/clshortfuse/renodx");
              },
          },
          new renodx::utils::settings::Setting{
              .value_type = renodx::utils::settings::SettingValueType::BUTTON,
              .label = "Musa's Ko-Fi",
              .section = "Links",
              .group = "button-line-3",
              .tint = 0xFF5A16,
              .on_change = []() { renodx::utils::platform::LaunchURL("https://ko-fi.com/musaqh"); },
          },
          new renodx::utils::settings::Setting{
              .value_type = renodx::utils::settings::SettingValueType::BUTTON,
              .label = "ShortFuse's Ko-Fi",
              .section = "Links",
              .group = "button-line-3",
              .tint = 0xFF5A16,
              .on_change = []() { renodx::utils::platform::LaunchURL("https://ko-fi.com/shortfuse"); },
          },
          new renodx::utils::settings::Setting{
              .value_type = renodx::utils::settings::SettingValueType::TEXT,
              .label = std::string("Build: ") + renodx::utils::date::ISO_DATE_TIME,
              .section = "About",
          },
          new renodx::utils::settings::Setting{
              .value_type = renodx::utils::settings::SettingValueType::TEXT,
              .label = std::string("- Thanks to Maple for the FSR3 implementation\n"),
              .section = "About",
          },
      });
}

inline void ApplyPresetOff(renodx::utils::settings::Settings* registry, ShaderInjectData* shader_injection,
                           const mgsv::output::State& output_state) {
  constexpr std::pair<const char*, float> PRESET_VALUES[] = {
      {"OutputMode", 0.f},
      {"ToneMapType", 0.f},
      {"ToneMapPeakNits", 203.f},
      {"ToneMapGameNits", 203.f},
      {"ToneMapUINits", 203.f},
      {"GammaCorrection", 0.f},
      {"ColorGradeExposure", 1.f},
      {"ColorGradeHighlights", 50.f},
      {"ColorGradeHighlightContrast", 50.f},
      {"ColorGradeShadows", 50.f},
      {"ColorGradeShadowContrast", 50.f},
      {"ColorGradeContrast", 50.f},
      {"ColorGradeSaturation", 50.f},
      {"ColorGradeHighlightSaturation", 50.f},
      {"ColorGradeDechroma", 0.f},
      {"ColorGradeFlare", 0.f},
      {"ColorGradeScene", 100.f},
      {"ColorGradeSceneScaling", 0.f},
      {"FxBloomType", 0.f},
      {"FxBloom", 100.f},
      {"FxBoostSun", 0.f},
      {"FxFilmGrain", 0.f},
      {"FxMotionBlur", 100.f},
      {"FxDirectionalContactHardening", 0.f},
      {"FxSpotContactHardening", 0.f},
      {"FxPointContactHardening", 0.f},
      {"FxDirectionalScreenSpaceShadows", 0.f},
      {"FxSpotScreenSpaceShadows", 0.f},
      {"FxPointScreenSpaceShadows", 0.f},
      {"FxAmbientOcclusion", 0.f},
      {"FxIndirectLighting", 0.f},
  };
  {
    const std::unique_lock lock(renodx::utils::mutex::global_mutex);
    for (const auto& [key, value] : PRESET_VALUES) {
      const auto setting = std::ranges::find_if(*registry, [key](auto* entry) {
        return entry->key == key;
      });
      if (setting != registry->end()) {
        (*setting)->Set(value)->Write();
      }
    }
  }
  mgsv::output::SyncOutputMode(shader_injection, output_state);
}

}  // namespace mgsv::settings