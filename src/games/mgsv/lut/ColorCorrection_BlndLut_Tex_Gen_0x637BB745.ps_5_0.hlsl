#include "../common.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:26 2026

// clang-format off
cbuffer cPSMaterial : register(b4) {
  struct
  {
    float4 m_materials[8];
  } g_psMaterial: packoffset(c0);
}
// clang-format on

SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture3D<float4> inBaseLut0 : register(t0);

// 3Dmigoto declarations
#define cmp -

void main(
    float2 v0: TEXCOORD0,
    float4 v1: SV_Position0,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2, r3, r4, r5, r6, r7;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = v0.xy;
  r0.xy = r0.xy;
  r0.z = -r0.y;
  r0.xyz = float3(0.5, 0.5, 0.5) * r0.xzx;
  r0.xyz = float3(0.5, 0.5, 0.5) + r0.xyz;
  r0.xyz = r0.xyz;
  r1.xyz = r0.zyz;
  r1.xyz = r1.xyz;
  r1.xyz = float3(-0.001953125, -0.03125, -0.001953125) + r1.xyz;
  r1.xyz = float3(16, 1, 16) * r1.xyz;
  
  
  r1.x = frac(r1.x);
  r1.z = floor(r1.z);
  r1.xyz = float3(1, 1, 0.0625) * r1.xyz;
  
  float3 lut_input = r1.rgb;

  r1.xyz = r1.xyz;
  r0.xyz = r0.xyz;
  r2.xyz = g_psMaterial.m_materials[1].xyz;
  r3.xy = g_psMaterial.m_materials[2].zw;
  r0.xyz = r0.xyz;
  r2.xyz = r2.xyz;
  r3.xy = r3.xy;
  r0.xyz = r0.xyz;
  r0.xyz = float3(-0.001953125, -0.03125, -0.001953125) + r0.xyz;
  r0.xyz = float3(16, 1, 16) * r0.xyz;
  r0.x = frac(r0.x);
  r0.z = floor(r0.z);
  r0.xyz = float3(1.06666672, 1.06666672, 0.0666666701) * r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = max(float3(0, 0, 0), r0.xyz);
  r0.xyz = min(float3(1, 1, 1), r0.xyz);
  r0.xyz = r0.xyz;
  r3.x = r3.x;
  r3.y = r3.y;
  r0.xyz = r0.xyz;
  r4.xyz = r0.xyz * r0.xyz;
  r5.xyz = r4.xyz * r0.xyz;
  r3.x = r3.x;
  r3.y = r3.y;
  r6.xyz = float3(2, 2, 2) * r4.xyz;
  r6.xyz = -r6.xyz;
  r6.xyz = r6.xyz + r5.xyz;
  r0.xyz = r6.xyz + r0.xyz;
  r0.xyz = r0.xyz * r3.xxx;
  r3.xzw = float3(-2, -2, -2) * r5.xyz;
  r6.xyz = float3(3, 3, 3) * r4.xyz;
  r3.xzw = r6.xyz + r3.xzw;
  r0.xyz = r3.xzw + r0.xyz;
  r3.xzw = -r4.xyz;
  r3.xzw = r5.xyz + r3.xzw;
  r3.xyz = r3.xzw * r3.yyy;
  r0.xyz = r3.xyz + r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz * r2.xyz;
  r0.xyz = r0.xyz;
  r0.w = 1;
  r1.xyz = r1.xyz;
  r2.xyz = g_psMaterial.m_materials[0].xyz;
  r3.xy = g_psMaterial.m_materials[2].xy;
  r1.xyz = r1.xyz;
  r2.xyz = r2.xyz;
  r3.xy = r3.xy;
  r1.xyz = float3(0.03125, 0.03125, 0.03125) + r1.xyz;
  r1.xyzw = inBaseLut0.Sample(g_samplerLinear_Clamp_s, r1.xyz).xyzw;
  r1.xyzw = r1.xyzw;
  r3.x = r3.x;
  r3.y = r3.y;
  r1.xyzw = r1.xyzw;
  r4.xyzw = r1.xyzw * r1.xyzw;
  r5.xyzw = r4.xyzw * r1.xyzw;
  r3.x = r3.x;
  r3.y = r3.y;
  r6.xyzw = float4(2, 2, 2, 2) * r4.xyzw;
  r6.xyzw = -r6.xyzw;
  r6.xyzw = r6.xyzw + r5.xyzw;
  r1.xyzw = r6.xyzw + r1.xyzw;
  r1.xyzw = r1.xyzw * r3.xxxx;
  r6.xyzw = float4(-2, -2, -2, -2) * r5.xyzw;
  r7.xyzw = float4(3, 3, 3, 3) * r4.xyzw;
  r6.xyzw = r7.xyzw + r6.xyzw;
  r1.xyzw = r6.xyzw + r1.xyzw;
  r4.xyzw = -r4.xyzw;
  r4.xyzw = r5.xyzw + r4.xyzw;
  r3.xyzw = r4.xyzw * r3.yyyy;
  r1.xyzw = r3.xyzw + r1.xyzw;
  r1.xyzw = r1.xyzw;
  r1.xyzw = r1.xyzw;
  r1.xyz = r1.xyz * r2.xyz;
  r1.xyz = r1.xyz;
  r1.w = r1.w;
  r1.xyzw = r1.xyzw;
  r0.xyzw = r0.xyzw;
  r1.xyzw = r1.xyzw;
  r0.xyzw = r0.xyzw;
  r2.xyzw = g_psMaterial.m_materials[3].xxxx;
  r3.xyzw = -r1.xyzw;
  r0.xyzw = r3.xyzw + r0.xyzw;
  r0.xyzw = r2.xyzw * r0.xyzw;
  r0.xyzw = r1.xyzw + r0.xyzw;
  r0.xyzw = r0.xyzw;
  o0.xyzw = r0.xyzw;

  return;
}
