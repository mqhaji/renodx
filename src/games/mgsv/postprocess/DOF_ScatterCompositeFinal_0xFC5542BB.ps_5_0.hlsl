#include "../common.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:35 2026

// clang-format off
cbuffer cPSSystem : register(b0) {
  struct
  {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem: packoffset(c0);
}
// clang-format on

SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inTexture0 : register(t0);
Texture2D<float4> inTexture1 : register(t1);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position0,
    float2 v1: TEXCOORD0,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2, r3, r4;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = v1.xy;
  r0.xy = r0.xy;
  r0.xy = g_psSystem.m_renderInfo.xy * r0.xy;
  r0.xy = g_psSystem.m_renderBuffer.zw * r0.xy;
  r1.xyzw = inTexture0.Sample(g_samplerLinear_Clamp_s, r0.xy).wxyz;

  r0.xyzw = inTexture1.Sample(g_samplerLinear_Clamp_s, r0.xy).xyzw;

  r1.x = max(0, r1.x);
  r1.x = min(1, r1.x);
  r1.x = -r1.x;
  r1.x = 1 + r1.x;
  r0.xyz = r0.xyz;
  r2.xyz = cmp(float3(0.0392800011, 0.0392800011, 0.0392800011) >= r0.xyz);
  r2.xyz = r2.xyz ? float3(1, 1, 1) : float3(0, 0, 0);

  r3.xyz = r0.xyz / float3(12.9200001, 12.9200001, 12.9200001);
  r3.xyz = r3.xyz * r2.xyz;
  r2.xyz = -r2.xyz;
  r2.xyz = float3(1, 1, 1) + r2.xyz;
  r4.xyz = float3(0.0549999997, 0.0549999997, 0.0549999997) + r0.xyz;
  r4.xyz = r4.xyz / float3(1.05499995, 1.05499995, 1.05499995);
  r4.xyz = max(float3(9.99999975e-06, 9.99999975e-06, 9.99999975e-06), r4.xyz);
  r4.xyz = log2(r4.xyz);
  r4.xyz = float3(2.4000001, 2.4000001, 2.4000001) * r4.xyz;
  r4.xyz = exp2(r4.xyz);
  r2.xyz = r4.xyz * r2.xyz;
  r2.xyz = r3.xyz + r2.xyz;

  r2.xyz = r2.xyz * r1.xxx;
  r1.xyz = r2.xyz + r1.yzw;
  r1.xyz = r1.xyz;
  r2.xyz = cmp(float3(0.00313080009, 0.00313080009, 0.00313080009) >= r1.xyz);
  r2.xyz = r2.xyz ? float3(1, 1, 1) : float3(0, 0, 0);

  r3.xyz = float3(12.9200001, 12.9200001, 12.9200001) * r1.xyz;
  r3.xyz = r3.xyz * r2.xyz;
  r2.xyz = -r2.xyz;
  r2.xyz = float3(1, 1, 1) + r2.xyz;
  r1.xyz = max(float3(9.99999975e-06, 9.99999975e-06, 9.99999975e-06), r1.xyz);
  r1.xyz = log2(r1.xyz);
  r1.xyz = float3(0.416666657, 0.416666657, 0.416666657) * r1.xyz;
  r1.xyz = exp2(r1.xyz);
  r1.xyz = float3(1.05499995, 1.05499995, 1.05499995) * r1.xyz;
  r1.xyz = float3(-0.0549999997, -0.0549999997, -0.0549999997) + r1.xyz;
  r1.xyz = r2.xyz * r1.xyz;
  r0.xyz = r3.xyz + r1.xyz;

  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = r0.w;
  r0.xyz = r0.xyz;
  r0.w = r0.w;
  o0.xyzw = r0.xyzw;

  o0 = max(0, o0);  // fixes black dots
  return;
}
