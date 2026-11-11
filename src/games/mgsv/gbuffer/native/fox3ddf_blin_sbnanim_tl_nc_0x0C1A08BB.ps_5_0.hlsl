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
SamplerState g_sampler_normal_s : register(s1);
SamplerState g_sampler_srm_s : register(s2);
SamplerState g_sampler_subNormal_s : register(s3);
SamplerState g_sampler_subNormalMask_s : register(s4);
SamplerState g_sampler_translucent_s : register(s5);
SamplerState g_samplerPoint_Wrap_s : register(s8);
Texture2D<float4> g_tex_diffuse : register(t0);
Texture2D<float4> g_tex_normal : register(t1);
Texture2D<float4> g_tex_srm : register(t2);
Texture2D<float4> g_tex_subNormal : register(t3);
Texture2D<float4> g_tex_subNormalMask : register(t4);
Texture2D<float4> g_tex_translucent : register(t5);
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
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9;
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
  r1.xy = v2.xy;
  r1.xy = r1.xy;
  r1.xy = r1.xy;
  r1.xy = r1.xy;
  r2.xyzw = g_tex_diffuse.Sample(g_sampler_diffuse_s, v2.xy).wxyz;
  r2.xyzw = r2.xyzw;
  r3.xyz = r2.yzw;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r4.z = g_psMaterial.m_materials[0].x;
  r4.z = r4.z;
  r1.xy = r1.xy;
  r0.z = r0.z;
  r1.xy = r1.xy;
  r0.z = r0.z;
  r1.xy = r1.xy;
  r1.zw = g_psMaterial.m_materials[1].yz;
  r2.zw = g_psMaterial.m_materials[2].xy;
  r0.w = g_tex_subNormalMask.Sample(g_sampler_subNormalMask_s, v2.xy).x;
  r5.xy = r0.ww;
  r5.zw = g_psMaterial.m_materials[1].xx;
  r0.z = r0.z;
  r6.xyz = v3.xyz;
  r7.xyz = v4.xyz;
  r8.xyz = v5.xyz;
  r8.xyz = r8.xyz * r0.zzz;
  r0.zw = g_tex_normal.Sample(g_sampler_normal_s, v2.xy).wy;
  r0.zw = r0.zw;
  r0.zw = float2(2,2) * r0.zw;
  r9.xy = float2(-1,-1) + r0.zw;
  r0.z = r9.x * r9.x;
  r0.z = -r0.z;
  r0.z = 1 + r0.z;
  r0.w = r9.y * r9.y;
  r0.w = -r0.w;
  r0.z = r0.z + r0.w;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = 9.99999975e-05 + r0.z;
  r0.w = rsqrt(r0.z);
  r9.z = r0.z * r0.w;
  r9.xy = r9.xy;
  r9.z = r9.z;
  r9.xyz = r9.xyz;
  r9.xyz = r9.xyz;
  r0.zw = r1.xy * r1.zw;
  r0.zw = r0.zw + r2.zw;
  r0.zw = g_tex_subNormal.Sample(g_sampler_subNormal_s, r0.zw).wy;
  r0.zw = r0.zw;
  r0.zw = float2(2,2) * r0.zw;
  r0.zw = float2(-1,-1) + r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw * r5.xy;
  r0.zw = r0.zw * r5.zw;
  r9.xy = r9.xy + r0.zw;
  r0.z = dot(r9.xyz, r9.xyz);
  r0.z = rsqrt(r0.z);
  r1.xyz = r9.xyz * r0.zzz;
  r0.z = dot(r6.xyz, r6.xyz);
  r0.z = rsqrt(r0.z);
  r5.xyz = r6.xyz * r0.zzz;
  r6.xyz = r1.xxx;
  r5.xyz = r6.xyz * r5.xyz;
  r0.z = dot(r7.xyz, r7.xyz);
  r0.z = rsqrt(r0.z);
  r6.xyz = r7.xyz * r0.zzz;
  r1.xyw = r1.yyy;
  r1.xyw = r6.xyz * r1.xyw;
  r1.xyw = r5.xyz + r1.xyw;
  r0.z = dot(r8.xyz, r8.xyz);
  r0.z = rsqrt(r0.z);
  r5.xyz = r8.xyz * r0.zzz;
  r6.xyz = r1.zzz;
  r5.xyz = r6.xyz * r5.xyz;
  r1.xyz = r5.xyz + r1.xyw;
  r0.z = dot(r1.xyz, r1.xyz);
  r0.z = rsqrt(r0.z);
  r1.xyz = r1.xyz * r0.zzz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r5.xyw = g_tex_srm.Sample(g_sampler_srm_s, v2.xy).xyz;
  r5.xyw = r5.xyw;
  r5.xyw = r5.xyw;
  r4.y = r5.x;
  r4.y = r4.y;
  r4.y = r4.y;
  r4.y = r4.y;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r4.x = r5.y;
  r3.xyz = r3.xyz;
  r4.x = r4.x;
  r3.xyz = r3.xyz;
  r4.x = r4.x;
  r3.xyz = r3.xyz;
  r4.x = r4.x;
  r4.w = g_tex_translucent.Sample(g_sampler_translucent_s, v2.xy).y;
  r4.w = r4.w;
  r4.w = r4.w;
  r2.x = r2.x;
  r2.x = r2.x;
  r2.x = r2.x;
  r2.x = r2.x;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r2.x = r2.x;
  r2.y = v1.w;
  r0.xy = r0.xy;
  r2.x = r2.x;
  r2.y = r2.y;
  r0.xy = r0.xy;
  r0.z = g_psSystem.m_param.w;
  r0.xy = float2(0.125,0.125) * r0.xy;
  r0.yw = g_tex_mesh.Sample(g_samplerPoint_Wrap_s, r0.xy).xw;
  r0.yw = r0.yw;
  r0.x = r0.w * r0.z;
  r0.xy = -r0.xy;
  r0.xy = r2.xy + r0.xy;
  r0.xy = cmp(r0.xy < float2(0,0));
  r0.x = (int)r0.y | (int)r0.x;
  r0.x = r0.x ? asfloat(0xFFFFFFFFu) : 0;
  if (r0.x != 0) discard;
  r4.y = r4.y;
  r4.y = r4.y;
  r4.y = r4.y;
  r4.y = r4.y;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r0.xy = float2(0.5,0.5) * r1.xy;
  r5.xy = float2(0.5,0.5) + r0.xy;
  r0.x = 0.5 * r1.z;
  r0.x = 0.5 + r0.x;
  r5.z = sqrt(r0.x);
  r5.xy = r5.xy;
  r5.z = r5.z;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz;
  r3.xyz = r3.xyz;
  r4.w = r4.w;
  r5.xyz = r5.xyz;
  r4.x = r4.x;
  r4.y = r4.y;
  r5.w = r5.w;
  r4.z = r4.z;
  r3.xyz = r3.xyz;
  r4.w = r4.w;
  r5.xyz = r5.xyz;
  r4.x = r4.x;
  r4.y = r4.y;
  r5.w = r5.w;
  r4.z = r4.z;
  r3.xyz = r3.xyz;
  r3.w = 1;
  r5.xyz = r5.xyz;
  r5.w = r5.w;
  r4.x = r4.x;
  r4.y = r4.y;
  r4.z = r4.z;
  r4.w = r4.w;
  r3.xyzw = r3.xyzw;
  r5.xyzw = r5.xyzw;
  r4.xyzw = r4.xyzw;
  o0.xyzw = r3.xyzw;
  o1.xyzw = r5.xyzw;
  o2.xyzw = r4.xyzw;
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
