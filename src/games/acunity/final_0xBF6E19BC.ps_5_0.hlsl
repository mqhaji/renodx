#include "../../shaders/color.hlsl"
#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Sat May 18 21:43:02 2024

SamplerState s_s0_s : register(s0);
Texture2D<float4> t_s0 : register(t0);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  o0.xyzw = t_s0.Sample(s_s0_s, v1.xy).xyzw;

  o0.rgb = injectedData.toneMapGammaCorrection ? pow(o0.rgb, 2.2f) : linearFromSRGB(o0.rgb);
  o0.rgb *= injectedData.toneMapUINits / 80.f;
 
  return;
}