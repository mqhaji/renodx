#include "../../common.hlsli"
#include "composition.hlsli"

// Port of the game's LocalReflectionComposite, which adds the native reflection
// result to the scene. With an enhanced result bound for this view, the mod
// composes that result instead (composition.hlsli).
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:29 2026

cbuffer cPSSystem : register(b0) {
  struct
  {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem : packoffset(c0);
}

SamplerState g_samplerLinear_Wrap_s : register(s10);
Texture2D<float4> inColorImage : register(t0);
Texture2D<float4> inRefMapImage : register(t1);
Texture2D<float4> inMaterialImage : register(t2);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0 : SV_Position0,
    out float4 o0 : SV_Target0) {
  const float2 uv = (v0.xy - 0.00390625) * g_psSystem.m_renderBuffer.zw;
  ReflectionResult result = (ReflectionResult)0;
  if (CUSTOM_MGSV_ENHANCED_INDIRECT_LIGHTING) {
    result = LoadReflectionResult(uv);
  }
  if (result.replaces_native) {
    // General surfaces and the sortie floor use the same GGX result.
    // ComposeReflections takes extended sRGB: the FP16 target already holds it,
    // while a game sRGB view has decoded it and must be encoded again.
    const float3 color = inColorImage.Sample(g_samplerLinear_Wrap_s, uv).rgb;
#if FIX_UNORM_SRGB
    o0 = float4(ComposeReflections(color, result), 1.0);
#else
    o0 = float4(ComposeReflections(renodx::color::srgb::Encode(max(color, 0.0)), result), 1.0);
#endif
    return;
  }
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
  r1.xyz = inColorImage.Sample(g_samplerLinear_Wrap_s, r0.xy).xyz;
#if FIX_UNORM_SRGB
  // Mod edit: decode the FP16 target's extended sRGB, as the game's sRGB view did.
  r1.rgb = renodx::color::srgb::Decode(max(r1.rgb, 0.0));
#endif
  r1.xyz = r1.xyz;
  r0.z = inMaterialImage.Sample(g_samplerLinear_Wrap_s, r0.xy).y;
  r0.z = r0.z;
  r0.xyw = inRefMapImage.Sample(g_samplerLinear_Wrap_s, r0.xy).xyz;
  r0.xyw = r0.xyw;
  r0.xyz = r0.xyw * r0.zzz;
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
