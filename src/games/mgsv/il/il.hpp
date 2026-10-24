/*
 * Copyright (C) 2026 Musa Haji
 * Copyright (C) 2026 ZioPao
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "../light_pass.hpp"

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

  // The gather samples the light the retail scatter pass itself reads. No copy: additive
  // mode only adds, so it never needs a matching pre-AO snapshot to compare against.
  const auto light_source = frame.light_source;

  // 0 = full, 1 = half, 2 = quarter: the gather, history, moments and a-trous all follow.
  const uint32_t scale = shader_injection.il_resolution > 1.5f
                             ? 4u
                             : (shader_injection.il_resolution > 0.5f ? 2u : 1u);
  shader_injection.il_scale = static_cast<float>(scale);
  const uint32_t width = (frame.width + scale - 1u) / scale;
  const uint32_t height = (frame.height + scale - 1u) / scale;

  // The history is only valid while its size matches and the GI chain has run continuously.
  if (history_width != width || history_height != height) {
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
                         __mgsv_il_accumulate, width, height)) {
    const std::array<reshade::api::resource_view, 7> gather_sources = {
        frame.depth,                 // t0
        frame.normal,                // t1
        light_source,                // t2
        albedo_view,                 // t3
        specular_view,               // t4
        GetFallbackView(cmd_list),   // t5, unused
        GetFallbackView(cmd_list),   // t6, unused
    };
    DispatchLightPass(cmd_list, &pass, gather_sources, bounce_target.uav,
                      filtered_target.uav, accumulated_target.uav,
                      moments_targets[history_index].uav, width, height);

    // Temporal accumulation against the previous result. The velocity view comes from the
    // TAA capture, which is backend-independent and already written by this point in the
    // frame; when it is missing (reconstruction off) the blend collapses to zero.
    const auto velocity_view = taa::input_capture::resources.velocity_srv;
    shader_injection.il_history_valid = history_valid && velocity_view.handle != 0 ? 1.f : 0.f;
    const std::array<reshade::api::resource_view, 7> accumulate_sources = {
        bounce_target.view,                      // t0
        history_targets[history_index].view,     // t1
        velocity_view,                           // t2
        frame.depth,                             // t3
        frame.normal,                            // t4
        history_aux_targets[history_index].view, // t5
        moments_targets[history_index].view,     // t6
    };
    const uint32_t write_index = history_index ^ 1u;
    DispatchLightPass(cmd_list, &accumulate_pass, accumulate_sources,
                      history_targets[write_index].uav, accumulated_target.uav,
                      history_aux_targets[write_index].uav,
                      moments_targets[write_index].uav, width, height);
    history_index = write_index;
    history_valid = true;

    // A-trous: stride 1, 2, 4 per iteration. The targets alternate so no dispatch reads what
    // it writes, and bounce_target stays the gather.
    const uint32_t filter_passes =
        static_cast<uint32_t>(std::clamp(shader_injection.il_filter_radius, 1.f, 3.f));
    reshade::api::resource_view filter_input = accumulated_target.view;
    for (uint32_t step = 0; step < filter_passes; ++step) {
      const auto filter_output = (step % 2) == 0 ? filtered_target.uav : accumulated_target.uav;
      const std::array<reshade::api::resource_view, 7> filter_sources = {
          filter_input,                              // t0
          frame.depth,                               // t1
          frame.normal,                              // t2
          moments_targets[history_index].view,       // t3, has-signal and variance
          GetFallbackView(cmd_list),                 // t4, unused
          GetFallbackView(cmd_list),                 // t5, unused
          GetFallbackView(cmd_list),                 // t6, unused
      };
      shader_injection.il_filter_step = static_cast<float>(step);
      DispatchLightPass(cmd_list, &composite_pass, filter_sources, filter_output,
                        bounce_target.uav, history_aux_targets[history_index].uav,
                        history_targets[history_index].uav, width, height);
      filter_input = (step % 2) == 0 ? filtered_target.view : accumulated_target.view;
    }
    shader_injection.il_filter_step = 0.f;

    // An even pass count ends in the accumulate target, so the resolve follows whichever
    // target holds the last iteration instead of assuming it is filtered_target.
    const bool filtered_in_filtered = (filter_passes % 2) != 0;
    const auto filtered_view = filtered_in_filtered ? filtered_target.view
                                                    : accumulated_target.view;

    const std::array<reshade::api::resource_view, 7> add_sources = {
        filtered_view,             // t0
        frame.depth,               // t1, joint bilateral upsample
        GetFallbackView(cmd_list), // t2, unused
        GetFallbackView(cmd_list), // t3, unused
        GetFallbackView(cmd_list), // t4, unused
        GetFallbackView(cmd_list), // t5, unused
        GetFallbackView(cmd_list), // t6, unused
    };
    DispatchLightPass(cmd_list, &add_pass, add_sources, frame.light,
                      accumulated_target.uav, bounce_target.uav,
                      moments_targets[history_index].uav, frame.width, frame.height);
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
          .key = "IlEnabled",
          .binding = &shader_injection.il_enabled,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 1.f,
          .label = "Screen Space GI",
          .section = "Indirect Lighting",
          .tooltip = "Adds a screen-space diffuse gather after the AO.",
      },
      new renodx::utils::settings::Setting{
          .key = "IlAmount",
          .binding = &shader_injection.il_amount,
          .default_value = 80.f,
          .label = "GI Amount",
          .section = "Indirect Lighting",
          .tooltip = "How much of the gathered light is added, as a percentage.",
          .max = 200.f,
          .parse = [](float value) { return value * 0.01f; },
      },
      new renodx::utils::settings::Setting{
          .key = "IlRadius",
          .binding = &shader_injection.il_radius,
          .default_value = 80.f,
          .label = "GI Radius",
          .section = "Indirect Lighting",
          .tooltip = "Gather radius as a percentage of screen height.",
          .max = 100.f,
          .parse = [](float value) { return value * 0.01f; },
      },
      new renodx::utils::settings::Setting{
          .key = "IlSamples",
          .binding = &shader_injection.il_samples,
          .default_value = 8.f,
          .label = "GI Samples",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Samples per pixel. More is less noise, at linear cost.",
          .min = 2.f,
          .max = 16.f,
          .is_visible = [] { return shader_injection.il_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "IlResolution",
          .binding = &shader_injection.il_resolution,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 2.f,
          .label = "GI Resolution",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Resolution the gather and its denoiser run at.",
          .labels = {"Full", "Half", "Quarter"},
          .is_visible = [] { return shader_injection.il_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "IlFilterRadius",
          .binding = &shader_injection.il_filter_radius,
          .default_value = 3.f,
          .label = "GI Denoise Passes",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "A-trous iterations the denoiser runs.",
          .min = 1.f,
          .max = 3.f,
          .is_visible = [] { return shader_injection.il_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "IlHistory",
          .binding = &shader_injection.il_history_blend,
          .default_value = 90.f,
          .label = "GI History",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Ceiling on how much of the previous frame survives.",
          .max = 95.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [] { return shader_injection.il_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "IlDebug",
          .binding = &shader_injection.il_debug,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 0.f,
          .label = "GI Debug",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Shows a diagnostic view instead of the scene.",
          .labels = {"Off", "GI only", "History delta", "History length", "Reprojection", "Variance"},
          .is_visible = [] { return shader_injection.il_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "IlMinSamples",
          .binding = &shader_injection.il_min_samples,
          .default_value = 1.f,
          .label = "GI Min Samples",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Samples that must survive rejection before a pixel contributes.",
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
