#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Sat May 18 02:42:02 2024

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
  float4 v1 : COLOR1,
  float2 v2 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = tex0.Sample(samp_s, v2.xy).xyzw;
  r0.xyzw = -v0.xyzw + r0.xyzw;
  r0.xyzw = v1.zzzz * r0.xyzw + v0.xyzw;
  r0.xyzw = r0.xyzw * cxmul.xyzw + cxadd.xyzw;
  o0.w = v1.w * r0.w;
  o0.xyz = r0.xyz;

  o0.xyzw = saturate(o0.xyzw);
  o0.rgb = (injectedData.toneMapGammaCorrection
                ? pow(o0.rgb, 2.2f)
                : renodx::color::bt709::from::SRGB(o0.rgb));
  o0.rgb *= injectedData.toneMapUINits / 80.f;
  return;
}