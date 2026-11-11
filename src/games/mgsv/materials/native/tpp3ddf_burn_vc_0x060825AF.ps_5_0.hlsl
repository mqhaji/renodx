#include "../material_capture.hlsli"

// clang-format off
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:20 2026

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
SamplerState g_sampler_SubNorm_s : register(s3);
SamplerState g_sampler_Burn_s : register(s4);
SamplerState g_sampler_Charcoal_s : register(s5);
SamplerState g_samplerPoint_Wrap_s : register(s8);
Texture2D<float4> g_tex_diffuse : register(t0);
Texture2D<float4> g_tex_normal : register(t1);
Texture2D<float4> g_tex_srm : register(t2);
Texture2D<float4> g_tex_SubNorm : register(t3);
Texture2D<float4> g_tex_Burn : register(t4);
Texture2D<float4> g_tex_Charcoal : register(t5);
Texture2D<float4> g_tex_mesh : register(t15);


// 3Dmigoto declarations
#define cmp -


void NativeMaterialMain(
  float4 v0 : SV_Position0,
  float4 v1 : COLOR0,
  float4 v2 : TEXCOORD0,
  float2 v3 : TEXCOORD1,
  float2 w3 : TEXCOORD9,
  float4 v4 : TEXCOORD5,
  float4 v5 : TEXCOORD6,
  float4 v6 : TEXCOORD7,
  float4 v7 : TEXCOORD8,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1,
  out float4 o2 : SV_Target2)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v0.xy;
  r1.xyzw = g_tex_diffuse.Sample(g_sampler_diffuse_s, v2.xy).xyzw;
  r1.xyzw = r1.xyzw;
  r2.z = g_psMaterial.m_materials[0].x;
  r2.z = r2.z;
  r3.xyz = g_tex_srm.Sample(g_sampler_srm_s, v2.xy).yxz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r1.xyz = r1.xyz;
  r3.x = r3.x;
  r0.z = v1.x;
  r1.xyz = r1.xyz;
  r3.x = r3.x;
  r0.z = r0.z;
  r4.xyzw = g_tex_Burn.Sample(g_sampler_Burn_s, v3.xy).xyzw;
  r5.xyz = g_tex_Charcoal.Sample(g_sampler_Charcoal_s, v3.xy).xyz;
  r5.xyz = r5.xyz;
  r0.w = -0.25 + r0.z;
  r0.w = 4 * r0.w;
  r0.w = r0.w + r4.w;
  r0.w = max(0, r0.w);
  r6.z = min(1, r0.w);
  r0.z = -0.75 + r0.z;
  r0.z = 4 * r0.z;
  r0.z = r0.z + r4.w;
  r0.z = max(0, r0.z);
  r7.xyz = min(float3(1,1,1), r0.zzz);
  r0.z = g_psMaterial.m_materials[2].y;
  r0.z = r0.z * r6.z;
  r0.z = r0.z + r3.x;
  r0.z = max(0, r0.z);
  r2.x = min(1, r0.z);
  r8.xyz = g_psMaterial.m_materials[2].xxx;
  r9.xyz = -r1.xyz;
  r4.xyz = r9.xyz + r4.xyz;
  r4.xyz = r8.xyz * r4.xyz;
  r4.xyz = r4.xyz + r1.xyz;
  r6.xyz = r6.zzz;
  r4.xyz = r4.xyz + r9.xyz;
  r4.xyz = r6.xyz * r4.xyz;
  r4.xyz = r4.xyz + r1.xyz;
  r6.yzw = -r4.xyz;
  r5.xyz = r6.yzw + r5.xyz;
  r5.xyz = r7.xyz * r5.xyz;
  r4.xyz = r5.xyz + r4.xyz;
  r4.xyz = r4.xyz;
  r2.x = r2.x;
  r6.x = r6.x;
  r1.w = r1.w;
  r1.x = v1.w;
  r0.xy = r0.xy;
  r1.w = r1.w;
  r1.x = r1.x;
  r0.xy = r0.xy;
  r0.z = g_psSystem.m_param.w;
  r0.xy = float2(0.125,0.125) * r0.xy;
  r0.yw = g_tex_mesh.Sample(g_samplerPoint_Wrap_s, r0.xy).xw;
  r0.yw = r0.yw;
  r0.x = r0.w * r0.z;
  r0.xy = -r0.xy;
  r0.xy = r1.wx + r0.xy;
  r0.xy = cmp(r0.xy < float2(0,0));
  r0.x = (int)r0.y | (int)r0.x;
  r0.x = r0.x ? asfloat(0xFFFFFFFFu) : 0;
  if (r0.x != 0) discard;
  r0.xy = v2.zw;
  r6.x = r6.x;
  r0.xy = r0.xy;
  r6.x = r6.x;
  r0.xy = r0.xy;
  r0.zw = g_psMaterial.m_materials[1].yz;
  r1.xy = g_psMaterial.m_materials[1].xx * r6.xx;
  r5.xyz = v4.xyz;
  r6.xyz = v5.xyz;
  r7.xyz = v6.xyz;
  r7.xyz = float3(1,1,1) * r7.xyz;
  r1.zw = g_tex_normal.Sample(g_sampler_normal_s, v2.zw).wy;
  r1.zw = r1.zw;
  r1.zw = float2(2,2) * r1.zw;
  r8.xy = float2(-1,-1) + r1.zw;
  r1.z = r8.x * r8.x;
  r1.z = -r1.z;
  r1.z = 1 + r1.z;
  r1.w = r8.y * r8.y;
  r1.w = -r1.w;
  r1.z = r1.z + r1.w;
  r1.z = max(0, r1.z);
  r1.z = min(1, r1.z);
  r1.z = 9.99999975e-05 + r1.z;
  r1.w = rsqrt(r1.z);
  r8.z = r1.z * r1.w;
  r8.xy = r8.xy;
  r8.z = r8.z;
  r8.xyz = r8.xyz;
  r8.xyz = r8.xyz;
  r0.xy = r0.xy * r0.zw;
  r0.xy = g_tex_SubNorm.Sample(g_sampler_SubNorm_s, r0.xy).wy;
  r0.xy = r0.xy;
  r0.xy = float2(2,2) * r0.xy;
  r0.xy = float2(-1,-1) + r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = float2(1,1) * r0.xy;
  r0.xy = r0.xy * r1.xy;
  r8.xy = r8.xy + r0.xy;
  r0.x = dot(r8.xyz, r8.xyz);
  r0.x = rsqrt(r0.x);
  r0.xyz = r8.xyz * r0.xxx;
  r0.w = dot(r5.xyz, r5.xyz);
  r0.w = rsqrt(r0.w);
  r1.xyz = r5.xyz * r0.www;
  r5.xyz = r0.xxx;
  r1.xyz = r5.xyz * r1.xyz;
  r0.x = dot(r6.xyz, r6.xyz);
  r0.x = rsqrt(r0.x);
  r5.xyz = r6.xyz * r0.xxx;
  r0.xyw = r0.yyy;
  r0.xyw = r5.xyz * r0.xyw;
  r0.xyw = r1.xyz + r0.xyw;
  r1.x = dot(r7.xyz, r7.xyz);
  r1.x = rsqrt(r1.x);
  r1.xyz = r7.xyz * r1.xxx;
  r5.xyz = r0.zzz;
  r1.xyz = r5.xyz * r1.xyz;
  r0.xyz = r1.xyz + r0.xyw;
  r0.w = dot(r0.xyz, r0.xyz);
  r0.w = rsqrt(r0.w);
  r0.xyz = r0.xyz * r0.www;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xy = float2(0.5,0.5) * r0.xy;
  r1.xy = float2(0.5,0.5) + r0.xy;
  r0.x = 0.5 * r0.z;
  r0.x = 0.5 + r0.x;
  r1.z = sqrt(r0.x);
  r1.xy = r1.xy;
  r1.z = r1.z;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r4.xyz = r4.xyz;
  r1.xyz = r1.xyz;
  r2.x = r2.x;
  r2.y = r3.y;
  r1.w = r3.z;
  r2.z = r2.z;
  r4.xyz = r4.xyz;
  r1.xyz = r1.xyz;
  r2.x = r2.x;
  r2.y = r2.y;
  r1.w = r1.w;
  r2.z = r2.z;
  r4.xyz = r4.xyz;
  r4.w = 1;
  r1.xyz = r1.xyz;
  r1.w = r1.w;
  r2.x = r2.x;
  r2.y = r2.y;
  r2.z = r2.z;
  r2.w = 0;
  r4.xyzw = r4.xyzw;
  r1.xyzw = r1.xyzw;
  r2.xyzw = r2.xyzw;
  o0.xyzw = r4.xyzw;
  o1.xyzw = r1.xyzw;
  o2.xyzw = r2.xyzw;
  return;
}
// clang-format on

void main(
    float4 v0 : SV_Position0,
    float4 v1 : COLOR0,
    float4 v2 : TEXCOORD0,
    float2 v3 : TEXCOORD1,
    float2 w3 : TEXCOORD9,
    float4 v4 : TEXCOORD5,
    float4 v5 : TEXCOORD6,
    float4 v6 : TEXCOORD7,
    float4 v7 : TEXCOORD8,
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
  NativeMaterialMain(v0, v1, v2, v3, w3, v4, v5, v6, v7, o0, o1, o2);
  clean_material = float4(0.0, 0.0, 0.0, -1.0);
  material_owner = 0u;
  const uint capture = material_capture_parameters.Load(int3(0, 0, 0));
  if ((capture & 1u) == 0u) return;
  clean_material = float4(renodx::color::srgb::Decode(saturate(o0.rgb)), 1.0);
  material_owner = uint4(PackMaterialUNorm(o0), PackMaterialUNorm(o1), PackMaterialUNorm(o2), asuint(v0.z));
}
