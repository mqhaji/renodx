// ---- Created with 3Dmigoto v1.4.1 on Wed Jan  8 23:26:04 2025

cbuffer _Globals : register(b0) {
  float2 invPixelSize : packoffset(c0);
  float4 depthFactors : packoffset(c1);
  float2 fadeParams : packoffset(c2);
  float4 color : packoffset(c3);
  float4 color2 : packoffset(c4);
  float4 colorMatrix0 : packoffset(c5);
  float4 colorMatrix1 : packoffset(c6);
  float4 colorMatrix2 : packoffset(c7);
  float exponent : packoffset(c8);
  float4 ironsightsDofParams : packoffset(c9);
  uint4 mainTextureDimensions : packoffset(c10);
  float cubeArrayIndex : packoffset(c11);
  float g_outputExposureMultiplier : packoffset(c11.y);
  float4 combineTextureWeights[2] : packoffset(c12);
  float4x3 g_normalBasisTransforms[6] : packoffset(c14);
}

SamplerState mainTextureSampler_s : register(s0);
SamplerState mainTexture2Sampler_s : register(s1);
Texture2D<float4> mainTexture : register(t0);
Texture2D<float4> mainTexture2 : register(t1);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position0,
    float4 v1: TEXCOORD0,
    float2 v2: TEXCOORD1,
    out float4 o0: SV_Target0) {
  float4 r0, r1;

  r0.xyzw = mainTexture2.Sample(mainTexture2Sampler_s, v2.xy).xyzw;
  r0.xyzw = color2.xyzw * r0.xyzw;
  r1.xyzw = mainTexture.Sample(mainTextureSampler_s, v2.xy).xyzw;
  o0.xyzw = r1.xyzw * color.xyzw + r0.xyzw;
  return;
}
