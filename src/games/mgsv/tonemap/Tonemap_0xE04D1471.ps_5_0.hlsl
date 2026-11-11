#include "../postprocess/bloom/bloom.hlsli"
#include "tonemap.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:33 2026

// clang-format off
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
// clang-format on

SamplerState g_samplerPoint_Wrap_s : register(s8);
SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inImage : register(t0);
Texture2D<float4> inColorLUT : register(t6);
Texture2D<float4> inBloom : register(t7);

// 3Dmigoto declarations
#define cmp -

void main(
    float2 v0 : TEXCOORD0,
    float4 v1 : SV_Position0,
    out float4 o0 : SV_Target0) {
  float4 r0, r1, r2;

  r0.xy = float2(-0.5, -0.5) + v1.xy;
  r0.zw = g_psSystem.m_renderBuffer.zw;
  r1.xy = float2(0.5, 0.5) * r0.zw;
  r0.xy = r0.xy * r0.zw;
  r0.xy = r0.xy + r1.xy;
  r1.xy = v0.xy;
  r1.z = -r1.y;
  r0.zw = float2(0.5, 0.5) * r1.xz;
  r0.zw = float2(0.5, 0.5) + r0.zw;
  r1.xyz = inImage.Sample(g_samplerPoint_Wrap_s, r0.xy).xyz;

  const bool enhanced = CUSTOM_BLOOM_TYPE >= 0.5f;
  float3 bloom_color = 0.f;
  [branch]  // Each mode reads only its own bloom representation.
  if (enhanced) {
    [branch]  // Zero strength also skips the four reconstruction samples.
    if (CUSTOM_BLOOM_STRENGTH != 0.f) {
      bloom_color = SampleEnhancedBloom(inBloom, g_samplerLinear_Clamp_s, r0.zw);
      const float3 scene = renodx::color::srgb::DecodeSafe(r1.rgb);
      r1.rgb = renodx::color::srgb::EncodeSafe(lerp(scene, bloom_color, CUSTOM_BLOOM_STRENGTH * 0.1f));
    }
  } else {
    bloom_color = inBloom.Sample(g_samplerLinear_Clamp_s, r0.zw + 2.f * g_psSystem.m_renderBuffer.zw).rgb;
  }

  // Run bloom through a per-channel SDR proxy so its white blend reconstructs without max-channel amplification.
  float3 perch_scale = 1.f;
  float maxch_scale = 1.f;
  if (RENODX_TONE_MAP_TYPE != 0.f && !enhanced) {
    r1.rgb *= r1.rgb;

    const float3 perch_tonemapped = ApplyAnchoredCInfinityShoulder(r1.rgb, 1.f, 0.5f, 2.f);
    perch_scale = renodx::math::DivideSafe(perch_tonemapped, r1.rgb, 1.f);
    r1.rgb = perch_tonemapped;
  } else {
    r1.xyz = r1.xyz * r1.xyz;
  }

  if (enhanced) {
    r0.xyz = r1.xyz;
  } else {
    const float3 with_bloom = lerp(r1.xyz, 1.f, min(0.5f, bloom_color));
    r0.xyz = lerp(r1.xyz, with_bloom, CUSTOM_BLOOM_STRENGTH);
  }

  // Reconstruct bloom in HDR, then use a hue-preserving SDR proxy for the LUT.
  if (RENODX_TONE_MAP_TYPE != 0.f) {
    if (!enhanced) {
      r0.rgb /= perch_scale;
    }
    maxch_scale = ComputeAnchoredCInfinityMaxChannelScale(r0.rgb, 1.f, 0.5f, 2.f);
    r0.rgb *= maxch_scale;
  }

  r0.rgb = Sample2DLUTWithScaling(r0.rgb, inColorLUT, g_samplerLinear_Clamp_s);

  // Restore the LUT result to HDR and return to the gamma domain expected by ApplyFinalTonemap.
  r0.rgb = max(0, r0.rgb);
  if (RENODX_TONE_MAP_TYPE != 0.f) {
    r0.rgb *= r0.rgb;
    r0.rgb /= maxch_scale;
    r0.rgb = renodx::math::SqrtSafe(r0.rgb);
  }

  r0.rgb = ApplyFinalTonemap(r0.rgb);

  o0.w = dot(r0.xyz, float3(0.298999995, 0.587000012, 0.114));
  o0.xyz = r0.xyz;
  return;
}
