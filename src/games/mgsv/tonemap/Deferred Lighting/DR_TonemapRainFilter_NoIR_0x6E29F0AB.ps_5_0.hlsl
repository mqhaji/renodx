#include "../tonemap.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:27 2026

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
  } g_psScene: packoffset(c0);
}

cbuffer cPSMaterial : register(b4) {
  struct
  {
    float4 m_materials[8];
  } g_psMaterial: packoffset(c0);
}

cbuffer cPSSystem : register(b0) {
  struct
  {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem: packoffset(c0);
}
// clang-format on

SamplerState g_samplerPoint_Wrap_s : register(s8);
SamplerState g_samplerPoint_Clamp_s : register(s9);
SamplerState g_samplerLinear_Wrap_s : register(s10);
SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inFloorNormalMap : register(t0);
TextureCube<float4> inReflectionCubeMap : register(t1);
Texture2D<float4> inSpecular : register(t3);
Texture2D<float4> inNormal : register(t4);
Texture2D<float4> inAlbedo : register(t8);
Texture2D<float4> inLightDiffuse : register(t9);
Texture2D<float4> inLightSpecular : register(t10);
Texture2D<float4> inDepth : register(t11);
Texture2D<float4> g_tex_fog : register(t12);
Texture2D<float4> inOcclusion : register(t13);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position0,
    float4 v1: TEXCOORD0,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2, r3, r4, r5;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5, -0.5) + v0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = float2(0.49609375, 0.49609375) + r0.xy;
  r0.xy = g_psSystem.m_renderBuffer.zw * r0.xy;
  r0.xy = r0.xy;
  r0.zw = v1.xy;
  r0.xy = r0.xy;
  r0.zw = r0.zw;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r1.xy = -g_psScene.m_cameraCenterOffset.xy;
  r0.zw = r1.xy + r0.zw;
  r1.xyzw = g_psScene.m_projectionParam.xyzw;
  r2.x = inDepth.SampleLevel(g_samplerPoint_Clamp_s, r0.xy, 0).x;
  r2.x = r2.x;
  r0.zw = r0.zw;
  r2.x = r2.x;
  r1.xyzw = r1.xyzw;
  r1.w = -r1.w;
  r1.w = r2.x + r1.w;
  r2.z = r1.z / r1.w;
  r1.z = r2.z;
  r0.zw = r1.xy * r0.zw;
  r2.xy = r0.zw * r1.zz;
  r1.z = r1.z;
  r1.z = r1.z;
  r1.z = r1.z;
  r1.z = r1.z;
  r1.z = r1.z;
  r1.z = r1.z;
  r2.w = 1;
  r3.x = dot(r2.xyzw, g_psScene.m_view._m00_m10_m20_m30);
  r3.y = dot(r2.xyzw, g_psScene.m_view._m01_m11_m21_m31);
  r3.z = dot(r2.xyzw, g_psScene.m_view._m02_m12_m22_m32);
  r3.xyz = r3.xyz;
  r0.zw = g_psMaterial.m_materials[4].xy;
  r1.xy = max(g_psMaterial.m_materials[2].xz, r1.zz);
  r1.xy = min(g_psMaterial.m_materials[2].yw, r1.xy);
  r2.xy = -g_psMaterial.m_materials[2].xz;
  r1.xy = r2.xy + r1.xy;
  r0.zw = r1.xy * r0.zw;
  r1.x = cmp(r1.z >= g_psMaterial.m_materials[2].z);
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r0.z = r1.x ? r0.w : r0.z;
  r3.xyz = r3.xyz;
  r0.z = r0.z;
  r1.xy = r3.xz;
  r1.xy = r1.xy;
  r1.xy = g_psMaterial.m_materials[7].zw * r1.xy;
  r1.xy = g_psMaterial.m_materials[1].ww * r1.xy;
  r1.xy = g_psMaterial.m_materials[7].xy + r1.xy;
  r1.xy = r1.xy;
  r2.xyz = inNormal.SampleLevel(g_samplerPoint_Clamp_s, r0.xy, 0).xyz;
  r2.xyz = r2.xyz;
  r2.xy = float2(2, 2) * r2.xy;
  r2.xy = float2(-1, -1) + r2.xy;
  r0.w = r2.z * r2.z;
  r0.w = 2 * r0.w;
  r4.z = -1 + r0.w;
  r0.w = r4.z;
  r0.w = r0.w * r0.w;
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r2.zw = r2.xy * r0.ww;
  r1.w = dot(r2.xy, r2.xy);
  r0.w = r1.w * r0.w;
  r0.w = 1.00000001e-07 + r0.w;
  r0.w = rsqrt(r0.w);
  r4.xy = r2.zw * r0.ww;
  r4.w = 0;
  r2.x = dot(r4.xyzw, g_psScene.m_view._m00_m10_m20_m30);
  r2.y = dot(r4.xyzw, g_psScene.m_view._m01_m11_m21_m31);
  r2.z = dot(r4.xyzw, g_psScene.m_view._m02_m12_m22_m32);
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r3.xyz = r3.xyz;
  r1.xy = r1.xy;
  r2.xyz = r2.xyz;
  r0.z = r0.z;
  r3.xyz = r3.xyz;
  r1.xy = r1.xy;
  r2.xyz = r2.xyz;
  r0.z = r0.z;
  r0.w = r2.y;
  r1.w = 1 + r0.w;
  r1.w = max(0.25, r1.w);
  r1.w = min(1, r1.w);
  r2.w = cmp(r1.w >= 1);
  r2.w = r2.w ? 1 : 0;
  r2.w = -r2.w;
  r2.w = 1 + r2.w;
  r3.w = -0.25 + r1.w;
  r3.w = 1.33329999 * r3.w;
  r3.w = -r3.w;
  r3.w = 1 + r3.w;
  r2.w = r3.w * r2.w;
  r2.w = -r2.w;
  r1.w = r2.w + r1.w;
  r2.w = inSpecular.Sample(g_samplerLinear_Clamp_s, r0.xy).y;
  r2.w = r2.w;
  r0.z = r1.w * r0.z;
  r0.z = r0.z * r2.w;
  r0.z = 1 * r0.z;
  r4.xyz = -g_psScene.m_eyepos.xyz;
  r3.xyz = r4.xyz + r3.xyz;
  r1.w = dot(r3.xyz, r3.xyz);
  r1.w = rsqrt(r1.w);
  r3.xyz = r3.xyz * r1.www;
  r1.x = inFloorNormalMap.Sample(g_samplerLinear_Wrap_s, r1.xy).y;
  r1.x = r1.x;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r1.y = dot(r3.xyz, r2.xyz);
  r1.w = r1.y + r1.y;
  r1.w = -r1.w;
  r2.xyz = r2.xyz * r1.www;
  r2.xyz = r2.xyz + r3.xyz;
  r2.xyzw = inReflectionCubeMap.SampleBias(g_samplerLinear_Clamp_s, r2.xyz, -2).xyzw;
  r3.xyz = r2.www;
  r0.w = r1.x * r0.w;
  r4.xyz = -r2.xyz;
  r3.xyz = r4.xyz + r3.xyz;
  r3.xyz = r3.xyz * r0.www;
  r2.xyz = r3.xyz + r2.xyz;
  r2.xyz = r2.xyz * r2.xyz;
  r2.xyz = g_psMaterial.m_materials[1].xxx * r2.xyz;
  r2.xyz = g_psMaterial.m_materials[1].zzz * r2.xyz;
  r2.xyz = g_psMaterial.m_materials[1].yyy + r2.xyz;
  r2.xyz = max(float3(0, 0, 0), r2.xyz);
  r2.xyz = min(float3(1, 1, 1), r2.xyz);
  r0.w = -r1.y;
  r0.w = max(r1.y, r0.w);
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r0.w = r0.w * r0.w;
  r0.w = 0.25 + r0.w;
  r1.xyw = r2.xyz * r0.www;
  r0.w = inOcclusion.Sample(g_samplerLinear_Wrap_s, r0.xy).x;
  r2.xyz = r0.www;
  r3.xyz = r2.xyz * r0.zzz;
  r1.xyw = r3.xyz * r1.xyw;
  r1.xyw = max(float3(0, 0, 0), r1.xyw);
  r1.xyw = min(float3(1, 1, 1), r1.xyw);
  r1.xyw = r1.xyw;
  r3.xyz = inLightSpecular.Sample(g_samplerPoint_Wrap_s, r0.xy).xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r4.xyz = inLightDiffuse.Sample(g_samplerPoint_Wrap_s, r0.xy).xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r3.xyz = r3.xyz;
  r4.xyz = r4.xyz;
  r3.xyz = r3.xyz;
  r0.xyz = inAlbedo.Sample(g_samplerLinear_Wrap_s, r0.xy).xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz * r4.xyz;
  r0.xyz = r0.xyz + r3.xyz;
  r0.xyz = r0.xyz * r2.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r1.xyw = r1.xyw;
  r0.xyz = r0.xyz;
  r1.xyw = r1.xyw;
  r0.xyz = r1.xyw + r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r2.xyzw = v1.xyxy;
  r0.xyz = r0.xyz;
  r2.xyzw = r2.xyzw;
  r2.xyzw = r2.xyzw;
  r1.z = r1.z;
  r2.xyzw = r2.xyzw;
  r1.z = r1.z;
  r1.z = r1.z;
  r0.w = g_psScene.m_fogParam[1].x;
  r1.x = log2(r1.z);
  r0.w = r1.x * r0.w;
  r0.w = r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r0.w = 127 * r0.w;
  r1.xyzw = float4(0.0146484375, 0.123046875, 0.0146484375, 0.123046875) * r2.xyzw;
  r1.xyzw = float4(0.015625, 0.125, 0.015625, 0.125) + r1.xyzw;
  r2.x = 1 + r0.w;
  r2.x = max(0, r2.x);
  r2.w = min(127, r2.x);
  r2.y = r0.w;
  r2.xy = floor(r2.yw);
  r2.xy = r2.xy / float2(32, 32);
  r2.zw = frac(r2.xy);
  r3.xz = float2(32, 32) * r2.zw;
  r3.yw = floor(r2.xy);
  r2.xyzw = float4(0.03125, 0.25, 0.03125, 0.25) * r3.xyzw;
  r1.xyzw = r2.xyzw + r1.xyzw;
  r2.xyzw = frac(r0.wwww);
  r3.xyzw = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r1.xy).xyzw;
  r1.xyzw = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r1.zw).xyzw;
  r4.xyzw = -r2.xyzw;
  r4.xyzw = float4(1, 1, 1, 1) + r4.xyzw;
  r3.xyzw = r4.xyzw * r3.xyzw;
  r1.xyzw = r2.xyzw * r1.xyzw;
  r1.xyzw = r3.xyzw + r1.xyzw;
  r0.w = g_psScene.m_fogParam[1].y;
  r1.xyz = r1.xyz * r0.www;
  r1.xyz = r1.xyz;
  r1.w = r1.w;
  r1.xyzw = r1.xyzw;
  r1.xyzw = r1.xyzw;
  r0.xyz = r1.www * r0.xyz;
  r0.xyz = r0.xyz + r1.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = dot(r0.xyz, float3(0.212500006, 0.715399981, 0.0720999986));
  r0.w = 0.03125 * r0.w;
  r1.x = max(0.001953125, r0.w);
  r1.x = rsqrt(r1.x);
  r1.w = r1.x * r0.w;

  // TppTonemap<0:NaN:Inf,1:NaN:Inf,2:NaN:Inf>
  float3 untonemapped = r0.rgb;
  if (RENODX_TONE_MAP_TYPE != 0.f) {
    r1.rgb = untonemapped;
    r1.rgb = max(0, r1.rgb);
  } else {
    r2.xyz = g_psMaterial.m_materials[0].xyz;
    r0.xyz = r0.xyz;
    r3.xyz = r2.yyy;
    r3.xzw = cmp(r3.xyz >= r0.xyz);
    r3.xzw = r3.xzw ? float3(1, 1, 1) : float3(0, 0, 0);
    r4.xyz = r3.xzw * r0.xyz;
    r3.xzw = -r3.xzw;
    r3.xzw = float3(1, 1, 1) + r3.xzw;
    r0.xyz = r0.xyz + r2.zzz;
    r5.xyz = -r3.yyy;
    r0.xyz = r5.xyz + r0.xyz;
    r0.xyz = r2.xxx * r0.xyz;
    r0.xyz = float3(-1, -1, -1) / r0.xyz;
    r0.xyz = r0.xyz + r2.zzz;
    r0.xyz = r0.xyz + r3.yyy;
    r0.xyz = r3.xzw * r0.xyz;
    r1.xyz = r4.xyz + r0.xyz;
  }

  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.w = r1.w;
  o0.xyzw = r1.xyzw;

#if FIX_UNORM_SRGB
  o0.rgb = renodx::color::srgb::Encode(max(0, o0.rgb));
#endif

  return;
}
