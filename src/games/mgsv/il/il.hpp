/*
 * Copyright (C) 2026 Musa Haji
 * Copyright (C) 2026 ZioPao
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

#include <embed/shaders.h>

#include "../light_pass.hpp"
#include "../taa/runtime/input_capture.hpp"

namespace il {

inline LightPass pass;
inline LightPass composite_pass;
inline LightPass add_pass;
inline LightPass accumulate_pass;

inline LightTarget bounce_target;
inline LightTarget filtered_target;
inline LightTarget history_targets[2];
inline LightTarget history_aux_targets[2];
inline LightTarget moments_targets[2];
inline LightTarget accumulated_target;

inline uint32_t history_index = 0;
inline uint32_t history_width = 0;
inline uint32_t history_height = 0;
inline bool history_valid = false;

inline void Apply(const LightFrame& frame, reshade::api::command_list* cmd_list) {
  auto* device = cmd_list->get_device();
  ShaderInjectData constants = shader_injection;

  const auto light_source = frame.light_source;

  // 0 = full, 1 = half, 2 = quarter: the gather, history, moments and a-trous all follow.
  uint32_t scale = 1u;
  if (constants.il_resolution > 1.5f) {
    scale = 4u;
  } else if (constants.il_resolution > 0.5f) {
    scale = 2u;
  }
  constants.il_scale = static_cast<float>(scale);
  const uint32_t width = (frame.width + scale - 1u) / scale;
  const uint32_t height = (frame.height + scale - 1u) / scale;

  if (history_width != width || history_height != height) {
    LogLight(reshade::log::level::info, "indirect lighting dimensions changed; previous_size=",
             history_width, "x", history_height, " size=", width, "x", height,
             " output=", frame.width, "x", frame.height, "; history invalidated, preparing resources");
    history_width = width;
    history_height = height;
    history_valid = false;
  }

  if (EnsureLightTarget(device, &bounce_target, width, height)
      && EnsureLightTarget(device, &filtered_target, width, height)
      && EnsureLightTarget(device, &history_targets[0], width, height)
      && EnsureLightTarget(device, &history_targets[1], width, height)
      && EnsureLightTarget(device, &history_aux_targets[0], width, height)
      && EnsureLightTarget(device, &history_aux_targets[1], width, height)
      && EnsureLightTarget(device, &moments_targets[0], width, height,
                           reshade::api::format::r16g16_float)
      && EnsureLightTarget(device, &moments_targets[1], width, height,
                           reshade::api::format::r16g16_float)
      && EnsureLightTarget(device, &accumulated_target, width, height)
      && EnsureLightPass(device, &pass, "mgsv: IL pass created", __mgsv_il, width, height)
      && EnsureLightPass(device, &composite_pass, "mgsv: IL composite created",
                         __mgsv_il_composite, width, height)
      && EnsureLightPass(device, &add_pass, "mgsv: IL add created",
                         __mgsv_il_add, frame.width, frame.height)
      && EnsureLightPass(device, &accumulate_pass, "mgsv: IL accumulate created",
                         __mgsv_il_accumulate, width, height)
      && EnsureFallbackViews(device)) {
    // Use separate textures for unused outputs so they cannot unbind a pass input.
    const auto fallback_uav0 = GetFallbackUav(cmd_list, 0u);
    const auto fallback_uav1 = GetFallbackUav(cmd_list, 1u);
    const auto fallback_uav2 = GetFallbackUav(cmd_list, 2u);
    const std::array<reshade::api::resource_view, 7> gather_sources = {
        frame.depth,                // t0
        frame.normal,               // t1
        light_source,               // t2
        frame.albedo,               // t3
        frame.specular,             // t4
        GetFallbackView(cmd_list),  // t5, unused
        GetFallbackView(cmd_list),  // t6, unused
    };
    DispatchLightPass(cmd_list, &pass, constants, gather_sources, bounce_target.uav,
                      fallback_uav0, fallback_uav1, fallback_uav2, width, height);

    // Reuse the velocity view captured for temporal reconstruction. A missing view sets
    // the history blend weight to zero; this check does not verify which frame the view belongs to.
    const auto velocity_view = taa::input_capture::resources.velocity_srv;
    if (velocity_view.handle == 0u) {
      static std::atomic_flag reported = {};
      LogLightOnce(&reported, reshade::log::level::warning,
                   "indirect lighting has no captured velocity view; history blend weight is zero");
    }
    constants.il_history_valid = history_valid && velocity_view.handle != 0 ? 1.f : 0.f;
    const std::array<reshade::api::resource_view, 7> accumulate_sources = {
        bounce_target.view,                       // t0
        history_targets[history_index].view,      // t1
        velocity_view,                            // t2
        frame.depth,                              // t3
        frame.normal,                             // t4
        history_aux_targets[history_index].view,  // t5
        moments_targets[history_index].view,      // t6
    };
    const uint32_t write_index = history_index ^ 1u;
    DispatchLightPass(cmd_list, &accumulate_pass, constants, accumulate_sources,
                      history_targets[write_index].uav, accumulated_target.uav,
                      history_aux_targets[write_index].uav,
                      moments_targets[write_index].uav, width, height);
    history_index = write_index;
    history_valid = true;

    // Denoise with sample spacing of 1, 2, then 4 pixels. Alternate input and output
    // textures so each pass reads a different texture from the one it writes.
    const uint32_t filter_passes =
        static_cast<uint32_t>(std::clamp(constants.il_filter_radius, 1.f, 3.f));
    reshade::api::resource_view filter_input = accumulated_target.view;
    for (uint32_t step = 0; step < filter_passes; ++step) {
      const auto filter_output = (step % 2) == 0 ? filtered_target.uav : accumulated_target.uav;
      const std::array<reshade::api::resource_view, 7> filter_sources = {
          filter_input,                         // t0
          frame.depth,                          // t1
          frame.normal,                         // t2
          moments_targets[history_index].view,  // t3, mean and mean-square luminance
          GetFallbackView(cmd_list),            // t4, unused
          GetFallbackView(cmd_list),            // t5, unused
          GetFallbackView(cmd_list),            // t6, unused
      };
      constants.il_filter_step = static_cast<float>(step);
      DispatchLightPass(cmd_list, &composite_pass, constants, filter_sources, filter_output,
                        fallback_uav0, fallback_uav1, fallback_uav2, width, height);
      filter_input = (step % 2) == 0 ? filtered_target.view : accumulated_target.view;
    }
    constants.il_filter_step = 0.f;

    // Use the last denoise result: filtered_target after 1 or 3 passes,
    // accumulated_target after 2 passes.
    const bool filtered_in_filtered = (filter_passes % 2) != 0;
    const auto filtered_view = filtered_in_filtered ? filtered_target.view
                                                    : accumulated_target.view;

    const std::array<reshade::api::resource_view, 7> add_sources = {
        filtered_view,              // t0
        frame.depth,                // t1, joint bilateral upsample
        GetFallbackView(cmd_list),  // t2, unused
        GetFallbackView(cmd_list),  // t3, unused
        GetFallbackView(cmd_list),  // t4, unused
        GetFallbackView(cmd_list),  // t5, unused
        GetFallbackView(cmd_list),  // t6, unused
    };
    DispatchLightPass(cmd_list, &add_pass, constants, add_sources, frame.light,
                      fallback_uav0, fallback_uav1, fallback_uav2, frame.width, frame.height);
    static std::atomic_flag recorded = {};
    LogLightOnce(&recorded, reshade::log::level::info,
                 "indirect lighting commands recorded; output=", frame.width, "x", frame.height,
                 " effect=", width, "x", height, " denoise_iterations=", filter_passes,
                 " velocity_present=", velocity_view.handle != 0u, "; this is not GPU completion");
  } else {
    static std::atomic_flag failed = {};
    LogLightOnce(&failed, reshade::log::level::warning,
                 "indirect lighting setup failed; no indirect lighting commands recorded; effect=",
                 width, "x", height);
  }
}

inline void Destroy(reshade::api::device* device) {
  DestroyLightPass(device, &pass);
  DestroyLightPass(device, &composite_pass);
  DestroyLightPass(device, &add_pass);
  DestroyLightPass(device, &accumulate_pass);
  DestroyLightTarget(device, &bounce_target);
  DestroyLightTarget(device, &filtered_target);
  DestroyLightTarget(device, &history_targets[0]);
  DestroyLightTarget(device, &history_targets[1]);
  DestroyLightTarget(device, &history_aux_targets[0]);
  DestroyLightTarget(device, &history_aux_targets[1]);
  DestroyLightTarget(device, &moments_targets[0]);
  DestroyLightTarget(device, &moments_targets[1]);
  DestroyLightTarget(device, &accumulated_target);
  history_valid = false;
}

inline bool settings_appended = false;

inline void AppendSettings(renodx::utils::settings::Settings& settings) {
  if (settings_appended) return;
  settings_appended = true;
  renodx::utils::settings::Settings additions = {
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLighting",
          .binding = &shader_injection.il_enabled,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 1.f,
          .label = "Screen-Space Indirect Lighting",
          .section = "Indirect Lighting",
          .tooltip = "Adds approximate bounced light from surfaces visible on screen. Objects outside the screen cannot contribute.",
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingStrength",
          .binding = &shader_injection.il_amount,
          .default_value = 80.f,
          .label = "Indirect Lighting Strength",
          .section = "Indirect Lighting",
          .tooltip = "Controls how much bounced light is added to the scene.",
          .max = 200.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [] { return shader_injection.il_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingSamplingRadius",
          .binding = &shader_injection.il_radius,
          .default_value = 80.f,
          .label = "Indirect Lighting Sampling Radius",
          .section = "Indirect Lighting",
          .tooltip = "Controls how far the effect searches for surfaces contributing light, as a percentage of screen height.",
          .max = 100.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [] { return shader_injection.il_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingSampleCount",
          .binding = &shader_injection.il_samples,
          .default_value = 8.f,
          .label = "Indirect Lighting Sample Count",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Controls how many nearby samples are checked per pixel. Higher values can reduce noise but cost more GPU time.",
          .min = 2.f,
          .max = 16.f,
          .is_visible = [] { return shader_injection.il_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingResolution",
          .binding = &shader_injection.il_resolution,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 2.f,
          .label = "Indirect Lighting Resolution",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Sets the resolution used to calculate and smooth bounced light. Lower resolutions are faster but can lose detail.",
          .labels = {"Full", "Half", "Quarter"},
          .is_visible = [] { return shader_injection.il_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingDenoiseIterations",
          .binding = &shader_injection.il_filter_radius,
          .default_value = 3.f,
          .label = "Indirect Lighting Denoise Iterations",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Controls how many smoothing passes are applied. More passes reduce noise but can soften detail and cost more GPU time.",
          .min = 1.f,
          .max = 3.f,
          .is_visible = [] { return shader_injection.il_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingHistoryWeight",
          .binding = &shader_injection.il_history_blend,
          .default_value = 90.f,
          .label = "Indirect Lighting History Weight",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Limits how much previous frames contribute when history is usable. Higher values can reduce noise but leave longer trails.",
          .max = 95.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [] { return shader_injection.il_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingDebugView",
          .binding = &shader_injection.il_debug,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 0.f,
          .label = "Indirect Lighting Debug View",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Replaces the normal lighting output with a view used to inspect the effect.",
          .labels = {"Off", "Indirect Lighting Only", "History Difference", "History Length", "Reprojection Diagnostics", "Relative Variance"},
          .is_visible = [] { return shader_injection.il_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingMinimumValidSamples",
          .binding = &shader_injection.il_min_samples,
          .default_value = 1.f,
          .label = "Indirect Lighting Minimum Valid Samples",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Sets how many samples must pass the checks before a pixel contributes bounced light.",
          .min = 1.f,
          .max = 16.f,
          // Hidden from the UI: grazing surfaces need one sample to survive for the ground to
          // contribute at all, and the shader clamps the value, so there is nothing to tune.
          .is_visible = [] { return false; },
      },
  };
  InsertSettingsBeforeSection(settings, "Options", additions);
}

}  // namespace il
