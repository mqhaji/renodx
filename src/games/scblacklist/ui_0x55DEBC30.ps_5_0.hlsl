#include "../../shaders/color.hlsl"
#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Thu May 23 03:04:27 2024

cbuffer CB_PS_SimpleVCTexMux : register(b7)
{
  float4x3 mat : packoffset(c0);
  float4 uv[4] : packoffset(c3);
}

SamplerState texX_s : register(s0);
SamplerState texY_s : register(s1);
SamplerState texZ_s : register(s2);
Texture2D<float4> texX : register(t0);
Texture2D<float4> texY : register(t1);
Texture2D<float4> texZ : register(t2);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float4 v1 : TEXCOORD0,
  float2 v2 : TEXCOORD1,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.w = 1;
  r1.xy = v2.xy * uv[0].xy + uv[0].zw;
  r1.x = texX.Sample(texX_s, r1.xy).x;
  r2.xy = v2.xy * uv[1].xy + uv[1].zw;
  r1.y = texY.Sample(texY_s, r2.xy).x;
  r2.xy = v2.xy * uv[2].xy + uv[2].zw;
  r1.z = texZ.Sample(texZ_s, r2.xy).x;
  r1.w = 1;
  r0.x = dot(r1.xyzw, mat._m00_m10_m20_m30);
  r0.y = dot(r1.xyzw, mat._m01_m11_m21_m31);
  r0.z = dot(r1.xyzw, mat._m02_m12_m22_m32);
  o0.xyzw = v1.xyzw * r0.xyzw;

  o0.rgb = injectedData.toneMapGammaCorrection ? pow(o0.rgb, 2.2f) : linearFromSRGB(o0.rgb);
  o0.rgb *= injectedData.toneMapUINits / 80.f;

  return;
}