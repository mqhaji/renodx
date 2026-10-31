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

  // Contact-hardening shadows for the directional light (sun or moon).
  float sun_contact_hardening;
  float sun_shadow_softness;
  float sun_shadow_k;
  float sun_shadow_blur_pixels;

  // Spotlights use their own softness and response settings.
  float spot_contact_hardening;
  float spot_shadow_softness;
  float spot_shadow_k;
  float spot_shadow_blur_pixels;
  float spot_shadow_darkness;
  float spot_cone_falloff;

  // Extra directional shadows from objects visible on screen.
  float screen_space_shadows;
  float ss_shadow_reach;
  float ss_shadow_thickness;
  float ss_shadow_strength;
  float ss_shadow_fade_end;
  float ss_shadow_material_max;

  // Extra spotlight shadows run separately for each light.
  float spot_screen_space_shadows;
  // Spotlight tuning is independent of directional shadows.
  float spot_ss_reach;
  float spot_ss_thickness;
  float spot_ss_strength;
  float spot_ss_fade_end;
  float spot_ss_material_max;
  // Point-light shadow filtering and screen-space shadows.
  float point_contact_hardening;
  float point_shadow_softness;
  float point_shadow_k;
  float point_shadow_blur_pixels;
  float point_shadow_darkness;
  float point_screen_space_shadows;
  float point_ss_reach;
  float point_ss_thickness;
  float point_ss_strength;
  float point_ss_fade_end;
  float point_ss_material_max;

  // Indirect Lighting controls, in GUI order.
  float lighting_mode;  // 0 = Off, 1 = On (AO + GI).
  float ao_strength;
  float gi_strength;
  float gt_resolution;  // 0 = full, 1 = half, 2 = quarter, 3 = eighth.
  float gt_search_radius_pixels;
  float gt_surface_thickness;  // Native view-depth units.
  float gt_surface_offset;     // Receiver offset along the shading normal, in native view-space units.
  float gt_slice_count;
  float gt_samples_per_side;
  float gt_depth_mips_enabled;  // Set by the CPU when requested depth mips are available.
  float gi_reject_backfaces;
  float gi_source_specular_weight;
  float gi_debug_view;

  // Dispatch-local values, separate from user controls.
  float gt_resolution_divisor;  // Full-resolution pixels per trace pixel along each axis: 1, 2, 4, or 8.
  float il_scale;               // Matches the shared trace resolution divisor.
  float gt_noise_frame;         // Ordered FAST sample index, not a history-validity token.
};

#ifndef __cplusplus
cbuffer RenoDXSettings : register(b13) {
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

#define CUSTOM_DIRECTIONAL_CONTACT_HARDENING         shader_injection.sun_contact_hardening
#define CUSTOM_DIRECTIONAL_SHADOW_SOFTNESS           shader_injection.sun_shadow_softness
#define CUSTOM_DIRECTIONAL_PENUMBRA_RESPONSE         shader_injection.sun_shadow_k
#define CUSTOM_DIRECTIONAL_PENUMBRA_SMOOTHING_RADIUS shader_injection.sun_shadow_blur_pixels

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

#define CUSTOM_SS_SPOT_SHADOWS      shader_injection.spot_screen_space_shadows
#define CUSTOM_SS_SPOT_REACH        shader_injection.spot_ss_reach
#define CUSTOM_SS_SPOT_THICKNESS    shader_injection.spot_ss_thickness
#define CUSTOM_SS_SPOT_STRENGTH     shader_injection.spot_ss_strength
#define CUSTOM_SS_SPOT_FADE_END     shader_injection.spot_ss_fade_end
#define CUSTOM_SS_SPOT_MATERIAL_MAX shader_injection.spot_ss_material_max

#define CUSTOM_POINT_CONTACT_HARDENING shader_injection.point_contact_hardening
#define CUSTOM_POINT_SHADOW_SOFTNESS   shader_injection.point_shadow_softness
#define CUSTOM_POINT_SHADOW_K          shader_injection.point_shadow_k
#define CUSTOM_POINT_BLUR_PIXELS       shader_injection.point_shadow_blur_pixels
#define CUSTOM_POINT_SHADOW_DARKNESS   shader_injection.point_shadow_darkness

#define CUSTOM_SS_POINT_SHADOWS      shader_injection.point_screen_space_shadows
#define CUSTOM_SS_POINT_REACH        shader_injection.point_ss_reach
#define CUSTOM_SS_POINT_THICKNESS    shader_injection.point_ss_thickness
#define CUSTOM_SS_POINT_STRENGTH     shader_injection.point_ss_strength
#define CUSTOM_SS_POINT_FADE_END     shader_injection.point_ss_fade_end
#define CUSTOM_SS_POINT_MATERIAL_MAX shader_injection.point_ss_material_max

#define CUSTOM_MGSV_AO_ENABLED                (shader_injection.lighting_mode >= 0.5)
#define CUSTOM_MGSV_AO_STRENGTH               shader_injection.ao_strength
#define CUSTOM_MGSV_GI_STRENGTH               shader_injection.gi_strength
#define CUSTOM_MGSV_GT_SEARCH_RADIUS_PIXELS   shader_injection.gt_search_radius_pixels
#define CUSTOM_MGSV_GT_SURFACE_THICKNESS      shader_injection.gt_surface_thickness
#define CUSTOM_MGSV_GT_SURFACE_OFFSET         shader_injection.gt_surface_offset
#define CUSTOM_MGSV_GT_SLICE_COUNT            shader_injection.gt_slice_count
#define CUSTOM_MGSV_GT_SAMPLES_PER_SIDE       shader_injection.gt_samples_per_side
#define CUSTOM_MGSV_GT_DEPTH_MIPS_ENABLED     shader_injection.gt_depth_mips_enabled
#define CUSTOM_MGSV_GI_REJECT_BACKFACES       shader_injection.gi_reject_backfaces
#define CUSTOM_MGSV_GI_SOURCE_SPECULAR_WEIGHT shader_injection.gi_source_specular_weight
#define CUSTOM_MGSV_GI_DEBUG_VIEW             shader_injection.gi_debug_view

#define CUSTOM_MGSV_GT_RESOLUTION_DIVISOR shader_injection.gt_resolution_divisor
#define CUSTOM_MGSV_IL_SCALE              shader_injection.il_scale
#define CUSTOM_MGSV_GT_NOISE_FRAME        shader_injection.gt_noise_frame

#include "../../shaders/renodx.hlsl"

#endif

#endif  // SRC_MGSV_SHARED_H_
