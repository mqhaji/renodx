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

  // Effects.
  float bloom_type;  // 0 = Vanilla, 1 = Enhanced.
  float bloom_strength;
  float sun_brightness_boost_enabled;
  float film_grain_random;
  float film_grain_strength;
  float motion_blur_strength;

  // Temporal Anti-Aliasing.
  float reconstruction_mode;  // 0 = Off, 1 = Analytical TAA, 2 = FSR, 3 = NVIDIA DLSS.

  float swap_chain_output_preset;

  // Direct Lighting.
  float shadows_mode;                    // 0 = Vanilla, 1 = Enhanced shadow-map filtering and contact shadows.
  float shadow_softness;                 // Scales the light sizes that soften shadow-map and contact edges; 1 is the tuned size.
  float contact_shadow_length;           // Search length toward the light in game units; each receiver converts it to pixels.
  float contact_shadow_spotlight_limit;  // Spotlights traced per lighting pass; 0 = no count, 13 screens of masks.
  float shadow_map_debug_view;
  float contact_shadow_debug_view;
  float shadow_debug_light;  // Light the shadow debug views show: 0 = sun or moon, 1 = spot, 2 = point.
  // Set by the host, not by the menu.
  float contact_shadow_render_scale;  // Render pixels per output pixel along each axis; 1 without upscaling.
  float shadow_noise_frame;           // IS-FAST layer (0 to 31) for shadow-map filtering, advanced once per Present.

  // Indirect Lighting.
  float indirect_lighting_mode;        // 0 = Vanilla, 1 = Enhanced diffuse AO/GI and reflections together.
  float diffuse_resolution;            // Diffuse trace pixel count: 0 = full, 1 = 1/4, 2 = 1/16, 3 = 1/64.
  float reflection_rays_per_pixel;     // GGX samples per pixel; sets trace cost, not strength.
  float ambient_occlusion_strength;    // 0 to 1; darkens only the game's ambient light.
  float diffuse_bounce_strength;       // 0 to 1.
  float reflection_strength;           // 0 to 1; blends the whole screen and environment reflection replacement.
  float specular_occlusion_strength;   // 0 to 1; blocks the environment fallback where a reflection ray hits geometry.
  float diffuse_search_radius_pixels;  // Output pixels in the menu; each dispatch's copy is in render pixels.
  float diffuse_direction_count;
  float diffuse_samples_per_side;
  float diffuse_coarse_depth;      // Requested in the menu; each dispatch's copy says whether the depth mips are ready.
  float diffuse_reject_backfaces;  // Surfaces facing away from a receiver still block but send it no light.
  float diffuse_multiple_bounces;  // Read by the host only: the trace's source includes the previous frame's indirect light.
  float indirect_lighting_debug_view;
  // Set by the host for each diffuse dispatch, not by the menu.
  float diffuse_resolution_divisor;  // Full-resolution pixels per trace pixel along each axis: 1, 2, 4, or 8.
  float diffuse_noise_frame;         // Selects the IS-FAST layer; does not say whether history is valid.

  // Atmosphere. Read by the host only.
  float fog_mode;             // 0 = Vanilla, 1 = Enhanced.
  float fog_shadow_strength;  // 0 to 1.
  float fog_height_falloff;   // 0 to 1.
  float fog_unevenness;       // 0 to 1.
  float fog_local_lights;     // 0 to 1.
  float fog_terrain_shadows;  // 0 = Off, 1 = On.
  float fog_debug_view;       // 0 = Off, 1 = Shadows, 2 = Local Lights.
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

#define CUSTOM_BLOOM_TYPE                   shader_injection.bloom_type
#define CUSTOM_BLOOM_STRENGTH               shader_injection.bloom_strength
#define CUSTOM_SUN_BRIGHTNESS_BOOST_ENABLED shader_injection.sun_brightness_boost_enabled
#define CUSTOM_FILM_GRAIN_RANDOM            shader_injection.film_grain_random
#define CUSTOM_FILM_GRAIN_STRENGTH          shader_injection.film_grain_strength
#define CUSTOM_MOTION_BLUR_STRENGTH         shader_injection.motion_blur_strength
#define CUSTOM_RECONSTRUCTION_MODE          shader_injection.reconstruction_mode

#define CUSTOM_MGSV_ENHANCED_SHADOWS          (shader_injection.shadows_mode >= 0.5)
#define CUSTOM_SHADOW_SOFTNESS                shader_injection.shadow_softness
#define CUSTOM_CONTACT_SHADOW_LENGTH          shader_injection.contact_shadow_length
#define CUSTOM_CONTACT_SHADOW_SPOTLIGHT_LIMIT shader_injection.contact_shadow_spotlight_limit
#define CUSTOM_SHADOW_MAP_DEBUG_VIEW          shader_injection.shadow_map_debug_view
#define CUSTOM_CONTACT_SHADOW_DEBUG_VIEW      shader_injection.contact_shadow_debug_view
#define CUSTOM_SHADOW_DEBUG_LIGHT             shader_injection.shadow_debug_light
#define CUSTOM_CONTACT_SHADOW_RENDER_SCALE    shader_injection.contact_shadow_render_scale
#define CUSTOM_SHADOW_NOISE_FRAME             shader_injection.shadow_noise_frame

// The vanilla tone curve runs inside deferred composition, before the late reflection pass could
// swap in linear light, so Enhanced needs the RenoDX tone mapper. OnPresent in addon.cpp applies
// the same rule on the host side.
#define CUSTOM_MGSV_ENHANCED_INDIRECT_LIGHTING   (shader_injection.indirect_lighting_mode >= 0.5 && RENODX_TONE_MAP_TYPE != 0.0)
#define CUSTOM_MGSV_REFLECTION_RAYS_PER_PIXEL    shader_injection.reflection_rays_per_pixel
#define CUSTOM_MGSV_AMBIENT_OCCLUSION_STRENGTH   shader_injection.ambient_occlusion_strength
#define CUSTOM_MGSV_DIFFUSE_BOUNCE_STRENGTH      shader_injection.diffuse_bounce_strength
#define CUSTOM_MGSV_REFLECTION_STRENGTH          shader_injection.reflection_strength
#define CUSTOM_MGSV_SPECULAR_OCCLUSION_STRENGTH  shader_injection.specular_occlusion_strength
#define CUSTOM_MGSV_DIFFUSE_SEARCH_RADIUS_PIXELS shader_injection.diffuse_search_radius_pixels
#define CUSTOM_MGSV_DIFFUSE_DIRECTION_COUNT      shader_injection.diffuse_direction_count
#define CUSTOM_MGSV_DIFFUSE_SAMPLES_PER_SIDE     shader_injection.diffuse_samples_per_side
#define CUSTOM_MGSV_DIFFUSE_COARSE_DEPTH         shader_injection.diffuse_coarse_depth
#define CUSTOM_MGSV_DIFFUSE_REJECT_BACKFACES     shader_injection.diffuse_reject_backfaces
#define CUSTOM_MGSV_INDIRECT_LIGHTING_DEBUG_VIEW shader_injection.indirect_lighting_debug_view
#define CUSTOM_MGSV_DIFFUSE_RESOLUTION_DIVISOR   shader_injection.diffuse_resolution_divisor
#define CUSTOM_MGSV_DIFFUSE_NOISE_FRAME          shader_injection.diffuse_noise_frame

#include "../../shaders/renodx.hlsl"

#endif

#endif  // SRC_MGSV_SHARED_H_
