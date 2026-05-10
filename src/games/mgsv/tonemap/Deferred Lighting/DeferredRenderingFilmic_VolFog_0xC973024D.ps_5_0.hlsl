#include "../tonemap.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:32 2026

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

cbuffer cPSObject : register(b5) {
  struct
  {
    float4x4 m_viewWorld;
    float4x4 m_world;
    float4 m_useWeightCount;
    float4 m_localParam[4];
  }
g_psObject:
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

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position0,
    float2 v1: TEXCOORD0,
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
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.zw = g_psScene.m_projectionParam.zw;
  r1.x = inDepth.Sample(g_samplerPoint_Clamp_s, r0.xy).x;
  r1.x = r1.x;
  r1.x = r1.x;
  r0.zw = r0.zw;
  r0.w = -r0.w;
  r0.w = r1.x + r0.w;
  r0.z = r0.z / r0.w;
  r0.z = r0.z;
  r0.z = r0.z;
  r0.z = r0.z;
  r0.z = r0.z;
  r1.xyz = inLightSpecular.Sample(g_samplerPoint_Wrap_s, r0.xy).xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r2.xyz = inLightDiffuse.Sample(g_samplerPoint_Wrap_s, r0.xy).xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r1.xyz = r1.xyz;
  r2.xyz = r2.xyz;
  r1.xyz = r1.xyz;
  r3.xyz = inAlbedo.Sample(g_samplerLinear_Wrap_s, r0.xy).xyz;
  r3.xyz = r3.xyz;
  r0.x = inOcclusion.Sample(g_samplerLinear_Wrap_s, r0.xy).x;
  r0.xyw = r0.xxx;
  r2.xyz = r3.xyz * r2.xyz;
  r1.xyz = r2.xyz + r1.xyz;
  r0.xyw = r1.xyz * r0.xyw;
  r0.xyw = r0.xyw;
  r0.xyw = r0.xyw;
  r1.xyzw = v1.xyxy;
  r0.z = r0.z;
  r0.xyw = r0.xyw;
  r1.xyzw = r1.xyzw;
  r0.z = r0.z;
  r1.xyzw = r1.xyzw;
  r0.z = r0.z;
  r1.xyzw = r1.xyzw;
  r0.z = r0.z;
  r0.z = r0.z;
  r2.x = g_psScene.m_fogParam[1].x;
  r0.z = log2(r0.z);
  r0.z = r2.x * r0.z;
  r0.z = r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = 127 * r0.z;
  r1.xyzw = float4(0.0146484375, 0.123046875, 0.0146484375, 0.123046875) * r1.xyzw;
  r1.xyzw = float4(0.015625, 0.125, 0.015625, 0.125) + r1.xyzw;
  r2.x = 1 + r0.z;
  r2.x = max(0, r2.x);
  r2.w = min(127, r2.x);
  r2.y = r0.z;
  r2.xy = floor(r2.yw);
  r2.xy = r2.xy / float2(32, 32);
  r2.zw = frac(r2.xy);
  r3.xz = float2(32, 32) * r2.zw;
  r3.yw = floor(r2.xy);
  r2.xyzw = float4(0.03125, 0.25, 0.03125, 0.25) * r3.xyzw;
  r1.xyzw = r2.xyzw + r1.xyzw;
  r2.xyzw = frac(r0.zzzz);
  r3.xyzw = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r1.xy).xyzw;
  r1.xyzw = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r1.zw).xyzw;
  r4.xyzw = -r2.xyzw;
  r4.xyzw = float4(1, 1, 1, 1) + r4.xyzw;
  r3.xyzw = r4.xyzw * r3.xyzw;
  r1.xyzw = r2.xyzw * r1.xyzw;
  r1.xyzw = r3.xyzw + r1.xyzw;
  r0.z = g_psScene.m_fogParam[1].y;
  r1.xyz = r1.xyz * r0.zzz;
  r1.xyz = r1.xyz;
  r1.w = r1.w;
  r1.xyzw = r1.xyzw;
  r1.xyzw = r1.xyzw;
  r0.xyz = r1.www * r0.xyw;
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
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r2.xyz = g_psObject.m_localParam[2].zzz;
  r0.w = g_psObject.m_localParam[1].x;
  r2.w = g_psObject.m_localParam[1].y;
  r3.x = g_psObject.m_localParam[1].z;
  r3.y = g_psObject.m_localParam[1].w;
  r3.z = g_psObject.m_localParam[2].x;
  r3.w = g_psObject.m_localParam[2].y;
  r2.xyz = r2.xyz;
  r0.w = r0.w;
  r2.w = r2.w;
  r3.x = r3.x;
  r3.y = r3.y;
  r3.z = r3.z;
  r3.w = r3.w;
  r4.xyz = r0.www * r2.xyz;
  r3.x = r3.x * r2.w;
  r5.xyz = r4.xyz + r3.xxx;
  r5.xyz = r5.xyz * r2.xyz;
  r4.w = r3.y * r3.z;
  r5.xyz = r5.xyz + r4.www;
  r4.xyz = r4.xyz + r2.www;
  r2.xyz = r4.xyz * r2.xyz;
  r3.y = r3.y * r3.w;
  r2.xyz = r3.yyy + r2.xyz;
  r2.xyz = r5.xyz / r2.xyz;
  r3.z = r3.z / r3.w;
  r4.xyz = -r3.zzz;
  r2.xyz = r4.xyz + r2.xyz;
  r2.xyz = float3(1, 1, 1) / r2.xyz;
  r0.xyz = r0.xyz;
  r5.xyz = r0.www * r0.xyz;
  r3.xzw = r5.xyz + r3.xxx;
  r3.xzw = r3.xzw * r0.xyz;
  r3.xzw = r3.xzw + r4.www;
  r5.xyz = r5.xyz + r2.www;
  r0.xyz = r5.xyz * r0.xyz;
  r0.xyz = r0.xyz + r3.yyy;
  r0.xyz = r3.xzw / r0.xyz;
  r0.xyz = r0.xyz + r4.xyz;
  r1.xyz = r0.xyz * r2.xyz;
  r1 = max(0, r1);

  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.w = r1.w;
  o0.xyzw = r1.xyzw;
  return;
}
