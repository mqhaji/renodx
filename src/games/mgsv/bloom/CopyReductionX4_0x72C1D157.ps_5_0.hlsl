#include "./bloom_native.hlsli"

void main(
    float4 v0: SV_Position0,
    float2 v1: TEXCOORD0,
    out float4 o0: SV_Target0) {
  if (CUSTOM_BLOOM_TYPE >= 0.5f) {
    uint width, height;
    inImage.GetDimensions(width, height);
    const bool scene_input = all(g_psObject.m_localParam[3].xy == 0.f);
    const uint2 work_size = (scene_input ? uint2(width, height) / 4u : min(uint2(width, height), BLOOM_WORK_SIZE));
    const float2 scale = float2(width, height) / float2(work_size);
    float4 sum = 0.f;
    float weight_sum = 0.f;
    [branch]  // Only the first reduction reads encoded scene color.
    if (scene_input) {
      const float2 begin = floor(v0.xy) * scale;
      const float2 end = (floor(v0.xy) + 1.f) * scale;
      [loop]  // Include fractional odd-resolution edges.
      for (int y = int(floor(begin.y)); y < int(ceil(end.y)); ++y) {
        const float row_weight = max(0.f, min(end.y, float(y + 1)) - max(begin.y, float(y)));
        [loop]  // The footprint width is known only at runtime.
        for (int x = int(floor(begin.x)); x < int(ceil(end.x)); ++x) {
          const float column_weight = max(0.f, min(end.x, float(x + 1)) - max(begin.x, float(x)));
          const float weight = column_weight * row_weight;
          float4 color = inImage.Load(int3(x, y, 0));
          color.rgb = renodx::color::srgb::DecodeSafe(color.rgb);
          sum += color * weight;
          weight_sum += weight;
        }
      }
      o0 = sum / weight_sum;
      return;
    }

    // Share moving lights smoothly across the working grid in linear light.
    const float2 center = (floor(v0.xy) + 0.5f) * scale;
    const float2 begin = center - 2.f * scale;
    const float2 end = center + 2.f * scale;
    [loop]  // Keep the variable-size filter from being unrolled.
    for (int y = int(floor(begin.y)); y < int(ceil(end.y)); ++y) {
      // All columns in this row share the same vertical filter weight.
      const float row_position = (float(y) - center.y) / scale.y;
      const float row_weight = max(0.f, BloomCubicIntegral(row_position + rcp(scale.y)) - BloomCubicIntegral(row_position));
      [loop]  // The footprint can span many source columns.
      for (int x = int(floor(begin.x)); x < int(ceil(end.x)); ++x) {
        const float column_position = (float(x) - center.x) / scale.x;
        const float column_weight = max(0.f, BloomCubicIntegral(column_position + rcp(scale.x)) - BloomCubicIntegral(column_position));
        const float weight = column_weight * row_weight;
        sum += inImage.Load(int3(clamp(int2(x, y), 0, int2(width, height) - 1), 0)) * weight;
        weight_sum += weight;
      }
    }
    o0 = sum / weight_sum;
    return;
  }

  float4 r0, r1, r2;

  r0.xy = v1.xy;
  r0.z = -r0.y;
  r0.xy = float2(0.5, 0.5) * r0.xz;
  r0.xy = float2(0.5, 0.5) + r0.xy;
  r0.zw = g_psObject.m_localParam[3].xy;
  r1.xy = g_psObject.m_localParam[3].zw;
  r0.xy = r1.xy * r0.xy;
  r0.xy = r0.zw + r0.xy;
  // The decompiler lost the four offsets; the DXBC uses (-1,-1), (1,-1), (-1,1), (1,1).
  r1.xyzw = inImage.Sample(g_samplerLinear_Clamp_s, r0.xy, int2(-1, -1)).xyzw;
  r2.xyzw = inImage.Sample(g_samplerLinear_Clamp_s, r0.xy, int2(1, -1)).xyzw;
  r1.xyzw = r2.xyzw + r1.xyzw;
  r2.xyzw = inImage.Sample(g_samplerLinear_Clamp_s, r0.xy, int2(-1, 1)).xyzw;
  r1.xyzw = r2.xyzw + r1.xyzw;
  r0.xyzw = inImage.Sample(g_samplerLinear_Clamp_s, r0.xy, int2(1, 1)).xyzw;
  r0.xyzw = r1.xyzw + r0.xyzw;
  r0.xyzw = r0.xyzw / float4(4, 4, 4, 4);
  o0.xyzw = saturate(r0);
  return;
}
