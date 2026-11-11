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
SamplerState g_samplerLinear_Clamp_s : register(s11);
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
  float4 r0,r1,r2,r3,r4,r5,r6,r7;
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
  r4.x = dot(r3.xy, r3.xy);
  r4.y = dot(r3.zw, r3.zw);
  r4.x = max(r4.x, r4.y);
  r4.x = 960 * r4.x;
  r4.x = 960 * r4.x;
  r4.x = log2(r4.x);
  r4.x = 0.5 * r4.x;
  r4.w = max(0, r4.x);
  r4.x = 0.0078125;
  r4.y = 0.25;
  r4.z = 0.234375;
  r4.w = r4.w;
  r5.x = -5 + r4.w;
  r5.w = max(0, r5.x);
  r5.x = 0.773925781;
  r5.y = 0.0625;
  r5.z = 0.0146484375;
  r5.w = r5.w;
  r6.x = cmp(r4.w >= 5);
  r6.x = r6.x ? 1 : 0;
  r6.y = -r6.x;
  r7.xyzw = float4(1,1,1,1) + r6.yyyy;
  r4.xyzw = r7.xyzw * r4.xyzw;
  r5.xyzw = r6.xxxx * r5.xyzw;
  r4.xyzw = r5.xyzw + r4.xyzw;
  r4.x = r4.x;
  r4.y = r4.y;
  r4.z = r4.z;
  r4.w = r4.w;
  r5.xy = r0.zw / float2(4,4);
  r5.zw = floor(r5.xy);
  r6.xy = float2(4,4) * r5.zw;
  r6.xy = -r6.xy;
  r5.xy = r6.xy + r0.zw;
  r5.xy = r5.xy;
  r5.zw = r5.zw;
  r5.xyzw = r5.xyzw * r4.yyyy;
  r5.xyzw = r5.xyzw + r4.xxxx;
  r1.xyzw = r4.zzzz * r1.xyzw;
  r1.xyzw = r1.xzyw + r5.xzyw;
  r1.xy = r1.xy;
  r4.w = r4.w;
  r1.zw = r1.zw;
  r0.zw = r4.zz * r3.xy;
  r3.xy = r4.zz * r3.zw;
  r4.xyz = inAlbedoTexture.SampleGrad(g_samplerAnisotropic_Clamp_s, r1.xy, r0.zwz, r3.xyx).xyz;
  r5.xyz = r0.xxx;
  r4.xyz = r5.xyz * r4.xyz;
  r5.yzw = inNormalTexture.SampleGrad(g_samplerAnisotropic_Clamp_s, r1.xy, r0.wzz, r3.yxx).xyz;
  r5.yzw = r5.yzw;
  r5.yzw = float3(2,2,2) * r5.yzw;
  r5.yzw = float3(-1,-1,-1) + r5.yzw;
  r5.yzw = r5.yzw;
  r5.yzw = r5.yzw * r5.xxx;
  r1.xy = inSrmTexture.SampleLevel(g_samplerLinear_Wrap_s, r1.xy, r4.w).yx;
  r1.xy = r1.xy * r5.xx;
  r6.xyz = inAlbedoTexture.SampleGrad(g_samplerAnisotropic_Clamp_s, r1.zw, r0.zwz, r3.xyx).xyz;
  r7.xyz = r0.yyy;
  r6.xyz = r7.xyz * r6.xyz;
  r6.xyz = r6.xyz + r4.xyz;
  r0.xyz = inNormalTexture.SampleGrad(g_samplerAnisotropic_Clamp_s, r1.zw, r0.zwz, r3.xyx).xyz;
  r0.xyz = r0.xyz;
  r0.xyz = float3(2,2,2) * r0.xyz;
  r0.xyz = float3(-1,-1,-1) + r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz * r7.yyy;
  r0.xyz = r5.yzw + r0.xyz;
  r1.zw = inSrmTexture.SampleLevel(g_samplerLinear_Clamp_s, r1.zw, r4.w).yx;
  r1.zw = r1.zw * r7.yy;
  r2.xy = r1.xy + r1.zw;
  r6.xyz = r6.xyz;
  r0.xyz = r0.xyz;
  r2.xy = r2.xy;
  r6.xyz = r6.xyz;
  r0.xyz = r0.xyz;
  r2.xy = r2.xy;
  r6.xyz = r6.xyz;
  r0.xyz = r0.xyz;
  r2.xy = r2.xy;
  r6.xyz = r6.xyz;
  r0.xyz = r0.xyz;
  r2.xy = r2.xy;
  r6.xyz = r6.xyz;
  r0.xyz = r0.xyz;
  r2.xy = r2.xy;
  r6.xyz = r6.xyz;
  r0.xyz = r0.xyz;
  r2.xy = r2.xy;
  r2.z = r2.z;
  r6.xyz = r6.xyz;
  r0.xyz = r0.xyz;
  r2.xy = r2.xy;
  r2.z = r2.z;
  r6.xyz = r6.xyz;
  r0.xyz = r0.xyz;
  r2.xy = r2.xy;
  r1.xyz = v2.xyz;
  r0.w = dot(r1.xyz, r1.xyz);
  r0.w = rsqrt(r0.w);
  r1.xyz = r1.xyz * r0.www;
  r3.xyz = r0.xxx;
  r1.xyz = r3.xyz * r1.xyz;
  r3.xyz = v3.xyz;
  r0.x = dot(r3.xyz, r3.xyz);
  r0.x = rsqrt(r0.x);
  r3.xyz = r3.xyz * r0.xxx;
  r0.xyw = r0.yyy;
  r0.xyw = r3.xyz * r0.xyw;
  r0.xyw = r1.xyz + r0.xyw;
  r1.xyz = v4.xyz;
  r1.w = dot(r1.xyz, r1.xyz);
  r1.w = rsqrt(r1.w);
  r1.xyz = r1.xyz * r1.www;
  r3.xyz = r0.zzz;
  r1.xyz = r3.xyz * r1.xyz;
  r0.xyz = r1.xyz + r0.xyw;
  r0.w = dot(r0.xyz, r0.xyz);
  r0.w = rsqrt(r0.w);
  r0.xyz = r0.xyz * r0.www;
  r6.w = 1;
  r6.xyz = r6.xyz;
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
  r6.xyzw = r6.xyzw;
  r1.xyzw = r1.xyzw;
  r2.xyzw = r2.xyzw;
  o0.xyzw = r6.xyzw;
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
