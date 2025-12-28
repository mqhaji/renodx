#ifndef SRC_BLOODBORNE_SHARED_H_
#define SRC_BLOODBORNE_SHARED_H_

/* 
  Shaders use different push constants (depends on pipeline not shaders actually. So if a vertex shader is using 
  push constants but not frag/pixel shader, then that vertex shader push constant has to be accounted for
  since they share the same pipeline)

  Anyway, here we define different offsets based on shader used
*/
#ifdef USE_SETTINGS_PUSHCONSTANTS
#define PUSH_CONSTANTS_OFFSET 16
#endif

#ifdef USE_AUX_PUSHCONSTANTS
#define PUSH_CONSTANTS_OFFSET 128
#endif

// Fallback
#ifndef PUSH_CONSTANTS_OFFSET
#define PUSH_CONSTANTS_OFFSET 0
#endif

// Must be 32bit aligned
// Should be 4x32
struct ShaderInjectData {
  float peak_white_nits;
  float diffuse_white_nits;
  float graphics_white_nits;
  float tone_map_type;

  float tone_map_exposure;
  float tone_map_highlights;
  float tone_map_shadows;
  float tone_map_contrast;

  float tone_map_saturation;
  float tone_map_highlight_saturation;
  float tone_map_blowout;
  float tone_map_flare;

  float gamma_correction;
  float custom_bloom;
  float custom_dof;
  float swap_chain_output_dither_bits;
};

#define RENODX_PEAK_WHITE_NITS                        shader_injection.peak_white_nits
#define RENODX_DIFFUSE_WHITE_NITS                     shader_injection.diffuse_white_nits
#define RENODX_GRAPHICS_WHITE_NITS                    shader_injection.graphics_white_nits
#define RENODX_TONE_MAP_TYPE                          shader_injection.tone_map_type
#define RENODX_TONE_MAP_EXPOSURE                      shader_injection.tone_map_exposure
#define RENODX_TONE_MAP_HIGHLIGHTS                    shader_injection.tone_map_highlights
#define RENODX_TONE_MAP_SHADOWS                       shader_injection.tone_map_shadows
#define RENODX_TONE_MAP_CONTRAST                      shader_injection.tone_map_contrast
#define RENODX_TONE_MAP_SATURATION                    shader_injection.tone_map_saturation
#define RENODX_TONE_MAP_HIGHLIGHT_SATURATION          shader_injection.tone_map_highlight_saturation
#define RENODX_TONE_MAP_BLOWOUT                       shader_injection.tone_map_blowout
#define RENODX_TONE_MAP_FLARE                         shader_injection.tone_map_flare
#define RENODX_RENO_DRT_WHITE_CLIP                    100.f
#define RENODX_RENO_DRT_TONE_MAP_METHOD               renodx::tonemap::renodrt::config::tone_map_method::HERMITE_SPLINE
#define RENODX_RENO_DRT_NEUTRAL_SDR_CLAMP_PEAK        -1.0f
#define RENODX_RENO_DRT_NEUTRAL_SDR_CLAMP_COLOR_SPACE -1.0f
#define RENODX_RENO_DRT_NEUTRAL_SDR_TONE_MAP_METHOD   renodx::tonemap::renodrt::config::tone_map_method::HERMITE_SPLINE
#define RENODX_SWAP_CHAIN_OUTPUT_PRESET               SWAP_CHAIN_OUTPUT_PRESET_SCRGB
#define RENODX_GAMMA_CORRECTION                       renodx::draw::GAMMA_CORRECTION_NONE
#define CUSTOM_BLOOM                                  shader_injection.custom_bloom
#define CUSTOM_DOF                                    shader_injection.custom_dof
#define RENODX_INTERMEDIATE_ENCODING                  renodx::draw::ENCODING_SRGB
#define RENODX_SWAP_CHAIN_GAMMA_CORRECTION            renodx::draw::GAMMA_CORRECTION_GAMMA_2_2
#define RENODX_TONE_MAP_PASS_AUTOCORRECTION           1.f

#ifndef __cplusplus
#ifdef __SLANG__
/*
  We need to account for the padding of the original struct
  (Aux size is 120 bytes & Setting is 8 bytes). Vulkan adjustments
  will add the correct offset when pushing constants,
  but we still need to define the proper offset
  to account for the original game/emulator push constants.

  IMPORTANT: AUX SIZE WILL BECOME 128 Bytes because alignment depends on the largest
  element within the struct. Aux Data has uvec4 which is 16 bytes, so total size will
  have to be aligned to 16. Settings largest element is 4 bytes so it aligns to 4 bytes,
  so final size will be 8. This is important for offsets and will mess up cbuffers unless
  they're manually aligned. You can use define DEBUG_LEVEL_1/DEBUG_LEVEL_2 and renodx will
  log the injection offset
  e.g. utils::constants::PushShaderInjections(layout: 0x0165a600000165a6[2], dispatch: true, resource_tag: -1, offset: 4) <- Might look different
  Offset here is 4(in float4) so it is 16

  PUSH CONSTANTS TOTAL SIZE LIMIT IS 256 BYTES! You can't add cbuffers willy nilly.
  Log should show a warning if it overflows
*/
struct PushData
{
    [[vk::offset(PUSH_CONSTANTS_OFFSET)]]
    ShaderInjectData shader_injection;
};

[[vk::push_constant]]
PushData gPush;
#define shader_injection gPush.shader_injection
#else
#if ((__SHADER_TARGET_MAJOR == 5 && __SHADER_TARGET_MINOR >= 1) || __SHADER_TARGET_MAJOR >= 6)
cbuffer injected_buffer : register(b13, space50) {
#elif (__SHADER_TARGET_MAJOR < 5) || ((__SHADER_TARGET_MAJOR == 5) && (__SHADER_TARGET_MINOR < 1))
cbuffer injected_buffer : register(b13) {
#endif
  ShaderInjectData shader_injection : packoffset(c0);
}
#endif

#include "../../shaders/renodx.hlsl"

#endif

#endif  // SRC_BLOODBORNE_SHARED_H_
