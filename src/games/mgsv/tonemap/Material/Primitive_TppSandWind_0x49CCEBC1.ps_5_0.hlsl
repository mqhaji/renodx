#include "../tonemap.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:25 2026

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

cbuffer cPSLight : register(b3)
{

  struct
  {
    float4 m_lightParams[11];
  } g_psLight : packoffset(c0);

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
SamplerState g_samplerPoint_Clamp_s : register(s9);
SamplerState g_samplerLinear_Wrap_s : register(s10);
Texture2D<float4> inDistortionTexture : register(t0);
Texture2D<float4> inNoiseTexture : register(t1);
Texture2D<float4> inNormalTexture : register(t2);
Texture2D<float4> inMaterialTexture : register(t3);
Texture2D<float4> inHalfDepthTexture : register(t13);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : COLOR0,
  float4 v1 : COLOR1,
  float4 v2 : SV_Position0,
  float4 v3 : TEXCOORD0,
  float4 v4 : TEXCOORD1,
  float3 v5 : TEXCOORD2,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v2.xy;
  r1.xyz = g_psObject.m_localParam[0].xyz;
  r1.xyz = r1.xyz;
  r0.z = g_psScene.m_exposure.z;
  r2.xyz = g_psObject.m_localParam[1].xyz * r0.zzz;
  r2.xyz = r2.xyz;
  r1.xyz = r1.xyz;
  r2.xyz = r2.xyz;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.zw = float2(0.49609375,0.49609375) + r0.xy;
  r0.zw = g_psSystem.m_renderBuffer.zw * r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r1.xyz = r1.xyz;
  r2.xyz = r2.xyz;
  r0.zw = r0.zw;
  r1.xyz = r1.xyz;
  r2.xyz = r2.xyz;
  r3.x = g_psMaterial.m_materials[0].x;
  r1.w = g_psMaterial.m_materials[0].y;
  r2.w = g_psMaterial.m_materials[0].z;
  r3.z = g_psMaterial.m_materials[1].x;
  r3.w = g_psMaterial.m_materials[1].y;
  r4.x = g_psMaterial.m_materials[1].z;
  r4.y = g_psMaterial.m_materials[1].w;
  r5.xyz = g_psMaterial.m_materials[2].xyz;
  r4.z = g_psMaterial.m_materials[2].w;
  r4.w = g_psMaterial.m_materials[3].w;
  r6.xy = g_psMaterial.m_materials[4].xy;
  r6.zw = g_psMaterial.m_materials[4].zw;
  r0.zw = r0.zw;
  r7.xy = g_psScene.m_projectionParam.zw;
  r5.w = inHalfDepthTexture.SampleLevel(g_samplerPoint_Clamp_s, r0.zw, 0).x;
  r5.w = r5.w;
  r5.w = r5.w;
  r7.xy = r7.xy;
  r7.y = -r7.y;
  r5.w = r7.y + r5.w;
  r5.w = r7.x / r5.w;
  r5.w = r5.w;
  r5.w = r5.w;
  r5.w = r5.w;
  r0.xy = r0.xy;
  r7.xy = g_psSystem.m_renderInfo.xy;
  r7.zw = g_psScene.m_projectionParam.xy;
  r0.xy = r0.xy / r7.xy;
  r0.xy = float2(2,-2) * r0.xy;
  r0.xy = float2(-1,1) + r0.xy;
  r0.xy = r0.xy * r7.zw;
  r7.xy = r0.xy * r5.ww;
  r7.z = r5.w;
  r7.xy = r7.xy;
  r7.z = r7.z;
  r7.xyz = r7.xyz;
  r7.xyz = r7.xyz;
  r8.x = g_psScene.m_view._m00;
  r8.y = g_psScene.m_view._m01;
  r8.z = g_psScene.m_view._m02;
  r8.xyz = r8.xyz;
  r9.x = g_psScene.m_view._m10;
  r9.y = g_psScene.m_view._m11;
  r9.z = g_psScene.m_view._m12;
  r9.xyz = r9.xyz;
  r10.x = g_psScene.m_view._m20;
  r10.y = g_psScene.m_view._m21;
  r10.z = g_psScene.m_view._m22;
  r10.xyz = r10.xyz;
  r11.x = g_psScene.m_view._m30;
  r11.y = g_psScene.m_view._m31;
  r11.z = g_psScene.m_view._m32;
  r11.xyz = -r11.xyz;
  r11.xyz = r11.xyz + r7.xyz;
  r8.x = dot(r8.xyz, r11.xyz);
  r8.y = dot(r9.xyz, r11.xyz);
  r8.z = dot(r10.xyz, r11.xyz);
  r8.x = r8.x;
  r8.y = r8.y;
  r8.z = r8.z;
  r8.xyz = r8.xyz;
  r9.xyz = inNormalTexture.SampleLevel(g_samplerPoint_Clamp_s, r0.zw, 0).xyz;
  r9.xyz = r9.xyz;
  r0.xy = float2(2,2) * r9.xy;
  r0.xy = float2(-1,-1) + r0.xy;
  r7.w = r9.z * r9.z;
  r7.w = 2 * r7.w;
  r9.z = -1 + r7.w;
  r7.w = r9.z * r9.z;
  r7.w = -r7.w;
  r7.w = 1 + r7.w;
  r10.xy = r7.ww * r0.xy;
  r0.x = dot(r0.xy, r0.xy);
  r0.x = r7.w * r0.x;
  r0.x = 1.00000001e-07 + r0.x;
  r0.x = rsqrt(r0.x);
  r9.xy = r10.xy * r0.xx;
  r9.xy = r9.xy;
  r9.z = r9.z;
  r9.xyz = r9.xyz;
  r9.xyz = r9.xyz;
  r9.xyz = r9.xyz;
  r10.x = g_psScene.m_view._m00;
  r10.y = g_psScene.m_view._m01;
  r10.z = g_psScene.m_view._m02;
  r10.xyz = r10.xyz;
  r11.x = g_psScene.m_view._m10;
  r11.y = g_psScene.m_view._m11;
  r11.z = g_psScene.m_view._m12;
  r11.xyz = r11.xyz;
  r12.x = g_psScene.m_view._m20;
  r12.y = g_psScene.m_view._m21;
  r12.z = g_psScene.m_view._m22;
  r12.xyz = r12.xyz;
  r9.xyz = r9.xyz;
  r10.x = dot(r10.xyz, r9.xyz);
  r10.y = dot(r11.xyz, r9.xyz);
  r10.z = dot(r12.xyz, r9.xyz);
  r10.x = r10.x;
  r10.y = r10.y;
  r10.z = r10.z;
  r10.xyz = r10.xyz;
  r9.xyz = float3(0.00200000009,0.00200000009,0.00200000009) * r7.xyz;
  r0.x = dot(r9.xyz, r9.xyz);
  r0.x = -r0.x;
  r0.x = 1 + r0.x;
  r0.x = max(0, r0.x);
  r0.x = min(1, r0.x);
  r0.x = r0.x;
  r0.x = r0.x;
  r0.y = inMaterialTexture.SampleLevel(g_samplerPoint_Wrap_s, r0.zw, 0).z;
  r0.y = r0.y;
  r0.z = cmp(0.0764705911 >= r0.y);
  r0.y = cmp(r0.y >= 0.0803921595);
  r0.y = (int)r0.y | (int)r0.z;
  r0.x = r0.y ? 0 : r0.x;
  r9.x = cos(r2.w);
  r9.y = sin(r2.w);
  r11.y = 0;
  r11.x = r9.x;
  r11.z = r9.y;
  r0.y = dot(r11.xyz, r11.xyz);
  r0.y = rsqrt(r0.y);
  r0.yzw = r11.xyz * r0.yyy;
  r0.y = dot(r10.xyz, r0.yzw);
  r0.y = 0.5 * r0.y;
  r0.y = 0.5 + r0.y;
  r0.y = max(0, r0.y);
  r0.y = min(1, r0.y);
  r3.y = 1.20000005 * r3.x;
  r0.zw = r8.xz * r3.xy;
  r7.xyz = float3(0.0199999996,0.0199999996,0.0199999996) * r7.xyz;
  r3.x = dot(r7.xyz, r7.xyz);
  r3.x = -r3.x;
  r3.x = 1 + r3.x;
  r3.x = max(0, r3.x);
  r3.x = min(1, r3.x);
  r3.x = r3.x;
  r3.x = r3.x;
  r3.y = -r3.x;
  r3.y = 1 + r3.y;
  r7.xy = float2(0.100000001,0.100000001) * r0.zw;
  r7.zw = float2(0.200000003,0.200000003) * r6.xy;
  r7.xy = r7.xy + r7.zw;
  r7.x = inDistortionTexture.SampleLevel(g_samplerLinear_Wrap_s, r7.xy, 2).z;
  r7.x = r7.x;
  r2.w = -0.330000013 + r2.w;
  r11.x = cos(r2.w);
  r11.y = sin(r2.w);
  r2.w = r3.w * r3.y;
  r3.w = r7.x * r3.z;
  r3.w = r4.z + r3.w;
  r12.x = r3.w / r3.z;
  r3.z = inDistortionTexture.SampleLevel(g_samplerLinear_Wrap_s, r0.zw, 2).x;
  r3.zw = r3.zz * r2.ww;
  r12.y = 0.5 + r12.x;
  r3.zw = r12.xy + r3.zw;
  r3.zw = frac(r3.zw);
  r7.yz = float2(0.349999994,0.349999994) * r0.zw;
  r9.zw = float2(0.5,0.5) * r9.xy;
  r9.zw = -r9.zw;
  r7.yz = r9.zw + r7.yz;
  r9.zw = r9.xy * r3.zz;
  r7.yz = r9.zw + r7.yz;
  r9.zw = float2(0.419999987,0.419999987) * r0.zw;
  r11.zw = float2(0.5,0.5) * r11.xy;
  r11.zw = -r11.zw;
  r9.zw = r11.zw + r9.zw;
  r3.zw = r11.xy * r3.ww;
  r3.zw = r9.zw + r3.zw;
  r9.zw = inDistortionTexture.SampleLevel(g_samplerLinear_Wrap_s, r7.yz, 2).xy;
  r9.zw = r9.zw;
  r11.xy = inDistortionTexture.SampleLevel(g_samplerLinear_Wrap_s, r3.zw, 2).xy;
  r11.xy = r11.xy;
  r9.zw = float2(0.100000001,0.100000001) * r9.zw;
  r7.yz = r9.zw + r7.yz;
  r7.yz = inDistortionTexture.SampleLevel(g_samplerLinear_Wrap_s, r7.yz, 2).xy;
  r7.yz = r7.yz;
  r9.zw = float2(0.100000001,0.100000001) * r11.xy;
  r3.zw = r9.zw + r3.zw;
  r3.zw = inDistortionTexture.SampleLevel(g_samplerLinear_Wrap_s, r3.zw, 2).xy;
  r3.zw = r3.zw;
  r2.w = -r7.x;
  r2.w = 1 + r2.w;
  r4.z = 0.200000003 * r2.w;
  r4.z = r7.x + r4.z;
  r9.zw = r7.yz * r4.zz;
  r4.z = 0.200000003 * r7.x;
  r2.w = r4.z + r2.w;
  r7.xw = r3.zw * r2.ww;
  r2.w = frac(r12.x);
  r2.w = 2 * r2.w;
  r2.w = -1 + r2.w;
  r4.z = -r2.w;
  r2.w = max(r4.z, r2.w);
  r11.xy = -r9.zw;
  r7.xw = r11.xy + r7.xw;
  r7.xw = r7.xw * r2.ww;
  r7.xw = r9.zw + r7.xw;
  r2.w = r7.x * r7.x;
  r2.w = max(9.99999975e-06, r2.w);
  r2.w = log2(r2.w);
  r2.w = r4.x * r2.w;
  r2.w = exp2(r2.w);
  r0.zw = float2(5,5) * r0.zw;
  r4.xz = float2(0.150000006,0.150000006) * r7.yz;
  r4.xz = r4.xz + r0.zw;
  r7.yz = float2(16,16) * r4.ww;
  r6.xy = r7.yz * r6.xy;
  r4.xz = r6.xy + r4.xz;
  r4.x = inNoiseTexture.Sample(g_samplerPoint_Wrap_s, r4.xz).x;
  r4.x = r4.x;
  r3.zw = float2(0.5,0.5) * r3.zw;
  r0.zw = r3.zw + r0.zw;
  r3.zw = float2(4,4) * r4.ww;
  r3.zw = r6.zw * r3.zw;
  r0.zw = r3.zw + r0.zw;
  r0.z = inNoiseTexture.Sample(g_samplerPoint_Wrap_s, r0.zw).x;
  r0.z = r0.z;
  r6.xyz = g_psScene.m_eyepos.xyz;
  r6.xyz = r6.xyz;
  r6.xyz = r6.xyz;
  r6.xyz = -r6.xyz;
  r6.xyz = r8.xyz + r6.xyz;
  r0.w = dot(r6.xyz, r6.xyz);
  r0.w = rsqrt(r0.w);
  r3.zw = r6.xz * r0.ww;
  r0.w = dot(r3.zw, r9.xy);
  r3.z = -r0.w;
  r0.w = max(r3.z, r0.w);
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r0.w = log2(r0.w);
  r0.w = 16 * r0.w;
  r0.w = exp2(r0.w);
  r0.w = r0.w * r3.y;
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r0.x = r0.x * r0.w;
  r0.w = dot(r1.xyz, r10.xyz);
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r1.x = dot(r10.xzy, float3(0,0,1));
  r3.yzw = g_psLight.m_lightParams[0].xyz;
  r3.yzw = r3.yzw;
  r6.xyz = g_psLight.m_lightParams[1].xyz;
  r6.xyz = r6.xyz;
  r1.xyz = r3.yzw * r1.xxx;
  r1.xyz = r1.xyz + r6.xyz;
  r3.yzw = float3(1,0.5,0.150000006) * r1.www;
  r2.xyz = r0.www * r2.xyz;
  r1.xyz = r2.xyz + r1.xyz;
  r1.xyz = r3.yzw * r1.xyz;
  r0.w = -r0.y;
  r0.w = 1.25 + r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r0.w = r3.x * r0.w;
  r2.x = r2.w * r0.y;
  r2.y = 0.5 * r7.x;
  r0.y = r7.w * r0.y;
  r0.y = -r0.y;
  r0.y = r7.w + r0.y;
  r0.y = r2.y * r0.y;
  r2.y = r2.x * r4.x;
  r2.z = 1.5 * r4.y;
  r0.y = r2.y + r0.y;
  r0.y = r2.z * r0.y;
  r0.y = r0.y * r0.z;
  r0.z = -r2.x;
  r0.y = r0.y + r0.z;
  r0.y = r0.w * r0.y;
  r0.y = r2.x + r0.y;
  r0.x = 0.5 * r0.x;
  r0.z = max(0, r1.w);
  r0.z = min(1, r0.z);
  r0.x = r0.x * r0.z;
  r0.x = r0.y * r0.x;
  r0.y = -1 + r5.w;
  r0.y = 0.5 * r0.y;
  r0.y = max(0, r0.y);
  r0.y = min(1, r0.y);
  r0.w = r0.x * r0.y;
  r5.xyz = r5.xyz;
  r1.xyz = r1.xyz;
  r2.xyz = r5.yyy;
  r2.xzw = cmp(r2.xyz >= r1.xyz);
  r2.xzw = r2.xzw ? float3(1,1,1) : float3(0,0,0);
  r3.xyz = r2.xzw * r1.xyz;
  r2.xzw = -r2.xzw;
  r2.xzw = float3(1,1,1) + r2.xzw;
  r1.xyz = r1.xyz + r5.zzz;
  r4.xyz = -r2.yyy;
  r1.xyz = r4.xyz + r1.xyz;
  r1.xyz = r5.xxx * r1.xyz;
  r1.xyz = float3(-1,-1,-1) / r1.xyz;
  r1.xyz = r1.xyz + r5.zzz;
  r1.xyz = r1.xyz + r2.yyy;
  r1.xyz = r2.xzw * r1.xyz;
  r1.xyz = r3.xyz + r1.xyz;
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
  r1.xyz = r3.xyz + r1.xyz;
  r1.xyz = r1.xyz;
  r0.xyz = r1.xyz * r0.www;
  r0.xyz = r0.xyz;
  r0.w = r0.w;
  o0.xyzw = r0.xyzw;
  o1.xyzw = r0.xyzw;
  return;
}