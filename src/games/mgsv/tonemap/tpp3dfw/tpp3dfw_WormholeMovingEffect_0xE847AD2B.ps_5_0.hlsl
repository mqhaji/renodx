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

cbuffer cPSMaterial : register(b4)
{

  struct
  {
    float4 m_materials[8];
  } g_psMaterial : packoffset(c0);

}

SamplerState inDiffuseSampler_s : register(s0);
SamplerState inNormalSampler_s : register(s1);
SamplerState g_samplerLinear_Wrap_s : register(s10);
Texture2D<float4> inDiffuseTexture : register(t0);
Texture2D<float4> inNormalTexture : register(t1);
Texture2D<float4> inSpecularTexture : register(t2);
Texture2D<float4> inFireTexture : register(t5);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : COLOR0,
  float2 v1 : TEXCOORD0,
  float2 w1 : TEXCOORD1,
  float4 v2 : TEXCOORD3,
  float4 v3 : TEXCOORD4,
  float4 v4 : TEXCOORD5,
  float4 v5 : TEXCOORD6,
  float3 v6 : TEXCOORD7,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = inDiffuseTexture.Sample(inDiffuseSampler_s, v1.xy).xyzw;
  r0.xyz = r0.xyz;
  r1.xyz = cmp(float3(0.0392800011,0.0392800011,0.0392800011) >= r0.xyz);
  r1.xyz = r1.xyz ? float3(1,1,1) : float3(0,0,0);
  r2.xyz = r0.xyz / float3(12.9200001,12.9200001,12.9200001);
  r2.xyz = r2.xyz * r1.xyz;
  r1.xyz = -r1.xyz;
  r1.xyz = float3(1,1,1) + r1.xyz;
  r3.xyz = float3(0.0549999997,0.0549999997,0.0549999997) + r0.xyz;
  r3.xyz = r3.xyz / float3(1.05499995,1.05499995,1.05499995);
  r3.xyz = max(float3(9.99999975e-06,9.99999975e-06,9.99999975e-06), r3.xyz);
  r3.xyz = log2(r3.xyz);
  r3.xyz = float3(2.4000001,2.4000001,2.4000001) * r3.xyz;
  r3.xyz = exp2(r3.xyz);
  r1.xyz = r3.xyz * r1.xyz;
  r0.xyz = r2.xyz + r1.xyz;
  r0.xyz = r0.xyz;
  r0.w = r0.w;
  r0.xyz = r0.xyz;
  r0.w = r0.w;
  r1.xy = inSpecularTexture.Sample(g_samplerLinear_Wrap_s, v1.xy).yz;
  r1.xy = r1.xy;
  r2.xyz = v2.xyz;
  r2.xyz = r2.xyz;
  r3.xyz = g_psScene.m_eyepos.xyz;
  r3.xyz = -r3.xyz;
  r2.xyz = r3.xyz + r2.xyz;
  r1.z = dot(r2.xyz, r2.xyz);
  r1.z = rsqrt(r1.z);
  r2.xyz = r2.xyz * r1.zzz;
  r2.xyz = r2.xyz;
  r1.zw = inNormalTexture.Sample(inNormalSampler_s, v1.xy).wy;
  r1.zw = r1.zw;
  r1.zw = float2(2,2) * r1.zw;
  r3.xy = float2(-1,-1) + r1.zw;
  r1.z = r3.x * r3.x;
  r1.z = -r1.z;
  r1.z = 1 + r1.z;
  r1.w = r3.y * r3.y;
  r1.w = -r1.w;
  r1.z = r1.z + r1.w;
  r1.z = max(0, r1.z);
  r1.z = min(1, r1.z);
  r1.z = 9.99999975e-05 + r1.z;
  r1.w = rsqrt(r1.z);
  r3.z = r1.z * r1.w;
  r3.xy = r3.xy;
  r3.z = r3.z;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r1.zw = v1.xy;
  r2.w = v2.y;
  r4.xyz = v6.xyz;
  r3.xyz = r3.xyz;
  r2.xyz = r2.xyz;
  r3.w = v0.w;
  r5.xyz = -g_psMaterial.m_materials[4].xyz;
  r6.xyz = g_psMaterial.m_materials[3].xyz;
  r0.xyzw = r0.xyzw;
  r1.xy = r1.xy;
  r1.zw = r1.zw;
  r2.w = r2.w;
  r4.xyz = r4.xyz;
  r3.xyz = r3.xyz;
  r2.xyz = r2.xyz;
  r3.w = r3.w;
  r5.xyz = r5.xyz;
  r6.xyz = r6.xyz;
  r0.xyzw = r0.xyzw;
  r1.xy = r1.xy;
  r4.w = g_psMaterial.m_materials[5].w;
  r5.w = g_psMaterial.m_materials[6].x;
  r6.w = g_psMaterial.m_materials[6].y;
  r7.x = g_psMaterial.m_materials[7].x;
  r7.y = g_psMaterial.m_materials[7].z;
  r4.w = r7.x * r4.w;
  r1.zw = float2(1.5,1.5) * r1.zw;
  r7.x = inFireTexture.Sample(g_samplerLinear_Wrap_s, r1.zw).x;
  r7.x = r7.x;
  r4.w = 0.0287529994 * r4.w;
  r1.zw = r4.ww + r1.zw;
  r1.z = inFireTexture.Sample(g_samplerLinear_Wrap_s, r1.zw).x;
  r1.z = r1.z;
  r1.z = r1.z * r1.z;
  r1.w = 0.300000012 * r7.x;
  r1.w = r5.w + r1.w;
  r4.w = -r6.w;
  r2.w = r4.w + r2.w;
  r2.w = -r2.w;
  r1.w = r2.w + r1.w;
  r1.w = max(0, r1.w);
  r1.w = min(1, r1.w);
  r2.w = 20 * r1.w;
  r0.w = r0.w * r3.w;
  r0.w = r2.w * r0.w;
  r2.w = cmp(r0.w < 0.100000001);
  if (r2.w != 0) {
    if (-1 != 0) discard;
    r3.xyzw = float4(0,0,0,0);
  } else {
    r2.w = g_psMaterial.m_materials[7].y;
    r8.xyz = g_psMaterial.m_materials[1].xyz;
    r9.xyz = g_psMaterial.m_materials[2].xyz;
    r3.w = dot(r3.xyz, r3.xyz);
    r3.w = rsqrt(r3.w);
    r3.xyz = r3.xyz * r3.www;
    r10.xyz = v4.xyz;
    r10.xyz = r10.xyz * r3.xxx;
    r11.xyz = v5.xyz;
    r3.xyw = r11.xyz * r3.yyy;
    r11.xyz = r4.xyz * r3.zzz;
    r3.xyz = r11.xyz + r3.xyw;
    r3.xyz = r10.xyz + r3.xyz;
    r3.w = dot(r3.xyz, r3.xyz);
    r3.w = rsqrt(r3.w);
    r3.xyz = r3.xyz * r3.www;
    r1.x = r1.x;
    r1.x = -11 * r1.x;
    r1.x = 12 + r1.x;
    r1.x = exp2(r1.x);
    r1.y = r1.y;
    r3.w = dot(r3.yxz, float3(1,0,0));
    r8.xyz = r8.xyz * r3.www;
    r8.xyz = r8.xyz + r9.xyz;
    r3.w = dot(r5.xyz, r3.xyz);
    r3.w = max(0, r3.w);
    r3.w = min(1, r3.w);
    r9.xyz = -r2.xyz;
    r5.xyz = r9.xyz + r5.xyz;
    r4.w = dot(r5.xyz, r5.xyz);
    r4.w = rsqrt(r4.w);
    r5.xyz = r5.xyz * r4.www;
    r3.x = dot(r5.xyz, r3.xyz);
    r3.x = max(9.99999975e-05, r3.x);
    r3.yzw = r6.xyz * r3.www;
    r3.x = log2(r3.x);
    r1.x = r3.x * r1.x;
    r1.x = exp2(r1.x);
    r1.x = r1.y * r1.x;
    r5.xyz = r6.xyz * r1.xxx;
    r3.xyz = r3.yzw + r8.xyz;
    r0.xyz = r3.xyz * r0.xyz;
    r0.xyz = r0.xyz + r5.xyz;
    r1.x = g_psScene.m_exposure.z;
    r0.xyz = r1.xxx * r0.xyz;
    r0.w = r0.w;
    r1.x = 0.150000006 * r7.y;
    r1.y = -r1.w;
    r1.y = 1 + r1.y;
    r1.x = r1.x + r1.y;
    r1.x = 0.00249999994 * r1.x;
    r1.x = max(0, r1.x);
    r1.x = min(1, r1.x);
    r1.x = r1.z * r1.x;
    r1.xzw = float3(25,5,0.100000001) * r1.xxx;
    r0.xyz = r1.xzw + r0.xyz;
    r1.x = r7.x * r7.x;
    r1.xzw = float3(5,2.5,1) * r1.xxx;
    r1.y = 270 * r1.y;
    r1.y = -230 + r1.y;
    r1.y = max(0, r1.y);
    r1.y = min(1, r1.y);
    r3.xyz = r1.xzw * r1.yyy;
    r0.xyz = r3.xyz + r0.xyz;
    r3.xyz = g_psMaterial.m_materials[5].xyz;
    r3.xyz = r3.xyz;
    r0.xyz = r0.xyz;
    r5.xyz = r3.yyy;
    r5.xzw = cmp(r5.xyz >= r0.xyz);
    r5.xzw = r5.xzw ? float3(1,1,1) : float3(0,0,0);
    r6.xyz = r5.xzw * r0.xyz;
    r5.xzw = -r5.xzw;
    r5.xzw = float3(1,1,1) + r5.xzw;
    r0.xyz = r0.xyz + r3.zzz;
    r7.xyz = -r5.yyy;
    r0.xyz = r7.xyz + r0.xyz;
    r0.xyz = r3.xxx * r0.xyz;
    r0.xyz = float3(-1,-1,-1) / r0.xyz;
    r0.xyz = r0.xyz + r3.zzz;
    r0.xyz = r0.xyz + r5.yyy;
    r0.xyz = r5.xzw * r0.xyz;
    r0.xyz = r6.xyz + r0.xyz;
    r0.xyz = r0.xyz;
    r0.xyz = r0.xyz;
    r3.xyz = cmp(float3(0.00313080009,0.00313080009,0.00313080009) >= r0.xyz);
    r3.xyz = r3.xyz ? float3(1,1,1) : float3(0,0,0);
    r5.xyz = float3(12.9200001,12.9200001,12.9200001) * r0.xyz;
    r5.xyz = r5.xyz * r3.xyz;
    r3.xyz = -r3.xyz;
    r3.xyz = float3(1,1,1) + r3.xyz;
    r0.xyz = max(float3(9.99999975e-06,9.99999975e-06,9.99999975e-06), r0.xyz);
    r0.xyz = log2(r0.xyz);
    r0.xyz = float3(0.416666657,0.416666657,0.416666657) * r0.xyz;
    r0.xyz = exp2(r0.xyz);
    r0.xyz = float3(1.05499995,1.05499995,1.05499995) * r0.xyz;
    r0.xyz = float3(-0.0549999997,-0.0549999997,-0.0549999997) + r0.xyz;
    r0.xyz = r3.xyz * r0.xyz;
    r0.xyz = r5.xyz + r0.xyz;
    r0.xyz = r0.xyz;
    r1.y = 1.64999998 * r2.w;
    r1.y = max(0, r1.y);
    r1.y = min(1, r1.y);
    r1.y = r1.y * r1.y;
    r1.y = r1.y * r1.y;
    r1.y = r1.y * r1.y;
    r1.y = 1 * r1.y;
    r3.w = 1 * r1.y;
    r1.y = dot(r4.xyz, r2.xyz);
    r0.w = cmp(0.949999988 < r0.w);
    r1.y = cmp(0.25 < r1.y);
    r0.w = r0.w ? r1.y : 0;
    r1.xyz = float3(3,3,3) * r1.xzw;
    r0.xyz = r0.www ? r1.xyz : r0.xyz;
    r3.xyz = r0.xyz * r3.www;
  }
  r3.xyzw = r3.xyzw;
  r3.xyzw = r3.xyzw;
  o0.xyzw = r3.xyzw;
  return;
}