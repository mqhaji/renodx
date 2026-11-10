// Copyright 2023 Sony Interactive Entertainment.
// SPDX-License-Identifier: Apache-2.0
// GPU adaptation of Bend's BuildDispatchList. See ../BEND_LICENSE.md.

#define MGSV_BEND_SETUP
#include "bend_common.hlsli"
#include "../pcss.hlsli"

struct BendDispatch {
  int3 groups;
  int2 offset;
};

[numthreads(1, 1, 1)]
void main() {
  uint width, height;
  bend_output.GetDimensions(width, height);
  // Start with no supported contact output. Every failed qualification leaves
  // neutral metadata, while PCSS can still use the cached native projection.
  bend_dispatch.Store4(0, 0u);
  bend_dispatch.Store4(16, 0u);
  bend_dispatch.Store4(32, uint4(width, height, 0, 0));
  // Only the two actual indirect calls need empty arguments on early returns.
  // Active sweeps are overwritten below; the published count limits reads.
  bend_dispatch.Store3(BEND_DISPATCH_HEADER + 8 * BEND_DISPATCH_STRIDE, 0u);
  bend_dispatch.Store3(BEND_DISPATCH_HEADER + 9 * BEND_DISPATCH_STRIDE, 0u);
  const bool directional = CUSTOM_SS_LIGHT_TYPE < 0.5;
  // Keep PCSS available even when this light skips contact tracing. Point
  // shadows use their native radial projection and need no cached matrix data.
  PcssProjection shadow_projection = (PcssProjection)0;
  if (!BEND_POINT_LIGHT && CUSTOM_SHADOW_SOFTNESS > 0.0) {
    shadow_projection = CalculatePcssProjection(bend_shadow_projection, bend_shadow_cascades, directional);
  }
  bend_dispatch.Store4(PCSS_PROJECTION_OFFSET, asuint(float4(shadow_projection.uv_scale, shadow_projection.depth_scale, shadow_projection.depth_a)));
  bend_dispatch.Store(PCSS_PROJECTION_OFFSET + 16u, asuint(shadow_projection.depth_b));
  const uint2 source_size = uint2(bend_viewport.xy + 0.5);
  if (width == 0 || height == 0 || any(source_size == 0)
      || any(source_size != uint2(width, height))) return;
  const uint spot_budget = !directional && !BEND_POINT_LIGHT
                                   && isfinite(CUSTOM_SS_SPOT_BUDGET) && CUSTOM_SS_SPOT_BUDGET >= 1.0
                               ? uint(min(CUSTOM_SS_SPOT_BUDGET, 2147483520.0))
                               : 0u;
  // Once the budget is full, later lights need no projection or sweep setup.
  // Empty indirect arguments and state 2 keep their original game shadows.
  if (spot_budget != 0u && bend_spot_budget.Load(0) >= spot_budget) {
    bend_dispatch.Store(40, 2u);
    return;
  }
  // Native directional shading uses N dot L, so this points toward the source.
  // Local positions are already in view space. Keep the native center for bounds.
  const float4 position = directional ? float4(bend_light_params[0].xyz, 0.0)
                                      : float4(bend_light_params[3].xyz, 1.0);
  if (!all(isfinite(position)) || dot(position.xyz, position.xyz) < 1e-12) return;
  // A common source offset moves whole shadow edges together. Keep the sweep
  // centered and resolve its finite-source footprint in the light shader.
  const float4 projected = mul(position, bend_projection);
  if (!all(isfinite(projected))) return;
  // Use the same bounded divisor and sign for every projected coordinate.
  // At exactly W=0, mixing a positive XY divisor with a negative sweep sign
  // reverses the ray. Local lights also need their finite clip Z preserved.
  const float xy_divisor = (projected.w < 0.0 ? -1.0 : 1.0) * max(abs(projected.w), 0.000002 * BEND_GROUP_SIZE);
  const float2 projection_size = float2(source_size);
  const float4 light = float4((projected.xy / xy_divisor * float2(0.5, -0.5) + 0.5) * projection_size,
                              projected.z / xy_divisor,
                              projected.w >= 0.0 ? 1.0 : -1.0);
  // Leave room for signed tile arithmetic when a local light projects very
  // far away near the camera plane. Unsupported projections keep native light.
  if (!all(isfinite(light)) || any(abs(light.xy) > 1073741824.0)) return;

  int2 minimum = 0;
  int2 maximum = int2(width, height);
  // Native attenuation gives a finite sphere. For spotlights, keep only the
  // part inside the native light cone before projecting the receiver bounds.
  const bool distance_light = BEND_DISTANCE_LIGHT;
  const float attenuation = distance_light ? bend_light_params[6].z : bend_light_params[6].w;
  // The distance variant caps d^16 at FLT_MAX. Tiny/subnormal coefficients
  // need not reach zero after that cap, so do not infer a finite range there.
  const bool finite_cutoff = attenuation > 0.0 && (!distance_light || attenuation >= 1.17549435e-38);
  if (!directional && isfinite(attenuation) && finite_cutoff) {
    // Distance lights cut off at 1-k*d^16 = 0. Other local lights use
    // 1/d^2-k*d^2 = 0. Both point variants can offset that distance.
    const float radius = (distance_light ? rsqrt(sqrt(sqrt(sqrt(attenuation)))) : rsqrt(sqrt(attenuation)))
                         + (BEND_POINT_LIGHT ? abs(bend_light_params[6].x) : 0.0);
    if (BendSphereOutsideRays(position.xyz, radius, 0.0, float2(source_size), float2(source_size))) {
      bend_dispatch.Store(40, 2u);
      return;
    }
    float3 lower_corner = position.xyz - radius;
    float3 upper_corner = position.xyz + radius;
    const float3 cone = bend_light_params[7].xyz;
    const float3 axis = bend_light_params[5].xyz;
    const float axis_length = length(axis);
    if (!BEND_POINT_LIGHT && isfinite(radius) && all(isfinite(cone))
        && cone.y > 0.0 && cone.z > 0.0 && isfinite(axis_length) && axis_length > 1e-6) {
      float cosine = cone.x / axis_length;
      if (cosine > 0.0 && cosine < 1.0) {
        const float3 direction = axis / axis_length;
        // Widen the angular limit slightly so rounding cannot trim a lit edge.
        cosine = max(0.0, cosine - 0.00002);
        const float3 side = sqrt(saturate(1.0 - direction * direction) * (1.0 - cosine * cosine));
        // These are the axis limits of the cone inside the range sphere. They
        // limit receivers only; the trace still reads casting depth beyond them.
        const float3 lower = direction <= -cosine ? -1.0 : min(0.0, direction * cosine - side);
        const float3 upper = direction >= cosine ? 1.0 : max(0.0, direction * cosine + side);
        const float3 padding = (abs(position.xyz) + radius) * 0.000002;
        lower_corner = position.xyz + lower * radius - padding;
        upper_corner = position.xyz + upper * radius + padding;
      }
    }
    if (isfinite(radius) && upper_corner.z <= 0.0) return;
    // A bound crossing the eye plane still needs the full viewport.
    if (isfinite(radius) && lower_corner.z > 0.0) {
      // Project all eight corners of the conservative receiver box. One pixel
      // of padding protects lit edges from integer rounding during the clear.
      float2 lower = float2(width, height);
      float2 upper = 0.0;
      bool bounded = true;
      [unroll]
      for (uint corner = 0; corner < 8; ++corner) {
        const float3 corner_position = float3((corner & 1) != 0 ? upper_corner.x : lower_corner.x,
                                              (corner & 2) != 0 ? upper_corner.y : lower_corner.y,
                                              (corner & 4) != 0 ? upper_corner.z : lower_corner.z);
        const float4 clip = mul(float4(corner_position, 1.0), bend_projection);
        bounded = bounded && clip.w > 1e-6 && all(isfinite(clip));
        const float2 pixel = (clip.xy / max(clip.w, 1e-6) * float2(0.5, -0.5) + 0.5) * projection_size;
        lower = min(lower, pixel);
        upper = max(upper, pixel);
      }
      if (bounded) {
        minimum = clamp(int2(floor(lower)) - 1, 0, int2(width, height));
        maximum = clamp(int2(ceil(upper)) + 1, 0, int2(width, height));
      }
    }
  }
  if (any(maximum <= minimum)) return;

  BendDispatch dispatches[8];
  uint count = 0;
  const int2 light_pixel = int2(light.xy + 0.5);
  const int4 bounds = int4(minimum.x - light_pixel.x, -(maximum.y - light_pixel.y),
                           maximum.x - light_pixel.x, -(minimum.y - light_pixel.y));
  // Divide the viewport around the projected light. A quadrant can need two
  // sweeps where the shared ray changes its major axis, for at most eight records.
  [unroll]
  for (int quadrant = 0; quadrant < 4; ++quadrant) {
    const bool vertical = quadrant == 0 || quadrant == 3;
    const int4 tiles = int4(
        max(0, (quadrant & 1) != 0 ? bounds.x : -bounds.z) >> 6,
        max(0, (quadrant & 2) != 0 ? bounds.y : -bounds.w) >> 6,
        max(0, ((quadrant & 1) != 0 ? bounds.z : -bounds.x) + BEND_GROUP_SIZE * (vertical ? 1 : 2) - 1) >> 6,
        max(0, ((quadrant & 2) != 0 ? bounds.w : -bounds.y) + BEND_GROUP_SIZE * (vertical ? 2 : 1) - 1) >> 6);
    if (tiles.z <= tiles.x || tiles.w <= tiles.y) continue;
    const uint first = count++;
    dispatches[first].groups = int3(BEND_GROUP_SIZE, tiles.z - tiles.x, tiles.w - tiles.y);
    dispatches[first].offset = int2(((quadrant & 1) != 0 ? tiles.x : -tiles.z) + (quadrant == 2 || quadrant == 3 ? 1 : 0),
                                    ((quadrant & 2) != 0 ? -tiles.w : tiles.y) + (quadrant == 1 || quadrant == 3 ? 1 : 0));
    int split = bounds.x - bounds.y;
    if (quadrant == 1) {
      split = bounds.z + bounds.y;
    }
    if (quadrant == 2) {
      split = -bounds.x - bounds.w;
    }
    if (quadrant == 3) {
      split = -bounds.z + bounds.w;
    }
    split = max(split + BEND_GROUP_SIZE - 1, 0) >> 6;
    if (split <= 0) continue;
    const uint second = count++;
    dispatches[second] = dispatches[first];
    switch (quadrant) {
      case 0:
        dispatches[second].groups.z = min(dispatches[first].groups.z, split);
        dispatches[first].groups.z -= dispatches[second].groups.z;
        dispatches[second].offset.y += dispatches[first].groups.z;
        dispatches[second].offset.x -= 1;
        dispatches[second].groups.y += 1;
        break;
      case 1:
        dispatches[second].groups.y = min(dispatches[first].groups.y, split);
        dispatches[first].groups.y -= dispatches[second].groups.y;
        dispatches[second].offset.x += dispatches[first].groups.y;
        dispatches[second].groups.z += 1;
        break;
      case 2:
        dispatches[second].groups.y = min(dispatches[first].groups.y, split);
        dispatches[first].groups.y -= dispatches[second].groups.y;
        dispatches[first].offset.x += dispatches[second].groups.y;
        dispatches[second].groups.z += 1;
        dispatches[second].offset.y -= 1;
        break;
      case 3:
        dispatches[second].groups.z = min(dispatches[first].groups.z, split);
        dispatches[first].groups.z -= dispatches[second].groups.z;
        dispatches[first].offset.y += dispatches[second].groups.z;
        dispatches[second].groups.y += 1;
        break;
    }
    if (dispatches[second].groups.y <= 0 || dispatches[second].groups.z <= 0) {
      dispatches[second] = dispatches[--count];
    }
    if (dispatches[first].groups.y <= 0 || dispatches[first].groups.z <= 0) {
      dispatches[first] = dispatches[--count];
    }
  }
  if (count == 0) return;
  if (spot_budget != 0u) {
    // Admit only lights with actual sweep work. This counter is read and
    // incremented by the GPU, so CPU recording order cannot spend the budget.
    uint index;
    bend_spot_budget.InterlockedAdd(0, 1u, index);
    if (index >= spot_budget) {
      bend_dispatch.Store(40, 2u);
      return;
    }
  }
  bend_dispatch.Store4(0, asuint(light));
  bend_dispatch.Store4(16, uint4(minimum, maximum));
  bend_dispatch.Store4(32, uint4(width, height, 1, count));
  uint tiles = 0;
  [loop]
  for (uint index = 0; index < count; ++index) {
    const uint address = BEND_DISPATCH_HEADER + index * BEND_DISPATCH_STRIDE;
    bend_dispatch.Store3(address, uint3(dispatches[index].groups));
    bend_dispatch.Store2(address + 12, asuint(dispatches[index].offset * BEND_GROUP_SIZE));
    tiles += dispatches[index].groups.y * dispatches[index].groups.z;
    bend_dispatch.Store(address + 20, tiles);
  }
  // Clear only this light's footprint. The pixel shader rejects pixels outside
  // these bounds, so no old result can be consumed from another light.
  bend_dispatch.Store3(BEND_DISPATCH_HEADER + 8 * BEND_DISPATCH_STRIDE,
                       uint3((maximum.x - minimum.x + BEND_GROUP_SIZE - 1) >> 6, maximum.y - minimum.y, 1));
  // Flatten all sweep tiles into one indirect call. The shader recovers each
  // original quadrant from the small prefix list above, without CPU readback.
  bend_dispatch.Store3(BEND_DISPATCH_HEADER + 9 * BEND_DISPATCH_STRIDE,
                       uint3(BEND_GROUP_SIZE, min(tiles, BEND_TILE_ROWS), (tiles + BEND_TILE_ROWS - 1) >> BEND_TILE_ROW_BITS));
}
