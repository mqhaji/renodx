// Copyright 2023 Sony Interactive Entertainment.
// SPDX-License-Identifier: Apache-2.0
// Adapted for MGSV and D3D11. See BEND_LICENSE.md for the full license.

#ifndef MGSV_BEND_COMMON_HLSLI_
#define MGSV_BEND_COMMON_HLSLI_

#include "../shared.h"

#define BEND_GROUP_SIZE             64
#define BEND_MAX_SAMPLES            256
#define BEND_DISPATCH_HEADER        48
#define BEND_DISPATCH_STRIDE        32
#define BEND_COVERAGE_WEIGHT_OFFSET (BEND_DISPATCH_HEADER + 8 * BEND_DISPATCH_STRIDE + 12)
#define BEND_SOURCE_INFO_OFFSET     (BEND_DISPATCH_HEADER + 8 * BEND_DISPATCH_STRIDE + 16)
#define BEND_TILE_ROW_BITS          15
#define BEND_TILE_ROWS              (1 << BEND_TILE_ROW_BITS)

// These are the light draw's native buffers, with their original offsets.
// They are bound with the same D3D11 constant-buffer ranges as the pixel shader.
cbuffer BendScene : register(b2) {
  float4x4 bend_projection : packoffset(c4);
  float4 bend_projection_param : packoffset(c21);
  float4 bend_viewport : packoffset(c22);
  float4 bend_camera_center : packoffset(c28);
}
cbuffer BendLight : register(b3) {
  float4 bend_light_params[11] : packoffset(c0);
}

// Test a conservative light sphere against a bundle of camera rays. Used for
// both the whole viewport and individual sweep regions before reading depth.
bool BendSphereOutsideRays(float3 center, float radius, float2 low_pixel, float2 high_pixel, float2 source_size) {
  if (!all(isfinite(bend_projection_param.xy)) || !all(isfinite(bend_camera_center.xy))
      || !all(isfinite(center)) || !isfinite(radius)) return false;
  const float2 first = (low_pixel / source_size * float2(2.0, -2.0) + float2(-1.0, 1.0)
                        - bend_camera_center.xy)
                       * bend_projection_param.xy;
  const float2 last = (high_pixel / source_size * float2(2.0, -2.0) + float2(-1.0, 1.0)
                       - bend_camera_center.xy)
                      * bend_projection_param.xy;
  const float2 ray_min = min(first, last);
  const float2 ray_max = max(first, last);
  const float4 planes = float4(center.x - ray_min.x * center.z, ray_max.x * center.z - center.x,
                               center.y - ray_min.y * center.z, ray_max.y * center.z - center.y);
  const float4 slopes = float4(ray_min.x, ray_max.x, ray_min.y, ray_max.y);
  const float padded_radius = radius + (length(center) + radius) * 0.00002;
  return any(planes < -padded_radius * sqrt(1.0 + slopes * slopes));
}

#ifdef MGSV_BEND_SETUP
RWByteAddressBuffer bend_dispatch : register(u0);
RWByteAddressBuffer bend_spot_budget : register(u2);
#else
ByteAddressBuffer bend_dispatch : register(t3);
#endif
RWTexture2D<float4> bend_output : register(u1);

#endif
