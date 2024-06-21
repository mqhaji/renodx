#include "../../shaders/color.hlsl"
#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Sat Jun 01 17:30:33 2024

cbuffer PostFxSimple : register(b0)
{
  float4 Color : packoffset(c0);
  float4 QuadParams : packoffset(c1);
  float4 UVScaleOffset : packoffset(c2);
  float3 GammaBrightnessContrastParams : packoffset(c3);
  float2 Tiling : packoffset(c4);
}

SamplerState PostFxSimple__TextureSampler__SampObj___s : register(s0);
Texture2D<float4> PostFxSimple__TextureSampler__TexObj__ : register(t0);


// 3Dmigoto declarations
#define cmp -


void main(
  linear centroid float2 v0 : TEXCOORD0,
  float4 v1 : SV_Position0,
  out float4 o0 : SV_Target0)
{
  float4 r0;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = PostFxSimple__TextureSampler__TexObj__.Sample(PostFxSimple__TextureSampler__SampObj___s, v0.xy).xyzw;
  r0.xyz = log2(abs(r0.xyz));
  o0.w = r0.w;
  r0.xyz = GammaBrightnessContrastParams.xxx * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  o0.xyz = r0.xyz * GammaBrightnessContrastParams.yyy + GammaBrightnessContrastParams.zzz;

  o0.rgb = injectedData.toneMapGammaCorrection ? pow(o0.rgb, 2.2f) : linearFromSRGB(o0.rgb);
  o0.rgb *= injectedData.toneMapGameNits / 80.f;



  return;
}