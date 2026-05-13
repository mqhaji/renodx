#include "../common.hlsli"

float3 Sample2DLUT(float3 color, Texture2D<float4> inColorLUT, SamplerState g_samplerLinear_Clamp_s) {
  // if (RENODX_TONE_MAP_TYPE != 0.f) return renodx::lut::SampleTetrahedral(inColorLUT, renodx::math::SqrtSafe(color), 16u);

  float4 r0;
  r0.rgb = color;
  float3 r1;

  const float LUT_SIZE = 16.0;
  const float INV_LUT_SIZE = 1.0 / LUT_SIZE;  // 0.0625
  const float LUT_BIAS = 0.5 * INV_LUT_SIZE;  // 0.03125

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
}

float3 Unclamp(float3 original_gamma, float3 black_gamma, float3 mid_gray_gamma, float3 neutral_gamma) {
  const float3 added_gamma = black_gamma;

  const float mid_gray_average = renodx::math::Average(mid_gray_gamma);

  // Remove from 0 to mid-gray
  const float shadow_length = mid_gray_average;
  const float shadow_stop = renodx::math::Max(neutral_gamma);
  const float3 floor_remove = added_gamma * max(0, shadow_length - shadow_stop) / shadow_length;

  const float3 unclamped_gamma = max(0, original_gamma - floor_remove);
  return unclamped_gamma;
}

float3 Sample2DLUTWithScaling(float3 color, Texture2D<float4> inColorLUT, SamplerState g_samplerLinear_Clamp_s) {
  float3 output_gamma = Sample2DLUT(color, inColorLUT, g_samplerLinear_Clamp_s);

  float3 input_gamma = renodx::math::SqrtSafe(color);

  if (RENODX_COLOR_GRADE_SCALING != 0.f) {
    float3 input_linear = renodx::color::gamma::DecodeSafe(input_gamma);

    float3 lut_black_gamma = Sample2DLUT(0, inColorLUT, g_samplerLinear_Clamp_s);
    float3 lut_black_linear = renodx::color::gamma::DecodeSafe(lut_black_gamma);
    float lut_black_y = max(0, renodx::color::y::from::BT709(lut_black_linear));

    if (lut_black_y > 0.f) {
      float3 lut_mid_gamma = Sample2DLUT(lut_black_y * lut_black_y, inColorLUT, g_samplerLinear_Clamp_s);

      float3 unclamped_gamma = Unclamp(
          output_gamma,
          lut_black_gamma,
          lut_mid_gamma,
          input_gamma);
      float3 unclamped_linear = renodx::color::gamma::DecodeSafe(unclamped_gamma);

      float3 output_linear = renodx::color::gamma::DecodeSafe(output_gamma);

      float3 recolored = output_linear * lerp(1.f, renodx::math::DivideSafe(renodx::color::yf::from::BT709(unclamped_linear), renodx::color::yf::from::BT709(output_linear), 1.f), RENODX_COLOR_GRADE_SCALING * 0.99735);

      recolored = max(0, recolored);
      output_gamma = renodx::color::gamma::EncodeSafe(recolored);
    }
  }

  if (RENODX_COLOR_GRADE_STRENGTH != 1.f) {
    output_gamma = lerp(input_gamma, output_gamma, RENODX_COLOR_GRADE_STRENGTH);
  }

  return output_gamma;
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
