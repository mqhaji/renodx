// Port of a native lit particle shader (Prim_Poly_LitDP2_NS_VF), which the game lights with the sun
// but without its shadow maps. The mod scales the sun's term by the fog volume's sun
// visibility at the particle, so smoke and dust in shade lose the sun as the fog does.
#include "../../atmosphere/fog/fog_particle.hlsli"
#include "../../atmosphere/fog/fog_read.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:28 2026

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

SamplerState inTextureSampler_s : register(s0);
SamplerState g_samplerPoint_Clamp_s : register(s9);
SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inTexture : register(t0);
Texture2D<float4> g_tex_fog : register(t12);
Texture2D<float4> inDepthTexture : register(t13);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float4 v1 : TEXCOORD0,
  float4 v2 : TEXCOORD1,
  float4 v3 : TEXCOORD2,
  float4 v4 : TEXCOORD3,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v0.xy;
  r1.xyz = v3.xyz;
  r1.xyz = r1.xyz;
  r2.xyz = g_psLight.m_lightParams[0].xyz;
  r2.xyz = r2.xyz;
  r3.xyz = g_psLight.m_lightParams[1].xyz;
  r3.xyz = r3.xyz;
  r0.z = g_psScene.m_exposure.z;
  r4.xyz = g_psObject.m_localParam[1].xyz * r0.zzz;
  // Mod edit: the game adds the sun's light to this particle unshadowed.
  r4.xyz *= FogParticleSunVisibility(v0.xy, v2.z, g_psSystem.m_renderInfo.xy, g_psScene.m_fogParam[1].x, g_samplerLinear_Clamp_s);
  r4.xyz = float3(0,0,0) + r4.xyz;
  r5.xyz = g_psLight.m_lightParams[2].xyz;
  r5.xyz = r5.xyz;
  r6.xyz = g_psLight.m_lightParams[3].xyz;
  r6.xyz = r6.xyz * r0.zzz;
  r6.w = g_psLight.m_lightParams[2].w;
  r6.w = r6.w;
  r6.xyz = r6.xyz;
  r6.w = r6.w;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz;
  r6.xyzw = r6.xyzw;
  r1.xyz = r1.xyz;
  r1.xyz = -r1.xyz;
  r5.xyz = r5.xyz + r1.xyz;
  r0.w = dot(r5.xyz, r5.xyz);
  r0.w = sqrt(r0.w);
  r0.w = max(0.00999999978, r0.w);
  r0.w = r0.w * r0.w;
  r1.w = 1 / r0.w;
  r0.w = r6.w * r0.w;
  r0.w = -r0.w;
  r0.w = r1.w + r0.w;
  r0.w = max(0, r0.w);
  r5.xyz = r6.xyz * r0.www;
  r5.xyz = r5.xyz;
  r5.xyz = float3(0,0,0) + r5.xyz;
  r6.xyz = g_psLight.m_lightParams[4].xyz;
  r6.xyz = r6.xyz;
  r7.xyz = g_psLight.m_lightParams[5].xyz;
  r7.xyz = r7.xyz * r0.zzz;
  r7.w = g_psLight.m_lightParams[4].w;
  r7.w = r7.w;
  r7.xyz = r7.xyz;
  r7.w = r7.w;
  r6.xyz = r6.xyz;
  r6.xyz = r6.xyz;
  r7.xyzw = r7.xyzw;
  r1.xyz = r6.xyz + r1.xyz;
  r0.w = dot(r1.xyz, r1.xyz);
  r0.w = sqrt(r0.w);
  r0.w = max(0.00999999978, r0.w);
  r0.w = r0.w * r0.w;
  r1.x = 1 / r0.w;
  r0.w = r7.w * r0.w;
  r0.w = -r0.w;
  r0.w = r1.x + r0.w;
  r0.w = max(0, r0.w);
  r1.xyz = r7.xyz * r0.www;
  r1.xyz = r1.xyz;
  r1.xyz = r5.xyz + r1.xyz;
  r0.w = g_psMaterial.m_materials[0].x;
  r1.w = g_psMaterial.m_materials[0].y;
  r2.w = g_psMaterial.m_materials[0].z;
  r4.xyz = r4.xyz * r1.www;
  r4.xyz = float3(1,1,1) * r4.xyz;
  r1.xyz = r2.www * r1.xyz;
  r2.xyz = r3.xyz + r2.xyz;
  r2.xyz = r2.xyz * r0.zzz;
  r2.xyz = r2.xyz * r0.www;
  r1.xyz = r4.xyz + r1.xyz;
  r2.xyz = r2.xyz;
  r1.xyz = r1.xyz;
  r3.xyzw = v4.xyzw;
  r3.xyzw = r3.xyzw;
  r4.xyzw = inTexture.Sample(inTextureSampler_s, v1.xy).xyzw;
  r4.xyz = r4.xyz;
  r5.xyz = cmp(float3(0.0392800011,0.0392800011,0.0392800011) >= r4.xyz);
  r5.xyz = r5.xyz ? float3(1,1,1) : float3(0,0,0);
  r6.xyz = r4.xyz / float3(12.9200001,12.9200001,12.9200001);
  r6.xyz = r6.xyz * r5.xyz;
  r5.xyz = -r5.xyz;
  r5.xyz = float3(1,1,1) + r5.xyz;
  r4.xyz = float3(0.0549999997,0.0549999997,0.0549999997) + r4.xyz;
  r4.xyz = r4.xyz / float3(1.05499995,1.05499995,1.05499995);
  r4.xyz = max(float3(9.99999975e-06,9.99999975e-06,9.99999975e-06), r4.xyz);
  r4.xyz = log2(r4.xyz);
  r4.xyz = float3(2.4000001,2.4000001,2.4000001) * r4.xyz;
  r4.xyz = exp2(r4.xyz);
  r4.xyz = r5.xyz * r4.xyz;
  r4.xyz = r6.xyz + r4.xyz;
  r4.xyz = r4.xyz;
  r5.w = r4.w * r3.w;
  r5.xyz = r4.xyz * r3.xyz;
  r5.xyz = r5.xyz;
  r5.w = r5.w;
  r5.xyzw = r5.xyzw;
  r2.xyz = r2.xyz;
  r1.xyz = r1.xyz;
  r5.xyzw = r5.xyzw;
  r2.xyz = r2.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz + r2.xyz;
  r5.xyz = r5.xyz * r1.xyz;
  r5.w = r5.w;
  r5.xyz = r5.xyz;
  r5.w = r5.w;
  r5.xyzw = r5.xyzw;
  r0.xyzw = r0.xyxy;
  r1.x = v2.z;
  r5.xyzw = r5.xyzw;
  r2.xyzw = r0.xyzw;
  r1.x = r1.x;
  r2.xyzw = r2.xyzw;
  r1.x = r1.x;
  r5.xyzw = r5.xyzw;
  r2.xyzw = r2.xyzw;
  r0.z = r1.x;
  r3.xyzw = g_psSystem.m_renderInfo.xyxy;
  r2.xyzw = r2.xyzw / r3.xyzw;
  r2.xyzw = float4(2,-2,2,-2) * r2.xyzw;
  r2.xyzw = float4(-1,1,-1,1) + r2.xyzw;
  r2.xyzw = r2.xyzw;
  r0.z = r0.z;
  r2.xyzw = r2.xyzw;
  r0.z = r0.z;
  r0.z = r0.z;
  r0.w = g_psScene.m_fogParam[1].x;
  r0.z = log2(r0.z);
  r0.z = r0.w * r0.z;
  r0.z = r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = 127 * r0.z;
  r2.xyzw = float4(0.0146484375,0.123046875,0.0146484375,0.123046875) * r2.xyzw;
  r2.xyzw = float4(0.015625,0.125,0.015625,0.125) + r2.xyzw;
  r0.w = 1 + r0.z;
  r0.w = max(0, r0.w);
  r1.w = min(127, r0.w);
  r1.y = r0.z;
  r1.yz = floor(r1.yw);
  r1.yz = r1.yz / float2(32,32);
  r3.xy = frac(r1.yz);
  r3.xz = float2(32,32) * r3.xy;
  r3.yw = floor(r1.yz);
  r3.xyzw = float4(0.03125,0.25,0.03125,0.25) * r3.xyzw;
  r2.xyzw = r3.xyzw + r2.xyzw;
  r3.xyzw = frac(r0.zzzz);
  r4.xyzw = FogVolumeSample(g_tex_fog, g_samplerLinear_Clamp_s, r2.xy).xyzw;
  r2.xyzw = FogVolumeSample(g_tex_fog, g_samplerLinear_Clamp_s, r2.zw).xyzw;
  r6.xyzw = -r3.xyzw;
  r6.xyzw = float4(1,1,1,1) + r6.xyzw;
  r4.xyzw = r6.xyzw * r4.xyzw;
  r2.xyzw = r3.xyzw * r2.xyzw;
  r2.xyzw = r4.xyzw + r2.xyzw;
  r0.z = g_psScene.m_fogParam[1].y;
  r2.xyz = r2.xyz * r0.zzz;
  r2.xyz = r2.xyz;
  r2.w = r2.w;
  r2.xyzw = r2.xyzw;
  r2.xyzw = r2.xyzw;
  r2.xyzw = r2.xyzw;
  r2.xyzw = r2.xyzw;
  r5.xyzw = r5.xyzw;
  r2.xyzw = r2.xyzw;
  r0.z = g_psObject.m_localParam[3].y;
  r0.z = r0.z;
  r0.z = cmp(r0.z < 0);
  r1.yzw = r0.zzz ? float3(1,1,1) : float3(0,0,0);
  r1.yzw = r1.yzw;
  r0.z = -r2.w;
  r0.z = 1 + r0.z;
  r0.z = r1.w * r0.z;
  r0.z = -r0.z;
  r0.z = 1 + r0.z;
  r3.xyz = r5.xyz * r2.www;
  r2.xyz = r3.xyz + r2.xyz;
  r3.xyz = -r2.xyz;
  r3.xyz = r5.xyz + r3.xyz;
  r1.yzw = r3.xyz * r1.yzw;
  r2.yzw = r2.xyz + r1.yzw;
  r2.x = r5.w * r0.z;
  r2.yzw = r2.yzw;
  r2.x = r2.x;
  r2.xyzw = r2.xyzw;
  r2.xyzw = r2.xyzw;
  r2.xyzw = r2.xyzw;
  r2.xyzw = r2.xyzw;
  r2.x = r2.x;
  r2.x = r2.x;
  r0.z = g_psObject.m_localParam[3].z;
  r0.z = r0.z;
  r0.z = cmp(r0.z < 0);
  r0.z = r0.z ? 1 : 0;
  r0.z = cmp(r0.z == 1.000000);
  if (r0.z != 0) {
    r0.z = -0.496078432 + r2.x;
    r0.z = cmp(r0.z < 0);
    r0.z = r0.z ? 1 : 0;
    if (r0.z != 0) discard;
  }
  r2.x = r2.x;
  r2.x = r2.x;
  r2.yzw = r2.yzw;
  r2.yzw = r2.yzw;
  r2.yzw = r2.yzw;
  r1.yzw = cmp(float3(0.00313080009,0.00313080009,0.00313080009) >= r2.yzw);
  r1.yzw = r1.yzw ? float3(1,1,1) : float3(0,0,0);
  r3.xyz = float3(12.9200001,12.9200001,12.9200001) * r2.yzw;
  r3.xyz = r3.xyz * r1.yzw;
  r1.yzw = -r1.yzw;
  r1.yzw = float3(1,1,1) + r1.yzw;
  r2.yzw = max(float3(9.99999975e-06,9.99999975e-06,9.99999975e-06), r2.yzw);
  r2.yzw = log2(r2.yzw);
  r2.yzw = float3(0.416666657,0.416666657,0.416666657) * r2.yzw;
  r2.yzw = exp2(r2.yzw);
  r2.yzw = float3(1.05499995,1.05499995,1.05499995) * r2.yzw;
  r2.yzw = float3(-0.0549999997,-0.0549999997,-0.0549999997) + r2.yzw;
  r1.yzw = r2.yzw * r1.yzw;
  r1.yzw = r3.xyz + r1.yzw;
  r1.yzw = r1.yzw;
  r1.yzw = r1.yzw;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.zw = g_psObject.m_localParam[2].zw;
  r0.zw = r0.zw;
  r0.xy = float2(0.49609375,0.49609375) + r0.xy;
  r0.xy = r0.xy * r0.zw;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.zw = g_psScene.m_projectionParam.zw;
  r0.xy = inDepthTexture.Sample(g_samplerPoint_Clamp_s, r0.xy).xy;
  r0.xy = r0.xy;
  r0.x = r0.x;
  r0.zw = r0.zw;
  r0.w = -r0.w;
  r0.x = r0.x + r0.w;
  r3.x = r0.z / r0.x;
  r3.x = r3.x;
  r0.y = r0.y;
  r0.x = r0.y + r0.w;
  r3.y = r0.z / r0.x;
  r3.y = r3.y;
  r3.x = r3.x;
  r3.y = r3.y;
  r3.xy = r3.xy;
  r3.xy = r3.xy;
  r3.xy = r3.xy;
  r0.x = g_psObject.m_localParam[2].x;
  r0.x = r0.x;
  r0.y = -r1.x;
  r0.z = r3.x + r0.y;
  r0.z = r0.z * r0.x;
  r0.z = max(0, r0.z);
  r4.x = min(1, r0.z);
  r0.y = r3.y + r0.y;
  r0.x = r0.y * r0.x;
  r0.x = max(0, r0.x);
  r4.y = min(1, r0.x);
  r4.x = r4.x;
  r4.y = r4.y;
  r4.xy = r4.xy;
  r1.yzw = r1.yzw;
  r2.x = r2.x;
  r4.xy = r4.xy;
  r1.yzw = r1.yzw;
  r2.x = r2.x;
  r0.x = r2.x * r4.x;
  r0.xyz = r1.yzw * r0.xxx;
  r0.w = r2.x * r4.x;
  r1.x = r2.x * r4.y;
  r1.xyz = r1.yzw * r1.xxx;
  r1.w = r2.x * r4.y;
  r0.xyzw = r0.xyzw;
  r1.xyzw = r1.xyzw;
  o0.xyzw = r0.xyzw;
  o1.xyzw = r1.xyzw;
  return;
}