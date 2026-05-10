#include "../tonemap.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:26 2026

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
  }
g_psScene:
  packoffset(c0);
}

cbuffer cPSMaterial : register(b4) {
  struct
  {
    float4 m_materials[8];
  }
g_psMaterial:
  packoffset(c0);
}

cbuffer cPSSystem : register(b0) {
  struct
  {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  }
g_psSystem:
  packoffset(c0);
}

SamplerState g_samplerPoint_Wrap_s : register(s8);
SamplerState g_samplerPoint_Clamp_s : register(s9);
SamplerState g_samplerLinear_Wrap_s : register(s10);
SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inAlbedo : register(t8);
Texture2D<float4> inLightDiffuse : register(t9);
Texture2D<float4> inLightSpecular : register(t10);
Texture2D<float4> inDepth : register(t11);
Texture2D<float4> g_tex_fog : register(t12);
Texture2D<float4> inOcclusion : register(t13);
Texture2D<float4> inMesh : register(t15);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position0,
    float2 v1: TEXCOORD0,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2, r3, r4, r5, r6;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5, -0.5) + v0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.zw = float2(0.49609375, 0.49609375) + r0.xy;
  r0.zw = g_psSystem.m_renderBuffer.zw * r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r1.xy = g_psScene.m_projectionParam.zw;
  r1.z = inDepth.Sample(g_samplerPoint_Clamp_s, r0.zw).x;
  r1.z = r1.z;
  r1.z = r1.z;
  r1.xy = r1.xy;
  r1.y = -r1.y;
  r1.y = r1.z + r1.y;
  r1.x = r1.x / r1.y;
  r1.x = r1.x;
  r1.x = r1.x;
  r1.x = r1.x;
  r1.x = r1.x;
  r1.yzw = inLightSpecular.Sample(g_samplerPoint_Wrap_s, r0.zw).xyz;
  r1.yzw = r1.yzw;
  r1.yzw = r1.yzw;
  r1.yzw = r1.yzw;
  r1.yzw = r1.yzw;
  r1.yzw = r1.yzw;
  r2.xyz = inLightDiffuse.Sample(g_samplerPoint_Wrap_s, r0.zw).xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r1.yzw = r1.yzw;
  r2.xyz = r2.xyz;
  r1.yzw = r1.yzw;
  r3.xyz = inAlbedo.Sample(g_samplerLinear_Wrap_s, r0.zw).xyz;
  r3.xyz = r3.xyz;
  r0.z = inOcclusion.Sample(g_samplerLinear_Wrap_s, r0.zw).x;
  r4.xyz = r0.zzz;
  r2.xyz = r3.xyz * r2.xyz;
  r1.yzw = r2.xyz + r1.yzw;
  r1.yzw = r1.yzw * r4.xyz;
  r1.yzw = r1.yzw;
  r1.yzw = r1.yzw;
  r2.xyzw = v1.xyxy;
  r1.x = r1.x;
  r1.yzw = r1.yzw;
  r2.xyzw = r2.xyzw;
  r1.x = r1.x;
  r2.xyzw = r2.xyzw;
  r1.x = r1.x;
  r2.xyzw = r2.xyzw;
  r1.x = r1.x;
  r1.x = r1.x;
  r0.z = g_psScene.m_fogParam[1].x;
  r0.w = log2(r1.x);
  r0.z = r0.z * r0.w;
  r0.z = r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = 127 * r0.z;
  r2.xyzw = float4(0.0146484375, 0.123046875, 0.0146484375, 0.123046875) * r2.xyzw;
  r2.xyzw = float4(0.015625, 0.125, 0.015625, 0.125) + r2.xyzw;
  r0.w = 1 + r0.z;
  r0.w = max(0, r0.w);
  r3.w = min(127, r0.w);
  r3.y = r0.z;
  r3.xy = floor(r3.yw);
  r3.xy = r3.xy / float2(32, 32);
  r3.zw = frac(r3.xy);
  r4.xz = float2(32, 32) * r3.zw;
  r4.yw = floor(r3.xy);
  r3.xyzw = float4(0.03125, 0.25, 0.03125, 0.25) * r4.xyzw;
  r2.xyzw = r3.xyzw + r2.xyzw;
  r3.xyzw = frac(r0.zzzz);
  r4.xyzw = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r2.xy).xyzw;
  r2.xyzw = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r2.zw).xyzw;
  r5.xyzw = -r3.xyzw;
  r5.xyzw = float4(1, 1, 1, 1) + r5.xyzw;
  r4.xyzw = r5.xyzw * r4.xyzw;
  r2.xyzw = r3.xyzw * r2.xyzw;
  r2.xyzw = r4.xyzw + r2.xyzw;
  r0.z = g_psScene.m_fogParam[1].y;
  r2.xyz = r2.xyz * r0.zzz;
  r2.xyz = r2.xyz;
  r2.w = r2.w;
  r2.xyzw = r2.xyzw;
  r2.xyzw = r2.xyzw;
  r1.xyz = r2.www * r1.yzw;
  r1.xyz = r1.xyz + r2.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r0.z = dot(r1.xyz, float3(0.212500006, 0.715399981, 0.0720999986));
  r0.z = 0.03125 * r0.z;
  r0.w = max(0.001953125, r0.z);
  r0.w = rsqrt(r0.w);
  r2.w = r0.z * r0.w;
  r3.xyz = g_psMaterial.m_materials[0].xyz;
  r1.xyz = r1.xyz;

  float3 untonemapped = r1.rgb;
  // TppTonemap: apply the per-channel Fox Engine tonemap curve.
  r4.xyz = r3.yyy;
  r4.xzw = cmp(r4.xyz >= r1.xyz);
  r4.xzw = r4.xzw ? float3(1, 1, 1) : float3(0, 0, 0);
  r5.xyz = r4.xzw * r1.xyz;
  r4.xzw = -r4.xzw;
  r4.xzw = float3(1, 1, 1) + r4.xzw;
  r1.xyz = r1.xyz + r3.zzz;
  r6.xyz = -r4.yyy;
  r1.xyz = r6.xyz + r1.xyz;
  r1.xyz = r3.xxx * r1.xyz;
  r1.xyz = float3(-1, -1, -1) / r1.xyz;
  r1.xyz = r1.xyz + r3.zzz;
  r1.xyz = r1.xyz + r4.yyy;
  r1.xyz = r4.xzw * r1.xyz;
  r1.xyz = r5.xyz + r1.xyz;
  r1.xyz = r1.xyz;

  if (RENODX_TONE_MAP_TYPE != 0.f) {
    r1.rgb = untonemapped;
  }
  r1 = max(0, r1);

  r0.xy = float2(0.125, 0.125) * r0.xy;
  r0.xy = frac(r0.xy);
  r0.x = inMesh.Sample(g_samplerPoint_Wrap_s, r0.xy).x;
  r0.x = -0.5 + r0.x;
  r0.x = r0.x / 128;
  r2.xyz = r1.xyz + r0.xxx;
  r2.xyz = r2.xyz;
  r2.w = r2.w;
  o0.xyzw = r2.xyzw;
  return;
}
