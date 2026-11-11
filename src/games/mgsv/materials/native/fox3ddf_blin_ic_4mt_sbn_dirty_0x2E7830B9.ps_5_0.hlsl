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
SamplerState g_sampler_subNormal_s : register(s4);
SamplerState g_sampler_subNormalMask_s : register(s5);
SamplerState g_sampler_Dirty_s : register(s6);
SamplerState g_samplerPoint_Wrap_s : register(s8);
Texture2D<float4> g_tex_diffuse : register(t0);
Texture2D<float4> g_tex_normal : register(t1);
Texture2D<float4> g_tex_srm : register(t2);
Texture2D<float4> g_tex_materialmap : register(t3);
Texture2D<float4> g_tex_subNormal : register(t4);
Texture2D<float4> g_tex_subNormalMask : register(t5);
Texture2D<float4> g_tex_Dirty : register(t6);
Texture2D<float4> g_tex_mesh : register(t15);


// 3Dmigoto declarations
#define cmp -


void NativeMaterialMain(
  float4 v0 : SV_Position0,
  float4 v1 : COLOR0,
  float4 v2 : TEXCOORD0,
  float4 v3 : TEXCOORD4,
  float4 v4 : TEXCOORD5,
  float4 v5 : TEXCOORD6,
  float3 v6 : TEXCOORD7,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1,
  out float4 o2 : SV_Target2)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v0.xy;
  r0.zw = v2.xy;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r1.x = g_psMaterial.m_materials[0].x;
  r1.y = g_tex_materialmap.Sample(g_samplerPoint_Wrap_s, v2.xy).x;
  r1.y = r1.y;
  r1.z = cmp(r1.y >= 0.25);
  r1.z = r1.z ? 1 : 0;
  r1.w = -r1.z;
  r1.w = 1 + r1.w;
  r1.x = r1.w * r1.x;
  r1.w = g_psMaterial.m_materials[0].y;
  r1.z = r1.z * r1.w;
  r1.x = r1.x + r1.z;
  r1.z = cmp(r1.y >= 0.5);
  r1.z = r1.z ? 1 : 0;
  r1.w = -r1.z;
  r1.w = 1 + r1.w;
  r1.x = r1.w * r1.x;
  r1.w = g_psMaterial.m_materials[0].z;
  r1.z = r1.z * r1.w;
  r1.x = r1.x + r1.z;
  r1.y = cmp(r1.y >= 0.75);
  r1.y = r1.y ? 1 : 0;
  r1.z = -r1.y;
  r1.z = 1 + r1.z;
  r1.x = r1.z * r1.x;
  r1.z = g_psMaterial.m_materials[0].w;
  r1.y = r1.y * r1.z;
  r1.z = r1.x + r1.y;
  r1.z = r1.z;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r2.xy = g_psMaterial.m_materials[1].yz;
  r2.z = g_tex_subNormalMask.Sample(g_sampler_subNormalMask_s, v2.xy).x;
  r2.zw = r2.zz;
  r3.xy = g_psMaterial.m_materials[1].xx;
  r4.xyz = v4.xyz;
  r5.xyz = v5.xyz;
  r6.xyz = v6.xyz;
  r6.xyz = float3(1,1,1) * r6.xyz;
  r3.zw = g_tex_normal.Sample(g_sampler_normal_s, v2.xy).wy;
  r3.zw = r3.zw;
  r3.zw = float2(2,2) * r3.zw;
  r7.xy = float2(-1,-1) + r3.zw;
  r3.z = r7.x * r7.x;
  r3.z = -r3.z;
  r3.z = 1 + r3.z;
  r3.w = r7.y * r7.y;
  r3.w = -r3.w;
  r3.z = r3.z + r3.w;
  r3.z = max(0, r3.z);
  r3.z = min(1, r3.z);
  r3.z = 9.99999975e-05 + r3.z;
  r3.w = rsqrt(r3.z);
  r7.z = r3.z * r3.w;
  r7.xy = r7.xy;
  r7.z = r7.z;
  r7.xyz = r7.xyz;
  r7.xyz = r7.xyz;
  r0.zw = r2.xy * r0.zw;
  r0.zw = g_tex_subNormal.Sample(g_sampler_subNormal_s, r0.zw).wy;
  r0.zw = r0.zw;
  r0.zw = float2(2,2) * r0.zw;
  r0.zw = float2(-1,-1) + r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw * r2.zw;
  r0.zw = r0.zw * r3.xy;
  r7.xy = r7.xy + r0.zw;
  r0.z = dot(r7.xyz, r7.xyz);
  r0.z = rsqrt(r0.z);
  r2.xyz = r7.xyz * r0.zzz;
  r0.z = dot(r4.xyz, r4.xyz);
  r0.z = rsqrt(r0.z);
  r3.xyz = r4.xyz * r0.zzz;
  r4.xyz = r2.xxx;
  r3.xyz = r4.xyz * r3.xyz;
  r0.z = dot(r5.xyz, r5.xyz);
  r0.z = rsqrt(r0.z);
  r4.xyz = r5.xyz * r0.zzz;
  r2.xyw = r2.yyy;
  r2.xyw = r4.xyz * r2.xyw;
  r2.xyw = r3.xyz + r2.xyw;
  r0.z = dot(r6.xyz, r6.xyz);
  r0.z = rsqrt(r0.z);
  r3.xyz = r6.xyz * r0.zzz;
  r4.xyz = r2.zzz;
  r3.xyz = r4.xyz * r3.xyz;
  r2.xyz = r3.xyz + r2.xyw;
  r0.z = dot(r2.xyz, r2.xyz);
  r0.z = rsqrt(r0.z);
  r2.xyz = r2.xyz * r0.zzz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r3.xyz = g_tex_srm.Sample(g_sampler_srm_s, v2.xy).yxz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r1.y = r3.y;
  r1.y = r1.y;
  r1.y = r1.y;
  r1.y = r1.y;
  r4.xyzw = g_tex_diffuse.Sample(g_sampler_diffuse_s, v2.xy).xyzw;
  r4.xyzw = r4.xyzw;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
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
  r5.xyz = v3.xyz;
  r1.y = r1.y;
  r4.xyz = r4.xyz;
  r2.xyz = r2.xyz;
  r5.xyz = r5.xyz;
  r1.y = r1.y;
  r6.xyz = g_psMaterial.m_materials[3].xyz;
  r0.z = g_psMaterial.m_materials[3].w * r1.y;
  r0.w = dot(r5.xyz, r5.xyz);
  r0.w = rsqrt(r0.w);
  r5.xyz = r5.xyz * r0.www;
  r0.w = dot(r2.xyz, r5.xyz);
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r2.w = g_psMaterial.m_materials[2].x;
  r0.w = log2(r0.w);
  r0.w = r2.w * r0.w;
  r0.w = exp2(r0.w);
  r0.z = r0.z * r0.w;
  r5.xyz = -r4.xyz;
  r5.xyz = r6.xyz + r5.xyz;
  r5.xyz = r5.xyz * r0.zzz;
  r5.xyz = r5.xyz + r4.xyz;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz;
  r3.x = r3.x;
  r6.xyz = v1.xyz;
  r5.xyz = r5.xyz;
  r3.x = r3.x;
  r6.xyz = r6.xyz;
  r5.xyz = r5.xyz;
  r3.x = r3.x;
  r6.xyz = r6.xyz;
  r7.xyz = g_tex_Dirty.Sample(g_sampler_Dirty_s, v2.xy).xyz;
  r7.xyz = r7.xyz;
  r7.xyz = r7.xyz;
  r5.xyz = r5.xyz;
  r3.x = r3.x;
  r6.xyz = r6.xyz;
  r6.xyz = r6.xyz * r7.xyz;
  r0.z = 255 * r3.x;
  r0.w = -205 + r0.z;
  r0.w = r0.w / 25;
  r0.w = -0.200000003 * r0.w;
  r0.w = 0.930000007 + r0.w;
  r0.w = max(0.730000019, r0.w);
  r0.w = min(0.930000007, r0.w);
  r0.z = -170 + r0.z;
  r0.z = r0.z / 60;
  r0.z = 0.660000026 * r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = min(0.660000026, r0.z);
  r7.xyz = r5.xyz * r0.www;
  r8.xyz = r6.zzz;
  r9.xyz = -r5.xyz;
  r7.xyz = r9.xyz + r7.xyz;
  r7.xyz = r8.yzx * r7.xyz;
  r5.xyz = r7.xyz + r5.xyz;
  r6.yzw = r6.yyy;
  r7.xyz = -r5.xyz;
  r7.xyz = float3(0.200000003,0.196078435,0.192156866) + r7.xyz;
  r7.xyz = r7.xyz * r6.zyw;
  r5.xyz = r7.xyz + r5.xyz;
  r6.xzw = r6.xxx;
  r7.xyz = -r5.xyz;
  r7.xyz = float3(0.2588,0.0470499992,0.043136999) + r7.xyz;
  r7.xyz = r7.xyz * r6.xzw;
  r5.xyz = r7.xyz + r5.xyz;
  r0.z = r3.x * r0.z;
  r0.w = -r3.x;
  r0.z = r0.z + r0.w;
  r0.z = r8.x * r0.z;
  r0.z = r3.x + r0.z;
  r0.w = -r0.z;
  r0.w = 0.980000019 + r0.w;
  r0.w = r6.y * r0.w;
  r0.z = r0.z + r0.w;
  r0.w = -r0.z;
  r0.w = 0.200000003 + r0.w;
  r0.w = r6.x * r0.w;
  r1.x = r0.z + r0.w;
  r5.xyz = r5.xyz;
  r1.x = r1.x;
  r5.xyz = r5.xyz;
  r1.x = r1.x;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz;
  r1.x = r1.x;
  r4.w = r4.w;
  r4.w = r4.w;
  r4.w = r4.w;
  r4.w = r4.w;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz;
  r4.w = r4.w;
  r4.x = v1.w;
  r0.xy = r0.xy;
  r4.w = r4.w;
  r4.x = r4.x;
  r0.xy = r0.xy;
  r0.z = g_psSystem.m_param.w;
  r0.xy = float2(0.125,0.125) * r0.xy;
  r0.yw = g_tex_mesh.Sample(g_samplerPoint_Wrap_s, r0.xy).xw;
  r0.yw = r0.yw;
  r0.x = r0.w * r0.z;
  r0.xy = -r0.xy;
  r0.xy = r4.wx + r0.xy;
  r0.xy = cmp(r0.xy < float2(0,0));
  r0.x = (int)r0.y | (int)r0.x;
  r0.x = r0.x ? asfloat(0xFFFFFFFFu) : 0;
  if (r0.x != 0) discard;
  r1.y = r1.y;
  r1.y = r1.y;
  r2.xyz = r2.xyz;
  r0.xy = float2(0.5,0.5) * r2.xy;
  r3.xy = float2(0.5,0.5) + r0.yx;
  r0.x = 0.5 * r2.z;
  r0.x = 0.5 + r0.x;
  r3.w = sqrt(r0.x);
  r3.xy = r3.xy;
  r3.w = r3.w;
  r3.xyw = r3.xyw;
  r3.xyw = r3.xyw;
  r5.xyz = r5.xyz;
  r3.xyw = r3.xyw;
  r1.x = r1.x;
  r1.y = r1.y;
  r3.z = r3.z;
  r1.z = r1.z;
  r5.xyz = r5.xyz;
  r3.xyw = r3.xyw;
  r1.x = r1.x;
  r1.y = r1.y;
  r3.z = r3.z;
  r1.z = r1.z;
  r5.xyz = r5.xyz;
  r5.w = 1;
  r3.xyw = r3.xyw;
  r3.z = r3.z;
  r1.x = r1.x;
  r1.y = r1.y;
  r1.z = r1.z;
  r1.w = 0;
  r5.xyzw = r5.xyzw;
  r3.xyzw = r3.xyzw;
  r1.xyzw = r1.xyzw;
  o0.xyzw = r5.xyzw;
  o1.xyzw = r3.yxwz;
  o2.xyzw = r1.xyzw;
  return;
}
// clang-format on

void main(
    float4 v0 : SV_Position0,
    float4 v1 : COLOR0,
    float4 v2 : TEXCOORD0,
    float4 v3 : TEXCOORD4,
    float4 v4 : TEXCOORD5,
    float4 v5 : TEXCOORD6,
    float3 v6 : TEXCOORD7,
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
