#include "../tonemap.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:27 2026

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
Texture2D<float4> inMesh : register(t8);
Texture2D<float4> g_tex_fog : register(t12);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float4 v1 : TEXCOORD0,
  float4 v2 : TEXCOORD1,
  float4 v3 : TEXCOORD2,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2,r3,r4;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.zw = float2(0.125,0.125) * r0.xy;
  r0.zw = frac(r0.zw);
  r0.zw = r0.zw;
  r0.xy = r0.xy / g_psSystem.m_renderInfo.xy;
  r0.xy = float2(2,-2) * r0.xy;
  r0.xy = float2(-1,1) + r0.xy;
  r0.xy = r0.xy;
  r1.xyz = v1.xyz;
  r1.xyz = r1.xyz;
  r1.w = g_psMaterial.m_materials[2].y;
  r2.x = g_psMaterial.m_materials[2].w;
  r2.y = g_psMaterial.m_materials[6].z;
  r2.z = dot(r1.xyz, r1.xyz);
  r2.z = rsqrt(r2.z);
  r1.xyz = r2.zzz * r1.xyz;
  r1.w = 1 / r1.w;
  r2.x = g_psScene.m_eyepos.y * r2.x;
  r2.y = -r2.y;
  r2.y = r2.x + r2.y;
  r2.xz = float2(0,0);
  r2.xyz = r2.xyz * r1.www;
  r1.xyz = r2.xyz + r1.xyz;
  r1.x = dot(r1.xyz, r1.xyz);
  r1.x = rsqrt(r1.x);
  r1.x = r1.y * r1.x;
  r1.x = max(0, r1.x);
  r1.x = r1.x;
  r1.x = r1.x;
  r0.xy = r0.xy;
  r1.x = r1.x;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = float2(0.0146484375,0.123046875) * r0.xy;
  r0.xy = float2(0.015625,0.125) + r0.xy;
  r0.xy = float2(0.96875,0.75) + r0.xy;
  r1.yzw = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r0.xy).xyz;
  r1.yzw = r1.yzw;
  r0.x = g_psScene.m_fogParam[1].y;
  r1.yzw = r1.yzw * r0.xxx;
  r1.yzw = r1.yzw;
  r1.yzw = r1.yzw;
  r1.yzw = r1.yzw;
  r0.x = g_psMaterial.m_materials[1].w;
  r0.y = g_psMaterial.m_materials[5].w;
  r2.x = g_psMaterial.m_materials[6].x;
  r2.y = g_psMaterial.m_materials[6].y;
  r1.x = -1 + r1.x;
  r0.x = r0.x;
  r0.y = r0.y;
  r2.x = r2.x;
  r2.y = r2.y;
  r1.x = r2.y * r1.x;
  r1.x = max(0, r1.x);
  r1.x = min(1, r1.x);
  r2.x = r2.x * r1.x;
  r1.x = r2.x + r1.x;
  r2.x = 1 + r2.x;
  r1.x = r1.x / r2.x;
  r0.y = r1.x * r0.y;
  r0.x = r0.x + r0.y;
  r0.x = r0.x;
  r0.x = r0.x;
  r1.xyz = r1.yzw * r0.xxx;
  r1.xyz = float3(0,0,0) + r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r0.zw = r0.zw;
  r1.xyz = r1.xyz;
  r0.zw = r0.zw;
  r0.x = inMesh.Sample(g_samplerPoint_Wrap_s, r0.zw).x;
  r0.x = -0.5 + r0.x;
  r0.x = r0.x / 128;
  r0.xyz = r1.xyz + r0.xxx;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r1.xyz = g_psMaterial.m_materials[1].xyz;
  r1.xyz = r1.xyz;
  r0.xyz = r0.xyz;
  r2.xyz = r1.yyy;
  r2.xzw = cmp(r2.xyz >= r0.xyz);
  r2.xzw = r2.xzw ? float3(1,1,1) : float3(0,0,0);
  r3.xyz = r2.xzw * r0.xyz;
  r2.xzw = -r2.xzw;
  r2.xzw = float3(1,1,1) + r2.xzw;
  r0.xyz = r0.xyz + r1.zzz;
  r4.xyz = -r2.yyy;
  r0.xyz = r4.xyz + r0.xyz;
  r0.xyz = r1.xxx * r0.xyz;
  r0.xyz = float3(-1,-1,-1) / r0.xyz;
  r0.xyz = r0.xyz + r1.zzz;
  r0.xyz = r0.xyz + r2.yyy;
  r0.xyz = r2.xzw * r0.xyz;
  r0.xyz = r3.xyz + r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = max(float3(0,0,0), r0.xyz);
  r0.xyz = min(float3(1,1,1), r0.xyz);
  r0.xyz = r0.xyz;
  r1.xyz = cmp(float3(0.00313080009,0.00313080009,0.00313080009) >= r0.xyz);
  r1.xyz = r1.xyz ? float3(1,1,1) : float3(0,0,0);
  r2.xyz = float3(12.9200001,12.9200001,12.9200001) * r0.xyz;
  r2.xyz = r2.xyz * r1.xyz;
  r1.xyz = -r1.xyz;
  r1.xyz = float3(1,1,1) + r1.xyz;
  r0.xyz = max(float3(9.99999975e-06,9.99999975e-06,9.99999975e-06), r0.xyz);
  r0.xyz = log2(r0.xyz);
  r0.xyz = float3(0.416666657,0.416666657,0.416666657) * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  r0.xyz = float3(1.05499995,1.05499995,1.05499995) * r0.xyz;
  r0.xyz = float3(-0.0549999997,-0.0549999997,-0.0549999997) + r0.xyz;
  r0.xyz = r1.xyz * r0.xyz;
  r0.xyz = r2.xyz + r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = 0;
  o0.xyzw = r0.xyzw;
  return;
}