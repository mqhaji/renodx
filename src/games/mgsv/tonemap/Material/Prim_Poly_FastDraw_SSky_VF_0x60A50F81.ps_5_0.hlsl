#include "../tonemap.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:26 2026

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

cbuffer cPSObject : register(b5)
{

  struct
  {
    float4x4 m_viewWorld;
    float4x4 m_world;
    float4 m_useWeightCount;
    float4 m_localParam[4];
  } g_psObject : packoffset(c0);

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

SamplerState inTextureSampler_s : register(s0);
SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inTexture : register(t0);
Texture2D<float4> g_tex_fog : register(t12);


// 3Dmigoto declarations
#define cmp -


void main(
  float2 v0 : TEXCOORD0,
  float w0 : TEXCOORD3,
  float4 v1 : TEXCOORD1,
  float4 v2 : TEXCOORD2,
  float4 v3 : SV_Position0,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1)
{
  float4 r0,r1,r2,r3,r4,r5;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = float4(-0.5,-0.5,-0.5,-0.5) + v3.xyxy;
  r1.xyz = v1.xyz;
  r2.xy = v2.xy;
  r1.xyz = r1.xyz;
  r2.xy = r2.xy;
  r3.xyzw = inTexture.Sample(inTextureSampler_s, v0.xy).xyzw;
  r3.xyz = r3.xyz / r3.www;
  r1.xyz = r3.xyz * r1.xyz;
  r2.xy = r2.xy;
  r1.w = r2.x * r2.y;
  r1.xyz = r1.xyz * r1.www;
  r1.xyz = max(float3(0,0,0), r1.xyz);
  r1.xyz = min(float3(154.600006,154.600006,154.600006), r1.xyz);
  r1.xyz = log2(r1.xyz);
  r1.xyz = float3(2.20000005,2.20000005,2.20000005) * r1.xyz;
  r1.xyz = exp2(r1.xyz);
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r0.xyzw = r0.xyzw;
  r1.w = v2.z;
  r2.x = w0.x;
  r1.xyz = r1.xyz;
  r0.xyzw = r0.xyzw;
  r1.w = r1.w;
  r2.x = r2.x;
  r0.xyzw = r0.xyzw;
  r1.w = r1.w;
  r3.xyzw = g_psSystem.m_renderInfo.xyxy;
  r0.xyzw = r0.xyzw / r3.xyzw;
  r0.xyzw = float4(2,-2,2,-2) * r0.xyzw;
  r0.xyzw = float4(-1,1,-1,1) + r0.xyzw;
  r0.xyzw = r0.xyzw;
  r1.w = r1.w;
  r0.xyzw = r0.xyzw;
  r1.w = r1.w;
  r1.w = r1.w;
  r2.y = g_psScene.m_fogParam[1].x;
  r1.w = log2(r1.w);
  r1.w = r2.y * r1.w;
  r1.w = r1.w;
  r1.w = max(0, r1.w);
  r1.w = min(1, r1.w);
  r1.w = 127 * r1.w;
  r0.xyzw = float4(0.0146484375,0.123046875,0.0146484375,0.123046875) * r0.xyzw;
  r0.xyzw = float4(0.015625,0.125,0.015625,0.125) + r0.xyzw;
  r2.y = 1 + r1.w;
  r2.y = max(0, r2.y);
  r2.w = min(127, r2.y);
  r2.y = r1.w;
  r2.yz = floor(r2.yw);
  r2.yz = r2.yz / float2(32,32);
  r3.xy = frac(r2.yz);
  r3.xz = float2(32,32) * r3.xy;
  r3.yw = floor(r2.yz);
  r3.xyzw = float4(0.03125,0.25,0.03125,0.25) * r3.xyzw;
  r0.xyzw = r3.xyzw + r0.xyzw;
  r3.xyzw = frac(r1.wwww);
  r4.xyzw = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r0.xy).xyzw;
  r0.xyzw = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r0.zw).xyzw;
  r5.xyzw = -r3.xyzw;
  r5.xyzw = float4(1,1,1,1) + r5.xyzw;
  r4.xyzw = r5.xyzw * r4.xyzw;
  r0.xyzw = r3.xyzw * r0.xyzw;
  r0.xyzw = r4.xyzw + r0.xyzw;
  r1.w = g_psScene.m_fogParam[1].y;
  r0.xyz = r1.www * r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = r0.w;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyz = r0.xyz * r2.xxx;
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r0.w = r0.w * r2.x;
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r1.xyz = r1.xyz;
  r0.xyz = r0.xyz;
  r0.w = r0.w;
  r1.w = g_psObject.m_localParam[3].y;
  r1.w = r1.w;
  r1.w = cmp(r1.w < 0);
  r2.xyz = r1.www ? float3(1,1,1) : float3(0,0,0);
  r2.xyz = r2.xyz;
  r1.w = -r0.w;
  r1.w = 1 + r1.w;
  r1.w = r2.z * r1.w;
  r1.w = -r1.w;
  r1.w = 1 + r1.w;
  r3.xyz = r1.xyz * r0.www;
  r0.xyz = r3.xyz + r0.xyz;
  r3.xyz = -r0.xyz;
  r1.xyz = r3.xyz + r1.xyz;
  r1.xyz = r2.xyz * r1.xyz;
  r0.xyz = r1.xyz + r0.xyz;
  r0.w = 1 * r1.w;
  r0.xyz = r0.xyz;
  r0.w = r0.w;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.w = r0.w;
  r0.w = r0.w;
  r1.x = g_psObject.m_localParam[3].z;
  r1.x = r1.x;
  r1.x = cmp(r1.x < 0);
  r1.x = r1.x ? 1 : 0;
  r1.x = cmp(r1.x == 1.000000);
  if (r1.x != 0) {
    r1.x = -0.496078432 + r0.w;
    r1.x = cmp(r1.x < 0);
    // ASM: and r1.x, r1.x, l(-1)
    // r1.x = r1.x ? -nan : 0;
    r1.x = -r1.x;
    if (r1.x != 0) discard;
  }
  r0.w = r0.w;
  r0.w = r0.w;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  
#if 1
  r1.rgb = ApplyTppTonemap(r0.rgb, g_psMaterial.m_materials[0].xyz);
#else
  r1.xyz = g_psMaterial.m_materials[0].xyz;
  r0.xyz = r0.xyz;
  r2.xyz = r1.yyy;
  r2.xzw = cmp(r2.xyz >= r0.xyz);
  r2.xzw = r2.xzw ? float3(1,1,1) : float3(0,0,0);
  r3.xyz = r2.xzw * r0.xyz;
  r2.xzw = -r2.xzw;
  r2.xzw = float3(1,1,1) + r2.xzw;
  r4.xyz = r0.xyz + r1.zzz;
  r5.xyz = -r2.yyy;
  r4.xyz = r5.xyz + r4.xyz;
  r1.xyw = r4.xyz * r1.xxx;
  r1.xyw = float3(-1,-1,-1) / r1.xyw;
  r1.xyz = r1.xyw + r1.zzz;
  r1.xyz = r1.xyz + r2.yyy;
  r1.xyz = r2.xzw * r1.xyz;
  r1.xyz = r3.xyz + r1.xyz;
#endif

  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r2.xyz = cmp(float3(0.00313080009,0.00313080009,0.00313080009) >= r1.xyz);
  r2.xyz = r2.xyz ? float3(1,1,1) : float3(0,0,0);
  r3.xyz = float3(12.9200001,12.9200001,12.9200001) * r1.xyz;
  r3.xyz = r3.xyz * r2.xyz;
  r2.xyz = -r2.xyz;
  r2.xyz = float3(1,1,1) + r2.xyz;
  r1.xyz = max(float3(9.99999975e-06,9.99999975e-06,9.99999975e-06), r1.xyz);
  r1.xyz = log2(r1.xyz);
  r1.xyz = float3(0.416666657,0.416666657,0.416666657) * r1.xyz;
  r1.xyz = exp2(r1.xyz);
  r1.xyz = float3(1.05499995,1.05499995,1.05499995) * r1.xyz;
  r1.xyz = float3(-0.0549999997,-0.0549999997,-0.0549999997) + r1.xyz;
  r1.xyz = r2.xyz * r1.xyz;
  r0.xyz = r3.xyz + r1.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = r0.w;
  r0.xyz = r0.xyz;
  r0.w = r0.w;
  r0.xyzw = r0.xyzw;
  r0.xyz = r0.xyz * r0.www;
  r0.xyz = r0.xyz;
  r0.w = r0.w;
  o0.xyzw = r0.xyzw;
  o1.xyzw = r0.xyzw;
  return;
}