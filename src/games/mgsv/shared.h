#ifndef SRC_MGSV_SHARED_H_
#define SRC_MGSV_SHARED_H_

#define FIX_UNORM_SRGB 1

struct ShaderInjectData {
  float tone_map_type;
  float peak_white_nits;
  float diffuse_white_nits;
  float graphics_white_nits;
  float gamma_correction;

  float tone_map_exposure;
  float tone_map_highlights;
  float tone_map_contrast_highlights;
  float tone_map_shadows;
  float tone_map_contrast;
  float tone_map_contrast_shadows;
  float tone_map_saturation;
  float tone_map_highlight_saturation;
  float tone_map_dechroma;
  float tone_map_flare;
  float scene_grade_strength;
  float scene_grade_scaling;

  float custom_bloom_type;
  float custom_bloom;
  float custom_boost_sun;
  float custom_random;
  float custom_grain_strength;

  // TemporalMode: 0 = Off, 1 = Analytical TAA, 2 = AMD FSR3, 3 = NVIDIA DLSS.
  float custom_taa;
  float unclamp_motion_vectors;

  float swap_chain_output_preset;
  float custom_motion_blur;

  // Contact-hardening shadows. Appended after every existing field so no
  // established offset moves.
  float sun_contact_hardening;
  float sun_shadow_softness;
  float sun_shadow_k;
  float sun_shadow_blur_pixels;

  // Spot lights. Separate values because the scale differs: a spot's cone is
  // tens of metres where the sun's cascade is hundreds, so the same separation
  // produces a much larger ratio.
  float spot_contact_hardening;
  float spot_shadow_softness;
  float spot_shadow_k;
  float spot_shadow_blur_pixels;
  float spot_shadow_darkness;
  float spot_cone_falloff;

  // Screen-space shadows. Appended after every existing field so no established
  // offset moves.
  float screen_space_shadows;
  float ss_shadow_reach;
  float ss_shadow_thickness;
  float ss_shadow_strength;
  float ss_shadow_fade_end;
  float ss_shadow_material_max;

  // Ambient occlusion, applied after the game's subsurface-scatter pass.
  //
  // ao_enabled is read *host-side*, by the dispatch callbacks, so the toggle needs no
  // shader compile and the compute shader keeps no constant buffer dependency.
  // The rest are used by the AO shader itself.
  float ao_enabled;
  float ao_strength;
  float ao_radius;
  float ao_slices;
  float ao_steps;
  float ao_thickness;
  float ao_fade;
  // Host-side: 1 while the depth mip chain is built for this frame. The pass reads coarse
  // levels only when it is set, so an unbindable chain degrades to the exact fetch instead
  // of reading a fallback texel.
  float ao_depth_mips;

  // Screen-space diffuse gather, dispatched after the AO.
  //
  // il_enabled is host-side, like ao_enabled; the rest are read by the gather.
  float il_enabled;
  // Fraction of the neighbourhood's mean radiance added to the light.
  float il_amount;
  // Gather radius as a fraction of screen height.
  float il_radius;
  // Samples per pixel.
  float il_samples;
  // Contributors required before a pixel is allowed to add any bounce.
  float il_min_samples;
  // 1 writes the gather's term alone into the light, for judging it.
  float il_debug;
};

#ifndef __cplusplus
cbuffer cb13 : register(b13) {
  ShaderInjectData shader_injection : packoffset(c0);
}

#define RENODX_TONE_MAP_TYPE            shader_injection.tone_map_type
#define RENODX_PEAK_WHITE_NITS          shader_injection.peak_white_nits
#define RENODX_DIFFUSE_WHITE_NITS       shader_injection.diffuse_white_nits
#define RENODX_GRAPHICS_WHITE_NITS      shader_injection.graphics_white_nits
#define RENODX_GAMMA_CORRECTION         shader_injection.gamma_correction
#define RENODX_SWAP_CHAIN_OUTPUT_PRESET shader_injection.swap_chain_output_preset

#define RENODX_TONE_MAP_EXPOSURE             shader_injection.tone_map_exposure
#define RENODX_TONE_MAP_HIGHLIGHTS           shader_injection.tone_map_highlights
#define RENODX_TONE_MAP_CONTRAST_HIGHLIGHTS  shader_injection.tone_map_contrast_highlights
#define RENODX_TONE_MAP_SHADOWS              shader_injection.tone_map_shadows
#define RENODX_TONE_MAP_CONTRAST_SHADOWS     shader_injection.tone_map_contrast_shadows
#define RENODX_TONE_MAP_CONTRAST             shader_injection.tone_map_contrast
#define RENODX_TONE_MAP_SATURATION           shader_injection.tone_map_saturation
#define RENODX_TONE_MAP_HIGHLIGHT_SATURATION shader_injection.tone_map_highlight_saturation
#define RENODX_TONE_MAP_DECHROMA             shader_injection.tone_map_dechroma
#define RENODX_TONE_MAP_FLARE                shader_injection.tone_map_flare
#define RENODX_COLOR_GRADE_STRENGTH          shader_injection.scene_grade_strength
#define RENODX_COLOR_GRADE_SCALING           shader_injection.scene_grade_scaling

#define CUSTOM_BLOOM_TYPE     shader_injection.custom_bloom_type
#define CUSTOM_BLOOM          shader_injection.custom_bloom
#define CUSTOM_BOOST_SUN      shader_injection.custom_boost_sun
#define CUSTOM_RANDOM         shader_injection.custom_random
#define CUSTOM_GRAIN_STRENGTH shader_injection.custom_grain_strength
#define CUSTOM_MOTION_BLUR    shader_injection.custom_motion_blur

#define CUSTOM_TAA                    shader_injection.custom_taa
#define CUSTOM_UNCLAMP_MOTION_VECTORS shader_injection.unclamp_motion_vectors

#define CUSTOM_SUN_CONTACT_HARDENING shader_injection.sun_contact_hardening
#define CUSTOM_SUN_SHADOW_SOFTNESS   shader_injection.sun_shadow_softness
#define CUSTOM_SUN_SHADOW_K          shader_injection.sun_shadow_k
#define CUSTOM_SUN_BLUR_PIXELS       shader_injection.sun_shadow_blur_pixels

#define CUSTOM_SPOT_CONTACT_HARDENING shader_injection.spot_contact_hardening
#define CUSTOM_SPOT_SHADOW_SOFTNESS   shader_injection.spot_shadow_softness
#define CUSTOM_SPOT_SHADOW_K          shader_injection.spot_shadow_k
#define CUSTOM_SPOT_BLUR_PIXELS       shader_injection.spot_shadow_blur_pixels
#define CUSTOM_SPOT_SHADOW_DARKNESS   shader_injection.spot_shadow_darkness
#define CUSTOM_SPOT_CONE_FALLOFF      shader_injection.spot_cone_falloff

#define CUSTOM_SS_SHADOWS      shader_injection.screen_space_shadows
#define CUSTOM_SS_REACH        shader_injection.ss_shadow_reach
#define CUSTOM_SS_THICKNESS    shader_injection.ss_shadow_thickness
#define CUSTOM_SS_STRENGTH     shader_injection.ss_shadow_strength
#define CUSTOM_SS_FADE_END     shader_injection.ss_shadow_fade_end
#define CUSTOM_SS_MATERIAL_MAX shader_injection.ss_shadow_material_max

#define CUSTOM_MGSV_AO_STRENGTH  shader_injection.ao_strength
#define CUSTOM_MGSV_AO_RADIUS    shader_injection.ao_radius
#define CUSTOM_MGSV_AO_SLICES    shader_injection.ao_slices
#define CUSTOM_MGSV_AO_STEPS     shader_injection.ao_steps
#define CUSTOM_MGSV_AO_THICKNESS shader_injection.ao_thickness
#define CUSTOM_MGSV_AO_FADE      shader_injection.ao_fade
#define CUSTOM_MGSV_AO_DEPTH_MIPS shader_injection.ao_depth_mips

#define CUSTOM_MGSV_IL_AMOUNT shader_injection.il_amount
#define CUSTOM_MGSV_IL_RADIUS shader_injection.il_radius
#define CUSTOM_MGSV_IL_SAMPLES shader_injection.il_samples
#define CUSTOM_MGSV_IL_MIN_SAMPLES shader_injection.il_min_samples
#define CUSTOM_MGSV_IL_DEBUG  shader_injection.il_debug

#include "../../shaders/renodx.hlsl"

#endif

#endif  // SRC_MGSV_SHARED_H_
