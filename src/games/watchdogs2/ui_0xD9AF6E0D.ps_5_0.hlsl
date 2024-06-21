#include "../../shaders/color.hlsl"
#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Sat Jun 01 17:30:35 2024

cbuffer FireUiPrimitive : register(b0)
{
  float4 ColorAdd : packoffset(c0);
  float4 ColorMultiplier : packoffset(c1);
  float4 DiffuseSampler0Size : packoffset(c2);
  float4 DistanceFieldFloatArray[3] : packoffset(c3);
  float4x4 Transform : packoffset(c6);
  float4 UVOffsets[9] : packoffset(c10);
  float4x4 UVTransform : packoffset(c19);
  float4 VideoTextureUnpack[8] : packoffset(c23);
  float3 GammaBrightnessContrastParams : packoffset(c31);
  float DesaturationFactor : packoffset(c31.w);
  float2 SystemTime_GlitchFactor : packoffset(c32);
}



// 3Dmigoto declarations
#define cmp -


void main(
  linear centroid float4 v0 : TEXCOORD0,
  float4 v1 : SV_Position0,
  out float4 o0 : SV_Target0)
{
  float4 r0;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.x = dot(v0.xyz, float3(0.298999995,0.587000012,0.114));
  r0.yzw = v0.xyz + -r0.xxx;
  r0.xyz = DesaturationFactor * r0.yzw + r0.xxx;
  r0.w = v0.w;
  r0.xyzw = ColorMultiplier.xyzw * r0.xyzw;
  r0.xyz = log2(abs(r0.xyz));
  o0.w = r0.w;
  r0.xyz = GammaBrightnessContrastParams.xxx * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  o0.xyz = r0.xyz * GammaBrightnessContrastParams.yyy + GammaBrightnessContrastParams.zzz;

  o0.rgb = injectedData.toneMapGammaCorrection ? pow(o0.rgb, 2.2f) : linearFromSRGB(o0.rgb);
  o0.rgb *= injectedData.toneMapUINits / 80.f;

  return;
}