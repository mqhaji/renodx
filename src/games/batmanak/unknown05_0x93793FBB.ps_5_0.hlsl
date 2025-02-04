#include "./shared.h"

Texture2D<float4> t1 : register(t1);

Texture2D<float4> t0 : register(t0);

SamplerState s1_s : register(s1);

SamplerState s0_s : register(s0);

cbuffer cb0 : register(b0) {
  float4 cb0[6];
}

// 3Dmigoto declarations
#define cmp -

void main(float4 v0 : COLOR0, float2 v1 : TEXCOORD0, float2 w1 : TEXCOORD1, out float4 o0 : SV_TARGET0) {
  float4 r0, r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = t0.Sample(s0_s, v1.xy).xyzw;
  r1.xyzw = t1.Sample(s1_s, w1.xy).xyzw;
  r0.xyzw = -r1.xyzw + r0.xyzw;
  r0.xyzw = v0.xxxx * r0.xyzw + r1.xyzw;
  r0.xyz = saturate(r0.xyz);
  o0.w = v0.w * r0.w;
  r0.xyz = log2(r0.xyz);
  r0.xyz = cb0[5].www * r0.xyz;
  o0.xyz = exp2(r0.xyz);

  o0.rgb = saturate(o0.rgb);
  return;
}
