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
SamplerState g_sampler_Dirty_s : register(s4);
SamplerState g_samplerPoint_Wrap_s : register(s8);
Texture2D<float4> g_tex_diffuse : register(t0);
Texture2D<float4> g_tex_normal : register(t1);
Texture2D<float4> g_tex_srm : register(t2);
Texture2D<float4> g_tex_materialmap : register(t3);
Texture2D<float4> g_tex_Dirty : register(t4);
Texture2D<float4> g_tex_mesh : register(t15);


// 3Dmigoto declarations
#define cmp -


void NativeMaterialMain(
  float4 v0 : SV_Position0,
  float4 v1 : COLOR0,
  float2 v2 : TEXCOORD0,
  float2 w2 : TEXCOORD1,
  float4 v3 : TEXCOORD5,
  float4 v4 : TEXCOORD6,
  float3 v5 : TEXCOORD7,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1,
  out float4 o2 : SV_Target2)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v0.xy;
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
  r0.z = g_psMaterial.m_materials[0].x;
  r0.w = g_tex_materialmap.Sample(g_samplerPoint_Wrap_s, v2.xy).x;
  r0.w = r0.w;
  r0.w = cmp(r0.w >= 0.25);
  r0.w = r0.w ? 1 : 0;
  r2.x = -r0.w;
  r2.x = 1 + r2.x;
  r0.z = r2.x * r0.z;
  r2.x = g_psMaterial.m_materials[0].y;
  r0.w = r2.x * r0.w;
  r2.z = r0.z + r0.w;
  r2.z = r2.z;
  r3.xyz = float3(1,1,1) * v5.xyz;
  r0.zw = g_tex_normal.Sample(g_sampler_normal_s, v2.xy).wy;
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
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r5.xyz = v3.xyz;
  r0.z = dot(r5.xyz, r5.xyz);
  r0.z = rsqrt(r0.z);
  r5.xyz = r5.xyz * r0.zzz;
  r6.xyz = r4.xxx;
  r5.xyz = r6.xyz * r5.xyz;
  r6.xyz = v4.xyz;
  r0.z = dot(r6.xyz, r6.xyz);
  r0.z = rsqrt(r0.z);
  r6.xyz = r6.xyz * r0.zzz;
  r4.xyw = r4.yyy;
  r4.xyw = r6.xyz * r4.xyw;
  r4.xyw = r5.xyz + r4.xyw;
  r0.z = dot(r3.xyz, r3.xyz);
  r0.z = rsqrt(r0.z);
  r3.xyz = r3.xyz * r0.zzz;
  r5.xyz = r4.zzz;
  r3.xyz = r5.xyz * r3.xyz;
  r3.xyz = r4.xyw + r3.xyz;
  r0.z = dot(r3.xyz, r3.xyz);
  r0.z = rsqrt(r0.z);
  r3.xyz = r3.xyz * r0.zzz;
  r3.xyz = r3.xyz;
  r4.xyz = g_tex_srm.Sample(g_sampler_srm_s, v2.xy).yxz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r2.y = r4.y;
  r2.y = r2.y;
  r2.y = r2.y;
  r2.y = r2.y;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r4.x = r4.x;
  r5.xyz = v1.xyz;
  r1.xyz = r1.xyz;
  r4.x = r4.x;
  r5.xyz = r5.xyz;
  r1.xyz = r1.xyz;
  r4.x = r4.x;
  r5.xyz = r5.xyz;
  r6.xyz = g_tex_Dirty.Sample(g_sampler_Dirty_s, v2.xy).xyz;
  r6.xyz = r6.xyz;
  r6.xyz = r6.xyz;
  r1.xyz = r1.xyz;
  r4.x = r4.x;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz * r6.xyz;
  r0.z = 255 * r4.x;
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
  r6.xyz = r1.xyz * r0.www;
  r7.xyz = r5.zzz;
  r8.xyz = -r1.xyz;
  r6.xyz = r8.xyz + r6.xyz;
  r6.xyz = r7.yzx * r6.xyz;
  r6.xyz = r6.xyz + r1.xyz;
  r5.yzw = r5.yyy;
  r7.yzw = -r6.xyz;
  r7.yzw = float3(0.200000003,0.196078435,0.192156866) + r7.yzw;
  r7.yzw = r7.yzw * r5.zyw;
  r6.xyz = r7.yzw + r6.xyz;
  r5.xzw = r5.xxx;
  r7.yzw = -r6.xyz;
  r7.yzw = float3(0.2588,0.0470499992,0.043136999) + r7.yzw;
  r7.yzw = r7.yzw * r5.xzw;
  r6.xyz = r7.yzw + r6.xyz;
  r0.z = r4.x * r0.z;
  r0.w = -r4.x;
  r0.z = r0.z + r0.w;
  r0.z = r7.x * r0.z;
  r0.z = r4.x + r0.z;
  r0.w = -r0.z;
  r0.w = 0.980000019 + r0.w;
  r0.w = r5.y * r0.w;
  r0.z = r0.z + r0.w;
  r0.w = -r0.z;
  r0.w = 0.200000003 + r0.w;
  r0.w = r5.x * r0.w;
  r2.x = r0.z + r0.w;
  r6.xyz = r6.xyz;
  r2.x = r2.x;
  r6.xyz = r6.xyz;
  r2.x = r2.x;
  r6.xyz = r6.xyz;
  r6.xyz = r6.xyz;
  r2.x = r2.x;
  r1.w = r1.w;
  r1.w = r1.w;
  r1.w = r1.w;
  r1.w = r1.w;
  r6.xyz = r6.xyz;
  r6.xyz = r6.xyz;
  r6.xyz = r6.xyz;
  r6.xyz = r6.xyz;
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
  r2.y = r2.y;
  r2.y = r2.y;
  r2.y = r2.y;
  r2.y = r2.y;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r0.xy = float2(0.5,0.5) * r3.xy;
  r4.xy = float2(0.5,0.5) + r0.yx;
  r0.x = 0.5 * r3.z;
  r0.x = 0.5 + r0.x;
  r4.w = sqrt(r0.x);
  r4.xy = r4.xy;
  r4.w = r4.w;
  r4.xyw = r4.xyw;
  r4.xyw = r4.xyw;
  r6.xyz = r6.xyz;
  r4.xyw = r4.xyw;
  r2.x = r2.x;
  r2.y = r2.y;
  r4.z = r4.z;
  r2.z = r2.z;
  r6.xyz = r6.xyz;
  r4.xyw = r4.xyw;
  r2.x = r2.x;
  r2.y = r2.y;
  r4.z = r4.z;
  r2.z = r2.z;
  r6.xyz = r6.xyz;
  r6.w = 1;
  r4.xyw = r4.xyw;
  r4.z = r4.z;
  r2.x = r2.x;
  r2.y = r2.y;
  r2.z = r2.z;
  r2.w = 0;
  r6.xyzw = r6.xyzw;
  r4.xyzw = r4.xyzw;
  r2.xyzw = r2.xyzw;
  o0.xyzw = r6.xyzw;
  o1.xyzw = r4.yxwz;
  o2.xyzw = r2.xyzw;
  return;
}
// clang-format on

void main(
    float4 v0 : SV_Position0,
    float4 v1 : COLOR0,
    float2 v2 : TEXCOORD0,
    float2 w2 : TEXCOORD1,
    float4 v3 : TEXCOORD5,
    float4 v4 : TEXCOORD6,
    float3 v5 : TEXCOORD7,
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
  NativeMaterialMain(v0, v1, v2, w2, v3, v4, v5, o0, o1, o2);
  clean_material = float4(0.0, 0.0, 0.0, -1.0);
  material_owner = 0u;
  const uint capture = material_capture_parameters.Load(int3(0, 0, 0));
  if ((capture & 1u) == 0u) return;
  clean_material = float4(renodx::color::srgb::Decode(saturate(o0.rgb)), 1.0);
  material_owner = uint4(PackMaterialUNorm(o0), PackMaterialUNorm(o1), PackMaterialUNorm(o2), asuint(v0.z));
}
