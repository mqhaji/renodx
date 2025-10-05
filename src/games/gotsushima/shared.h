#ifndef SRC_GOTSUSHIMA_SHARED_H_
#define SRC_GOTSUSHIMA_SHARED_H_

// Must be 32bit aligned
// Should be 4x32
struct ShaderInjectData {
  // float peak_white_nits;
  float diffuse_white_nits;
  // float graphics_white_nits;
  // float color_grade_strength;

  //   float tone_map_type;
  // float tone_map_exposure;
  // float tone_map_highlights;
  // float tone_map_shadows;

  // float tone_map_contrast;
  // float tone_map_saturation;
  // float tone_map_highlight_saturation;
  // float tone_map_blowout;

  // float tone_map_flare;
  // float tone_map_white_clip;
  // float gamma_correction;

  // float scene_grade_strength;
};

#ifndef __cplusplus
#if ((__SHADER_TARGET_MAJOR == 5 && __SHADER_TARGET_MINOR >= 1) || __SHADER_TARGET_MAJOR >= 6)
cbuffer shader_injection : register(b13, space50) {
#elif (__SHADER_TARGET_MAJOR < 5) || ((__SHADER_TARGET_MAJOR == 5) && (__SHADER_TARGET_MINOR < 1))
cbuffer shader_injection : register(b13) {
#endif
  ShaderInjectData shader_injection : packoffset(c0);
}

#if (__SHADER_TARGET_MAJOR >= 6)
#pragma dxc diagnostic ignored "-Wparentheses-equality"
#endif

// #define RENODX_PEAK_WHITE_NITS      400.f
#define RENODX_DIFFUSE_WHITE_NITS shader_injection.diffuse_white_nits
// #define RENODX_GRAPHICS_WHITE_NITS  100.f
// #define RENODX_GAMMA_CORRECTION     1u
// #define RENODX_TONE_MAP_TYPE        1u
// #define RENODX_TONE_MAP_EXPOSURE             1.f
// #define RENODX_TONE_MAP_HIGHLIGHTS           1.f
// #define RENODX_TONE_MAP_SHADOWS              1.f
// #define RENODX_TONE_MAP_CONTRAST             1.f
// #define RENODX_TONE_MAP_SATURATION           1.f
// #define RENODX_TONE_MAP_HIGHLIGHT_SATURATION 1.f
// #define RENODX_TONE_MAP_BLOWOUT              0.f

// #define RENODX_COLOR_GRADE_STRENGTH 1.f

#include "../../shaders/renodx.hlsl"

#endif

#endif  // SRC_GOTSUSHIMA_SHARED_H_
