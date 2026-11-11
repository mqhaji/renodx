#include "../shadows/screen_space_shadows.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:33 2026

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
SamplerComparisonState g_samplerComparisonLess_Linear_Clmap_s : register(s15);
Texture2D<float4> g_NormalTexture : register(t0);
Texture2D<float4> g_SpecularTexture : register(t1);
Texture2D<float4> g_DepthTexture : register(t2);
Texture2D<float4> g_ShadowTexture : register(t3);
Texture2D<float4> g_MaterialTexture : register(t10);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0 : SV_Position0,
    float4 v1 : TEXCOORD0,
    float4 v2 : TEXCOORD1,
    out float4 o0 : SV_Target0,
    out float4 o1 : SV_Target1) {
  float4 r0, r1, r2, r3, r4, r5, r6, r7, r8;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = float4(-0.5, -0.5, -0.5, -0.5) + v0.xyxy;
  r1.xyz = v2.xyw;
  r1.xyz = r1.xyz;
  r1.xy = r1.xy / r1.zz;
  r1.xy = r1.xy;
  r1.xy = r1.xy;
  r1.xy = r1.xy;
  r2.xyz = g_NormalTexture.Sample(g_samplerPoint_Wrap_s, r1.xy).xyz;
  r2.xyz = r2.xyz;
  r1.zw = g_SpecularTexture.Sample(g_samplerPoint_Wrap_s, r1.xy).wz;
  r1.zw = r1.zw;
  r1.x = g_DepthTexture.Sample(g_samplerPoint_Wrap_s, r1.xy).x;
  r1.x = r1.x;
  r2.xyz = r2.xyz;
  r2.xy = float2(2, 2) * r2.xy;
  r2.xy = float2(-1, -1) + r2.xy;
  r1.y = r2.z * r2.z;
  r1.y = 2 * r1.y;
  r3.z = -1 + r1.y;
  r1.y = r3.z * r3.z;
  r1.y = -r1.y;
  r1.y = 1 + r1.y;
  r2.zw = r2.xy * r1.yy;
  r2.x = dot(r2.xy, r2.xy);
  r1.y = r2.x * r1.y;
  r1.y = 1.00000001e-07 + r1.y;
  r1.y = rsqrt(r1.y);
  r3.xy = r2.zw * r1.yy;
  r3.xy = r3.xy;
  r3.z = r3.z;
  r3.xyz = r3.xyz;
  r2.x = r1.w;
  r1.z = r1.z;
  r3.xyz = r3.xyz;
  r2.x = r2.x;
  r1.z = r1.z;
  r1.x = r1.x;
  r1.x = r1.x;
  r4.xyz = v1.xyw;
  r1.x = r1.x;
  r4.xyz = r4.xyz;
  r1.yw = r4.xy / r4.zz;
  r2.zw = -g_psScene.m_cameraCenterOffset.xy;
  r1.yw = r2.zw + r1.yw;
  r1.x = r1.x;
  r4.xyzw = g_psScene.m_projectionParam.xyzw;
  r1.yw = r4.xy * r1.yw;
  r2.z = -r4.w;
  r1.x = r2.z + r1.x;
  r4.z = r4.z / r1.x;
  r5.z = r4.z;
  r4.xy = r5.zz * r1.yw;
  r5.xy = r4.xy;
  r4.w = 1;
  r5.xy = r5.xy;
  r5.z = r5.z;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz;
  r1.xyw = -r5.xyz;
  r1.xyw = g_psLight.m_lightParams[3].xyz + r1.xyw;
  r2.z = dot(r1.xyw, r1.xyw);
  r2.w = rsqrt(r2.z);
  r1.xyw = r2.www * r1.xyw;
  r3.w = g_psMaterial.m_materials[7].w;
  r2.w = r2.w * r2.w;
  r2.z = g_psLight.m_lightParams[6].w * r2.z;
  r2.z = -r2.z;
  r2.z = r2.w + r2.z;
  r2.z = min(r3.w, r2.z);
  r2.z = max(0, r2.z);
  r5.xyz = g_psLight.m_lightParams[7].xyz;
  r6.xyz = g_psLight.m_lightParams[5].xyz;
  r1.xyw = r1.xyw;
  r5.x = r5.x;
  r5.y = r5.y;
  r7.xyz = -r1.xyw;
  r2.w = dot(r6.xyz, r7.xyz);
  r5.z = r5.z;
  r3.w = -r5.x;
  r2.w = r3.w + r2.w;
  r2.w = r2.w * r5.y;
  r2.w = max(0, r2.w);
  r2.w = min(1, r2.w);
  r2.w = log2(r2.w);
  r2.w = r5.z * r2.w;
  r5.xyz = exp2(r2.www);
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz;
  r7.xyz = g_psLight.m_lightParams[4].xyz * r2.zzz;
  r5.xyz = r7.xyz * r5.xyz;
  r5.xyz = g_psScene.m_exposure.zzz * r5.xyz;
  r1.xyw = r1.xyw;
  r5.xyz = r5.xyz;
  r0.xyzw = r0.xyzw;
  r1.xyw = r1.xyw;
  r0.xyzw = r0.xyzw;
  r1.xyw = r1.xyw;
  r7.x = dot(r4.xyzw, g_psScene.m_shadowProjection._m00_m10_m20_m30);
  r7.y = dot(r4.xyzw, g_psScene.m_shadowProjection._m01_m11_m21_m31);
  r7.z = dot(r4.xyzw, g_psScene.m_shadowProjection._m02_m12_m22_m32);
  r7.w = dot(r4.xyzw, g_psScene.m_shadowProjection._m03_m13_m23_m33);
  const float4 shadow_projected = r7;
  const float4 shadow_projected_dx = ddx(shadow_projected);
  const float4 shadow_projected_dy = ddy(shadow_projected);
  r7.xyzw = r7.xyzw;
  r7.xyzw = r7.xyzw;
  r2.z = g_psMaterial.m_materials[7].y;
  r2.w = g_psMaterial.m_materials[7].z;
  r2.z = -r2.z;
  r2.z = r7.w + r2.z;
  r2.z = r2.z * r2.w;
  r2.z = max(0, r2.z);
  r2.z = min(1, r2.z);
  r4.xy = g_psMaterial.m_materials[6].xy;
  r1.xyw = r1.xyw;
  r4.x = r4.x;
  r4.y = r4.y;
  r8.xyz = -r1.xyw;
  r2.w = dot(r6.xyz, r8.xyz);
  r3.w = -r4.x;
  r2.w = r3.w + r2.w;
  r2.w = r2.w * r4.y;
  r2.w = max(0, r2.w);
  r2.w = min(1, r2.w);
  r2.w = r2.w * r2.w;
  r2.w = r2.w;
  r2.w = r2.w;
  r7.xyzw = r7.xyzw;
  r0.xyzw = r0.xyzw;
  r2.w = r2.w;
  r7.xyzw = r7.xyzw;
  r0.xyzw = r0.xyzw;
  r2.w = r2.w;
  r7.xyzw = r7.xyzw;
  r0.xyzw = r0.xyzw;
  r2.w = r2.w;
  r0.xyzw = r0.xyzw / float4(2, 2, 2, 2);
  r0.xyzw = frac(r0.xyzw);
  r0.xyzw = cmp(r0.xyzw >= float4(0.300000012, 0.300000012, 0.300000012, 0.300000012));
  r0.xyzw = r0.xyzw ? float4(1, 1, 1, 1) : float4(0, 0, 0, 0);
  r0.xyzw = float4(2, 2, 2, 2) * r0.xyzw;
  r0.xyzw = float4(-1, -1, -1, -1) + r0.xyzw;
  r0.xyzw = float4(0.125, 0.125, 0.125, 0.125) * r0.xyzw;
  r0.xyzw = float4(0.5, 0.5, -0.5, -0.5) + r0.xyzw;
  r4.xyzw = r7.wwww / g_psScene.m_shadowMapResolutions.yyyy;
  r0.xyzw = r4.xyzw * r0.xyzw;
  r7.xyzw = r7.xyzw;
  r4.xy = r7.xy + r0.xy;
  r4.xy = r4.xy;
  r4.zw = r7.zw;
  r6.xyz = r4.xyz / r4.www;
  const BendShadow contact_shadow = ReadBendShadow(v0.xy, 1u, g_DepthTexture);
  const bool map_debug = CUSTOM_ENHANCED_SHADOWS_ENABLED && CUSTOM_SHADOW_MAP_DEBUG_VIEW >= 0.5
                         && uint(CUSTOM_SHADOW_DEBUG_LIGHT) == 1u;
  const bool shadow_required = contact_shadow.visibility > 0.0 || contact_shadow.diagnostic || map_debug;
  PcssResult filtered_shadow = (PcssResult)0;
  [branch]
  if (CUSTOM_ENHANCED_SHADOWS_ENABLED && shadow_required) {
    filtered_shadow = FilterSpotShadow(g_ShadowTexture, g_samplerComparisonLess_Linear_Clmap_s,
                                       shadow_projected, shadow_projected_dx, shadow_projected_dy,
                                       ReadPcssProjection(g_psScene.m_shadowProjection, (float4x4)0, false), PcssNoise(v0.xy));
  }
  // The native comparisons return occlusion. Preserve the native cone,
  // distance fade, projected-light mask, and shadow-strength operations below.
  [branch]
  if (!shadow_required) {
    r0.x = 1.0;
  }
  else if (filtered_shadow.filtered) {
    r0.x = 1.0 - filtered_shadow.visibility;
  }
  else {
    r3.w = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r6.xy, r6.z).x;
    r3.w = 0 + r3.w;
    r6.xy = r7.xy + r0.zy;
    r6.xy = r6.xy;
    r6.zw = r4.zw;
    r6.xyz = r6.xyz / r6.www;
    r0.y = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r6.xy, r6.z).x;
    r0.y = r3.w + r0.y;
    r6.xy = r7.xy + r0.xw;
    r6.xy = r6.xy;
    r6.zw = r4.zw;
    r6.xyz = r6.xyz / r6.www;
    r0.x = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r6.xy, r6.z).x;
    r0.x = r0.y + r0.x;
    r4.xy = r7.xy + r0.zw;
    r4.xy = r4.xy;
    r0.yzw = r4.xyz / r4.www;
    r0.y = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r0.yz, r0.w).x;
    r0.x = r0.x + r0.y;
    r0.x = 0.25 * r0.x;
  }
  filtered_shadow.visibility = 1.0 - r0.x;
  r0.x = r0.x * r2.w;
  r0.x = -r0.x;
  r0.x = 1 + r0.x;
  r0.x = r0.x * r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.y = -r0.x;
  r0.y = 1 + r0.y;
  r0.y = r2.z * r0.y;
  r0.x = r0.x + r0.y;
  r0.x = r0.x;
  r3.xyz = r3.xyz;
  r5.xyz = r5.xyz;
  r2.x = r2.x;
  r1.z = r1.z;
  r3.xyz = r3.xyz;
  r5.xyz = r5.xyz;
  r2.x = r2.x;
  r1.z = r1.z;
  r3.xyz = r3.xyz;
  r0.y = dot(r1.xyw, r3.xyz);
  r2.x = r2.x;
  r2.y = 0.25;
  r0.z = g_MaterialTexture.Sample(g_samplerPoint_Clamp_s, r2.xy).w;
  r0.z = r0.z;
  r0.z = r0.z;
  r0.w = r0.y * r0.y;
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r0.w = r0.w * r1.z;
  r0.y = r0.y + r0.w;
  r0.z = r1.z * r0.z;
  r0.y = max(r0.y, r0.z);
  r0.y = max(0, r0.y);
  r0.y = min(1, r0.y);
  r0.y = 1 * r0.y;
  r0.y = 1 * r0.y;
  r0.yzw = r0.yyy * r5.xyz;
  r0.yzw = r0.yzw;
  r0.yzw = r0.yzw;
  r0.yzw = r0.yzw;
  r0.x = r0.x;
  r0.yzw = r0.yzw;
  r0.x = r0.x;
  r0.x = -1 + r0.x;
  r0.x = g_psLight.m_lightParams[8].z * r0.x;
  r0.x = 1 + r0.x;
  r0.xyz = r0.yzw * r0.xxx;
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
  // Preserve native output alpha and any diffuse-only material behavior.
  o0.rgb *= contact_shadow.visibility;
  o1.rgb *= contact_shadow.visibility;
  ApplyBendDebug(contact_shadow, o0.rgb, o1.rgb);
  if (map_debug) {
    o0.rgb = PcssDebug(filtered_shadow, uint(CUSTOM_SHADOW_MAP_DEBUG_VIEW), PCSS_MAX_RADIUS);
    o1.rgb = 0.0;
  }
  return;
}
