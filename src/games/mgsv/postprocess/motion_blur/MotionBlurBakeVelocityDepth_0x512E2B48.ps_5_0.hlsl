// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:25 2026

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
  } g_psScene : packoffset(c0);
}

cbuffer cPSObject : register(b5) {
  struct
  {
    float4x4 m_viewWorld;
    float4x4 m_world;
    float4 m_useWeightCount;
    float4 m_localParam[4];
  } g_psObject : packoffset(c0);
}

cbuffer cPSSystem : register(b0) {
  struct
  {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem : packoffset(c0);
}

SamplerState g_samplerPoint_Clamp_s : register(s9);
SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inVelocity : register(t2);
Texture2D<float4> inDepth : register(t3);
Texture2D<float4> inTiledVelocity : register(t4);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0 : SV_Position0,
    float2 v1 : TEXCOORD0,
    out float4 o0 : SV_Target0) {
  float4 r0, r1, r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = v1.xy;
  r0.xy = r0.xy;
  r0.xy = g_psSystem.m_renderInfo.xy * r0.xy;
  r0.xy = g_psSystem.m_renderBuffer.zw * r0.xy;
  r0.z = g_psObject.m_localParam[0].w;
  r0.zw = float2(64, 64) * r0.zz;
  r0.zw = g_psObject.m_localParam[0].xy * r0.zw;
  r1.xy = inVelocity.Sample(g_samplerPoint_Clamp_s, r0.xy).zw;
  r1.xy = float2(2, 2) * r1.xy;
  r1.xy = float2(-1, -1) + r1.xy;
  // Post-processing receives output-sized depth. Keep b2's native scene
  // dimensions so both direct and tiled motion convert into output pixels.
  uint depth_width, depth_height;
  inDepth.GetDimensions(depth_width, depth_height);
  const float2 motion_scale = float2(depth_width, depth_height) / g_psScene.m_viewportSize.xy;
  if (any(motion_scale != 1.f)) r1.xy *= motion_scale;
  r0.zw = r1.xy * r0.zw;
  r1.z = inDepth.Sample(g_samplerPoint_Clamp_s, r0.xy).x;
  r1.z = r1.z;
  r2.xy = g_psScene.m_projectionParam.zw;
  r1.w = -r2.y;
  r1.z = r1.z + r1.w;
  r1.z = r2.x / r1.z;
  r1.w = -r1.z;
  r1.z = max(r1.z, r1.w);
  r1.z = r1.z / 32;
  r1.z = max(0, r1.z);
  r1.z = min(1, r1.z);
  r2.y = sqrt(r1.z);
  r0.z = dot(r0.zw, r0.zw);
  r0.z = 1.00000001e-07 + r0.z;
  r0.z = sqrt(r0.z);
  r2.x = r0.z / 64;
  r2.x = r2.x;
  r2.y = r2.y;
  r0.xy = min(g_psObject.m_localParam[2].zw, r0.xy);
  r0.xy = inTiledVelocity.Sample(g_samplerLinear_Clamp_s, r0.xy).zw;
  r0.xy = float2(2, 2) * r0.xy;
  r0.xy = float2(-1, -1) + r0.xy;
  if (any(motion_scale != 1.f)) r0.xy *= motion_scale;
  r0.z = dot(r0.xy, r0.xy);
  r0.w = dot(r1.xy, r1.xy);
  r0.z = cmp(r0.z >= r0.w);
  r0.xy = r0.zz ? r0.xy : r1.xy;
  r0.xy = float2(0.5, 0.5) * r0.xy;
  r2.zw = float2(0.5, 0.5) + r0.xy;
  r2.xy = r2.xy;
  r2.zw = r2.zw;
  o0.xyzw = r2.xyzw;
  return;
}
