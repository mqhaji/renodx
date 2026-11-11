#include "../material_capture.hlsli"

// clang-format off
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:33 2026

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

SamplerState inBaseSampler_s : register(s0);
SamplerState inNormalSampler_s : register(s1);
SamplerState inSrmSampler_s : register(s3);
Texture2D<float4> inTexture : register(t0);
Texture2D<float4> inNormalTexture : register(t1);
Texture2D<float4> inSrmTexture : register(t3);


// 3Dmigoto declarations
#define cmp -


void NativeMaterialMain(
  float4 v0 : SV_Position0,
  float4 v1 : COLOR0,
  float4 v2 : COLOR1,
  float4 v3 : TEXCOORD0,
  float4 v4 : TEXCOORD5,
  float4 v5 : TEXCOORD6,
  float3 v6 : TEXCOORD7,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1,
  out float4 o2 : SV_Target2)
{
  float4 r0,r1,r2,r3,r4,r5;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = v2.xyzw;
  r1.xyzw = v1.xyzw;
  r2.xy = v3.xy;
  r0.xyzw = r0.xyzw;
  r1.xyzw = r1.xyzw;
  r2.xy = r2.xy;
  r2.zw = g_psObject.m_localParam[2].xy;
  r2.xy = r2.xy * r2.zw;
  r3.xyzw = inTexture.Sample(inBaseSampler_s, r2.xy).xyzw;
  r4.xyzw = r0.xxxx;
  r3.xyzw = max(r4.xyzw, r3.xyzw);
  r1.xyzw = r3.xyzw * r1.xyzw;
  r2.xy = r2.xy;
  r2.zw = inNormalTexture.Sample(inNormalSampler_s, r2.xy).wy;
  r2.zw = r2.zw;
  r2.zw = float2(2,2) * r2.zw;
  r3.xy = float2(-1,-1) + r2.zw;
  r0.x = r3.x * r3.x;
  r0.x = -r0.x;
  r0.x = 1 + r0.x;
  r2.z = r3.y * r3.y;
  r2.z = -r2.z;
  r0.x = r2.z + r0.x;
  r0.x = max(0, r0.x);
  r0.x = min(1, r0.x);
  r0.x = 9.99999975e-05 + r0.x;
  r2.z = rsqrt(r0.x);
  r3.z = r2.z * r0.x;
  r3.xy = r3.xy;
  r3.z = r3.z;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r4.xyz = v4.xyz;
  r0.x = dot(r4.xyz, r4.xyz);
  r0.x = rsqrt(r0.x);
  r4.xyz = r4.xyz * r0.xxx;
  r5.xyz = r3.xxx;
  r4.xyz = r5.xyz * r4.xyz;
  r5.xyz = v5.xyz;
  r0.x = dot(r5.xyz, r5.xyz);
  r0.x = rsqrt(r0.x);
  r5.xyz = r5.xyz * r0.xxx;
  r3.xyw = r3.yyy;
  r3.xyw = r5.xyz * r3.xyw;
  r3.xyw = r4.xyz + r3.xyw;
  r4.xyz = v6.xyz;
  r0.x = dot(r4.xyz, r4.xyz);
  r0.x = rsqrt(r0.x);
  r4.xyz = r4.xyz * r0.xxx;
  r5.xyz = r3.zzz;
  r4.xyz = r5.xyz * r4.xyz;
  r3.xyz = r4.xyz + r3.xyw;
  r2.xy = inSrmTexture.Sample(inSrmSampler_s, r2.xy).xy;
  r2.xy = r2.xy;
  r1.xyzw = r1.xyzw;
  r0.x = dot(r3.xyz, r3.xyz);
  r0.x = rsqrt(r0.x);
  r3.xyz = r3.xyz * r0.xxx;
  r2.zw = float2(0.5,0.5) * r3.xy;
  r4.xy = float2(0.5,0.5) + r2.zw;
  r0.x = 0.5 * r3.z;
  r0.x = 0.5 + r0.x;
  r4.z = sqrt(r0.x);
  r4.xy = r4.xy;
  r4.z = r4.z;
  r4.xyz = r4.xyz;
  r3.xyz = min(r4.xyz, r0.yyy);
  r4.w = r1.w;
  r4.x = min(r2.y, r0.w);
  r4.y = min(r2.x, r0.z);
  r4.z = g_psMaterial.m_materials[0].x;
  r4.w = r4.w;
  r1.xyzw = r1.xyzw;
  r3.xyz = r3.xyz;
  r3.w = r4.w;
  r4.xyzw = r4.xyzw;
  o0.xyzw = r1.xyzw;
  o1.xyzw = r3.xyzw;
  o2.xyzw = r4.xyzw;
  return;
}
// clang-format on

void main(
    float4 v0 : SV_Position0,
    float4 v1 : COLOR0,
    float4 v2 : COLOR1,
    float4 v3 : TEXCOORD0,
    float4 v4 : TEXCOORD5,
    float4 v5 : TEXCOORD6,
    float3 v6 : TEXCOORD7,
    out float4 o0 : SV_Target0,
    out float4 o1 : SV_Target1,
    out float4 o2 : SV_Target2,
    out float4 clean_material : SV_Target3,
    out uint4 material_owner : SV_Target4,
    out float4 specular_reset : SV_Target5,
    out float4 composition_reset : SV_Target6) {
  specular_reset = 0.0;
  composition_reset = 0.0;
  material_clean_pass = false;
  NativeMaterialMain(v0, v1, v2, v3, v4, v5, v6, o0, o1, o2);
  clean_material = float4(0.0, 0.0, 0.0, -1.0);
  material_owner = 0u;
  const uint capture = material_capture_parameters.Load(int3(0, 0, 0));
  if ((capture & 1u) == 0u) return;
  clean_material = float4(renodx::color::srgb::Decode(saturate(o0.rgb)), 1.0);
  material_owner = uint4(PackMaterialUNorm(o0), PackMaterialUNorm(o1), PackMaterialUNorm(o2), asuint(v0.z));
}
