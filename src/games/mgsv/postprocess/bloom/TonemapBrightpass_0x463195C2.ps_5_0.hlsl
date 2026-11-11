#include "bloom_native.hlsli"

void main(
    float2 v0: TEXCOORD0,
    float4 v1: SV_Position0,
    out float4 o0: SV_Target0) {
  if (CUSTOM_BLOOM_TYPE >= 0.5f) {
    uint width, height;
    inImage.GetDimensions(width, height);
    const uint2 work_size = min(uint2(width, height), BLOOM_WORK_SIZE);
    // Retain the native working rectangle for the unchanged final copy.
    if (any(v1.xy >= float2(work_size))) {
      o0 = float4(0.f, 0.f, 0.f, 1.f);
      return;
    }
    // Cancel the final copy's half-texel offset so the halo stays on its source.
    const float2 pixel = min(v1.xy + 0.5f, float2(work_size) - 0.5f);
    const float4 color = inImage.SampleLevel(g_samplerLinear_Clamp_s, pixel / float2(width, height), 0.f);
    // Keep native flare selection bounded without limiting the HDR bloom color.
    const float packed_alpha = saturate(color.a);
    const float luminance = max(renodx::color::y::from::BT709(saturate(color.rgb)), 32.f * packed_alpha * packed_alpha);
    const float2 powers = float2(luminance, luminance * luminance);
    const float4 terms = min(float4(powers, powers.y * luminance, powers.y * powers.y), 32.f);
    // Preserve linear color; alpha carries the native flare brightness scale.
    o0 = float4(color.rgb, max(0.f, dot(g_psObject.m_localParam[0], terms)));
    return;
  }

  float4 r0, r1;

  r0.xy = float2(-0.5, -0.5) + v1.xy;
  r0.zw = g_psSystem.m_renderBuffer.zw;
  r0.xy = float2(0.5, 0.5) + r0.xy;
  r0.xy = r0.xy * r0.zw;
  r0.xyzw = inImage.Sample(g_samplerPoint_Wrap_s, r0.xy).xyzw;
  r0 = saturate(r0);
  r0.rgb = renodx::color::srgb::Decode(r0.rgb);
  r0.w = r0.w * r0.w;
  r0.w = 32 * r0.w;
  r1.x = dot(r0.xyz, float3(0.212500006, 0.715399981, 0.0720999986));
  r1.x = max(r1.x, r0.w);
  r1.y = r1.x * r1.x;
  r0.w = r1.x * r1.x;
  r1.z = r0.w * r1.x;
  r0.w = r0.w * r1.x;
  r1.w = r0.w * r1.x;
  r1.xyzw = min(float4(32, 32, 32, 32), r1.xyzw);
  r0.w = dot(g_psObject.m_localParam[0].xyzw, r1.xyzw);
  r0.xyz = r0.xyz * r0.www;
  r0.w = 1;
  o0.xyzw = saturate(r0);
  return;
}
