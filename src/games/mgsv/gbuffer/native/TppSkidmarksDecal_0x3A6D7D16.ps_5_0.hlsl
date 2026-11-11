#include "../material_capture.hlsli"

// clang-format off
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:24 2026

cbuffer cPSScene : register(b2)
{

  struct
  {
    float4x4 m_projectionView;
    float4x4 m_projection;
    float4x4 m_view;
    float4x4 m_shadowProjection;
    float4x4 m_shadowProjection2;
    float4 m_eyepos;
    float4 m_projectionParam;
    float4 m_viewportSize;
    float4 m_exposure;
    float4 m_fogParam[3];
    float4 m_fogColor;
    float4 m_cameraCenterOffset;
    float4 m_shadowMapResolutions;
  } g_psScene : packoffset(c0);

}

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

SamplerState g_samplerPoint_Clamp_s : register(s9);
SamplerState g_samplerLinear_Wrap_s : register(s10);
Texture2D<float4> inNormalTexture : register(t0);
Texture2D<float4> inMaskTexture : register(t1);
Texture2D<float4> inDepthTexture : register(t13);


// 3Dmigoto declarations
#define cmp -


void NativeMaterialMain(
  float4 v0 : SV_Position0,
  float4 v1 : TEXCOORD0,
  float4 v2 : TEXCOORD1,
  float4 v3 : TEXCOORD2,
  float4 v4 : TEXCOORD3,
  float4 v5 : TEXCOORD4,
  float3 v6 : TEXCOORD5,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1,
  out float4 o2 : SV_Target2)
{
  float4 r0,r1,r2,r3,r4,r5,r6;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = float2(0.49609375,0.49609375) + r0.xy;
  r0.xy = g_psSystem.m_renderBuffer.zw * r0.xy;
  r0.xy = r0.xy;
  r0.z = v1.w;
  r0.w = v1.z;
  r1.xyz = v2.xyz;
  r1.w = v3.x;
  r2.x = v3.y;
  r0.xy = r0.xy;
  r0.z = r0.z;
  r0.w = r0.w;
  r1.xyz = r1.xyz;
  r1.w = r1.w;
  r2.x = r2.x;
  r0.xy = r0.xy;
  r2.y = g_psMaterial.m_materials[0].x;
  r2.z = g_psMaterial.m_materials[0].y;
  r2.w = g_psMaterial.m_materials[0].z;
  r0.w = -r0.w;
  r0.w = r2.y + r0.w;
  r0.w = frac(r0.w);
  r3.xyz = v6.xyz;
  r2.y = dot(r3.xyz, r3.xyz);
  r2.y = rsqrt(r2.y);
  r3.xyz = r3.xyz * r2.yyy;
  r4.xyz = -r1.xyz;
  r1.x = dot(r4.xyz, r4.xyz);
  r1.x = rsqrt(r1.x);
  r4.xyz = r4.xyz * r1.xxx;
  r1.x = dot(r3.xyz, r4.xyz);
  r1.x = 0.949999988 * r1.x;
  r1.x = 0.0500000007 + r1.x;
  r0.xy = r0.xy;
  r4.xy = g_psScene.m_projectionParam.zw;
  r0.x = inDepthTexture.SampleLevel(g_samplerPoint_Clamp_s, r0.xy, 0).x;
  r0.x = r0.x;
  r0.x = r0.x;
  r4.xy = r4.xy;
  r0.y = -r4.y;
  r0.x = r0.x + r0.y;
  r0.x = r4.x / r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.y = -r1.z;
  r0.x = r0.x + r0.y;
  r0.y = -r0.x;
  r0.x = max(r0.x, r0.y);
  r0.x = r0.x * r1.x;
  r1.xy = inNormalTexture.Sample(g_samplerLinear_Wrap_s, v1.xy).wy;
  r1.xy = r1.xy;
  r0.y = inMaskTexture.Sample(g_samplerLinear_Wrap_s, v1.xy).y;
  r0.y = r0.y;
  r1.xy = r1.xy;
  r1.xy = float2(2,2) * r1.xy;
  r1.xz = float2(-1,-1) + r1.yx;
  r2.y = r1.z * r1.z;
  r2.y = -r2.y;
  r2.y = 1 + r2.y;
  r3.w = r1.x * r1.x;
  r3.w = -r3.w;
  r2.y = r3.w + r2.y;
  r2.y = max(0, r2.y);
  r2.y = min(1, r2.y);
  r2.y = 9.99999975e-05 + r2.y;
  r3.w = rsqrt(r2.y);
  r1.y = r3.w * r2.y;
  r1.xz = r1.xz;
  r1.y = r1.y;
  r1.xyz = r1.xyz;
  r4.xyz = r1.zzz * r2.xxx;
  r4.xyz = r4.xyz;
  r1.xy = r1.xy;
  r5.xyz = v4.xyz;
  r1.z = dot(r5.xyz, r5.xyz);
  r1.z = rsqrt(r1.z);
  r5.xyz = r5.xyz * r1.zzz;
  r4.xyz = r5.xyz * r4.xyz;
  r5.xyz = v5.xyz;
  r1.z = dot(r5.xyz, r5.xyz);
  r1.z = rsqrt(r1.z);
  r5.xyz = r5.xyz * r1.zzz;
  r6.xyz = r1.xxx;
  r5.xyz = r6.xyz * r5.xyz;
  r4.xyz = r5.xyz + r4.xyz;
  r1.xyz = r1.yyy;
  r1.xyz = r3.xyz * r1.xyz;
  r1.xyz = r4.xyz + r1.xyz;
  r2.x = dot(r1.xyz, r1.xyz);
  r2.x = rsqrt(r2.x);
  r1.xyz = r2.xxx * r1.xyz;
  r1.xyz = r1.xyz;
  r0.x = r0.x / 0.0250000004;
  r0.x = -0.800000012 + r0.x;
  r0.x = -r0.x;
  r0.x = 1 + r0.x;
  r0.x = max(0, r0.x);
  r0.x = min(1, r0.x);
  r0.x = r0.x * r0.z;
  r0.x = r0.x * r0.x;
  r0.z = r0.w * r2.w;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.x = r0.x * r0.z;
  r0.z = -r0.w;
  r0.z = 1 + r0.z;
  r0.z = r0.z * r2.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.x = r0.x * r0.z;
  r0.x = r0.y * r0.x;
  r2.x = 0.150000006;
  r2.y = 0.150000006;
  r2.z = 0.150000006;
  r0.y = 0.600000024 * r0.x;
  r2.w = r0.y * r1.w;
  r1.xyz = r1.xyz;
  r0.yz = float2(0.5,0.5) * r1.xy;
  r3.xy = float2(0.5,0.5) + r0.yz;
  r0.y = 0.5 * r1.z;
  r0.y = 0.5 + r0.y;
  r3.z = sqrt(r0.y);
  r3.xy = r3.xy;
  r3.z = r3.z;
  r0.y = -r1.w;
  r0.y = 1 + r0.y;
  r3.w = r0.x * r0.y;
  r3.xyz = r3.xyz;
  r2.xyzw = r2.xyzw;
  r3.xyzw = r3.xyzw;
  o0.xyzw = r2.xyzw;
  o1.xyzw = r3.xyzw;
  o2.xyzw = float4(0,0,0,0);
  return;
}
// clang-format on

void main(
    float4 v0 : SV_Position0,
    float4 v1 : TEXCOORD0,
    float4 v2 : TEXCOORD1,
    float4 v3 : TEXCOORD2,
    float4 v4 : TEXCOORD3,
    float4 v5 : TEXCOORD4,
    float3 v6 : TEXCOORD5,
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
