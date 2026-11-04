/*
 * Copyright (C) 2026 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <shared_mutex>

#include <embed/shaders.h>

#include <include/reshade.hpp>

#include "../../mods/shader.hpp"
#include "../../utils/mutex.hpp"
#include "gtvbgi/lighting.hpp"
#include "inputs.hpp"
#include "resource_upgrades.hpp"
#include "shadows/screen_space_shadows.hpp"
#include "shared.h"

// Explicit registrations precede the generated shader entries.
namespace mgsv::game {

#define MGSV_UPGRADE_RTV_REPLACE_SHADER_CALLBACK(value, callback)                       \
  {                                                                                     \
      value,                                                                            \
      {                                                                                 \
          .crc32 = (value),                                                             \
          .code = __##value,                                                            \
          .on_draw = [](auto* cmd_list) {                                               \
            mgsv::rendering::resource_upgrades::OnActivateRenderTargetClones(cmd_list); \
            callback(cmd_list);                                                         \
            return true;                                                                \
          },                                                                            \
      },                                                                                \
  }

#define MGSV_UPGRADE_RTV_SHADER(value)                                                  \
  {                                                                                     \
      value,                                                                            \
      {                                                                                 \
          .crc32 = (value),                                                             \
          .on_inject = [](auto*) { return false; },                                     \
          .on_draw = &mgsv::rendering::resource_upgrades::OnActivateRenderTargetClones, \
      },                                                                                \
  }

#define MGSV_UPGRADE_DEFERRED_TONEMAP_OUTPUT_RTV(value)                                    \
  {                                                                                        \
      value,                                                                               \
      {                                                                                    \
          .crc32 = (value),                                                                \
          .code = __##value,                                                               \
          .on_draw = [](auto* cmd_list) {                                                  \
            mgsv::rendering::resource_upgrades::UpgradeDeferredSceneColorTarget(cmd_list); \
            mgsv::inputs::CaptureCompositeInputs(cmd_list);                                \
            mgsv::lighting::environment_specular::BeginComposite(cmd_list);                \
            return true;                                                                   \
          },                                                                               \
          .on_drawn = &mgsv::lighting::environment_specular::EndComposite,                 \
      },                                                                                   \
  }

#define MGSV_TRACK_MOTION_BLUR_TILE_PREP(value)     \
  {                                                 \
      value,                                        \
      {                                             \
          .crc32 = (value),                         \
          .on_inject = [](auto*) { return false; }, \
          .on_draw = [](auto* cmd_list) {                                                     \
            mgsv::rendering::resource_upgrades::ArmMotionBlurCopyRenderBufferWindow(cmd_list); \
                        return true; },        \
      },                                            \
  }

// The supplied shader data must outlive this registry and its draw callbacks.
inline renodx::mods::shader::CustomShaders CreateShaders(ShaderInjectData* shader_injection) {
  const auto begin_shadows = [shader_injection](reshade::api::command_list* cmd_list, uint32_t light_type) {
    ShaderInjectData constants = {};
    {
      const std::shared_lock lock(renodx::utils::mutex::global_mutex);
      const float enabled = (light_type == 0u ? shader_injection->screen_space_shadows
                                              : (light_type == 2u ? shader_injection->point_screen_space_shadows : shader_injection->spot_screen_space_shadows));
      if (enabled < 0.5f) return;
      constants = *shader_injection;
    }
    mgsv::shadows::screen_space::Begin(cmd_list, constants, light_type);
  };
  const auto capture_environment = [shader_injection](reshade::api::command_list* cmd_list) {
    mgsv::lighting::temporal_inputs::CaptureScene(cmd_list);
    bool lighting_enabled = false;
    {
      const std::shared_lock lock(renodx::utils::mutex::global_mutex);
      lighting_enabled = shader_injection->gt_enabled >= 0.5f;
    }
    mgsv::lighting::environment_specular::BeginCapture(cmd_list, lighting_enabled);
    return true;
  };
  const auto capture_active = [](reshade::api::command_list* cmd_list) {
    const auto* saved = cmd_list->get_private_data<mgsv::lighting::environment_specular::DrawState>();
    return saved != nullptr && saved->active;
  };
  const auto begin_directional = [shader_injection](reshade::api::command_list* cmd_list) {
    ShaderInjectData constants = {};
    bool shadows_enabled = false, lighting_enabled = false;
    {
      const std::shared_lock lock(renodx::utils::mutex::global_mutex);
      shadows_enabled = shader_injection->screen_space_shadows >= 0.5f;
      lighting_enabled = shader_injection->gt_enabled >= 0.5f;
      if (shadows_enabled) {
        constants = *shader_injection;
      }
    }
    if (shadows_enabled) {
      mgsv::shadows::screen_space::Begin(cmd_list, constants, 0u);
    }
    mgsv::lighting::temporal_inputs::CaptureScene(cmd_list);
    mgsv::lighting::environment_specular::BeginCapture(cmd_list, lighting_enabled);
    return true;
  };
  const auto end_directional = [](reshade::api::command_list* cmd_list) {
    mgsv::lighting::environment_specular::EndCapture(cmd_list);
    mgsv::shadows::screen_space::End(cmd_list);
  };
  return {
      // DOF_ScatterCompositeNear
      MGSV_UPGRADE_RTV_REPLACE_SHADER_CALLBACK(
          0xE2D609B1, mgsv::rendering::resource_upgrades::ArmDofFinalCopyRenderBufferWindow),
      // DOF_ScatterCompositeFar
      MGSV_UPGRADE_RTV_REPLACE_SHADER_CALLBACK(
          0x7C017264, mgsv::rendering::resource_upgrades::ArmDofFinalCopyRenderBufferWindow),
      // DOF_ScatterCompositeFinal
      MGSV_UPGRADE_RTV_REPLACE_SHADER_CALLBACK(
          0xFC5542BB, mgsv::rendering::resource_upgrades::DisarmDofFinalCopyRenderBufferAfterDofFinal),

      MGSV_TRACK_MOTION_BLUR_TILE_PREP(0xF05DCBFD),
      MGSV_TRACK_MOTION_BLUR_TILE_PREP(0x512E2B48),
      {
          0x83272BCB,
          {
              .crc32 = 0x83272BCB,
              .code = __0x83272BCB,
              .on_inject = [](auto*) { return false; },
              .on_draw = &mgsv::rendering::resource_upgrades::OnCopyRenderBufferDraw,
              .on_drawn = &mgsv::rendering::resource_upgrades::OnCopyRenderBufferDrawn,
          },
      },

      // Motion blur ping-pongs through half-resolution scene-color intermediates.
      {
          0xBFC7D3C2,
          {
              .crc32 = 0xBFC7D3C2,
              .code = __0xBFC7D3C2,
              .on_draw = &mgsv::rendering::resource_upgrades::OnUpgradeMotionBlurDraw,
          },
      },

      // Deferred rendering paths perform the actual filmic tonemap before the
      // later LUT/color-grading pass. Mark their full-size output so the extra
      // CopyRenderBuffer draws used by rain/fog paths propagate the HDR clone.
      MGSV_UPGRADE_DEFERRED_TONEMAP_OUTPUT_RTV(0xC973024D),  // DeferredRenderingFilmic_VolFog
      MGSV_UPGRADE_DEFERRED_TONEMAP_OUTPUT_RTV(0xBDE1F4CD),  // DR_TonemapRainFilter
      MGSV_UPGRADE_DEFERRED_TONEMAP_OUTPUT_RTV(0x6E29F0AB),  // DR_TonemapRainFilter_NoIR
      MGSV_UPGRADE_DEFERRED_TONEMAP_OUTPUT_RTV(0x2EA8F13F),  // DR_VolFog_TppTonemap
      MGSV_UPGRADE_DEFERRED_TONEMAP_OUTPUT_RTV(0x59B44963),  // DR_VolFog_TppTonemap_MD

      // LUT builders write into typeless BGRA render targets that need the clone activated.
      MGSV_UPGRADE_RTV_SHADER(0x244877AB),
      MGSV_UPGRADE_RTV_SHADER(0x3BCB1620),
      MGSV_UPGRADE_RTV_SHADER(0x5A8584C0),
      MGSV_UPGRADE_RTV_SHADER(0x5E320D1D),
      MGSV_UPGRADE_RTV_SHADER(0x637BB745),
      MGSV_UPGRADE_RTV_SHADER(0xCA7F0E3D),
      MGSV_UPGRADE_RTV_SHADER(0xDAA0FD48),
      MGSV_UPGRADE_RTV_SHADER(0xF50444CF),

      {
          0x900968FF,
          {
              .crc32 = 0x900968FF,
              .code = __0x900968FF,
              .on_draw = &mgsv::rendering::resource_upgrades::OnActivateRenderTargetClones,
          },
      },

      // The named Tonemap shader is the late LUT/color-grading pass in MGSV.
      {
          0xE04D1471,
          {
              .crc32 = 0xE04D1471,
              .code = __0xE04D1471,
              .on_draw = &mgsv::rendering::resource_upgrades::OnUpgradeSceneColorDraw,
          },
      },

      MGSV_UPGRADE_DEFERRED_TONEMAP_OUTPUT_RTV(0x410AE8C5),  // DeferredRenderingFilmic

      {
          0x5AFD2E17,
          {
              .crc32 = 0x5AFD2E17,
              .code = __0x5AFD2E17,
              .on_draw = begin_directional,
              .on_drawn = end_directional,
              .get_replacement_pipeline = [](reshade::api::command_list* cmd_list) { return mgsv::lighting::environment_specular::GetShadowedCapturePipeline(cmd_list, __sun_environment_capture); },
          },
      },
      {
          0x712F7222,
          {
              .crc32 = 0x712F7222,
              .code = __0x712F7222,
              .on_replace = [capture_active](reshade::api::command_list* cmd_list) {
                const auto* state = cmd_list->get_private_data<mgsv::shadows::screen_space::DrawState>();
                return capture_active(cmd_list) || (state != nullptr && state->active && state->replacement_safe);
              },
              .on_draw = begin_directional,
              .on_drawn = end_directional,
          },
      },
      {
          0x08292A5E,
          {
              .crc32 = 0x08292A5E,
              .code = __0x08292A5E,
              .on_draw = [begin_shadows](reshade::api::command_list* cmd_list) {
                begin_shadows(cmd_list, 1u);
                return true;
              },
              .on_drawn = &mgsv::shadows::screen_space::End,
          },
      },
      {
          0x1D4DB9C5,
          {
              .crc32 = 0x1D4DB9C5,
              .code = __0x1D4DB9C5,
              .on_draw = [begin_shadows](reshade::api::command_list* cmd_list) {
                begin_shadows(cmd_list, 3u);
                return true;
              },
              .on_drawn = &mgsv::shadows::screen_space::End,
          },
      },
      {
          0x80F0A0BC,
          {
              .crc32 = 0x80F0A0BC,
              .code = __0x80F0A0BC,
              .on_draw = [begin_shadows](reshade::api::command_list* cmd_list) {
                begin_shadows(cmd_list, 2u);
                return true;
              },
              .on_drawn = &mgsv::shadows::screen_space::End,
          },
      },
      {
          0x34B04FB1,
          {
              .crc32 = 0x34B04FB1,
              .code = __0x34B04FB1,
              .on_replace = capture_active,
              .on_inject = [](reshade::api::command_list*) { return false; },
              .on_draw = capture_environment,
              .on_drawn = &mgsv::lighting::environment_specular::EndCapture,
          },
      },

      // Keep the second scatter dispatch; record added lighting after it.
      {
          0xB2601810,
          {
              .crc32 = 0xB2601810,
              // Do not inject constants into the game's shader. Each added pass binds its own.
              .on_inject = [](reshade::api::command_list*) { return false; },
              .on_drawn = [shader_injection](reshade::api::command_list* cmd_list) { mgsv::lighting::ApplyLightPasses(cmd_list, *shader_injection); },
          },
      },

      __ALL_CUSTOM_SHADERS,
  };
}

#undef MGSV_UPGRADE_RTV_REPLACE_SHADER_CALLBACK
#undef MGSV_UPGRADE_RTV_SHADER
#undef MGSV_UPGRADE_DEFERRED_TONEMAP_OUTPUT_RTV
#undef MGSV_TRACK_MOTION_BLUR_TILE_PREP

}  // namespace mgsv::game
