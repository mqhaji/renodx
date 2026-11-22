// SPDX-License-Identifier: MIT

// Sorts the frame's local lights into bins of the fog volume, so each fog
// cell looks at a few lights and not at hundreds.
//
// A bin is a block of cells: FOG_BIN_CELLS a side on screen and
// FOG_BIN_SLICES deep. One thread per bin keeps the lights whose reach
// touches the bin (for a spot, whose cone does), and of those the
// FOG_BIN_LIGHTS that light the bin most. Choosing by strength and not by
// order keeps a crowded bin's choice the same from frame to frame.
//
// A group's 64 bins lie side by side at one depth or two, and most lights
// reach only a few depths. So the group first goes through the lights once,
// its threads sharing them out, and lists the ones that reach the box around
// all its bins; each thread then tests only that list against its own bin.
//
// The image has a row per bin. An entry holds a light's index, the tile of
// its kept shadow map and its reach (layout in fog_lights.hlsli);
// FOG_BIN_END closes a row that is not full. A shadowed spot is matched to
// its tile here, by its position, so the fog's cells need not search.

#include "./fog_lights.hlsli"

RWTexture2D<uint> bins : register(u0);

// The group's lights: position and reach, and index with the shadow tile in
// an entry's place.
groupshared float4 group_spheres[FOG_BIN_LIGHT_MASK + 1u];
groupshared uint group_lights[FOG_BIN_LIGHT_MASK + 1u];
groupshared uint group_count;

// The view-space box around the bins from first to last. Cell s covers its
// column from slice s - 1 to slice s.
void FogBinBox(uint3 first, uint3 last, uint2 cells, out float3 lower, out float3 upper) {
  const float near_depth = first.z == 0u ? 0.0 : FogSliceDepth(float(first.z * FOG_BIN_SLICES) - 1.0);
  const float far_depth = FogSliceDepth(float((last.z + 1u) * FOG_BIN_SLICES) - 1.0);
  // The box's two sides at a unit of depth, in either order: the projection's
  // scales can be negative.
  const float2 first_side = FogClip(float2(first.xy * FOG_BIN_CELLS) / float2(cells)) * fog_scene.m_projectionParam.xy;
  const float2 last_side = FogClip(float2(min((last.xy + 1u) * FOG_BIN_CELLS, cells)) / float2(cells)) * fog_scene.m_projectionParam.xy;
  lower = float3(min(min(first_side, last_side) * near_depth, min(first_side, last_side) * far_depth), near_depth);
  upper = float3(max(max(first_side, last_side) * near_depth, max(first_side, last_side) * far_depth), far_depth);
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint thread : SV_GroupIndex) {
  const uint2 cells = uint2(fog_cells_x, fog_cells_y);
  const uint2 bin_count = FogBins(cells);
  const uint depth_bins = uint(FOG_SLICES) / FOG_BIN_SLICES;
  // Threads run over x and depth together along x.
  const uint3 bin = uint3(id.x % bin_count.x, id.y, id.x / bin_count.x);

  // 1. The lights that reach the group's bins. Its eight columns of threads
  // are at one depth, or at two where a row of bins ends among them, and
  // then they are taken to span the row.
  if (thread == 0u) {
    group_count = 0u;
  }
  GroupMemoryBarrierWithGroupSync();
  const uint2 group_depths = min(uint2(group.x * 8u, group.x * 8u + 7u) / bin_count.x, depth_bins - 1u);
  const uint2 group_columns = group_depths.x == group_depths.y ? uint2(group.x * 8u, group.x * 8u + 7u) % bin_count.x : uint2(0u, bin_count.x - 1u);
  float3 group_lower, group_upper;
  FogBinBox(uint3(group_columns.x, group.y * 8u, group_depths.x), uint3(group_columns.y, min(group.y * 8u + 7u, bin_count.y - 1u), group_depths.y), cells,
            group_lower, group_upper);
  const uint light_count = min(asuint(fog_lights.Load(0u).x), FOG_BIN_LIGHT_MASK + 1u);
  [loop] for (uint light = thread; light < light_count; light += 64u) {
    const uint2 kind = asuint(FogLightVector(light, FOG_LIGHT_KIND).xy);
    if (kind.x == 0u || kind.x > 4u) continue;
    const float3 position = FogLightVector(light, 3u).xyz;
    const float reach = FogLightReach(light, kind.x);
    const float3 outside = max(max(group_lower - position, position - group_upper), 0.0);
    if (dot(outside, outside) >= reach * reach) continue;
    uint shadow_tile = 0u;
    if ((kind.x & 1u) != 0u && kind.y != 0u) {
      for (uint tile = 0u; tile < FOG_SPOT_SHADOWS; ++tile) {
        const float4 projection_w = fog_spot_shadows.Load(tile * FOG_SPOT_SHADOW_VECTORS + 4u);
        if (dot(projection_w, projection_w) > 0.0 && all(fog_spot_shadows.Load(tile * FOG_SPOT_SHADOW_VECTORS).xyz == position)) {
          shadow_tile = tile + 1u;
        }
      }
    }
    uint listed;
    InterlockedAdd(group_count, 1u, listed);
    group_spheres[listed] = float4(position, reach);
    group_lights[listed] = (shadow_tile << FOG_BIN_SHADOW_SHIFT) | light;
  }
  GroupMemoryBarrierWithGroupSync();
  if (bin.y >= bin_count.y || bin.z >= depth_bins) return;

  // 2. Of those, the ones that reach this thread's bin, strongest first to
  // keep. The bin is taken as a sphere around its box.
  float3 lower, upper;
  FogBinBox(bin, bin, cells, lower, upper);
  const float3 center = 0.5 * (lower + upper);
  const float radius = 0.5 * length(upper - lower);
  uint kept_entries[FOG_BIN_LIGHTS];
  uint kept = 0u;
  const uint listed_count = group_count;
  [loop] for (uint listed = 0u; listed < listed_count; ++listed) {
    const float4 sphere = group_spheres[listed];
    const float3 from_light = center - sphere.xyz;
    const float distance_squared = dot(from_light, from_light);
    if (distance_squared >= (sphere.w + radius) * (sphere.w + radius)) continue;
    const uint light = group_lights[listed] & FOG_BIN_LIGHT_MASK;
    if ((asuint(FogLightVector(light, FOG_LIGHT_KIND).x) & 1u) != 0u) {
      // A spot: skip a bin wholly outside its cone. The cone's edge is where
      // its light starts, at the cosine in [7].x against the axis in [5].
      const float3 axis = FogLightVector(light, 5u).xyz;
      const float cosine = FogLightVector(light, 7u).x / max(length(axis), 1.0e-6);
      if (cosine > 0.0 && cosine < 1.0) {
        const float along = dot(from_light, normalize(axis));
        const float sine = sqrt(1.0 - cosine * cosine);
        if (along < -radius || cosine * sqrt(max(distance_squared - along * along, 0.0)) - along * sine > radius) continue;
      }
    }
    // Its light at the bin's nearest point, as a strength to rank by: sixteen
    // steps to a doubling, over a range far wider than lights span.
    const float nearest = max(sqrt(distance_squared) - radius, 0.1);
    const float3 color = FogLightVector(light, 4u).xyz * FogLightVector(light, 8u).y;
    const uint entry = (uint(clamp(log2(max(max(color.r, max(color.g, color.b)) / (nearest * nearest), 1.0e-20)) * 16.0 + 32768.0, 1.0, 65534.0)) << 16u)
                       | group_lights[listed];
    if (kept < FOG_BIN_LIGHTS) {
      kept_entries[kept] = entry;
      ++kept;
    } else {
      // Full: the new light takes the weakest kept light's place if it is stronger.
      uint weakest = 0u;
      for (uint slot = 1u; slot < FOG_BIN_LIGHTS; ++slot) {
        if (kept_entries[slot] < kept_entries[weakest]) {
          weakest = slot;
        }
      }
      if (entry > kept_entries[weakest]) {
        kept_entries[weakest] = entry;
      }
    }
  }
  // The strength has done its work; the cells need the light's reach in its place.
  const uint row = (bin.z * bin_count.y + bin.y) * bin_count.x + bin.x;
  for (uint slot = 0u; slot < kept; ++slot) {
    const uint light = kept_entries[slot] & FOG_BIN_LIGHT_MASK;
    bins[uint2(slot, row)] = (f32tof16(FogLightReach(light, asuint(FogLightVector(light, FOG_LIGHT_KIND).x))) << 16u) | (kept_entries[slot] & 0xFFFFu);
  }
  if (kept < FOG_BIN_LIGHTS) {
    bins[uint2(kept, row)] = FOG_BIN_END;
  }
}
