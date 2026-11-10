// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

#ifndef SRC_GAMES_MGSV_GTVBGI_INDIRECT_DIFFUSE_HLSLI_
#define SRC_GAMES_MGSV_GTVBGI_INDIRECT_DIFFUSE_HLSLI_

// One RGBA16F image carries GI in RGB and ambient visibility in alpha. Both
// signals are nonnegative. A negative R marks missing GI; a negative alpha
// marks missing AO. Valid black GI and fully blocked AO remain valid zeros.
// The flags are independent, and neither consumes precision from a valid value.
// Use point loads, then normalize each signal over its own valid filter taps.
bool HasDiffuseGI(float4 sample) {
  return sample.r >= 0.0 && all(isfinite(sample.rgb));
}

bool HasAmbientVisibility(float4 sample) {
  return sample.a >= 0.0 && isfinite(sample.a);
}

float4 PackIndirectDiffuse(float3 gi, bool gi_valid, float visibility, bool ao_valid) {
  return float4(gi_valid ? gi : float3(-1.0, 0.0, 0.0), ao_valid ? visibility : -1.0);
}

#endif  // SRC_GAMES_MGSV_GTVBGI_INDIRECT_DIFFUSE_HLSLI_
