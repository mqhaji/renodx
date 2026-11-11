#include "../indirect/compose/specular_composition.hlsli"
#include "../../tonemap/tonemap.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:24 2026

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

cbuffer cPSObject : register(b5) {
  struct
  {
    float4x4 m_viewWorld;
    float4x4 m_world;
    float4 m_useWeightCount;
    float4 m_localParam[4];
  } g_psObject: packoffset(c0);
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
Texture2D<float4> inAlbedo : register(t8);
Texture2D<float4> inLightDiffuse : register(t9);
Texture2D<float4> inLightSpecular : register(t10);
Texture2D<float4> inDepth : register(t11);
Texture2D<float4> inOcclusion : register(t13);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0 : SV_Position0,
    float2 v1 : TEXCOORD0,
    out float4 o0 : SV_Target0,
    out float4 reflection_mask : SV_Target1) {
  float4 r0, r1, r2, r3, r4, r5;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5, -0.5) + v0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = float2(0.49609375, 0.49609375) + r0.xy;
  r0.xy = g_psSystem.m_renderBuffer.zw * r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.zw = g_psScene.m_projectionParam.zw;
  r1.x = inDepth.SampleLevel(g_samplerPoint_Clamp_s, r0.xy, 0).x;
  r1.x = r1.x;
  r1.x = r1.x;
  r0.zw = r0.zw;
  r0.w = -r0.w;
  r0.w = r1.x + r0.w;
  r0.z = r0.z / r0.w;
  r0.z = r0.z;
  r0.z = r0.z;
  r0.z = r0.z;
  r0.z = r0.z;
  r1.xyz = inLightSpecular.Sample(g_samplerPoint_Wrap_s, r0.xy).xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r2.xyz = inLightDiffuse.Sample(g_samplerPoint_Wrap_s, r0.xy).xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r1.xyz = r1.xyz;
  r2.xyz = r2.xyz;
  r1.xyz = r1.xyz;
  r3.xyz = inAlbedo.Sample(g_samplerLinear_Wrap_s, r0.xy).xyz;
  r3.xyz = r3.xyz;
  const float2 specular_uv = r0.xy;
  r0.x = inOcclusion.Sample(g_samplerLinear_Wrap_s, r0.xy).x;
  const float3 specular_correction = GetEnvironmentSpecularCorrection(specular_uv, r0.x, r1.xyz, r3.xyz, r2.xyz);
  r0.xyw = r0.xxx;
  r2.xyz = r3.xyz * r2.xyz;
  // Diffuse already has ambient-only AO; the correction separates specular below.
  if (CUSTOM_MGSV_LIGHTING_ENABLED != 0.f) {
    r0.xyw = r2.xyz + r1.xyz * r0.xyw;
  } else {
    r1.xyz = r2.xyz + r1.xyz;
    r0.xyw = r1.xyz * r0.xyw;
  }
  if (CUSTOM_MGSV_LIGHTING_ENABLED != 0.0) {
    r0.xyw += specular_correction;
  }
  r0.xyw = r0.xyw;
  r0.xyw = r0.xyw;
  r0.z = r0.z;
  r0.xyw = r0.xyw;
  r0.z = r0.z;
  r0.z = g_psScene.m_fogParam[0].x * r0.z;
  r0.z = g_psScene.m_fogParam[0].y + r0.z;
  r0.z = max(g_psScene.m_fogParam[0].z, r0.z);
  r1.xyz = min(g_psScene.m_fogParam[0].www, r0.zzz);
  r2.xyz = -r0.xyw;
  r2.xyz = g_psScene.m_fogColor.xyz + r2.xyz;
  r1.xyz = r2.xyz * r1.xyz;
  r0.xyz = r1.xyz + r0.xyw;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = dot(r0.xyz, float3(0.212500006, 0.715399981, 0.0720999986));
  r0.w = 0.03125 * r0.w;
  r1.x = max(0.001953125, r0.w);
  r1.x = rsqrt(r1.x);
  r1.w = r1.x * r0.w;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;

  float A = g_psObject.m_localParam[1].x, B = g_psObject.m_localParam[1].y, C = g_psObject.m_localParam[1].z, D = g_psObject.m_localParam[1].w;
  float E = g_psObject.m_localParam[2].x, F = g_psObject.m_localParam[2].y, W = g_psObject.m_localParam[2].z;
  r1.rgb = ApplyUnchartedFilmicTonemap(r0.rgb, A, B, C, D, E, F, W);

  r1.w = r1.w;
  o0.xyzw = r1.xyzw;

#if FIX_UNORM_SRGB
  o0.rgb = renodx::color::srgb::Encode(max(0, o0.rgb));
#endif
  reflection_mask = specular_replacement_applied ? 1.0 : 0.0;
  return;
}
