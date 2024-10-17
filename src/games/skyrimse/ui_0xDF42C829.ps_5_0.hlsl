#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Sat May 18 02:42:34 2024

cbuffer PSConstants : register(b0)
{
  float4 color : packoffset(c0);
}



// 3Dmigoto declarations
#define cmp -


void main(
  out float4 o0 : SV_Target0)
{
  o0.xyzw = color.xyzw;

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