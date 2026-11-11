#include "../material_capture.hlsli"

// clang-format off
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:23 2026

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

SamplerState g_samplerPoint_Wrap_s : register(s8);
SamplerState g_samplerLinear_Wrap_s : register(s10);
SamplerState g_samplerAnisotropic_Clamp_s : register(s13);
Texture2D<float4> inTileMapTexture : register(t4);
Texture2D<float4> inMaterialWeightMapTexture : register(t6);
Texture2D<float4> inMaterialSelectMapTexture : register(t7);
Texture2D<float4> inMaterialIndicesTexture : register(t8);
Texture2D<float4> inWorldTexture : register(t9);


// 3Dmigoto declarations
#define cmp -


void NativeMaterialMain(
  float4 v0 : TEXCOORD0,
  float v1 : TEXCOORD1,
  float3 w1 : TEXCOORD2,
  float4 v2 : TEXCOORD5,
  float4 v3 : TEXCOORD6,
  float3 v4 : TEXCOORD7,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1,
  out float4 o2 : SV_Target2)
{
  float4 r0,r1,r2,r3;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = v0.xy;
  r0.zw = v0.zw;
  r0.xy = r0.xy;
  r0.zw = r0.zw;
  r0.zw = g_psMaterial.m_materials[3].xy * r0.zw;
  r0.zw = floor(r0.zw);
  r0.zw = r0.zw / g_psMaterial.m_materials[3].xy;
  r1.xyz = inTileMapTexture.Sample(g_samplerPoint_Wrap_s, r0.zw).xyz;
  r1.xyz = float3(255.399994,255.399994,255.399994) * r1.xyz;
  r1.xyz = floor(r1.xyz);
  r0.zw = g_psMaterial.m_materials[0].xy * r0.xy;
  r1.z = -r1.z;
  r1.z = exp2(r1.z);
  r0.zw = r1.zz * r0.zw;
  r0.zw = frac(r0.zw);
  r0.zw = r1.xy + r0.zw;
  r0.zw = g_psMaterial.m_materials[0].zw * r0.zw;
  r0.zw = g_psMaterial.m_materials[1].xy + r0.zw;
  r1.xyzw = inMaterialSelectMapTexture.Sample(g_samplerPoint_Wrap_s, v0.xy).xyzw;
  r1.xyzw = float4(255.399994,255.399994,255.399994,255.399994) * r1.xyzw;
  r1.xyzw = floor(r1.xyzw);
  r2.xyzw = inMaterialWeightMapTexture.Sample(g_samplerLinear_Wrap_s, r0.zw).xyzw;
  r1.xyzw = float4(1,1,1,1) * r1.xyzw;
  r1.xyzw = r1.xyzw / float4(256,256,256,256);
  r2.xyzw = float4(256,256,256,256) * r2.xyzw;
  r2.xyzw = floor(r2.xyzw);
  r1.xyzw = r2.xyzw + r1.xyzw;
  r0.z = cmp(r1.y < r1.x);
  r0.z = r0.z ? r1.x : r1.y;
  r0.w = cmp(r1.w < r1.z);
  r0.w = r0.w ? r1.z : r1.w;
  r1.x = cmp(r0.w < r0.z);
  r0.z = r1.x ? r0.z : r0.w;
  r0.z = frac(r0.z);
  r0.z = 256 * r0.z;
  r0.z = 0.0625 * r0.z;
  r1.x = 0.03125 + r0.z;
  r1.y = 0;
  r1.z = inMaterialIndicesTexture.Sample(g_samplerPoint_Wrap_s, r1.xy).x;
  r1.z = r1.z;
  r1.z = r1.z;
  r0.xy = r0.xy / g_psObject.m_localParam[1].ww;
  r0.zw = -r0.xy;
  r2.xy = max(r0.xy, r0.zw);
  r0.xy = cmp(r0.xy >= r0.zw);
  r0.zw = frac(r2.xy);
  r2.xy = -r0.zw;
  r0.xy = r0.xy ? r0.zw : r2.xy;
  r0.xy = g_psObject.m_localParam[1].ww * r0.xy;
  r0.xy = g_psObject.m_localParam[2].ww * r0.xy;
  r0.xyz = inWorldTexture.Sample(g_samplerAnisotropic_Clamp_s, r0.xy).xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r1.z = r1.z;
  r0.xyz = r0.xyz;
  r1.z = r1.z;
  r0.xyz = r0.xyz;
  r2.xyz = v4.xyz;
  r2.w = dot(r2.xyz, r2.xyz);
  r2.w = rsqrt(r2.w);
  r2.xyz = r2.xyz * r2.www;
  r2.xyz = float3(1,1,1) * r2.xyz;
  r2.xyz = float3(0,0,0) + r2.xyz;
  r2.w = dot(r2.xyz, r2.xyz);
  r2.w = rsqrt(r2.w);
  r2.xyz = r2.xyz * r2.www;
  r0.w = 1;
  r0.xyz = r0.xyz;
  r2.xyz = r2.xyz;
  r2.xy = float2(0.5,0.5) * r2.xy;
  r3.xy = float2(0.5,0.5) + r2.xy;
  r2.x = 0.5 * r2.z;
  r2.x = 0.5 + r2.x;
  r3.z = sqrt(r2.x);
  r3.xy = r3.xy;
  r3.z = r3.z;
  r3.w = 0;
  r3.xyzw = r3.xyzw;
  r1.w = 0;
  r1.xy = float2(1,1);
  r1.z = r1.z;
  r0.xyzw = r0.xyzw;
  r3.xyzw = r3.xyzw;
  r1.xyzw = r1.xyzw;
  o0.xyzw = r0.xyzw;
  o1.xyzw = r3.xyzw;
  o2.xyzw = r1.xyzw;
  return;
}
// clang-format on

void main(
    float4 v0 : TEXCOORD0,
    float v1 : TEXCOORD1,
    float3 w1 : TEXCOORD2,
    float4 v2 : TEXCOORD5,
    float4 v3 : TEXCOORD6,
    float3 v4 : TEXCOORD7,
    out float4 o0 : SV_Target0,
    out float4 o1 : SV_Target1,
    out float4 o2 : SV_Target2,
    float4 material_position : SV_Position,
    out float4 clean_material : SV_Target3,
    out uint4 material_owner : SV_Target4,
    out float4 specular_reset : SV_Target5,
    out float4 composition_reset : SV_Target6) {
  specular_reset = 0.0;
  composition_reset = 0.0;
  material_clean_pass = false;
  NativeMaterialMain(v0, v1, w1, v2, v3, v4, o0, o1, o2);
  clean_material = float4(0.0, 0.0, 0.0, -1.0);
  material_owner = 0u;
  const uint capture = material_capture_parameters.Load(int3(0, 0, 0));
  if ((capture & 1u) == 0u) return;
  clean_material = float4(renodx::color::srgb::Decode(saturate(o0.rgb)), 1.0);
  material_owner = uint4(PackMaterialUNorm(o0), PackMaterialUNorm(o1), PackMaterialUNorm(o2), asuint(material_position.z));
}
