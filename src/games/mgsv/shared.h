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

  float custom_bloom;
  float custom_boost_sun;
  float custom_random;
  float custom_grain_strength;

  // TemporalMode: 0 = Off, 1 = Analytical TAA, 2 = AMD FSR3, 3 = NVIDIA DLSS.
  float custom_taa;
  float unclamp_motion_vectors;

  float swap_chain_output_preset;
  float custom_motion_blur;

  // One switch controls PCSS and contact shadows for every supported light.
  float enhanced_shadows_enabled;

  float shadow_softness;  // Multiplies tuned source sizes; 1 keeps the default softness.
  float ss_shadow_reach;  // Contact-shadow search length in output pixels; effect-local copies convert to render pixels.
  float shadow_map_debug_view;

  // Indirect Lighting controls, in GUI order.
  float gt_enabled;  // 0 = Off, 1 = GT-VBGI.
  float ao_strength;
  float gi_strength;
  float specular_gi_strength;
  float specular_occlusion_strength;
  float gt_resolution;  // Pixel count: 0 = full, 1 = 1/4, 2 = 1/16, 3 = 1/64.
  float gt_search_radius_pixels;
  float gt_surface_thickness;  // Native view-depth units.
  float gt_surface_offset;     // Receiver offset along the shading normal, in native view-space units.
  float gt_slice_count;
  float gt_samples_per_side;
  float gt_depth_mips_enabled;  // User choice; each dispatch copy records whether the mips are ready.
  float gi_reject_backfaces;
  float gt_temporal_enabled;
  // AO and specular visibility share these history checks, with separate clamps.
  float occlusion_history_weight;
  float occlusion_depth_rejection;
  float occlusion_normal_rejection;
  float ao_history_clamp;
  float specular_history_clamp;
  float gi_history_weight;
  float gi_depth_rejection;
  float gi_normal_rejection;
  float gi_history_clamp;
  float gt_spatial_enabled;
  float gt_filter_radius;
  float gt_filter_passes;
  float gt_filter_taps;
  float gt_debug_view;

  // Dispatch-local values, separate from user controls.
  float gt_resolution_divisor;  // Full-resolution pixels per trace pixel along each axis: 1, 2, 4, or 8.
  float gt_noise_frame;         // Changes the FAST sample sequence; does not prove history is valid.
  float gi_history_valid;
  float gi_temporal_active;
  float gt_filter_step;
  float gt_current_jitter_x;
  float gt_current_jitter_y;
  float gt_previous_jitter_x;
  float gt_previous_jitter_y;
  float gt_velocity_jitter_scale;
  float ao_history_valid;
  float ao_temporal_active;
  float gt_specular_active;  // Dispatch-local input/output availability.
  float gt_specular_history_valid;

  // Bend contact-shadow controls and values copied for this light draw.
  float ss_shadow_debug_view;
  float ss_shadow_debug_light;
  float ss_shadow_light_type;  // 0 = directional, 1/2 = spot/point, 3/4 = distance spot/point.

  // Local-light contact work limits.
  float ss_spot_budget;  // Maximum spotlight evaluations per lighting-buffer fill; 0 = unlimited.

  // Shared IS-FAST frame phase.
  float shadow_noise_frame;

  float bloom_type;  // 0 = Vanilla, 1 = Enhanced.

  // Dispatch-local specular transport availability and history.
  float gt_specular_gi_active;
  float gt_specular_gi_history_valid;
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

#define CUSTOM_BLOOM          shader_injection.custom_bloom
#define CUSTOM_BLOOM_TYPE     shader_injection.bloom_type
#define CUSTOM_BOOST_SUN      shader_injection.custom_boost_sun
#define CUSTOM_RANDOM         shader_injection.custom_random
#define CUSTOM_GRAIN_STRENGTH shader_injection.custom_grain_strength
#define CUSTOM_MOTION_BLUR    shader_injection.custom_motion_blur

#define CUSTOM_TAA                    shader_injection.custom_taa
#define CUSTOM_UNCLAMP_MOTION_VECTORS shader_injection.unclamp_motion_vectors

#define CUSTOM_ENHANCED_SHADOWS_ENABLED (shader_injection.enhanced_shadows_enabled >= 0.5)

#define CUSTOM_SHADOW_SOFTNESS       shader_injection.shadow_softness
#define CUSTOM_SS_SHADOW_REACH       shader_injection.ss_shadow_reach
#define CUSTOM_SHADOW_MAP_DEBUG_VIEW shader_injection.shadow_map_debug_view
#define CUSTOM_SHADOW_NOISE_FRAME    shader_injection.shadow_noise_frame

#define CUSTOM_SS_DEBUG_VIEW  shader_injection.ss_shadow_debug_view
#define CUSTOM_SS_DEBUG_LIGHT shader_injection.ss_shadow_debug_light
#define CUSTOM_SS_LIGHT_TYPE  shader_injection.ss_shadow_light_type

#define CUSTOM_SS_SPOT_BUDGET shader_injection.ss_spot_budget

#define CUSTOM_MGSV_LIGHTING_ENABLED            (shader_injection.gt_enabled >= 0.5)
#define CUSTOM_MGSV_AO_STRENGTH                 shader_injection.ao_strength
#define CUSTOM_MGSV_GI_STRENGTH                 shader_injection.gi_strength
#define CUSTOM_MGSV_SPECULAR_GI_STRENGTH        shader_injection.specular_gi_strength
#define CUSTOM_MGSV_SPECULAR_OCCLUSION_STRENGTH shader_injection.specular_occlusion_strength
#define CUSTOM_MGSV_GT_SEARCH_RADIUS_PIXELS     shader_injection.gt_search_radius_pixels
#define CUSTOM_MGSV_GT_SURFACE_THICKNESS        shader_injection.gt_surface_thickness
#define CUSTOM_MGSV_GT_SURFACE_OFFSET           shader_injection.gt_surface_offset
#define CUSTOM_MGSV_GT_SLICE_COUNT              shader_injection.gt_slice_count
#define CUSTOM_MGSV_GT_SAMPLES_PER_SIDE         shader_injection.gt_samples_per_side
#define CUSTOM_MGSV_GT_DEPTH_MIPS_ENABLED       shader_injection.gt_depth_mips_enabled
#define CUSTOM_MGSV_GI_REJECT_BACKFACES         shader_injection.gi_reject_backfaces
#define CUSTOM_MGSV_GT_TEMPORAL_ENABLED         shader_injection.gt_temporal_enabled
#define CUSTOM_MGSV_OCCLUSION_HISTORY_WEIGHT    shader_injection.occlusion_history_weight
#define CUSTOM_MGSV_OCCLUSION_DEPTH_REJECTION   shader_injection.occlusion_depth_rejection
#define CUSTOM_MGSV_OCCLUSION_NORMAL_REJECTION  shader_injection.occlusion_normal_rejection
#define CUSTOM_MGSV_AO_HISTORY_CLAMP            shader_injection.ao_history_clamp
#define CUSTOM_MGSV_SPECULAR_HISTORY_CLAMP      shader_injection.specular_history_clamp
#define CUSTOM_MGSV_GI_HISTORY_WEIGHT           shader_injection.gi_history_weight
#define CUSTOM_MGSV_GI_DEPTH_REJECTION          shader_injection.gi_depth_rejection
#define CUSTOM_MGSV_GI_NORMAL_REJECTION         shader_injection.gi_normal_rejection
#define CUSTOM_MGSV_GI_HISTORY_CLAMP            shader_injection.gi_history_clamp
#define CUSTOM_MGSV_GT_SPATIAL_ENABLED          shader_injection.gt_spatial_enabled
#define CUSTOM_MGSV_GT_FILTER_TAPS              shader_injection.gt_filter_taps
#define CUSTOM_MGSV_GT_DEBUG_VIEW               shader_injection.gt_debug_view

#define CUSTOM_MGSV_GT_RESOLUTION_DIVISOR     shader_injection.gt_resolution_divisor
#define CUSTOM_MGSV_GT_NOISE_FRAME            shader_injection.gt_noise_frame
#define CUSTOM_MGSV_GI_HISTORY_VALID          shader_injection.gi_history_valid
#define CUSTOM_MGSV_GI_TEMPORAL_ACTIVE        shader_injection.gi_temporal_active
#define CUSTOM_MGSV_GT_FILTER_STEP            shader_injection.gt_filter_step
#define CUSTOM_MGSV_GT_CURRENT_JITTER_X       shader_injection.gt_current_jitter_x
#define CUSTOM_MGSV_GT_CURRENT_JITTER_Y       shader_injection.gt_current_jitter_y
#define CUSTOM_MGSV_GT_PREVIOUS_JITTER_X      shader_injection.gt_previous_jitter_x
#define CUSTOM_MGSV_GT_PREVIOUS_JITTER_Y      shader_injection.gt_previous_jitter_y
#define CUSTOM_MGSV_GT_VELOCITY_JITTER_SCALE  shader_injection.gt_velocity_jitter_scale
#define CUSTOM_MGSV_AO_HISTORY_VALID          shader_injection.ao_history_valid
#define CUSTOM_MGSV_AO_TEMPORAL_ACTIVE        shader_injection.ao_temporal_active
#define CUSTOM_MGSV_SPECULAR_ACTIVE           shader_injection.gt_specular_active
#define CUSTOM_MGSV_SPECULAR_HISTORY_VALID    shader_injection.gt_specular_history_valid
#define CUSTOM_MGSV_SPECULAR_GI_ACTIVE        shader_injection.gt_specular_gi_active
#define CUSTOM_MGSV_SPECULAR_GI_HISTORY_VALID shader_injection.gt_specular_gi_history_valid

#include "../../shaders/renodx.hlsl"

#endif

#endif  // SRC_MGSV_SHARED_H_
