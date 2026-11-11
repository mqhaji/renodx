#include "bloom_native.hlsli"

// Clip the source before weighting, then bound the native flare signal.
float3 PrepareFlareSource(float4 color) {
  return saturate(saturate(color.rgb) * color.a);
}

// Apply the brightness stored in alpha before bilinear interpolation.
float3 SampleFlareSource(float2 uv, uint2 work_size) {
  const float2 position = uv * float2(work_size) - 0.5f;
  const int2 base = int2(floor(position));
  const int2 last = int2(work_size) - 1;
  const int2 lower = clamp(base, 0, last);
  const int2 upper = clamp(base + 1, 0, last);
  const float2 fraction = frac(position);
  const float4 a = inImage.Load(int3(lower, 0));
  const float4 b = inImage.Load(int3(upper.x, lower.y, 0));
  const float4 c = inImage.Load(int3(lower.x, upper.y, 0));
  const float4 d = inImage.Load(int3(upper, 0));
  return lerp(lerp(PrepareFlareSource(a), PrepareFlareSource(b), fraction.x), lerp(PrepareFlareSource(c), PrepareFlareSource(d), fraction.x), fraction.y);
}

void main(
    float4 v0: SV_Position0,
    float2 v1: TEXCOORD0,
    out float4 o0: SV_Target0) {
  if (CUSTOM_BLOOM_TYPE >= 0.5f) {
    uint width, height;
    inImage.GetDimensions(width, height);
    const uint2 work_size = min(uint2(width, height), BLOOM_WORK_SIZE);
    if (any(v0.xy >= float2(work_size))) {
      o0 = float4(0.f, 0.f, 0.f, 1.f);
      return;
    }
    const float2 center = 0.5f;
    const float2 offset = (v0.xy / float2(work_size) - center) * 2.f;
    const float radial_squared = dot(offset, offset) * (2.f * 1.4f * 1.4f);
    [branch]  // The flare is zero outside this circle; skip its source reads.
    if (radial_squared >= 1.f) {
      o0 = float4(0.f, 0.f, 0.f, 1.f);
      return;
    }
    const float2 edge = 1.f - saturate(abs(offset) * 64.f - 31.f);
    const float radial_fourth = radial_squared * radial_squared;
    const float falloff = radial_fourth * (1.f - radial_fourth * radial_fourth) * edge.x * edge.y;
    const float red = SampleFlareSource(center + offset * 0.94f, work_size).r;
    const float green = SampleFlareSource(center + offset, work_size).g;
    const float blue = SampleFlareSource(center + offset * 1.05f, work_size).b;
    o0 = float4(saturate(float3(red, green, blue) * falloff), 1.f);
    return;
  }

  float4 r0, r1, r2, r3;

  r0.xy = float2(-0.5, -0.5) + v0.xy;
  r0.xy = float2(0.5, 0.5) + r0.xy;
  r0.xy = g_psSystem.m_renderBuffer.zw * r0.xy;
  r0.zw = g_psObject.m_localParam[0].xy;
  r1.xy = g_psObject.m_localParam[1].xy;
  r1.zw = -r0.zw;
  r0.xy = r1.zw + r0.xy;
  r0.xy = float2(2, 2) * r0.xy;
  r1.z = dot(r0.xy, r0.xy);
  r1.z = sqrt(r1.z);
  r1.z = g_psObject.m_localParam[0].z * r1.z;
  r1.z = 1.39999998 * r1.z;
  r2.xy = -r0.xy;
  r2.xy = max(r2.xy, r0.xy);
  r1.xy = r2.xy * r1.xy;
  r1.xy = float2(32, 32) * r1.xy;
  r1.xy = float2(-31, -31) + r1.xy;
  r1.xy = max(float2(0, 0), r1.xy);
  r1.xy = min(float2(1, 1), r1.xy);
  r1.xy = -r1.xy;
  r1.xy = float2(1, 1) + r1.xy;
  r1.w = r1.z * r1.z;
  r1.w = max(0, r1.w);
  r1.w = min(1, r1.w);
  r1.w = r1.w * r1.w;
  r2.x = -r1.z;
  r1.z = max(r2.x, r1.z);
  r1.z = max(9.99999997e-07, r1.z);
  r1.z = r1.z * r1.z;
  r1.z = r1.z * r1.z;
  r1.z = r1.z * r1.z;
  r1.z = 1 * r1.z;
  r1.z = -r1.z;
  r1.z = 1 + r1.z;
  r1.z = max(0, r1.z);
  r1.z = min(1, r1.z);
  r2.xy = r0.xy + r0.zw;
  r2.xw = inImage.Sample(g_samplerLinear_Clamp_s, r2.xy).yw;
  r3.xy = float2(0.939999998, 0.939999998) * r0.xy;
  r3.xy = r3.xy + r0.zw;
  r3.x = inImage.Sample(g_samplerLinear_Clamp_s, r3.xy).x;
  r0.xy = float2(1.04999995, 1.04999995) * r0.xy;
  r0.xy = r0.xy + r0.zw;
  r3.z = inImage.Sample(g_samplerLinear_Clamp_s, r0.xy).z;
  r0.x = r1.w * r1.z;
  r0.x = r0.x * r1.x;
  r0.x = r0.x * r1.y;
  r3.y = r2.x;
  r2.xyz = r3.xyz * r0.xxx;
  o0.xyzw = saturate(r2);
  return;
}
