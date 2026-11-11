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

SamplerState g_sampler_diffuse_s : register(s0);
SamplerState g_sampler_normal_s : register(s1);
SamplerState g_sampler_srm_s : register(s2);
SamplerState g_sampler_metalicLayer_s : register(s3);
SamplerState g_sampler_metalicBacteria_s : register(s4);
SamplerState g_sampler_Dirty_s : register(s5);
SamplerState g_samplerPoint_Wrap_s : register(s8);
Texture2D<float4> g_tex_diffuse : register(t0);
Texture2D<float4> g_tex_normal : register(t1);
Texture2D<float4> g_tex_srm : register(t2);
Texture2D<float4> g_tex_metalicLayer : register(t3);
Texture2D<float4> g_tex_metalicBacteria : register(t4);
Texture2D<float4> g_tex_Dirty : register(t5);
Texture2D<float4> g_tex_mesh : register(t15);


// 3Dmigoto declarations
#define cmp -


void NativeMaterialMain(
  float4 v0 : SV_Position0,
  float4 v1 : COLOR0,
  float4 v2 : TEXCOORD0,
  float4 v3 : TEXCOORD1,
  float4 v4 : TEXCOORD4,
  float4 v5 : TEXCOORD5,
  float4 v6 : TEXCOORD6,
  float3 v7 : TEXCOORD7,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1,
  out float4 o2 : SV_Target2)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v0.xy;
  r1.xyz = float3(1,1,1) * v7.xyz;
  r0.zw = g_tex_normal.Sample(g_sampler_normal_s, v2.zw).wy;
  r0.zw = r0.zw;
  r0.zw = float2(2,2) * r0.zw;
  r2.xy = float2(-1,-1) + r0.zw;
  r0.z = r2.x * r2.x;
  r0.z = -r0.z;
  r0.z = 1 + r0.z;
  r0.w = r2.y * r2.y;
  r0.w = -r0.w;
  r0.z = r0.z + r0.w;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = 9.99999975e-05 + r0.z;
  r0.w = rsqrt(r0.z);
  r2.z = r0.z * r0.w;
  r2.xy = r2.xy;
  r2.z = r2.z;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r3.xyz = v5.xyz;
  r0.z = dot(r3.xyz, r3.xyz);
  r0.z = rsqrt(r0.z);
  r3.xyz = r3.xyz * r0.zzz;
  r4.xyz = r2.xxx;
  r3.xyz = r4.xyz * r3.xyz;
  r4.xyz = v6.xyz;
  r0.z = dot(r4.xyz, r4.xyz);
  r0.z = rsqrt(r0.z);
  r4.xyz = r4.xyz * r0.zzz;
  r2.xyw = r2.yyy;
  r2.xyw = r4.xyz * r2.xyw;
  r2.xyw = r3.xyz + r2.xyw;
  r0.z = dot(r1.xyz, r1.xyz);
  r0.z = rsqrt(r0.z);
  r1.xyz = r1.xyz * r0.zzz;
  r3.xyz = r2.zzz;
  r1.xyz = r3.xyz * r1.xyz;
  r1.xyz = r2.xyw + r1.xyz;
  r0.z = dot(r1.xyz, r1.xyz);
  r0.z = rsqrt(r0.z);
  r1.xyz = r1.xyz * r0.zzz;
  r1.xyz = r1.xyz;
  r2.xyz = g_tex_srm.Sample(g_sampler_srm_s, v2.xy).yxz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r3.xyz = v3.xyz;
  r4.xyz = v1.xyz;
  r3.xyz = r3.xyz;
  r4.xyz = r4.xyz;
  r5.xyzw = g_tex_diffuse.Sample(g_sampler_diffuse_s, v2.xy).xyzw;
  r6.xyz = float3(0.100000001,0.100000001,0.100000001) * g_psMaterial.m_materials[4].xyz;
  r6.xyz = -r6.xyz;
  r3.xyz = r6.xyz + r3.xyz;
  r0.z = dot(r3.xyz, r3.xyz);
  r0.z = sqrt(r0.z);
  r0.w = g_tex_metalicBacteria.Sample(g_sampler_metalicBacteria_s, v2.xy).y;
  r0.w = r0.w;
  r1.w = 0.100000001 * g_psMaterial.m_materials[5].x;
  r1.w = -r1.w;
  r3.x = r1.w + r0.z;
  r3.y = 0.100000001 * g_psMaterial.m_materials[5].y;
  r1.w = r3.y + r1.w;
  r1.w = r3.x / r1.w;
  r1.w = max(0, r1.w);
  r1.w = min(1, r1.w);
  r1.w = 2 * r1.w;
  r1.w = r1.w + r0.w;
  r1.w = -1 + r1.w;
  r1.w = max(0, r1.w);
  r1.w = min(1, r1.w);
  r1.w = r1.w;
  r3.x = cmp(g_psMaterial.m_materials[5].w != 0.000000);
  if (r3.x != 0) {
    r3.x = 0.100000001 * g_psMaterial.m_materials[5].z;
    r3.x = -r3.x;
    r3.y = r3.x + r0.z;
    r3.z = 0.100000001 * g_psMaterial.m_materials[5].w;
    r3.x = r3.z + r3.x;
    r3.x = r3.y / r3.x;
    r3.x = -r3.x;
    r3.x = 1 + r3.x;
    r3.x = max(0, r3.x);
    r3.x = min(1, r3.x);
    r0.w = r3.x * r0.w;
    r0.w = r1.w + r0.w;
  } else {
    r0.w = r1.w;
  }
  r3.xyzw = g_tex_metalicLayer.Sample(g_sampler_metalicLayer_s, v2.xy).wxyz;
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r6.y = r3.x * r0.w;
  r7.xyz = r6.yyy;
  r8.xyz = -r5.xyz;
  r3.xyz = r8.xyz + r3.yzw;
  r3.xyz = r7.xyz * r3.xyz;
  r3.xyz = r5.xyz + r3.xyz;
  r0.w = 0.100000001 * g_psMaterial.m_materials[6].x;
  r0.w = -r0.w;
  r0.z = r0.z + r0.w;
  r3.w = 0.100000001 * g_psMaterial.m_materials[6].y;
  r0.w = r3.w + r0.w;
  r0.z = r0.z / r0.w;
  r0.z = -r0.z;
  r0.z = 1 + r0.z;
  r0.w = -r1.w;
  r0.z = r0.z + r0.w;
  r0.w = g_psMaterial.m_materials[6].z;
  r0.z = r0.z * r0.w;
  r0.z = 1 + r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = r5.w * r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r6.xz = float2(0,0);
  r5.xyz = max(r6.xyz, r4.xyz);
  r3.xyz = r3.xyz;
  r0.z = r0.z;
  r5.xyz = r5.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r5.xyz = r5.xyz;
  r2.x = r2.x;
  r3.xyz = r3.xyz;
  r5.xyz = r5.xyz;
  r2.x = r2.x;
  r3.xyz = r3.xyz;
  r2.x = r2.x;
  r4.xyz = r4.xyz;
  r5.xyz = r5.xyz;
  r6.xyz = g_tex_Dirty.Sample(g_sampler_Dirty_s, v2.xy).xyz;
  r6.xyz = r6.xyz;
  r6.xyz = r6.xyz;
  r3.xyz = r3.xyz;
  r2.x = r2.x;
  r4.xyz = r4.xyz;
  r5.xyz = r5.xyz;
  r4.xyz = r4.xyz * r6.xyz;
  r5.xyz = r5.zxy * r6.zxy;
  r0.w = 255 * r2.x;
  r1.w = -205 + r0.w;
  r1.w = r1.w / 25;
  r1.w = -0.200000003 * r1.w;
  r1.w = 0.930000007 + r1.w;
  r1.w = max(0.730000019, r1.w);
  r1.w = min(0.930000007, r1.w);
  r0.w = -170 + r0.w;
  r0.w = r0.w / 60;
  r0.w = 0.660000026 * r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r0.w = min(0.660000026, r0.w);
  r6.xyz = r3.xyz * r1.www;
  r7.xyz = r4.zzz;
  r8.xyz = -r3.xyz;
  r6.xyz = r8.xyz + r6.xyz;
  r6.xyz = r7.xyz * r6.xyz;
  r3.xyz = r6.xyz + r3.xyz;
  r4.yzw = r4.yyy;
  r6.xyz = -r3.xyz;
  r6.xyz = float3(0.200000003,0.196078435,0.192156866) + r6.xyz;
  r4.yzw = r6.xyz * r4.yzw;
  r3.xyz = r4.yzw + r3.xyz;
  r4.xyz = r4.xxx;
  r6.xyz = -r3.xyz;
  r6.xyz = float3(0.2588,0.0470499992,0.043136999) + r6.xyz;
  r4.xyz = r6.xyz * r4.xyz;
  r3.xyz = r4.xyz + r3.xyz;
  r0.w = r2.x * r0.w;
  r1.w = -r2.x;
  r0.w = r1.w + r0.w;
  r0.w = r5.x * r0.w;
  r0.w = r2.x + r0.w;
  r1.w = -r0.w;
  r1.w = 0.980000019 + r1.w;
  r1.w = r5.z * r1.w;
  r0.w = r1.w + r0.w;
  r1.w = -r0.w;
  r1.w = 0.200000003 + r1.w;
  r1.w = r5.y * r1.w;
  r0.w = r1.w + r0.w;
  r3.xyz = r3.xyz;
  r0.w = r0.w;
  r3.xyz = r3.xyz;
  r0.w = r0.w;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r0.w = r0.w;
  r0.z = r0.z;
  r4.xyz = v7.xyz;
  r5.xyz = v4.xyz;
  r0.z = r0.z;
  r4.xyz = r4.xyz;
  r5.xyz = r5.xyz;
  r1.w = g_psMaterial.m_materials[3].y;
  r1.w = r1.w * r0.z;
  r4.x = dot(r4.xyz, r5.xyz);
  r4.x = -r4.x;
  r4.x = 1 + r4.x;
  r4.x = max(0, r4.x);
  r4.x = min(1, r4.x);
  r4.y = g_psMaterial.m_materials[3].x;
  r4.x = log2(r4.x);
  r4.x = r4.y * r4.x;
  r4.x = exp2(r4.x);
  r4.z = -r0.z;
  r1.w = r4.z + r1.w;
  r1.w = r4.x * r1.w;
  r3.w = r1.w + r0.z;
  r0.z = cmp(r4.y == 0.000000);
  if (r0.z != 0) {
    r3.w = 0;
  }
  r3.w = r3.w;
  r3.w = r3.w;
  r3.w = r3.w;
  r4.y = v1.w;
  r0.xy = r0.xy;
  r3.w = r3.w;
  r4.y = r4.y;
  r0.xy = r0.xy;
  r0.z = g_psSystem.m_param.w;
  r0.xy = float2(0.125,0.125) * r0.xy;
  r5.yz = g_tex_mesh.Sample(g_samplerPoint_Wrap_s, r0.xy).xw;
  r5.yz = r5.yz;
  r5.x = r5.z * r0.z;
  r4.x = r3.w;
  r0.xy = -r5.xy;
  r0.xy = r4.xy + r0.xy;
  r0.xy = cmp(r0.xy < float2(0,0));
  r0.x = (int)r0.y | (int)r0.x;
  r0.x = r0.x ? asfloat(0xFFFFFFFFu) : 0;
  if (r0.x != 0) discard;
  r3.w = r3.w;
  r3.w = r3.w;
  r2.x = r2.y;
  r2.x = r2.x;
  r2.x = r2.x;
  r2.x = r2.x;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r2.x = r2.x;
  r2.x = r2.x;
  r2.x = r2.x;
  r2.x = r2.x;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r0.xy = float2(0.5,0.5) * r1.xy;
  r4.xy = float2(0.5,0.5) + r0.xy;
  r0.x = 0.5 * r1.z;
  r0.x = 0.5 + r0.x;
  r4.z = sqrt(r0.x);
  r4.xy = r4.xy;
  r4.z = r4.z;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r3.xyz = r3.xyz;
  r4.xyz = r4.xyz;
  r2.y = r0.w;
  r2.x = r2.x;
  r2.z = r2.z;
  r3.w = r3.w;
  r3.xyz = r3.xyz;
  r4.xyz = r4.xyz;
  r2.y = r2.y;
  r2.x = r2.x;
  r2.z = r2.z;
  r3.w = r3.w;
  r3.xyz = r3.xyz;
  r3.w = r3.w;
  r4.xyz = r4.xyz;
  r4.w = r3.w;
  r2.y = r2.y;
  r2.x = r2.x;
  r2.z = r2.z;
  r2.w = r3.w;
  r3.xyzw = r3.xyzw;
  r4.xyzw = r4.xyzw;
  r2.xyzw = r2.xyzw;
  o0.xyzw = r3.xyzw;
  o1.xyzw = r4.xyzw;
  o2.xyzw = r2.yxzw;
  return;
}
// clang-format on

void main(
    float4 v0 : SV_Position0,
    float4 v1 : COLOR0,
    float4 v2 : TEXCOORD0,
    float4 v3 : TEXCOORD1,
    float4 v4 : TEXCOORD4,
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
