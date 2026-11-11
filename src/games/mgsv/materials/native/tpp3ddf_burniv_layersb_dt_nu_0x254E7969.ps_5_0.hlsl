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
SamplerState g_sampler_normal_s : register(s1);
SamplerState g_sampler_srm_s : register(s2);
SamplerState g_sampler_invisiblePattern_s : register(s3);
SamplerState g_sampler_layer_s : register(s4);
SamplerState g_sampler_Dirty_s : register(s5);
SamplerState g_samplerPoint_Wrap_s : register(s8);
Texture2D<float4> g_tex_diffuse : register(t0);
Texture2D<float4> g_tex_normal : register(t1);
Texture2D<float4> g_tex_srm : register(t2);
Texture2D<float4> g_tex_invisiblePattern : register(t3);
Texture2D<float4> g_tex_layer : register(t4);
Texture2D<float4> g_tex_Dirty : register(t5);
Texture2D<float4> g_tex_mesh : register(t15);


// 3Dmigoto declarations
#define cmp -


void NativeMaterialMain(
  float4 v0 : SV_Position0,
  float4 v1 : COLOR0,
  float4 v2 : TEXCOORD0,
  float4 v3 : TEXCOORD1,
  float4 v4 : TEXCOORD5,
  float4 v5 : TEXCOORD6,
  float3 v6 : TEXCOORD7,
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
  r0.zw = g_tex_invisiblePattern.Sample(g_sampler_invisiblePattern_s, v2.xy).xy;
  r0.z = r0.z;
  r0.z = r0.z;
  r1.xyz = r1.xyz;
  r2.xyzw = g_tex_layer.Sample(g_sampler_layer_s, v3.xy).xyzw;
  r3.xyz = r2.www * r0.zzz;
  r4.xyz = -r1.xyz;
  r2.xyz = r4.xyz + r2.xyz;
  r2.xyz = r3.xyz * r2.xyz;
  r2.xyz = r2.xyz + r1.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r3.z = g_psMaterial.m_materials[0].x;
  r3.z = r3.z;
  r4.xyz = float3(1,1,1) * v6.xyz;
  r1.yz = g_tex_normal.Sample(g_sampler_normal_s, v2.zw).wy;
  r1.yz = r1.yz;
  r1.yz = float2(2,2) * r1.yz;
  r5.xy = float2(-1,-1) + r1.yz;
  r0.z = r5.x * r5.x;
  r0.z = -r0.z;
  r0.z = 1 + r0.z;
  r1.y = r5.y * r5.y;
  r1.y = -r1.y;
  r0.z = r1.y + r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = 9.99999975e-05 + r0.z;
  r1.y = rsqrt(r0.z);
  r5.z = r1.y * r0.z;
  r5.xy = r5.xy;
  r5.z = r5.z;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz;
  r6.xyz = v4.xyz;
  r0.z = dot(r6.xyz, r6.xyz);
  r0.z = rsqrt(r0.z);
  r6.xyz = r6.xyz * r0.zzz;
  r7.xyz = r5.xxx;
  r6.xyz = r7.xyz * r6.xyz;
  r7.xyz = v5.xyz;
  r0.z = dot(r7.xyz, r7.xyz);
  r0.z = rsqrt(r0.z);
  r7.xyz = r7.xyz * r0.zzz;
  r5.xyw = r5.yyy;
  r5.xyw = r7.xyz * r5.xyw;
  r5.xyw = r6.xyz + r5.xyw;
  r0.z = dot(r4.xyz, r4.xyz);
  r0.z = rsqrt(r0.z);
  r4.xyz = r4.xyz * r0.zzz;
  r6.xyz = r5.zzz;
  r4.xyz = r6.xyz * r4.xyz;
  r4.xyz = r5.xyw + r4.xyz;
  r0.z = dot(r4.xyz, r4.xyz);
  r0.z = rsqrt(r0.z);
  r4.xyz = r4.xyz * r0.zzz;
  r4.xyz = r4.xyz;
  r5.xyz = g_tex_srm.Sample(g_sampler_srm_s, v2.xy).yxz;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz;
  r3.y = r5.y;
  r3.y = r3.y;
  r3.y = r3.y;
  r3.y = r3.y;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r5.x = r5.x;
  r0.z = v1.z;
  r2.xyz = r2.xyz;
  r5.x = r5.x;
  r0.z = r0.z;
  r2.xyz = r2.xyz;
  r5.x = r5.x;
  r0.z = r0.z;
  r1.y = g_tex_Dirty.Sample(g_sampler_Dirty_s, v2.xy).z;
  r1.y = r1.y;
  r1.y = r1.y;
  r2.xyz = r2.xyz;
  r5.x = r5.x;
  r0.z = r0.z;
  r6.z = r0.z * r1.y;
  r0.z = 255 * r5.x;
  r1.y = -205 + r0.z;
  r1.y = r1.y / 25;
  r1.y = -0.200000003 * r1.y;
  r1.y = 0.930000007 + r1.y;
  r1.y = max(0.730000019, r1.y);
  r1.y = min(0.930000007, r1.y);
  r0.z = -170 + r0.z;
  r0.z = r0.z / 60;
  r0.z = 0.660000026 * r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = min(0.660000026, r0.z);
  r7.xyz = r2.xyz * r1.yyy;
  r6.xyz = r6.zzz;
  r8.xyz = -r2.xyz;
  r7.xyz = r8.xyz + r7.xyz;
  r6.yzw = r7.xyz * r6.xyz;
  r2.xyz = r6.yzw + r2.xyz;
  r0.z = r5.x * r0.z;
  r1.y = -r5.x;
  r0.z = r1.y + r0.z;
  r0.z = r6.x * r0.z;
  r3.x = r5.x + r0.z;
  r2.xyz = r2.xyz;
  r3.x = r3.x;
  r2.xyz = r2.xyz;
  r3.x = r3.x;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r3.x = r3.x;
  r1.w = r1.w;
  r1.w = r1.w;
  r1.w = r1.w;
  r1.w = r1.w;
  r2.xyz = r2.xyz;
  r1.w = r1.w;
  r2.xyz = r2.xyz;
  r2.w = r1.w;
  r2.xyzw = r2.xyzw;
  r0.z = g_psMaterial.m_materials[3].x;
  r0.z = 2 * r0.z;
  r0.z = -1 + r0.z;
  r0.z = r0.w + r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.w = g_psMaterial.m_materials[1].w;
  r1.y = -r0.w;
  r1.z = r1.y + r0.z;
  r1.z = max(0, r1.z);
  r1.z = min(1, r1.z);
  r1.y = 1 + r1.y;
  r1.y = r1.z / r1.y;
  r1.y = -r1.y;
  r6.xyz = float3(1,1,1) + r1.yyy;
  r0.z = r0.z / r0.w;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r7.xyz = g_psMaterial.m_materials[1].xyz;
  r8.xyz = -r2.xyz;
  r7.xyz = r8.xyz + r7.xyz;
  r6.xyz = r7.xyz * r6.xyz;
  r2.xyz = r6.xyz + r2.xyz;
  r2.xyz = max(float3(0,0,0), r2.xyz);
  r6.xyz = min(float3(1,1,1), r2.xyz);
  r0.w = cmp(r0.z >= 0.25);
  r0.w = r0.w ? 1 : 0;
  r0.w = 0.25 * r0.w;
  r1.y = cmp(r0.z >= 0.5);
  r1.y = r1.y ? 1 : 0;
  r1.y = 0.25 * r1.y;
  r1.y = r1.y + r0.z;
  r0.w = r1.y + r0.w;
  r1.y = cmp(r0.z >= 0.75);
  r1.y = r1.y ? 1 : 0;
  r1.y = 0.25 * r1.y;
  r1.y = r1.y + r0.z;
  r0.w = r1.y + r0.w;
  r1.y = cmp(r0.z >= 1);
  r1.y = r1.y ? 1 : 0;
  r1.y = 0.25 * r1.y;
  r0.z = r1.y + r0.z;
  r0.z = r0.w + r0.z;
  r0.z = r2.w * r0.z;
  r6.xyz = r6.xyz;
  r0.z = r0.z;
  r1.x = v1.w;
  r0.xy = r0.xy;
  r1.w = r1.w;
  r1.x = r1.x;
  r0.xy = r0.xy;
  r0.w = g_psSystem.m_param.w;
  r0.xy = float2(0.125,0.125) * r0.xy;
  r0.xy = g_tex_mesh.Sample(g_samplerPoint_Wrap_s, r0.xy).wx;
  r2.yz = r0.yx;
  r2.x = r2.z * r0.w;
  r1.yz = -r2.xy;
  r1.xy = r1.wx + r1.yz;
  r1.xy = cmp(r1.xy < float2(0,0));
  r0.y = (int)r1.y | (int)r1.x;
  r0.y = (int)r0.y & -1;
  if (r0.y != 0) discard;
  r0.z = r0.z;
  r0.z = r0.z;
  r0.y = min(1, r0.z);
  r0.x = r0.x;
  r0.x = r0.x * r0.w;
  r0.x = -r0.x;
  r0.x = r0.y + r0.x;
  r0.x = cmp(r0.x < 0);
  r0.x = r0.x ? asfloat(0xFFFFFFFFu) : 0;
  if (r0.x != 0) discard;
  r3.y = r3.y;
  r3.y = r3.y;
  r3.y = r3.y;
  r3.y = r3.y;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r0.xy = float2(0.5,0.5) * r4.xy;
  r5.xy = float2(0.5,0.5) + r0.yx;
  r0.x = 0.5 * r4.z;
  r0.x = 0.5 + r0.x;
  r5.w = sqrt(r0.x);
  r5.xy = r5.xy;
  r5.w = r5.w;
  r5.xyw = r5.xyw;
  r5.xyw = r5.xyw;
  r6.xyz = r6.xyz;
  r5.xyw = r5.xyw;
  r3.x = r3.x;
  r3.y = r3.y;
  r5.z = r5.z;
  r3.z = r3.z;
  r6.xyz = r6.xyz;
  r5.xyw = r5.xyw;
  r3.x = r3.x;
  r3.y = r3.y;
  r5.z = r5.z;
  r3.z = r3.z;
  r6.xyz = r6.xyz;
  r6.w = 1;
  r5.xyw = r5.xyw;
  r5.z = r5.z;
  r3.x = r3.x;
  r3.y = r3.y;
  r3.z = r3.z;
  r3.w = 0;
  r6.xyzw = r6.xyzw;
  r5.xyzw = r5.xyzw;
  r3.xyzw = r3.xyzw;
  o0.xyzw = r6.xyzw;
  o1.xyzw = r5.yxwz;
  o2.xyzw = r3.xyzw;
  return;
}
// clang-format on

void main(
    float4 v0 : SV_Position0,
    float4 v1 : COLOR0,
    float4 v2 : TEXCOORD0,
    float4 v3 : TEXCOORD1,
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
