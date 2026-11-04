// Directional lighting, ported from native shader 0x5AFD2E17 with 3Dmigoto.
// PCSS replaces only shadow-map filtering. Native lighting, cloud shadows,
// distance fades, and output packing retain their original calculations.
#include "../shared.h"
#include "pcss.hlsli"
#include "screen_space_shadows.hlsli"

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

cbuffer cPSSystem : register(b0) {
  struct
  {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem : packoffset(c0);
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
Texture2D<float4> g_SkyReflectTexture : register(t15);
// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0 : SV_Position0,
    float4 v1 : TEXCOORD0,
    float4 v2 : TEXCOORD1,
    out float4 o0 : SV_Target0,
    out float4 o1 : SV_Target1
#ifdef MGSV_CAPTURE_ENVIRONMENT_SPECULAR
    ,
    out float4 o2 : SV_Target2
#endif
) {
  float4 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = float4(-0.5, -0.5, -0.5, -0.5) + v0.xyxy;
  r1.xyz = v2.xyw;
  r1.xy = r1.xy / r1.zz;
  r2.xyz = g_NormalTexture.Sample(g_samplerPoint_Wrap_s, r1.xy).xyz;
  r3.xyzw = g_SpecularTexture.Sample(g_samplerPoint_Wrap_s, r1.xy).yxzw;
  r1.x = g_DepthTexture.Sample(g_samplerPoint_Wrap_s, r1.xy).x;
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
  r1.y = 1.00000001e-007 + r1.y;
  r1.y = rsqrt(r1.y);
  r2.xy = r4.xy * r1.yy;
  r1.y = r3.y;
  r4.xyz = v1.xyw;
  r1.zw = r4.xy / r4.zz;
  r4.xy = -g_psScene.m_cameraCenterOffset.xy;
  r1.zw = r4.xy + r1.zw;
  r4.xyzw = g_psScene.m_projectionParam.xyzw;
  r1.zw = r4.xy * r1.zw;
  r2.w = -r4.w;
  r1.x = r2.w + r1.x;
  r4.z = r4.z / r1.x;
  r5.z = r4.z;
  r4.xy = r5.zz * r1.zw;
  r5.xy = r4.xy;
  const float3 receiver_dx = ddx(r5.xyz);
  const float3 receiver_dy = ddy(r5.xyz);
  r4.w = 1;
  r5.w = 1;
  r1.x = dot(r5.xyz, r5.xyz);
  r1.x = rsqrt(r1.x);
  r1.xzw = r5.xyz * r1.xxx;
  r6.xyz = g_psLight.m_lightParams[0].xyz;
  r7.xyz = g_psLight.m_lightParams[1].xyz * g_psScene.m_exposure.zzz;
  r7.w = g_psLight.m_lightParams[1].w;
  r8.x = 0.5 / g_psScene.m_shadowMapResolutions.x;
  r8.y = 0.5 / g_psScene.m_shadowMapResolutions.x;
  r9.x = dot(r4.xyzw, g_psScene.m_shadowProjection._m00_m10_m20_m30);
  r9.y = dot(r4.xyzw, g_psScene.m_shadowProjection._m01_m11_m21_m31);
  r9.z = dot(r4.xyzw, g_psScene.m_shadowProjection._m02_m12_m22_m32);
  const float3 shadow_projected = r9.xyz;
  r4.xy = r0.zw;
  r2.w = 1 / g_psScene.m_shadowMapResolutions.x;
  r4.z = 8 * r2.w;
  r4.z = -r4.z;
  r4.z = 1 + r4.z;
  r8.zw = r4.zz * r4.zz;
  r4.w = g_psScene.m_shadowProjection2._m33;
  r6.w = r8.w;
  r4.w = cmp(0 != r4.w);
  if (r4.w != 0) {
    r4.xy = float2(0.03125, 0.03125) * r4.xy;
    r4.xy = g_RandomTexture.Sample(g_samplerLinear_Wrap_s, r4.xy).xy;
    r4.x = max(r4.x, r4.y);
    r4.y = 0.00249999994 * r4.x;
    r4.x = g_psScene.m_shadowMapResolutions.x * r4.x;
    r4.x = r4.x / 4;
    r2.w = r4.x * r2.w;
    r2.w = -r2.w;
    r2.w = r4.z + r2.w;
    r2.w = r2.w * r2.w;
    r6.w = 1 * r2.w;
  } else {
    r4.y = 0;
  }
  r2.w = g_psScene.m_shadowProjection2._m30;
  r9.w = 0;
  r10.x = g_psScene.m_shadowProjection2._m00;
  r10.y = g_psScene.m_shadowProjection2._m01;
  r10.z = g_psScene.m_shadowProjection2._m02;
  r10.w = g_psScene.m_shadowProjection2._m03;
  r11.x = g_psScene.m_shadowProjection2._m10;
  r11.y = g_psScene.m_shadowProjection2._m11;
  r11.z = g_psScene.m_shadowProjection2._m12;
  r11.w = g_psScene.m_shadowProjection2._m13;
  r12.x = g_psScene.m_shadowProjection2._m20;
  r12.y = g_psScene.m_shadowProjection2._m21;
  r12.z = g_psScene.m_shadowProjection2._m22;
  r12.w = g_psScene.m_shadowProjection2._m23;
  r4.xzw = r10.www * r9.xyz;
  r10.xyz = r4.xzw + r10.xyz;
  r10.w = 1;
  r4.xzw = r11.www * r9.xyz;
  r11.xyz = r4.xzw + r11.xyz;
  r11.w = 2;
  r4.xzw = r12.www * r9.xyz;
  r12.xyz = r4.xzw + r12.xyz;
  r12.w = 3;
  r4.x = 1 + r9.z;
  r9.z = r4.x * r2.w;
  r4.x = 1 + r10.z;
  r10.z = r4.x * r2.w;
  r4.x = 1 + r11.z;
  r11.z = r4.x * r2.w;
  r4.x = 1 + r12.z;
  r12.z = r4.x * r2.w;
  r4.xz = r9.xy * r9.xy;
  r13.xy = r10.xy * r10.xy;
  r13.zw = r11.xy * r11.xy;
  r4.xz = cmp(r8.zw >= r4.xz);
  r4.xz = r4.xz ? float2(1, 1) : float2(0, 0);
  r2.w = cmp(r9.z >= r4.y);
  r2.w = r2.w ? 1 : 0;
  r8.zw = r6.ww;
  r13.xy = cmp(r8.zw >= r13.xy);
  r13.xy = r13.xy ? float2(1, 1) : float2(0, 0);
  r4.w = cmp(r10.z >= r4.y);
  r4.w = r4.w ? 1 : 0;
  r8.zw = cmp(r8.zw >= r13.zw);
  r8.zw = r8.zw ? float2(1, 1) : float2(0, 0);
  r4.y = cmp(r11.z >= r4.y);
  r4.y = r4.y ? 1 : 0;
  r4.x = r4.x * r4.z;
  r14.xyzw = r4.xxxx * r2.wwww;
  r2.w = r13.x * r13.y;
  r2.w = r2.w * r4.w;
  r4.x = r8.z * r8.w;
  r4.x = r4.x * r4.y;
  r4.y = -r14.w;
  r4.y = 1 + r4.y;
  r13.xyzw = r4.yyyy * r2.wwww;
  r2.w = -r13.w;
  r2.w = 1 + r2.w;
  r2.w = r4.y * r2.w;
  r4.xyzw = r4.xxxx * r2.wwww;
  r6.w = -r4.w;
  r6.w = 1 + r6.w;
  r2.w = r6.w * r2.w;
  r15.xyzw = float4(1, 1, 1, 1) * r2.wwww;
  r9.xyzw = r14.xyzw * r9.xyzw;
  r10.xyzw = r13.xyzw * r10.xyzw;
  r9.xyzw = r10.xyzw + r9.xyzw;
  r4.xyzw = r11.xyzw * r4.xyzw;
  r4.xyzw = r9.xyzw + r4.xyzw;
  r9.xyzw = r15.xyzw * r12.xyzw;
  r4.xyzw = r9.wxyz + r4.wxyz;
  r4.w = max(9.99999997e-007, r4.w);
  r8.zw = float2(0.5, 0.5) * r4.yz;
  r9.xy = float2(0.5, 0.5) + r8.zw;
  r2.w = -r9.y;
  r9.z = 1 + r2.w;
  r8.zw = float2(0.5, 0.5) * r9.xz;
  r9.xy = float2(2, 2) * r8.xy;
  r9.xy = -r9.xy;
  r9.xy = float2(1, 1) + r9.xy;
  r8.zw = min(r9.xy, r8.zw);
  r2.w = -1 + r4.x;
  r2.w = max(0, r2.w);
  r2.w = min(1, r2.w);
  r4.x = r4.x / 2;
  r6.w = -r2.w;
  r9.x = r6.w + r4.x;
  r9.y = 0.5 * r2.w;
  r8.zw = r9.xy + r8.zw;
  r4.yz = r8.zw + r8.xy;
  const BendShadow contact_shadow = ReadBendShadow(v0.xy, 0u);
  const bool map_debug = CUSTOM_ENHANCED_SHADOWS_ENABLED && CUSTOM_SHADOW_MAP_DEBUG_VIEW >= 0.5
                         && uint(CUSTOM_SS_DEBUG_LIGHT) == 0u;
  const bool shadow_required = contact_shadow.visibility > 0.0 || contact_shadow.diagnostic
                               || map_debug;
  PcssResult sun_pcss = (PcssResult)0;
  float map_visibility = 1.0;
  [branch]
  if (shadow_required) {
    [branch]
    if (CUSTOM_ENHANCED_SHADOWS_ENABLED) {
      sun_pcss = FilterSunShadow(g_ShadowTexture, g_samplerComparisonLess_Linear_Clmap_s,
                                 shadow_projected,
                                 mul(float4(receiver_dx, 0.0), g_psScene.m_shadowProjection).xyz,
                                 mul(float4(receiver_dy, 0.0), g_psScene.m_shadowProjection).xyz, r4.yz,
                                 ReadPcssProjection(g_psScene.m_shadowProjection, g_psScene.m_shadowProjection2, true),
                                 g_psScene.m_shadowProjection2, PcssNoise(v0.xy));
    }
    // Only evaluate native taps when the custom filter could not supply visibility.
    [branch]
    if (sun_pcss.filtered) {
      map_visibility = sun_pcss.visibility;
    }
    else {
      map_visibility = NativeShadowVisibility(g_ShadowTexture, g_samplerComparisonLess_Linear_Clmap_s,
                                              float4(r4.yz, r4.w, 1.0), g_psScene.m_shadowMapResolutions.xx, v0.xy);
    }
  }
  sun_pcss.visibility = map_visibility;
  r0.x = map_visibility * map_visibility;

  r4.xy = float2(-0, -0);
  r4.z = -g_psLight.m_lightParams[8].x;
  r0.yzw = r5.xyz + r4.xyz;
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
  r4.x = dot(g_psObject.m_localParam[0].xyzw, r5.xyzw);
  r4.y = dot(g_psObject.m_localParam[1].xyzw, r5.xyzw);
  r0.y = g_psObject.m_localParam[3].w;
  r0.z = g_psObject.m_localParam[3].z;
  r0.w = g_CloudTexture.Sample(g_samplerLinear_Wrap_s, r4.xy).x;
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
  r0.y = r5.z + r0.y;
  r0.y = g_psObject.m_localParam[2].y * r0.y;
  r0.y = max(0, r0.y);
  r0.y = min(1, r0.y);
  r0.y = -r0.y;
  r0.y = 1 + r0.y;
  r0.y = r0.y * r0.y;
  r0.x = r0.x * r0.y;
  r0.y = g_psLight.m_lightParams[7].x;
  r0.y = max(r0.y, r0.x);
  r4.xyz = r6.xyz;
  r0.z = dot(r4.xyz, r2.xyz);
  r5.xyz = -r1.xzw;
  r0.w = dot(r5.xyz, r2.xyz);
  r0.w = r0.w + r0.w;
  r0.w = -r0.w;
  r8.xyz = r2.xyz * r0.www;
  r8.xyz = r8.xyz + r5.xyz;
  r8.xyz = -r8.xyz;
  r9.xyz = -r4.xyz;
  r9.xyz = r9.xyz + r8.xyz;
  r0.w = dot(r9.xyz, r9.xyz);
  r0.w = sqrt(r0.w);
  r0.w = r7.w / r0.w;
  r0.w = max(0, r0.w);
  r9.xyz = min(float3(1, 1, 1), r0.www);
  r10.xyz = -r6.xyz;
  r8.xyz = r10.xyz + r8.xyz;
  r8.xyz = r9.xyz * r8.xyz;
  r6.xyz = r8.xyz + r6.xyz;
  r0.w = dot(r6.xyz, r6.xyz);
  r0.w = rsqrt(r0.w);
  r6.xyz = r6.xyz * r0.www;
  r5.xyz = r6.xyz + r5.xyz;
  r0.w = dot(r5.xyz, r5.xyz);
  r0.w = rsqrt(r0.w);
  r5.xyz = r5.xyz * r0.www;
  r0.w = dot(r5.xyz, r2.xyz);
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r4.x = dot(r5.xyz, r4.xyz);
  r3.y = 0.25;
  r5.xyzw = g_MaterialTexture.Sample(g_samplerPoint_Clamp_s, r3.zy).xyzw;
  r3.y = 0.75;
  r6.xz = g_MaterialTexture.Sample(g_samplerPoint_Clamp_s, r3.zy).xw;
  r2.w = r6.x;
  r4.y = r2.w;
  r3.y = g_lut1.Sample(g_samplerLinear_Clamp_s, r4.xy).x;
  r3.z = r0.z * r0.z;
  r3.z = -r3.z;
  r3.z = 1 + r3.z;
  r3.z = r3.z * r3.w;
  r3.z = r3.z + r0.z;
  r3.w = r5.w * r3.w;
  r3.z = max(r3.z, r3.w);
  r3.z = max(0, r3.z);
  r3.z = min(1, r3.z);
  r4.xyz = r5.xyz * r3.xxx;
  r0.w = max(0, r0.w);
  r6.x = min(1, r0.w);
  r6.y = r1.y;
  r6.xyz = r6.xyz * r6.xyz;
  r6.xyz = float3(0.984375, 0.9375, 0.9375) * r6.xyz;
  r6.xyz = float3(0.0078125, 0.03125, 0.03125) + r6.xyz;
  r6.xy = g_lut2.Sample(g_samplerLinear_Clamp_s, r6.xyz).xy;
  r6.xy = r6.xy * r6.xy;
  r0.w = r6.x / r6.y;
  r0.w = r3.y * r0.w;
  r0.z = 4 * r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = r0.w * r0.z;
  r4.xyz = r4.xyz * r7.xyz;
  r4.xyz = r4.xyz * r0.zzz;
  r0.z = 1 * r3.z;
  r0.z = 1 * r0.z;
  r3.yzw = r0.zzz * r7.xyz;
  r0.z = dot(r1.xzw, r2.xyz);
  r0.z = r0.z + r0.z;
  r0.z = -r0.z;
  r6.xyz = r2.xyz * r0.zzz;
  r6.xyz = r6.xyz + r1.xzw;
  r1.xzw = -r1.xzw;
  r0.z = dot(r1.xzw, r2.xyz);
  r0.z = max(0, r0.z);
  r2.x = min(1, r0.z);
  r2.y = max(g_psLight.m_lightParams[2].z, r2.w);
  r0.z = g_lut1.Sample(g_samplerLinear_Clamp_s, r2.xy).x;
  r1.xz = -r6.xy;
  r1.xz = float2(0.96875, 0.96875) * r1.xz;
  r1.xz = float2(0.5, 0.5) * r1.xz;
  r1.xz = float2(0.5, 0.5) + r1.xz;
  r1.xz = float2(0.25, 0.125) * r1.xz;
  r0.w = cmp(0 < r6.z);
  r2.xy = r0.ww ? float2(0.25, 0) : float2(0, 0);
  r1.xz = r2.xy + r1.xz;
  r1.xz = g_SkyReflectTexture.Sample(g_samplerLinear_Clamp_s, r1.xz).xy;
  r1.xz = float2(4, 4) * r1.xz;
  r0.w = 1.42799997 * r1.y;
  r0.w = -0.300000012 + r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r1.w = -r1.x;
  r1.z = r1.z + r1.w;
  r0.w = r1.z * r0.w;
  r1.xzw = r1.xxx + r0.www;
  r2.xyz = g_psLight.m_lightParams[3].xyz;
  r1.xzw = r2.xyz * r1.xzw;
  r0.w = -r1.y;
  r0.w = 1 + r0.w;
  r0.w = 0.5 * r0.w;
  r0.z = r0.w * r0.z;
  r0.z = r0.z * r3.x;
  r1.xyz = r1.xzw * r0.zzz;
  r1.xyz = r1.xyz * r5.xyz;
  r2.xyz = r3.yzw * (r0.x * contact_shadow.visibility);
  r2.xyz = float3(1, 1, 1) * r2.xyz;
  r0.yzw = r4.xyz * (r0.y * contact_shadow.visibility);
  r0.yzw = float3(1, 1, 1) * r0.yzw;
  r0.x = max(g_psLight.m_lightParams[2].x, r0.x);
  r0.x = g_psLight.m_lightParams[2].y * r0.x;
  r2.xyz = float3(-0, -0, -0) + r2.xyz;
  r1.xyz = r1.xyz * r0.xxx;
#ifdef MGSV_CAPTURE_ENVIRONMENT_SPECULAR
  // Exact environment contribution, before it is added to direct specular.
  const float3 environment_specular = r1.xyz;
#endif
  ApplyBendDebug(contact_shadow, r2.xyz, r0.yzw);
  r0.xyz = r1.xyz + r0.yzw;
  r0.w = 1;
  r2.w = 1;
  o0.xyzw = r2.xyzw;
  o1.xyzw = r0.xyzw;
  // Only replace diffuse RGB. Preserve alpha and the captured environment term.
  if (map_debug) {
    o0.rgb = PcssDebug(sun_pcss, uint(CUSTOM_SHADOW_MAP_DEBUG_VIEW), PCSS_MAX_RADIUS);
  }
#ifdef MGSV_CAPTURE_ENVIRONMENT_SPECULAR
  o2 = float4(environment_specular, 1.0);
#endif
  return;
}
