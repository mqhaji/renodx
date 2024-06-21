#include "../../shaders/color.hlsl"
#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Thu May 23 03:04:12 2024

cbuffer CB_PS_Sampling : register(b1)
{
  float4 cTexelSize : packoffset(c0);
  float2 cTexelSize1D : packoffset(c1);
  float4 cTexelSize1D_2 : packoffset(c2);
  float4x4 gScreenToWorld : packoffset(c3);
}

SamplerState sTex_s : register(s0);
Texture2D<float4> sTex : register(t0);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  out float4 o0 : SV_Target0)
{
  float4 r0;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v0.xy;
  r0.xy = r0.xy * cTexelSize1D_2.xy + cTexelSize1D.xy;
  o0.xyzw = sTex.Sample(sTex_s, r0.xy).xyzw;

  o0.rgb = injectedData.toneMapGammaCorrection ? pow(o0.rgb, 2.2f) : linearFromSRGB(o0.rgb);
  o0.rgb *= injectedData.toneMapUINits / 80.f;

  return;
}