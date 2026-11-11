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

SamplerState inBaseSampler_s : register(s0);
SamplerState inNormalSampler_s : register(s1);
SamplerState inSrmSampler_s : register(s3);
SamplerState g_samplerPoint_Wrap_s : register(s8);
Texture2D<float4> inTexture : register(t0);
Texture2D<float4> inNormalTexture : register(t1);
Texture2D<float4> inSrmTexture : register(t3);
Texture2D<float4> g_tex_mesh : register(t15);


// 3Dmigoto declarations
#define cmp -


void NativeMaterialMain(
  float4 v0 : SV_Position0,
  float4 v1 : COLOR0,
  float4 v2 : COLOR1,
  float4 v3 : TEXCOORD0,
  float4 v4 : TEXCOORD3,
  float4 v5 : TEXCOORD5,
  float4 v6 : TEXCOORD6,
  float3 v7 : TEXCOORD7,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1,
  out float4 o2 : SV_Target2)
{
  float4 r0,r1,r2,r3,r4,r5;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v0.xy;
  r1.xyz = v2.xyz;
  r2.xyzw = v3.xyzw;
  r0.zw = v4.xy;
  r1.xyz = r1.xyz;
  r2.xyzw = r2.xyzw;
  r0.zw = r0.zw;
  r3.xy = g_psObject.m_localParam[2].xy;
  r1.w = r2.w;
  r2.xyz = r2.xyz;
  r4.xyz = -r2.xyz;
  r4.xyz = max(r4.xyz, r2.xyz);
  r4.xyz = -r4.xyz;
  r4.xyz = r4.xyz + r1.www;
  r4.xyz = cmp(r4.xyz < float3(0,0,0));
  r1.w = (int)r4.y | (int)r4.x;
  r1.w = (int)r4.z | (int)r1.w;
  r1.w = (int)r1.w & -1;
  if (r1.w != 0) discard;
  r0.zw = r2.xy + r0.zw;
  r0.zw = r0.zw / r2.ww;
  r0.zw = float2(1,1) + r0.zw;
  r0.zw = float2(0.5,0.5) * r0.zw;
  r0.zw = r0.zw * r3.xy;
  r2.xyzw = inTexture.Sample(inBaseSampler_s, r0.zw).xyzw;
  r2.xyzw = max(r2.xyzw, r1.xxxx);
  r0.zw = r0.zw;
  r1.xw = inNormalTexture.Sample(inNormalSampler_s, r0.zw).wy;
  r1.xw = r1.xw;
  r1.xw = float2(2,2) * r1.xw;
  r3.xy = float2(-1,-1) + r1.xw;
  r1.x = r3.x * r3.x;
  r1.x = -r1.x;
  r1.x = 1 + r1.x;
  r1.w = r3.y * r3.y;
  r1.w = -r1.w;
  r1.x = r1.x + r1.w;
  r1.x = max(0, r1.x);
  r1.x = min(1, r1.x);
  r1.x = 9.99999975e-05 + r1.x;
  r1.w = rsqrt(r1.x);
  r3.z = r1.x * r1.w;
  r3.xy = r3.xy;
  r3.z = r3.z;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r4.xyz = v5.xyz;
  r1.x = dot(r4.xyz, r4.xyz);
  r1.x = rsqrt(r1.x);
  r4.xyz = r4.xyz * r1.xxx;
  r5.xyz = r3.xxx;
  r4.xyz = r5.xyz * r4.xyz;
  r5.xyz = v6.xyz;
  r1.x = dot(r5.xyz, r5.xyz);
  r1.x = rsqrt(r1.x);
  r5.xyz = r5.xyz * r1.xxx;
  r3.xyw = r3.yyy;
  r3.xyw = r5.xyz * r3.xyw;
  r3.xyw = r4.xyz + r3.xyw;
  r4.xyz = v7.xyz;
  r1.x = dot(r4.xyz, r4.xyz);
  r1.x = rsqrt(r1.x);
  r4.xyz = r4.xyz * r1.xxx;
  r5.xyz = r3.zzz;
  r4.xyz = r5.xyz * r4.xyz;
  r3.xyz = r4.xyz + r3.xyw;
  r4.xy = inSrmTexture.Sample(inSrmSampler_s, r0.zw).yx;
  r4.xy = r4.xy;
  r2.xyzw = r2.xyzw;
  r0.z = dot(r3.xyz, r3.xyz);
  r0.z = rsqrt(r0.z);
  r3.xyz = r3.xyz * r0.zzz;
  r0.zw = float2(0.5,0.5) * r3.xy;
  r5.xy = float2(0.5,0.5) + r0.zw;
  r0.z = 0.5 * r3.z;
  r0.z = 0.5 + r0.z;
  r5.z = sqrt(r0.z);
  r5.xy = r5.xy;
  r5.z = r5.z;
  r5.xyz = r5.xyz;
  r5.w = r2.w;
  r3.xyzw = min(r5.xyzw, r1.yyyy);
  r4.z = g_psMaterial.m_materials[0].x;
  r4.w = r5.w * r1.z;
  r4.xy = r4.xy;
  r2.xyzw = r2.xyzw;
  r3.xyzw = r3.xyzw;
  r4.xyzw = r4.xyzw;
  r2.w = r2.w;
  r0.z = g_psLight.m_lightParams[2].x;
  r0.xy = r0.xy;
  r2.w = r2.w;
  r0.z = r0.z;
  r0.xy = r0.xy;
  r0.xy = float2(0.125,0.125) * r0.xy;
  r0.x = g_tex_mesh.Sample(g_samplerPoint_Wrap_s, r0.xy).x;
  r0.x = r0.x;
  r0.x = -r0.x;
  r0.x = r0.z + r0.x;
  r0.x = cmp(r0.x < 0);
  r0.x = r0.x ? asfloat(0xFFFFFFFFu) : 0;
  if (r0.x != 0) discard;
  r2.w = r2.w;
  r2.w = r2.w;
  o0.xyz = r2.xyz;
  o0.w = r2.w;
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
    float4 v4 : TEXCOORD3,
    float4 v5 : TEXCOORD5,
    float4 v6 : TEXCOORD6,
    float3 v7 : TEXCOORD7,
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
  NativeMaterialMain(v0, v1, v2, v3, v4, v5, v6, v7, o0, o1, o2);
  clean_material = float4(0.0, 0.0, 0.0, -1.0);
  material_owner = 0u;
  const uint capture = material_capture_parameters.Load(int3(0, 0, 0));
  if ((capture & 1u) == 0u) return;
  clean_material = float4(renodx::color::srgb::Decode(saturate(o0.rgb)), 1.0);
  material_owner = uint4(PackMaterialUNorm(o0), PackMaterialUNorm(o1), PackMaterialUNorm(o2), asuint(v0.z));
}
