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

// Writes one light's record except its state, and returns the atlas texels and
// trace tiles it needs, or 0 when it has no contact work. Admission decides later.
uint2 PrepareBendLight(uint light_index, uint2 output_size) {
  const uint record = light_index * BEND_RECORD_STRIDE;
  const uint3 kind = ReadBendLightKind(light_index);
  const float light_type = float(kind.x);
  const uint width = output_size.x;
  const uint height = output_size.y;
  // Start with no supported contact output. Every failed qualification leaves
  // neutral metadata, while PCSS can still use the cached native projection.
  // The caller writes each record's atlas location and work totals afterward.
  bend_dispatch.Store4(record, 0u);
  bend_dispatch.Store4(record + 16u, 0u);
  bend_dispatch.Store4(record + 32u, uint4(width, height, 0, 0));
  const bool directional = light_type < 0.5;
  // Keep PCSS available even when this light skips contact tracing. Only draws
  // that read a shadow map use it. Point shadows use their native radial
  // projection and need no cached matrix data. A local light's projection is
  // its own; the sun's is in the scene constants.
  PcssProjection shadow_projection = (PcssProjection)0;
  if (kind.y != 0u && !IsBendPointLight(light_type) && CUSTOM_SHADOW_SOFTNESS > 0.0) {
    if (directional) {
      shadow_projection = CalculatePcssProjection(bend_shadow_projection, bend_shadow_cascades, true);
    } else {
      // The four vectors are the matrix's columns, as the native buffer stores them.
      shadow_projection = CalculatePcssProjection(
          transpose(float4x4(ReadBendLightParam(light_index, BEND_LIGHT_SHADOW), ReadBendLightParam(light_index, BEND_LIGHT_SHADOW + 1u),
                             ReadBendLightParam(light_index, BEND_LIGHT_SHADOW + 2u), ReadBendLightParam(light_index, BEND_LIGHT_SHADOW + 3u))),
          (float4x4)0, false);
    }
  }
  bend_dispatch.Store4(record + PCSS_PROJECTION_OFFSET, asuint(float4(shadow_projection.uv_scale, shadow_projection.depth_scale, shadow_projection.depth_a)));
  bend_dispatch.Store(record + PCSS_PROJECTION_OFFSET + 16u, asuint(shadow_projection.depth_b));
  const uint2 source_size = uint2(bend_viewport.xy + 0.5);
  if (width == 0 || height == 0 || any(source_size == 0)
      || any(source_size != uint2(width, height))) return 0u;
  // Native directional shading uses N dot L, so this points toward the source.
  // Local positions are already in view space. Keep the native center for bounds.
  const float4 position = directional ? float4(ReadBendLightParam(light_index, 0u).xyz, 0.0)
                                      : float4(ReadBendLightParam(light_index, 3u).xyz, 1.0);
  if (!all(isfinite(position)) || dot(position.xyz, position.xyz) < 1e-12) return 0u;
  // A common source offset moves whole shadow edges together. Keep the sweep
  // centered and resolve its finite-source footprint in the light shader.
  const float4 projected = mul(position, bend_projection);
  if (!all(isfinite(projected))) return 0u;
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
  if (!all(isfinite(light)) || any(abs(light.xy) > 1073741824.0)) return 0u;

  int2 minimum = 0;
  int2 maximum = int2(width, height);
  // Native attenuation gives a finite sphere. For spotlights, keep only the
  // part inside the native light cone before projecting the receiver bounds.
  const bool distance_light = IsBendDistanceLight(light_type);
  const float attenuation = distance_light ? ReadBendLightParam(light_index, 6u).z : ReadBendLightParam(light_index, 6u).w;
  // The distance variant caps d^16 at FLT_MAX. Tiny/subnormal coefficients
  // need not reach zero after that cap, so do not infer a finite range there.
  const bool finite_cutoff = attenuation > 0.0 && (!distance_light || attenuation >= 1.17549435e-38);
  if (!directional && isfinite(attenuation) && finite_cutoff) {
    // Distance lights cut off at 1-k*d^16 = 0. Other local lights use
    // 1/d^2-k*d^2 = 0. Both point variants can offset that distance.
    const float radius = (distance_light ? rsqrt(sqrt(sqrt(sqrt(attenuation)))) : rsqrt(sqrt(attenuation)))
                         + (IsBendPointLight(light_type) ? abs(ReadBendLightParam(light_index, 6u).x) : 0.0);
    if (BendSphereOutsideRays(position.xyz, radius, 0.0, float2(source_size), float2(source_size))) {
      bend_dispatch.Store(record + 40u, 2u);
      return 0u;
    }
    float3 lower_corner = position.xyz - radius;
    float3 upper_corner = position.xyz + radius;
    const float3 cone = ReadBendLightParam(light_index, 7u).xyz;
    const float3 axis = ReadBendLightParam(light_index, 5u).xyz;
    const float axis_length = length(axis);
    if (!IsBendPointLight(light_type) && isfinite(radius) && all(isfinite(cone))
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
    if (isfinite(radius) && upper_corner.z <= 0.0) return 0u;
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
  if (any(maximum <= minimum)) return 0u;

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
  if (count == 0) return 0u;
  bend_dispatch.Store4(record, asuint(light));
  bend_dispatch.Store4(record + 16u, uint4(minimum, maximum));
  bend_dispatch.Store4(record + 32u, uint4(width, height, 0, count));
  uint tiles = 0;
  [loop]
  for (uint index = 0; index < count; ++index) {
    const uint address = record + BEND_DISPATCH_HEADER + index * BEND_DISPATCH_STRIDE;
    bend_dispatch.Store3(address, uint3(dispatches[index].groups));
    bend_dispatch.Store2(address + 12, asuint(dispatches[index].offset * BEND_GROUP_SIZE));
    tiles += dispatches[index].groups.y * dispatches[index].groups.z;
    bend_dispatch.Store(address + 20, tiles);
  }
  return uint2(uint(maximum.x - minimum.x) * uint(maximum.y - minimum.y), tiles);
}

#define BEND_SETUP_THREADS 256
#define BEND_CANDIDATE     3u
#define BEND_BUDGETED_SPOT 4u

// Per draw position, the light drawn there. Per light, its texels, trace tiles,
// and state; admission and packing then turn those into running totals.
groupshared uint bend_order[BEND_MAX_LIGHTS];
groupshared uint bend_area[BEND_MAX_LIGHTS];
groupshared uint bend_tiles[BEND_MAX_LIGHTS];
groupshared uint bend_state[BEND_MAX_LIGHTS];
groupshared uint bend_start[BEND_MAX_LIGHTS];

[numthreads(BEND_SETUP_THREADS, 1, 1)]
void main(uint thread : SV_GroupIndex) {
  const uint4 batch = asuint(bend_lights.Load(0u));
  const uint light_count = min(batch.x, BEND_MAX_LIGHTS);
  const uint2 output_size = uint2(bend_viewport.xy + 0.5);
  const bool spot_budget_enabled = isfinite(CUSTOM_SPOT_CONTACT_SHADOW_BUDGET) && CUSTOM_SPOT_CONTACT_SHADOW_BUDGET >= 1.0;
  // Every light's geometry is independent, so all threads build records.
  [loop]
  for (uint position = thread; position < light_count; position += BEND_SETUP_THREADS) {
    const uint light_index = ReadBendLightKind(position).z;
    const uint light_type = ReadBendLightKind(light_index).x;
    const uint2 work = PrepareBendLight(light_index, output_size);
    bend_order[position] = light_index;
    bend_area[light_index] = work.x;
    bend_tiles[light_index] = work.y;
    bend_state[light_index] = work.x == 0u ? 0u
                              : (spot_budget_enabled && (light_type == 1u || light_type == 3u) ? BEND_BUDGETED_SPOT : BEND_CANDIDATE);
  }
  GroupMemoryBarrierWithGroupSync();
  if (thread == 0u) {
    // Admit lights in the order their draws execute, which the host supplies
    // because parallel jobs append their lights in any order. The atlas space
    // and the spot budget go to the first lights in that order; a light that
    // does not fit, or a spot past the budget, keeps native shadows. Each
    // budgeted spot that fits counts, as the GPU-ordered counter always has.
    const uint spot_budget = spot_budget_enabled ? uint(min(CUSTOM_SPOT_CONTACT_SHADOW_BUDGET, 2147483520.0)) : 0u;
    uint spots = spot_budget_enabled ? bend_spot_budget.Load(0) : 0u;
    uint atlas_free = batch.y * batch.z;
    [loop]
    for (uint position = 0u; position < light_count; ++position) {
      const uint light_index = bend_order[position];
      const uint state = bend_state[light_index];
      if (state == 0u) continue;
      const uint area = bend_area[light_index];
      bool admitted = area <= atlas_free;
      if (admitted && state == BEND_BUDGETED_SPOT) {
        admitted = spots < spot_budget;
        ++spots;
      }
      bend_state[light_index] = admitted ? 1u : 2u;
      atlas_free -= admitted ? area : 0u;
    }
    if (spot_budget_enabled) {
      bend_spot_budget.Store(0, spots);
    }
    // Pack the admitted rectangles in record order. Clear and trace find a
    // light by binary search over these running totals.
    uint atlas_end = 0u;
    uint trace_tiles = 0u;
    [loop]
    for (uint light_index = 0u; light_index < light_count; ++light_index) {
      const bool admitted = bend_state[light_index] == 1u;
      bend_start[light_index] = atlas_end;
      atlas_end += admitted ? bend_area[light_index] : 0u;
      trace_tiles += admitted ? bend_tiles[light_index] : 0u;
      bend_area[light_index] = atlas_end;
      bend_tiles[light_index] = trace_tiles;
    }
    const uint footer = batch.x * BEND_RECORD_STRIDE;
    const uint groups = (atlas_end + BEND_GROUP_SIZE - 1u) >> 6u;
    bend_dispatch.Store3(footer, atlas_end > 0u ? uint3(min(groups, BEND_TILE_ROWS), (groups + BEND_TILE_ROWS - 1u) >> BEND_TILE_ROW_BITS, 1u) : 0u);
    bend_dispatch.Store3(footer + 16u, trace_tiles > 0u ? uint3(BEND_GROUP_SIZE, min(trace_tiles, BEND_TILE_ROWS),
                                                                (trace_tiles + BEND_TILE_ROWS - 1u) >> BEND_TILE_ROW_BITS)
                                                        : 0u);
  }
  GroupMemoryBarrierWithGroupSync();
  // Each thread finishes the records it built. Native coordinates stay
  // unchanged; only mask storage is packed.
  [loop]
  for (uint built = thread; built < light_count; built += BEND_SETUP_THREADS) {
    const uint light_index = bend_order[built];
    const uint record = light_index * BEND_RECORD_STRIDE;
    bend_dispatch.Store(record + 40u, bend_state[light_index]);
    bend_dispatch.Store2(record + BEND_ATLAS_OFFSET, uint2(bend_start[light_index], batch.y));
    bend_dispatch.Store(record + BEND_TRACE_END_OFFSET, bend_tiles[light_index]);
    bend_dispatch.Store(record + BEND_CLEAR_END_OFFSET, bend_area[light_index]);
  }
}
