#include "./shared.h"
#include "../../shaders/color.hlsl"

// ---- Created with 3Dmigoto v1.3.16 on Wed Jul 03 02:21:17 2024
Texture2D<float4> t7 : register(t7);

SamplerState s7_s : register(s7);

cbuffer cb0 : register(b0)
{
  float4 cb0[1];
}




// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float4 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.x = v1.z / v1.w;
  r0.y = v1.w;
  r0.xyzw = t7.Sample(s7_s, r0.xy).xyzw;
  r1.xyz = cb0[0].xyz * cb0[0].www;
  r1.w = cb0[0].w;
  o0.xyzw = r1.xyzw * r0.xyzw;

  o0.rgb = abs(o0.rgb);
  o0.rgb = (injectedData.toneMapGammaCorrection
                ? pow(o0.rgb, 2.2f)
                : renodx::color::bt709::from::SRGB(o0.rgb));
  o0.rgb *= injectedData.toneMapGameNits / 80.f;

  return;
}