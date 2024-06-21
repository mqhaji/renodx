#include "./shared.h"
#include "../../shaders/color.hlsl"

// ---- Created with 3Dmigoto v1.3.16 on Sat May 25 22:39:10 2024
Texture2D<float4> t0 : register(t0);

SamplerState s0_s : register(s0);

cbuffer cb0 : register(b0)
{
  float4 cb0[1];
}




// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_POSITION0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_TARGET0)
{
  float4 r0;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = t0.SampleLevel(s0_s, v1.xy, 0).xyzw;
  o0.w = r0.w;

  // r0.xyz = log2(abs(r0.xyz));
  // r0.xyz = cb0[0].xxx * r0.xyz;
  // o0.xyz = exp2(r0.xyz);

  o0.xyz = sign(r0.xyz) * pow(abs(r0.xyz), cb0[0].xxx);

  if (injectedData.toneMapGammaCorrection == 1) { // apply 2.2 gamma correction to fix srgb 2.2 mismatch
    o0.xyz = srgbFromLinear(o0.xyz);
    o0.xyz = pow(o0.xyz, 2.2f);
  }

  o0.xyz *= injectedData.toneMapGameNits/80.f;

  return;
}