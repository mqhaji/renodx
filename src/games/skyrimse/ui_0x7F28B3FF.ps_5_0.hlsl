#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Sat May 18 02:41:48 2024

cbuffer PSConstants : register(b0)
{
  float4 cxmul : packoffset(c0);
  float4 cxadd : packoffset(c1);
}

SamplerState samp_s : register(s0);
Texture2D<float4> tex0 : register(t0);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : COLOR0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = tex0.Sample(samp_s, v1.xy).xyzw;
  r1.xyzw = v0.xyzw * cxmul.xyzw + cxadd.xyzw;
  o0.w = r1.w * r0.w;
  o0.xyz = r1.xyz;

  o0.xyzw = saturate(o0.xyzw);
  if (injectedData.toneMapGammaCorrection == 1) {
    o0.xyz = pow(o0.xyz, 2.2);
    o0.xyz *= injectedData.toneMapUINits / injectedData.toneMapGameNits;
    o0.xyz = pow(o0.xyz, 1.0 / 2.2);
  } else {
    o0.xyz = renodx::color::srgb::Decode(o0.xyz);
    o0.xyz *= injectedData.toneMapUINits / injectedData.toneMapGameNits;
    o0.xyz = renodx::color::srgb::Encode(o0.xyz);
  }
  return;
}