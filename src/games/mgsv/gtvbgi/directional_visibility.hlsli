// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

#ifndef MGSV_DIRECTIONAL_VISIBILITY_HLSLI_
#define MGSV_DIRECTIONAL_VISIBILITY_HLSLI_

// RGB is an unnormalized cosine-weighted view-space moment. Alpha is raw
// visibility; -1 marks missing data, while zero is a valid closed hemisphere.
bool ValidDirectionalVisibility(float4 value) {
  return all(isfinite(value)) && value.a >= 0.0 && value.a <= 1.0;
}

float3 BentReceiverNormal(float3 normal, float3 position) {
  const float3 view = normalize(-position);
  return normal - 2.0 * min(dot(normal, view), 0.0) * view;
}

float4 BoundDirectionalVisibility(float4 value, float3 normal) {
  value.a = saturate(value.a);
  value.rgb += normal * max(-dot(value.rgb, normal), 0.0);
  const float length_squared = dot(value.rgb, value.rgb);
  if (length_squared > value.a * value.a) {
    value.rgb *= value.a * rsqrt(length_squared);
  }
  return value;
}

float3 BentNormalDirection(float4 value, float3 fallback_normal) {
  const float length_squared = dot(value.rgb, value.rgb);
  return length_squared > 1e-10 ? value.rgb * rsqrt(length_squared) : fallback_normal;
}

#endif  // MGSV_DIRECTIONAL_VISIBILITY_HLSLI_
