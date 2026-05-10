#include "../tonemap.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:34 2026

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

SamplerState g_samplerLinear_Wrap_s : register(s10);
SamplerState g_samplerLinear_Clamp_s : register(s11);
SamplerState g_samplerAnisotropic_Wrap_s : register(s12);
Texture2D<float4> inGradientTexture : register(t1);
Texture2D<float4> inWhitecapTexture1 : register(t2);
Texture2D<float4> inFoamTexture : register(t3);
Texture2D<float4> inNormalDetailTexture : register(t4);
Texture2D<float4> inFoamTexture2 : register(t5);
Texture2D<float4> g_tex_fog : register(t12);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float4 v1 : TEXCOORD0,
  float4 v2 : TEXCOORD1,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r13,r14,r15,r16;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = float4(-0.5,-0.5,-0.5,-0.5) + v0.xyxy;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw / g_psSystem.m_renderInfo.xyxy;
  r0.xyzw = float4(2,-2,2,-2) * r0.xyzw;
  r0.xyzw = float4(-1,1,-1,1) + r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r1.x = v1.z;
  r0.xyzw = r0.xyzw;
  r1.x = r1.x;
  r0.xyzw = r0.xyzw;
  r1.x = r1.x;
  r0.xyzw = r0.xyzw;
  r1.y = r1.x;
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
  r2.w = min(127, r1.z);
  r2.y = r1.y;
  r1.zw = floor(r2.yw);
  r1.zw = r1.zw / float2(32,32);
  r2.xy = frac(r1.zw);
  r2.xz = float2(32,32) * r2.xy;
  r2.yw = floor(r1.zw);
  r2.xyzw = float4(0.03125,0.25,0.03125,0.25) * r2.xyzw;
  r0.xyzw = r2.xyzw + r0.xyzw;
  r2.xyzw = frac(r1.yyyy);
  r3.xyzw = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r0.xy).xyzw;
  r0.xyzw = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r0.zw).xyzw;
  r4.xyzw = -r2.xyzw;
  r4.xyzw = float4(1,1,1,1) + r4.xyzw;
  r3.xyzw = r4.xyzw * r3.xyzw;
  r0.xyzw = r2.xyzw * r0.xyzw;
  r0.xyzw = r3.xyzw + r0.xyzw;
  r1.y = g_psScene.m_fogParam[1].y;
  r0.xyz = r1.yyy * r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = r0.w;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r1.yzw = g_psObject.m_localParam[0].xzy;
  r1.yzw = r1.yzw;
  r1.yzw = r1.yzw;
  r2.xy = v1.xy;
  r3.xyz = v2.xyz;
  r2.z = v2.w;
  r1.yzw = r1.yzw;
  r0.xyzw = r0.xyzw;
  r2.xy = r2.xy;
  r3.xyz = r3.xyz;
  r2.z = r2.z;
  r1.yzw = r1.yzw;
  r0.xyzw = r0.xyzw;
  r4.w = 78.125;
  r5.xy = g_psMaterial.m_materials[0].xy;
  r2.w = g_psMaterial.m_materials[1].z;
  r5.zw = float2(0.620000005,0.889999986) * g_psMaterial.m_materials[2].yy;
  r6.xyz = g_psMaterial.m_materials[3].xyz;
  r7.xyz = g_psMaterial.m_materials[4].xyz;
  r3.w = g_psMaterial.m_materials[4].w;
  r6.w = g_psMaterial.m_materials[5].x;
  r7.w = g_psMaterial.m_materials[5].y;
  r8.x = g_psMaterial.m_materials[5].z;
  r8.w = g_psMaterial.m_materials[5].w;
  r9.x = g_psMaterial.m_materials[6].x;
  r8.y = g_psMaterial.m_materials[6].y;
  r8.z = g_psMaterial.m_materials[6].z;
  r9.y = g_psMaterial.m_materials[6].w;
  r10.xyz = g_psMaterial.m_materials[7].xyz * g_psScene.m_exposure.zzz;
  r11.xyz = g_psScene.m_eyepos.xyz;
  r11.xyz = -r11.xyz;
  r3.xyz = r11.xyz + r3.xyz;
  r9.z = dot(r3.xyz, r3.xyz);
  r9.z = rsqrt(r9.z);
  r3.xyz = r9.zzz * r3.xyz;
  r6.w = -r6.w;
  r6.w = r6.w + r1.x;
  r6.w = r6.w / r7.w;
  r6.w = -r6.w;
  r6.w = 1 + r6.w;
  r6.w = max(0, r6.w);
  r6.w = min(1, r6.w);
  r2.z = 0.649999976 * r2.z;
  r2.z = 0.349999994 + r2.z;
  r2.z = r6.w * r2.z;
  r6.w = r1.x / 5000;
  r6.w = -r6.w;
  r6.w = 1 + r6.w;
  r9.zw = r5.zz * r2.xy;
  r5.zw = r5.ww * r2.yx;
  r11.xy = inGradientTexture.Sample(g_samplerLinear_Wrap_s, r9.zw).xy;
  r11.xy = r11.xy;
  r5.zw = inGradientTexture.Sample(g_samplerLinear_Wrap_s, r5.zw).xy;
  r5.zw = r5.zw;
  r11.xy = float2(0.649999976,0.649999976) * r11.xy;
  r5.zw = float2(0.349999994,0.349999994) * r5.zw;
  r5.zw = r11.xy + r5.zw;
  r11.xyzw = inGradientTexture.Sample(g_samplerLinear_Wrap_s, v1.xy).wxyz;
  r12.xy = g_psMaterial.m_materials[2].zz * r5.zw;
  r12.zw = -r12.xy;
  r12.zw = r12.zw + r11.yz;
  r12.zw = r12.zw * r2.zz;
  r12.xz = r12.xy + r12.zw;
  r5.zw = float2(0.100000001,0.100000001) * r5.zw;
  r13.xy = r5.zw * r6.ww;
  r13.z = 0;
  r14.xyz = -r13.xyz;
  r11.yzw = r14.xyz + r11.yzw;
  r11.yzw = r11.yzw * r2.zzz;
  r4.xyz = r13.xyz + r11.yzw;
  r2.z = dot(r4.xyw, r4.xyw);
  r2.z = rsqrt(r2.z);
  r4.xyw = r4.xwy * r2.zzz;
  r12.y = 78.125;
  r2.z = dot(r12.xyz, r12.xyz);
  r2.z = rsqrt(r2.z);
  r11.yzw = r12.xyz * r2.zzz;
  r5.zw = r9.zw + r5.xy;
  r5.zw = float2(0.400000006,0.400000006) * r5.zw;
  r5.zw = inNormalDetailTexture.Sample(g_samplerAnisotropic_Wrap_s, r5.zw).yw;
  r5.zw = r5.zw;
  r5.zw = float2(2,2) * r5.zw;
  r12.xz = float2(-1,-1) + r5.zw;
  r2.z = r12.z * r12.z;
  r2.z = -r2.z;
  r2.z = 1 + r2.z;
  r5.z = r12.x * r12.x;
  r5.z = -r5.z;
  r2.z = r5.z + r2.z;
  r2.z = max(0, r2.z);
  r2.z = min(1, r2.z);
  r2.z = 9.99999975e-05 + r2.z;
  r5.z = rsqrt(r2.z);
  r12.y = r5.z * r2.z;
  r12.xz = r12.xz;
  r12.y = r12.y;
  r12.xyz = r12.xyz;
  r2.z = 0.649999976 * r6.w;
  r2.z = max(0, r2.z);
  r2.z = min(1, r2.z);
  r13.xyz = -r12.xyz;
  r11.yzw = r13.xyz + r11.yzw;
  r11.yzw = r11.yzw * r2.zzz;
  r11.yzw = r12.xyz + r11.yzw;
  r2.z = -100 + r1.x;
  r5.z = r2.z / 100;
  r5.z = max(0, r5.z);
  r12.w = min(1, r5.z);
  r13.xyz = -r3.xyz;
  r5.z = dot(r4.xyw, r13.xyz);
  r5.w = dot(r3.xyz, r11.yzw);
  r5.w = r5.w + r5.w;
  r5.w = -r5.w;
  r14.xyz = r11.yzw * r5.www;
  r14.xyz = r14.xyz + r3.xyz;
  r9.zw = float2(0.200000003,0.200000003) * r2.xy;
  r9.zw = r9.zw + r5.xy;
  r15.xyz = inFoamTexture.Sample(g_samplerAnisotropic_Wrap_s, r9.zw).xyz;
  r15.xyz = r15.xyz;
  r5.w = -r5.z;
  r5.z = max(r5.z, r5.w);
  r5.z = r6.w * r5.z;
  r5.z = -r5.z;
  r5.z = 1.00010002 + r5.z;
  r5.w = 8 * r6.w;
  r5.z = log2(r5.z);
  r5.z = r5.w * r5.z;
  r5.z = exp2(r5.z);
  r5.z = 0.949999988 * r5.z;
  r5.z = 0.0500000007 + r5.z;
  r5.z = max(0, r5.z);
  r16.xyz = min(float3(1,1,1), r5.zzz);
  r5.z = dot(r1.ywz, r14.xyz);
  r5.z = max(0, r5.z);
  r5.z = min(1, r5.z);
  r5.w = 2 * r8.x;
  r8.xyz = r8.xyz;
  r5.z = log2(r5.z);
  r7.w = r8.x * r5.z;
  r14.x = exp2(r7.w);
  r5.z = r5.w * r5.z;
  r14.y = exp2(r5.z);
  r5.z = dot(r8.yz, r14.xy);
  r5.w = r2.z / 500;
  r5.w = max(0, r5.w);
  r5.w = min(1, r5.w);
  r7.w = -r9.y;
  r7.w = max(r9.y, r7.w);
  r7.w = 1000 * r7.w;
  r7.w = -r7.w;
  r7.w = 1 + r7.w;
  r7.w = max(0.5, r7.w);
  r8.x = 0.5 * r15.z;
  r5.w = r8.x * r5.w;
  r5.w = r5.w * r7.w;
  r5.z = r5.z * r5.w;
  r5.w = r1.x / 2500;
  r5.w = -r5.w;
  r5.w = 1 + r5.w;
  r5.w = max(0, r5.w);
  r5.w = min(1, r5.w);
  r7.w = 0.200000003 * r5.w;
  r8.xyz = -r6.xyz;
  r8.xyz = r8.xyz + r1.ywz;
  r8.xyz = r8.xyz * r7.www;
  r8.xyz = r8.xyz + r6.xyz;
  r6.x = dot(r8.xyz, r8.xyz);
  r6.x = rsqrt(r6.x);
  r8.xyz = r8.xyz * r6.xxx;
  r6.x = 0.150000006 * r6.w;
  r6.x = 0.0250000004 + r6.x;
  r9.yzw = float3(-0,-1,-0) + r11.yzw;
  r6.xzw = r9.yzw * r6.xxx;
  r6.xzw = float3(0,1,0) + r6.xzw;
  r7.w = dot(r6.xzw, r6.xzw);
  r7.w = rsqrt(r7.w);
  r6.xzw = r7.www * r6.xzw;
  r7.w = dot(r3.xyz, r6.xzw);
  r7.w = r7.w + r7.w;
  r7.w = -r7.w;
  r6.xzw = r7.www * r6.xzw;
  r6.xzw = r6.xzw + r3.xyz;
  r6.z = dot(r8.xyz, r6.xzw);
  r6.z = max(0, r6.z);
  r6.z = min(1, r6.z);
  r7.w = r6.z * r6.z;
  r8.x = 1 * r7.w;
  r7.w = r7.w * r7.w;
  r7.w = r8.x * r7.w;
  r7.w = 1.27323997 * r7.w;
  r7.w = r7.w * r9.x;
  r5.z = r7.w + r5.z;
  r6.z = log2(r6.z);
  r6.z = 5000 * r6.z;
  r6.z = exp2(r6.z);
  r6.z = 796.093323 * r6.z;
  r6.z = r6.z * r9.x;
  r6.z = 0.25 * r6.z;
  r5.z = r6.z + r5.z;
  r8.xyz = r10.xyz * r5.zzz;
  r5.z = dot(r8.xyz, float3(0.333000004,0.333000004,0.333000004));
  r5.z = r5.z * r5.z;
  r5.z = max(0, r5.z);
  r5.z = min(1, r5.z);
  r8.xyz = r8.xyz * r5.zzz;
  r9.xyz = r16.zzz;
  r8.xyz = r9.xyz * r8.xyz;
  r8.xyz = r8.xyz * r8.www;
  r8.xyz = float3(1.04999995,1.04999995,1.04999995) * r8.xyz;
  r5.z = -0.0399999991 + r6.y;
  r5.z = 14 * r5.z;
  r5.z = 1 + r5.z;
  r5.z = max(0, r5.z);
  r5.z = min(1, r5.z);
  r6.y = 0.239999995 + r6.y;
  r6.y = 14 * r6.y;
  r6.y = -r6.y;
  r6.y = 1 + r6.y;
  r6.y = max(0, r6.y);
  r6.y = min(1, r6.y);
  r6.y = 0.5 * r6.y;
  r5.z = r6.y + r5.z;
  r5.z = r5.z * r5.z;
  r8.xyz = r8.xyz * r5.zzz;
  r5.z = 0.5 * r15.x;
  r5.z = max(0, r5.z);
  r5.z = min(1, r5.z);
  r9.xyz = float3(-0.075000003,-0.0900000036,-0.0599999987) * r5.zzz;
  r9.xyz = float3(0.135000005,0.194999993,0.254999995) + r9.xyz;
  r5.z = 9.99999975e-05 / g_psScene.m_exposure.z;
  r5.z = max(0, r5.z);
  r5.z = min(1, r5.z);
  r5.z = max(0.349999994, r5.z);
  r9.xyz = r9.xyz * r5.zzz;
  r3.y = r1.w;
  r6.y = dot(r3.xyz, r3.xyz);
  r6.y = rsqrt(r6.y);
  r3.xyz = r6.yyy * r3.xyz;
  r3.x = dot(r3.xzy, r1.yzw);
  r3.x = max(0, r3.x);
  r3.xyz = min(float3(1,1,1), r3.xxx);
  r6.y = dot(r10.xyz, r10.xyz);
  r6.y = rsqrt(r6.y);
  r11.yzw = r10.xyz * r6.yyy;
  r11.yzw = float3(-0.349999994,-0.349999994,-0.349999994) + r11.yzw;
  r3.xyz = r11.yzw * r3.xyz;
  r3.xyz = float3(0.349999994,0.349999994,0.349999994) + r3.xyz;
  r9.xyz = r9.xyz * r3.xyz;
  r9.xyz = float3(0.0960000008,0.200000003,0.379999995) * r9.xyz;
  r11.yzw = float3(-0.075000003,0.0250000004,0.0250000004) * r12.www;
  r11.yzw = float3(0.135000005,0.194999993,0.254999995) + r11.yzw;
  r6.yz = r2.xy / float2(10,10);
  r14.xy = r4.xw * r1.xx;
  r14.xy = r14.xy / float2(75,75);
  r6.yz = r14.xy + r6.yz;
  r1.x = inWhitecapTexture1.Sample(g_samplerLinear_Wrap_s, r6.yz).z;
  r1.x = r1.x * r5.w;
  r1.x = r1.x * r1.x;
  r1.x = 400 * r1.x;
  r1.x = max(0, r1.x);
  r1.x = min(1, r1.x);
  r11.yzw = float3(-0.0599999987,-0.104999997,-0.194999993) + r11.yzw;
  r11.yzw = r11.yzw * r1.xxx;
  r11.yzw = float3(0.0599999987,0.104999997,0.194999993) + r11.yzw;
  r11.yzw = r11.yzw * r5.zzz;
  r3.xyz = r11.yzw * r3.xyz;
  r1.x = dot(float3(0,1,0), r13.xyz);
  r1.x = 5 * r1.x;
  r1.x = 3 + r1.x;
  r5.z = -r3.w;
  r5.z = 1 + r5.z;
  r5.z = max(0, r5.z);
  r5.z = min(1, r5.z);
  r5.z = 10 * r5.z;
  r5.z = 1 + r5.z;
  r5.xy = r5.xy * r2.ww;
  r5.xy = float2(2,2) * r5.xy;
  r2.xy = r5.xy + r2.xy;
  r2.x = inFoamTexture2.Sample(g_samplerLinear_Wrap_s, r2.xy).y;
  r2.x = r2.x;
  r2.y = r2.z / 1500;
  r2.y = max(0, r2.y);
  r2.y = min(1, r2.y);
  r2.z = r3.w + r2.y;
  r2.z = 0.100000001 + r2.z;
  r2.z = max(0, r2.z);
  r2.z = min(1, r2.z);
  r2.w = -1 + r15.y;
  r2.z = r2.z * r2.w;
  r2.z = 1 + r2.z;
  r11.x = r11.x;
  r2.w = r4.z * r4.z;
  r2.w = r2.w * r2.w;
  r2.z = r2.z * r15.x;
  r2.x = r2.z * r2.x;
  r2.x = r2.x * r11.x;
  r2.x = 2 * r2.x;
  r2.y = r2.x * r2.y;
  r2.y = -r2.y;
  r2.x = r2.x + r2.y;
  r2.x = r5.z * r2.x;
  r2.x = 0.5 * r2.x;
  r5.xyz = float3(0.150000006,0.150000006,0.150000006) * r10.xyz;
  r5.xyz = r5.xyz + r9.xyz;
  r5.xyz = float3(0.0500000007,0.0500000007,0.0500000007) + r5.xyz;
  r1.w = dot(r4.xwy, r1.yzw);
  r2.y = -r1.w;
  r1.w = max(r2.y, r1.w);
  r1.y = dot(r1.yz, r6.xw);
  r1.z = -r1.y;
  r1.y = max(r1.y, r1.z);
  r1.y = r1.w + r1.y;
  r1.y = 0.5 * r1.y;
  r1.z = 0.25 * r2.w;
  r1.y = r1.y * r1.z;
  r1.yzw = r1.yyy * r10.xyz;
  r1.yzw = float3(0.00875000004,0.0315000005,0.0157500003) * r1.yzw;
  r2.y = 12 * r2.x;
  r2.y = -r2.y;
  r2.y = 1 + r2.y;
  r2.y = max(0, r2.y);
  r2.y = min(1, r2.y);
  r2.yzw = r8.xyz * r2.yyy;
  r4.xyz = float3(1.85000002,1.85000002,1.85000002) * r9.xyz;
  r3.xyz = r3.xyz * r1.xxx;
  r6.xyz = -r4.xyz;
  r3.xyz = r6.xyz + r3.xyz;
  r3.xyz = r16.xyz * r3.xyz;
  r3.xyz = r4.xyz + r3.xyz;
  r3.xyz = float3(0,0,0) + r3.xyz;
  r1.xyz = r3.xyz + r1.yzw;
  r3.xyz = r5.xyz * r2.xxx;
  r1.xyz = r3.xyz + r1.xyz;
  r1.xyz = float3(1,1,1) * r1.xyz;
  r1.xyz = r2.yzw + r1.xyz;
  r1.xyz = r1.xyz * r0.www;
  r0.xyz = r1.xyz + r0.xyz;
  r7.xyz = r7.xyz;
  r0.xyz = r0.xyz;
  r1.xyz = r7.yyy;
  r1.xzw = cmp(r1.xyz >= r0.xyz);
  r1.xzw = r1.xzw ? float3(1,1,1) : float3(0,0,0);
  r2.xyz = r1.xzw * r0.xyz;
  r1.xzw = -r1.xzw;
  r1.xzw = float3(1,1,1) + r1.xzw;
  r0.xyz = r0.xyz + r7.zzz;
  r3.xyz = -r1.yyy;
  r0.xyz = r3.xyz + r0.xyz;
  r0.xyz = r7.xxx * r0.xyz;
  r0.xyz = float3(-1,-1,-1) / r0.xyz;
  r0.xyz = r0.xyz + r7.zzz;
  r0.xyz = r0.xyz + r1.yyy;
  r0.xyz = r1.xzw * r0.xyz;
  r0.xyz = r2.xyz + r0.xyz;
  r0.xyz = r0.xyz;
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
  r12.w = r12.w;
  r12.xyz = r12.www * r0.xyz;
  r12.xyz = r12.xyz;
  r12.w = r12.w;
  o0.xyzw = r12.xyzw;
  o1.xyzw = r12.xyzw;
  return;
}