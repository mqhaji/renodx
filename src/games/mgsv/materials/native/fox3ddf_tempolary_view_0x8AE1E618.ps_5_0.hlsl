#include "../material_capture.hlsli"

// clang-format off
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:29 2026

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
Texture2D<float4> g_tex_mesh : register(t15);


// 3Dmigoto declarations
#define cmp -


void NativeMaterialMain(
  float4 v0 : SV_Position0,
  float2 v1 : TEXCOORD0,
  float2 w1 : TEXCOORD9,
  float4 v2 : TEXCOORD1,
  float4 v3 : TEXCOORD8,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1,
  out float4 o2 : SV_Target2)
{
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v0.xy;
  r0.zw = v1.xy;
  r0.zw = r0.zw;
  r1.xyz = g_psMaterial.m_materials[1].xyz;
  r0.zw = r0.zw;
  r0.zw = float2(5,5) * r0.zw;
  r0.zw = frac(r0.zw);
  r0.zw = cmp(r0.zw >= float2(0.5,0.5));
  r0.zw = r0.zw ? float2(1,1) : float2(0,0);
  r0.w = -r0.w;
  r0.z = r0.z + r0.w;
  r0.w = -r0.z;
  r0.z = max(r0.z, r0.w);
  r0.z = -r0.z;
  r0.z = 1 + r0.z;
  r0.w = g_psMaterial.m_materials[1].w;
  r0.z = -1 + r0.z;
  r0.z = r0.w * r0.z;
  r0.z = 1 + r0.z;
  r1.xyz = r1.xyz * r0.zzz;
  r1.xyz = r1.xyz;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.z = g_psSystem.m_param.w;
  r0.xy = float2(0.125,0.125) * r0.xy;
  r0.yw = g_tex_mesh.Sample(g_samplerPoint_Wrap_s, r0.xy).xw;
  r0.yw = r0.yw;
  r0.x = r0.w * r0.z;
  r0.xy = -r0.xy;
  r0.xy = float2(1,1) + r0.xy;
  r0.xy = cmp(r0.xy < float2(0,0));
  r0.x = (int)r0.y | (int)r0.x;
  r0.x = r0.x ? asfloat(0xFFFFFFFFu) : 0;
  if (r0.x != 0) discard;
  r0.xyz = v2.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xy = float2(0.5,0.5) * r0.xy;
  r2.xy = float2(0.5,0.5) + r0.xy;
  r0.x = 0.5 * r0.z;
  r0.x = 0.5 + r0.x;
  r2.z = sqrt(r0.x);
  r2.xy = r2.xy;
  r2.z = r2.z;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r1.xyz = r1.xyz;
  r2.xyz = r2.xyz;
  r1.xyz = r1.xyz;
  r2.xyz = r2.xyz;
  r1.xyz = r1.xyz;
  r1.w = 1;
  r2.xyz = r2.xyz;
  r2.w = 0;
  r1.xyzw = r1.xyzw;
  r2.xyzw = r2.xyzw;
  o0.xyzw = r1.xyzw;
  o1.xyzw = r2.xyzw;
  o2.xyzw = float4(0.643137276,1,0,0);
  return;
}
// clang-format on

void main(
    float4 v0 : SV_Position0,
    float2 v1 : TEXCOORD0,
    float2 w1 : TEXCOORD9,
    float4 v2 : TEXCOORD1,
    float4 v3 : TEXCOORD8,
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
  NativeMaterialMain(v0, v1, w1, v2, v3, o0, o1, o2);
  clean_material = float4(0.0, 0.0, 0.0, -1.0);
  material_owner = 0u;
  const uint capture = material_capture_parameters.Load(int3(0, 0, 0));
  if ((capture & 1u) == 0u) return;
  clean_material = float4(renodx::color::srgb::Decode(saturate(o0.rgb)), 1.0);
  material_owner = uint4(PackMaterialUNorm(o0), PackMaterialUNorm(o1), PackMaterialUNorm(o2), asuint(v0.z));
}
