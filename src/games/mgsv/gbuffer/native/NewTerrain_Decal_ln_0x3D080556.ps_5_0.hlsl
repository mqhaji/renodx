#include "../material_capture.hlsli"

// clang-format off
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:24 2026

cbuffer cPSLight : register(b3)
{

  struct
  {
    float4 m_lightParams[11];
  } g_psLight : packoffset(c0);

}

cbuffer cPSMaterial : register(b4)
{

  struct
  {
    float4 m_materials[8];
  } g_psMaterial : packoffset(c0);

}

SamplerState inAlbedoSampler_s : register(s0);
SamplerState inNormalSampler_s : register(s1);
SamplerState inSrmSampler_s : register(s2);
SamplerState g_samplerPoint_Wrap_s : register(s8);
Texture2D<float4> inAlbedoTexture : register(t0);
Texture2D<float4> inNormalTexture : register(t1);
Texture2D<float4> inSrmTexture : register(t2);
Texture2D<float4> g_tex_mesh : register(t15);


// 3Dmigoto declarations
#define cmp -


void NativeMaterialMain(
  float4 v0 : SV_Position0,
  float4 v1 : COLOR0,
  float2 v2 : TEXCOORD0,
  float2 w2 : TEXCOORD1,
  float4 v3 : TEXCOORD2,
  float4 v4 : TEXCOORD5,
  float4 v5 : TEXCOORD6,
  float4 v6 : TEXCOORD7,
  float4 v7 : TEXCOORD3,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1,
  out float4 o2 : SV_Target2)
{
  float4 r0,r1,r2,r3,r4,r5,r6;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v0.xy;
  r1.w = v1.w;
  r0.z = g_psLight.m_lightParams[2].x;
  r0.xy = r0.xy;
  r1.w = r1.w;
  r0.z = r0.z;
  r0.xy = r0.xy;
  r0.xy = float2(0.125,0.125) * r0.xy;
  r0.x = g_tex_mesh.Sample(g_samplerPoint_Wrap_s, r0.xy).x;
  r0.x = r0.x;
  r0.x = -r0.x;
  r0.x = r0.z + r0.x;
  r0.x = cmp(r0.x < 0);
  r0.x = r0.x ? asfloat(0xFFFFFFFFu) : 0;
  if (r0.x != 0) discard;
  r1.w = r1.w;
  r1.w = r1.w;
  r1.xyz = v1.xyz;
  r1.w = r1.w;
  r0.xy = v3.xz;
  r0.zw = w2.xy;
  r2.xyzw = v7.xyzw;
  r1.xyz = r1.xyz;
  r1.w = r1.w;
  r0.xy = r0.xy;
  r0.zw = r0.zw;
  r2.xyzw = r2.xyzw;
  r2.x = min(r2.x, r2.y);
  r2.y = min(r2.z, r2.w);
  r2.x = min(r2.x, r2.y);
  r2.x = cmp(r2.x < 0);
  r2.x = r2.x ? asfloat(0xFFFFFFFFu) : 0;
  if (r2.x != 0) discard;
  r2.y = r0.z / r0.w;
  r3.xyzw = g_psMaterial.m_materials[2].xyzw * r2.yyyy;
  r3.xyzw = g_psMaterial.m_materials[3].xyzw + r3.xyzw;
  r0.zw = -r3.xy;
  r0.xy = r0.xy + r0.zw;
  r0.zw = r3.zw + r0.zw;
  r2.x = dot(r0.zw, r0.xy);
  r0.xy = g_psMaterial.m_materials[4].xy * r2.xy;
  r0.xy = g_psMaterial.m_materials[4].zw + r0.xy;
  r2.xyzw = inAlbedoTexture.Sample(inAlbedoSampler_s, r0.xy).xyzw;
  r3.xy = inSrmTexture.Sample(inSrmSampler_s, r0.xy).yx;
  r3.xy = r3.xy;
  r0.xy = r0.xy;
  r0.xzw = inNormalTexture.Sample(inNormalSampler_s, r0.xy).xyz;
  r0.xzw = r0.xzw;
  r0.xzw = float3(2,2,2) * r0.xzw;
  r0.xzw = float3(-1,-1,-1) + r0.xzw;
  r0.xzw = r0.xzw;
  r0.xzw = r0.xzw;
  r0.xzw = r0.xzw;
  r4.x = dot(v4.xyz, v4.xyz);
  r4.x = rsqrt(r4.x);
  r4.xyz = v4.xyz * r4.xxx;
  r5.xyz = r0.xxx;
  r4.xyz = r5.xyz * r4.xyz;
  r0.x = dot(v5.xyz, v5.xyz);
  r0.x = rsqrt(r0.x);
  r5.xyz = v5.xyz * r0.xxx;
  r6.xyz = r0.zzz;
  r5.xyz = r6.xyz * r5.xyz;
  r4.xyz = r5.xyz + r4.xyz;
  r0.x = dot(v6.xyz, v6.xyz);
  r0.x = rsqrt(r0.x);
  r5.xyz = v6.xyz * r0.xxx;
  r0.xzw = r0.www;
  r0.xzw = r5.xyz * r0.xzw;
  r0.xzw = r4.xyz + r0.xzw;
  r4.x = dot(r0.xzw, r0.xzw);
  r4.x = rsqrt(r4.x);
  r0.xzw = r4.xxx * r0.xzw;
  r1.xyzw = r2.xyzw * r1.xyzw;
  r2.xy = g_psLight.m_lightParams[6].xy * r0.yy;
  r2.xy = g_psLight.m_lightParams[6].zw + r2.xy;
  r2.xy = max(float2(0,0), r2.xy);
  r2.xy = min(float2(1,1), r2.xy);
  r0.y = min(r2.x, r2.y);
  r0.y = r1.w * r0.y;
  r1.xyz = r1.xyz;
  r1.w = r0.y;
  r0.xzw = r0.xzw;
  r0.xz = float2(0.5,0.5) * r0.xz;
  r2.xy = float2(0.5,0.5) + r0.xz;
  r0.x = 0.5 * r0.w;
  r0.x = 0.5 + r0.x;
  r2.z = sqrt(r0.x);
  r2.xy = r2.xy;
  r2.z = r2.z;
  r2.xyz = r2.xyz;
  r0.x = g_psMaterial.m_materials[5].y;
  r2.w = r0.y * r0.x;
  r0.x = cmp(r0.y >= 0.5);
  r3.w = r0.x ? 1 : 0;
  r3.z = g_psMaterial.m_materials[5].x;
  r3.xy = r3.xy;
  r1.xyzw = r1.xyzw;
  r2.xyz = r2.xyz;
  r2.w = r2.w;
  r3.xyzw = r3.xyzw;
  o0.xyzw = r1.xyzw;
  o1.xyzw = r2.xyzw;
  o2.xyzw = r3.xyzw;
  return;
}
// clang-format on

void main(
    float4 v0 : SV_Position0,
    float4 v1 : COLOR0,
    float2 v2 : TEXCOORD0,
    float2 w2 : TEXCOORD1,
    float4 v3 : TEXCOORD2,
    float4 v4 : TEXCOORD5,
    float4 v5 : TEXCOORD6,
    float4 v6 : TEXCOORD7,
    float4 v7 : TEXCOORD3,
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
