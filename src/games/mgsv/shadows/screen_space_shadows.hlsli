// SPDX-License-Identifier: MIT
#ifndef MGSV_SCREEN_SPACE_SHADOWS_HLSLI_
#define MGSV_SCREEN_SPACE_SHADOWS_HLSLI_

#include "../shared.h"

Texture2D<float4> bend_shadow_mask : register(t18);
ByteAddressBuffer bend_shadow_data : register(t19);

// Apply only to this light's direct contributions. The caller keeps environment
// reflection RGB separate, so contact shadows cannot darken it accidentally.
void ApplyBendShadow(float2 pixel, uint light_type, Texture2D<float4> depth_texture,
                     float2 projection_param, inout float3 diffuse, inout float3 specular) {
  const bool directional = light_type == 0;
  const bool spot = light_type == 1;
  const float enabled = directional ? CUSTOM_SS_SHADOWS : (spot ? CUSTOM_SS_SPOT_SHADOWS : CUSTOM_SS_POINT_SHADOWS);
  if (enabled < 0.5) return;
  const uint debug = (uint)CUSTOM_SS_DEBUG_VIEW;
  // The bypass view keeps the same compute work for performance comparisons.
  if (debug == 4) return;
  const bool diagnostic = ((debug >= 1 && debug <= 3) || debug == 5) && light_type == (uint)CUSTOM_SS_DEBUG_LIGHT;
  const uint4 metadata = bend_shadow_data.Load4(32);
  // Quality limits can omit this light's clear and trace completely. Its mask
  // still contains another light's pixels, so never read it in this state.
  if (metadata.z == 2) {
    if (diagnostic) {
      diffuse = debug == 1 ? 1.0 : 0.0;
      specular = 0.0;
    }
    return;
  }
  const uint4 bounds = bend_shadow_data.Load4(16);
  const uint2 position = uint2(pixel);
  bool supported = metadata.z == 1 && all(position >= bounds.xy) && all(position < bounds.zw);
  float2 sample = float2(1.0, 0.0);
  // The setup pass records the mode beside the clear dispatch: 0 = Full,
  // 1 = Checkerboard, 2 = 1/4. These are also the menu's sampling values.
  const uint sampling_mode = bend_shadow_data.Load(48 + 8 * 32 + 28);
  if (metadata.z == 1 && sampling_mode == 2u) {
    const float raw_depth = depth_texture.Load(int3(position, 0)).x;
    const float tolerance = max(abs(raw_depth - projection_param.y) * 0.02, 1e-7);
    const float2 coordinate = pixel * 0.5 - 0.5;
    const int2 base = int2(floor(coordinate));
    const float2 fraction = frac(coordinate);
    float2 sum = 0.0;
    float weight_sum = 0.0;
    [unroll]
    for (uint tap = 0; tap < 4u; ++tap) {
      const int2 offset = int2(tap & 1u, tap >> 1u);
      const int2 target = base + offset;
      if (any(target < int2(bounds.xy)) || any(target >= int2(bounds.zw))) continue;
      const float4 value = bend_shadow_mask.Load(int3(target, 0));
      if (value.y < 0.24 || !all(isfinite(value.xyz))) continue;
      const float2 spatial = float2(offset.x != 0 ? fraction.x : 1.0 - fraction.x,
                                    offset.y != 0 ? fraction.y : 1.0 - fraction.y);
      const float weight = spatial.x * spatial.y * saturate(1.0 - abs(value.z - raw_depth) / tolerance);
      sum += value.xy * weight;
      weight_sum += weight;
    }
    supported = weight_sum > 1e-6;
    if (supported) {
      sample = sum / weight_sum;
    }
  } else if (supported) {
    sample = bend_shadow_mask.Load(int3(position, 0)).xy;
    if (sampling_mode == 1u && ((position.x + position.y) & 1u) != 0u && sample.y < 0.24) {
      const float raw_depth = depth_texture.Load(int3(position, 0)).x;
      const float tolerance = max(abs(raw_depth - projection_param.y) * 0.02, 1e-7);
      float2 sum = 0.0;
      float weight_sum = 0.0;
      // The four cardinal neighbors belong to the traced half of the grid.
      // Reject depths across a surface edge instead of spreading its shadow.
      [unroll]
      for (uint tap = 0; tap < 4u; ++tap) {
        const int2 offset = tap < 2u ? int2(tap == 0u ? -1 : 1, 0) : int2(0, tap == 2u ? -1 : 1);
        const int2 target = int2(position) + offset;
        if (any(target < int2(bounds.xy)) || any(target >= int2(bounds.zw))) continue;
        const float2 value = bend_shadow_mask.Load(int3(target, 0)).xy;
        if (value.y < 0.24 || !all(isfinite(value))) continue;
        const float neighbor_depth = depth_texture.Load(int3(target, 0)).x;
        const float weight = saturate(1.0 - abs(neighbor_depth - raw_depth) / tolerance);
        sum += value * weight;
        weight_sum += weight;
      }
      supported = weight_sum > 1e-6;
      if (supported) {
        sample = sum / weight_sum;
      }
    }
  }
  // Whole unlit regions can retain their neutral clear value without a trace.
  if (!directional && metadata.z == 1 && CUSTOM_SS_LOCAL_CULLING >= 0.5 && (!supported || sample.y < 0.24)) {
    supported = true;
    sample = float2(1.0, 0.25);
  }
  const bool valid = supported && sample.y >= 0.24 && all(isfinite(sample));
  if (diagnostic) {
    diffuse = valid ? (debug == 1 ? sample.xxx : saturate((sample.y - 0.25) / 0.75).xxx)
                    : (debug == 5 && metadata.z == 1 ? 0.0 : float3(1.0, 0.0, 1.0));
    specular = 0.0;
    return;
  }
  if (!valid) return;
  diffuse *= sample.x;
  specular *= sample.x;
}

#endif
