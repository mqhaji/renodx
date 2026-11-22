// Copyright 2023 Sony Interactive Entertainment.
// SPDX-License-Identifier: Apache-2.0
// Adapted for MGSV and D3D11. See ../BEND_LICENSE.md for the full license.

#ifndef MGSV_BEND_COMMON_HLSLI_
#define MGSV_BEND_COMMON_HLSLI_

#include "../../../shared.h"

// Shared declarations for the Bend Studio screen-space (contact) shadow passes.
// Setup builds one record per light and admits lights into a shared mask
// atlas; clear resets each admitted light's rectangle; trace sweeps rays from
// the projected light through the depth buffer and writes the mask that the
// light shaders read (../screen_space_shadows.hlsli).

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
// Each light's record holds, by byte offset:
//   0   projected light: pixel x and y, depth, and the sign of clip W
//   16  receiver bounds in pixels (min xy, max xy)
//   32  output width and height, state (0 no work, 1 traced, 2 not traced),
//       and the number of sweeps
//   48  up to eight sweep dispatches, BEND_DISPATCH_STRIDE bytes each
//   348 cached PCSS projection (PCSS_PROJECTION_OFFSET)
//   368 first atlas texel and atlas width (a power of two)
//   380 and 384 running totals of trace tiles and atlas texels
// The indirect arguments for clear and trace follow the last record.
#define BEND_GROUP_SIZE          64
// The farthest pixel toward the light a receiver tests, and the casting
// depths one group keeps to reach it (BendDepthSlot in bend_trace). The last
// lane's farthest pixel, 704 + 63, is the last slot's: a longer reach needs
// more slots.
#define BEND_MAX_REACH           704
#define BEND_DEPTH_SLOTS         320
#define BEND_DISPATCH_HEADER     48
#define BEND_DISPATCH_STRIDE     32
#define BEND_TILE_ROW_BITS       15
#define BEND_TILE_ROWS           (1 << BEND_TILE_ROW_BITS)
#define BEND_RECORD_STRIDE       400
// The host's batch::MAX_LIGHTS. Setup holds one shared-memory entry per light.
#define BEND_MAX_LIGHTS          1024u
#define BEND_LIGHT_VECTORS       17
#define BEND_LIGHT_KIND          11
#define BEND_LIGHT_SHADOW        12
#define BEND_LIGHT_FALLOFF_LIMITS 16
#define BEND_ATLAS_OFFSET        368
#define BEND_TRACE_END_OFFSET    380
#define BEND_CLEAR_END_OFFSET    384

// A copy of the light draws' native scene constants, with their original offsets.
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
// Header: integer light count and atlas dimensions, stored as raw float bits.
// Each light then has its eleven native float4 values, a kind vector, the
// four vectors of its shadow-map projection (native b2 c12 to c15), and its
// falloff limits (native b4 m_materials[7]), which the fog's local lights read.
Buffer<float4> bend_lights : register(t4);

float4 ReadBendLightParam(uint light_index, uint parameter) {
  return bend_lights.Load(1u + light_index * BEND_LIGHT_VECTORS + parameter);
}

// x: light type (0 directional, 1/2 spot/point, 3/4 distance spot/point).
// y: 1 when the light draw reads a native shadow map.
// z: the light at this slot's position in draw order, which settles ties in the order lights admit in.
uint3 ReadBendLightKind(uint light_index) {
  return asuint(ReadBendLightParam(light_index, BEND_LIGHT_KIND).xyz);
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
