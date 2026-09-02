#include "../common.hlsli"
#include "./customtest30.hlsli"
#include "./uncharted2extended.hlsli"

#define LMS_D65_WHITE renodx::color::lms::from::BT709(1.f)

float3 ApplyUnchartedFilmicTonemap(float3 untonemapped, float A, float B, float C, float D, float E, float F, float W) {
  if (RENODX_TONE_MAP_TYPE != 0.f) {
    float coeffs[6] = { A, B, C, D, E, F };
    float white_precompute = 1.f / renodx::tonemap::ApplyCurve(W, A, B, C, D, E, F);
    Uncharted2::Config::Uncharted2ExtendedConfig uc2_config = Uncharted2::Config::CreateUncharted2ExtendedConfig(coeffs, white_precompute);
    return Uncharted2::ApplyExtended(untonemapped, uc2_config);
  }

  return renodx::tonemap::ApplyCurve(untonemapped, A, B, C, D, E, F)
         / renodx::tonemap::ApplyCurve(W, A, B, C, D, E, F);
}

float3 ApplyTppTonemap(float3 untonemapped, float3 params) {
  if (RENODX_TONE_MAP_TYPE != 0.f) return untonemapped;

  float shoulder_start = params.y, shoulder_offset = params.z, shoulder_scale = params.x;

  float3 linear_weight = renodx::math::Select(untonemapped <= shoulder_start, 1.f, 0.f);
  float3 shoulder_curve = shoulder_start + shoulder_offset - (1.f / (shoulder_scale * (untonemapped - shoulder_start + shoulder_offset)));

  return untonemapped * linear_weight + shoulder_curve * (1.f - linear_weight);
}

/// Identity through anchor to every derivative; then approaches peak
/// monotonically and concave down. Requires anchor < peak and compression_strength >= 1.
#define APPLYANCHORED_CINFINITY_SHOULDER_GENERATOR(T)                                                      \
  T ApplyAnchoredCInfinityShoulder(T color, T peak, T anchor, float compression_strength) {                \
    T shoulder_range = peak - anchor;                                                                      \
    T distance_from_anchor = max(color - anchor, (T)0.f);                                                  \
    T flat_weight = exp2(-shoulder_range / (compression_strength * distance_from_anchor));                 \
    T response_denominator = mad(distance_from_anchor, flat_weight, shoulder_range);                       \
    return mad(shoulder_range, distance_from_anchor / response_denominator, color - distance_from_anchor); \
  }

APPLYANCHORED_CINFINITY_SHOULDER_GENERATOR(float)
APPLYANCHORED_CINFINITY_SHOULDER_GENERATOR(float3)
#undef APPLYANCHORED_CINFINITY_SHOULDER_GENERATOR

float ComputeAnchoredCInfinityMaxChannelScale(
    float3 color,
    float peak,
    float anchor,
    float compression_strength) {
  const float max_channel = renodx::math::Max(color);
  const float compressed_max = ApplyAnchoredCInfinityShoulder(max_channel, peak, anchor, compression_strength);
  return renodx::math::DivideSafe(compressed_max, max_channel, 1.f);
}

float3 ApplyFinalTonemap(float3 untonemapped) {
  float3 r0 = untonemapped;

  r0.rgb = renodx::color::srgb::DecodeSafe(r0.rgb);

  if (RENODX_TONE_MAP_TYPE != 0.f) {
    r0.rgb = renodx::tonemap::psychov::psychotm_custom_test30(
        r0.rgb, RENODX_PEAK_WHITE_NITS / RENODX_DIFFUSE_WHITE_NITS, RENODX_TONE_MAP_EXPOSURE, RENODX_TONE_MAP_HIGHLIGHTS, RENODX_TONE_MAP_SHADOWS,
        RENODX_TONE_MAP_CONTRAST, 0.10f * pow(RENODX_TONE_MAP_FLARE, 10.f), RENODX_TONE_MAP_CONTRAST_HIGHLIGHTS, RENODX_TONE_MAP_CONTRAST_SHADOWS,
        RENODX_TONE_MAP_SATURATION, RENODX_TONE_MAP_HIGHLIGHT_SATURATION, RENODX_TONE_MAP_DECHROMA,
        0.1f, 0.1f, RENODX_GAMMA_CORRECTION, 1.f, renodx::tonemap::psychov::PSYCHO30_TARGET_GAMUT_BT2020, 1.5f);

  } else {
    if (RENODX_GAMMA_CORRECTION != 0.f) {
      r0.rgb = renodx::color::correct::GammaSafe(r0.rgb);
    }
    r0.rgb = saturate(r0.rgb);
  }

  r0.rgb *= RENODX_DIFFUSE_WHITE_NITS / RENODX_GRAPHICS_WHITE_NITS;

  if (RENODX_GAMMA_CORRECTION != 0.f) {
    r0.rgb = renodx::color::gamma::EncodeSafe(r0.rgb);
  } else {
    r0.rgb = renodx::color::srgb::EncodeSafe(r0.rgb);
  }

  return r0.rgb;
}

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
    float3 input_linear = renodx::color::srgb::DecodeSafe(input_gamma);

    float3 lut_black_gamma = Sample2DLUT(0, inColorLUT, g_samplerLinear_Clamp_s);
    float3 lut_black_linear = renodx::color::srgb::DecodeSafe(lut_black_gamma);
    float lut_black_y = max(0, renodx::color::yf::from::BT709(lut_black_linear));

    if (lut_black_y > 0.f) {
      float3 lut_mid_gamma = Sample2DLUT(lut_black_y * lut_black_y, inColorLUT, g_samplerLinear_Clamp_s);

      float3 unclamped_gamma = Unclamp(
          output_gamma,
          lut_black_gamma,
          lut_mid_gamma,
          input_gamma);
      float3 unclamped_linear = renodx::color::srgb::DecodeSafe(unclamped_gamma);

      float3 output_linear = renodx::color::srgb::DecodeSafe(output_gamma);

      float3 recolored = output_linear * lerp(1.f, renodx::math::DivideSafe(renodx::color::yf::from::BT709(unclamped_linear), renodx::color::yf::from::BT709(output_linear), 1.f), RENODX_COLOR_GRADE_SCALING * 0.95);

      recolored = max(0, recolored);
      output_gamma = renodx::color::srgb::EncodeSafe(recolored);
    }
  }

  if (RENODX_COLOR_GRADE_STRENGTH != 1.f) {
    output_gamma = lerp(input_gamma, output_gamma, RENODX_COLOR_GRADE_STRENGTH);
  }

  return output_gamma;
}
