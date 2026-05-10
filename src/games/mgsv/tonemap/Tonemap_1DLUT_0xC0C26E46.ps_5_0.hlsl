#include "./tonemap.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:32 2026

cbuffer cPSScene : register(b2)
{

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

cbuffer cPSMaterial : register(b4)
{

  struct
  {
    float4 m_materials[8];
  } g_psMaterial : packoffset(c0);

}

cbuffer cPSSystem : register(b0)
{

  struct
  {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem : packoffset(c0);

}

SamplerState g_samplerPoint_Wrap_s : register(s8);
SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inImage : register(t0);
Texture2D<float4> inColorLUT : register(t6);
Texture2D<float4> inBloom : register(t7);


// 3Dmigoto declarations
#define cmp -


void main(
  float2 v0 : TEXCOORD0,
  float4 v1 : SV_Position0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v1.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.zw = g_psSystem.m_renderBuffer.zw;
  r1.xy = float2(0.5,0.5) * r0.zw;
  r0.xy = r0.xy * r0.zw;
  r0.xy = r0.xy + r1.xy;
  r0.xy = r0.xy;
  r1.xy = v0.xy;
  r1.xy = r1.xy;
  r1.z = -r1.y;
  r0.zw = float2(0.5,0.5) * r1.xz;
  r0.zw = float2(0.5,0.5) + r0.zw;
  r0.zw = r0.zw;
  r0.xy = r0.xy;
  r0.zw = r0.zw;
  r0.xy = r0.xy;
  r0.zw = r0.zw;
  r1.xyz = inImage.Sample(g_samplerPoint_Wrap_s, r0.xy).xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz * r1.xyz;
  r0.xy = float2(4,4) * g_psSystem.m_renderBuffer.zw;
  r0.xy = float2(0.5,0.5) * r0.xy;
  r0.xy = r0.xy + r0.zw;
  r0.xyz = inBloom.Sample(g_samplerLinear_Clamp_s, r0.xy).xyz;
  r0.xyz = r0.xyz;
  r0.xyz = min(float3(0.5,0.5,0.5), r0.xyz);
  r2.xyz = r1.xyz + r0.xyz;
  r0.xyz = r1.xyz * r0.xyz;
  r0.xyz = -r0.xyz;
  r0.xyz = r2.xyz + r0.xyz;
  r0.xyz = max(float3(0,0,0), r0.xyz);
  r0.xyz = min(float3(1,1,1), r0.xyz);
  r1.xyz = max(float3(0.001953125,0.001953125,0.001953125), r0.xyz);
  r1.xyz = rsqrt(r1.xyz);
  r0.xyz = r1.xyz * r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r1.xyz = g_psMaterial.m_materials[0].xyz;
  r0.xyz = r1.xyz * r0.xyz;
  r0.w = 0;
  r1.x = dot(r0.xyzw, g_psScene.m_shadowProjection._m00_m10_m20_m30);
  r1.y = dot(r0.xyzw, g_psScene.m_shadowProjection._m01_m11_m21_m31);
  r1.z = dot(r0.xyzw, g_psScene.m_shadowProjection._m02_m12_m22_m32);
  r1.xyz = r1.xyz;
  r0.x = 0.9921875 * r1.x;
  r0.xy = float2(0.00390625,0.00390625) + r0.xx;
  r0.x = inColorLUT.Sample(g_samplerLinear_Clamp_s, r0.xy).x;
  r0.x = r0.x;
  r0.w = 0.9921875 * r1.y;
  r1.xy = float2(0.00390625,0.00390625) + r0.ww;
  r0.y = inColorLUT.Sample(g_samplerLinear_Clamp_s, r1.xy).y;
  r0.y = r0.y;
  r0.w = 0.9921875 * r1.z;
  r1.xy = float2(0.00390625,0.00390625) + r0.ww;
  r0.z = inColorLUT.Sample(g_samplerLinear_Clamp_s, r1.xy).z;
  r0.z = r0.z;
  r0.x = r0.x;
  r0.y = r0.y;
  r0.z = r0.z;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r1.xyz = r0.xyz;
  r0.w = dot(r1.xyz, float3(0.298999995,0.587000012,0.114));
  r0.w = r0.w;
  o0.xyz = r0.xyz;
  o0.w = r0.w;
  return;
}