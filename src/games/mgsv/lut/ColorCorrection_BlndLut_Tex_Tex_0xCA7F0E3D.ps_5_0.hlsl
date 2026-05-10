#include "../common.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:32 2026

// clang-format off
cbuffer cPSMaterial : register(b4)
{
  struct
  {
    float4 m_materials[8];
  } g_psMaterial : packoffset(c0);
}
// clang-format on

SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture3D<float4> inBaseLut0 : register(t0);
Texture3D<float4> inBaseLut1 : register(t1);


// 3Dmigoto declarations
#define cmp -


void main(
  float2 v0 : TEXCOORD0,
  float4 v1 : SV_Position0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = v0.xy;
  r0.xy = r0.xy;
  r0.z = -r0.y;
  r0.xyz = float3(0.5,0.5,0.5) * r0.xzx;
  r0.xyz = float3(0.5,0.5,0.5) + r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = float3(-0.001953125,-0.03125,-0.001953125) + r0.xyz;
  r0.xyz = float3(16,1,16) * r0.xyz;


  
  r0.x = frac(r0.x);
  r0.z = floor(r0.z);
  r0.xyz = float3(1,1,0.0625) * r0.xyz;
  
  float3 lut_input = r0.rgb;

  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r1.xyz = g_psMaterial.m_materials[1].xyz;
  r2.xy = g_psMaterial.m_materials[2].zw;
  r0.xyz = r0.xyz;
  r1.xyz = r1.xyz;
  r2.xy = r2.xy;
  r0.xyz = float3(0.03125,0.03125,0.03125) + r0.xyz;
  r3.xyzw = inBaseLut1.Sample(g_samplerLinear_Clamp_s, r0.xyz).xyzw;
  r3.xyzw = r3.xyzw;
  r2.x = r2.x;
  r2.y = r2.y;
  r3.xyzw = r3.xyzw;
  r4.xyzw = r3.xyzw * r3.xyzw;
  r5.xyzw = r4.xyzw * r3.xyzw;
  r2.x = r2.x;
  r2.y = r2.y;
  r6.xyzw = float4(2,2,2,2) * r4.xyzw;
  r6.xyzw = -r6.xyzw;
  r6.xyzw = r6.xyzw + r5.xyzw;
  r3.xyzw = r6.xyzw + r3.xyzw;
  r3.xyzw = r3.xyzw * r2.xxxx;
  r6.xyzw = float4(-2,-2,-2,-2) * r5.xyzw;
  r7.xyzw = float4(3,3,3,3) * r4.xyzw;
  r6.xyzw = r7.xyzw + r6.xyzw;
  r3.xyzw = r6.xyzw + r3.xyzw;
  r4.xyzw = -r4.xyzw;
  r4.xyzw = r5.xyzw + r4.xyzw;
  r2.xyzw = r4.xyzw * r2.yyyy;
  r2.xyzw = r3.xyzw + r2.xyzw;
  r2.xyzw = r2.xyzw;
  r2.xyzw = r2.xyzw;
  r2.xyz = r2.xyz * r1.xyz;
  r2.xyz = r2.xyz;
  r2.w = r2.w;
  r1.xyz = g_psMaterial.m_materials[0].xyz;
  r3.xy = g_psMaterial.m_materials[2].xy;
  r1.xyz = r1.xyz;
  r3.xy = r3.xy;
  r0.xyzw = inBaseLut0.Sample(g_samplerLinear_Clamp_s, r0.xyz).xyzw;
  r0.xyzw = r0.xyzw;
  r3.x = r3.x;
  r3.y = r3.y;
  r0.xyzw = r0.xyzw;
  r4.xyzw = r0.xyzw * r0.xyzw;
  r5.xyzw = r4.xyzw * r0.xyzw;
  r3.x = r3.x;
  r3.y = r3.y;
  r6.xyzw = float4(2,2,2,2) * r4.xyzw;
  r6.xyzw = -r6.xyzw;
  r6.xyzw = r6.xyzw + r5.xyzw;
  r0.xyzw = r6.xyzw + r0.xyzw;
  r0.xyzw = r0.xyzw * r3.xxxx;
  r6.xyzw = float4(-2,-2,-2,-2) * r5.xyzw;
  r7.xyzw = float4(3,3,3,3) * r4.xyzw;
  r6.xyzw = r7.xyzw + r6.xyzw;
  r0.xyzw = r6.xyzw + r0.xyzw;
  r4.xyzw = -r4.xyzw;
  r4.xyzw = r5.xyzw + r4.xyzw;
  r3.xyzw = r4.xyzw * r3.yyyy;
  r0.xyzw = r3.xyzw + r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyz = r0.xyz * r1.xyz;
  r0.xyz = r0.xyz;
  r0.w = r0.w;
  r0.xyzw = r0.xyzw;
  r2.xyzw = r2.xyzw;
  r0.xyzw = r0.xyzw;
  r2.xyzw = r2.xyzw;
  r1.xyzw = g_psMaterial.m_materials[3].xxxx;
  r3.xyzw = -r0.xyzw;
  r2.xyzw = r3.xyzw + r2.xyzw;
  r1.xyzw = r2.xyzw * r1.xyzw;
  r0.xyzw = r1.xyzw + r0.xyzw;
  r0.xyzw = r0.xyzw;
  o0.xyzw = r0.xyzw;

  if (RENODX_TONE_MAP_TYPE != 0.f) {

  }
  // o0.rgb = lerp(lut_input, o0.rgb, RENODX_COLOR_GRADE_STRENGTH);

  return;
}