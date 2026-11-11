#include "../material_capture.hlsli"

// clang-format off
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:32 2026

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
SamplerState g_sampler_viewReflection_s : register(s4);
SamplerState g_sampler_height_s : register(s5);
SamplerState g_sampler_base2_s : register(s6);
SamplerState g_sampler_metalicLayer_s : register(s7);
SamplerState g_samplerPoint_Wrap_s : register(s8);
Texture2D<float4> g_tex_diffuse : register(t0);
Texture2D<float4> g_tex_normal : register(t1);
Texture2D<float4> g_tex_srm : register(t2);
Texture2D<float4> g_tex_translucent : register(t3);
Texture2D<float4> g_tex_viewReflection : register(t4);
Texture2D<float4> g_tex_height : register(t5);
Texture2D<float4> g_tex_base2 : register(t6);
Texture2D<float4> g_tex_metalicLayer : register(t7);
Texture2D<float4> g_tex_mesh : register(t15);


// 3Dmigoto declarations
#define cmp -


void NativeMaterialMain(
  float4 v0 : SV_Position0,
  float4 v1 : COLOR0,
  float4 v2 : TEXCOORD0,
  float4 v3 : TEXCOORD1,
  float4 v4 : TEXCOORD2,
  float4 v5 : TEXCOORD3,
  float4 v6 : TEXCOORD4,
  float4 v7 : TEXCOORD5,
  float4 v8 : TEXCOORD6,
  float3 v9 : TEXCOORD7,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1,
  out float4 o2 : SV_Target2)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v0.xy;
  r1.z = g_psMaterial.m_materials[0].x;
  r1.z = r1.z;
  r2.xyz = float3(1,1,1) * v9.xyz;
  r0.zw = g_tex_normal.Sample(g_sampler_normal_s, v2.xy).wy;
  r0.zw = r0.zw;
  r0.zw = float2(2,2) * r0.zw;
  r3.xy = float2(-1,-1) + r0.zw;
  r0.z = r3.x * r3.x;
  r0.z = -r0.z;
  r0.z = 1 + r0.z;
  r0.w = r3.y * r3.y;
  r0.w = -r0.w;
  r0.z = r0.z + r0.w;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = 9.99999975e-05 + r0.z;
  r0.w = rsqrt(r0.z);
  r3.z = r0.z * r0.w;
  r3.xy = r3.xy;
  r3.z = r3.z;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r4.xyz = v7.xyz;
  r0.z = dot(r4.xyz, r4.xyz);
  r0.z = rsqrt(r0.z);
  r4.xyz = r4.xyz * r0.zzz;
  r5.xyz = r3.xxx;
  r4.xyz = r5.xyz * r4.xyz;
  r5.xyz = v8.xyz;
  r0.z = dot(r5.xyz, r5.xyz);
  r0.z = rsqrt(r0.z);
  r5.xyz = r5.xyz * r0.zzz;
  r3.xyw = r3.yyy;
  r3.xyw = r5.xyz * r3.xyw;
  r3.xyw = r4.xyz + r3.xyw;
  r0.z = dot(r2.xyz, r2.xyz);
  r0.z = rsqrt(r0.z);
  r2.xyz = r2.xyz * r0.zzz;
  r4.xyz = r3.zzz;
  r2.xyz = r4.xyz * r2.xyz;
  r2.xyz = r3.xyw + r2.xyz;
  r0.z = dot(r2.xyz, r2.xyz);
  r0.z = rsqrt(r0.z);
  r2.xyz = r2.xyz * r0.zzz;
  r2.xyz = r2.xyz;
  r3.xyw = g_tex_srm.Sample(g_sampler_srm_s, v2.xy).xyz;
  r3.xyw = r3.xyw;
  r3.xyw = r3.xyw;
  r4.xyz = v5.xyz;
  r4.xyz = r4.xyz;
  r5.xyzw = g_tex_diffuse.Sample(g_sampler_diffuse_s, v2.xy).xyzw;
  r6.xyz = float3(0.100000001,0.100000001,0.100000001) * g_psMaterial.m_materials[4].xyz;
  r6.xyz = -r6.xyz;
  r4.xyz = r6.xyz + r4.xyz;
  r0.z = dot(r4.xyz, r4.xyz);
  r0.z = sqrt(r0.z);
  r0.w = 0.100000001 * g_psMaterial.m_materials[5].x;
  r0.w = -r0.w;
  r2.w = r0.z + r0.w;
  r4.x = 0.100000001 * g_psMaterial.m_materials[5].y;
  r0.w = r4.x + r0.w;
  r0.w = r2.w / r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r0.w = r0.w;
  r4.xyzw = g_tex_metalicLayer.Sample(g_sampler_metalicLayer_s, v2.xy).wxyz;
  r2.w = -r0.w;
  r2.w = 1 + r2.w;
  r2.w = max(0, r2.w);
  r2.w = min(1, r2.w);
  r6.xyz = r4.xxx * r2.www;
  r4.yzw = r4.yzw;
  r2.w = 0.100000001 * g_psMaterial.m_materials[6].x;
  r2.w = -r2.w;
  r0.z = r2.w + r0.z;
  r4.x = 0.100000001 * g_psMaterial.m_materials[6].y;
  r2.w = r4.x + r2.w;
  r0.z = r0.z / r2.w;
  r0.z = -r0.z;
  r0.z = 1 + r0.z;
  r0.w = -r0.w;
  r0.z = r0.z + r0.w;
  r0.w = g_psMaterial.m_materials[6].z;
  r0.z = r0.z * r0.w;
  r0.z = r5.w + r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r6.xyz = r6.xyz;
  r5.xyz = r5.xyz;
  r0.z = r0.z;
  r6.xyz = r6.xyz;
  r4.yzw = r4.yzw;
  r7.xy = v2.xy;
  r8.xyz = v4.xyz;
  r5.xyz = r5.xyz;
  r4.yzw = r4.yzw;
  r6.xyz = r6.xyz;
  r7.xy = r7.xy;
  r8.xyz = r8.xyz;
  r5.xyz = r5.xyz;
  r4.yzw = r4.yzw;
  r6.xyz = r6.xyz;
  r8.w = -1 * r8.y;
  r0.w = g_tex_height.Sample(g_sampler_height_s, v2.xy).y;
  r0.w = r0.w;
  r2.w = dot(r8.xzw, r8.xzw);
  r2.w = rsqrt(r2.w);
  r8.xyz = r8.xwz * r2.www;
  r7.zw = r8.xy * r0.ww;
  r7.zw = g_psMaterial.m_materials[1].xx * r7.zw;
  r7.zw = float2(0.25,0.25) * r7.zw;
  r7.zw = r7.zw / r8.zz;
  r7.zw = -r7.zw;
  r7.xy = r7.xy + r7.zw;
  r7.xyz = g_tex_base2.Sample(g_sampler_base2_s, r7.xy).xyz;
  r7.xyz = r7.xyz;
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r0.w = r0.w * r0.w;
  r0.w = r0.w * r0.w;
  r0.w = 1 * r0.w;
  r0.w = -r0.w;
  r8.xyz = float3(1,1,1) + r0.www;
  r9.xyz = -r5.xyz;
  r7.xyz = r9.xyz + r7.xyz;
  r7.xyz = r8.xyz * r7.xyz;
  r5.xyz = r7.xyz + r5.xyz;
  r7.xyz = -r5.xyz;
  r4.xyz = r7.xyz + r4.yzw;
  r4.xyz = r6.xyz * r4.xyz;
  r4.xyz = r5.xyz + r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r1.x = r3.y;
  r4.xyz = r4.xyz;
  r1.x = r1.x;
  r4.xyz = r4.xyz;
  r1.x = r1.x;
  r4.xyz = r4.xyz;
  r1.x = r1.x;
  r1.w = g_tex_translucent.Sample(g_sampler_translucent_s, v2.xy).y;
  r1.w = r1.w;
  r1.w = r1.w;
  r0.z = r0.z;
  r5.xyz = v9.xyz;
  r6.xyz = v6.xyz;
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
  r4.w = g_psMaterial.m_materials[3].x;
  r2.w = log2(r2.w);
  r2.w = r4.w * r2.w;
  r2.w = exp2(r2.w);
  r5.x = -r0.z;
  r0.w = r5.x + r0.w;
  r0.w = r2.w * r0.w;
  r5.x = r0.z + r0.w;
  r0.z = cmp(r4.w == 0.000000);
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
  r1.y = r3.x;
  r1.y = r1.y;
  r1.y = r1.y;
  r1.y = r1.y;
  r4.xyz = r4.xyz;
  r0.xy = r2.xy;
  r1.y = r1.y;
  r4.xyz = r4.xyz;
  r0.xy = r0.xy;
  r1.y = r1.y;
  r0.xy = float2(0.5,-0.5) * r0.xy;
  r0.xy = float2(0.5,0.5) + r0.xy;
  r0.xyz = g_tex_viewReflection.Sample(g_sampler_viewReflection_s, r0.xy).xyz;
  r0.xyz = r0.xyz * r1.yyy;
  r0.xyz = r4.xyz + r0.xyz;
  r0.xyz = max(float3(0,0,0), r0.xyz);
  r0.xyz = min(float3(1,1,1), r0.xyz);
  r0.xyz = r0.xyz;
  r1.y = r1.y;
  r1.y = r1.y;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xy = float2(0.5,0.5) * r2.xy;
  r3.xy = float2(0.5,0.5) + r2.xy;
  r2.x = 0.5 * r2.z;
  r2.x = 0.5 + r2.x;
  r3.z = sqrt(r2.x);
  r3.xy = r3.xy;
  r3.z = r3.z;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r0.xyz = r0.xyz;
  r1.w = r1.w;
  r3.xyz = r3.xyz;
  r1.x = r1.x;
  r1.y = r1.y;
  r3.w = r3.w;
  r1.z = r1.z;
  r0.xyz = r0.xyz;
  r1.w = r1.w;
  r3.xyz = r3.xyz;
  r1.x = r1.x;
  r1.y = r1.y;
  r3.w = r3.w;
  r1.z = r1.z;
  r0.xyz = r0.xyz;
  r0.w = 1;
  r3.xyz = r3.xyz;
  r3.w = r3.w;
  r1.x = r1.x;
  r1.y = r1.y;
  r1.z = r1.z;
  r1.w = r1.w;
  r0.xyzw = r0.xyzw;
  r3.xyzw = r3.xyzw;
  r1.xyzw = r1.xyzw;
  o0.xyzw = r0.xyzw;
  o1.xyzw = r3.xyzw;
  o2.xyzw = r1.xyzw;
  return;
}
// clang-format on

void main(
    float4 v0 : SV_Position0,
    float4 v1 : COLOR0,
    float4 v2 : TEXCOORD0,
    float4 v3 : TEXCOORD1,
    float4 v4 : TEXCOORD2,
    float4 v5 : TEXCOORD3,
    float4 v6 : TEXCOORD4,
    float4 v7 : TEXCOORD5,
    float4 v8 : TEXCOORD6,
    float3 v9 : TEXCOORD7,
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
  NativeMaterialMain(v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, o0, o1, o2);
  clean_material = float4(0.0, 0.0, 0.0, -1.0);
  material_owner = 0u;
  const uint capture = material_capture_parameters.Load(int3(0, 0, 0));
  if ((capture & 1u) == 0u) return;
  clean_material = float4(renodx::color::srgb::Decode(saturate(o0.rgb)), 1.0);
  material_owner = uint4(PackMaterialUNorm(o0), PackMaterialUNorm(o1), PackMaterialUNorm(o2), asuint(v0.z));
}
