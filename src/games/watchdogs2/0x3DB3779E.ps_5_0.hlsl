#include "../../shaders/color.hlsl"
#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Sat Jun 01 17:30:33 2024

cbuffer Primitive : register(b0)
{
  float4 GeometryPickingID : packoffset(c0);
  float4 MaterialPickingID : packoffset(c1);
  float4 ModelPickingID : packoffset(c2);
  float4 PickingID : packoffset(c3);
  float4 ColorMultiplier : packoffset(c4);
  float4x4 SecondTextureUVTransform : packoffset(c5);
  float4x4 Transform : packoffset(c9);
  float4x4 UVTransform : packoffset(c13);
  float4 VideoTextureUnpack[8] : packoffset(c17);
  float3 GammaBrightnessContrastParams : packoffset(c25);
  float CustomExposureScale : packoffset(c25.w);
  float MipLevel : packoffset(c26);
  float Sharpness : packoffset(c26.y);
  float VolumeTextureSizeZ : packoffset(c26.z);
  bool SecondTextureAdditive : packoffset(c26.w);
  bool TextureIsOffscreenComposited : packoffset(c27);
}

SamplerState Primitive__DiffuseSampler0__SampObj___s : register(s0);
Texture2D<float4> Primitive__DiffuseSampler0__TexObj__ : register(t0);


// 3Dmigoto declarations
#define cmp -


void main(
  linear centroid float4 v0 : TEXCOORD0,
  linear centroid float2 v1 : TEXCOORD1,
  float4 v2 : SV_Position0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = Primitive__DiffuseSampler0__TexObj__.Sample(Primitive__DiffuseSampler0__SampObj___s, v1.xy).xyzw;
  r1.w = 1 + -r0.w;
  r2.xyz = r0.xyz / r1.www;
  r2.w = cmp(0 < r1.w);
  r1.xyz = r2.www ? r2.xyz : r0.xyz;
  r0.xyzw = TextureIsOffscreenComposited ? r1.xyzw : r0.xyzw;
  r0.xyzw = v0.xyzw * r0.xyzw;
  r0.xyz = log2(abs(r0.xyz));
  o0.w = r0.w;
  r0.xyz = GammaBrightnessContrastParams.xxx * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  o0.xyz = r0.xyz * GammaBrightnessContrastParams.yyy + GammaBrightnessContrastParams.zzz;

  o0.rgb = injectedData.toneMapGammaCorrection ? pow(o0.rgb, 2.2f) : linearFromSRGB(o0.rgb);
  o0.rgb *= injectedData.toneMapUINits / 80.f;

  return;
}