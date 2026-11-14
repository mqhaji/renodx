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

  float bloom_strength;
  float sun_brightness_boost_enabled;
  float film_grain_random;
  float film_grain_strength;

  // TemporalMode: 0 = Off, 1 = Analytical TAA, 2 = FSR, 3 = NVIDIA DLSS.
  float reconstruction_mode;
  float unclamp_motion_vectors;

  float swap_chain_output_preset;
  float motion_blur_strength;

  // One switch controls PCSS and contact shadows for every supported light.
  float enhanced_shadows_enabled;

  float shadow_softness;              // Multiplies tuned source sizes; 1 keeps the default softness.
  float contact_shadow_reach_pixels;  // Contact-shadow search length in output pixels; effect-local copies convert to render pixels.
  float shadow_map_debug_view;

  // Indirect Lighting controls, in GUI order.
  float indirect_lighting_enabled;  // 0 = Off, 1 = GT-VBGI.
  float ao_strength;
  float diffuse_gi_strength;
  float specular_gi_strength;
  float specular_occlusion_strength;
  float lighting_resolution;  // Pixel count: 0 = full, 1 = 1/4, 2 = 1/16, 3 = 1/64.
  float lighting_search_radius_pixels;
  float lighting_surface_thickness;  // Native view-depth units.
  float lighting_surface_offset;     // Receiver offset along the shading normal, in native view-space units.
  float lighting_slice_count;
  float lighting_samples_per_side;
  float lighting_depth_mips_enabled;  // User choice; each dispatch copy records whether the mips are ready.
  float gi_reject_backfaces;
  float lighting_temporal_enabled;
  // AO and specular visibility share these history checks, with separate clamps.
  float occlusion_history_weight;
  float occlusion_depth_rejection;
  float occlusion_normal_rejection;
  float ao_history_clamp;
  float specular_occlusion_history_clamp;
  float gi_history_weight;
  float gi_depth_rejection;
  float gi_normal_rejection;
  float gi_history_clamp;
  float lighting_spatial_enabled;
  float lighting_filter_radius;
  float lighting_filter_passes;
  float lighting_filter_width;
  float lighting_debug_view;

  // Dispatch-local values, separate from user controls.
  float lighting_resolution_divisor;  // Full-resolution pixels per trace pixel along each axis: 1, 2, 4, or 8.
  float lighting_noise_frame;         // Changes the FAST sample sequence; does not prove history is valid.
  float gi_history_valid;
  float gi_temporal_active;
  float lighting_filter_step;
  float lighting_current_jitter_uv_x;
  float lighting_current_jitter_uv_y;
  float lighting_previous_jitter_uv_x;
  float lighting_previous_jitter_uv_y;
  float lighting_velocity_jitter_scale;
  float ao_history_valid;
  float ao_temporal_active;
  float specular_occlusion_active;  // Dispatch-local input/output availability.
  float specular_occlusion_history_valid;

  // Bend contact-shadow controls and values copied for this light draw.
  float contact_shadow_debug_view;
  float shadow_debug_light;
  float contact_shadow_light_type;  // 0 = directional, 1/2 = spot/point, 3/4 = distance spot/point.

  // Local-light contact work limits.
  float spot_contact_shadow_budget;  // Maximum spotlight evaluations per lighting-buffer fill; 0 = unlimited.

  // Shared IS-FAST frame phase.
  float shadow_noise_frame;

  float bloom_type;  // 0 = Vanilla, 1 = Enhanced.

  // Dispatch-local specular transport availability and history.
  float specular_gi_active;
  float specular_gi_history_valid;
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

#define CUSTOM_BLOOM_STRENGTH               shader_injection.bloom_strength
#define CUSTOM_BLOOM_TYPE                   shader_injection.bloom_type
#define CUSTOM_SUN_BRIGHTNESS_BOOST_ENABLED shader_injection.sun_brightness_boost_enabled
#define CUSTOM_FILM_GRAIN_RANDOM            shader_injection.film_grain_random
#define CUSTOM_FILM_GRAIN_STRENGTH          shader_injection.film_grain_strength
#define CUSTOM_MOTION_BLUR_STRENGTH         shader_injection.motion_blur_strength

#define CUSTOM_RECONSTRUCTION_MODE    shader_injection.reconstruction_mode
#define CUSTOM_UNCLAMP_MOTION_VECTORS shader_injection.unclamp_motion_vectors

#define CUSTOM_ENHANCED_SHADOWS_ENABLED (shader_injection.enhanced_shadows_enabled >= 0.5)

#define CUSTOM_SHADOW_SOFTNESS             shader_injection.shadow_softness
#define CUSTOM_CONTACT_SHADOW_REACH_PIXELS shader_injection.contact_shadow_reach_pixels
#define CUSTOM_SHADOW_MAP_DEBUG_VIEW       shader_injection.shadow_map_debug_view
#define CUSTOM_SHADOW_NOISE_FRAME          shader_injection.shadow_noise_frame

#define CUSTOM_CONTACT_SHADOW_DEBUG_VIEW shader_injection.contact_shadow_debug_view
#define CUSTOM_SHADOW_DEBUG_LIGHT        shader_injection.shadow_debug_light
#define CUSTOM_CONTACT_SHADOW_LIGHT_TYPE shader_injection.contact_shadow_light_type

#define CUSTOM_SPOT_CONTACT_SHADOW_BUDGET shader_injection.spot_contact_shadow_budget

#define CUSTOM_MGSV_LIGHTING_ENABLED                 (shader_injection.indirect_lighting_enabled >= 0.5)
#define CUSTOM_MGSV_AO_STRENGTH                      shader_injection.ao_strength
#define CUSTOM_MGSV_DIFFUSE_GI_STRENGTH              shader_injection.diffuse_gi_strength
#define CUSTOM_MGSV_SPECULAR_GI_STRENGTH             shader_injection.specular_gi_strength
#define CUSTOM_MGSV_SPECULAR_OCCLUSION_STRENGTH      shader_injection.specular_occlusion_strength
#define CUSTOM_MGSV_LIGHTING_SEARCH_RADIUS_PIXELS    shader_injection.lighting_search_radius_pixels
#define CUSTOM_MGSV_LIGHTING_SURFACE_THICKNESS       shader_injection.lighting_surface_thickness
#define CUSTOM_MGSV_LIGHTING_SURFACE_OFFSET          shader_injection.lighting_surface_offset
#define CUSTOM_MGSV_LIGHTING_SLICE_COUNT             shader_injection.lighting_slice_count
#define CUSTOM_MGSV_LIGHTING_SAMPLES_PER_SIDE        shader_injection.lighting_samples_per_side
#define CUSTOM_MGSV_LIGHTING_DEPTH_MIPS_ENABLED      shader_injection.lighting_depth_mips_enabled
#define CUSTOM_MGSV_GI_REJECT_BACKFACES              shader_injection.gi_reject_backfaces
#define CUSTOM_MGSV_LIGHTING_TEMPORAL_ENABLED        shader_injection.lighting_temporal_enabled
#define CUSTOM_MGSV_OCCLUSION_HISTORY_WEIGHT         shader_injection.occlusion_history_weight
#define CUSTOM_MGSV_OCCLUSION_DEPTH_REJECTION        shader_injection.occlusion_depth_rejection
#define CUSTOM_MGSV_OCCLUSION_NORMAL_REJECTION       shader_injection.occlusion_normal_rejection
#define CUSTOM_MGSV_AO_HISTORY_CLAMP                 shader_injection.ao_history_clamp
#define CUSTOM_MGSV_SPECULAR_OCCLUSION_HISTORY_CLAMP shader_injection.specular_occlusion_history_clamp
#define CUSTOM_MGSV_GI_HISTORY_WEIGHT                shader_injection.gi_history_weight
#define CUSTOM_MGSV_GI_DEPTH_REJECTION               shader_injection.gi_depth_rejection
#define CUSTOM_MGSV_GI_NORMAL_REJECTION              shader_injection.gi_normal_rejection
#define CUSTOM_MGSV_GI_HISTORY_CLAMP                 shader_injection.gi_history_clamp
#define CUSTOM_MGSV_LIGHTING_SPATIAL_ENABLED         shader_injection.lighting_spatial_enabled
#define CUSTOM_MGSV_LIGHTING_FILTER_WIDTH            shader_injection.lighting_filter_width
#define CUSTOM_MGSV_LIGHTING_DEBUG_VIEW              shader_injection.lighting_debug_view

#define CUSTOM_MGSV_LIGHTING_RESOLUTION_DIVISOR      shader_injection.lighting_resolution_divisor
#define CUSTOM_MGSV_LIGHTING_NOISE_FRAME             shader_injection.lighting_noise_frame
#define CUSTOM_MGSV_GI_HISTORY_VALID                 shader_injection.gi_history_valid
#define CUSTOM_MGSV_GI_TEMPORAL_ACTIVE               shader_injection.gi_temporal_active
#define CUSTOM_MGSV_LIGHTING_FILTER_STEP             shader_injection.lighting_filter_step
#define CUSTOM_MGSV_LIGHTING_CURRENT_JITTER_UV_X     shader_injection.lighting_current_jitter_uv_x
#define CUSTOM_MGSV_LIGHTING_CURRENT_JITTER_UV_Y     shader_injection.lighting_current_jitter_uv_y
#define CUSTOM_MGSV_LIGHTING_PREVIOUS_JITTER_UV_X    shader_injection.lighting_previous_jitter_uv_x
#define CUSTOM_MGSV_LIGHTING_PREVIOUS_JITTER_UV_Y    shader_injection.lighting_previous_jitter_uv_y
#define CUSTOM_MGSV_LIGHTING_VELOCITY_JITTER_SCALE   shader_injection.lighting_velocity_jitter_scale
#define CUSTOM_MGSV_AO_HISTORY_VALID                 shader_injection.ao_history_valid
#define CUSTOM_MGSV_AO_TEMPORAL_ACTIVE               shader_injection.ao_temporal_active
#define CUSTOM_MGSV_SPECULAR_OCCLUSION_ACTIVE        shader_injection.specular_occlusion_active
#define CUSTOM_MGSV_SPECULAR_OCCLUSION_HISTORY_VALID shader_injection.specular_occlusion_history_valid
#define CUSTOM_MGSV_SPECULAR_GI_ACTIVE               shader_injection.specular_gi_active
#define CUSTOM_MGSV_SPECULAR_GI_HISTORY_VALID        shader_injection.specular_gi_history_valid

#include "../../shaders/renodx.hlsl"

#endif

#endif  // SRC_MGSV_SHARED_H_
