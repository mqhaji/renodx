#include "../tonemap.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:31 2026

// clang-format off
cbuffer cPSMaterial : register(b4) {
  struct
  {
    float4 m_materials[8];
  } g_psMaterial: packoffset(c0);
}
// clang-format on

SamplerState inTextureSampler_s : register(s0);
Texture2D<float4> inTexture : register(t0);

// 3Dmigoto declarations
#define cmp -

void main(
    float2 v0: TEXCOORD0,
    float w0: TEXCOORD3,
    float4 v1: TEXCOORD1,
    float4 v2: TEXCOORD2,
    float4 v3: SV_Position0,
    out float4 o0: SV_Target0,
    out float4 o1: SV_Target1) {
  float4 r0, r1, r2, r3, r4;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyz = v1.xyz;
  r1.xy = v2.xy;
  r0.xyz = r0.xyz;
  r1.xy = r1.xy;
  r2.xyzw = inTexture.Sample(inTextureSampler_s, v0.xy).xyzw;
  r2.xyz = r2.xyz / r2.www;
  r0.xyz = r2.xyz * r0.xyz;
  r1.xy = r1.xy;
  r0.w = r1.x * r1.y;
  r0.xyz = r0.xyz * r0.www;
  r0.xyz = max(float3(0, 0, 0), r0.xyz);
  r0.xyz = min(float3(154.600006, 154.600006, 154.600006), r0.xyz);
  r0.xyz = log2(r0.xyz);
  r0.xyz = float3(2.20000005, 2.20000005, 2.20000005) * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;

  // TppTonemap
#if 1
  r0.rgb = ApplyTppTonemap(r0.rgb, g_psMaterial.m_materials[0].xyz);
#else
  r1.xyz = g_psMaterial.m_materials[0].xyz;
  r0.xyz = r0.xyz;
  r2.xyz = r1.yyy;
  r2.xzw = cmp(r2.xyz >= r0.xyz);
  r2.xzw = r2.xzw ? float3(1, 1, 1) : float3(0, 0, 0);
  r3.xyz = r2.xzw * r0.xyz;
  r2.xzw = -r2.xzw;
  r2.xzw = float3(1, 1, 1) + r2.xzw;
  r0.xyz = r0.xyz + r1.zzz;
  r4.xyz = -r2.yyy;
  r0.xyz = r4.xyz + r0.xyz;
  r0.xyz = r1.xxx * r0.xyz;
  r0.xyz = float3(-1, -1, -1) / r0.xyz;
  r0.xyz = r0.xyz + r1.zzz;
  r0.xyz = r0.xyz + r2.yyy;
  r0.xyz = r2.xzw * r0.xyz;
  r0.xyz = r3.xyz + r0.xyz;
#endif

  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r1.xyz = cmp(float3(0.00313080009, 0.00313080009, 0.00313080009) >= r0.xyz);
  r1.xyz = r1.xyz ? float3(1, 1, 1) : float3(0, 0, 0);
  r2.xyz = float3(12.9200001, 12.9200001, 12.9200001) * r0.xyz;
  r2.xyz = r2.xyz * r1.xyz;
  r1.xyz = -r1.xyz;
  r1.xyz = float3(1, 1, 1) + r1.xyz;
  r0.xyz = max(float3(9.99999975e-06, 9.99999975e-06, 9.99999975e-06), r0.xyz);
  r0.xyz = log2(r0.xyz);
  r0.xyz = float3(0.416666657, 0.416666657, 0.416666657) * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  r0.xyz = float3(1.05499995, 1.05499995, 1.05499995) * r0.xyz;
  r0.xyz = float3(-0.0549999997, -0.0549999997, -0.0549999997) + r0.xyz;
  r0.xyz = r1.xyz * r0.xyz;
  r0.xyz = r2.xyz + r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = float3(1, 1, 1) * r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = 1;
  o0.xyzw = r0.xyzw;
  o1.xyzw = r0.xyzw;
  return;
}
