#include "../material_capture.hlsli"

// clang-format off
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:24 2026

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
Texture2D<float4> inAlbedoTexture : register(t0);
Texture2D<float4> inNormalTexture : register(t1);
Texture2D<float4> inSrmTexture : register(t2);
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
  float4 r0,r1,r2,r3,r4,r5,r6;
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
  r2.xy = r0.zz ? r1.xy : r1.yx;
  r0.z = cmp(r1.w < r1.z);
  r2.zw = r0.zz ? r1.zw : r1.wz;
  r0.z = cmp(r2.z < r2.x);
  r0.zw = r0.zz ? r2.xz : r2.zx;
  r1.x = cmp(r2.w < r2.y);
  r1.x = r1.x ? r2.y : r2.w;
  r1.y = cmp(r0.w < r1.x);
  r0.w = r1.y ? r1.x : r0.w;
  r1.x = frac(r0.z);
  r1.y = frac(r0.w);
  r1.xy = float2(256,256) * r1.xy;
  r2.y = floor(r0.z);
  r2.z = floor(r0.w);
  r2.yz = r2.yz / float2(256,256);
  r0.z = 0.0625 * r1.x;
  r3.x = 0.03125 + r0.z;
  r3.y = 0;
  r3.z = inMaterialIndicesTexture.Sample(g_samplerPoint_Wrap_s, r3.xy).x;
  r3.z = r3.z;
  r2.x = 0.00100000005 + r2.y;
  r0.z = r2.x + r2.z;
  r0.zw = r2.xz / r0.zz;
  r1.xy = r1.xy;
  r0.zw = r0.zw;
  r3.z = r3.z;
  r2.xyzw = v0.xxyy;
  r1.xy = r1.xy;
  r0.zw = r0.zw;
  r2.xyzw = r2.xyzw;
  r1.xy = r1.xy;
  r0.zw = r0.zw;
  r2.xyzw = g_psMaterial.m_materials[2].xxyy * r2.xyzw;
  r1.zw = ddx_coarse(r2.yw);
  r4.xy = ddy_coarse(r2.yw);
  r2.xyzw = frac(r2.xyzw);
  r1.z = dot(r1.zw, r1.zw);
  r1.w = dot(r4.xy, r4.xy);
  r1.z = max(r1.z, r1.w);
  r1.z = 960 * r1.z;
  r1.z = 960 * r1.z;
  r1.z = log2(r1.z);
  r1.z = 0.5 * r1.z;
  r1.z = max(0, r1.z);
  r1.z = min(5, r1.z);
  r4.xy = r1.xy / float2(4,4);
  r4.zw = floor(r4.xy);
  r5.xy = float2(4,4) * r4.zw;
  r5.xy = -r5.xy;
  r4.xy = r5.xy + r1.xy;
  r4.xy = r4.xy;
  r4.zw = r4.zw;
  r4.xyzw = float4(0.25,0.25,0.25,0.25) * r4.xyzw;
  r4.xyzw = float4(0.0078125,0.0078125,0.0078125,0.0078125) + r4.xyzw;
  r2.xyzw = float4(0.234375,0.234375,0.234375,0.234375) * r2.xyzw;
  r2.xyzw = r2.xzyw + r4.xzyw;
  r2.xy = r2.xy;
  r1.z = r1.z;
  r2.zw = r2.zw;
  r1.xyw = inAlbedoTexture.SampleLevel(g_samplerAnisotropic_Clamp_s, r2.xy, r1.z).xyz;
  r1.xyw = r1.xyw * r0.zzz;
  r4.xyz = inNormalTexture.SampleLevel(g_samplerAnisotropic_Clamp_s, r2.xy, r1.z).xyz;
  r4.xyz = r4.xyz;
  r4.xyz = float3(2,2,2) * r4.xyz;
  r4.xyz = float3(-1,-1,-1) + r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz * r0.zzz;
  r2.xy = inSrmTexture.SampleLevel(g_samplerAnisotropic_Clamp_s, r2.xy, r1.z).yx;
  r2.xy = r2.xy * r0.zz;
  r5.xyz = inAlbedoTexture.SampleLevel(g_samplerAnisotropic_Clamp_s, r2.zw, r1.z).xyz;
  r5.xyz = r5.xyz * r0.www;
  r1.xyw = r5.xyz + r1.xyw;
  r5.xyz = inNormalTexture.SampleLevel(g_samplerAnisotropic_Clamp_s, r2.zw, r1.z).xyz;
  r5.xyz = r5.xyz;
  r5.xyz = float3(2,2,2) * r5.xyz;
  r5.xyz = float3(-1,-1,-1) + r5.xyz;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz * r0.www;
  r4.xyz = r5.xyz + r4.xyz;
  r2.zw = inSrmTexture.SampleLevel(g_samplerAnisotropic_Clamp_s, r2.zw, r1.z).yx;
  r0.zw = r2.zw * r0.ww;
  r3.xy = r2.xy + r0.zw;
  r1.xyw = r1.xyw;
  r4.xyz = r4.xyz;
  r3.xy = r3.xy;
  r1.xyw = r1.xyw;
  r4.xyz = r4.xyz;
  r3.xy = r3.xy;
  r2.xyz = v1.xxx;
  r1.xyw = r1.xyw;
  r4.xyz = r4.xyz;
  r3.xy = r3.xy;
  r2.xyz = r2.xyz;
  r1.xyw = r1.xyw;
  r4.xyz = r4.xyz;
  r3.xy = r3.xy;
  r0.xy = r0.xy / g_psObject.m_localParam[1].ww;
  r0.zw = -r0.xy;
  r5.xy = max(r0.xy, r0.zw);
  r0.xy = cmp(r0.xy >= r0.zw);
  r0.zw = frac(r5.xy);
  r5.xy = -r0.zw;
  r0.xy = r0.xy ? r0.zw : r5.xy;
  r0.xy = g_psObject.m_localParam[1].ww * r0.xy;
  r0.xy = g_psObject.m_localParam[2].ww * r0.xy;
  r0.xyz = inWorldTexture.Sample(g_samplerAnisotropic_Clamp_s, r0.xy).xyz;
  r0.xyz = r0.xyz;
  r5.xyz = r2.zzz;
  r6.xyz = -r1.xyw;
  r0.xyz = r6.xyz + r0.xyz;
  r0.xyz = r5.xyz * r0.xyz;
  r0.xyz = r1.xyw + r0.xyz;
  r1.xyz = -r4.xyz;
  r1.xyz = float3(0,0,1) + r1.xyz;
  r1.xyz = r2.xyz * r1.xyz;
  r1.xyz = r4.xyz + r1.xyz;
  r3.xy = r3.xy;
  r0.xyz = r0.xyz;
  r1.xyz = r1.xyz;
  r3.xy = r3.xy;
  r3.z = r3.z;
  r0.xyz = r0.xyz;
  r1.xyz = r1.xyz;
  r3.xy = r3.xy;
  r3.z = r3.z;
  r0.xyz = r0.xyz;
  r1.xyz = r1.xyz;
  r3.xy = r3.xy;
  r2.xyz = v2.xyz;
  r1.w = dot(r2.xyz, r2.xyz);
  r1.w = rsqrt(r1.w);
  r2.xyz = r2.xyz * r1.www;
  r4.xyz = r1.xxx;
  r2.xyz = r4.xyz * r2.xyz;
  r4.xyz = v3.xyz;
  r1.x = dot(r4.xyz, r4.xyz);
  r1.x = rsqrt(r1.x);
  r4.xyz = r4.xyz * r1.xxx;
  r1.xyw = r1.yyy;
  r1.xyw = r4.xyz * r1.xyw;
  r1.xyw = r2.xyz + r1.xyw;
  r2.xyz = v4.xyz;
  r2.w = dot(r2.xyz, r2.xyz);
  r2.w = rsqrt(r2.w);
  r2.xyz = r2.xyz * r2.www;
  r4.xyz = r1.zzz;
  r2.xyz = r4.xyz * r2.xyz;
  r1.xyz = r2.xyz + r1.xyw;
  r1.w = dot(r1.xyz, r1.xyz);
  r1.w = rsqrt(r1.w);
  r1.xyz = r1.xyz * r1.www;
  r0.w = 1;
  r0.xyz = r0.xyz;
  r1.xyz = r1.xyz;
  r1.xy = float2(0.5,0.5) * r1.xy;
  r2.xy = float2(0.5,0.5) + r1.xy;
  r1.x = 0.5 * r1.z;
  r1.x = 0.5 + r1.x;
  r2.z = sqrt(r1.x);
  r2.xy = r2.xy;
  r2.z = r2.z;
  r2.w = 0;
  r2.xyzw = r2.xyzw;
  r3.w = 0;
  r3.xy = r3.xy;
  r3.z = r3.z;
  r0.xyzw = r0.xyzw;
  r2.xyzw = r2.xyzw;
  r3.xyzw = r3.xyzw;
  o0.xyzw = r0.xyzw;
  o1.xyzw = r2.xyzw;
  o2.xyzw = r3.xyzw;
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
