#include "../tonemap.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:20 2026

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

SamplerState g_samplerLinear_Wrap_s : register(s10);
SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inDomeTexture : register(t0);
Texture2D<float4> inLowCylinderTexture : register(t1);
Texture2D<float4> inMidCylinderTexture : register(t2);
Texture2D<float4> g_tex_fog : register(t12);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : COLOR0,
  float4 v1 : TEXCOORD1,
  float4 v2 : TEXCOORD2,
  float3 v3 : TEXCOORD3,
  float4 v4 : SV_Position0,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1)
{
  float4 r0,r1,r2,r3,r4,r5,r6;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = float4(-0.5,-0.5,-0.5,-0.5) + v4.xyxy;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw / g_psSystem.m_renderInfo.xyxy;
  r0.xyzw = float4(2,-2,2,-2) * r0.xyzw;
  r0.xyzw = float4(-1,1,-1,1) + r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r1.x = v3.z;
  r0.xyzw = r0.xyzw;
  r1.x = r1.x;
  r0.xyzw = r0.xyzw;
  r1.x = r1.x;
  r0.xyzw = r0.xyzw;
  r1.x = r1.x;
  r1.x = r1.x;
  r1.y = g_psScene.m_fogParam[1].x;
  r1.x = log2(r1.x);
  r1.x = r1.y * r1.x;
  r1.x = r1.x;
  r1.x = max(0, r1.x);
  r1.x = min(1, r1.x);
  r1.x = 127 * r1.x;
  r0.xyzw = float4(0.0146484375,0.123046875,0.0146484375,0.123046875) * r0.xyzw;
  r0.xyzw = float4(0.015625,0.125,0.015625,0.125) + r0.xyzw;
  r1.y = 1 + r1.x;
  r1.y = max(0, r1.y);
  r1.w = min(127, r1.y);
  r1.y = r1.x;
  r1.yz = floor(r1.yw);
  r1.yz = r1.yz / float2(32,32);
  r2.xy = frac(r1.yz);
  r2.xz = float2(32,32) * r2.xy;
  r2.yw = floor(r1.yz);
  r2.xyzw = float4(0.03125,0.25,0.03125,0.25) * r2.xyzw;
  r0.xyzw = r2.xyzw + r0.xyzw;
  r1.xyzw = frac(r1.xxxx);
  r2.xyzw = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r0.xy).xyzw;
  r0.xyzw = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r0.zw).xyzw;
  r3.xyzw = -r1.xyzw;
  r3.xyzw = float4(1,1,1,1) + r3.xyzw;
  r2.xyzw = r3.xyzw * r2.xyzw;
  r0.xyzw = r1.xyzw * r0.xyzw;
  r0.xyzw = r2.xyzw + r0.xyzw;
  r1.x = g_psScene.m_fogParam[1].y;
  r0.xyz = r1.xxx * r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = r0.w;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r1.xyz = v0.xyz;
  r0.xyzw = r0.xyzw;
  r2.xyzw = v1.wxyz;
  r3.xyz = v2.zwx;
  r4.xy = v3.xy;
  r1.xyz = r1.xyz;
  r0.xyzw = r0.xyzw;
  r2.xyzw = r2.xyzw;
  r3.xyz = r3.xyz;
  r4.xy = r4.xy;
  r3.xy = r3.xy;
  r2.x = r2.x;
  r3.xy = g_psMaterial.m_materials[1].xy + r3.xy;
  r5.xyzw = inDomeTexture.Sample(g_samplerLinear_Wrap_s, r3.xy).xyzw;
  r5.xyzw = r5.xyzw * r2.xxxx;
  r3.xy = r2.yz;
  r6.x = g_psMaterial.m_materials[1].z;
  r6.y = 0;
  r3.xy = r6.xy + r3.xy;
  r6.xyzw = inLowCylinderTexture.Sample(g_samplerLinear_Wrap_s, r3.xy).xyzw;
  r1.w = -r2.x;
  r1.w = 1 + r1.w;
  r6.xyzw = r6.xyzw * r1.wwww;
  r5.xyzw = r6.xyzw + r5.xyzw;
  r2.yw = r2.yw;
  r3.x = g_psMaterial.m_materials[1].w;
  r3.y = 0;
  r2.xy = r3.xy + r2.yw;
  r2.xyzw = inMidCylinderTexture.Sample(g_samplerLinear_Wrap_s, r2.xy).xyzw;
  r3.z = r3.z;
  r2.xyzw = r3.zzzz * r2.xyzw;
  r1.w = -r2.w;
  r1.w = 1 + r1.w;
  r3.xyz = r5.xyz * r1.www;
  r2.xyz = r2.xyz * r2.www;
  r2.xyz = r3.xyz + r2.xyz;
  r1.w = r5.w + r2.w;
  r4.x = r4.x;
  r3.xyz = g_psMaterial.m_materials[4].xyz;
  r3.xyz = r3.xyz * r4.xxx;
  r2.w = -r4.x;
  r2.w = 1 + r2.w;
  r2.xyz = r2.xyz * r2.www;
  r2.xyz = r3.xyz + r2.xyz;
  r1.w = min(1, r1.w);
  r2.w = g_psMaterial.m_materials[0].x;
  r3.x = 1 + r4.y;
  r2.w = r3.x * r2.w;
  r2.w = g_psScene.m_exposure.z * r2.w;
  r2.xyz = r2.xyz * r2.www;
  r2.w = -r1.w;
  r2.w = 1 + r2.w;
  r1.xyz = r2.www * r1.xyz;
  r2.xyz = r2.xyz * r1.www;
  r1.xyz = r2.xyz + r1.xyz;
  r0.xyz = r0.xyz;
  r2.xyz = cmp(float3(0.00313080009,0.00313080009,0.00313080009) >= r0.xyz);
  r2.xyz = r2.xyz ? float3(1,1,1) : float3(0,0,0);
  r3.xyz = float3(12.9200001,12.9200001,12.9200001) * r0.xyz;
  r3.xyz = r3.xyz * r2.xyz;
  r2.xyz = -r2.xyz;
  r2.xyz = float3(1,1,1) + r2.xyz;
  r0.xyz = max(float3(9.99999975e-06,9.99999975e-06,9.99999975e-06), r0.xyz);
  r0.xyz = log2(r0.xyz);
  r0.xyz = float3(0.416666657,0.416666657,0.416666657) * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  r0.xyz = float3(1.05499995,1.05499995,1.05499995) * r0.xyz;
  r0.xyz = float3(-0.0549999997,-0.0549999997,-0.0549999997) + r0.xyz;
  r0.xyz = r2.xyz * r0.xyz;
  r0.xyz = r3.xyz + r0.xyz;
  r0.xyz = r0.xyz;
  r1.xyz = r1.xyz * r0.www;
  r0.xyz = r1.xyz + r0.xyz;
  r1.xyz = g_psMaterial.m_materials[5].xyz;
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
  r0.xyz = r0.xyz;
  r0.w = 0;
  o0.xyzw = r0.xyzw;
  o1.xyzw = r0.xyzw;
  return;
}