// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

#ifndef SRC_GAMES_MGSV_GTVBGI_INDIRECT_DIFFUSE_HLSLI_
#define SRC_GAMES_MGSV_GTVBGI_INDIRECT_DIFFUSE_HLSLI_

// Packing of the GT-VBGI trace result, shared by the trace and application
// passes. One RGBA16F image carries GI in RGB and ambient visibility in alpha.
// Both signals are nonnegative, so a negative R marks missing GI and a negative
// alpha marks missing AO. Black GI and fully blocked AO are valid zeros. The
// two flags are independent and cost no precision in valid values. Readers use
// point loads and normalize each signal over its own valid filter taps.
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
