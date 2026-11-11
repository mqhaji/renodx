#include "../shadows/screen_space_shadows.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:35 2026

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

cbuffer cPSLight : register(b3) {
  struct
  {
    float4 m_lightParams[11];
  } g_psLight : packoffset(c0);
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
Texture2D<float4> g_CloudTexture : register(t4);
Texture3D<float4> g_lut2 : register(t8);
Texture2D<float4> g_lut1 : register(t9);
Texture2D<float4> g_MaterialTexture : register(t10);
Texture2D<float4> g_RandomTexture : register(t12);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0 : SV_Position0,
    float4 v1 : TEXCOORD0,
    float4 v2 : TEXCOORD1,
    out float4 o0 : SV_Target0,
    out float4 o1 : SV_Target1,
    out float4 o2 : SV_Target2,
    out float4 specular_reset : SV_Target3,
    out float4 composition_reset : SV_Target4) {
  float4 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15, r16;
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
  r6.w = 1;
  r6.xyzw = r6.xyzw;
  r1.x = dot(r6.xyz, r6.xyz);
  r1.x = rsqrt(r1.x);
  r1.xyz = r6.xyz * r1.xxx;
  r6.xyzw = r6.xyzw;
  r1.xyz = r1.xyz;
  r7.xyz = g_psLight.m_lightParams[0].xyz;
  r8.xyz = g_psLight.m_lightParams[1].xyz * g_psScene.m_exposure.zzz;
  r8.w = g_psLight.m_lightParams[1].w;
  r7.xyz = r7.xyz;
  r8.xyzw = r8.xyzw;
  r6.xyzw = r6.xyzw;
  r0.xyzw = r0.xyzw;
  r6.xyzw = r6.xyzw;
  r0.xyzw = r0.xyzw;
  r9.x = 0.5 / g_psScene.m_shadowMapResolutions.x;
  r9.y = 0.5 / g_psScene.m_shadowMapResolutions.x;
  r10.x = dot(r5.xyzw, g_psScene.m_shadowProjection._m00_m10_m20_m30);
  r10.y = dot(r5.xyzw, g_psScene.m_shadowProjection._m01_m11_m21_m31);
  r10.z = dot(r5.xyzw, g_psScene.m_shadowProjection._m02_m12_m22_m32);
  const float3 shadow_projected = r10.xyz;
  const float3 shadow_projected_dx = ddx(shadow_projected);
  const float3 shadow_projected_dy = ddy(shadow_projected);
  r10.xyz = r10.xyz;
  r10.xyz = r10.xyz;
  r5.xy = r0.zw;
  r1.w = 1 / g_psScene.m_shadowMapResolutions.x;
  r2.w = 8 * r1.w;
  r2.w = -r2.w;
  r2.w = 1 + r2.w;
  r5.zw = r2.ww * r2.ww;
  r4.w = g_psScene.m_shadowProjection2._m33;
  r4.w = r4.w;
  r7.w = r5.w;
  r4.w = cmp(0 != r4.w);
  if (r4.w != 0) {
    r5.xy = float2(0.03125, 0.03125) * r5.xy;
    r5.xy = g_RandomTexture.Sample(g_samplerLinear_Wrap_s, r5.xy).xy;
    r4.w = max(r5.x, r5.y);
    r5.x = 0.00249999994 * r4.w;
    r4.w = g_psScene.m_shadowMapResolutions.x * r4.w;
    r4.w = r4.w / 4;
    r1.w = r4.w * r1.w;
    r1.w = -r1.w;
    r1.w = r2.w + r1.w;
    r1.w = r1.w * r1.w;
    r7.w = 1 * r1.w;
  } else {
    r5.x = 0;
  }
  r1.w = g_psScene.m_shadowProjection2._m30;
  r1.w = r1.w;
  r10.w = 0;
  r11.x = g_psScene.m_shadowProjection2._m00;
  r11.y = g_psScene.m_shadowProjection2._m01;
  r11.z = g_psScene.m_shadowProjection2._m02;
  r11.w = g_psScene.m_shadowProjection2._m03;
  r11.xyzw = r11.xyzw;
  r12.x = g_psScene.m_shadowProjection2._m10;
  r12.y = g_psScene.m_shadowProjection2._m11;
  r12.z = g_psScene.m_shadowProjection2._m12;
  r12.w = g_psScene.m_shadowProjection2._m13;
  r12.xyzw = r12.xyzw;
  r13.x = g_psScene.m_shadowProjection2._m20;
  r13.y = g_psScene.m_shadowProjection2._m21;
  r13.z = g_psScene.m_shadowProjection2._m22;
  r13.w = g_psScene.m_shadowProjection2._m23;
  r13.xyzw = r13.xyzw;
  r14.xyz = r11.www * r10.xyz;
  r11.xyz = r14.xyz + r11.xyz;
  r11.w = 1;
  r14.xyz = r12.www * r10.xyz;
  r12.xyz = r14.xyz + r12.xyz;
  r12.w = 2;
  r14.xyz = r13.www * r10.xyz;
  r13.xyz = r14.xyz + r13.xyz;
  r13.w = 3;
  r2.w = 1 + r10.z;
  r10.z = r2.w * r1.w;
  r2.w = 1 + r11.z;
  r11.z = r2.w * r1.w;
  r2.w = 1 + r12.z;
  r12.z = r2.w * r1.w;
  r2.w = 1 + r13.z;
  r13.z = r2.w * r1.w;
  r9.zw = r10.xy * r10.xy;
  r14.xy = r11.xy * r11.xy;
  r14.zw = r12.xy * r12.xy;
  r5.yz = cmp(r5.zw >= r9.zw);
  r5.yz = r5.yz ? float2(1, 1) : float2(0, 0);
  r1.w = cmp(r10.z >= r5.x);
  r1.w = r1.w ? 1 : 0;
  r9.zw = r7.ww;
  r14.xy = cmp(r9.zw >= r14.xy);
  r14.xy = r14.xy ? float2(1, 1) : float2(0, 0);
  r2.w = cmp(r11.z >= r5.x);
  r2.w = r2.w ? 1 : 0;
  r9.zw = cmp(r9.zw >= r14.zw);
  r9.zw = r9.zw ? float2(1, 1) : float2(0, 0);
  r4.w = cmp(r12.z >= r5.x);
  r4.w = r4.w ? 1 : 0;
  r5.x = r5.y * r5.z;
  r5.xyzw = r5.xxxx * r1.wwww;
  r1.w = r14.x * r14.y;
  r1.w = r1.w * r2.w;
  r2.w = r9.z * r9.w;
  r2.w = r2.w * r4.w;
  r4.w = -r5.w;
  r4.w = 1 + r4.w;
  r14.xyzw = r4.wwww * r1.wwww;
  r1.w = -r14.w;
  r1.w = 1 + r1.w;
  r1.w = r4.w * r1.w;
  r15.xyzw = r2.wwww * r1.wwww;
  r2.w = -r15.w;
  r2.w = 1 + r2.w;
  r1.w = r2.w * r1.w;
  r16.xyzw = float4(1, 1, 1, 1) * r1.wwww;
  r5.xyzw = r10.xyzw * r5.xyzw;
  r10.xyzw = r14.xyzw * r11.xyzw;
  r5.xyzw = r10.xyzw + r5.xyzw;
  r10.xyzw = r15.xyzw * r12.xyzw;
  r5.xyzw = r10.xyzw + r5.xyzw;
  r10.xyzw = r16.xyzw * r13.xyzw;
  r5.xyzw = r10.wxyz + r5.wxyz;
  r5.w = max(9.99999997e-07, r5.w);
  r5.xyz = r5.xyz;
  r5.w = r5.w;
  r5.xyzw = r5.xyzw;
  r5.x = r5.x;
  r9.zw = float2(0.5, 0.5) * r5.yz;
  r10.xy = float2(0.5, 0.5) + r9.zw;
  r1.w = -r10.y;
  r10.z = 1 + r1.w;
  r9.zw = float2(0.5, 0.5) * r10.xz;
  r10.xy = float2(2, 2) * r9.xy;
  r10.xy = -r10.xy;
  r10.xy = float2(1, 1) + r10.xy;
  r9.zw = min(r10.xy, r9.zw);
  r1.w = -1 + r5.x;
  r1.w = max(0, r1.w);
  r1.w = min(1, r1.w);
  r2.w = r5.x / 2;
  r4.w = -r1.w;
  r10.x = r4.w + r2.w;
  r10.y = 0.5 * r1.w;
  r9.zw = r10.xy + r9.zw;
  r5.yz = r9.zw + r9.xy;
  r5.yz = r5.yz;
  r5.w = r5.w;
  r0.xyzw = r0.xyzw;
  r5.yzw = r5.yzw;
  r0.xyzw = r0.xyzw;
  r5.yzw = r5.yzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw / float4(2, 2, 2, 2);
  r0.xyzw = frac(r0.xyzw);
  r0.xyzw = cmp(r0.xyzw >= float4(0.300000012, 0.300000012, 0.300000012, 0.300000012));
  r0.xyzw = r0.xyzw ? float4(1, 1, 1, 1) : float4(0, 0, 0, 0);
  r0.xyzw = float4(2, 2, 2, 2) * r0.xyzw;
  r0.xyzw = float4(-1, -1, -1, -1) + r0.xyzw;
  r0.xyzw = float4(0.125, 0.125, 0.125, 0.125) * r0.xyzw;
  r0.xyzw = float4(0.5, 0.5, -0.5, -0.5) + r0.xyzw;
  r9.xyzw = float4(1, 1, 1, 1) / g_psScene.m_shadowMapResolutions.xxxx;
  r0.xyzw = r9.xyzw * r0.xyzw;
  r5.yzw = r5.yzw;
  r9.xy = r5.yz + r0.xy;
  r9.xy = r9.xy;
  r9.z = r5.w;
  r10.xyz = r9.xyz / float3(1, 1, 1);
  const BendShadow contact_shadow = ReadBendShadow(v0.xy, 0u, g_DepthTexture);
  const bool map_debug = CUSTOM_ENHANCED_SHADOWS_ENABLED && CUSTOM_SHADOW_MAP_DEBUG_VIEW >= 0.5
                         && uint(CUSTOM_SHADOW_DEBUG_LIGHT) == 0u;
  const bool shadow_required = contact_shadow.visibility > 0.0 || contact_shadow.diagnostic || map_debug;
  PcssResult filtered_shadow = (PcssResult)0;
  [branch]
  if (CUSTOM_ENHANCED_SHADOWS_ENABLED && shadow_required) {
    filtered_shadow = FilterSunShadow(g_ShadowTexture, g_samplerComparisonLess_Linear_Clmap_s,
                                      shadow_projected, shadow_projected_dx, shadow_projected_dy, r5.yz,
                                      ReadPcssProjection(g_psScene.m_shadowProjection, g_psScene.m_shadowProjection2, true),
                                      g_psScene.m_shadowProjection2, PcssNoise(v0.xy));
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
    r1.w = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r10.xy, r10.z).x;
    r1.w = 0 + r1.w;
    r10.xy = r5.yz + r0.zy;
    r10.xy = r10.xy;
    r10.z = r9.z;
    r10.xyz = r10.xyz / float3(1, 1, 1);
    r0.y = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r10.xy, r10.z).x;
    r0.y = r1.w + r0.y;
    r10.xy = r5.yz + r0.xw;
    r10.xy = r10.xy;
    r10.z = r9.z;
    r10.xyz = r10.xyz / float3(1, 1, 1);
    r0.x = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r10.xy, r10.z).x;
    r0.x = r0.y + r0.x;
    r9.xy = r5.yz + r0.zw;
    r9.xy = r9.xy;
    r0.yzw = r9.xyz / float3(1, 1, 1);
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
  r5.xy = float2(-0, -0);
  r5.z = -g_psLight.m_lightParams[8].x;
  r0.yzw = r6.xyz + r5.xyz;
  r0.y = dot(r0.yzw, r0.yzw);
  r0.y = sqrt(r0.y);
  r0.z = -g_psLight.m_lightParams[8].y;
  r0.y = r0.y + r0.z;
  r0.y = max(0, r0.y);
  r0.y = g_psLight.m_lightParams[6].x * r0.y;
  r0.y = max(0, r0.y);
  r0.y = min(1, r0.y);
  r0.z = cmp(r0.w >= 0);
  r0.z = r0.z ? 1 : 0;
  r0.y = r0.y * r0.z;
  r0.y = r0.y * r0.y;
  r0.z = -r0.x;
  r0.z = 1 + r0.z;
  r0.y = r0.y * r0.z;
  r0.x = r0.x + r0.y;
  r0.y = g_psLight.m_lightParams[6].w;
  r0.y = -r0.y;
  r0.x = r0.x + r0.y;
  r0.x = max(0, r0.x);
  r0.x = min(1, r0.x);
  r5.x = dot(g_psObject.m_localParam[0].xyzw, r6.xyzw);
  r5.y = dot(g_psObject.m_localParam[1].xyzw, r6.xyzw);
  r0.y = g_psObject.m_localParam[3].w;
  r0.z = g_psObject.m_localParam[3].z;
  r0.w = g_CloudTexture.Sample(g_samplerLinear_Wrap_s, r5.xy).x;
  r0.z = -0 + r0.z;
  r0.w = -0 + r0.w;
  r0.z = 1 / r0.z;
  r0.z = r0.w * r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.w = -2 * r0.z;
  r0.w = 3 + r0.w;
  r0.z = r0.z * r0.z;
  r0.z = r0.w * r0.z;
  r0.y = r0.z * r0.y;
  r0.y = max(0, r0.y);
  r0.y = min(1, r0.y);
  r0.x = r0.x * r0.y;
  r0.y = -g_psObject.m_localParam[2].x;
  r0.y = r6.z + r0.y;
  r0.y = g_psObject.m_localParam[2].y * r0.y;
  r0.y = max(0, r0.y);
  r0.y = min(1, r0.y);
  r0.y = -r0.y;
  r0.y = 1 + r0.y;
  r0.y = r0.y * r0.y;
  r0.x = r0.x * r0.y;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.y = g_psLight.m_lightParams[7].x;
  r0.y = max(r0.y, r0.x);
  r0.y = r0.y;
  r2.xyz = r2.xyz;
  r7.xyz = r7.xyz;
  r8.xyzw = r8.xyzw;
  r1.xyz = r1.xyz;
  r3.x = r3.x;
  r3.z = r3.z;
  r4.y = r4.y;
  r3.w = r3.w;
  r2.xyz = r2.xyz;
  r7.xyz = r7.xyz;
  r8.xyzw = r8.xyzw;
  r1.xyz = r1.xyz;
  r3.x = r3.x;
  r3.z = r3.z;
  r4.y = r4.y;
  r3.w = r3.w;
  r7.xyz = r7.xyz;
  r2.xyz = r2.xyz;
  r5.xyz = r7.xyz;
  r0.z = dot(r5.xyz, r2.xyz);
  r1.xyz = -r1.xyz;
  r0.w = dot(r1.xyz, r2.xyz);
  r0.w = r0.w + r0.w;
  r0.w = -r0.w;
  r6.xyz = r2.xyz * r0.www;
  r6.xyz = r6.xyz + r1.xyz;
  r6.xyz = -r6.xyz;
  r9.xyz = -r5.xyz;
  r9.xyz = r9.xyz + r6.xyz;
  r0.w = dot(r9.xyz, r9.xyz);
  r0.w = sqrt(r0.w);
  r0.w = r8.w / r0.w;
  r0.w = max(0, r0.w);
  r9.xyz = min(float3(1, 1, 1), r0.www);
  r10.xyz = -r7.xyz;
  r6.xyz = r10.xyz + r6.xyz;
  r6.xyz = r9.xyz * r6.xyz;
  r6.xyz = r7.xyz + r6.xyz;
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
  r1.x = dot(r1.xyz, r5.xyz);
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
  r1.xzw = r2.xyz * r8.xyz;
  r1.xzw = r1.xzw * r0.zzz;
  r0.z = 1 * r1.y;
  r0.z = 1 * r0.z;
  r2.xyz = r0.zzz * r8.xyz;
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
  r0.xzw = r2.xyz * r0.xxx;
  r0.xzw = float3(1, 1, 1) * r0.xzw;
  r1.xyz = r1.xzw * r0.yyy;
  r1.xyz = float3(1, 1, 1) * r1.xyz;
  r0.xzw = r0.xzw;
  r1.xyz = r1.xyz;
  r0.xzw = r0.xzw;
  r1.xyz = r1.xyz;
  r0.xzw = r0.xzw;
  r1.xyz = r1.xyz;
  r0.xyz = float3(-0, -0, -0) + r0.xzw;
  r1.xyz = float3(0, 0, 0) + r1.xyz;
  r0.xyz = r0.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.w = 1;
  r1.xyz = r1.xyz;
  r1.xyzw = r1.xyzw;
  r1.xyzw = r1.xyzw;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = 1;
  r0.xyz = r0.xyz;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  o0.xyzw = r0.xyzw;
  o1.xyzw = r1.xyzw;
  // Preserve native output alpha and any diffuse-only material behavior.
  o0.rgb *= contact_shadow.visibility;
  o1.rgb *= contact_shadow.visibility;
  ApplyBendDebug(contact_shadow, o0.rgb, o1.rgb);
  if (map_debug) {
    o0.rgb = PcssDebug(filtered_shadow, uint(CUSTOM_SHADOW_MAP_DEBUG_VIEW), PCSS_MAX_RADIUS);
    o1.rgb = 0.0;
  }
  o2 = float4(0.0, 0.0, 0.0, 1.0);
  specular_reset = 0.0;
  composition_reset = 0.0;
  return;
}
