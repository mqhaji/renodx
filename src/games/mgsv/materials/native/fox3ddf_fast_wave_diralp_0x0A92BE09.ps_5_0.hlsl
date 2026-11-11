#include "../material_capture.hlsli"

// clang-format off
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:21 2026

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

SamplerState g_sampler_diffuse_s : register(s0);
SamplerState g_samplerPoint_Wrap_s : register(s8);
Texture2D<float4> g_tex_diffuse : register(t0);
Texture2D<float4> g_tex_mesh : register(t15);


// 3Dmigoto declarations
#define cmp -


void NativeMaterialMain(
  float4 v0 : SV_Position0,
  float4 v1 : COLOR0,
  float4 v2 : TEXCOORD0,
  float4 v3 : TEXCOORD5,
  float4 v4 : TEXCOORD6,
  float3 v5 : TEXCOORD7,
  uint v6 : SV_IsFrontFace0,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1,
  out float4 o2 : SV_Target2)
{
  float4 r0,r1,r2,r3,r4;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v0.xy;
  r0.z = v6.x;
  r0.z = r0.z;
  r0.z = r0.z;
  r0.z = r0.z ? 1 : -1;
  r0.z = (int)r0.z;
  r0.z = r0.z;
  r0.z = r0.z;
  r1.xyzw = g_tex_diffuse.Sample(g_sampler_diffuse_s, v2.xy).xyzw;
  r1.xyzw = r1.xyzw;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r2.z = g_psMaterial.m_materials[0].x;
  r2.z = r2.z;
  r0.z = r0.z;
  r0.z = r0.z;
  r3.xyz = v5.xyz * r0.zzz;
  r0.z = dot(r3.xyz, r3.xyz);
  r0.z = rsqrt(r0.z);
  r3.xyz = r3.xyz * r0.zzz;
  r3.xyz = r3.xyz;
  r2.xy = g_psMaterial.m_materials[1].yx;
  r2.xy = r2.xy;
  r2.y = r2.y;
  r2.y = r2.y;
  r2.y = r2.y;
  r2.y = r2.y;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r2.x = r2.x;
  r1.xyz = r1.xyz;
  r2.x = r2.x;
  r1.xyz = r1.xyz;
  r2.x = r2.x;
  r1.xyz = r1.xyz;
  r2.x = r2.x;
  r2.w = g_psMaterial.m_materials[1].z;
  r2.w = r2.w;
  r1.w = r1.w;
  r4.xyz = v5.xyz;
  r1.w = r1.w;
  r4.xyz = r4.xyz;
  r0.z = g_psMaterial.m_materials[7].y;
  r0.w = g_psMaterial.m_materials[7].x;
  r3.w = dot(r4.xyz, float3(0,0,1));
  r4.x = -r3.w;
  r3.w = max(r4.x, r3.w);
  r0.z = -r0.z;
  r3.w = r3.w + r0.z;
  r3.w = r3.w * r1.w;
  r0.z = r0.w + r0.z;
  r4.x = r3.w / r0.z;
  r4.x = r4.x;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r4.x = r4.x;
  r4.y = v1.w;
  r0.xy = r0.xy;
  r4.x = r4.x;
  r4.y = r4.y;
  r0.xy = r0.xy;
  r0.z = g_psSystem.m_param.w;
  r0.xy = float2(0.125,0.125) * r0.xy;
  r0.yw = g_tex_mesh.Sample(g_samplerPoint_Wrap_s, r0.xy).xw;
  r0.yw = r0.yw;
  r0.x = r0.w * r0.z;
  r0.xy = -r0.xy;
  r0.xy = r4.xy + r0.xy;
  r0.xy = cmp(r0.xy < float2(0,0));
  r0.x = (int)r0.y | (int)r0.x;
  r0.x = r0.x ? asfloat(0xFFFFFFFFu) : 0;
  if (r0.x != 0) discard;
  r2.y = r2.y;
  r2.y = r2.y;
  r2.y = r2.y;
  r2.y = r2.y;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r0.xy = float2(0.5,0.5) * r3.xy;
  r0.xy = float2(0.5,0.5) + r0.xy;
  r3.x = 0.5 * r3.z;
  r3.x = 0.5 + r3.x;
  r0.z = sqrt(r3.x);
  r0.xy = r0.xy;
  r0.z = r0.z;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r1.xyz = r1.xyz;
  r2.w = r2.w;
  r0.xyz = r0.xyz;
  r2.x = r2.x;
  r2.y = r2.y;
  r2.z = r2.z;
  r1.xyz = r1.xyz;
  r2.w = r2.w;
  r0.xyz = r0.xyz;
  r2.x = r2.x;
  r2.y = r2.y;
  r2.z = r2.z;
  r1.xyz = r1.xyz;
  r1.w = 1;
  r0.xyz = r0.xyz;
  r0.w = 0;
  r2.x = r2.x;
  r2.y = r2.y;
  r2.z = r2.z;
  r2.w = r2.w;
  r1.xyzw = r1.xyzw;
  r0.xyzw = r0.xyzw;
  r2.xyzw = r2.xyzw;
  o0.xyzw = r1.xyzw;
  o1.xyzw = r0.xyzw;
  o2.xyzw = r2.xyzw;
  return;
}
// clang-format on

void main(
    float4 v0 : SV_Position0,
    float4 v1 : COLOR0,
    float4 v2 : TEXCOORD0,
    float4 v3 : TEXCOORD5,
    float4 v4 : TEXCOORD6,
    float3 v5 : TEXCOORD7,
    uint v6 : SV_IsFrontFace0,
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
