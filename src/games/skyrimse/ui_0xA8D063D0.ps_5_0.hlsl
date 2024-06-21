#include "../../shaders/color.hlsl"
#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Sat May 18 02:42:07 2024

cbuffer PSConstants : register(b0)
{
  float4 cxmul : packoffset(c0);
  float4 cxadd : packoffset(c1);
}



// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : COLOR0,
  out float4 o0 : SV_Target0)
{
  o0.xyzw = v0.xyzw * cxmul.xyzw + cxadd.xyzw;

  o0.xyzw = saturate(o0.xyzw);
  o0.rgb = injectedData.toneMapGammaCorrection ? pow(o0.rgb, 2.2f) : linearFromSRGB(o0.rgb);
  o0.rgb *= injectedData.toneMapUINits / 80.f;

  return;
}