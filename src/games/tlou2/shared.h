#ifndef SRC_TLOU2_SHARED_H_
#define SRC_TLOU2_SHARED_H_

// #define RENODX_TONE_MAP_TYPE           1.f
// #define RENODX_PEAK_WHITE_NITS         400.f
// #define RENODX_DIFFUSE_WHITE_NITS      100.f
// #define RENODX_TONE_MAP_SHOULDER_START 0.375f
// #define RENODX_TONE_MAP_HUE_CORRECTION 0.f

// Must be 32bit aligned
// Should be 4x32
struct ShaderInjectData {
  float tone_map_type;
  float peak_white_nits;
  float tone_map_shoulder_start;
  float diffuse_white_nits;
  float tone_map_hue_shift;
  float gamma_correction_type;
};

#ifndef __cplusplus
cbuffer shader_injection : register(b13, space50) {
  ShaderInjectData shader_injection : packoffset(c0);
}

#define RENODX_TONE_MAP_TYPE           shader_injection.tone_map_type
#define RENODX_PEAK_WHITE_NITS         shader_injection.peak_white_nits
#define RENODX_DIFFUSE_WHITE_NITS      shader_injection.diffuse_white_nits
#define RENODX_TONE_MAP_SHOULDER_START shader_injection.tone_map_shoulder_start
#define RENODX_TONE_MAP_HUE_SHIFT      shader_injection.tone_map_hue_shift
#define RENODX_GAMMA_CORRECTION_TYPE   shader_injection.gamma_correction_type

#include "../../shaders/renodx.hlsl"
#endif

#endif  // SRC_TLOU2_SHARED_H_
