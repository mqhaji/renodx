#include "./common.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:22 2026

// clang-format off
cbuffer cPSObject : register(b5)
{
  struct
  {
    float4x4 m_viewWorld;
    float4x4 m_world;
    float4 m_useWeightCount;
    float4 m_localParam[4];
  } g_psObject : packoffset(c0);
}

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

SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inImage : register(t0);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position0,
    float2 v1: TEXCOORD0,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2, r3, r4;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = v1.xy;
  r0.xy = r0.xy;
  r0.xy = g_psSystem.m_renderInfo.xy * r0.xy;
  r0.xy = float2(0, 0) + r0.xy;
  r0.xy = g_psSystem.m_renderBuffer.zw * r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xyzw = inImage.Sample(g_samplerLinear_Clamp_s, r0.xy).xyzw;
  r0.rgb = renodx::color::srgb::DecodeSafe(r0.rgb);  // needed due to upgrades

  if (RENODX_TONE_MAP_TYPE != 0.f) {
    o0.w = saturate(r0.w);

    if (RENODX_GAMMA_CORRECTION) {
      r0.rgb = renodx::color::correct::GammaSafe(r0.rgb);
    }
    o0.rgb = renodx::color::bt709::clamp::BT2020(r0.rgb);
    o0.rgb *= RENODX_GRAPHICS_WHITE_NITS / 80.f;

    return;
  } else {
    r0 = max(0, r0);
  }

  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r1.xyzw = g_psObject.m_localParam[0].xyzw;
  r2.xyzw = g_psObject.m_localParam[1].xyzw;
  r3.xyzw = g_psObject.m_localParam[2].xyzw;
  r4.xyzw = -r0.xyzw;
  r0.xyzw = max(r4.xyzw, r0.xyzw);
  r0.xyzw = log2(r0.xyzw);
  r0.xyzw = r3.xyzw * r0.xyzw;
  r0.xyzw = exp2(r0.xyzw);
  r0.xyzw = r2.xyzw * r0.xyzw;
  r0.xyzw = r1.xyzw + r0.xyzw;
  o0.xyzw = r0.xyzw;

  o0.w = saturate(o0.w);

  if (RENODX_GAMMA_CORRECTION) {
    o0.rgb = renodx::color::correct::GammaSafe(o0.rgb);
  }
  o0.rgb *= RENODX_GRAPHICS_WHITE_NITS / 80.f;

  return;
}
