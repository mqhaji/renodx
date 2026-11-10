#include "../screen_space_shadows.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:27 2026

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
SamplerState g_samplerLinear_Clamp_s : register(s11);
SamplerComparisonState g_samplerComparisonLess_Linear_Clmap_s : register(s15);
Texture2D<float4> g_NormalTexture : register(t0);
Texture2D<float4> g_SpecularTexture : register(t1);
Texture2D<float4> g_DepthTexture : register(t2);
Texture2D<float4> g_ShadowTexture : register(t3);
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
    out float4 o1 : SV_Target1) {
  float4 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10;
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
  r3.xyzw = g_SpecularTexture.Sample(g_samplerPoint_Wrap_s, r1.xy).yxzw;
  r1.x = g_DepthTexture.Sample(g_samplerPoint_Wrap_s, r1.xy).x;
  r1.x = r1.x;
  r2.xyz = r2.xyz;
  r1.yz = float2(2, 2) * r2.xy;
  r1.yz = float2(-1, -1) + r1.yz;
  r1.w = r2.z * r2.z;
  r1.w = 2 * r1.w;
  r2.z = -1 + r1.w;
  r1.w = r2.z * r2.z;
  r1.w = -r1.w;
  r1.w = 1 + r1.w;
  r4.xy = r1.yz * r1.ww;
  r1.y = dot(r1.yz, r1.yz);
  r1.y = r1.w * r1.y;
  r1.y = 1.00000001e-07 + r1.y;
  r1.y = rsqrt(r1.y);
  r2.xy = r4.xy * r1.yy;
  r2.xy = r2.xy;
  r2.z = r2.z;
  r2.xyz = r2.xyz;
  r4.y = r3.y;
  r3.x = r3.x;
  r3.z = r3.z;
  r3.w = r3.w;
  r2.xyz = r2.xyz;
  r3.z = r3.z;
  r3.x = r3.x;
  r4.y = r4.y;
  r3.w = r3.w;
  r1.x = r1.x;
  r1.x = r1.x;
  r1.yzw = v1.xyw;
  r1.x = r1.x;
  r1.yzw = r1.yzw;
  r1.yz = r1.yz / r1.ww;
  r5.xy = -g_psScene.m_cameraCenterOffset.xy;
  r1.yz = r5.xy + r1.yz;
  r1.x = r1.x;
  r5.xyzw = g_psScene.m_projectionParam.xyzw;
  r1.yz = r5.xy * r1.yz;
  r1.w = -r5.w;
  r1.x = r1.x + r1.w;
  r5.x = r5.z / r1.x;
  r1.xy = r5.xx * r1.yz;
  r5.yz = r1.xy;
  r1.w = 1;
  r5.yz = r5.yz;
  r5.x = r5.x;
  r5.xyz = r5.xyz;
  r2.w = dot(r5.xyz, r5.xyz);
  r2.w = rsqrt(r2.w);
  r6.xyz = r5.yzx * r2.www;
  r5.xyz = r5.xyz;
  r6.xyz = r6.xyz;
  r5.xyz = r5.xyz;
  r5.yzw = r5.yzx;
  r5.yzw = -r5.yzw;
  r5.yzw = g_psLight.m_lightParams[3].xyz + r5.yzw;
  r2.w = g_psMaterial.m_materials[7].x;
  r4.w = dot(r5.yzw, r5.yzw);
  r4.w = sqrt(r4.w);
  r2.w = max(r4.w, r2.w);
  r6.w = g_psLight.m_lightParams[6].x;
  r5.yzw = r5.yzw / r4.www;
  r2.w = r6.w + r2.w;
  r4.w = r2.w * r2.w;
  r6.w = r4.w * r4.w;
  r6.w = r6.w * r6.w;
  r6.w = r6.w * r6.w;
  r6.w = min(3.40282347e+38, r6.w);
  r4.w = 1 / r4.w;
  r6.w = g_psLight.m_lightParams[6].z * r6.w;
  r6.w = -r6.w;
  r6.w = 1 + r6.w;
  r4.w = r6.w * r4.w;
  r4.w = max(0, r4.w);
  r5.yzw = r5.yzw;
  r7.xyz = g_psLight.m_lightParams[4].xyz * r4.www;
  r7.xyz = g_psScene.m_exposure.zzz * r7.xyz;
  r4.w = g_psLight.m_lightParams[4].w;
  r4.w = r4.w;
  r2.w = r2.w;
  r4.w = 1 * r4.w;
  r2.w = r4.w / r2.w;
  r2.w = 0.899999976 * r2.w;
  r2.w = max(0, r2.w);
  r7.w = min(1, r2.w);
  r7.w = r7.w;
  r5.yzw = r5.yzw;
  r7.xyz = r7.xyz;
  r7.w = r7.w;
  r5.x = r5.x;
  r0.xyzw = r0.xyzw;
  r5.x = r5.x;
  r0.xyzw = r0.xyzw;
  r2.w = g_psLight.m_lightParams[3].w;
  r2.w = g_psLight.m_lightParams[7].w * r2.w;
  r1.z = r5.x + r2.w;
  r8.x = dot(r1.xyzw, g_psScene.m_view._m00_m10_m20_m30);
  r8.y = dot(r1.xyzw, g_psScene.m_view._m01_m11_m21_m31);
  r8.z = dot(r1.xyzw, g_psScene.m_view._m02_m12_m22_m32);
  r8.w = dot(r1.xyzw, g_psScene.m_view._m03_m13_m23_m33);
  r8.xyzw = r8.xyzw;
  r1.xyz = g_psLight.m_lightParams[3].xyz;
  r1.w = 1;
  r9.x = dot(r1.xyzw, g_psScene.m_view._m00_m10_m20_m30);
  r9.y = dot(r1.xyzw, g_psScene.m_view._m01_m11_m21_m31);
  r9.z = dot(r1.xyzw, g_psScene.m_view._m02_m12_m22_m32);
  r9.xyz = r9.xyz;
  r1.x = -r8.x;
  r8.x = r1.x + r9.x;
  r1.x = -r9.y;
  r8.y = r8.y + r1.x;
  r1.x = -r9.z;
  r8.z = r8.z + r1.x;
  r8.w = r8.w;
  r1.x = dot(r8.xyz, r8.xyz);
  r1.x = sqrt(r1.x);
  r1.y = g_psLight.m_lightParams[7].z;
  r8.xyz = r8.xyz;
  r8.w = r8.w;
  r1.y = r1.y;
  r8.xyzw = r8.xyzw;
  r8.xyz = r8.xyz / r8.www;
  const float3 shadow_projected = r8.xyz;
  const float3 shadow_projected_dx = ddx(shadow_projected);
  const float3 shadow_projected_dy = ddy(shadow_projected);
  r1.z = cmp(0 >= r8.z);
  r9.x = r1.z ? -1 : 1;
  r8.w = r9.x * r8.z;
  r1.z = dot(r8.xyw, r8.xyw);
  r1.z = sqrt(r1.z);
  r8.xyz = r8.xyw / r1.zzz;
  r1.w = 1 + r8.z;
  r8.xy = r8.xy / r1.ww;
  r1.w = g_psScene.m_shadowMapResolutions.z / 2;
  r2.w = g_psScene.m_shadowMapResolutions.w;
  r4.w = -2 + r1.w;
  r10.x = r4.w / r1.w;
  r1.w = -2 + r2.w;
  r10.y = r1.w / r2.w;
  r9.yz = r10.xy * r8.xy;
  r1.y = r1.z * r1.y;
  r1.y = -r1.y;
  r9.w = 1 + r1.y;
  r9.x = r9.x;
  r9.yz = r9.yz;
  r9.w = r9.w;
  r9.x = r9.x;
  r9.xyzw = r9.xyzw;
  r9.x = r9.x;
  r1.y = r9.y + r9.x;
  r9.y = 0.5 * r1.y;
  r1.yz = float2(0.5, 0.5) * r9.yz;
  r8.xy = float2(0.5, 0.5) + r1.yz;
  r1.y = -r8.y;
  r8.z = 1 + r1.y;
  r10.x = -0.5 / g_psScene.m_shadowMapResolutions.z;
  r10.y = -0.5 / g_psScene.m_shadowMapResolutions.w;
  r9.yz = r10.xy + r8.xz;
  r9.yz = r9.yz;
  r9.w = r9.w;
  r0.xyzw = r0.xyzw;
  r9.yzw = r9.yzw;
  r0.xyzw = r0.xyzw;
  r9.yzw = r9.yzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw / float4(2, 2, 2, 2);
  r0.xyzw = frac(r0.xyzw);
  r0.xyzw = cmp(r0.xyzw >= float4(0.300000012, 0.300000012, 0.300000012, 0.300000012));
  r0.xyzw = r0.xyzw ? float4(1, 1, 1, 1) : float4(0, 0, 0, 0);
  r0.xyzw = float4(2, 2, 2, 2) * r0.xyzw;
  r0.xyzw = float4(-1, -1, -1, -1) + r0.xyzw;
  r0.xyzw = float4(0.125, 0.125, 0.125, 0.125) * r0.xyzw;
  r0.xyzw = float4(0.5, 0.5, -0.5, -0.5) + r0.xyzw;
  r8.xyzw = float4(1, 1, 1, 1) / g_psScene.m_shadowMapResolutions.zwzw;
  r0.xyzw = r8.xyzw * r0.xyzw;
  r9.yzw = r9.yzw;
  r8.xy = r9.yz + r0.xy;
  r8.xy = r8.xy;
  r8.z = r9.w;
  r1.yzw = r8.xyz / float3(1, 1, 1);
  const BendShadow contact_shadow = ReadBendShadow(v0.xy, 2u, g_DepthTexture);
  const bool map_debug = CUSTOM_ENHANCED_SHADOWS_ENABLED && CUSTOM_SHADOW_MAP_DEBUG_VIEW >= 0.5
                         && uint(CUSTOM_SS_DEBUG_LIGHT) == 2u;
  const bool shadow_required = contact_shadow.visibility > 0.0 || contact_shadow.diagnostic || map_debug;
  PcssResult filtered_shadow = (PcssResult)0;
  [branch]
  if (CUSTOM_ENHANCED_SHADOWS_ENABLED && shadow_required) {
    filtered_shadow = FilterPointShadow(g_ShadowTexture, g_samplerComparisonLess_Linear_Clmap_s,
                                        shadow_projected, shadow_projected_dx, shadow_projected_dy,
                                        g_psLight.m_lightParams[7].z, PcssNoise(v0.xy));
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
    r1.y = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r1.yz, r1.w).x;
    r1.y = 0 + r1.y;
    r10.xy = r9.yz + r0.zy;
    r10.xy = r10.xy;
    r10.z = r8.z;
    r10.xyz = r10.xyz / float3(1, 1, 1);
    r0.y = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r10.xy, r10.z).x;
    r0.y = r1.y + r0.y;
    r10.xy = r9.yz + r0.xw;
    r10.xy = r10.xy;
    r10.z = r8.z;
    r1.yzw = r10.xyz / float3(1, 1, 1);
    r0.x = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r1.yz, r1.w).x;
    r0.x = r0.y + r0.x;
    r8.xy = r9.yz + r0.zw;
    r8.xy = r8.xy;
    r0.yzw = r8.xyz / float3(1, 1, 1);
    r0.y = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r0.yz, r0.w).x;
    r0.x = r0.x + r0.y;
    r0.x = 0.25 * r0.x;
  }
  filtered_shadow.visibility = 1.0 - r0.x;
  r0.x = 1 * r0.x;
  r0.x = -r0.x;
  r0.x = 1 + r0.x;
  r0.x = r0.x * r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.y = g_psMaterial.m_materials[7].y;
  r0.z = g_psMaterial.m_materials[7].z;
  r0.y = -r0.y;
  r0.y = r1.x + r0.y;
  r0.y = r0.y * r0.z;
  r0.y = max(0, r0.y);
  r0.y = min(1, r0.y);
  r0.z = -r0.x;
  r0.z = 1 + r0.z;
  r0.y = r0.y * r0.z;
  r0.x = r0.x + r0.y;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.y = r0.x;
  r2.xyz = r2.xyz;
  r5.yzw = r5.yzw;
  r7.xyzw = r7.xyzw;
  r6.xyz = r6.xyz;
  r3.x = r3.x;
  r3.z = r3.z;
  r4.y = r4.y;
  r3.w = r3.w;
  r2.xyz = r2.xyz;
  r5.yzw = r5.yzw;
  r7.xyzw = r7.xyzw;
  r6.xyz = r6.xyz;
  r3.x = r3.x;
  r3.z = r3.z;
  r4.y = r4.y;
  r3.w = r3.w;
  r5.yzw = r5.yzw;
  r2.xyz = r2.xyz;
  r1.xyz = r5.yzw;
  r0.z = dot(r1.xyz, r2.xyz);
  r6.xyz = -r6.xyz;
  r0.w = dot(r6.xyz, r2.xyz);
  r0.w = r0.w + r0.w;
  r0.w = -r0.w;
  r8.xyz = r2.xyz * r0.www;
  r8.xyz = r8.xyz + r6.xyz;
  r8.xyz = -r8.xyz;
  r9.xyz = -r1.xyz;
  r9.xyz = r9.xyz + r8.xyz;
  r0.w = dot(r9.xyz, r9.xyz);
  r0.w = sqrt(r0.w);
  r0.w = r7.w / r0.w;
  r0.w = max(0, r0.w);
  r9.xyz = min(float3(1, 1, 1), r0.www);
  r10.xyz = -r5.yzw;
  r8.xyz = r10.xyz + r8.xyz;
  r8.xyz = r9.xyz * r8.xyz;
  r5.xyz = r8.xyz + r5.yzw;
  r0.w = dot(r5.xyz, r5.xyz);
  r0.w = rsqrt(r0.w);
  r5.xyz = r5.xyz * r0.www;
  r5.xyz = r5.xyz + r6.xyz;
  r0.w = dot(r5.xyz, r5.xyz);
  r0.w = rsqrt(r0.w);
  r5.xyz = r5.xyz * r0.www;
  r0.w = dot(r5.xyz, r2.xyz);
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r1.x = dot(r5.xyz, r1.xyz);
  r3.z = r3.z;
  r3.y = 0.25;
  r2.xyzw = g_MaterialTexture.Sample(g_samplerPoint_Clamp_s, r3.zy).xyzw;
  r3.y = 0.75;
  r4.xz = g_MaterialTexture.Sample(g_samplerPoint_Clamp_s, r3.zy).xw;
  r4.xz = r4.xz;
  r2.xyz = r2.xyz;
  r2.w = r2.w;
  r1.y = r4.x;
  r4.z = r4.z;
  r1.x = r1.x;
  r1.y = r1.y;
  r1.x = g_lut1.Sample(g_samplerLinear_Clamp_s, r1.xy).x;
  r1.x = r1.x;
  r1.x = r1.x;
  r1.y = r0.z * r0.z;
  r1.y = -r1.y;
  r1.y = 1 + r1.y;
  r1.y = r1.y * r3.w;
  r1.y = r1.y + r0.z;
  r1.z = r3.w * r2.w;
  r1.y = max(r1.y, r1.z);
  r1.y = max(0, r1.y);
  r1.y = min(1, r1.y);
  r2.xyz = r2.xyz * r3.xxx;
  r0.w = r0.w;
  r4.y = r4.y;
  r4.z = r4.z;
  r0.w = max(0, r0.w);
  r4.x = min(1, r0.w);
  r4.y = r4.y;
  r4.z = r4.z;
  r3.xyz = r4.xyz * r4.xyz;
  r3.xyz = float3(0.984375, 0.9375, 0.9375) * r3.xyz;
  r3.xyz = float3(0.0078125, 0.03125, 0.03125) + r3.xyz;
  r1.zw = g_lut2.Sample(g_samplerLinear_Clamp_s, r3.xyz).xy;
  r1.zw = r1.zw;
  r1.zw = r1.zw * r1.zw;
  r0.w = r1.z / r1.w;
  r0.w = r1.x * r0.w;
  r0.z = r0.z;
  r0.z = 4 * r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = r0.w * r0.z;
  r1.xzw = r2.xyz * r7.xyz;
  r1.xzw = r1.xzw * r0.zzz;
  r0.z = 1 * r1.y;
  r0.z = 1 * r0.z;
  r2.xyz = r0.zzz * r7.xyz;
  r1.xzw = r1.xzw;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r1.xzw = r1.xzw;
  r2.xyz = r2.xyz;
  r1.xzw = r1.xzw;
  r0.y = r0.y;
  r2.xyz = r2.xyz;
  r1.xzw = r1.xzw;
  r0.y = r0.y;
  r0.x = -1 + r0.x;
  r0.x = g_psLight.m_lightParams[8].z * r0.x;
  r0.x = 1 + r0.x;
  r0.y = -1 + r0.y;
  r0.y = g_psLight.m_lightParams[8].z * r0.y;
  r0.y = 1 + r0.y;
  r0.xzw = r2.xyz * r0.xxx;
  r1.y = g_psLight.m_lightParams[8].y;
  r0.xzw = r1.yyy * r0.xzw;
  r2.xyz = float3(1, 1, 1) * r0.xzw;
  r0.xyz = r1.xzw * r0.yyy;
  r0.w = g_psLight.m_lightParams[8].x;
  r0.xyz = r0.xyz * r0.www;
  r0.xyz = float3(1, 1, 1) * r0.xyz;
  r2.xyz = r2.xyz;
  r0.xyz = r0.xyz;
  r2.xyz = r2.xyz;
  r0.xyz = r0.xyz;
  r2.xyz = r2.xyz;
  r0.xyz = r0.xyz;
  r2.xyz = r2.xyz;
  r0.xyz = r0.xyz;
  r2.xyz = r2.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = 1;
  r0.xyz = r0.xyz;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.w = 1;
  r2.xyz = r2.xyz;
  r2.xyzw = r2.xyzw;
  r2.xyzw = r2.xyzw;
  o0.xyzw = r2.xyzw;
  o1.xyzw = r0.xyzw;
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
