#include "./common.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Mon Jan 27 21:37:55 2025

SamplerState g_sDrawSource_s : register(s0);
Texture2D<float4> g_sDrawSource : register(t0);

// 3Dmigoto declarations
#define cmp -

void main(
    float2 v0: TEXCOORD0,
    out float4 o0: SV_Target0) {
  o0.xyzw = g_sDrawSource.Sample(g_sDrawSource_s, v0.xy).xyzw;
  o0.rgb = SwapchainPass(o0.rgb);
  return;
}
