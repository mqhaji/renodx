/*
 * Copyright (C) 2026 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <shared_mutex>
#include <span>
#include <utility>

#include <embed/shaders.h>

#include <include/reshade.hpp>

#include "../../mods/shader.hpp"
#include "../../utils/mutex.hpp"
#include "atmosphere/fog/volume.hpp"
#include "lighting/environment_specular.hpp"
#include "lighting/indirect/ambient_diffuse.hpp"
#include "lighting/indirect/runtime.hpp"
#include "lighting/indirect/source_preparation.hpp"
#include "lighting/indirect_lighting_mode.hpp"
#include "lighting/reflections/native_chain.hpp"
#include "lighting/reflections/runtime.hpp"
#include "lighting/shadows/screen_space_shadows.hpp"
#include "materials/capture.hpp"
#include "materials/native/material_shaders.h"
#include "postprocess/bloom/bloom.hpp"
#include "reconstruction/output_size/resources.hpp"
#include "reconstruction/state.hpp"
#include "rendering/inputs.hpp"
#include "rendering/producer_records.hpp"
#include "shared.h"

// The game shaders the mod replaces or watches, each with the callbacks that run
// around its draw. Hand-written entries come first; the generated entries then
// fill in every other replacement shader.
namespace mgsv::game {

#define MGSV_DEFERRED_COMPOSITE_SHADER(value)                              \
  {                                                                        \
      value,                                                               \
      {                                                                    \
          .crc32 = (value),                                                \
          .code = __##value,                                               \
          .on_replace = [](auto* cmd_list) {                                                                 \
            const auto* state = cmd_list->template get_private_data<mgsv::lighting::environment_specular::DrawState>(); \
            return state != nullptr && state->composite_replacement_safe; },                            \
          .on_draw = [](auto* cmd_list) {                                                                     \
            mgsv::inputs::CaptureCompositeInputs(cmd_list);                                                    \
            mgsv::lighting::environment_specular::BeginComposite(cmd_list);                                    \
            mgsv::lighting::reflections::Capture(cmd_list);                                                    \
            return true; },                               \
          .on_drawn = &mgsv::lighting::environment_specular::EndComposite, \
      },                                                                   \
  }

#define MGSV_BLOOM_SHADER(value, reduction)                       \
  {                                                               \
      (value),                                                    \
      {                                                           \
          .crc32 = (value),                                       \
          .code = __##value,                                      \
          .on_replace = [](auto* cmd_list) {                      \
            return mgsv::bloom::IsChainDraw(cmd_list, reduction); \
          },                                                      \
      },                                                          \
  }

// The ports that write motion in precise form, and the motion blur ports that
// read it, replace the game's shaders while temporal reconstruction is on
// (rendering/motion.hlsli).
inline bool IsTemporalReconstructionOn(reshade::api::command_list* /*cmd_list*/) {
  return mgsv::reconstruction::state::IsEnabled();
}

// The supplied shader data must outlive this registry and its draw callbacks.
inline renodx::mods::shader::CustomShaders CreateShaders(ShaderInjectData* shader_injection) {
  mgsv::lighting::material_inputs::controls = shader_injection;
  mgsv::shadows::batch::controls = shader_injection;
  // Copy the settings once for the added shadow work. Bind the shared PCSS noise
  // (t20) only for lights with a shadow map; screen_space::End puts back every
  // pixel shader input it displaced.
  const auto begin_shadows = [shader_injection](reshade::api::command_list* cmd_list, uint32_t light_type,
                                                bool has_shadow_map = true) {
    ShaderInjectData constants = {};
    {
      const std::shared_lock lock(renodx::utils::mutex::global_mutex);
      if (shader_injection->shadows_mode < 0.5f) return;
      constants = *shader_injection;
    }
    mgsv::shadows::screen_space::Begin(cmd_list, constants, light_type, has_shadow_map);
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
  const auto capture_environment = [](reshade::api::command_list* cmd_list) {
    mgsv::lighting::temporal_inputs::CaptureScene(cmd_list);
    mgsv::lighting::environment_specular::BeginCapture(cmd_list, mgsv::lighting::UsesEnhancedIndirectLighting());
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
  // Sun draws also feed the environment-specular capture, so begin and end it
  // around the same draw as the enhanced shadows. The fog volume takes the
  // sun's shadow maps from the draw as the game bound it.
  const auto begin_directional = [begin_shadows](reshade::api::command_list* cmd_list, bool has_shadow_map = true, bool has_world_shadow_mask = false) {
    if (has_shadow_map) {
      mgsv::atmosphere::fog::OnShadowedSunDraw(cmd_list, has_world_shadow_mask);
    }
    begin_shadows(cmd_list, 0u, has_shadow_map);
    mgsv::lighting::temporal_inputs::CaptureScene(cmd_list);
    mgsv::lighting::environment_specular::BeginCapture(cmd_list, mgsv::lighting::UsesEnhancedIndirectLighting());
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
          // A shadowed spot (types 1 and 3): the fog keeps its shadow map
          // while the draw's inputs are still the game's.
          if (has_shadow_map && (light_type & 1u) != 0u) {
            mgsv::atmosphere::fog::OnShadowedSpotDraw(cmd_list);
          }
          begin_shadows(cmd_list, light_type, has_shadow_map);
          return true;
        },
        .on_drawn = &mgsv::shadows::screen_space::End,
    };
  };
  // Use the replacement only when the capture or the shadow pass accepted this
  // draw's bindings; otherwise the original light shader runs.
  const auto directional_shadow_shader = [begin_directional, end_directional, capture_active, shadow_active](
                                             uint32_t hash, std::span<const uint8_t> code, bool has_shadow_map, bool has_world_shadow_mask = false) {
    return renodx::mods::shader::CustomShader{
        .crc32 = hash,
        .code = code,
        .on_replace = [capture_active, shadow_active](auto* cmd_list) { return capture_active(cmd_list) || shadow_active(cmd_list); },
        .on_draw = [begin_directional, has_shadow_map, has_world_shadow_mask](auto* cmd_list) { return begin_directional(cmd_list, has_shadow_map, has_world_shadow_mask); },
        .on_drawn = end_directional,
    };
  };
  renodx::mods::shader::CustomShaders shaders = {
      // Sky ambient light and its upsample (BilateralUpscale2x2): enhanced
      // diffuse lighting captures the ambient term here.
      {
          0x4FBDBB39,
          {.crc32 = 0x4FBDBB39, .on_inject = [](auto*) { return false; }, .on_drawn = &mgsv::lighting::ambient_diffuse::OnSkyDrawn},
      },
      {
          0x844CE327,
          {
              .crc32 = 0x844CE327,
              .on_inject = [](auto*) { return false; },
              .on_draw = [](auto* cmd_list) {
                if (mgsv::lighting::UsesEnhancedIndirectLighting()) {
                  mgsv::lighting::ambient_diffuse::Capture(cmd_list);
                }
                return true; },
          },
      },
      // CopyBuffer copies the diffuse light buffer before the skin-scatter passes;
      // enhanced lighting prepares its diffuse source from this copy.
      {
          0xD71E843E,
          {
              .crc32 = 0xD71E843E,
              .code = __0xD71E843E,
              .on_replace = &mgsv::lighting::source_preparation::IsActive,
              .on_inject = [](auto*) { return false; },
              .on_draw = [](auto* cmd_list) {
                if (!mgsv::lighting::UsesEnhancedIndirectLighting()) return true;
                return mgsv::lighting::source_preparation::Begin(cmd_list); },
              .on_drawn = [](auto* cmd_list) {
                auto prepared = mgsv::lighting::source_preparation::End(cmd_list);
                if (mgsv::lighting::UsesEnhancedIndirectLighting()) {
                  mgsv::lighting::ambient_diffuse::OnDiffuseCopied(cmd_list, std::move(prepared));
                } },
          },
      },
      // Rain G-buffer material layers.
      {0x2C230875, {.crc32 = 0x2C230875, .code = __0x2C230875, .on_replace = &mgsv::lighting::material_inputs::IsCaptureActive, .on_inject = [](auto*) { return false; }, .on_draw = &mgsv::lighting::material_inputs::OnRainDraw, .on_drawn = &mgsv::lighting::material_inputs::EndCapture}},
      {0xC3538951, {.crc32 = 0xC3538951, .code = __0xC3538951, .on_replace = &mgsv::lighting::material_inputs::IsCaptureActive, .on_inject = [](auto*) { return false; }, .on_draw = &mgsv::lighting::material_inputs::OnRainDraw, .on_drawn = &mgsv::lighting::material_inputs::EndCapture}},
      // Bloom and lens-flare chain, replaced only for draws in that chain.
      MGSV_BLOOM_SHADER(0x72C1D157, true),
      MGSV_BLOOM_SHADER(0x463195C2, false),
      MGSV_BLOOM_SHADER(0x1AC1A5EF, false),
      MGSV_BLOOM_SHADER(0x265053A6, false),
      MGSV_BLOOM_SHADER(0x9BE51873, false),
      // Motion blur's tile pass and its velocity and depth bake, the two passes
      // that read the combined motion image. The bake also scales motion for
      // upscaled scenes.
      {0x260C371F, {.crc32 = 0x260C371F, .code = __0x260C371F, .on_replace = &IsTemporalReconstructionOn, .on_inject = [](auto*) { return false; }}},
      {
          0x512E2B48,
          {
              .crc32 = 0x512E2B48,
              .code = __0x512E2B48,
              .on_replace = [](auto* cmd_list) {
                if (IsTemporalReconstructionOn(cmd_list)) return true;
                const auto output = mgsv::reconstruction::output_size::GetActive(cmd_list);
                return output != nullptr && output->IsScaled(); },
              .on_inject = [](auto*) { return false; },
          },
      },
      // CopyRenderBuffer; its copy of the bloom result ends the bloom chain.
      {
          0x83272BCB,
          {
              .crc32 = 0x83272BCB,
              .code = __0x83272BCB,
              .on_inject = [](auto*) { return false; },
              .on_draw = &mgsv::bloom::OnCopyRenderBufferDraw,
          },
      },

      // Deferred composition variants. They apply the game's filmic tone curve,
      // before the later LUT/color-grading pass, and write the engine's float
      // scene color.
      MGSV_DEFERRED_COMPOSITE_SHADER(0xC973024D),  // DeferredRenderingFilmic_VolFog
      MGSV_DEFERRED_COMPOSITE_SHADER(0xBDE1F4CD),  // DR_TonemapRainFilter
      MGSV_DEFERRED_COMPOSITE_SHADER(0x6E29F0AB),  // DR_TonemapRainFilter_NoIR
      MGSV_DEFERRED_COMPOSITE_SHADER(0x2EA8F13F),  // DR_VolFog_TppTonemap
      MGSV_DEFERRED_COMPOSITE_SHADER(0x59B44963),  // DR_VolFog_TppTonemap_MD

      // FXAA.
      {
          0x900968FF,
          {
              .crc32 = 0x900968FF,
              .code = __0x900968FF,
          },
      },

      // The named Tonemap shader is the late LUT/color-grading pass in MGSV.
      {
          0xE04D1471,
          {
              .crc32 = 0xE04D1471,
              .code = __0xE04D1471,
          },
      },

      MGSV_DEFERRED_COMPOSITE_SHADER(0x410AE8C5),  // DeferredRenderingFilmic

      // Sun and local light draws. Type 0 is the sun; 1 to 4 are local light types.
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
      // SunLight_ES gets only the environment-specular capture, no enhanced shadows.
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

      // Light variants by type. Each port keeps its native masks and material
      // response. Lights without shadow maps still get contact shadows and the
      // same spotlight limit.
      {0x0636037A, local_shadow_shader(0x0636037A, __0x0636037A, 2u, true)},
      // The largest-cascade sun fades its shadows to a world shadow mask (t13).
      {0x0C708B4B, directional_shadow_shader(0x0C708B4B, __0x0C708B4B, true, true)},
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

      // Native reflection trace and its composite.
      {0xD6714838, {.crc32 = 0xD6714838, .code = __0xD6714838, .on_draw = &mgsv::lighting::reflections::native_chain::OnTrace}},
      {0xDC3A84C3, {.crc32 = 0xDC3A84C3, .code = __0xDC3A84C3, .on_draw = &mgsv::lighting::reflections::BeginComposite, .on_drawn = [](auto* cmd_list) { mgsv::lighting::reflections::EndComposite(cmd_list); mgsv::lighting::reflections::native_chain::OnResetCommandList(cmd_list); }}},
      // Velocity shaders opt out of constant injection. Their targets are
      // created as float (engine/render_targets.hpp), and projection_jitter.hpp
      // fixes the G-buffer pair's previous-position matrices when the engine
      // uploads them. While temporal reconstruction is on, the G-buffer pair's
      // ports write object motion in precise form and the camera-velocity port
      // writes the combined image in it (rendering/motion.hlsli); with it off
      // the game's shaders run.
      {0x9815404F, {.crc32 = 0x9815404F, .code = __0x9815404F, .on_replace = &IsTemporalReconstructionOn, .on_inject = [](auto*) { return false; }}},
      {0x58C10658, {.crc32 = 0x58C10658, .code = __0x58C10658, .on_replace = &IsTemporalReconstructionOn, .on_inject = [](auto*) { return false; }}},
      {0xA13321B6, {.crc32 = 0xA13321B6, .code = __0xA13321B6, .on_replace = &IsTemporalReconstructionOn, .on_inject = [](auto*) { return false; }}},
      // The fog volume passes and the enhanced diffuse lighting passes run just
      // before the first native skin-scatter pass. The scatter shader itself
      // stays native, because a port of it did not match its normal and angle
      // math.
      {
          0x6DAF0C4A,
          {
              .crc32 = 0x6DAF0C4A,
              .on_inject = [](auto*) { return false; },
              .on_draw = [shader_injection](auto* cmd_list) {
                mgsv::atmosphere::fog::Apply(cmd_list);
                mgsv::lighting::ApplyLightPasses(cmd_list, *shader_injection);
                return true; },
          },
      },
      // The second skin-scatter pass also stays native; the lighting passes run only once.
      {
          0xB2601810,
          {
              .crc32 = 0xB2601810,
              // Do not inject constants into the game's shader. Each added pass binds its own.
              .on_inject = [](reshade::api::command_list*) { return false; },
          },
      },

  };
  for (const auto& material : mgsv::lighting::material_inputs::MATERIAL_SHADERS) {
    shaders.emplace(material.hash, renodx::mods::shader::CustomShader{
                                       .crc32 = material.hash,
                                       .code = material.code,
                                       .on_replace = &mgsv::lighting::material_inputs::IsCaptureActive,
                                       .on_inject = [](auto*) { return false; },
                                       .on_draw = &mgsv::lighting::material_inputs::OnMaterialDraw,
                                       .on_drawn = &mgsv::lighting::material_inputs::EndCapture,
                                   });
  }
  // Both shaders that fill the fog volume keep running; their draw's viewport
  // is widened to the volume's created size. Only VolFog_TppVolFog (0x99BEE81F)
  // draws with the fog material the mod's volume pass reads.
  for (const uint32_t hash : {0x99BEE81Fu, 0xD76ECA29u}) {
    shaders.emplace(hash, renodx::mods::shader::CustomShader{.crc32 = hash, .on_inject = [](auto*) { return false; }, .on_draw = [hash](auto* cmd_list) { return mgsv::atmosphere::fog::OnVolumeDraw(cmd_list, hash == 0x99BEE81Fu); }, .on_drawn = &mgsv::atmosphere::fog::OnVolumeDrawn});
  }
  // Lit particles the game draws without the sun's shadow maps (the NS and
  // Fast variants of Prim_Poly_Lit* that read the fog volume). Their ports
  // scale the sun's term by the fog volume's sun visibility at the particle.
  for (const auto& [hash, code] : std::to_array<std::pair<uint32_t, std::span<const uint8_t>>>({
           {0xB170995Du, __0xB170995D},
           {0xA21162DFu, __0xA21162DF},
           {0x42EA9E8Cu, __0x42EA9E8C},
           {0x835A5097u, __0x835A5097},
           {0xD350003Au, __0xD350003A},
           {0x6A9A6354u, __0x6A9A6354},
           {0x73CC2D1Eu, __0x73CC2D1E},
           {0x7C887529u, __0x7C887529},
       })) {
    shaders.emplace(hash, renodx::mods::shader::CustomShader{
                              .crc32 = hash,
                              .code = code,
                              .on_replace = &mgsv::atmosphere::fog::IsParticleInputBound,
                              .on_inject = [](auto*) { return false; },
                              .on_draw = &mgsv::atmosphere::fog::OnParticleDraw,
                              .on_drawn = &mgsv::atmosphere::fog::OnParticleDrawn,
                          });
  }
  // The terrain depth and shadow draws (NewTerrain_Depth, NewTerrain_Shadow
  // and the _ln variant of each) show the fog volume where the map's height
  // textures are.
  for (const uint32_t hash : {0x4710870Eu, 0xFC72369Au, 0xD68FF97Au, 0xEAF0D644u}) {
    shaders.emplace(hash, renodx::mods::shader::CustomShader{.crc32 = hash, .on_inject = [](auto*) { return false; }, .on_draw = &mgsv::atmosphere::fog::OnTerrainDraw});
  }
  // Enhanced reflections cancel both the general and the sortie-floor native
  // traces. The shared filters are skipped only when they follow a canceled trace.
  shaders.emplace(0xA621C684u, renodx::mods::shader::CustomShader{.crc32 = 0xA621C684u, .code = __0xA621C684, .on_inject = [](auto*) { return false; }, .on_draw = &mgsv::lighting::reflections::native_chain::OnTrace});
  for (const uint32_t hash : {0xBC4665A3u, 0xB301C16Du}) {
    shaders.emplace(hash, renodx::mods::shader::CustomShader{.crc32 = hash, .on_inject = [](auto*) { return false; }, .on_draw = &mgsv::lighting::reflections::native_chain::OnTrace});
  }
  for (const uint32_t hash : {0x7B69DE2Bu, 0xE135BBD1u, 0x051711F2u, 0xA8529A23u}) {
    shaders.emplace(hash, renodx::mods::shader::CustomShader{.crc32 = hash, .on_inject = [](auto*) { return false; }, .on_draw = &mgsv::lighting::reflections::native_chain::OnFilter});
  }
  // Leave the native mask intact: it can precede selection of the planar path.
  shaders.emplace(0x93B69040u, renodx::mods::shader::CustomShader{.crc32 = 0x93B69040u, .code = __0x93B69040, .on_draw = &mgsv::lighting::reflections::BeginComposite, .on_drawn = [](auto* cmd_list) { mgsv::lighting::reflections::EndComposite(cmd_list); mgsv::lighting::reflections::native_chain::OnResetCommandList(cmd_list); }});
  // Deferred composition variants without a port keep the native shader and log
  // once when the game first uses them.
  struct NativeVariant {
    uint32_t hash;
    const char* message;
    reshade::log::level level = reshade::log::level::warning;
  };
  static constexpr auto NATIVE_VARIANTS = std::to_array<NativeVariant>({
      {.hash = 0xEF64DAEB, .message = "MgsvLighting: DeferredRendering 0xEF64DAEB observed; native shader retained, color path unverified"},
      {.hash = 0xCAE40369, .message = "MgsvLighting: DeferredRendering_VolFog 0xCAE40369 observed; native shader retained, color path unverified"},
      {.hash = 0x4B10100E, .message = "MgsvLighting: DeferredRenderingTone 0x4B10100E observed; native shader retained, color path unverified"},
      {.hash = 0xB42DC0EF, .message = "MgsvLighting: DeferredRenderingTone_GVFog 0xB42DC0EF observed; native shader retained, color path unverified"},
  });
  static std::array<std::atomic_flag, NATIVE_VARIANTS.size()> reported_variants = {};
  for (uint32_t index = 0u; index < NATIVE_VARIANTS.size(); ++index) {
    const auto& variant = NATIVE_VARIANTS[index];
    shaders.emplace(variant.hash, renodx::mods::shader::CustomShader{
                                      .crc32 = variant.hash,
                                      .on_inject = [](auto*) { return false; },
                                      .on_draw = [index, message = variant.message, level = variant.level](auto*) {
          if (!reported_variants[index].test_and_set(std::memory_order_relaxed)) {
            reshade::log::message(level, message);
          }
          return true; },
                                  });
  }
  // The game's shader set holds some shaders twice, with the same instructions
  // and bindings under two hashes, and runs one of each pair; which one is not
  // known in advance. A port named by one hash also replaces its twin.
  static constexpr auto TWINS = std::to_array<std::pair<uint32_t, std::span<const uint8_t>>>({
      {0xBA3880D3, __0x1A96649E},  // Primitive_TppFourierOceanHQ
      {0xE2A104C7, __0x42D27394},  // Primitive_TppFourierOceanLQ
      {0xE4387947, __0xEBCBED4E},  // Primitive_TppFourierOceanFill
      {0x51BA0392, __0x52398872},  // Primitive_Tpp24hOcean
      {0x0092C258, __0xC15B18F4},  // Primitive_TppBaseOcean
      {0xCC05FAFC, __0x49CCEBC1},  // Primitive_TppSandWind
      {0x91C4A87A, __0xC4B7795C},  // tpp3dfw_Wormhole
      {0xC4BE6ECB, __0x2B7E283E},  // tpp3dfw_WormholeForBigThings
      {0xE9351D6A, __0x2B479DA0},  // tpp3dfw_WormholeBigThingsMoving
      {0x9E5DECF7, __0xE847AD2B},  // tpp3dfw_WormholeMovingEffect
      {0xB26F03DB, __0xB4AF3A07},  // Prim_Poly_FastDraw_SSky
      {0x5617D364, __0x60A50F81},  // Prim_Poly_FastDraw_SSky_VF
  });
  for (const auto& [twin, code] : TWINS) {
    shaders.emplace(twin, renodx::mods::shader::CreateCustomShader(twin, code));
  }
  // merge keeps existing keys, so the entries above win over generated ones.
  shaders.merge(renodx::mods::shader::CustomShaders{__ALL_CUSTOM_SHADERS});
  return shaders;
}

#undef MGSV_DEFERRED_COMPOSITE_SHADER
#undef MGSV_BLOOM_SHADER

}  // namespace mgsv::game
