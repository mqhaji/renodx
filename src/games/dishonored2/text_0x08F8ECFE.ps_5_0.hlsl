#include "./shared.h"
#include "../../shaders/color.hlsl"

// ---- Created with 3Dmigoto v1.3.16 on Wed Jul 03 06:53:57 2024
Texture2D<float4> t7 : register(t7);

SamplerState s7_s : register(s7);

cbuffer cb0 : register(b0)
{
  float4 cb0[2];
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
  r0.xyzw = r1.xyzw * r0.xyzw;
  r0.xyz = cb0[1].xyz * r0.www + r0.xyz;
  o0.xyz = min(r0.xyz, r0.www);
  o0.w = r0.w;

  // o0.rgba = saturate(o0.rgba);
  // o0.rgb = linearFromSRGB(o0.rgb);

  return;
}