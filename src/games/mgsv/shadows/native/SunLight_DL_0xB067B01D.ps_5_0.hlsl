#include "../screen_space_shadows.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:31 2026

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

cbuffer cPSLight : register(b3) {
  struct
  {
    float4 m_lightParams[11];
  } g_psLight : packoffset(c0);
}

SamplerState g_samplerPoint_Wrap_s : register(s8);
SamplerState g_samplerPoint_Clamp_s : register(s9);
SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> g_NormalTexture : register(t0);
Texture2D<float4> g_SpecularTexture : register(t1);
Texture2D<float4> g_DepthTexture : register(t2);
Texture3D<float4> g_lut2 : register(t8);
Texture2D<float4> g_lut1 : register(t9);
Texture2D<float4> g_MaterialTexture : register(t10);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0 : SV_Position0,
    float4 v1 : TEXCOORD0,
    float4 v2 : TEXCOORD1,
    out float4 o0 : SV_Target0,
    out float4 o1 : SV_Target1,
    out float4 o2 : SV_Target2) {
  float4 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyz = v2.xyw;
  r0.xyz = r0.xyz;
  r0.xy = r0.xy / r0.zz;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r1.xyz = g_NormalTexture.Sample(g_samplerPoint_Wrap_s, r0.xy).xyz;
  r1.xyz = r1.xyz;
  r2.xyzw = g_SpecularTexture.Sample(g_samplerPoint_Wrap_s, r0.xy).yxzw;
  r0.x = g_DepthTexture.Sample(g_samplerPoint_Wrap_s, r0.xy).x;
  r0.x = r0.x;
  r1.xyz = r1.xyz;
  r0.yz = float2(2, 2) * r1.xy;
  r0.yz = float2(-1, -1) + r0.yz;
  r0.w = r1.z * r1.z;
  r0.w = 2 * r0.w;
  r1.z = -1 + r0.w;
  r0.w = r1.z * r1.z;
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r3.xy = r0.yz * r0.ww;
  r0.y = dot(r0.yz, r0.yz);
  r0.y = r0.w * r0.y;
  r0.y = 1.00000001e-07 + r0.y;
  r0.y = rsqrt(r0.y);
  r1.xy = r3.xy * r0.yy;
  r1.xy = r1.xy;
  r1.z = r1.z;
  r1.xyz = r1.xyz;
  r3.y = r2.y;
  r2.x = r2.x;
  r2.z = r2.z;
  r2.w = r2.w;
  r1.xyz = r1.xyz;
  r2.z = r2.z;
  r2.x = r2.x;
  r3.y = r3.y;
  r2.w = r2.w;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.yzw = v1.xyw;
  r0.x = r0.x;
  r0.yzw = r0.yzw;
  r0.yz = r0.yz / r0.ww;
  r4.xy = -g_psScene.m_cameraCenterOffset.xy;
  r0.yz = r4.xy + r0.yz;
  r0.x = r0.x;
  r4.xyzw = g_psScene.m_projectionParam.xyzw;
  r0.yz = r4.xy * r0.yz;
  r0.w = -r4.w;
  r0.x = r0.x + r0.w;
  r4.z = r4.z / r0.x;
  r4.xy = r4.zz * r0.yz;
  r4.xy = r4.xy;
  r4.z = r4.z;
  r4.xyz = r4.xyz;
  r0.x = dot(r4.xyz, r4.xyz);
  r0.x = rsqrt(r0.x);
  r0.xyz = r4.xyz * r0.xxx;
  r0.xyz = r0.xyz;
  r4.xyz = g_psLight.m_lightParams[0].xyz;
  r5.xyz = g_psLight.m_lightParams[1].xyz * g_psScene.m_exposure.zzz;
  r5.w = g_psLight.m_lightParams[1].w;
  r4.xyz = r4.xyz;
  r5.xyzw = r5.xyzw;
  r1.xyz = r1.xyz;
  r4.xyz = r4.xyz;
  r5.xyzw = r5.xyzw;
  r0.xyz = r0.xyz;
  r2.x = r2.x;
  r2.z = r2.z;
  r3.y = r3.y;
  r2.w = r2.w;
  r1.xyz = r1.xyz;
  r4.xyz = r4.xyz;
  r5.xyzw = r5.xyzw;
  r0.xyz = r0.xyz;
  r2.x = r2.x;
  r2.z = r2.z;
  r3.y = r3.y;
  r2.w = r2.w;
  r4.xyz = r4.xyz;
  r1.xyz = r1.xyz;
  r6.xyz = r4.xyz;
  r0.w = dot(r6.xyz, r1.xyz);
  r0.xyz = -r0.xyz;
  r1.w = dot(r0.xyz, r1.xyz);
  r1.w = r1.w + r1.w;
  r1.w = -r1.w;
  r7.xyz = r1.xyz * r1.www;
  r7.xyz = r7.xyz + r0.xyz;
  r7.xyz = -r7.xyz;
  r8.xyz = -r6.xyz;
  r8.xyz = r8.xyz + r7.xyz;
  r1.w = dot(r8.xyz, r8.xyz);
  r1.w = sqrt(r1.w);
  r1.w = r5.w / r1.w;
  r1.w = max(0, r1.w);
  r8.xyz = min(float3(1, 1, 1), r1.www);
  r9.xyz = -r4.xyz;
  r7.xyz = r9.xyz + r7.xyz;
  r7.xyz = r8.xyz * r7.xyz;
  r4.xyz = r7.xyz + r4.xyz;
  r1.w = dot(r4.xyz, r4.xyz);
  r1.w = rsqrt(r1.w);
  r4.xyz = r4.xyz * r1.www;
  r0.xyz = r4.xyz + r0.xyz;
  r1.w = dot(r0.xyz, r0.xyz);
  r1.w = rsqrt(r1.w);
  r0.xyz = r1.www * r0.xyz;
  r1.x = dot(r0.xyz, r1.xyz);
  r1.x = max(0, r1.x);
  r1.x = min(1, r1.x);
  r0.x = dot(r0.xyz, r6.xyz);
  r2.z = r2.z;
  r2.y = 0.25;
  r4.xyzw = g_MaterialTexture.Sample(g_samplerPoint_Clamp_s, r2.zy).xyzw;
  r2.y = 0.75;
  r3.xz = g_MaterialTexture.Sample(g_samplerPoint_Clamp_s, r2.zy).xw;
  r3.xz = r3.xz;
  r4.xyz = r4.xyz;
  r4.w = r4.w;
  r0.y = r3.x;
  r3.z = r3.z;
  r0.x = r0.x;
  r0.y = r0.y;
  r0.x = g_lut1.Sample(g_samplerLinear_Clamp_s, r0.xy).x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.y = r0.w * r0.w;
  r0.y = -r0.y;
  r0.y = 1 + r0.y;
  r0.y = r0.y * r2.w;
  r0.y = r0.w + r0.y;
  r0.z = r4.w * r2.w;
  r0.y = max(r0.y, r0.z);
  r0.y = max(0, r0.y);
  r0.y = min(1, r0.y);
  r1.yzw = r4.xyz * r2.xxx;
  r1.x = r1.x;
  r3.y = r3.y;
  r3.z = r3.z;
  r0.z = max(0, r1.x);
  r3.x = min(1, r0.z);
  r3.y = r3.y;
  r3.z = r3.z;
  r2.xyz = r3.xyz * r3.xyz;
  r2.xyz = float3(0.984375, 0.9375, 0.9375) * r2.xyz;
  r2.xyz = float3(0.0078125, 0.03125, 0.03125) + r2.xyz;
  r2.xy = g_lut2.Sample(g_samplerLinear_Clamp_s, r2.xyz).xy;
  r2.xy = r2.xy;
  r2.xy = r2.xy * r2.xy;
  r0.z = r2.x / r2.y;
  r0.x = r0.x * r0.z;
  r0.w = r0.w;
  r0.z = 4 * r0.w;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.x = r0.x * r0.z;
  r1.xyz = r1.yzw * r5.xyz;
  r0.xzw = r1.xyz * r0.xxx;
  r0.y = 1 * r0.y;
  r0.y = 1 * r0.y;
  r1.xyz = r0.yyy * r5.xyz;
  r0.xzw = r0.xzw;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r0.xzw = r0.xzw;
  r1.xyz = r1.xyz;
  r0.xzw = r0.xzw;
  r1.xyz = r1.xyz;
  r0.xzw = r0.xzw;
  r1.xyz = float3(1, 1, 1) * r1.xyz;
  r1.xyz = float3(1, 1, 1) * r1.xyz;
  r0.xyz = float3(1, 1, 1) * r0.xzw;
  r0.xyz = float3(1, 1, 1) * r0.xyz;
  r1.xyz = r1.xyz;
  r0.xyz = r0.xyz;
  r1.xyz = r1.xyz;
  r0.xyz = r0.xyz;
  r1.xyz = r1.xyz;
  r0.xyz = r0.xyz;
  r1.xyz = float3(-0, -0, -0) + r1.xyz;
  r0.xyz = float3(0, 0, 0) + r0.xyz;
  r1.xyz = r1.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = 1;
  r0.xyz = r0.xyz;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.w = 1;
  r1.xyz = r1.xyz;
  r1.xyzw = r1.xyzw;
  r1.xyzw = r1.xyzw;
  o0.xyzw = r1.xyzw;
  o1.xyzw = r0.xyzw;
  const BendShadow contact_shadow = ReadBendShadow(v0.xy, 0u, g_DepthTexture);
  // Preserve native output alpha and any diffuse-only material behavior.
  o0.rgb *= contact_shadow.visibility;
  o1.rgb *= contact_shadow.visibility;
  ApplyBendDebug(contact_shadow, o0.rgb, o1.rgb);
  o2 = float4(0.0, 0.0, 0.0, 1.0);
  return;
}