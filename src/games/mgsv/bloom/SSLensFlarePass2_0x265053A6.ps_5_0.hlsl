#include "./bloom_native.hlsli"

void main(
    float4 v0: SV_Position0,
    float2 v1: TEXCOORD0,
    out float4 o0: SV_Target0) {
  if (CUSTOM_BLOOM_TYPE >= 0.5f) {
    uint width, height;
    inImage.GetDimensions(width, height);
    const uint2 work_size = min(uint2(width, height), BLOOM_WORK_SIZE);
    if (any(v0.xy >= float2(work_size))) {
      o0 = 0.f;
      return;
    }
    const float2 center = 0.5f;
    const float2 offset = (v0.xy / float2(work_size) - center) * 0.5f * g_psObject.m_localParam[2].w;
    const float distances[5] = { 6.25f, -3.84615374f, -1.49253726f, -0.9009009f, -0.699300706f };
    const float3 tints[5] = { float3(0.5f, 0.3f, 0.4f), float3(0.1f, 0.4f, 0.7f), float3(0.1f, 0.2f, 0.2f), float3(0.3f, 0.1f, 0.1f), float3(0.2f, 0.2f, 0.08f) };
    float4 ghosts = 0.f;
    [unroll]
    for (uint i = 0u; i < 5u; ++i) {
      const float2 source_pixel = (center + offset * distances[i]) * float2(work_size);
      // Fade the bilinear footprint to zero instead of repeating an edge texel.
      const float2 coverage = saturate(source_pixel + 0.5f) * saturate(float2(work_size) - source_pixel + 0.5f);
      const float2 pixel = clamp(source_pixel, 0.5f, float2(work_size) - 0.5f);
      const float4 color = inImage.SampleLevel(g_samplerLinear_Clamp_s, pixel / float2(width, height), 0.f) * (coverage.x * coverage.y);
      ghosts += float4(color.rgb * tints[i], color.a);
    }
    ghosts.rgb *= g_psObject.m_localParam[2].xyz;
    o0 = float4(ghosts.rgb, saturate(ghosts.a));
    return;
  }

  float4 r0, r1, r2;

  r0.xy = float2(-0.5, -0.5) + v0.xy;
  r0.xy = float2(0.5, 0.5) + r0.xy;
  r0.xy = g_psSystem.m_renderBuffer.zw * r0.xy;
  r0.zw = g_psObject.m_localParam[0].xy;
  r1.xy = -r0.zw;
  r0.xy = r1.xy + r0.xy;
  r0.xy = float2(0.5, 0.5) * r0.xy;
  r0.xy = g_psObject.m_localParam[2].ww * r0.xy;
  r1.xy = float2(6.25, 6.25) * r0.xy;
  r1.xy = r1.xy + r0.zw;
  r1.xyzw = inImage.Sample(g_samplerLinear_Clamp_s, r1.xy).xyzw;
  r1.xyz = float3(0.5, 0.300000012, 0.400000006) * r1.xyz;
  r1.xyzw = float4(0, 0, 0, 0) + r1.xyzw;
  r2.xy = float2(-3.84615374, -3.84615374) * r0.xy;
  r2.xy = r2.xy + r0.zw;
  r2.xyzw = inImage.Sample(g_samplerLinear_Clamp_s, r2.xy).xyzw;
  r2.xyz = float3(0.100000001, 0.400000006, 0.699999988) * r2.xyz;
  r1.xyzw = r2.xyzw + r1.xyzw;
  r2.xy = float2(-1.49253726, -1.49253726) * r0.xy;
  r2.xy = r2.xy + r0.zw;
  r2.xyzw = inImage.Sample(g_samplerLinear_Clamp_s, r2.xy).xyzw;
  r2.xyz = float3(0.100000001, 0.200000003, 0.200000003) * r2.xyz;
  r1.xyzw = r2.xyzw + r1.xyzw;
  r2.xy = float2(-0.9009009, -0.9009009) * r0.xy;
  r2.xy = r2.xy + r0.zw;
  r2.xyzw = inImage.Sample(g_samplerLinear_Clamp_s, r2.xy).xyzw;
  r2.xyz = float3(0.300000012, 0.100000001, 0.100000001) * r2.xyz;
  r1.xyzw = r2.xyzw + r1.xyzw;
  r0.xy = float2(-0.699300706, -0.699300706) * r0.xy;
  r0.xy = r0.xy + r0.zw;
  r0.xyzw = inImage.Sample(g_samplerLinear_Clamp_s, r0.xy).xyzw;
  r0.xyz = float3(0.200000003, 0.200000003, 0.0799999982) * r0.xyz;
  r0.xyzw = r1.xyzw + r0.xyzw;
  r0.xyz = g_psObject.m_localParam[2].xyz * r0.xyz;
  o0 = saturate(r0);
  return;
}
