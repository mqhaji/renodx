#include "../shadows/screen_space_shadows.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:28 2026

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

cbuffer cPSMaterial : register(b4) {
  struct
  {
    float4 m_materials[8];
  } g_psMaterial : packoffset(c0);
}

SamplerState g_samplerPoint_Wrap_s : register(s8);
SamplerState g_samplerPoint_Clamp_s : register(s9);
Texture2D<float4> g_NormalTexture : register(t0);
Texture2D<float4> g_SpecularTexture : register(t1);
Texture2D<float4> g_DepthTexture : register(t2);
Texture2D<float4> g_MaterialTexture : register(t10);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0 : SV_Position0,
    float4 v1 : TEXCOORD0,
    float4 v2 : TEXCOORD1,
    out float4 o0 : SV_Target0,
    out float4 o1 : SV_Target1) {
  float4 r0, r1, r2, r3, r4, r5;
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
  r0.zw = g_SpecularTexture.Sample(g_samplerPoint_Wrap_s, r0.xy).wz;
  r0.zw = r0.zw;
  r0.x = g_DepthTexture.Sample(g_samplerPoint_Wrap_s, r0.xy).x;
  r0.x = r0.x;
  r1.xyz = r1.xyz;
  r1.xy = float2(2, 2) * r1.xy;
  r1.xy = float2(-1, -1) + r1.xy;
  r0.y = r1.z * r1.z;
  r0.y = 2 * r0.y;
  r2.z = -1 + r0.y;
  r0.y = r2.z * r2.z;
  r0.y = -r0.y;
  r0.y = 1 + r0.y;
  r1.zw = r1.xy * r0.yy;
  r1.x = dot(r1.xy, r1.xy);
  r0.y = r1.x * r0.y;
  r0.y = 1.00000001e-07 + r0.y;
  r0.y = rsqrt(r0.y);
  r2.xy = r1.zw * r0.yy;
  r2.xy = r2.xy;
  r2.z = r2.z;
  r2.xyz = r2.xyz;
  r1.x = r0.w;
  r0.z = r0.z;
  r2.xyz = r2.xyz;
  r1.x = r1.x;
  r0.z = r0.z;
  r0.x = r0.x;
  r0.x = r0.x;
  r3.xyz = v1.xyw;
  r0.x = r0.x;
  r3.xyz = r3.xyz;
  r0.yw = r3.xy / r3.zz;
  r1.zw = -g_psScene.m_cameraCenterOffset.xy;
  r0.yw = r1.zw + r0.yw;
  r0.x = r0.x;
  r3.xyzw = g_psScene.m_projectionParam.xyzw;
  r0.yw = r3.xy * r0.yw;
  r1.z = -r3.w;
  r0.x = r1.z + r0.x;
  r3.z = r3.z / r0.x;
  r3.xy = r3.zz * r0.yw;
  r3.xy = r3.xy;
  r3.z = r3.z;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r0.xyw = -r3.xyz;
  r0.xyw = g_psLight.m_lightParams[3].xyz + r0.xyw;
  r1.z = dot(r0.xyw, r0.xyw);
  r1.w = rsqrt(r1.z);
  r0.xyw = r1.www * r0.xyw;
  r2.w = g_psMaterial.m_materials[7].w;
  r1.w = r1.w * r1.w;
  r1.z = g_psLight.m_lightParams[6].w * r1.z;
  r1.z = -r1.z;
  r1.z = r1.w + r1.z;
  r1.z = min(r2.w, r1.z);
  r1.z = max(0, r1.z);
  r3.xyz = g_psLight.m_lightParams[7].xyz;
  r4.xyz = g_psLight.m_lightParams[5].xyz;
  r0.xyw = r0.xyw;
  r3.x = r3.x;
  r3.y = r3.y;
  r5.xyz = -r0.xyw;
  r1.w = dot(r4.xyz, r5.xyz);
  r3.z = r3.z;
  r2.w = -r3.x;
  r1.w = r2.w + r1.w;
  r1.w = r1.w * r3.y;
  r1.w = max(0, r1.w);
  r1.w = min(1, r1.w);
  r1.w = log2(r1.w);
  r1.w = r3.z * r1.w;
  r3.xyz = exp2(r1.www);
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r4.xyz = g_psLight.m_lightParams[4].xyz * r1.zzz;
  r3.xyz = r4.xyz * r3.xyz;
  r3.xyz = g_psScene.m_exposure.zzz * r3.xyz;
  r0.xyw = r0.xyw;
  r3.xyz = r3.xyz;
  r2.xyz = r2.xyz;
  r0.xyw = r0.xyw;
  r3.xyz = r3.xyz;
  r1.x = r1.x;
  r0.z = r0.z;
  r2.xyz = r2.xyz;
  r0.xyw = r0.xyw;
  r3.xyz = r3.xyz;
  r1.x = r1.x;
  r0.z = r0.z;
  r0.xyw = r0.xyw;
  r2.xyz = r2.xyz;
  r0.x = dot(r0.xyw, r2.xyz);
  r1.x = r1.x;
  r1.y = 0.25;
  r0.y = g_MaterialTexture.Sample(g_samplerPoint_Clamp_s, r1.xy).w;
  r0.y = r0.y;
  r0.y = r0.y;
  r0.w = r0.x * r0.x;
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r0.w = r0.w * r0.z;
  r0.x = r0.x + r0.w;
  r0.y = r0.z * r0.y;
  r0.x = max(r0.x, r0.y);
  r0.x = max(0, r0.x);
  r0.x = min(1, r0.x);
  r0.x = 1 * r0.x;
  r0.x = 1 * r0.x;
  r0.xyz = r0.xxx * r3.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = float3(1, 1, 1) * r0.xyz;
  r0.w = g_psLight.m_lightParams[8].y;
  r0.xyz = r0.xyz * r0.www;
  r0.xyz = float3(1, 1, 1) * r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = 1;
  r0.xyz = r0.xyz;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  o0.xyzw = r0.xyzw;
  o1.xyzw = float4(0, 0, 0, 1);
  const BendShadow contact_shadow = ReadBendShadow(v0.xy, 1u, g_DepthTexture);
  // Preserve native output alpha and any diffuse-only material behavior.
  o0.rgb *= contact_shadow.visibility;
  o1.rgb *= contact_shadow.visibility;
  ApplyBendDebug(contact_shadow, o0.rgb, o1.rgb);
  return;
}