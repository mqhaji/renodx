#include "../common.hlsli"
#include "./customtest31.hlsli"
#include "./uncharted2extended.hlsli"

float3 ApplyUnchartedFilmicTonemap(float3 untonemapped, float A, float B, float C, float D, float E, float F, float W) {
  [branch]
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
  [branch]
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

  [branch]
  if (RENODX_TONE_MAP_TYPE != 0.f) {
    r0.rgb = renodx::tonemap::psychov::custom_psychotm_test31(
        r0.rgb, RENODX_PEAK_WHITE_NITS / RENODX_DIFFUSE_WHITE_NITS, RENODX_TONE_MAP_EXPOSURE, RENODX_TONE_MAP_HIGHLIGHTS, RENODX_TONE_MAP_SHADOWS,
        RENODX_TONE_MAP_CONTRAST, 0.10f * pow(RENODX_TONE_MAP_FLARE, 10.f), RENODX_TONE_MAP_CONTRAST_HIGHLIGHTS, RENODX_TONE_MAP_CONTRAST_SHADOWS,
        RENODX_TONE_MAP_SATURATION, RENODX_TONE_MAP_HIGHLIGHT_SATURATION, RENODX_TONE_MAP_DECHROMA,
        0.1f, 0.1f, RENODX_GAMMA_CORRECTION, 1.f,
        RENODX_SWAP_CHAIN_OUTPUT_PRESET == renodx::draw::SWAP_CHAIN_OUTPUT_PRESET_SDR
            ? renodx::tonemap::psychov::CUSTOM_PSYCHO31_TARGET_GAMUT_BT709
            : renodx::tonemap::psychov::CUSTOM_PSYCHO31_TARGET_GAMUT_BT2020,
        1.5f, 1.f, renodx::tonemap::psychov::PSYCHO30_SOURCE_BOUNDARY_NONE, 0.f);

  } else {
    [branch]
    if (RENODX_GAMMA_CORRECTION != 0.f) {
      r0.rgb = renodx::color::correct::GammaSafe(r0.rgb);
    }
    r0.rgb = saturate(r0.rgb);
  }

  r0.rgb *= RENODX_DIFFUSE_WHITE_NITS / RENODX_GRAPHICS_WHITE_NITS;

  [branch]
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

float3 CompensateGradingZeroInputOffset(
    float3 graded,
    float3 source,
    float3 grading_zero_output,
    float half_weight_stops) {
  // Split the grading output at zero into a shared RGB offset and its unequal-channel residual.
  const float common_offset = max(renodx::math::Min(grading_zero_output), 0.f);
  const float3 channel_residual = grading_zero_output - common_offset;

  // Express the relative linear-light RMS source level in units of the chosen half-weight level.
  const float source_magnitude = sqrt(dot(source, source) / 3.f);
  const float source_to_common_offset = renodx::math::DivideSafe(source_magnitude, common_offset, 0.f);
  const float source_half_weight_units = source_to_common_offset * exp2(half_weight_stops);
  // Approximate exp2(-x), matching x = 0, 1, and 2 exactly at weights 1, 1/2, and 1/4.
  const float source_release_denominator = 1.f + 0.5f * source_half_weight_units * (1.f + source_half_weight_units);

  // Reduce compensation when the zero-input output contains unequal-channel structure; never subtract that residual.
  const float common_offset_squared_magnitude = 3.f * common_offset * common_offset;
  // Combine source release and the relative squared RGB residual weight into one division.
  const float compensation_weight = renodx::math::DivideSafe(
      common_offset_squared_magnitude,
      source_release_denominator
          * (common_offset_squared_magnitude + dot(channel_residual, channel_residual)),
      0.f);

  // Subtract one bounded scalar, preserving channel differences and keeping every channel at or above its source.
  const float removable_common_offset = max(renodx::math::Min(graded - source), 0.f);
  const float offset_compensation = min(common_offset * compensation_weight, removable_common_offset);

  return graded - offset_compensation;
}

float3 Sample2DLUTWithScaling(float3 color, Texture2D<float4> inColorLUT, SamplerState g_samplerLinear_Clamp_s) {
  float3 output_gamma = Sample2DLUT(color, inColorLUT, g_samplerLinear_Clamp_s);

  float3 input_gamma = renodx::math::SqrtSafe(color);

  [branch]
  if (RENODX_COLOR_GRADE_SCALING != 0.f) {
    float3 lut_black_gamma = Sample2DLUT(0, inColorLUT, g_samplerLinear_Clamp_s);
    float3 lut_black_linear = renodx::color::srgb::DecodeSafe(lut_black_gamma);

    [branch]
    if (renodx::math::Min(lut_black_linear) > 0.f) {
      float3 input_linear = renodx::color::srgb::DecodeSafe(input_gamma);
      float3 output_linear = renodx::color::srgb::DecodeSafe(output_gamma);

      output_linear = lerp(
          output_linear,
          CompensateGradingZeroInputOffset(output_linear, input_linear, lut_black_linear, 1.f),
          RENODX_COLOR_GRADE_SCALING);
      output_gamma = renodx::color::srgb::EncodeSafe(output_linear);
    }
  }

  if (RENODX_COLOR_GRADE_STRENGTH != 1.f) {
    output_gamma = lerp(input_gamma, output_gamma, RENODX_COLOR_GRADE_STRENGTH);
  }

  return output_gamma;
}
