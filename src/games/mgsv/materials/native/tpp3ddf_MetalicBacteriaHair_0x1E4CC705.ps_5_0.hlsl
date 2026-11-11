#include "../material_capture.hlsli"

// clang-format off
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:22 2026

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
SamplerState g_sampler_srm_hair_s : register(s1);
SamplerState g_sampler_translucent_s : register(s2);
SamplerState g_sampler_metalicLayer_s : register(s3);
SamplerState g_sampler_metalicBacteria_s : register(s4);
SamplerState g_samplerPoint_Wrap_s : register(s8);
Texture2D<float4> g_tex_diffuse : register(t0);
Texture2D<float4> g_tex_srm_hair : register(t1);
Texture2D<float4> g_tex_translucent : register(t2);
Texture2D<float4> g_tex_metalicLayer : register(t3);
Texture2D<float4> g_tex_metalicBacteria : register(t4);
Texture2D<float4> g_MaterialTexture : register(t10);
Texture2D<float4> g_tex_mesh : register(t15);


// 3Dmigoto declarations
#define cmp -


void NativeMaterialMain(
  float4 v0 : SV_Position0,
  float4 v1 : COLOR0,
  float4 v2 : TEXCOORD0,
  float4 v3 : TEXCOORD1,
  float4 v4 : TEXCOORD3,
  float4 v5 : TEXCOORD4,
  float4 v6 : TEXCOORD5,
  float4 v7 : TEXCOORD6,
  float3 v8 : TEXCOORD7,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1,
  out float4 o2 : SV_Target2)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v0.xy;
  r1.z = g_psMaterial.m_materials[0].x;
  r1.z = r1.z;
  r2.xyz = float3(1,1,1) * v8.xyz;
  r0.z = dot(r2.xyz, r2.xyz);
  r0.z = rsqrt(r0.z);
  r2.xyz = r2.xyz * r0.zzz;
  r2.xyz = r2.xyz;
  r3.xyz = g_tex_srm_hair.Sample(g_sampler_srm_hair_s, v2.xy).xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r1.w = g_tex_translucent.Sample(g_sampler_translucent_s, v2.xy).y;
  r1.w = r1.w;
  r1.w = r1.w;
  r4.xyz = v3.xyz;
  r4.xyz = r4.xyz;
  r5.xyzw = g_tex_diffuse.Sample(g_sampler_diffuse_s, v2.xy).xyzw;
  r6.xyz = float3(0.100000001,0.100000001,0.100000001) * g_psMaterial.m_materials[4].xyz;
  r6.xyz = -r6.xyz;
  r4.xyz = r6.xyz + r4.xyz;
  r0.z = dot(r4.xyz, r4.xyz);
  r0.z = sqrt(r0.z);
  r0.w = g_tex_metalicBacteria.Sample(g_sampler_metalicBacteria_s, v2.xy).y;
  r0.w = r0.w;
  r2.w = 0.100000001 * g_psMaterial.m_materials[5].x;
  r2.w = -r2.w;
  r3.w = r2.w + r0.z;
  r4.x = 0.100000001 * g_psMaterial.m_materials[5].y;
  r2.w = r4.x + r2.w;
  r2.w = r3.w / r2.w;
  r2.w = max(0, r2.w);
  r2.w = min(1, r2.w);
  r2.w = 2 * r2.w;
  r2.w = r2.w + r0.w;
  r2.w = -1 + r2.w;
  r2.w = max(0, r2.w);
  r2.w = min(1, r2.w);
  r2.w = r2.w;
  r3.w = cmp(g_psMaterial.m_materials[5].w != 0.000000);
  if (r3.w != 0) {
    r3.w = 0.100000001 * g_psMaterial.m_materials[5].z;
    r3.w = -r3.w;
    r4.x = r3.w + r0.z;
    r4.y = 0.100000001 * g_psMaterial.m_materials[5].w;
    r3.w = r4.y + r3.w;
    r3.w = r4.x / r3.w;
    r3.w = -r3.w;
    r3.w = 1 + r3.w;
    r3.w = max(0, r3.w);
    r3.w = min(1, r3.w);
    r0.w = r3.w * r0.w;
    r0.w = r2.w + r0.w;
  } else {
    r0.w = r2.w;
  }
  r4.xyzw = g_tex_metalicLayer.Sample(g_sampler_metalicLayer_s, v2.xy).wxyz;
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r6.xyz = r4.xxx * r0.www;
  r7.xyz = -r5.xyz;
  r4.xyz = r7.xyz + r4.yzw;
  r4.xyz = r6.xyz * r4.xyz;
  r4.xyz = r5.xyz + r4.xyz;
  r0.w = 0.100000001 * g_psMaterial.m_materials[6].x;
  r0.w = -r0.w;
  r0.z = r0.z + r0.w;
  r3.w = 0.100000001 * g_psMaterial.m_materials[6].y;
  r0.w = r3.w + r0.w;
  r0.z = r0.z / r0.w;
  r0.z = -r0.z;
  r0.z = 1 + r0.z;
  r0.w = -r2.w;
  r0.z = r0.z + r0.w;
  r0.w = g_psMaterial.m_materials[6].z;
  r0.z = r0.z * r0.w;
  r0.z = 1 + r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = r5.w * r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r4.xyz = r4.xyz;
  r0.z = r0.z;
  r0.z = r0.z;
  r5.xyz = v8.xyz;
  r6.xyz = v5.xyz;
  r0.z = r0.z;
  r5.xyz = r5.xyz;
  r6.xyz = r6.xyz;
  r0.w = g_psMaterial.m_materials[3].y;
  r0.w = r0.z * r0.w;
  r2.w = dot(r5.xyz, r6.xyz);
  r2.w = -r2.w;
  r2.w = 1 + r2.w;
  r2.w = max(0, r2.w);
  r2.w = min(1, r2.w);
  r3.w = g_psMaterial.m_materials[3].x;
  r2.w = log2(r2.w);
  r2.w = r3.w * r2.w;
  r2.w = exp2(r2.w);
  r4.w = -r0.z;
  r0.w = r4.w + r0.w;
  r0.w = r2.w * r0.w;
  r5.x = r0.z + r0.w;
  r0.z = cmp(r3.w == 0.000000);
  if (r0.z != 0) {
    r5.x = 0;
  }
  r5.x = r5.x;
  r5.x = r5.x;
  r5.x = r5.x;
  r5.y = v1.w;
  r0.xy = r0.xy;
  r5.x = r5.x;
  r5.y = r5.y;
  r0.xy = r0.xy;
  r0.z = g_psSystem.m_param.w;
  r0.xy = float2(0.125,0.125) * r0.xy;
  r0.yw = g_tex_mesh.Sample(g_samplerPoint_Wrap_s, r0.xy).xw;
  r0.yw = r0.yw;
  r0.x = r0.w * r0.z;
  r0.xy = -r0.xy;
  r0.xy = r5.xy + r0.xy;
  r0.xy = cmp(r0.xy < float2(0,0));
  r0.x = (int)r0.y | (int)r0.x;
  r0.x = r0.x ? asfloat(0xFFFFFFFFu) : 0;
  if (r0.x != 0) discard;
  r1.z = r1.z;
  r1.z = r1.z;
  r0.x = r1.z;
  r0.y = 0.75;
  r0.x = g_MaterialTexture.SampleLevel(g_samplerPoint_Wrap_s, r0.xy, 0).w;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r5.xyzw = g_psSystem.m_dominantLightDir.xyzw;
  r0.x = r0.x;
  r0.y = g_psMaterial.m_materials[2].x;
  r3.x = r3.x;
  r5.xyzw = r5.xyzw;
  r0.x = r0.x;
  r0.y = r0.y;
  r3.x = r3.x;
  r0.x = -6 * r0.x;
  r0.x = 7 + r0.x;
  r0.x = exp2(r0.x);
  r0.x = r0.x * r0.y;
  r0.y = dot(r6.xyz, r6.xyz);
  r0.y = rsqrt(r0.y);
  r0.yzw = r6.xyz * r0.yyy;
  r6.x = dot(r0.yzw, v6.xyz);
  r6.y = dot(r0.yzw, v7.xyz);
  r6.z = dot(r0.yzw, v8.xyz);
  r2.w = dot(r6.xyz, r6.xyz);
  r2.w = rsqrt(r2.w);
  r6.xyz = r6.xyz * r2.www;
  r2.w = dot(r5.xyz, r5.xyz);
  r2.w = rsqrt(r2.w);
  r5.xyz = r5.xyz * r2.www;
  r5.xyz = -r5.xyz;
  r7.x = dot(r5.xyz, v6.xyz);
  r7.y = dot(r5.xyz, v7.xyz);
  r7.z = dot(r5.xyz, v8.xyz);
  r2.w = dot(r7.xyz, r7.xyz);
  r2.w = rsqrt(r2.w);
  r5.xyz = r7.xyz * r2.www;
  r5.xyz = r6.xyz + r5.xyz;
  r2.w = dot(r5.xyz, r5.xyz);
  r3.w = rsqrt(r2.w);
  r3.w = r5.y * r3.w;
  r3.w = r3.w * r3.w;
  r3.w = -r3.w;
  r3.w = 1 + r3.w;
  r3.w = sqrt(r3.w);
  r3.w = log2(r3.w);
  r0.x = r3.w * r0.x;
  r0.x = exp2(r0.x);
  r2.w = sqrt(r2.w);
  r2.w = 10 * r2.w;
  r2.w = max(0, r2.w);
  r2.w = min(1, r2.w);
  r2.w = r2.w * r2.w;
  r2.w = r2.w * r2.w;
  r2.w = 1 * r2.w;
  r2.w = r2.w * r3.x;
  r2.w = r2.w * r5.w;
  r1.y = r2.w * r0.x;
  r1.y = r1.y;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r2.xyz = r2.xyz;
  r1.y = r1.y;
  r4.xyz = r4.xyz;
  r2.xyz = r2.xyz;
  r1.y = r1.y;
  r5.xyz = g_psMaterial.m_materials[7].xyz;
  r0.x = g_psMaterial.m_materials[7].w * r1.y;
  r0.y = dot(r2.xyz, r0.yzw);
  r0.y = -r0.y;
  r0.y = 1 + r0.y;
  r0.y = max(0, r0.y);
  r0.y = min(1, r0.y);
  r0.z = g_psMaterial.m_materials[3].z;
  r0.y = log2(r0.y);
  r0.y = r0.z * r0.y;
  r0.y = exp2(r0.y);
  r0.x = r0.x * r0.y;
  r0.yzw = -r4.xyz;
  r0.yzw = r5.xyz + r0.yzw;
  r0.xyz = r0.xxx * r0.yzw;
  r0.xyz = r4.xyz + r0.xyz;
  r0.xyz = r0.xyz;
  r1.y = r1.y;
  r1.y = r1.y;
  r2.xyz = r2.xyz;
  r2.xy = float2(0.5,0.5) * r2.xy;
  r4.xy = float2(0.5,0.5) + r2.xy;
  r2.x = 0.5 * r2.z;
  r2.x = 0.5 + r2.x;
  r4.z = sqrt(r2.x);
  r4.xy = r4.xy;
  r4.z = r4.z;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r0.xyz = r0.xyz;
  r1.w = r1.w;
  r4.xyz = r4.xyz;
  r1.x = r3.y;
  r1.y = r1.y;
  r4.w = r3.z;
  r0.xyz = r0.xyz;
  r1.w = r1.w;
  r4.xyz = r4.xyz;
  r1.x = r1.x;
  r1.y = r1.y;
  r4.w = r4.w;
  r0.xyz = r0.xyz;
  r0.w = 1;
  r4.xyz = r4.xyz;
  r4.w = r4.w;
  r1.x = r1.x;
  r1.y = r1.y;
  r1.z = r1.z;
  r1.w = r1.w;
  r0.xyzw = r0.xyzw;
  r4.xyzw = r4.xyzw;
  r1.xyzw = r1.xyzw;
  o0.xyzw = r0.xyzw;
  o1.xyzw = r4.xyzw;
  o2.xyzw = r1.xyzw;
  return;
}
// clang-format on

void main(
    float4 v0 : SV_Position0,
    float4 v1 : COLOR0,
    float4 v2 : TEXCOORD0,
    float4 v3 : TEXCOORD1,
    float4 v4 : TEXCOORD3,
    float4 v5 : TEXCOORD4,
    float4 v6 : TEXCOORD5,
    float4 v7 : TEXCOORD6,
    float3 v8 : TEXCOORD7,
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
  NativeMaterialMain(v0, v1, v2, v3, v4, v5, v6, v7, v8, o0, o1, o2);
  clean_material = float4(0.0, 0.0, 0.0, -1.0);
  material_owner = 0u;
  const uint capture = material_capture_parameters.Load(int3(0, 0, 0));
  if ((capture & 1u) == 0u) return;
  clean_material = float4(renodx::color::srgb::Decode(saturate(o0.rgb)), 1.0);
  material_owner = uint4(PackMaterialUNorm(o0), PackMaterialUNorm(o1), PackMaterialUNorm(o2), asuint(v0.z));
}
