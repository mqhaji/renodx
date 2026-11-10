/*
 * Copyright (C) 2026 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <shared_mutex>
#include <span>

#include <embed/shaders.h>

#include <include/reshade.hpp>

#include "../../mods/shader.hpp"
#include "../../utils/mutex.hpp"
#include "gtvbgi/ambient_diffuse.hpp"
#include "gtvbgi/lighting.hpp"
#include "inputs.hpp"
#include "reconstruction/postprocess/resources.hpp"
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

#define MGSV_BLOOM_SHADER(value, reduction)                                                     \
  {                                                                                             \
      (value),                                                                                  \
      {                                                                                         \
          .crc32 = (value),                                                                     \
          .code = __##value,                                                                    \
          .on_replace = [](auto* cmd_list) {                                                    \
            return mgsv::rendering::resource_upgrades::UpgradeBloomTarget(cmd_list, reduction); \
          },                                                                                    \
      },                                                                                        \
  }

// The supplied shader data must outlive this registry and its draw callbacks.
inline renodx::mods::shader::CustomShaders CreateShaders(ShaderInjectData* shader_injection) {
  // Take one settings snapshot for the added work. Bind the shared PCSS noise
  // only for lights with a native map; End restores every displaced pixel input.
  const auto begin_shadows = [shader_injection](reshade::api::command_list* cmd_list, uint32_t light_type,
                                                bool has_shadow_map = true) {
    ShaderInjectData constants = {};
    {
      const std::shared_lock lock(renodx::utils::mutex::global_mutex);
      if (shader_injection->enhanced_shadows_enabled < 0.5f) return;
      constants = *shader_injection;
    }
    mgsv::shadows::screen_space::Begin(cmd_list, constants, light_type);
    if (has_shadow_map) {
      auto* state = cmd_list->get_private_data<mgsv::shadows::screen_space::DrawState>();
      if (state != nullptr && state->replacement_safe) {
        const auto noise = mgsv::lighting::GetNoiseView(cmd_list->get_device());
        auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
        context->PSGetShaderResources(20u, 1u, state->displaced_pcss_noise.ReleaseAndGetAddressOf());
        context->PSSetShaderResources(20u, 1u, noise.GetAddressOf());
        state->pcss_noise_active = true;
      }
    }
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
  const auto shadow_active = [](reshade::api::command_list* cmd_list) {
    const auto* saved = cmd_list->get_private_data<mgsv::shadows::screen_space::DrawState>();
    return saved != nullptr && saved->active && saved->replacement_safe;
  };
  // Directional draws also supply the existing environment-specular capture.
  // Keep that attachment around the same native draw as the new shadow mask.
  const auto begin_directional = [shader_injection, begin_shadows](reshade::api::command_list* cmd_list,
                                                                   bool has_shadow_map = true) {
    begin_shadows(cmd_list, 0u, has_shadow_map);
    bool lighting_enabled = false;
    {
      const std::shared_lock lock(renodx::utils::mutex::global_mutex);
      lighting_enabled = shader_injection->gt_enabled >= 0.5f;
    }
    mgsv::lighting::temporal_inputs::CaptureScene(cmd_list);
    mgsv::lighting::environment_specular::BeginCapture(cmd_list, lighting_enabled);
    return true;
  };
  const auto end_directional = [](reshade::api::command_list* cmd_list) {
    mgsv::lighting::environment_specular::EndCapture(cmd_list);
    mgsv::shadows::screen_space::End(cmd_list);
  };
  const auto local_shadow_shader = [begin_shadows, shadow_active](uint32_t hash, std::span<const uint8_t> code,
                                                                  uint32_t light_type, bool has_shadow_map) {
    return renodx::mods::shader::CustomShader{
        .crc32 = hash,
        .code = code,
        .on_replace = shadow_active,
        .on_draw = [begin_shadows, light_type, has_shadow_map](auto* cmd_list) {
          begin_shadows(cmd_list, light_type, has_shadow_map);
          return true;
        },
        .on_drawn = &mgsv::shadows::screen_space::End,
    };
  };
  // A replacement can write the optional capture output only after its native
  // bindings were qualified. Otherwise the original light shader stays active.
  const auto directional_shadow_shader = [begin_directional, end_directional, capture_active, shadow_active](
                                             uint32_t hash, std::span<const uint8_t> code, bool has_shadow_map) {
    return renodx::mods::shader::CustomShader{
        .crc32 = hash,
        .code = code,
        .on_replace = [capture_active, shadow_active](auto* cmd_list) { return capture_active(cmd_list) || shadow_active(cmd_list); },
        .on_draw = [begin_directional, has_shadow_map](auto* cmd_list) { return begin_directional(cmd_list, has_shadow_map); },
        .on_drawn = end_directional,
    };
  };
  return {
      {
          0x4FBDBB39,
          {.crc32 = 0x4FBDBB39, .on_inject = [](auto*) { return false; }, .on_drawn = &mgsv::lighting::ambient_diffuse::OnSkyDrawn},
      },
      {
          0x844CE327,
          {.crc32 = 0x844CE327, .on_inject = [](auto*) { return false; }, .on_draw = [shader_injection](auto* cmd_list) {
             bool enabled;
             {
               const std::shared_lock lock(renodx::utils::mutex::global_mutex);
               enabled = shader_injection->gt_enabled >= 0.5f;
             }
             if (enabled) {
               mgsv::lighting::ambient_diffuse::Capture(cmd_list);
             }
             return true; }},
      },
      {
          0xD71E843E,
          {.crc32 = 0xD71E843E, .on_inject = [](auto*) { return false; }, .on_drawn = [shader_injection](auto* cmd_list) {
             bool enabled;
             {
               const std::shared_lock lock(renodx::utils::mutex::global_mutex);
               enabled = shader_injection->gt_enabled >= 0.5f;
             }
             if (enabled) {
               mgsv::lighting::ambient_diffuse::OnDiffuseCopied(cmd_list);
             } }},
      },
      MGSV_BLOOM_SHADER(0x72C1D157, true),
      MGSV_BLOOM_SHADER(0x463195C2, false),
      MGSV_BLOOM_SHADER(0x1AC1A5EF, false),
      MGSV_BLOOM_SHADER(0x265053A6, false),
      MGSV_BLOOM_SHADER(0x9BE51873, false),
      // DOF_ScatterCompositeNear
      MGSV_UPGRADE_RTV_REPLACE_SHADER_CALLBACK(0xE2D609B1, mgsv::rendering::resource_upgrades::ArmDofFinalCopyRenderBufferWindow),
      // DOF_ScatterCompositeFar
      MGSV_UPGRADE_RTV_REPLACE_SHADER_CALLBACK(0x7C017264, mgsv::rendering::resource_upgrades::ArmDofFinalCopyRenderBufferWindow),
      // DOF_ScatterCompositeFinal
      MGSV_UPGRADE_RTV_REPLACE_SHADER_CALLBACK(0xFC5542BB, mgsv::rendering::resource_upgrades::DisarmDofFinalCopyRenderBufferAfterDofFinal),

      {
          0xF05DCBFD,
          {
              .crc32 = 0xF05DCBFD,
              .on_inject = [](auto*) { return false; },
              .on_draw = [](auto* cmd_list) {
                mgsv::rendering::resource_upgrades::ArmMotionBlurCopyRenderBufferWindow(cmd_list);
                return true; },
          },
      },
      {
          0x512E2B48,
          {
              .crc32 = 0x512E2B48,
              .code = __0x512E2B48,
              .on_replace = [](auto* cmd_list) {
                const auto output = mgsv::reconstruction::post_process::GetActive(cmd_list);
                return output != nullptr && output->IsScaled(); },
              .on_inject = [](auto*) { return false; },
              .on_draw = [](auto* cmd_list) {
                mgsv::rendering::resource_upgrades::ArmMotionBlurCopyRenderBufferWindow(cmd_list);
                return true; },
          },
      },
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
      {0x712F7222, directional_shadow_shader(0x712F7222, __0x712F7222, false)},
      {0x08292A5E, local_shadow_shader(0x08292A5E, __0x08292A5E, 1u, true)},
      {0x1D4DB9C5, local_shadow_shader(0x1D4DB9C5, __0x1D4DB9C5, 3u, true)},
      {0x80F0A0BC, local_shadow_shader(0x80F0A0BC, __0x80F0A0BC, 2u, true)},
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

      // Preserve each native variant's masks and material response. Lights
      // without shadow maps use contact shadows and the same spotlight budget.
      {0x0636037A, local_shadow_shader(0x0636037A, __0x0636037A, 2u, true)},
      {0x0C708B4B, directional_shadow_shader(0x0C708B4B, __0x0C708B4B, true)},
      {0x1B5FA666, local_shadow_shader(0x1B5FA666, __0x1B5FA666, 1u, true)},
      {0x22A2A1C6, local_shadow_shader(0x22A2A1C6, __0x22A2A1C6, 1u, true)},
      {0x246ED82C, local_shadow_shader(0x246ED82C, __0x246ED82C, 4u, true)},
      {0x3AA3593B, local_shadow_shader(0x3AA3593B, __0x3AA3593B, 2u, false)},
      {0x3AF9D807, local_shadow_shader(0x3AF9D807, __0x3AF9D807, 3u, true)},
      {0x4671E064, local_shadow_shader(0x4671E064, __0x4671E064, 1u, false)},
      {0x541068FC, local_shadow_shader(0x541068FC, __0x541068FC, 2u, false)},
      {0x699E8CD0, local_shadow_shader(0x699E8CD0, __0x699E8CD0, 4u, true)},
      {0x6C006674, local_shadow_shader(0x6C006674, __0x6C006674, 3u, true)},
      {0x73783915, local_shadow_shader(0x73783915, __0x73783915, 1u, false)},
      {0x77FA7216, local_shadow_shader(0x77FA7216, __0x77FA7216, 3u, false)},
      {0x81D871E3, local_shadow_shader(0x81D871E3, __0x81D871E3, 1u, false)},
      {0x8E139033, local_shadow_shader(0x8E139033, __0x8E139033, 3u, false)},
      {0xA54AA716, local_shadow_shader(0xA54AA716, __0xA54AA716, 4u, false)},
      {0xB067B01D, directional_shadow_shader(0xB067B01D, __0xB067B01D, false)},
      {0xB08273FD, local_shadow_shader(0xB08273FD, __0xB08273FD, 1u, false)},
      {0xCF3011AA, local_shadow_shader(0xCF3011AA, __0xCF3011AA, 4u, false)},
      {0xD3CF7256, local_shadow_shader(0xD3CF7256, __0xD3CF7256, 1u, true)},
      {0xD7F4F7B2, local_shadow_shader(0xD7F4F7B2, __0xD7F4F7B2, 3u, false)},
      {0xDB15C6F6, local_shadow_shader(0xDB15C6F6, __0xDB15C6F6, 3u, true)},
      {0xE82F8D58, local_shadow_shader(0xE82F8D58, __0xE82F8D58, 3u, false)},
      {0xFB8F891F, directional_shadow_shader(0xFB8F891F, __0xFB8F891F, true)},

      // Correct ambient lighting and add GI before either scatter consumes it.
      {
          0x6DAF0C4A,
          {
              .crc32 = 0x6DAF0C4A,
              .code = __0x6DAF0C4A,
              .on_inject = [](auto*) { return false; },
              .on_draw = [shader_injection](auto* cmd_list) {
                mgsv::lighting::ApplyLightPasses(cmd_list, *shader_injection);
                return true; },
          },
      },
      // The original second scatter still runs, without another lighting chain.
      {
          0xB2601810,
          {
              .crc32 = 0xB2601810,
              // Do not inject constants into the game's shader. Each added pass binds its own.
              .on_inject = [](reshade::api::command_list*) { return false; },
          },
      },

      __ALL_CUSTOM_SHADERS,
  };
}

#undef MGSV_UPGRADE_RTV_REPLACE_SHADER_CALLBACK
#undef MGSV_UPGRADE_RTV_SHADER
#undef MGSV_UPGRADE_DEFERRED_TONEMAP_OUTPUT_RTV
#undef MGSV_BLOOM_SHADER

}  // namespace mgsv::game
