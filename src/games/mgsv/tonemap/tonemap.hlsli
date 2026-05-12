#include "../common.hlsli"

float3 Sample2DLUT(float3 color, Texture2D<float4> inColorLUT, SamplerState g_samplerLinear_Clamp_s) {
  float4 r0;
  r0.rgb = color;
  float3 r1;

  const float LUT_SIZE = 16.0;
  const float INV_LUT_SIZE = 1.0 / LUT_SIZE; // 0.0625
  const float LUT_BIAS = 0.5 * INV_LUT_SIZE; // 0.03125

  r0.rgb = saturate(r0.rgb);
  r1.xyz = max(float3(0.001953125, 0.001953125, 0.001953125), r0.xyz);
  r1.xyz = rsqrt(r1.xyz);
  r0.xyz = r1.xyz * r0.xyz;
  r0.rgb = saturate(r0.rgb);
  r0.yzw = float3(0.9375, 0.9375, 0.9375) * r0.xyz;
  r0.y = INV_LUT_SIZE * r0.y;
  r1.x = 16 * r0.w;
  r1.x = floor(r1.x);
  r1.x = INV_LUT_SIZE * r1.x;
  r1.y = -r1.x;
  r0.w = r1.y + r0.w;
  r0.w = 16 * r0.w;
  r0.x = r1.x + r0.y;
  r0.xy = float2(0.001953125, LUT_BIAS) + r0.xz;
  r1.xy = float2(INV_LUT_SIZE, 0) + r0.xy;
  r0.xyz = inColorLUT.Sample(g_samplerLinear_Clamp_s, r0.xy).xyz;
  r1.z = -r0.w;
  r1.z = 1 + r1.z;
  r0.xyz = r1.zzz * r0.xyz;
  r1.xyz = inColorLUT.Sample(g_samplerLinear_Clamp_s, r1.xy).xyz;
  r1.xyz = r1.xyz * r0.www;
  r0.xyz = r1.xyz + r0.xyz;

  return r0.rgb;

  // return renodx::lut::Sample(inColorLUT, g_samplerLinear_Clamp_s, color);

}

float ComputeMaxChCompressionScale(float3 untonemapped, float rolloff_start = 0.18f, float output_max = 1.f, float white_clip = 0.f) {
  float peak = renodx::math::Max(untonemapped.r, untonemapped.g, untonemapped.b);

  float mapped_peak;
  if (white_clip == 0.f) {
    mapped_peak = renodx::tonemap::ReinhardPiecewise(peak, output_max, rolloff_start);
  } else {
    mapped_peak = renodx::tonemap::ReinhardPiecewiseExtended(peak, white_clip, output_max, rolloff_start);
  }
  float scale = renodx::math::DivideSafe(mapped_peak, peak, 1.f);

  return scale;
}
