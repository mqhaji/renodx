#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Fri Jul 26 00:52:29 2024

Texture2DMS<float4,2> t_InputTexture : register(t0);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2,r3;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = (int2)v0.xy;
  r0.zw = float2(0,0);
  r1.xyzw = float4(0,0,0,0);
  r2.x = 0;
  while (true) {
    r2.y = cmp((int)r2.x >= 2);
    if (r2.y != 0) break;
    r3.xyzw = t_InputTexture.Load(r0.xy, r2.x).xyzw;
    r1.xyzw = r3.xyzw + r1.xyzw;
    r2.x = (int)r2.x + 1;
  }
  o0.xyzw = float4(0.5,0.5,0.5,0.5) * r1.xyzw;

  o0.rgb = saturate(o0.rgb);
  o0.rgb = (injectedData.toneMapGammaCorrection
                ? pow(o0.rgb, 2.2f)
                : renodx::color::bt709::from::SRGB(o0.rgb));
  o0.rgb *= injectedData.toneMapUINits / 80.f;
  return;
}