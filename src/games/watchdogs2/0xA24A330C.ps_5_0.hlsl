#include "../../shaders/color.hlsl"
#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Sat Jun 01 17:30:34 2024

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

SamplerState Video_s : register(s0);
Texture2D<float4> Primitive__DiffuseSampler0__TexObj__ : register(t0);


// 3Dmigoto declarations
#define cmp -


void main(
  linear centroid float4 v0 : TEXCOORD0,
  linear centroid float2 v1 : TEXCOORD1,
  float4 v2 : SV_Position0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = frac(v1.xy);
  r1.xy = r0.xy * VideoTextureUnpack[2].xy + VideoTextureUnpack[2].zw;
  r1.zw = r0.xy * VideoTextureUnpack[3].xy + VideoTextureUnpack[3].zw;
  r0.xy = r0.xy * VideoTextureUnpack[0].xy + VideoTextureUnpack[0].zw;
  r0.xy = max(VideoTextureUnpack[4].xy, r0.xy);
  r0.xy = min(VideoTextureUnpack[5].xy, r0.xy);
  r0.x = Primitive__DiffuseSampler0__TexObj__.Sample(Video_s, r0.xy).w;
  r1.xyzw = max(VideoTextureUnpack[6].xyzw, r1.xyzw);
  r1.xyzw = min(VideoTextureUnpack[7].xyzw, r1.xyzw);
  r0.y = Primitive__DiffuseSampler0__TexObj__.Sample(Video_s, r1.zw).w;
  r0.z = Primitive__DiffuseSampler0__TexObj__.Sample(Video_s, r1.xy).w;
  r1.xyz = float3(0,-0.391448975,2.01782227) * r0.yyy;
  r0.yzw = r0.zzz * float3(1.59579468,-0.813476563,0) + r1.xyz;
  r0.xyz = r0.xxx * float3(1.16412354,1.16412354,1.16412354) + r0.yzw;
  r0.xyz = float3(-0.87065506,0.529705048,-1.08166885) + r0.xyz;
  r0.xyz = max(float3(0,0,0), r0.xyz);
  r0.xyz = v0.xyz * r0.xyz;
  r0.xyz = log2(abs(r0.xyz));
  r0.xyz = GammaBrightnessContrastParams.xxx * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  o0.xyz = r0.xyz * GammaBrightnessContrastParams.yyy + GammaBrightnessContrastParams.zzz;
  o0.w = v0.w;

  o0.rgb = injectedData.toneMapGammaCorrection ? pow(o0.rgb, 2.2f) : linearFromSRGB(o0.rgb);
  o0.rgb *= injectedData.toneMapUINits / 80.f;

  return;
}