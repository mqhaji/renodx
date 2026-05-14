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
    r1.rgb = renodx::color::srgb::DecodeSafe(r1.rgb);
    maxch_scale = ComputeMaxChCompressionScale(r1.rgb, 0.5f);
    r1.rgb *= maxch_scale;
    r1.rgb = renodx::color::srgb::EncodeSafe(r1.rgb);
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
    r0.rgb = renodx::color::srgb::DecodeSafe(r0.rgb);
    r0.rgb /= maxch_scale;
    r0.rgb = renodx::color::srgb::EncodeSafe(r0.rgb);
  }

  r0.rgb = ApplyFinalTonemap(r0.rgb);

  o0.w = dot(r0.xyz, float3(0.298999995, 0.587000012, 0.114));
  o0.xyz = r0.xyz;
  return;
}
