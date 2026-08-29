#include "./common.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:22 2026

// clang-format off
cbuffer cVSScene : register(b2) {
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
  } g_vsScene: packoffset(c0);
}

cbuffer cVSObject : register(b5) {
  struct
  {
    float4x4 m_viewWorld;
    float4x4 m_world;
    float4 m_useWeightCount;
    float4 m_localParam[4];
  } g_vsObject: packoffset(c0);
}

cbuffer VSBones : register(b6) {
  struct
  {
    float4x3 m_boneMatrices[32];
  } g_vsBone: packoffset(c0);
  
  struct
  {
    float4x3 m_boneMatrices[32];
  } g_vsPrevBone: packoffset(c96);
}
// clang-format on

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: POSITION0,
    float4 v1: NORMAL0,
    float4 v2: COLOR0,
    float4 v3: TANGENT0,
    uint4 v4: BLENDINDICES0,
    float4 v5: BLENDWEIGHT0,
    float2 v6: TEXCOORD0,
    out float4 o0: SV_Position0,
    out float2 o1: TEXCOORD0,
    out float3 o2: TEXCOORD1) {
  float4 r0, r1, r2, r3, r4, r5, r6, r7;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyz = v0.xyz;
  r1.xyzw = v5.xyzw;
  r0.xyz = r0.xyz;
  r1.xyzw = r1.xyzw;
  r2.x = cmp(g_vsObject.m_useWeightCount.x != 0.000000);
  if (r2.x != 0) {
    r1.xyzw = r1.xyzw;
    r2.x = (int)v4.x * 3;
    r3.xyzw = g_vsBone.m_boneMatrices[v4.x]._m00_m10_m20_m30 * r1.xxxx;
    r4.xyzw = g_vsBone.m_boneMatrices[v4.x]._m01_m11_m21_m31 * r1.xxxx;
    r2.xyzw = g_vsBone.m_boneMatrices[v4.x]._m02_m12_m22_m32 * r1.xxxx;
    r1.x = (int)v4.y * 3;
    r5.xyzw = g_vsBone.m_boneMatrices[v4.y]._m00_m10_m20_m30 * r1.yyyy;
    r6.xyzw = g_vsBone.m_boneMatrices[v4.y]._m01_m11_m21_m31 * r1.yyyy;
    r7.xyzw = g_vsBone.m_boneMatrices[v4.y]._m02_m12_m22_m32 * r1.yyyy;
    r3.xyzw = r5.xyzw + r3.xyzw;
    r4.xyzw = r6.xyzw + r4.xyzw;
    r2.xyzw = r7.xyzw + r2.xyzw;
    r1.x = (int)v4.z * 3;
    r5.xyzw = g_vsBone.m_boneMatrices[v4.y]._m00_m10_m20_m30 * r1.zzzz;
    r6.xyzw = g_vsBone.m_boneMatrices[v4.y]._m01_m11_m21_m31 * r1.zzzz;
    r7.xyzw = g_vsBone.m_boneMatrices[v4.y]._m02_m12_m22_m32 * r1.zzzz;
    r3.xyzw = r5.xyzw + r3.xyzw;
    r4.xyzw = r6.xyzw + r4.xyzw;
    r2.xyzw = r7.xyzw + r2.xyzw;
    r1.x = (int)v4.w * 3;
    r5.xyzw = g_vsBone.m_boneMatrices[v4.y]._m00_m10_m20_m30 * r1.wwww;
    r6.xyzw = g_vsBone.m_boneMatrices[v4.y]._m01_m11_m21_m31 * r1.wwww;
    r1.xyzw = g_vsBone.m_boneMatrices[v4.y]._m02_m12_m22_m32 * r1.wwww;
    r3.xyzw = r5.xyzw + r3.xyzw;
    r4.xyzw = r6.xyzw + r4.xyzw;
    r1.xyzw = r2.xyzw + r1.xyzw;
    r0.x = dot(v0.xyzw, r3.xyzw);
    r0.y = dot(v0.xyzw, r4.xyzw);
    r0.z = dot(v0.xyzw, r1.xyzw);
    r0.xyz = r0.xyz;
    r0.xyz = r0.xyz;
  }
  r0.w = v0.w;
  r1.x = dot(r0.xyzw, g_vsObject.m_viewWorld._m00_m10_m20_m30);
  r1.y = dot(r0.xyzw, g_vsObject.m_viewWorld._m01_m11_m21_m31);
  r1.z = dot(r0.xyzw, g_vsObject.m_viewWorld._m02_m12_m22_m32);
  r1.w = dot(r0.xyzw, g_vsObject.m_viewWorld._m03_m13_m23_m33);
  r0.x = dot(r1.xyzw, g_vsScene.m_projection._m00_m10_m20_m30);
  r0.y = dot(r1.xyzw, g_vsScene.m_projection._m01_m11_m21_m31);
  r0.z = dot(r1.xyzw, g_vsScene.m_projection._m02_m12_m22_m32);
  r0.w = dot(r1.xyzw, g_vsScene.m_projection._m03_m13_m23_m33);
  // This light path receives an unjittered cVSScene projection while the main
  // scene and depth are jittered. Apply the exact native-hook offset to final
  // clip XY so its raster coverage matches the scene. The injected value is
  // zero when TAA or projection jitter is disabled.
  r0.xy += float2(2.f * TAA_JITTER_UV.x, -2.f * TAA_JITTER_UV.y) * r0.w;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r1.x = r0.w;
  r1.x = r1.x;
  r1.x = g_vsScene.m_fogParam[0].x * r1.x;
  r1.x = g_vsScene.m_fogParam[0].y + r1.x;
  r1.x = r1.x;
  o0.xyzw = r0.xyzw;
  o2.x = r1.x;
  o2.y = r0.w;
  o1.xy = v6.xy;
  o2.z = g_vsObject.m_useWeightCount.w;
  return;
}
