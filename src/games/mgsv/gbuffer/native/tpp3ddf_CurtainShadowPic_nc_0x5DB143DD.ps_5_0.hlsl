#include "../material_capture.hlsli"

// clang-format off
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:26 2026

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
SamplerState g_sampler_CylindricalLayer_s : register(s4);
SamplerState g_sampler_Light_s : register(s5);
SamplerState g_sampler_subNormal_s : register(s6);
SamplerState g_sampler_Dirty_s : register(s7);
SamplerState g_samplerPoint_Wrap_s : register(s8);
Texture2D<float4> g_tex_diffuse : register(t0);
Texture2D<float4> g_tex_normal : register(t1);
Texture2D<float4> g_tex_srm : register(t2);
Texture2D<float4> g_tex_CylindricalLayer : register(t4);
Texture2D<float4> g_tex_Light : register(t5);
Texture2D<float4> g_tex_subNormal : register(t6);
Texture2D<float4> g_tex_Dirty : register(t7);
Texture2D<float4> g_tex_mesh : register(t15);


// 3Dmigoto declarations
#define cmp -


void NativeMaterialMain(
  float4 v0 : SV_Position0,
  float4 v1 : COLOR0,
  float2 v2 : TEXCOORD0,
  float2 w2 : TEXCOORD2,
  float4 v3 : TEXCOORD1,
  float4 v4 : TEXCOORD5,
  float4 v5 : TEXCOORD6,
  float4 v6 : TEXCOORD7,
  float4 v7 : TEXCOORD8,
  float2 v8 : TEXCOORD9,
  uint v9 : SV_IsFrontFace0,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1,
  out float4 o2 : SV_Target2)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v0.xy;
  r0.zw = v2.xy;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
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
  r2.z = g_psMaterial.m_materials[0].x;
  r2.z = r2.z;
  r3.x = v9.x;
  r3.x = r3.x;
  r3.x = r3.x;
  r3.x = r3.x ? 1 : -1;
  r3.x = (int)r3.x;
  r3.x = r3.x;
  r3.x = r3.x;
  r0.zw = r0.zw;
  r3.x = r3.x;
  r0.zw = r0.zw;
  r3.x = r3.x;
  r0.zw = r0.zw;
  r3.yz = g_psMaterial.m_materials[1].yz;
  r4.xy = g_psMaterial.m_materials[1].xx;
  r3.x = r3.x;
  r5.xyz = v4.xyz;
  r6.xyz = v5.xyz;
  r7.xyz = v6.xyz;
  r7.xyz = r7.xyz * r3.xxx;
  r3.xw = g_tex_normal.Sample(g_sampler_normal_s, v2.xy).wy;
  r3.xw = r3.xw;
  r3.xw = float2(2,2) * r3.xw;
  r8.xy = float2(-1,-1) + r3.xw;
  r3.x = r8.x * r8.x;
  r3.x = -r3.x;
  r3.x = 1 + r3.x;
  r3.w = r8.y * r8.y;
  r3.w = -r3.w;
  r3.x = r3.x + r3.w;
  r3.x = max(0, r3.x);
  r3.x = min(1, r3.x);
  r3.x = 9.99999975e-05 + r3.x;
  r3.w = rsqrt(r3.x);
  r8.z = r3.x * r3.w;
  r8.xy = r8.xy;
  r8.z = r8.z;
  r8.xyz = r8.xyz;
  r8.xyz = r8.xyz;
  r0.zw = r3.yz * r0.zw;
  r0.zw = g_tex_subNormal.Sample(g_sampler_subNormal_s, r0.zw).wy;
  r0.zw = r0.zw;
  r0.zw = float2(2,2) * r0.zw;
  r0.zw = float2(-1,-1) + r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r0.zw = float2(1,1) * r0.zw;
  r0.zw = r0.zw * r4.xy;
  r8.xy = r8.xy + r0.zw;
  r0.z = dot(r8.xyz, r8.xyz);
  r0.z = rsqrt(r0.z);
  r3.xyz = r8.xyz * r0.zzz;
  r0.z = dot(r5.xyz, r5.xyz);
  r0.z = rsqrt(r0.z);
  r4.xyz = r5.xyz * r0.zzz;
  r5.xyz = r3.xxx;
  r4.xyz = r5.xyz * r4.xyz;
  r0.z = dot(r6.xyz, r6.xyz);
  r0.z = rsqrt(r0.z);
  r5.xyz = r6.xyz * r0.zzz;
  r3.xyw = r3.yyy;
  r3.xyw = r5.xyz * r3.xyw;
  r3.xyw = r4.xyz + r3.xyw;
  r0.z = dot(r7.xyz, r7.xyz);
  r0.z = rsqrt(r0.z);
  r4.xyz = r7.xyz * r0.zzz;
  r5.xyz = r3.zzz;
  r4.xyz = r5.xyz * r4.xyz;
  r3.xyz = r4.xyz + r3.xyw;
  r0.z = dot(r3.xyz, r3.xyz);
  r0.z = rsqrt(r0.z);
  r3.xyz = r3.xyz * r0.zzz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r4.xyz = g_tex_srm.Sample(g_sampler_srm_s, v2.xy).yxz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r1.xyz = r1.xyz;
  r4.x = r4.x;
  r0.z = v1.x;
  r1.xyz = r1.xyz;
  r4.x = r4.x;
  r0.z = r0.z;
  r1.xyz = r1.xyz;
  r4.x = r4.x;
  r0.z = r0.z;
  r0.w = g_tex_Dirty.Sample(g_sampler_Dirty_s, v2.xy).x;
  r0.w = r0.w;
  r0.w = r0.w;
  r1.xyz = r1.xyz;
  r4.x = r4.x;
  r0.z = r0.z;
  r5.z = r0.z * r0.w;
  r5.xyz = r5.zzz;
  r6.xyz = -r1.xyz;
  r6.xyz = float3(0.2588,0.0470499992,0.043136999) + r6.xyz;
  r5.yzw = r6.xyz * r5.xyz;
  r5.yzw = r5.yzw + r1.xyz;
  r0.z = -r4.x;
  r0.z = 0.200000003 + r0.z;
  r0.z = r5.x * r0.z;
  r2.x = r4.x + r0.z;
  r5.yzw = r5.yzw;
  r2.x = r2.x;
  r5.yzw = r5.yzw;
  r2.x = r2.x;
  r5.yzw = r5.yzw;
  r5.yzw = r5.yzw;
  r2.x = r2.x;
  r1.w = r1.w;
  r1.w = r1.w;
  r1.w = r1.w;
  r1.w = r1.w;
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
  r5.yzw = r5.yzw;
  r5.yzw = r5.yzw;
  r5.yzw = r5.yzw;
  r5.yzw = r5.yzw;
  r5.yzw = r5.yzw;
  r0.xyz = v3.xyz;
  r5.yzw = r5.yzw;
  r0.xyz = r0.xyz;
  r0.w = dot(r0.xyz, r0.xyz);
  r0.w = rsqrt(r0.w);
  r0.xyz = r0.xyz * r0.www;
  r1.xz = r0.xz;
  r1.y = 0;
  r0.w = dot(r1.xyz, r1.xyz);
  r0.w = rsqrt(r0.w);
  r1.xy = r1.xz * r0.ww;
  r0.xy = r0.xy;
  r0.z = 0;
  r0.z = dot(r0.xyz, r0.xyz);
  r0.z = rsqrt(r0.z);
  r0.x = r0.x * r0.z;
  r0.z = -r1.x;
  r0.w = max(r1.x, r0.z);
  r1.z = -r0.w;
  r1.z = 1 + r1.z;
  r1.z = sqrt(r1.z);
  r1.w = -0.0187292993 * r0.w;
  r1.w = 0.0742610022 + r1.w;
  r1.w = r1.w * r0.w;
  r1.w = -0.212114394 + r1.w;
  r0.w = r1.w * r0.w;
  r0.w = 1.57072878 + r0.w;
  r0.w = r0.w * r1.z;
  r1.z = -2 * r0.w;
  r1.z = 3.14159274 + r1.z;
  r0.z = cmp(r1.x < r0.z);
  r0.z = r0.z ? r1.z : 0;
  r6.x = r0.w + r0.z;
  r0.y = -r0.y;
  r0.z = -r0.y;
  r0.w = max(r0.y, r0.z);
  r1.x = -r0.w;
  r1.x = 1 + r1.x;
  r1.x = sqrt(r1.x);
  r1.z = -0.0187292993 * r0.w;
  r1.z = 0.0742610022 + r1.z;
  r1.z = r1.z * r0.w;
  r1.z = -0.212114394 + r1.z;
  r0.w = r1.z * r0.w;
  r0.w = 1.57072878 + r0.w;
  r0.w = r0.w * r1.x;
  r1.x = -2 * r0.w;
  r1.x = 3.14159274 + r1.x;
  r0.y = cmp(r0.y < r0.z);
  r0.y = r0.y ? r1.x : 0;
  r6.y = r0.w + r0.y;
  r0.y = cmp(0 < r1.y);
  if (r0.y != 0) {
    r0.y = -r6.x;
    r6.x = 6.28318501 + r0.y;
  }
  r0.x = cmp(0 < r0.x);
  if (r0.x != 0) {
    r0.x = -r6.y;
    r6.y = 6.28318501 + r0.x;
  }
  r0.xy = r6.xy / float2(6.28318501,3.1415925);
  r0.xy = float2(-0.5,-0.5) + r0.xy;
  r0.zw = g_psMaterial.m_materials[3].xy;
  r0.zw = float2(1,0.5) * r0.zw;
  r0.xy = r0.xy * r0.zw;
  r0.xy = float2(0.5,0.5) + r0.xy;
  r0.xy = max(float2(0,0), r0.xy);
  r0.xy = min(float2(1,1), r0.xy);
  r0.y = -1 * r0.y;
  r1.xyz = g_psMaterial.m_materials[5].xyz;
  r0.z = g_tex_CylindricalLayer.Sample(g_sampler_CylindricalLayer_s, r0.xy).x;
  r6.xyz = r0.zzz;
  r7.xyz = -r1.xyz;
  r7.xyz = float3(1,1,1) + r7.xyz;
  r6.xyz = r7.xyz * r6.xyz;
  r1.xyz = r6.xyz + r1.xyz;
  r0.x = g_tex_Light.Sample(g_sampler_Light_s, r0.xy).x;
  r0.x = r0.x;
  r0.y = g_tex_Light.Sample(g_sampler_Light_s, w2.xy).y;
  r0.y = r0.y;
  r0.zw = g_psMaterial.m_materials[4].xy;
  r0.xy = float2(-1,-1) + r0.xy;
  r0.xy = r0.zw * r0.xy;
  r0.xy = float2(1,1) + r0.xy;
  r0.x = r0.x * r0.y;
  r0.xyz = r1.xyz * r0.xxx;
  r1.xyz = g_psMaterial.m_materials[6].xyz;
  r0.xyz = r1.xyz * r0.xyz;
  r0.xyz = float3(1,1,1) + r0.xyz;
  r0.xyz = r5.yzw * r0.xyz;
  r0.xyz = r0.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r1.xy = float2(0.5,0.5) * r3.xy;
  r1.xy = float2(0.5,0.5) + r1.xy;
  r3.x = 0.5 * r3.z;
  r3.x = 0.5 + r3.x;
  r1.z = sqrt(r3.x);
  r1.xy = r1.xy;
  r1.z = r1.z;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r0.xyz = r0.xyz;
  r1.xyz = r1.xyz;
  r2.x = r2.x;
  r2.y = r4.y;
  r1.w = r4.z;
  r2.z = r2.z;
  r0.xyz = r0.xyz;
  r1.xyz = r1.xyz;
  r2.x = r2.x;
  r2.y = r2.y;
  r1.w = r1.w;
  r2.z = r2.z;
  r0.xyz = r0.xyz;
  r0.w = 1;
  r1.xyz = r1.xyz;
  r1.w = r1.w;
  r2.x = r2.x;
  r2.y = r2.y;
  r2.z = r2.z;
  r2.w = 0;
  r0.xyzw = r0.xyzw;
  r1.xyzw = r1.xyzw;
  r2.xyzw = r2.xyzw;
  o0.xyzw = r0.xyzw;
  o1.xyzw = r1.xyzw;
  o2.xyzw = r2.xyzw;
  return;
}
// clang-format on

void main(
    float4 v0 : SV_Position0,
    float4 v1 : COLOR0,
    float2 v2 : TEXCOORD0,
    float2 w2 : TEXCOORD2,
    float4 v3 : TEXCOORD1,
    float4 v4 : TEXCOORD5,
    float4 v5 : TEXCOORD6,
    float4 v6 : TEXCOORD7,
    float4 v7 : TEXCOORD8,
    float2 v8 : TEXCOORD9,
    uint v9 : SV_IsFrontFace0,
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
  NativeMaterialMain(v0, v1, v2, w2, v3, v4, v5, v6, v7, v8, v9, o0, o1, o2);
  clean_material = float4(0.0, 0.0, 0.0, -1.0);
  material_owner = 0u;
  const uint capture = material_capture_parameters.Load(int3(0, 0, 0));
  if ((capture & 1u) == 0u) return;
  clean_material = float4(renodx::color::srgb::Decode(saturate(o0.rgb)), 1.0);
  material_owner = uint4(PackMaterialUNorm(o0), PackMaterialUNorm(o1), PackMaterialUNorm(o2), asuint(v0.z));
}
