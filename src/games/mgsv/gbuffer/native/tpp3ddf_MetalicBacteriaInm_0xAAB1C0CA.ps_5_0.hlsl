#include "../material_capture.hlsli"

// clang-format off
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:31 2026

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
SamplerState g_sampler_translucent_s : register(s3);
SamplerState g_sampler_metalicLayer_s : register(s4);
SamplerState g_sampler_metalicBacteria_s : register(s5);
SamplerState g_sampler_subNormal_s : register(s6);
SamplerState g_sampler_Dirty_s : register(s7);
SamplerState g_samplerPoint_Wrap_s : register(s8);
Texture2D<float4> g_tex_diffuse : register(t0);
Texture2D<float4> g_tex_normal : register(t1);
Texture2D<float4> g_tex_srm : register(t2);
Texture2D<float4> g_tex_translucent : register(t3);
Texture2D<float4> g_tex_metalicLayer : register(t4);
Texture2D<float4> g_tex_metalicBacteria : register(t5);
Texture2D<float4> g_tex_subNormal : register(t6);
Texture2D<float4> g_tex_Dirty : register(t7);
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
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v0.xy;
  r1.z = g_psMaterial.m_materials[0].x;
  r1.z = r1.z;
  r2.xyz = v3.xyz;
  r3.xyz = v1.xyz;
  r2.xyz = r2.xyz;
  r3.xyz = r3.xyz;
  r4.xyzw = g_tex_diffuse.Sample(g_sampler_diffuse_s, v2.xy).xyzw;
  r5.xyz = float3(0.100000001,0.100000001,0.100000001) * g_psMaterial.m_materials[4].xyz;
  r5.xyz = -r5.xyz;
  r2.xyz = r5.xyz + r2.xyz;
  r0.z = dot(r2.xyz, r2.xyz);
  r0.z = sqrt(r0.z);
  r0.w = g_tex_metalicBacteria.Sample(g_sampler_metalicBacteria_s, v2.xy).y;
  r0.w = r0.w;
  r2.x = 0.100000001 * g_psMaterial.m_materials[5].x;
  r2.x = -r2.x;
  r2.y = r2.x + r0.z;
  r2.z = 0.100000001 * g_psMaterial.m_materials[5].y;
  r2.x = r2.z + r2.x;
  r2.x = r2.y / r2.x;
  r2.x = max(0, r2.x);
  r2.x = min(1, r2.x);
  r2.x = 2 * r2.x;
  r2.x = r2.x + r0.w;
  r2.x = -1 + r2.x;
  r2.x = max(0, r2.x);
  r2.x = min(1, r2.x);
  r2.x = r2.x;
  r2.y = cmp(g_psMaterial.m_materials[5].w != 0.000000);
  if (r2.y != 0) {
    r2.y = 0.100000001 * g_psMaterial.m_materials[5].z;
    r2.y = -r2.y;
    r2.z = r2.y + r0.z;
    r2.w = 0.100000001 * g_psMaterial.m_materials[5].w;
    r2.y = r2.w + r2.y;
    r2.y = r2.z / r2.y;
    r2.y = -r2.y;
    r2.y = 1 + r2.y;
    r2.y = max(0, r2.y);
    r2.y = min(1, r2.y);
    r0.w = r2.y * r0.w;
    r0.w = r2.x + r0.w;
  } else {
    r0.w = r2.x;
  }
  r5.xyzw = g_tex_metalicLayer.Sample(g_sampler_metalicLayer_s, v2.xy).wxyz;
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r2.w = r5.x * r0.w;
  r2.yzw = r2.www;
  r6.xyz = -r4.xyz;
  r5.xyz = r6.xyz + r5.yzw;
  r5.xyz = r5.xyz * r2.yzw;
  r4.xyz = r5.xyz + r4.xyz;
  r0.w = 0.100000001 * g_psMaterial.m_materials[6].x;
  r0.w = -r0.w;
  r0.z = r0.z + r0.w;
  r2.z = 0.100000001 * g_psMaterial.m_materials[6].y;
  r0.w = r2.z + r0.w;
  r0.z = r0.z / r0.w;
  r0.z = -r0.z;
  r0.z = 1 + r0.z;
  r0.w = -r2.x;
  r0.z = r0.z + r0.w;
  r0.w = g_psMaterial.m_materials[6].z;
  r0.z = r0.z * r0.w;
  r0.z = 1 + r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = r4.w * r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r3.xyz = r3.xyz;
  r2.y = r2.y;
  r4.xyz = r4.xyz;
  r0.z = r0.z;
  r3.xyz = r3.xyz;
  r2.y = r2.y;
  r2.y = r2.y;
  r2.y = r2.y;
  r2.y = r2.y;
  r2.xzw = v5.xyz;
  r5.xyz = v6.xyz;
  r6.xyz = v7.xyz;
  r7.xyz = float3(1,1,1) * r6.xyz;
  r8.xy = g_tex_normal.Sample(g_sampler_normal_s, v2.xy).wy;
  r8.xy = r8.xy;
  r8.xy = float2(2,2) * r8.xy;
  r8.xy = float2(-1,-1) + r8.xy;
  r0.w = r8.x * r8.x;
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r3.w = r8.y * r8.y;
  r3.w = -r3.w;
  r0.w = r3.w + r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r0.w = 9.99999975e-05 + r0.w;
  r3.w = rsqrt(r0.w);
  r8.z = r3.w * r0.w;
  r8.xy = r8.xy;
  r8.z = r8.z;
  r8.xyz = r8.xyz;
  r8.xyz = r8.xyz;
  r9.xy = g_tex_subNormal.Sample(g_sampler_subNormal_s, v2.xy).wy;
  r9.xy = r9.xy;
  r9.xy = float2(2,2) * r9.xy;
  r9.xy = float2(-1,-1) + r9.xy;
  r0.w = r9.x * r9.x;
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r3.w = r9.y * r9.y;
  r3.w = -r3.w;
  r0.w = r3.w + r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r0.w = 9.99999975e-05 + r0.w;
  r3.w = rsqrt(r0.w);
  r9.z = r3.w * r0.w;
  r9.xy = r9.xy;
  r9.z = r9.z;
  r9.xyz = r9.xyz;
  r9.xyz = r9.xyz;
  r10.xyz = -r8.xyz;
  r9.xyz = r10.xyz + r9.xyz;
  r9.xyz = r9.xyz * r2.yyy;
  r8.xyz = r9.xyz + r8.xyz;
  r0.w = dot(r8.xyz, r8.xyz);
  r0.w = rsqrt(r0.w);
  r8.xyz = r8.xyz * r0.www;
  r0.w = dot(r2.xzw, r2.xzw);
  r0.w = rsqrt(r0.w);
  r2.xyz = r2.xzw * r0.www;
  r9.xyz = r8.xxx;
  r2.xyz = r9.xyz * r2.xyz;
  r0.w = dot(r5.xyz, r5.xyz);
  r0.w = rsqrt(r0.w);
  r5.xyz = r5.xyz * r0.www;
  r8.xyw = r8.yyy;
  r5.xyz = r8.xyw * r5.xyz;
  r2.xyz = r5.xyz + r2.xyz;
  r0.w = dot(r7.xyz, r7.xyz);
  r0.w = rsqrt(r0.w);
  r5.xyz = r7.xyz * r0.www;
  r7.xyz = r8.zzz;
  r5.xyz = r7.xyz * r5.xyz;
  r2.xyz = r5.xyz + r2.xyz;
  r0.w = dot(r2.xyz, r2.xyz);
  r0.w = rsqrt(r0.w);
  r2.xyz = r2.xyz * r0.www;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r5.xyz = g_tex_srm.Sample(g_sampler_srm_s, v2.xy).yxz;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r3.xyz = r3.xyz;
  r5.x = r5.x;
  r4.xyz = r4.xyz;
  r7.xyz = r3.xyz;
  r5.x = r5.x;
  r4.xyz = r4.xyz;
  r5.x = r5.x;
  r7.xyz = r7.xyz;
  r8.xyz = g_tex_Dirty.Sample(g_sampler_Dirty_s, v2.xy).xyz;
  r8.xyz = r8.xyz;
  r8.xyz = r8.xyz;
  r4.xyz = r4.xyz;
  r5.x = r5.x;
  r7.xyz = r7.xyz;
  r3.xyz = r8.xyz * r3.xyz;
  r7.xyz = r7.zxy * r8.zxy;
  r0.w = 255 * r5.x;
  r2.w = -205 + r0.w;
  r2.w = r2.w / 25;
  r2.w = -0.200000003 * r2.w;
  r2.w = 0.930000007 + r2.w;
  r2.w = max(0.730000019, r2.w);
  r2.w = min(0.930000007, r2.w);
  r0.w = -170 + r0.w;
  r0.w = r0.w / 60;
  r0.w = 0.660000026 * r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r0.w = min(0.660000026, r0.w);
  r8.xyz = r4.xyz * r2.www;
  r9.xyz = r3.zzz;
  r10.xyz = -r4.xyz;
  r8.xyz = r10.xyz + r8.xyz;
  r8.xyz = r9.xyz * r8.xyz;
  r4.xyz = r8.xyz + r4.xyz;
  r3.yzw = r3.yyy;
  r8.xyz = -r4.xyz;
  r8.xyz = float3(0.200000003,0.196078435,0.192156866) + r8.xyz;
  r3.yzw = r8.xyz * r3.yzw;
  r3.yzw = r4.xyz + r3.yzw;
  r4.xyz = r3.xxx;
  r8.xyz = -r3.yzw;
  r8.xyz = float3(0.2588,0.0470499992,0.043136999) + r8.xyz;
  r4.xyz = r8.xyz * r4.xyz;
  r3.xyz = r4.xyz + r3.yzw;
  r0.w = r5.x * r0.w;
  r2.w = -r5.x;
  r0.w = r2.w + r0.w;
  r0.w = r7.x * r0.w;
  r0.w = r5.x + r0.w;
  r2.w = -r0.w;
  r2.w = 0.980000019 + r2.w;
  r2.w = r7.z * r2.w;
  r0.w = r2.w + r0.w;
  r2.w = -r0.w;
  r2.w = 0.200000003 + r2.w;
  r2.w = r7.y * r2.w;
  r1.x = r2.w + r0.w;
  r3.xyz = r3.xyz;
  r1.x = r1.x;
  r3.xyz = r3.xyz;
  r1.x = r1.x;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r1.x = r1.x;
  r1.w = g_tex_translucent.Sample(g_sampler_translucent_s, v2.xy).y;
  r1.w = r1.w;
  r1.w = r1.w;
  r0.z = r0.z;
  r4.xyz = v4.xyz;
  r0.z = r0.z;
  r6.xyz = r6.xyz;
  r4.xyz = r4.xyz;
  r0.w = g_psMaterial.m_materials[3].y;
  r0.w = r0.z * r0.w;
  r2.w = dot(r6.xyz, r4.xyz);
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
  r6.x = r0.z + r0.w;
  r0.z = cmp(r3.w == 0.000000);
  if (r0.z != 0) {
    r6.x = 0;
  }
  r6.x = r6.x;
  r6.x = r6.x;
  r6.x = r6.x;
  r6.y = v1.w;
  r0.xy = r0.xy;
  r6.x = r6.x;
  r6.y = r6.y;
  r0.xy = r0.xy;
  r0.z = g_psSystem.m_param.w;
  r0.xy = float2(0.125,0.125) * r0.xy;
  r0.yw = g_tex_mesh.Sample(g_samplerPoint_Wrap_s, r0.xy).xw;
  r0.yw = r0.yw;
  r0.x = r0.w * r0.z;
  r0.xy = -r0.xy;
  r0.xy = r6.xy + r0.xy;
  r0.xy = cmp(r0.xy < float2(0,0));
  r0.x = (int)r0.y | (int)r0.x;
  r0.x = r0.x ? asfloat(0xFFFFFFFFu) : 0;
  if (r0.x != 0) discard;
  r1.y = r5.y;
  r1.y = r1.y;
  r1.y = r1.y;
  r1.y = r1.y;
  r3.xyz = r3.xyz;
  r2.xyz = r2.xyz;
  r1.y = r1.y;
  r3.xyz = r3.xyz;
  r2.xyz = r2.xyz;
  r1.y = r1.y;
  r0.xyz = g_psMaterial.m_materials[2].xyz;
  r0.w = g_psMaterial.m_materials[2].w * r1.y;
  r2.w = dot(r4.xyz, r4.xyz);
  r2.w = rsqrt(r2.w);
  r4.xyz = r4.xyz * r2.www;
  r2.w = dot(r2.xyz, r4.xyz);
  r2.w = -r2.w;
  r2.w = 1 + r2.w;
  r2.w = max(0, r2.w);
  r2.w = min(1, r2.w);
  r3.w = g_psMaterial.m_materials[1].x;
  r2.w = log2(r2.w);
  r2.w = r3.w * r2.w;
  r2.w = exp2(r2.w);
  r0.w = r2.w * r0.w;
  r4.xyz = -r3.xyz;
  r0.xyz = r4.xyz + r0.xyz;
  r0.xyz = r0.www * r0.xyz;
  r0.xyz = r3.xyz + r0.xyz;
  r0.xyz = r0.xyz;
  r1.y = r1.y;
  r1.y = r1.y;
  r2.xyz = r2.xyz;
  r2.xy = float2(0.5,0.5) * r2.xy;
  r5.xy = float2(0.5,0.5) + r2.yx;
  r2.x = 0.5 * r2.z;
  r2.x = 0.5 + r2.x;
  r5.w = sqrt(r2.x);
  r5.xy = r5.xy;
  r5.w = r5.w;
  r5.xyw = r5.xyw;
  r5.xyw = r5.xyw;
  r0.xyz = r0.xyz;
  r1.w = r1.w;
  r5.xyw = r5.xyw;
  r1.x = r1.x;
  r1.y = r1.y;
  r5.z = r5.z;
  r1.z = r1.z;
  r0.xyz = r0.xyz;
  r1.w = r1.w;
  r5.xyw = r5.xyw;
  r1.x = r1.x;
  r1.y = r1.y;
  r5.z = r5.z;
  r1.z = r1.z;
  r0.xyz = r0.xyz;
  r0.w = 1;
  r5.xyw = r5.xyw;
  r5.z = r5.z;
  r1.x = r1.x;
  r1.y = r1.y;
  r1.z = r1.z;
  r1.w = r1.w;
  r0.xyzw = r0.xyzw;
  r5.xyzw = r5.xyzw;
  r1.xyzw = r1.xyzw;
  o0.xyzw = r0.xyzw;
  o1.xyzw = r5.yxwz;
  o2.xyzw = r1.xyzw;
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
