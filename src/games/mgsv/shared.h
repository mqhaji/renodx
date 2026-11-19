#ifndef SRC_MGSV_SHARED_H_
#define SRC_MGSV_SHARED_H_

#define FIX_UNORM_SRGB 1

// Reflection readiness values, stored as bytes in an R8 image, so shaders scale
// the loaded value by 255 before comparing. D3D11 and D3D12 passes both use them.
static const int MGSV_REFLECTION_UNAVAILABLE = 0;
static const int MGSV_REFLECTION_ENHANCED = 255;
static const int MGSV_REFLECTION_MAX_RAYS_PER_PIXEL = 4;

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

  float swap_chain_output_preset;
  float motion_blur_strength;

  // 0 = Vanilla, 1 = Enhanced shadow maps and contact shadows.
  float shadows_mode;

  float shadow_softness;              // Multiplies tuned source sizes; 1 keeps the default softness.
  float contact_shadow_reach_pixels;  // Contact-shadow search length in output pixels; effect-local copies convert to render pixels.
  float shadow_map_debug_view;

  // Indirect Lighting controls. Keep their storage order stable across passes.
  float indirect_lighting_mode;  // 0 = Vanilla, 1 = Enhanced diffuse AO/GI and reflections together.
  float ao_strength;
  float diffuse_gi_strength;
  float reflection_strength;  // Blends the complete screen/environment reflection replacement.
  float lighting_resolution;  // Pixel count: 0 = full, 1 = 1/4, 2 = 1/16, 3 = 1/64.
  float lighting_search_radius_pixels;
  float lighting_surface_thickness;  // Native view-depth units.
  float lighting_surface_offset;     // Receiver offset along the shading normal, in native view-space units.
  float lighting_slice_count;
  float lighting_samples_per_side;
  float lighting_depth_mips_enabled;  // User choice; each dispatch copy records whether the mips are ready.
  float gi_reject_backfaces;
  float lighting_debug_view;

  // Set by the host for each lighting dispatch, not by the menu.
  float lighting_resolution_divisor;  // Full-resolution pixels per trace pixel along each axis: 1, 2, 4, or 8.
  float lighting_noise_frame;         // Changes the FAST sample sequence; does not prove history is valid.

  // Bend contact-shadow controls and values copied for this light draw.
  float contact_shadow_debug_view;
  float shadow_debug_light;
  float contact_shadow_render_scale;  // Render pixels per output pixel along each axis; 1 without upscaling.

  // Local-light contact work limits.
  float spot_contact_shadow_budget;  // Maximum spotlight evaluations per lighting-buffer fill; 0 = unlimited.

  // IS-FAST noise layer (0-31) for shadows, advanced once per Present.
  float shadow_noise_frame;

  float bloom_type;                   // 0 = Vanilla, 1 = Enhanced.
  float specular_occlusion_strength;  // Blocks environment fallback along confirmed geometry hits.
  float reflection_rays_per_pixel;    // GGX samples per receiver; controls trace cost rather than strength.
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

#define CUSTOM_RECONSTRUCTION_MODE shader_injection.reconstruction_mode

#define CUSTOM_MGSV_ENHANCED_SHADOWS (shader_injection.shadows_mode >= 0.5)

#define CUSTOM_SHADOW_SOFTNESS             shader_injection.shadow_softness
#define CUSTOM_CONTACT_SHADOW_REACH_PIXELS shader_injection.contact_shadow_reach_pixels
#define CUSTOM_SHADOW_MAP_DEBUG_VIEW       shader_injection.shadow_map_debug_view
#define CUSTOM_SHADOW_NOISE_FRAME          shader_injection.shadow_noise_frame

#define CUSTOM_CONTACT_SHADOW_DEBUG_VIEW   shader_injection.contact_shadow_debug_view
#define CUSTOM_SHADOW_DEBUG_LIGHT          shader_injection.shadow_debug_light
#define CUSTOM_CONTACT_SHADOW_RENDER_SCALE shader_injection.contact_shadow_render_scale

#define CUSTOM_SPOT_CONTACT_SHADOW_BUDGET shader_injection.spot_contact_shadow_budget

// The vanilla tone curve runs inside deferred composition, before the late reflection pass could
// swap in linear light, so Enhanced needs the RenoDX tone mapper. OnPresent in addon.cpp applies
// the same rule on the host side.
#define CUSTOM_MGSV_ENHANCED_INDIRECT_LIGHTING    (shader_injection.indirect_lighting_mode >= 0.5 && RENODX_TONE_MAP_TYPE != 0.0)
#define CUSTOM_MGSV_AO_STRENGTH                   shader_injection.ao_strength
#define CUSTOM_MGSV_DIFFUSE_GI_STRENGTH           shader_injection.diffuse_gi_strength
#define CUSTOM_MGSV_REFLECTION_STRENGTH           shader_injection.reflection_strength
#define CUSTOM_MGSV_REFLECTION_RAYS_PER_PIXEL     shader_injection.reflection_rays_per_pixel
#define CUSTOM_MGSV_SPECULAR_OCCLUSION_STRENGTH   shader_injection.specular_occlusion_strength
#define CUSTOM_MGSV_LIGHTING_SEARCH_RADIUS_PIXELS shader_injection.lighting_search_radius_pixels
#define CUSTOM_MGSV_LIGHTING_SURFACE_THICKNESS    shader_injection.lighting_surface_thickness
#define CUSTOM_MGSV_LIGHTING_SURFACE_OFFSET       shader_injection.lighting_surface_offset
#define CUSTOM_MGSV_LIGHTING_SLICE_COUNT          shader_injection.lighting_slice_count
#define CUSTOM_MGSV_LIGHTING_SAMPLES_PER_SIDE     shader_injection.lighting_samples_per_side
#define CUSTOM_MGSV_LIGHTING_DEPTH_MIPS_ENABLED   shader_injection.lighting_depth_mips_enabled
#define CUSTOM_MGSV_GI_REJECT_BACKFACES           shader_injection.gi_reject_backfaces
#define CUSTOM_MGSV_LIGHTING_DEBUG_VIEW           shader_injection.lighting_debug_view

#define CUSTOM_MGSV_LIGHTING_RESOLUTION_DIVISOR shader_injection.lighting_resolution_divisor
#define CUSTOM_MGSV_LIGHTING_NOISE_FRAME        shader_injection.lighting_noise_frame

#include "../../shaders/renodx.hlsl"

#endif

#endif  // SRC_MGSV_SHARED_H_
