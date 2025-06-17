#include "./shared.h"
Texture2D<float4> t0 : register(t0);

// clang-format off
cbuffer cb0 : register(b0) {
  struct ApplyHdrCodingConstants {
    float2 ApplyHdrCodingConstants_000;
    float2 ApplyHdrCodingConstants_008;
  } g_applyHdrCodingConstants_000 : packoffset(c000.x);
};
// clang-format on

float3 HueAndChrominanceOKLab(
    float3 incorrect_color,
    float3 chrominance_reference_color,
    float3 hue_reference_color,
    float hue_correct_strength = 1.f)
{
  if (hue_correct_strength == 0.f)
    return renodx::color::correct::ChrominanceOKLab(incorrect_color, chrominance_reference_color);

  float3 incorrect_lab = renodx::color::oklab::from::BT709(incorrect_color);
  float3 hue_lab = renodx::color::oklab::from::BT709(hue_reference_color);
  float3 chrominance_lab = renodx::color::oklab::from::BT709(chrominance_reference_color);

  float2 incorrect_ab = incorrect_lab.yz;
  float2 hue_ab = hue_lab.yz;

  // Always use chrominance magnitude from chroma reference
  float target_chrominance = length(chrominance_lab.yz);

  // Compute blended hue direction
  float2 blended_ab_dir = normalize(lerp(normalize(incorrect_ab), normalize(hue_ab), hue_correct_strength));

  // Apply chrominance magnitude from chroma_reference_color
  float2 final_ab = blended_ab_dir * target_chrominance;

  incorrect_lab.yz = final_ab;

  float3 result = renodx::color::bt709::from::OkLab(incorrect_lab);
  return renodx::color::bt709::clamp::AP1(result);
}

// apply gamma correction and hue shift
// g_applyHdrCodingConstants_000.ApplyHdrCodingConstants_000.y = gamma (defaults to 2.4)
float3 GammaCorrectHueShift(float3 incorrect_color) {
  float3 ch = renodx::color::correct::GammaSafe(incorrect_color, false, g_applyHdrCodingConstants_000.ApplyHdrCodingConstants_000.y);

  if (RENODX_GAMMA_CORRECTION_TYPE == 0.f) {
    return renodx::color::correct::HueOKLab(ch, renodx::tonemap::ExponentialRollOff(ch, 1.f, 2.f), RENODX_TONE_MAP_HUE_SHIFT);
  }

  const float y_in = renodx::color::y::from::BT709(incorrect_color);
  const float y_out = max(0, renodx::color::correct::Gamma(y_in, false, g_applyHdrCodingConstants_000.ApplyHdrCodingConstants_000.y));

  float3 lum = incorrect_color * select(y_in > 0, y_out / y_in, 0.f);

  // use chrominance from channel gamma correction and apply hue shifting from per channel tonemap
  float3 result = HueAndChrominanceOKLab(lum, ch, renodx::tonemap::ExponentialRollOff(lum, 1.f, 2.f), RENODX_TONE_MAP_HUE_SHIFT);

  return result;
}

float3 ApplyDisplayMap(float3 untonemapped) {
  untonemapped = max(0, untonemapped);
  if (RENODX_TONE_MAP_TYPE == 0.f) return untonemapped;
  // use log2() as shaper
  float3 tonemapped =
      exp2(renodx::tonemap::ExponentialRollOff(
          log2(untonemapped * RENODX_DIFFUSE_WHITE_NITS),
          log2(RENODX_PEAK_WHITE_NITS * RENODX_TONE_MAP_SHOULDER_START),
          log2(RENODX_PEAK_WHITE_NITS)))
      / RENODX_DIFFUSE_WHITE_NITS;
  return tonemapped;
}

float4 main(
    noperspective float4 SV_Position: SV_Position)
    : SV_Target {
  uint2 pixelCoord = uint2(SV_Position.xy + g_applyHdrCodingConstants_000.ApplyHdrCodingConstants_008);
  float4 gamma_backbuffer = t0.Load(int3(pixelCoord, 0));

  float3 linear_color = renodx::color::srgb::DecodeSafe(gamma_backbuffer.rgb);

  linear_color = GammaCorrectHueShift(linear_color);

  float3 bt2020_color = renodx::color::bt2020::from::BT709(linear_color);

  bt2020_color = ApplyDisplayMap(bt2020_color);

  // originally uses `g_applyHdrCodingConstants_000.ApplyHdrCodingConstants_000.x` for brightness, seemingly always 300.0
  float3 pq_color = renodx::color::pq::Encode(bt2020_color, RENODX_DIFFUSE_WHITE_NITS);

  return float4(pq_color, 1.f);
}
