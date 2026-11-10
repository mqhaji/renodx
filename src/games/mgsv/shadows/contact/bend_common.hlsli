// Copyright 2023 Sony Interactive Entertainment.
// SPDX-License-Identifier: Apache-2.0
// Adapted for MGSV and D3D11. See ../BEND_LICENSE.md for the full license.

#ifndef MGSV_BEND_COMMON_HLSLI_
#define MGSV_BEND_COMMON_HLSLI_

#include "../../shared.h"

// Native distance variants share the same light geometry but use a different
// attenuation curve. Keep this classification common to setup and tracing.
bool IsBendPointLight(float light_type) {
  return light_type == 2.0 || light_type == 4.0;
}

bool IsBendDistanceLight(float light_type) {
  return light_type == 3.0 || light_type == 4.0;
}

// The reference sweep and its integer tile arithmetic use 64-thread groups.
// Header/record sizes are bytes and match the host's indirect argument buffer.
#define BEND_GROUP_SIZE          64
#define BEND_MAX_SAMPLES         256
#define BEND_DISPATCH_HEADER     48
#define BEND_DISPATCH_STRIDE     32
#define BEND_TILE_ROW_BITS       15
#define BEND_TILE_ROWS           (1 << BEND_TILE_ROW_BITS)
#define BEND_BATCH_RECORD_STRIDE 400
#define BEND_BATCH_LIGHT_VECTORS 12
#define BEND_ATLAS_OFFSET        368
#define BEND_TRACE_END_OFFSET    380
#define BEND_CLEAR_END_OFFSET    384

// These are the light draw's native buffers, with their original offsets.
// They are bound with the same D3D11 constant-buffer ranges as the pixel shader.
cbuffer BendScene : register(b2) {
  float4x4 bend_projection : packoffset(c4);
#ifdef MGSV_BEND_SETUP
  float4x4 bend_shadow_projection : packoffset(c12);
  float4x4 bend_shadow_cascades : packoffset(c16);
#endif
  float4 bend_projection_param : packoffset(c21);
  float4 bend_viewport : packoffset(c22);
  float4 bend_camera_center : packoffset(c28);
}
#ifdef MGSV_BEND_BATCH
// Header: integer light count and atlas dimensions, stored as raw float bits.
// Each light then has eleven native float4 values and one integer-kind vector.
Buffer<float4> bend_lights : register(t4);
#else
cbuffer BendLight : register(b3) {
  float4 bend_light_params[11] : packoffset(c0);
}
#endif

float4 ReadBendLightParam(uint light_index, uint parameter) {
#ifdef MGSV_BEND_BATCH
  return bend_lights.Load(1u + light_index * BEND_BATCH_LIGHT_VECTORS + parameter);
#else
  return bend_light_params[parameter];
#endif
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

#if defined(MGSV_BEND_BATCH) && !defined(MGSV_BEND_SETUP)
// Each light stores its cumulative work count. Skipped lights repeat the
// preceding count, so binary search jumps over them and finds the owning light.
uint FindBendBatchLight(uint work_index, uint light_count, uint prefix_offset) {
  uint lower = 0u;
  uint upper = light_count;
  [loop]
  while (lower < upper) {
    const uint middle = lower + ((upper - lower) >> 1u);
    if (bend_dispatch.Load(middle * BEND_BATCH_RECORD_STRIDE + prefix_offset) <= work_index) {
      lower = middle + 1u;
    } else {
      upper = middle;
    }
  }
  return lower;
}
#endif

#ifndef MGSV_BEND_SETUP
int2 BendMaskPixel(int2 pixel, int4 bounds, uint record) {
#ifdef MGSV_BEND_BATCH
  const uint2 storage = bend_dispatch.Load2(record + BEND_ATLAS_OFFSET);
  const uint2 local = uint2(pixel - bounds.xy);
  const uint texel_index = storage.x + local.y * uint(bounds.z - bounds.x) + local.x;
  return int2(texel_index % storage.y, texel_index / storage.y);
#else
  return pixel;
#endif
}
#endif

#endif
