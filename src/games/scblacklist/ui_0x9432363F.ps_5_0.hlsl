#include "../../shaders/color.hlsl"
#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Thu May 23 03:04:38 2024

cbuffer CB_PS_ScaleFormCxformTexture : register(b7)
{
  float4 cxmul : packoffset(c0);
  float4 cxadd : packoffset(c1);
}

SamplerState tex0_s : register(s0);
Texture2D<float4> tex0 : register(t0);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = tex0.Sample(tex0_s, v1.xy).xyzw;
  r0.xyzw = r0.xyzw * cxmul.xyzw + cxadd.xyzw;
  r1.xyzw = float4(-1,-1,-1,-1) + r0.xyzw;
  o0.xyzw = r0.wwww * r1.xyzw + float4(1,1,1,1);

  o0.rgb = injectedData.toneMapGammaCorrection ? pow(o0.rgb, 2.2f) : linearFromSRGB(o0.rgb);
  o0.rgb *= injectedData.toneMapUINits / 80.f;

  return;
}