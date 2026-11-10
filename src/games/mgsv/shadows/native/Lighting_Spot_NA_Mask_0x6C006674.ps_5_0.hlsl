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
SamplerState g_samplerLinear_Wrap_s : register(s10);
SamplerState g_samplerLinear_Clamp_s : register(s11);
SamplerComparisonState g_samplerComparisonLess_Linear_Clmap_s : register(s15);
Texture2D<float4> g_NormalTexture : register(t0);
Texture2D<float4> g_SpecularTexture : register(t1);
Texture2D<float4> g_DepthTexture : register(t2);
Texture2D<float4> g_ShadowTexture : register(t3);
Texture2D<float4> g_MaskTexture : register(t4);
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
  float4 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11;
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
  r5.z = r5.z / r1.x;
  r6.z = r5.z;
  r5.xy = r6.zz * r1.yz;
  r6.xy = r5.xy;
  r5.w = 1;
  r6.xy = r6.xy;
  r6.z = r6.z;
  r6.xyz = r6.xyz;
  r1.x = dot(r6.xyz, r6.xyz);
  r1.x = rsqrt(r1.x);
  r1.xyz = r6.xyz * r1.xxx;
  r6.xyz = r6.xyz;
  r1.xyz = r1.xyz;
  r6.xyz = r6.xyz;
  r6.xyz = r6.xyz;
  r6.xyz = -r6.xyz;
  r6.xyz = g_psLight.m_lightParams[3].xyz + r6.xyz;
  r1.w = g_psMaterial.m_materials[7].x;
  r2.w = dot(r6.xyz, r6.xyz);
  r4.w = rsqrt(r2.w);
  r6.w = r4.w * r2.w;
  r1.w = max(r6.w, r1.w);
  r6.xyz = r6.xyz * r4.www;
  r6.w = g_psMaterial.m_materials[7].w;
  r2.w = r2.w * r2.w;
  r2.w = r2.w * r2.w;
  r2.w = r2.w * r2.w;
  r2.w = min(3.40282347e+38, r2.w);
  r4.w = r4.w * r4.w;
  r2.w = g_psLight.m_lightParams[6].z * r2.w;
  r2.w = -r2.w;
  r2.w = 1 + r2.w;
  r2.w = r4.w * r2.w;
  r2.w = min(r6.w, r2.w);
  r2.w = max(0, r2.w);
  r7.xyz = g_psLight.m_lightParams[7].xyz;
  r8.xyz = g_psLight.m_lightParams[5].xyz;
  r6.xyz = r6.xyz;
  r7.x = r7.x;
  r7.y = r7.y;
  r9.xyz = -r6.xyz;
  r4.w = dot(r8.xyz, r9.xyz);
  r7.z = r7.z;
  r6.w = -r7.x;
  r4.w = r6.w + r4.w;
  r4.w = r4.w * r7.y;
  r4.w = max(0, r4.w);
  r4.w = min(1, r4.w);
  r4.w = log2(r4.w);
  r4.w = r7.z * r4.w;
  r7.xyz = exp2(r4.www);
  r7.xyz = r7.xyz;
  r7.xyz = r7.xyz;
  r9.xyz = g_psLight.m_lightParams[4].xyz * r2.www;
  r7.xyz = r9.xyz * r7.xyz;
  r7.xyz = g_psScene.m_exposure.zzz * r7.xyz;
  r2.w = g_psLight.m_lightParams[4].w;
  r1.w = r1.w;
  r2.w = 1 * r2.w;
  r1.w = r2.w / r1.w;
  r1.w = 0.899999976 * r1.w;
  r1.w = max(0, r1.w);
  r7.w = min(1, r1.w);
  r7.w = r7.w;
  r6.xyz = r6.xyz;
  r7.xyz = r7.xyz;
  r7.w = r7.w;
  r0.xyzw = r0.xyzw;
  r6.xyz = r6.xyz;
  r0.xyzw = r0.xyzw;
  r6.xyz = r6.xyz;
  r9.x = dot(r5.xyzw, g_psScene.m_shadowProjection._m00_m10_m20_m30);
  r9.y = dot(r5.xyzw, g_psScene.m_shadowProjection._m01_m11_m21_m31);
  r9.z = dot(r5.xyzw, g_psScene.m_shadowProjection._m02_m12_m22_m32);
  r9.w = dot(r5.xyzw, g_psScene.m_shadowProjection._m03_m13_m23_m33);
  const float4 shadow_projected = r9;
  const float4 shadow_projected_dx = ddx(shadow_projected);
  const float4 shadow_projected_dy = ddy(shadow_projected);
  r9.xyzw = r9.xyzw;
  r9.xyzw = r9.xyzw;
  r1.w = g_psMaterial.m_materials[7].y;
  r2.w = g_psMaterial.m_materials[7].z;
  r1.w = -r1.w;
  r1.w = r9.w + r1.w;
  r1.w = r1.w * r2.w;
  r1.w = max(0, r1.w);
  r1.w = min(1, r1.w);
  r10.xy = g_psMaterial.m_materials[6].xy;
  r6.xyz = r6.xyz;
  r10.x = r10.x;
  r10.y = r10.y;
  r11.xyz = -r6.xyz;
  r2.w = dot(r8.xyz, r11.xyz);
  r4.w = -r10.x;
  r2.w = r4.w + r2.w;
  r2.w = r2.w * r10.y;
  r2.w = max(0, r2.w);
  r2.w = min(1, r2.w);
  r2.w = r2.w * r2.w;
  r2.w = r2.w;
  r2.w = r2.w;
  r9.xyzw = r9.xyzw;
  r0.xyzw = r0.xyzw;
  r2.w = r2.w;
  r9.xyzw = r9.xyzw;
  r0.xyzw = r0.xyzw;
  r2.w = r2.w;
  r9.xyzw = r9.xyzw;
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
  r8.xyzw = r9.wwww / g_psScene.m_shadowMapResolutions.yyyy;
  r0.xyzw = r8.xyzw * r0.xyzw;
  r9.xyzw = r9.xyzw;
  r8.xy = r9.xy + r0.xy;
  r8.xy = r8.xy;
  r8.zw = r9.zw;
  r10.xyz = r8.xyz / r8.www;
  const BendShadow contact_shadow = ReadBendShadow(v0.xy, 1u, g_DepthTexture);
  const bool map_debug = CUSTOM_ENHANCED_SHADOWS_ENABLED && CUSTOM_SHADOW_MAP_DEBUG_VIEW >= 0.5
                         && uint(CUSTOM_SS_DEBUG_LIGHT) == 1u;
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
    r4.w = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r10.xy, r10.z).x;
    r4.w = 0 + r4.w;
    r10.xy = r9.xy + r0.zy;
    r10.xy = r10.xy;
    r10.zw = r8.zw;
    r10.xyz = r10.xyz / r10.www;
    r0.y = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r10.xy, r10.z).x;
    r0.y = r4.w + r0.y;
    r10.xy = r9.xy + r0.xw;
    r10.xy = r10.xy;
    r10.zw = r8.zw;
    r10.xyz = r10.xyz / r10.www;
    r0.x = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r10.xy, r10.z).x;
    r0.x = r0.y + r0.x;
    r8.xy = r9.xy + r0.zw;
    r8.xy = r8.xy;
    r0.yzw = r8.xyz / r8.www;
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
  r0.y = r1.w * r0.y;
  r0.x = r0.x + r0.y;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.y = r0.x;
  r2.xyz = r2.xyz;
  r7.xyzw = r7.xyzw;
  r1.xyz = r1.xyz;
  r3.x = r3.x;
  r3.z = r3.z;
  r4.y = r4.y;
  r3.w = r3.w;
  r2.xyz = r2.xyz;
  r7.xyzw = r7.xyzw;
  r1.xyz = r1.xyz;
  r3.x = r3.x;
  r3.z = r3.z;
  r4.y = r4.y;
  r3.w = r3.w;
  r2.xyz = r2.xyz;
  r8.xyz = r6.xyz;
  r0.z = dot(r8.xyz, r2.xyz);
  r1.xyz = -r1.xyz;
  r0.w = dot(r1.xyz, r2.xyz);
  r0.w = r0.w + r0.w;
  r0.w = -r0.w;
  r9.xyz = r2.xyz * r0.www;
  r9.xyz = r9.xyz + r1.xyz;
  r9.xyz = -r9.xyz;
  r10.xyz = r9.xyz + r11.xyz;
  r0.w = dot(r10.xyz, r10.xyz);
  r0.w = sqrt(r0.w);
  r0.w = r7.w / r0.w;
  r0.w = max(0, r0.w);
  r10.xyz = min(float3(1, 1, 1), r0.www);
  r11.xyz = -r6.xyz;
  r9.xyz = r11.xyz + r9.xyz;
  r9.xyz = r10.xyz * r9.xyz;
  r6.xyz = r9.xyz + r6.xyz;
  r0.w = dot(r6.xyz, r6.xyz);
  r0.w = rsqrt(r0.w);
  r6.xyz = r6.xyz * r0.www;
  r1.xyz = r6.xyz + r1.xyz;
  r0.w = dot(r1.xyz, r1.xyz);
  r0.w = rsqrt(r0.w);
  r1.xyz = r1.xyz * r0.www;
  r0.w = dot(r1.xyz, r2.xyz);
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r1.x = dot(r1.xyz, r8.xyz);
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
  r3.x = dot(r5.xyzw, g_psScene.m_shadowProjection2._m00_m10_m20_m30);
  r3.y = dot(r5.xyzw, g_psScene.m_shadowProjection2._m01_m11_m21_m31);
  r3.z = dot(r5.xyzw, g_psScene.m_shadowProjection2._m03_m13_m23_m33);
  r3.xyz = r3.xyz;
  r0.zw = r3.xy / r3.zz;
  r0.zw = float2(0.5, 0.5) * r0.zw;
  r3.xy = float2(0.5, 0.5) + r0.zw;
  r0.z = -r3.y;
  r3.z = 1 + r0.z;
  r3.xyz = g_MaskTexture.SampleLevel(g_samplerLinear_Wrap_s, r3.xz, 0).xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r2.xyz = r2.xyz;
  r1.xzw = r1.xzw;
  r0.y = r0.y;
  r3.xyz = r3.xyz;
  r2.xyz = r2.xyz;
  r1.xzw = r1.xzw;
  r0.y = r0.y;
  r3.xyz = r3.xyz;
  r0.x = -1 + r0.x;
  r0.x = g_psLight.m_lightParams[8].z * r0.x;
  r0.x = 1 + r0.x;
  r0.y = -1 + r0.y;
  r0.y = g_psLight.m_lightParams[8].z * r0.y;
  r0.y = 1 + r0.y;
  r0.xzw = r2.xyz * r0.xxx;
  r1.y = g_psLight.m_lightParams[8].y;
  r0.xzw = r1.yyy * r0.xzw;
  r2.xyz = r0.xzw * r3.xyz;
  r0.xyz = r1.xzw * r0.yyy;
  r0.w = g_psLight.m_lightParams[8].x;
  r0.xyz = r0.xyz * r0.www;
  r0.xyz = r0.xyz * r3.xyz;
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
