// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:30 2026

#include "../../shared.h"

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
  }
g_psScene: packoffset(c0);
}

cbuffer cPSObject : register(b5) {
  struct
  {
    float4x4 m_viewWorld;
    float4x4 m_world;
    float4 m_useWeightCount;
    float4 m_localParam[4];
  }
g_psObject: packoffset(c0);
}

cbuffer cPSSystem : register(b0) {
  struct
  {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  }
g_psSystem: packoffset(c0);
}
// clang-format on

SamplerState g_samplerPoint_Clamp_s : register(s9);
SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inDepth : register(t2);
Texture2D<float4> inVelocity : register(t3);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position0,
    float4 v1: TEXCOORD0,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2, r3, r4;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5, -0.5) + v0.xy;
  r0.zw = v1.zw;
  r1.xy = v1.xy;
  r0.xy = r0.xy;
  r0.zw = r0.zw;
  r1.xy = r1.xy;
  r0.xy = r0.xy;
  r1.zw = g_psObject.m_localParam[0].xy;
  r2.xy = g_psSystem.m_renderInfo.xy;
  r2.xy = float2(0.5, 0.5) * r2.xy;
  r2.xy = r2.xy / float2(64, 64);
  r1.xy = g_psSystem.m_renderInfo.xy * r1.xy;
  r1.xy = g_psSystem.m_renderBuffer.zw * r1.xy;
  r0.xy = g_psObject.m_localParam[0].zw * r0.xy;
  r0.xy = floor(r0.xy);
  r0.xy = float2(0.5, 0.5) + r0.xy;
  r0.xy = r0.xy * r1.zw;
  r1.xyz = inVelocity.Sample(g_samplerLinear_Clamp_s, r1.xy).xzw;
  r1.xyz = r1.xyz;
  r1.yz = r1.yz;
  r0.xy = r0.xy;
  r2.zw = -g_psScene.m_cameraCenterOffset.xy;
  r2.zw = r2.zw + r0.zw;
  r3.xyzw = g_psScene.m_projectionParam.xyzw;
  r0.x = inDepth.SampleLevel(g_samplerPoint_Clamp_s, r0.xy, 0).x;
  r0.x = r0.x;
  r2.zw = r2.zw;
  r0.x = r0.x;
  r3.xyzw = r3.xyzw;
  r0.y = -r3.w;
  r0.x = r0.x + r0.y;
  r4.z = r3.z / r0.x;
  r0.x = r4.z;
  r2.zw = r3.xy * r2.zw;
  r4.xy = r2.zw * r0.xx;
  r4.w = 1;
  r3.x = dot(r4.xyzw, g_psScene.m_shadowProjection2._m00_m10_m20_m30);
  r3.y = dot(r4.xyzw, g_psScene.m_shadowProjection2._m01_m11_m21_m31);
  r3.z = dot(r4.xyzw, g_psScene.m_shadowProjection2._m02_m12_m22_m32);
  r3.w = dot(r4.xyzw, g_psScene.m_shadowProjection2._m03_m13_m23_m33);
  r4.x = dot(r3.xyzw, g_psScene.m_view._m00_m10_m20_m30);
  r4.y = dot(r3.xyzw, g_psScene.m_view._m01_m11_m21_m31);
  r4.z = dot(r3.xyzw, g_psScene.m_view._m03_m13_m23_m33);
  r4.xyz = r4.xyz;
  r0.xy = r4.xy / r4.zz;
  r0.xy = -r0.xy;
  r0.xy = r0.zw + r0.xy;
  r0.xy = r0.xy * r2.xy;
  r0.z = dot(r0.xy, r0.xy);
  r0.w = sqrt(r0.z);
  r0.w = max(0, r0.w);
  if (CUSTOM_TAA == 0.f || CUSTOM_UNCLAMP_MOTION_VECTORS == 0.f) r0.w = min(1, r0.w);
  r1.w = cmp(r0.w == 0.000000);
  r0.z = rsqrt(r0.z);
  r0.xy = r0.xy * r0.zz;
  r0.xy = r0.xy * r0.ww;
  r0.xy = r1.ww ? float2(0, 0) : r0.xy;
  r0.z = -r0.y;
  r0.xy = float2(0.5, 0.5) * r0.xz;
  r0.xy = float2(0.5, 0.5) + r0.xy;
  r0.zw = r1.xx;
  r1.xw = -r0.xy;
  r1.xy = r1.yz + r1.xw;
  r0.zw = r1.xy * r0.zw;
  r0.zw = r0.xy + r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r0.xy = float2(0, 0);
  r0.zw = r0.zw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  o0.xyzw = r0.xyzw;
  return;
}
