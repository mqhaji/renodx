#include "../material_capture.hlsli"

// clang-format off
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:23 2026

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
  r0.xy = g_psMaterial.m_materials[0].xy * r0.xy;
  r0.z = -r1.z;
  r0.z = exp2(r0.z);
  r0.xy = r0.xy * r0.zz;
  r0.xy = frac(r0.xy);
  r0.xy = r1.xy + r0.xy;
  r0.xy = g_psMaterial.m_materials[0].zw * r0.xy;
  r0.xy = g_psMaterial.m_materials[1].xy + r0.xy;
  r1.xyzw = inMaterialSelectMapTexture.Sample(g_samplerPoint_Wrap_s, v0.xy).xyzw;
  r1.xyzw = float4(255.399994,255.399994,255.399994,255.399994) * r1.xyzw;
  r1.xyzw = floor(r1.xyzw);
  r0.xyzw = inMaterialWeightMapTexture.Sample(g_samplerLinear_Wrap_s, r0.xy).xyzw;
  r1.xyzw = float4(1,1,1,1) * r1.xyzw;
  r1.xyzw = r1.xyzw / float4(256,256,256,256);
  r0.xyzw = float4(256,256,256,256) * r0.xyzw;
  r0.xyzw = floor(r0.xyzw);
  r0.xyzw = r1.xyzw + r0.xyzw;
  r1.x = cmp(r0.y < r0.x);
  r1.xy = r1.xx ? r0.xy : r0.yx;
  r0.x = cmp(r0.w < r0.z);
  r1.zw = r0.xx ? r0.zw : r0.wz;
  r0.x = cmp(r1.z < r1.x);
  r0.xy = r0.xx ? r1.xz : r1.zx;
  r0.z = cmp(r1.w < r1.y);
  r0.z = r0.z ? r1.y : r1.w;
  r0.w = cmp(r0.y < r0.z);
  r0.y = r0.w ? r0.z : r0.y;
  r1.x = frac(r0.x);
  r1.y = frac(r0.y);
  r0.zw = float2(256,256) * r1.xy;
  r1.y = floor(r0.x);
  r1.z = floor(r0.y);
  r1.yz = r1.yz / float2(256,256);
  r0.x = 0.0625 * r0.z;
  r0.x = 0.03125 + r0.x;
  r0.y = 0;
  r2.z = inMaterialIndicesTexture.Sample(g_samplerPoint_Wrap_s, r0.xy).x;
  r2.z = r2.z;
  r1.x = 0.00100000005 + r1.y;
  r0.x = r1.x + r1.z;
  r0.xy = r1.xz / r0.xx;
  r0.zw = r0.zw;
  r0.xy = r0.xy;
  r2.z = r2.z;
  r1.xyzw = v0.xxyy;
  r0.zw = r0.zw;
  r0.xy = r0.xy;
  r1.xyzw = r1.xyzw;
  r0.zw = r0.zw;
  r0.xy = r0.xy;
  r1.xyzw = g_psMaterial.m_materials[2].xxyy * r1.xyzw;
  r3.xy = ddx_coarse(r1.yw);
  r3.zw = ddy_coarse(r1.yw);
  r1.xyzw = frac(r1.xyzw);
  r3.x = dot(r3.xy, r3.xy);
  r3.y = dot(r3.zw, r3.zw);
  r3.x = max(r3.x, r3.y);
  r3.x = 960 * r3.x;
  r3.x = 960 * r3.x;
  r3.x = log2(r3.x);
  r3.x = 0.5 * r3.x;
  r3.x = max(0, r3.x);
  r3.x = min(5, r3.x);
  r3.yz = r0.zw / float2(4,4);
  r4.zw = floor(r3.yz);
  r3.yz = float2(4,4) * r4.zw;
  r3.yz = -r3.yz;
  r4.xy = r3.yz + r0.zw;
  r4.xy = r4.xy;
  r4.zw = r4.zw;
  r4.xyzw = float4(0.25,0.25,0.25,0.25) * r4.xyzw;
  r4.xyzw = float4(0.0078125,0.0078125,0.0078125,0.0078125) + r4.xyzw;
  r1.xyzw = float4(0.234375,0.234375,0.234375,0.234375) * r1.xyzw;
  r1.xyzw = r1.xzyw + r4.xzyw;
  r1.xy = r1.xy;
  r3.x = r3.x;
  r1.zw = r1.zw;
  r3.yzw = inAlbedoTexture.SampleLevel(g_samplerAnisotropic_Clamp_s, r1.xy, r3.x).xyz;
  r3.yzw = r3.yzw * r0.xxx;
  r0.zw = inNormalTexture.SampleLevel(g_samplerAnisotropic_Clamp_s, r1.xy, r3.x).wy;
  r0.zw = r0.zw;
  r0.zw = float2(2,2) * r0.zw;
  r4.xy = float2(-1,-1) + r0.zw;
  r0.z = r4.x * r4.x;
  r0.z = -r0.z;
  r0.z = 1 + r0.z;
  r0.w = r4.y * r4.y;
  r0.w = -r0.w;
  r0.z = r0.z + r0.w;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = 9.99999975e-05 + r0.z;
  r0.w = rsqrt(r0.z);
  r4.z = r0.z * r0.w;
  r4.xy = r4.xy;
  r4.z = r4.z;
  r4.xyz = r4.xyz * r0.xxx;
  r0.zw = inSrmTexture.SampleLevel(g_samplerAnisotropic_Clamp_s, r1.xy, r3.x).yx;
  r0.xz = r0.zw * r0.xx;
  r5.xyz = inAlbedoTexture.SampleLevel(g_samplerAnisotropic_Clamp_s, r1.zw, r3.x).xyz;
  r5.xyz = r5.xyz * r0.yyy;
  r5.xyz = r5.xyz + r3.yzw;
  r1.xy = inNormalTexture.SampleLevel(g_samplerAnisotropic_Clamp_s, r1.zw, r3.x).wy;
  r1.xy = r1.xy;
  r1.xy = float2(2,2) * r1.xy;
  r6.xy = float2(-1,-1) + r1.xy;
  r0.w = r6.x * r6.x;
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r1.x = r6.y * r6.y;
  r1.x = -r1.x;
  r0.w = r1.x + r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r0.w = 9.99999975e-05 + r0.w;
  r1.x = rsqrt(r0.w);
  r6.z = r1.x * r0.w;
  r6.xy = r6.xy;
  r6.z = r6.z;
  r3.yzw = r6.xyz * r0.yyy;
  r3.yzw = r4.xyz + r3.yzw;
  r1.xy = inSrmTexture.SampleLevel(g_samplerAnisotropic_Clamp_s, r1.zw, r3.x).yx;
  r0.yw = r1.xy * r0.yy;
  r2.xy = r0.xz + r0.yw;
  r5.xyz = r5.xyz;
  r3.yzw = r3.yzw;
  r2.xy = r2.xy;
  r5.xyz = r5.xyz;
  r3.yzw = r3.yzw;
  r2.xy = r2.xy;
  r5.xyz = r5.xyz;
  r3.yzw = r3.yzw;
  r2.xy = r2.xy;
  r5.xyz = r5.xyz;
  r3.yzw = r3.yzw;
  r2.xy = r2.xy;
  r5.xyz = r5.xyz;
  r3.yzw = r3.yzw;
  r2.xy = r2.xy;
  r5.xyz = r5.xyz;
  r3.yzw = r3.yzw;
  r2.xy = r2.xy;
  r2.z = r2.z;
  r5.xyz = r5.xyz;
  r3.yzw = r3.yzw;
  r2.xy = r2.xy;
  r2.z = r2.z;
  r5.xyz = r5.xyz;
  r3.yzw = r3.yzw;
  r2.xy = r2.xy;
  r0.xyz = v2.xyz;
  r0.w = dot(r0.xyz, r0.xyz);
  r0.w = rsqrt(r0.w);
  r0.xyz = r0.xyz * r0.www;
  r1.xyz = r3.yyy;
  r0.xyz = r1.xyz * r0.xyz;
  r1.xyz = v3.xyz;
  r0.w = dot(r1.xyz, r1.xyz);
  r0.w = rsqrt(r0.w);
  r1.xyz = r1.xyz * r0.www;
  r3.xyz = r3.zzz;
  r1.xyz = r3.xyz * r1.xyz;
  r0.xyz = r1.xyz + r0.xyz;
  r1.xyz = v4.xyz;
  r0.w = dot(r1.xyz, r1.xyz);
  r0.w = rsqrt(r0.w);
  r1.xyz = r1.xyz * r0.www;
  r3.xyz = r3.www;
  r1.xyz = r3.xyz * r1.xyz;
  r0.xyz = r1.xyz + r0.xyz;
  r0.w = dot(r0.xyz, r0.xyz);
  r0.w = rsqrt(r0.w);
  r0.xyz = r0.xyz * r0.www;
  r5.w = 1;
  r5.xyz = r5.xyz;
  r0.xyz = r0.xyz;
  r0.xy = float2(0.5,0.5) * r0.xy;
  r1.xy = float2(0.5,0.5) + r0.xy;
  r0.x = 0.5 * r0.z;
  r0.x = 0.5 + r0.x;
  r1.z = sqrt(r0.x);
  r1.xy = r1.xy;
  r1.z = r1.z;
  r1.w = 0;
  r1.xyzw = r1.xyzw;
  r2.w = 0;
  r2.xy = r2.xy;
  r2.z = r2.z;
  r5.xyzw = r5.xyzw;
  r1.xyzw = r1.xyzw;
  r2.xyzw = r2.xyzw;
  o0.xyzw = r5.xyzw;
  o1.xyzw = r1.xyzw;
  o2.xyzw = r2.xyzw;
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
