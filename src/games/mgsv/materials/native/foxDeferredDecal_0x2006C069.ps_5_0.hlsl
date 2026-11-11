#include "../material_capture.hlsli"

// clang-format off
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:22 2026

SamplerState g_sampler_diffuse_s : register(s0);
SamplerState g_sampler_normal_s : register(s1);
Texture2D<float4> g_tex_diffuse : register(t0);
Texture2D<float4> g_tex_normal : register(t1);


// 3Dmigoto declarations
#define cmp -


void NativeMaterialMain(
  float4 v0 : SV_Position0,
  float4 v1 : COLOR0,
  float4 v2 : TEXCOORD0,
  float4 v3 : TEXCOORD5,
  float4 v4 : TEXCOORD6,
  float3 v5 : TEXCOORD7,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1,
  out float4 o2 : SV_Target2)
{
  float4 r0,r1,r2,r3;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = g_tex_diffuse.Sample(g_sampler_diffuse_s, v2.xy).xyzw;
  r0.xyzw = r0.xyzw;
  r1.xy = g_tex_normal.Sample(g_sampler_normal_s, v2.xy).wy;
  r1.xy = r1.xy;
  r1.xy = float2(2,2) * r1.xy;
  r1.xy = float2(-1,-1) + r1.xy;
  r1.w = r1.x * r1.x;
  r1.w = -r1.w;
  r1.w = 1 + r1.w;
  r2.x = r1.y * r1.y;
  r2.x = -r2.x;
  r1.w = r2.x + r1.w;
  r1.w = max(0, r1.w);
  r1.w = min(1, r1.w);
  r1.w = 9.99999975e-05 + r1.w;
  r2.x = rsqrt(r1.w);
  r1.z = r2.x * r1.w;
  r1.xy = r1.xy;
  r1.z = r1.z;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r2.xyz = v3.xyz;
  r1.w = dot(r2.xyz, r2.xyz);
  r1.w = rsqrt(r1.w);
  r2.xyz = r2.xyz * r1.www;
  r3.xyz = r1.xxx;
  r2.xyz = r3.xyz * r2.xyz;
  r3.xyz = v4.xyz;
  r1.x = dot(r3.xyz, r3.xyz);
  r1.x = rsqrt(r1.x);
  r3.xyz = r3.xyz * r1.xxx;
  r1.xyw = r1.yyy;
  r1.xyw = r3.xyz * r1.xyw;
  r1.xyw = r2.xyz + r1.xyw;
  r2.xyz = v5.xyz;
  r2.w = dot(r2.xyz, r2.xyz);
  r2.w = rsqrt(r2.w);
  r2.xyz = r2.xyz * r2.www;
  r3.xyz = r1.zzz;
  r2.xyz = r3.xyz * r2.xyz;
  r1.xyz = r2.xyz + r1.xyw;
  r1.w = dot(r1.xyz, r1.xyz);
  r1.w = rsqrt(r1.w);
  r1.xyz = r1.xyz * r1.www;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xy = float2(0.5,0.5) * r1.xy;
  r2.xy = float2(0.5,0.5) + r1.xy;
  r1.x = 0.5 * r1.z;
  r1.x = 0.5 + r1.x;
  r2.z = sqrt(r1.x);
  r2.xy = r2.xy;
  r2.z = r2.z;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = float3(1,0,0) * r0.xyz;
  r0.xyz = r0.xyz;
  o0.xyz = r0.xyz;
  o0.w = r0.w;
  o1.xyz = r2.xyz;
  o1.w = r0.w;
  o2.xyzw = float4(0,0,0,0);
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
  NativeMaterialMain(v0, v1, v2, v3, v4, v5, o0, o1, o2);
  clean_material = float4(0.0, 0.0, 0.0, -1.0);
  material_owner = 0u;
  const uint capture = material_capture_parameters.Load(int3(0, 0, 0));
  if ((capture & 1u) == 0u) return;
  clean_material = float4(renodx::color::srgb::Decode(saturate(o0.rgb)), 1.0);
  material_owner = uint4(PackMaterialUNorm(o0), PackMaterialUNorm(o1), PackMaterialUNorm(o2), asuint(v0.z));
}
