#include "../common.hlsli"

cbuffer flare : register(b0) {
  float2 g_vFlarePosition : packoffset(c0);
  float2 g_vFlareRotation : packoffset(c0.z);
  float2 g_vFlareSize : packoffset(c1);
  float4 g_vFlareColor : packoffset(c2);
  float g_fFlareLod : packoffset(c3);
}

SamplerState g_sFlareTexture_sampler_s : register(s0);
Texture2D<float4> g_sFlareTexture : register(t0);

// 3Dmigoto declarations
#define cmp -

void main(
    float2 v0: TEXCOORD0,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = g_sFlareTexture.SampleLevel(g_sFlareTexture_sampler_s, v0.xy, g_fFlareLod).xyzw;
  r1.xyz = r0.xyz * g_vFlareColor.xyz + float3(-6, -6, -6);
  r0.xyzw = g_vFlareColor.xyzw * r0.xyzw;
  r1.xyz = saturate(float3(0.0384615399, 0.0384615399, 0.0384615399) * r1.xyz);
  r1.xyz = r1.xyz * float3(20, 20, 20) + float3(12, 12, 12);
  r2.xyz = cmp(float3(6, 6, 6) >= r0.xyz);
  r1.xyz = r2.xyz ? float3(0, 0, 0) : r1.xyz;
  r2.xyz = r2.xyz ? float3(1, 1, 1) : 0;
  r0.xyz = r2.xyz * r0.xyz;
  o0.w = r0.w;
  o0.xyz = r0.xyz * float3(2, 2, 2) + r1.xyz;
  return;
}
