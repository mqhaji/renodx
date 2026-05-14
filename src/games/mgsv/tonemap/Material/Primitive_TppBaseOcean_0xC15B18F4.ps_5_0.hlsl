#include "../tonemap.hlsli"

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

SamplerState g_samplerPoint_Clamp_s : register(s9);
SamplerState g_samplerLinear_Wrap_s : register(s10);
SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inWhitecapTexture1 : register(t2);
Texture2D<float4> g_tex_fog : register(t12);
Texture2D<float4> inDepthTexture : register(t13);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float3 v1 : TEXCOORD0,
  float w1 : TEXCOORD1,
  float4 v2 : TEXCOORD3,
  float4 v3 : TEXCOORD4,
  float4 v4 : TEXCOORD6,
  float4 v5 : TEXCOORD7,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1)
{
  float4 r0,r1,r2,r3,r4,r5,r6;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = float4(-0.5,-0.5,-0.5,-0.5) + v0.xyxy;
  r1.xy = r0.zw;
  r1.xy = r1.xy;
  r1.xy = float2(0.49609375,0.49609375) + r1.xy;
  r1.xy = g_psSystem.m_renderBuffer.zw * r1.xy;
  r1.xy = r1.xy;
  r1.xy = r1.xy;
  r1.xy = r1.xy;
  r1.xy = r1.xy;
  r1.zw = g_psScene.m_projectionParam.zw;
  r1.x = inDepthTexture.Sample(g_samplerPoint_Clamp_s, r1.xy).y;
  r1.x = r1.x;
  r1.x = r1.x;
  r1.zw = r1.zw;
  r1.y = -r1.w;
  r1.x = r1.x + r1.y;
  r1.x = r1.z / r1.x;
  r1.x = r1.x;
  r1.x = r1.x;
  r1.x = r1.x;
  r1.yzw = v1.xyz;
  r1.yzw = r1.yzw;
  r2.xyz = -g_psScene.m_eyepos.xyz;
  r1.yzw = r2.xyz + r1.yzw;
  r1.z = dot(r1.yzw, r1.yzw);
  r1.z = rsqrt(r1.z);
  r2.xz = r1.yw * r1.zz;
  r2.xz = r2.xz;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw / g_psSystem.m_renderInfo.xyxy;
  r0.xyzw = float4(2,-2,2,-2) * r0.xyzw;
  r0.xyzw = float4(-1,1,-1,1) + r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r1.y = w1.x;
  r0.xyzw = r0.xyzw;
  r1.y = r1.y;
  r0.xyzw = r0.xyzw;
  r1.y = r1.y;
  r0.xyzw = r0.xyzw;
  r1.y = r1.y;
  r1.y = r1.y;
  r1.z = g_psScene.m_fogParam[1].x;
  r1.y = log2(r1.y);
  r1.y = r1.z * r1.y;
  r1.y = r1.y;
  r1.y = max(0, r1.y);
  r1.y = min(1, r1.y);
  r1.y = 127 * r1.y;
  r0.xyzw = float4(0.0146484375,0.123046875,0.0146484375,0.123046875) * r0.xyzw;
  r0.xyzw = float4(0.015625,0.125,0.015625,0.125) + r0.xyzw;
  r1.z = 1 + r1.y;
  r1.z = max(0, r1.z);
  r3.w = min(127, r1.z);
  r3.y = r1.y;
  r1.zw = floor(r3.yw);
  r1.zw = r1.zw / float2(32,32);
  r3.xy = frac(r1.zw);
  r3.xz = float2(32,32) * r3.xy;
  r3.yw = floor(r1.zw);
  r3.xyzw = float4(0.03125,0.25,0.03125,0.25) * r3.xyzw;
  r0.xyzw = r3.xyzw + r0.xyzw;
  r3.xyzw = frac(r1.yyyy);
  r4.xyzw = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r0.xy).xyzw;
  r0.xyzw = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r0.zw).xyzw;
  r5.xyzw = -r3.xyzw;
  r5.xyzw = float4(1,1,1,1) + r5.xyzw;
  r4.xyzw = r5.xyzw * r4.xyzw;
  r0.xyzw = r3.xyzw * r0.xyzw;
  r0.xyzw = r4.xyzw + r0.xyzw;
  r1.y = g_psScene.m_fogParam[1].y;
  r0.xyz = r1.yyy * r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = r0.w;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r2.xz = r2.xz;
  r1.yz = v1.xz;
  r0.xyzw = r0.xyzw;
  r2.xz = r2.xz;
  r1.yz = r1.yz;
  r3.xyz = g_psMaterial.m_materials[3].xyz;
  r4.xyz = g_psMaterial.m_materials[2].xyz;
  r5.xyz = g_psMaterial.m_materials[4].xyz;
  r1.yz = float2(0,0) + r1.yz;
  r1.yz = r1.yz / float2(300,300);
  r1.y = inWhitecapTexture1.Sample(g_samplerLinear_Wrap_s, r1.yz).y;
  r1.y = r1.y;
  r1.z = 9.99999975e-05 / g_psScene.m_exposure.z;
  r1.z = max(0, r1.z);
  r1.z = min(1, r1.z);
  r1.z = max(0.349999994, r1.z);
  r6.xyz = float3(0.25999999,0.280000001,0.340000004) * r1.zzz;
  r2.y = 0;
  r1.z = dot(r2.xyz, r2.xyz);
  r1.z = rsqrt(r1.z);
  r2.xyz = r2.xyz * r1.zzz;
  r1.z = dot(r2.xyz, r3.xyz);
  r1.z = r1.z + r3.y;
  r1.z = max(0, r1.z);
  r2.xyz = min(float3(1,1,1), r1.zzz);
  r1.z = dot(r4.xyz, r4.xyz);
  r1.z = rsqrt(r1.z);
  r3.xyz = r4.xyz * r1.zzz;
  r3.xyz = float3(-0.25,-0.25,-0.25) + r3.xyz;
  r2.xyz = r3.xyz * r2.xyz;
  r2.xyz = float3(0.25,0.25,0.25) + r2.xyz;
  r2.xyz = r6.xyz * r2.xyz;
  r3.xyz = float3(0.140000001,0.314999998,0.262499988) * r2.xyz;
  r2.xyz = float3(0.270000011,0.405000001,0.247500002) * r2.xyz;
  r1.y = 10 * r1.y;
  r1.y = max(0, r1.y);
  r1.y = min(1, r1.y);
  r1.z = -r1.y;
  r1.z = 1 + r1.z;
  r3.xyz = r3.xyz * r1.yyy;
  r1.yzw = r2.xyz * r1.zzz;
  r1.yzw = r3.xyz + r1.yzw;
  r1.yzw = float3(0.850000024,0.850000024,0.850000024) * r1.yzw;
  r1.yzw = r1.yzw * r0.www;
  r0.xyz = r1.yzw + r0.xyz;
  r5.xyz = r5.xyz;
  r0.xyz = r0.xyz;
#if 1
  r0.rgb = ApplyTppTonemap(r0.rgb, r5.xyz);
#else
  r1.yzw = r5.yyy;
  r2.xyz = cmp(r1.yzw >= r0.xyz);
  r2.xyz = r2.xyz ? float3(1,1,1) : float3(0,0,0);
  r3.xyz = r2.xyz * r0.xyz;
  r2.xyz = -r2.xyz;
  r2.xyz = float3(1,1,1) + r2.xyz;
  r0.xyz = r0.xyz + r5.zzz;
  r4.xyz = -r1.zzz;
  r0.xyz = r4.xyz + r0.xyz;
  r0.xyz = r5.xxx * r0.xyz;
  r0.xyz = float3(-1,-1,-1) / r0.xyz;
  r0.xyz = r0.xyz + r5.zzz;
  r0.xyz = r0.xyz + r1.zzz;
  r0.xyz = r2.xyz * r0.xyz;
  r0.xyz = r3.xyz + r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
#endif
  r1.yzw = cmp(float3(0.00313080009,0.00313080009,0.00313080009) >= r0.xyz);
  r1.yzw = r1.yzw ? float3(1,1,1) : float3(0,0,0);
  r2.xyz = float3(12.9200001,12.9200001,12.9200001) * r0.xyz;
  r2.xyz = r2.xyz * r1.yzw;
  r1.yzw = -r1.yzw;
  r1.yzw = float3(1,1,1) + r1.yzw;
  r0.xyz = max(float3(9.99999975e-06,9.99999975e-06,9.99999975e-06), r0.xyz);
  r0.xyz = log2(r0.xyz);
  r0.xyz = float3(0.416666657,0.416666657,0.416666657) * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  r0.xyz = float3(1.05499995,1.05499995,1.05499995) * r0.xyz;
  r0.xyz = float3(-0.0549999997,-0.0549999997,-0.0549999997) + r0.xyz;
  r0.xyz = r1.yzw * r0.xyz;
  r0.xyz = r2.xyz + r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = v2.z;
  r1.x = r1.x;
  r0.w = r0.w;
  r1.x = r1.x;
  r0.w = cmp(r1.x >= r0.w);
  r0.w = r0.w ? 1 : 0;
  r0.w = (int)r0.w;
  r0.w = r0.w;
  r0.w = r0.w;
  r0.xyz = r0.xyz;
  r0.w = r0.w;
  r0.xyz = r0.xyz;
  r1.xyz = float3(1,1,1) * r0.xyz;
  r1.w = 1;
  r2.x = 1 * r0.w;
  r2.xyz = r2.xxx * r0.xyz;
  r2.w = 1 * r0.w;
  r1.xyzw = r1.xyzw;
  r2.xyzw = r2.xyzw;
  o0.xyzw = r1.xyzw;
  o1.xyzw = r2.xyzw;
  return;
}