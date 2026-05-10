#include "../common.hlsli"

float ComputeReinhardSmoothClampScale(float3 untonemapped, float rolloff_start = 0.18f, float output_max = 1.f, float white_clip = 0.f) {
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