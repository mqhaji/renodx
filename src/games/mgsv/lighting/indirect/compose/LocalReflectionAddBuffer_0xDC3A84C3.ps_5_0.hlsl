#include "../../../common.hlsli"
#include "reflection_ownership.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:33 2026

// clang-format off
cbuffer cPSObject : register(b5) {
  struct
  {
    float4x4 m_viewWorld;
    float4x4 m_world;
    float4 m_useWeightCount;
    float4 m_localParam[4];
  } g_psObject: packoffset(c0);
}

cbuffer cPSSystem : register(b0) {
  struct
  {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem: packoffset(c0);
}
// clang-format on

SamplerState g_samplerPoint_Wrap_s : register(s8);
SamplerState g_samplerLinear_Wrap_s : register(s10);
Texture2D<float4> inColorImage : register(t0);
Texture2D<float4> inRefMapImage : register(t1);
Texture2D<float4> inMaterialImage : register(t2);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0 : SV_Position0,
    out float4 o0 : SV_Target0) {
  float4 r0, r1, r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5, -0.5) + v0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = float2(0.49609375, 0.49609375) + r0.xy;
  r0.xy = g_psSystem.m_renderBuffer.zw * r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.zw = inMaterialImage.Sample(g_samplerPoint_Wrap_s, r0.xy).xy;
  r0.z = r0.z;
  r0.w = r0.w;
  r1.x = g_psObject.m_localParam[3].y;
  r0.z = r1.x + r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = 5 * r0.z;
  r1.xyzw = inRefMapImage.SampleLevel(g_samplerLinear_Wrap_s, r0.xy, r0.z).xyzw;
  const bool specular_owned = HasComposedSpecular(r0.xy);
  r0.xyz = inColorImage.Sample(g_samplerLinear_Wrap_s, r0.xy).xyz;
  // Both regular SSR and the sortie menu's planar floor trace can contribute
  // detail that GT-VBGI misses. Hit alpha, not brightness, selects the native
  // blend. Empty hits keep the already-composed GT-VBGI scene unchanged.
  const bool native_reflection = r1.w > 0.0 && r0.w > 0.0 && g_psObject.m_localParam[3].x > 0.0;
  if (specular_owned && !native_reflection) {
#if FIX_UNORM_SRGB
    o0 = float4(max(r0.xyz, 0.0), 1.0);
#else
    o0 = float4(renodx::color::srgb::Encode(max(r0.xyz, 0.0)), 1.0);
#endif
    return;
  }
#if FIX_UNORM_SRGB
  r0.rgb = renodx::color::srgb::Decode(max(0, r0.rgb));
#endif

  r0.xyz = r0.xyz;
  r1.xyz = r1.xyz * r0.www;
  r1.xyz = g_psObject.m_localParam[3].xxx * r1.xyz;
  r0.w = r1.w * r0.w;
  r0.w = g_psObject.m_localParam[3].x * r0.w;
  r1.w = -r0.w;
  r1.w = 1 + r1.w;
  // Native hit alpha replaces part of the current scene, including GT-VBGI.
  // Keep that complement: adding the reflection alone would count both terms.
  r0.xyz = r1.www * r0.xyz;
  r1.xyz = r1.xyz * r0.www;
  r0.xyz = r1.xyz + r0.xyz;
  r0.xyz = r0.xyz;
  r1.xyz = cmp(float3(0.00313080009, 0.00313080009, 0.00313080009) >= r0.xyz);
  r1.xyz = r1.xyz ? float3(1, 1, 1) : float3(0, 0, 0);
  r2.xyz = float3(12.9200001, 12.9200001, 12.9200001) * r0.xyz;
  r2.xyz = r2.xyz * r1.xyz;
  r1.xyz = -r1.xyz;
  r1.xyz = float3(1, 1, 1) + r1.xyz;
  r0.xyz = max(float3(9.99999975e-06, 9.99999975e-06, 9.99999975e-06), r0.xyz);
  r0.xyz = log2(r0.xyz);
  r0.xyz = float3(0.416666657, 0.416666657, 0.416666657) * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  r0.xyz = float3(1.05499995, 1.05499995, 1.05499995) * r0.xyz;
  r0.xyz = float3(-0.0549999997, -0.0549999997, -0.0549999997) + r0.xyz;
  r0.xyz = r1.xyz * r0.xyz;
  r0.xyz = r2.xyz + r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = 1;
  o0.xyzw = r0.xyzw;
  return;
}
