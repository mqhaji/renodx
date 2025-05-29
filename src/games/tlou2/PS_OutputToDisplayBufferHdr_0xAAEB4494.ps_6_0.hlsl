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

float3 ApplyToneMap(float3 untonemapped) {
  if (RENODX_TONE_MAP_TYPE == 0.f) return untonemapped;
  // use log2() as shaper
  float3 tonemapped =
      sign(untonemapped) * exp2(renodx::tonemap::ExponentialRollOff(log2(abs(untonemapped) * RENODX_DIFFUSE_WHITE_NITS), log2(RENODX_PEAK_WHITE_NITS * RENODX_TONE_MAP_SHOULDER_START), log2(RENODX_PEAK_WHITE_NITS)))
      / RENODX_DIFFUSE_WHITE_NITS;

  tonemapped = renodx::color::correct::Hue(tonemapped, untonemapped, RENODX_TONE_MAP_HUE_CORRECTION);
  return tonemapped;
}

float4 main(
    noperspective float4 SV_Position: SV_Position)
    : SV_Target {
  uint2 pixelCoord = uint2(SV_Position.xy + g_applyHdrCodingConstants_000.ApplyHdrCodingConstants_008);
  float4 gamma_backbuffer = t0.Load(int3(pixelCoord, 0));
  float3 linear_color = renodx::color::gamma::DecodeSafe(
      gamma_backbuffer.rgb,
      g_applyHdrCodingConstants_000.ApplyHdrCodingConstants_000.y);  // gamma, defaults to 2.4

  linear_color = ApplyToneMap(linear_color);

  float3 bt2020_color = renodx::color::bt2020::from::BT709(linear_color);

  // originally uses `g_applyHdrCodingConstants_000.ApplyHdrCodingConstants_000.x` for brightness, but it seemingly is always 300.0
  float3 pq_color = renodx::color::pq::EncodeSafe(bt2020_color, RENODX_DIFFUSE_WHITE_NITS);

  return float4(pq_color, 1.f);
}
