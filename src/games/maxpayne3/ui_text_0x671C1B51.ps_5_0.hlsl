#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Sun Jun 09 17:02:16 2024

cbuffer AlphaTest : register(b11)
{
  float g_AlphaRef : packoffset(c0);
}

SamplerState DiffuseSampler_s : register(s1);
Texture2D<float4> DiffuseSampler : register(t1);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float4 v1 : COLOR0,
  float2 v2 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = DiffuseSampler.Sample(DiffuseSampler_s, v2.xy).xyzw;
  r0.xyzw = v1.xyzw * r0.xyzw;
  r1.x = cmp(g_AlphaRef >= r0.w);
  if (r1.x != 0) discard;
  o0.xyzw = r0.xyzw;

  if (injectedData.toneMapUINits != injectedData.toneMapGameNits) {
    o0.xyz = pow(o0.xyz, 2.2f);
    o0.xyz *= injectedData.toneMapUINits / injectedData.toneMapGameNits;
    o0.xyz = pow(o0.xyz, 1.f / 2.2f);
  }
  return;
}