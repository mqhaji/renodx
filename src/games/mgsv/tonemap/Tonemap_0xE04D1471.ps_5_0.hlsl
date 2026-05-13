#include "./tonemap.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:33 2026

// clang-format off
cbuffer cPSSystem : register(b0)
{
  struct
  {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem : packoffset(c0);
}
// clang-format on

SamplerState g_samplerPoint_Wrap_s : register(s8);
SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inImage : register(t0);
Texture2D<float4> inColorLUT : register(t6);
Texture2D<float4> inBloom : register(t7);

// 3Dmigoto declarations
#define cmp -

void main(
    float2 v0: TEXCOORD0,
    float4 v1: SV_Position0,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5, -0.5) + v1.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.zw = g_psSystem.m_renderBuffer.zw;
  r1.xy = float2(0.5, 0.5) * r0.zw;
  r0.xy = r0.xy * r0.zw;
  r0.xy = r0.xy + r1.xy;
  r0.xy = r0.xy;
  r1.xy = v0.xy;
  r1.xy = r1.xy;
  r1.z = -r1.y;
  r0.zw = float2(0.5, 0.5) * r1.xz;
  r0.zw = float2(0.5, 0.5) + r0.zw;
  r0.zw = r0.zw;
  r0.xy = r0.xy;
  r0.zw = r0.zw;
  r0.xy = r0.xy;
  r0.zw = r0.zw;
  r1.xyz = inImage.Sample(g_samplerPoint_Wrap_s, r0.xy).xyz;

  // tonemap to sdr before bloom
  float maxch_scale = 1.f;
  if (RENODX_TONE_MAP_TYPE != 0.f) {
    r1.rgb = renodx::color::gamma::DecodeSafe(r1.rgb);
    maxch_scale = ComputeMaxChCompressionScale(r1.rgb, 0.5f);
    r1.rgb *= maxch_scale;
    r1.rgb = renodx::color::gamma::EncodeSafe(r1.rgb);
  }

  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz * r1.xyz;
  r0.xy = float2(4, 4) * g_psSystem.m_renderBuffer.zw;
  r0.xy = float2(0.5, 0.5) * r0.xy;
  r0.xy = r0.xy + r0.zw;
  r0.xyz = inBloom.Sample(g_samplerLinear_Clamp_s, r0.xy).xyz;
  r0.xyz = min(float3(0.5, 0.5, 0.5), r0.xyz);
  r0.rgb = lerp(0, r0.rgb, CUSTOM_BLOOM);
  r0.xyz = lerp(r1.xyz, 1.f, r0.xyz);

  r0.rgb = Sample2DLUTWithScaling(r0.rgb, inColorLUT, g_samplerLinear_Clamp_s);

  // invert tonemap after LUT
  r0.rgb = max(0, r0.rgb);
  if (RENODX_TONE_MAP_TYPE != 0.f) {
    r0.rgb = renodx::color::gamma::DecodeSafe(r0.rgb);
    r0.rgb /= maxch_scale;
    r0.rgb = renodx::color::gamma::EncodeSafe(r0.rgb);
  }

  if (RENODX_GAMMA_CORRECTION == 1.f) {
    r0.rgb = renodx::color::gamma::DecodeSafe(r0.rgb);
  } else if (RENODX_GAMMA_CORRECTION == 2.f) {
    r0.rgb = renodx::color::srgb::DecodeSafe(r0.rgb);
    float3 perch = renodx::color::correct::GammaSafe(r0.rgb);

    float y_in = max(0, renodx::color::yf::from::BT709(r0.rgb));
    float y_out = renodx::color::correct::Gamma(y_in);

    r0.rgb = renodx::color::correct::Luminance(r0.rgb, y_in, y_out);

    r0.rgb = renodx::color::bt709::from::BT2020(renodx_custom::tonemap::psycho::psycho17_ApplyPurityFromBT2020(
        renodx::color::bt2020::from::BT709(perch),
        renodx::color::bt2020::from::BT709(r0.rgb), 1.f, 1.f, 1e-7f, false));
  } else {
    r0.rgb = renodx::color::srgb::DecodeSafe(r0.rgb);
  }

  if (RENODX_TONE_MAP_TYPE != 0.f) {
    r0.rgb = renodx::color::bt2020::from::BT709(r0.rgb);

    renodx_custom::tonemap::psycho::config17::Config psycho17_config =
        renodx_custom::tonemap::psycho::config17::Create();
    psycho17_config.peak_value = RENODX_PEAK_WHITE_NITS / RENODX_DIFFUSE_WHITE_NITS;
    psycho17_config.clip_point = 100.f;
    psycho17_config.exposure = RENODX_TONE_MAP_EXPOSURE;
    psycho17_config.gamma = 1.f;
    psycho17_config.highlights = RENODX_TONE_MAP_HIGHLIGHTS;
    psycho17_config.shadows = RENODX_TONE_MAP_SHADOWS;
    psycho17_config.contrast = RENODX_TONE_MAP_CONTRAST;
    psycho17_config.flare_lms = 0.10f * pow(RENODX_TONE_MAP_FLARE, 10.f);
    psycho17_config.contrast_highlights = 1.f;
    psycho17_config.contrast_shadows = 1.f;
    psycho17_config.purity_scale = RENODX_TONE_MAP_SATURATION;
    psycho17_config.purity_highlights = -1.f * (RENODX_TONE_MAP_HIGHLIGHT_SATURATION - 1.f);
    psycho17_config.dechroma = RENODX_TONE_MAP_DECHROMA;
    psycho17_config.adaptation_contrast = RENODX_TONE_MAP_CONE_CONTRAST;
    psycho17_config.bleaching_intensity = 0.f;
    psycho17_config.hue_emulation = 0.f;
    psycho17_config.pre_gamut_compress = false;
    psycho17_config.post_gamut_compress = true;
    psycho17_config.mid_gray = 0.1f;
    r0.rgb = renodx_custom::tonemap::psycho::ApplyTest17BT2020(r0.rgb, r0.rgb, psycho17_config);

    r0.rgb = renodx::color::bt709::from::BT2020(r0.rgb);

  } else if (RENODX_TONE_MAP_TYPE == 0.f) {
    r0.rgb = saturate(r0.rgb);
  }

  r0.rgb *= RENODX_DIFFUSE_WHITE_NITS / RENODX_GRAPHICS_WHITE_NITS;

  if (RENODX_GAMMA_CORRECTION != 0.f) {
    r0.rgb = renodx::color::gamma::EncodeSafe(r0.rgb);
  } else {
    r0.rgb = renodx::color::srgb::EncodeSafe(r0.rgb);
  }

  o0.w = dot(r0.xyz, float3(0.298999995, 0.587000012, 0.114));
  o0.xyz = r0.xyz;
  return;
}
