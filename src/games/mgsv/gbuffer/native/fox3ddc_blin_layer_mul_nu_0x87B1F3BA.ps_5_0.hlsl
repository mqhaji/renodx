#include "../material_capture.hlsli"

// clang-format off
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:28 2026

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
SamplerState g_sampler_layer_s : register(s3);
SamplerState g_sampler_layerMask_s : register(s4);
SamplerState g_samplerPoint_Wrap_s : register(s8);
Texture2D<float4> g_tex_diffuse : register(t0);
Texture2D<float4> g_tex_normal : register(t1);
Texture2D<float4> g_tex_srm : register(t2);
Texture2D<float4> g_tex_layer : register(t3);
Texture2D<float4> g_tex_layerMask : register(t4);
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
  float4 r0,r1,r2,r3,r4,r5;
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
  r0.z = g_tex_layerMask.Sample(g_sampler_layer_s, v2.xy).x;
  r0.z = r0.z;
  r0.z = r0.z;
  r1.xyz = r1.xyz;
  r2.xyzw = g_tex_layer.Sample(g_sampler_layerMask_s, v3.xy).xyzw;
  r2.xyz = r1.xyz * r2.xyz;
  r3.xyz = r2.www * r0.zzz;
  r4.xyz = -r1.xyz;
  r2.xyz = r4.xyz + r2.xyz;
  r2.xyz = r3.xyz * r2.xyz;
  r1.xyz = r2.xyz + r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r2.xyz = float3(1,1,1) * v6.xyz;
  r0.zw = g_tex_normal.Sample(g_sampler_normal_s, v2.zw).wy;
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
  r4.xyz = v4.xyz;
  r0.z = dot(r4.xyz, r4.xyz);
  r0.z = rsqrt(r0.z);
  r4.xyz = r4.xyz * r0.zzz;
  r5.xyz = r3.xxx;
  r4.xyz = r5.xyz * r4.xyz;
  r5.xyz = v5.xyz;
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
  r3.xyz = g_tex_srm.Sample(g_sampler_srm_s, v2.xy).xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r0.z = r3.x;
  r0.z = r0.z;
  r0.z = r0.z;
  r0.z = r0.z;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r3.x = r3.y;
  r1.xyz = r1.xyz;
  r3.x = r3.x;
  r1.xyz = r1.xyz;
  r3.x = r3.x;
  r1.xyz = r1.xyz;
  r3.x = r3.x;
  r1.w = r1.w;
  r1.w = r1.w;
  r1.w = r1.w;
  r1.w = r1.w;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.w = r1.w;
  r4.y = v1.w;
  r0.xy = r0.xy;
  r1.w = r1.w;
  r4.y = r4.y;
  r0.xy = r0.xy;
  r0.w = g_psSystem.m_param.w;
  r0.xy = float2(0.125,0.125) * r0.xy;
  r5.yz = g_tex_mesh.Sample(g_samplerPoint_Wrap_s, r0.xy).xw;
  r5.yz = r5.yz;
  r5.x = r5.z * r0.w;
  r4.x = r1.w;
  r0.xy = -r5.xy;
  r0.xy = r4.xy + r0.xy;
  r0.xy = cmp(r0.xy < float2(0,0));
  r0.x = (int)r0.y | (int)r0.x;
  r0.x = r0.x ? asfloat(0xFFFFFFFFu) : 0;
  if (r0.x != 0) discard;
  r1.w = r1.w;
  r1.w = r1.w;
  r1.w = r1.w;
  r1.w = r1.w;
  r1.w = r1.w;
  r1.w = r1.w;
  r3.y = r0.z;
  r3.y = r3.y;
  r3.y = r3.y;
  r3.y = r3.y;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r0.xy = float2(0.5,0.5) * r2.xy;
  r0.xy = float2(0.5,0.5) + r0.xy;
  r2.x = 0.5 * r2.z;
  r2.x = 0.5 + r2.x;
  r0.z = sqrt(r2.x);
  r0.xy = r0.xy;
  r0.z = r0.z;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r1.xyz = r1.xyz;
  r0.xyz = r0.xyz;
  r3.x = r3.x;
  r3.y = r3.y;
  r3.z = r3.z;
  r1.w = r1.w;
  r1.xyz = r1.xyz;
  r0.xyz = r0.xyz;
  r3.x = r3.x;
  r3.y = r3.y;
  r3.z = r3.z;
  r1.w = r1.w;
  r1.xyz = r1.xyz;
  r1.w = r1.w;
  r0.xyz = r0.xyz;
  r0.w = r1.w;
  r3.x = r3.x;
  r3.y = r3.y;
  r3.z = r3.z;
  r3.w = r1.w;
  r1.xyzw = r1.xyzw;
  r0.xyzw = r0.xyzw;
  r3.xyzw = r3.xyzw;
  o0.xyzw = r1.xyzw;
  o1.xyzw = r0.xyzw;
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
