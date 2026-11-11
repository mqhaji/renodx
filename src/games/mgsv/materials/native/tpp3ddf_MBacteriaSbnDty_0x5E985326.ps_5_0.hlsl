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
SamplerState g_sampler_layerMask_s : register(s3);
SamplerState g_sampler_subNormal_s : register(s4);
SamplerState g_sampler_metalicLayer_s : register(s5);
SamplerState g_sampler_Dirty2_s : register(s6);
SamplerState g_samplerPoint_Wrap_s : register(s8);
Texture2D<float4> g_tex_diffuse : register(t0);
Texture2D<float4> g_tex_normal : register(t1);
Texture2D<float4> g_tex_srm : register(t2);
Texture2D<float4> g_tex_layerMask : register(t3);
Texture2D<float4> g_tex_subNormal : register(t4);
Texture2D<float4> g_tex_metalicLayer : register(t5);
Texture2D<float4> g_tex_Dirty2 : register(t6);
Texture2D<float4> g_tex_mesh : register(t15);


// 3Dmigoto declarations
#define cmp -


void NativeMaterialMain(
  float4 v0 : SV_Position0,
  float4 v1 : COLOR0,
  float2 v2 : TEXCOORD0,
  float2 w2 : TEXCOORD1,
  float4 v3 : TEXCOORD2,
  float4 v4 : TEXCOORD4,
  float4 v5 : TEXCOORD5,
  float4 v6 : TEXCOORD6,
  float3 v7 : TEXCOORD7,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1,
  out float4 o2 : SV_Target2)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v0.xy;
  r0.zw = v2.xy;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r1.z = g_psMaterial.m_materials[0].x;
  r1.z = r1.z;
  r0.zw = r0.zw;
  r2.xy = w2.xy;
  r3.xyz = v3.xyz;
  r4.xyz = v1.xyz;
  r0.zw = r0.zw;
  r2.xy = r2.xy;
  r3.xyz = r3.xyz;
  r4.xyz = r4.xyz;
  r5.xyzw = g_tex_diffuse.Sample(g_sampler_diffuse_s, v2.xy).xyzw;
  r6.xyz = float3(0.100000001,0.100000001,0.100000001) * g_psMaterial.m_materials[4].xyz;
  r6.xyz = -r6.xyz;
  r3.xyz = r6.xyz + r3.xyz;
  r2.z = dot(r3.xyz, r3.xyz);
  r2.z = sqrt(r2.z);
  r3.xy = g_psMaterial.m_materials[2].zw * r0.zw;
  r2.w = g_tex_layerMask.Sample(g_sampler_layerMask_s, r3.xy).y;
  r2.w = r2.w;
  r3.x = 0.100000001 * g_psMaterial.m_materials[5].x;
  r3.x = -r3.x;
  r3.y = r3.x + r2.z;
  r3.z = 0.100000001 * g_psMaterial.m_materials[5].y;
  r3.x = r3.z + r3.x;
  r3.x = r3.y / r3.x;
  r3.x = max(0, r3.x);
  r3.x = min(1, r3.x);
  r3.x = 2 * r3.x;
  r3.x = r3.x + r2.w;
  r3.x = -1 + r3.x;
  r3.x = max(0, r3.x);
  r3.x = min(1, r3.x);
  r3.x = r3.x;
  r3.y = cmp(g_psMaterial.m_materials[5].w != 0.000000);
  if (r3.y != 0) {
    r3.y = 0.100000001 * g_psMaterial.m_materials[5].z;
    r3.y = -r3.y;
    r3.z = r3.y + r2.z;
    r3.w = 0.100000001 * g_psMaterial.m_materials[5].w;
    r3.y = r3.w + r3.y;
    r3.y = r3.z / r3.y;
    r3.y = -r3.y;
    r3.y = 1 + r3.y;
    r3.y = max(0, r3.y);
    r3.y = min(1, r3.y);
    r2.w = r3.y * r2.w;
    r2.w = r3.x + r2.w;
  } else {
    r2.w = r3.x;
  }
  r2.xy = g_psMaterial.m_materials[2].xy * r2.xy;
  r6.xyzw = g_tex_metalicLayer.Sample(g_sampler_metalicLayer_s, r2.xy).wxyz;
  r2.x = -r2.w;
  r2.x = 1 + r2.x;
  r2.x = max(0, r2.x);
  r2.x = min(1, r2.x);
  r2.w = r6.x * r2.x;
  r2.xyw = r2.www;
  r3.yzw = -r5.xyz;
  r3.yzw = r6.yzw + r3.yzw;
  r3.yzw = r3.yzw * r2.xyw;
  r3.yzw = r5.xyz + r3.yzw;
  r2.y = 0.100000001 * g_psMaterial.m_materials[6].x;
  r2.y = -r2.y;
  r2.z = r2.z + r2.y;
  r2.w = 0.100000001 * g_psMaterial.m_materials[6].y;
  r2.y = r2.w + r2.y;
  r2.y = r2.z / r2.y;
  r2.y = -r2.y;
  r2.y = 1 + r2.y;
  r2.z = -r3.x;
  r2.y = r2.y + r2.z;
  r2.z = g_psMaterial.m_materials[6].z;
  r2.y = r2.y * r2.z;
  r2.y = r5.w + r2.y;
  r2.y = max(0, r2.y);
  r2.y = min(1, r2.y);
  r4.xyz = r4.xyz;
  r2.x = r2.x;
  r3.yzw = r3.yzw;
  r2.y = r2.y;
  r4.xyz = r4.xyz;
  r2.x = r2.x;
  r2.xz = r2.xx;
  r2.zw = r2.xz;
  r0.zw = r0.zw;
  r5.xy = g_psMaterial.m_materials[1].zw;
  r2.zw = r2.zw;
  r6.xyz = v5.xyz;
  r7.xyz = v6.xyz;
  r8.xyz = v7.xyz;
  r9.xyz = float3(1,1,1) * r8.xyz;
  r5.zw = g_tex_normal.Sample(g_sampler_normal_s, v2.xy).wy;
  r5.zw = r5.zw;
  r5.zw = float2(2,2) * r5.zw;
  r10.xy = float2(-1,-1) + r5.zw;
  r3.x = r10.x * r10.x;
  r3.x = -r3.x;
  r3.x = 1 + r3.x;
  r4.w = r10.y * r10.y;
  r4.w = -r4.w;
  r3.x = r4.w + r3.x;
  r3.x = max(0, r3.x);
  r3.x = min(1, r3.x);
  r3.x = 9.99999975e-05 + r3.x;
  r4.w = rsqrt(r3.x);
  r10.z = r4.w * r3.x;
  r10.xy = r10.xy;
  r10.z = r10.z;
  r10.xyz = r10.xyz;
  r10.xyz = r10.xyz;
  r0.zw = r5.xy * r0.zw;
  r0.zw = g_tex_subNormal.Sample(g_sampler_subNormal_s, r0.zw).wy;
  r0.zw = r0.zw;
  r0.zw = float2(2,2) * r0.zw;
  r0.zw = float2(-1,-1) + r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r0.zw = float2(1,1) * r0.zw;
  r0.zw = r0.zw * r2.zw;
  r10.xy = r10.xy + r0.zw;
  r0.z = dot(r10.xyz, r10.xyz);
  r0.z = rsqrt(r0.z);
  r5.xyz = r10.xyz * r0.zzz;
  r0.z = dot(r6.xyz, r6.xyz);
  r0.z = rsqrt(r0.z);
  r6.xyz = r6.xyz * r0.zzz;
  r10.xyz = r5.xxx;
  r6.xyz = r10.xyz * r6.xyz;
  r0.z = dot(r7.xyz, r7.xyz);
  r0.z = rsqrt(r0.z);
  r7.xyz = r7.xyz * r0.zzz;
  r5.xyw = r5.yyy;
  r5.xyw = r7.xyz * r5.xyw;
  r5.xyw = r6.xyz + r5.xyw;
  r0.z = dot(r9.xyz, r9.xyz);
  r0.z = rsqrt(r0.z);
  r6.xyz = r9.xyz * r0.zzz;
  r7.xyz = r5.zzz;
  r6.xyz = r7.xyz * r6.xyz;
  r5.xyz = r6.xyz + r5.xyw;
  r0.z = dot(r5.xyz, r5.xyz);
  r0.z = rsqrt(r0.z);
  r5.xyz = r5.xyz * r0.zzz;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz;
  r6.xyz = g_tex_srm.Sample(g_sampler_srm_s, v2.xy).yxz;
  r6.xyz = r6.xyz;
  r6.xyz = r6.xyz;
  r3.yzw = r3.yzw;
  r3.yzw = r3.yzw;
  r3.yzw = r3.yzw;
  r3.yzw = r3.yzw;
  r3.yzw = r3.yzw;
  r3.yzw = r3.yzw;
  r3.yzw = r3.yzw;
  r3.yzw = r3.yzw;
  r3.yzw = r3.yzw;
  r4.xyz = r4.xyz;
  r2.x = r2.x;
  r6.x = r6.x;
  r3.yzw = r3.yzw;
  r7.xyz = r4.xyz;
  r2.x = r2.x;
  r6.x = r6.x;
  r3.yzw = r3.yzw;
  r6.x = r6.x;
  r7.xyz = r7.xyz;
  r9.xyz = g_tex_Dirty2.Sample(g_sampler_Dirty2_s, v2.xy).xyz;
  r9.xyz = r9.xyz;
  r9.xyz = r9.xyz;
  r3.yzw = r3.yzw;
  r6.x = r6.x;
  r7.xyz = r7.xyz;
  r4.xyz = r9.xyz * r4.xyz;
  r7.xyz = r7.zxy * r9.zxy;
  r0.z = 255 * r6.x;
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
  r9.xyz = r3.yzw * r0.www;
  r10.xyz = r4.zzz;
  r11.xyz = -r3.yzw;
  r9.xyz = r11.xyz + r9.xyz;
  r9.xyz = r10.xyz * r9.xyz;
  r3.xyz = r9.xyz + r3.yzw;
  r4.yzw = r4.yyy;
  r9.xyz = -r3.xyz;
  r9.xyz = float3(0.200000003,0.196078435,0.192156866) + r9.xyz;
  r4.yzw = r9.xyz * r4.yzw;
  r3.xyz = r4.yzw + r3.xyz;
  r4.xyz = r4.xxx;
  r9.xyz = -r3.xyz;
  r9.xyz = float3(0.2588,0.0470499992,0.043136999) + r9.xyz;
  r4.xyz = r9.xyz * r4.xyz;
  r3.xyz = r4.xyz + r3.xyz;
  r0.z = r6.x * r0.z;
  r0.w = -r6.x;
  r0.z = r0.z + r0.w;
  r0.z = r7.x * r0.z;
  r0.z = r6.x + r0.z;
  r0.w = -r0.z;
  r0.w = 0.980000019 + r0.w;
  r0.w = r7.z * r0.w;
  r0.z = r0.z + r0.w;
  r0.w = -r0.z;
  r0.w = 0.200000003 + r0.w;
  r0.w = r7.y * r0.w;
  r0.z = r0.z + r0.w;
  r3.xyz = r3.xyz;
  r0.z = r0.z;
  r3.xyz = r3.xyz;
  r0.z = r0.z;
  r3.xyz = r3.xyz;
  r1.x = max(r0.z, r2.x);
  r3.xyz = r3.xyz;
  r1.x = r1.x;
  r2.y = r2.y;
  r2.xzw = v4.xyz;
  r2.y = r2.y;
  r8.xyz = r8.xyz;
  r2.xzw = r2.xzw;
  r0.z = g_psMaterial.m_materials[3].y;
  r0.z = r2.y * r0.z;
  r0.w = dot(r8.xyz, r2.xzw);
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r2.x = g_psMaterial.m_materials[3].x;
  r0.w = log2(r0.w);
  r0.w = r2.x * r0.w;
  r0.w = exp2(r0.w);
  r2.z = -r2.y;
  r0.z = r2.z + r0.z;
  r0.z = r0.w * r0.z;
  r4.x = r2.y + r0.z;
  r0.z = cmp(r2.x == 0.000000);
  if (r0.z != 0) {
    r4.x = 0;
  }
  r4.x = r4.x;
  r4.x = r4.x;
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
  r1.y = r6.y;
  r1.y = r1.y;
  r1.y = r1.y;
  r1.y = r1.y;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r1.y = r1.y;
  r1.y = r1.y;
  r1.y = r1.y;
  r1.y = r1.y;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz;
  r0.xy = float2(0.5,0.5) * r5.xy;
  r6.xy = float2(0.5,0.5) + r0.yx;
  r0.x = 0.5 * r5.z;
  r0.x = 0.5 + r0.x;
  r6.w = sqrt(r0.x);
  r6.xy = r6.xy;
  r6.w = r6.w;
  r6.xyw = r6.xyw;
  r6.xyw = r6.xyw;
  r3.xyz = r3.xyz;
  r6.xyw = r6.xyw;
  r1.x = r1.x;
  r1.y = r1.y;
  r6.z = r6.z;
  r1.z = r1.z;
  r3.xyz = r3.xyz;
  r6.xyw = r6.xyw;
  r1.x = r1.x;
  r1.y = r1.y;
  r6.z = r6.z;
  r1.z = r1.z;
  r3.xyz = r3.xyz;
  r3.w = 1;
  r6.xyw = r6.xyw;
  r6.z = r6.z;
  r1.x = r1.x;
  r1.y = r1.y;
  r1.z = r1.z;
  r1.w = 0;
  r3.xyzw = r3.xyzw;
  r6.xyzw = r6.xyzw;
  r1.xyzw = r1.xyzw;
  o0.xyzw = r3.xyzw;
  o1.xyzw = r6.yxwz;
  o2.xyzw = r1.xyzw;
  return;
}
// clang-format on

void main(
    float4 v0 : SV_Position0,
    float4 v1 : COLOR0,
    float2 v2 : TEXCOORD0,
    float2 w2 : TEXCOORD1,
    float4 v3 : TEXCOORD2,
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
  NativeMaterialMain(v0, v1, v2, w2, v3, v4, v5, v6, v7, o0, o1, o2);
  clean_material = float4(0.0, 0.0, 0.0, -1.0);
  material_owner = 0u;
  const uint capture = material_capture_parameters.Load(int3(0, 0, 0));
  if ((capture & 1u) == 0u) return;
  clean_material = float4(renodx::color::srgb::Decode(saturate(o0.rgb)), 1.0);
  material_owner = uint4(PackMaterialUNorm(o0), PackMaterialUNorm(o1), PackMaterialUNorm(o2), asuint(v0.z));
}
