#include "bloom_native.hlsli"

// The native UNORM blend clamped each texel before the first blur sampled it.
float4 SampleVanillaBloom(float2 uv) {
  if (g_psObject.m_localParam[3].z == 1.f) {
    return inImage.Sample(g_samplerLinear_Clamp_s, uv);
  }
  uint width, height;
  inImage.GetDimensions(width, height);
  const float2 position = uv * float2(width, height) - 0.5f;
  const int2 base = int2(floor(position));
  const int2 last = int2(width, height) - 1;
  const int2 lower = clamp(base, 0, last);
  const int2 upper = clamp(base + 1, 0, last);
  const float2 fraction = frac(position);
  const float4 a = saturate(inImage.Load(int3(lower, 0)));
  const float4 b = saturate(inImage.Load(int3(upper.x, lower.y, 0)));
  const float4 c = saturate(inImage.Load(int3(lower.x, upper.y, 0)));
  const float4 d = saturate(inImage.Load(int3(upper, 0)));
  return lerp(lerp(a, b, fraction.x), lerp(c, d, fraction.x), fraction.y);
}

void main(
    float2 v0: TEXCOORD0,
    float4 v1: SV_Position0,
    out float4 o0: SV_Target0) {
  if (CUSTOM_BLOOM_TYPE >= 0.5f) {
    uint width, height;
    inImage.GetDimensions(width, height);
    const uint2 work_size = min(uint2(width, height), BLOOM_WORK_SIZE);
    if (any(v1.xy >= float2(work_size))) {
      o0 = float4(0.f, 0.f, 0.f, 1.f);
      return;
    }
    const float2 inverse_texture_size = rcp(float2(width, height));
    // Keep the glow tight and prevent taps from reading the cleared padding.
    const float2 lower_uv = max(v1.xy - 0.25f, 0.5f) * inverse_texture_size;
    const float2 upper_uv = min(v1.xy + 0.25f, float2(work_size) - 0.5f) * inverse_texture_size;
    precise float3 color = inImage.SampleLevel(g_samplerLinear_Clamp_s, lower_uv, 0.f).rgb;
    color += inImage.SampleLevel(g_samplerLinear_Clamp_s, float2(upper_uv.x, lower_uv.y), 0.f).rgb;
    color += inImage.SampleLevel(g_samplerLinear_Clamp_s, float2(lower_uv.x, upper_uv.y), 0.f).rgb;
    color += inImage.SampleLevel(g_samplerLinear_Clamp_s, upper_uv, 0.f).rgb;
    // Average in linear light without clipping highlights.
    o0 = float4(color * 0.25f, 1.f);
    return;
  }

  float4 r0, r1, r2, r3;

  r0.xyzw = float4(-0.5, -0.5, -0.5, -0.5) + v1.xyxy;
  r1.xyzw = g_psObject.m_localParam[3].xyxy;
  r0.xyzw = float4(0.5, 0.5, 0.5, 0.5) + r0.xyzw;
  r0.xyzw = r0.xyzw * r1.xyzw;
  r1.xyzw = g_psObject.m_localParam[0].xyzw;
  r2.xyzw = r1.xzyz + r0.zwzw;
  r3.xyzw = SampleVanillaBloom(r2.xy).xyzw;
  r2.xyzw = SampleVanillaBloom(r2.zw).xyzw;
  r2.xyzw = r3.xyzw + r2.xyzw;
  r0.xyzw = r1.xwyw + r0.xyzw;
  r1.xyzw = SampleVanillaBloom(r0.xy).xyzw;
  r1.xyzw = r2.xyzw + r1.xyzw;
  r0.xyzw = SampleVanillaBloom(r0.zw).xyzw;
  r0.xyzw = r1.xyzw + r0.xyzw;
  r1.x = 0.25 * g_psObject.m_localParam[3].z;
  r0.xyzw = r1.xxxx * r0.xyzw;
  o0.xyzw = saturate(r0);
  return;
}
