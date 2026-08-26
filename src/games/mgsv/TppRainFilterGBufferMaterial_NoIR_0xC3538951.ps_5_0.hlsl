#include "./common.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:32 2026

// clang-format off
cbuffer cPSScene : register(b2) {
  struct
  {
    float4x4 m_projectionView;
    float4x4 m_projection;
    float4x4 m_view;
    float4x4 m_shadowProjection;
    float4x4 m_shadowProjection2;
    float4 m_eyepos;
    float4 m_projectionParam;
    float4 m_viewportSize;
    float4 m_exposure;
    float4 m_fogParam[3];
    float4 m_fogColor;
    float4 m_cameraCenterOffset;
    float4 m_shadowMapResolutions;
  } g_psScene:
  packoffset(c0);
}

cbuffer cPSMaterial : register(b4) {
  struct
  {
    float4 m_materials[8];
  } g_psMaterial:
  packoffset(c0);
}

cbuffer cPSSystem : register(b0) {
  struct
  {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem:
  packoffset(c0);
}
// clang-format on

SamplerState g_samplerPoint_Wrap_s : register(s8);
SamplerState g_samplerPoint_Clamp_s : register(s9);
SamplerState g_samplerLinear_Wrap_s : register(s10);
Texture2D<float4> inAlbedoMap : register(t8);
Texture2D<float4> inMaterialMap : register(t11);
Texture2D<float4> inDepthMap : register(t12);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: TEXCOORD0,
    float4 v1: SV_Position0,
    out float4 o0: SV_Target0,
    out float4 o1: SV_Target1) {
  float4 r0, r1, r2, r3, r4;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5, -0.5) + v1.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = float2(0.49609375, 0.49609375) + r0.xy;
  r0.xy = g_psSystem.m_renderBuffer.zw * r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.zw = g_psScene.m_projectionParam.zw;
  r1.x = inDepthMap.SampleLevel(g_samplerPoint_Clamp_s, r0.xy, 0).x;
  r1.x = r1.x;
  r1.x = r1.x;
  r0.zw = r0.zw;
  r0.w = -r0.w;
  r0.w = r1.x + r0.w;
  r0.z = r0.z / r0.w;
  r0.z = r0.z;
  r0.z = r0.z;
  r0.z = r0.z;
  r0.w = -g_psMaterial.m_materials[2].x;
  r0.z = r0.z + r0.w;
  r0.z = g_psMaterial.m_materials[3].x * r0.z;
  r0.z = max(0, r0.z);
  r1.xyz = min(float3(1, 1, 1), r0.zzz);
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r2.xyzw = inAlbedoMap.Sample(g_samplerLinear_Wrap_s, r0.xy).xyzw;

  // #if FIX_UNORM_SRGB
  //   r2.xyz = renodx::color::srgb::Decode(max(0, r2.xyz));
  // #endif

  r0.xyzw = inMaterialMap.Sample(g_samplerPoint_Wrap_s, r0.xy).xyzw;
  r2.xyzw = r2.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r1.w = r1.z;
  r0.xyzw = r0.xyzw;
  r1.w = r1.w;
  r0.x = r0.x;
  r3.x = g_psMaterial.m_materials[0].z;
  r3.y = g_psMaterial.m_materials[3].y;
  r3.z = -r3.x;
  r3.z = r3.z + r0.x;
  r3.y = r3.y * r3.z;
  r3.y = r3.y + r3.x;
  r3.x = cmp(r0.x >= r3.x);
  r3.x = r3.x ? 1 : 0;
  r1.w = r3.x * r1.w;
  r3.x = -r0.x;
  r3.x = r3.y + r3.x;
  r1.w = r3.x * r1.w;
  r0.x = r1.w + r0.x;
  r0.yzw = r0.yzw;
  r0.xyzw = r0.xyzw;
  r2.xyz = r2.xyz;
  r1.xyz = r1.xyz;
  r2.xyz = r2.xyz;
  r1.xyz = r1.xyz;
  r3.xyz = g_psMaterial.m_materials[0].xxx * r2.xyz;
  r4.xyz = -r2.xyz;
  r3.xyz = r4.xyz + r3.xyz;
  r1.xyz = r3.xyz * r1.xyz;
  r1.xyz = r2.xyz + r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r3.xyz = cmp(float3(0.00313080009, 0.00313080009, 0.00313080009) >= r1.xyz);
  r3.xyz = r3.xyz ? float3(1, 1, 1) : float3(0, 0, 0);
  r4.xyz = float3(12.9200001, 12.9200001, 12.9200001) * r1.xyz;
  r4.xyz = r4.xyz * r3.xyz;
  r3.xyz = -r3.xyz;
  r3.xyz = float3(1, 1, 1) + r3.xyz;
  r1.xyz = max(float3(9.99999975e-06, 9.99999975e-06, 9.99999975e-06), r1.xyz);
  r1.xyz = log2(r1.xyz);
  r1.xyz = float3(0.416666657, 0.416666657, 0.416666657) * r1.xyz;
  r1.xyz = exp2(r1.xyz);
  r1.xyz = float3(1.05499995, 1.05499995, 1.05499995) * r1.xyz;
  r1.xyz = float3(-0.0549999997, -0.0549999997, -0.0549999997) + r1.xyz;
  r1.xyz = r3.xyz * r1.xyz;
  r2.xyz = r4.xyz + r1.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.w = r2.w;
  r2.xyz = r2.xyz;
  r2.w = r2.w;
  r2.xyz = r2.xyz;
  r2.w = r2.w;
  r2.xyzw = r2.xyzw;
  o0.xyzw = r2.xyzw;
  o1.xyzw = r0.xyzw;
  return;
}
