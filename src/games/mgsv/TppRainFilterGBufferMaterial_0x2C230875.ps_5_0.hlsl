#include "./common.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:23 2026

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
  } g_psScene: packoffset(c0);
}

cbuffer cPSMaterial : register(b4) {
  struct
  {
    float4 m_materials[8];
  } g_psMaterial: packoffset(c0);
}

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

SamplerState g_samplerPoint_Wrap_s : register(s8);
SamplerState g_samplerPoint_Clamp_s : register(s9);
SamplerState g_samplerLinear_Wrap_s : register(s10);
SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inViewportCaptureMap : register(t0);
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
  r0.zw = v0.xy;
  r0.xy = r0.xy;
  r0.zw = r0.zw;
  r0.xy = r0.xy;
  r1.xy = -g_psScene.m_cameraCenterOffset.xy;
  r0.zw = r1.xy + r0.zw;
  r1.xyzw = g_psScene.m_projectionParam.xyzw;
  r2.x = inDepthMap.SampleLevel(g_samplerPoint_Clamp_s, r0.xy, 0).x;
  r2.x = r2.x;
  r0.zw = r0.zw;
  r2.x = r2.x;
  r1.xyzw = r1.xyzw;
  r1.w = -r1.w;
  r1.w = r2.x + r1.w;
  r2.z = r1.z / r1.w;
  r1.z = r2.z;
  r0.zw = r1.xy * r0.zw;
  r2.xy = r0.zw * r1.zz;
  r1.z = r1.z;
  r1.z = r1.z;
  r1.z = r1.z;
  r0.z = -g_psMaterial.m_materials[2].x;
  r0.z = r1.z + r0.z;
  r0.z = g_psMaterial.m_materials[3].x * r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r2.w = 1;
  r1.x = dot(r2.xyzw, g_psScene.m_view._m00_m10_m20_m30);
  r1.y = dot(r2.xyzw, g_psScene.m_view._m01_m11_m21_m31);
  r1.z = dot(r2.xyzw, g_psScene.m_view._m03_m13_m23_m33);
  r1.xyz = r1.xyz;
  r1.xy = float2(0.0299999993, 0.0299999993) * r1.xy;
  r1.xy = float2(0.5, -0.5) * r1.xy;
  r1.xy = float2(0.5, 0.5) + r1.xy;
  r0.w = g_psMaterial.m_materials[0].w * r1.z;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r1.xy = r1.xy;
  r0.w = r0.w;
  r1.zw = float2(-0.5, -0.5) * g_psSystem.m_renderBuffer.zw;
  r1.xy = r1.xy + r1.zw;
  r1.xyzw = inViewportCaptureMap.Sample(g_samplerLinear_Clamp_s, r1.xy).xzyw;
  r1.xy = r1.xy;
  r1.zw = r1.zw;
  r1.xy = float2(0.0399999991, 0.0399999991) + r1.xy;
  r2.xy = -r0.ww;
  r1.xy = r2.xy + r1.xy;
  r1.xy = r1.xy / float2(0.0399999991, 0.0399999991);
  r1.xy = max(float2(0, 0), r1.xy);
  r1.xy = min(float2(1, 1), r1.xy);
  r1.zw = r1.xy * r1.zw;
  r1.xy = -r1.xy;
  r1.xy = float2(1, 1) + r1.xy;
  r1.xy = r1.zw + r1.xy;
  r0.w = r1.x * r1.y;
  r0.w = r0.w;
  r1.xyz = r0.www * r0.zzz;
  r1.xyz = r1.xyz;
  r2.xyzw = inAlbedoMap.Sample(g_samplerLinear_Wrap_s, r0.xy).xyzw;
  // r2.rgb = renodx::color::srgb::Decode(max(0, r2.rgb));

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
