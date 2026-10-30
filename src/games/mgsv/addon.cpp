/*
 * Copyright (C) 2024 Musa Haji
 * Copyright (C) 2026 ZioPao
 * SPDX-License-Identifier: MIT
 */

#define ImTextureID                   ImU64
#define RENODX_MODS_SWAPCHAIN_VERSION 2
// #define DEBUG_LEVEL_0
// #define DEBUG_LEVEL_1
// #define DEBUG_LEVEL_2
// #define DEBUG_LEVEL_3

#include <cstdint>

#include <windows.h>

#include <deps/imgui/imgui.h>
#include <include/reshade.hpp>

#include "../../mods/shader.hpp"
#include "../../mods/swapchain_v2.hpp"
#include "../../utils/random.hpp"
#include "../../utils/settings.hpp"
#include "../../utils/swapchain.hpp"
#include "./ao/ao.hpp"
#include "./game/shaders.hpp"
#include "./il/il.hpp"
#include "./output/output.hpp"
#include "./rendering/light_pass.hpp"
#include "./rendering/resource_upgrades.hpp"
#include "./settings.hpp"
#include "./shadows/settings.hpp"
#include "./shared.h"
#include "./taa/runtime.hpp"
#include "./taa/settings.hpp"

namespace {

// These objects outlive all settings and shader callbacks that borrow them.
ShaderInjectData shader_injection = {};
mgsv::output::State output_state;
mgsv::reconstruction::settings::State reconstruction_settings;
renodx::utils::settings::Settings settings;
renodx::mods::shader::CustomShaders custom_shaders = mgsv::game::CreateShaders(&shader_injection);

void OnPresetOff() {
  mgsv::settings::ApplyPresetOff(&settings, &shader_injection, output_state);
}

void OnInitSwapchain(reshade::api::swapchain* swapchain, bool resize) {
  (void)resize;
  mgsv::output::OnInitSwapchain(&output_state, swapchain);
}

void OnPresent(
    reshade::api::command_queue* queue,
    reshade::api::swapchain* swapchain,
    const reshade::api::rect* source_rect,
    const reshade::api::rect* dest_rect,
    uint32_t dirty_rect_count,
    const reshade::api::rect* dirty_rects) {
  (void)queue;
  (void)source_rect;
  (void)dest_rect;
  (void)dirty_rect_count;
  (void)dirty_rects;

  mgsv::output::SyncOutputMode(&shader_injection, output_state);
  const auto output_color_space = (mgsv::output::IsHDROutputSelected(output_state)
                                       ? reshade::api::color_space::hdr10_st2084
                                       : reshade::api::color_space::srgb_nonlinear);
  if (swapchain->get_color_space() != output_color_space) {
    renodx::utils::swapchain::ChangeColorSpace(swapchain, output_color_space);
  }

  mgsv::rendering::resource_upgrades::DeactivateDofFinalCopyRenderBufferTarget();
  mgsv::rendering::resource_upgrades::ResetTonemapCopyTracking();
}

void OnDestroyDevice(reshade::api::device* device) {
  mgsv::ao::Destroy(device);
  mgsv::il::Destroy(device);
  mgsv::rendering::DestroyLightPassLayout(device);
  mgsv::rendering::DestroyFallbackViews(device);
  mgsv::game::albedo_view = {0};
  mgsv::game::specular_view = {0};
}

bool initialized = false;

}  // namespace

extern "C" __declspec(dllexport) constexpr const char* NAME = "RenoDX";
extern "C" __declspec(dllexport) constexpr const char* DESCRIPTION = "RenoDX for METAL GEAR SOLID V: THE PHANTOM PAIN";

BOOL APIENTRY DllMain(HMODULE h_module, DWORD fdw_reason, LPVOID lpv_reserved) {
  switch (fdw_reason) {
    case DLL_PROCESS_ATTACH:
      if (!reshade::register_addon(h_module)) return FALSE;

      mgsv::settings::AppendSettings(&settings, &shader_injection, &output_state);
      mgsv::shadows::AppendSettings(&settings, &shader_injection);
      mgsv::reconstruction::settings::MigrateTemporalModeSetting();
      mgsv::reconstruction::settings::AppendSettings(&settings, &shader_injection, &reconstruction_settings);
      mgsv::ao::AppendSettings(&settings, &shader_injection);
      mgsv::il::AppendSettings(&settings, &shader_injection);

      if (!initialized) {
        renodx::mods::swapchain::v2::force_borderless = true;
        renodx::mods::swapchain::v2::prevent_full_screen = true;

        renodx::mods::swapchain::v2::SetUseHDR10();
        renodx::mods::swapchain::v2::set_color_space = false;

        renodx::utils::settings::on_preset_changed_callbacks.emplace_back(
            [] { mgsv::reconstruction::transitions::ApplySettingsSnapshot(reconstruction_settings); });

        // renodx::mods::shader::force_pipeline_cloning = true;

        renodx::mods::shader::expected_constant_buffer_index = 13;
        renodx::mods::shader::minimum_constant_buffer_stages =
            reshade::api::shader_stage::vertex
            | reshade::api::shader_stage::pixel
            | reshade::api::shader_stage::compute;
        renodx::mods::swapchain::v2::expected_constant_buffer_index = 13;

        renodx::mods::swapchain::v2::use_resource_cloning = true;

        // DevKit shows the LUT builder and scene RTs as typeless resources with
        // unorm views. Mark them as clone targets only; the shader callbacks
        // activate clones when those resources are used as color targets.
        renodx::mods::swapchain::v2::resource_upgrade_infos.push_back({
            .old_format = reshade::api::format::b8g8r8a8_typeless,
            .new_format = reshade::api::format::r16g16b16a16_float,
            .use_resource_view_cloning = true,
            .use_resource_view_hot_swap = true,
            .usage_set = static_cast<uint32_t>(
                reshade::api::resource_usage::shader_resource
                | reshade::api::resource_usage::render_target),
            .dimensions = {.width = 256, .height = 16},
            .usage_include = reshade::api::resource_usage::render_target,
            .name = "lutbuilder_256x16_bgra8_typeless_hot_swap",
        });

        // Keep the tuning and diagnostic groups out of the way on first open.
        // Every other section stays expanded, so only the "(Advanced)" groups start collapsed.
        renodx::utils::settings::open_sections_by_default = false;
        renodx::utils::settings::default_open_sections = {
            "Temporal Anti-Aliasing",
            "Presets",
            "Output",
            "Tone Mapping",
            "Color Grading",
            "Effects",
            "Contact-Hardening Shadows",
            "Screen-Space Shadows",
            "Ambient Occlusion",
            "Indirect Lighting",
            "Options",
            "Links",
            "About",
        };

        initialized = true;
      }

      reshade::register_event<reshade::addon_event::copy_resource>(mgsv::rendering::resource_upgrades::OnCopyTonemapOutputResource);
      reshade::register_event<reshade::addon_event::init_swapchain>(OnInitSwapchain);  // detect peak brightness
      reshade::register_event<reshade::addon_event::present>(OnPresent);
      reshade::register_event<reshade::addon_event::destroy_device>(OnDestroyDevice);

      break;
    case DLL_PROCESS_DETACH:
      reshade::unregister_event<reshade::addon_event::copy_resource>(mgsv::rendering::resource_upgrades::OnCopyTonemapOutputResource);
      reshade::unregister_event<reshade::addon_event::init_swapchain>(OnInitSwapchain);
      reshade::unregister_event<reshade::addon_event::present>(OnPresent);
      reshade::unregister_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
      renodx::mods::swapchain::v2::set_color_space = true;
      reshade::unregister_addon(h_module);
      break;
  }
  // Load the persisted temporal mode before the runtime reads it so shaders
  // and native reconstruction cannot disagree during the first frame.
  renodx::utils::settings::Use(fdw_reason, &settings, &OnPresetOff);
  if (fdw_reason == DLL_PROCESS_ATTACH) {
    mgsv::output::SyncOutputMode(&shader_injection, output_state);
  }

  mgsv::reconstruction::Use(fdw_reason, reconstruction_settings);

  renodx::utils::random::Use(fdw_reason, {&shader_injection.custom_random});

  renodx::mods::swapchain::v2::Use(fdw_reason, &shader_injection);

  renodx::mods::shader::Use(fdw_reason, custom_shaders, &shader_injection);

  return TRUE;
}
