// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

#ifndef MGSV_BENT_NORMALS_HLSLI_
#define MGSV_BENT_NORMALS_HLSLI_

// Directional first moment for the same cosine-weighted slice measure as
// SliceRelCDFCos. The trace supplies its CDF and AO_PI before this include.
float2 CosineSliceMomentPrimitive(float angle, float cos_normal, float sin_normal) {
  float sine, cosine;
  sincos(angle, sine, cosine);
  const float sine_cubed = sine * sine * sine;
  const float cosine_cubed = cosine * cosine * cosine;
  const float orientation = (angle < 0.0 ? -1.0 : 1.0);
  return orientation / 3.0 * float2(cos_normal * (1.0 - cosine_cubed) - sin_normal * sine_cubed, cos_normal * sine_cubed + sin_normal * (3.0 * cosine - cosine_cubed - 2.0));
}

// Each contiguous blocked run needs only two CDF inversions. Preserve holes
// between runs; reducing the mask to its outer horizons would lose them.
float2 VisibleCosineSliceMoment(uint occupied, float sector_offset, float normal_angle,
                                float cos_normal, out float2 open_moment) {
  const float sin_normal = sin(normal_angle);
  const float normalization = cos_normal + normal_angle * sin_normal;
  open_moment = (CosineSliceMomentPrimitive(AO_PI * 0.5 - normal_angle, cos_normal, sin_normal)
                 - CosineSliceMomentPrimitive(-AO_PI * 0.5 - normal_angle, cos_normal, sin_normal))
                / normalization;
  if (occupied == 0u) return open_moment;
  if (occupied == 0xFFFFFFFFu) return 0.0;

  float2 moment = open_moment;
  uint remaining = occupied;
  [loop]
  while (remaining != 0u) {
    const uint first = (uint)firstbitlow(remaining);
    const uint shifted = remaining >> first;
    const uint length = (shifted == 0xFFFFFFFFu ? 32u : (uint)firstbitlow(~shifted));
    const uint end = first + length;
    const float2 probability = float2(
        first == 0u ? 0.0 : saturate(float(first) / 32.0 - sector_offset),
        end == 32u ? 1.0 : saturate(float(end) / 32.0 - sector_offset));

    float2 lower = -AO_PI * 0.5;
    float2 upper = AO_PI * 0.5;
    [unroll]
    for (uint iteration = 0u; iteration < 10u; ++iteration) {
      const float2 middle = (lower + upper) * 0.5;
      const float2 cdf = float2(
          SliceRelCDFCos(middle.x / AO_PI + 0.5, normal_angle, cos_normal, middle.x > normal_angle),
          SliceRelCDFCos(middle.y / AO_PI + 0.5, normal_angle, cos_normal, middle.y > normal_angle));
      lower = (cdf < probability ? middle : lower);
      upper = (cdf < probability ? upper : middle);
    }
    float2 bounds = (lower + upper) * 0.5;
    bounds = (probability <= 0.0 ? -AO_PI * 0.5 : bounds);
    bounds = (probability >= 1.0 ? AO_PI * 0.5 : bounds);
    moment -= (CosineSliceMomentPrimitive(bounds.y - normal_angle, cos_normal, sin_normal)
               - CosineSliceMomentPrimitive(bounds.x - normal_angle, cos_normal, sin_normal))
              / normalization;
    if (end == 32u) {
      remaining = 0u;
    } else {
      remaining &= 0xFFFFFFFFu << end;
    }
  }
  return moment;
}

#endif  // MGSV_BENT_NORMALS_HLSLI_
