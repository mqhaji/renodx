#include "../reflections/common.hlsli"
#include "common.hlsli"

// Scene memory terrain: fills the grids from the terrain window, so the ground
// is known before the camera sees it. A cell within the band of the terrain
// surface gets its distance to it, as marking would; a cell below the ground
// beyond the band becomes solid if nothing is known there yet. Cells above the
// band are left alone: things stand on the ground. Only full-resolution
// heights are used. Each update takes one z slice in SLICE_STRIDE of every
// level, so all cells are visited every SLICE_STRIDE updates.

Texture2D<float> window : register(t0);
RWTexture2D<uint> state : register(u0);
RWTexture3D<uint> grid : register(u1);

static const uint SLICE_STRIDE = 8u;

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
  const uint3 texel = uint3(id.xy, id.z * SLICE_STRIDE + memory_frame % SLICE_STRIDE);
  const uint level = texel.z / MEMORY_LEVEL_SIZE.z;
  const float cell_size = MemoryCellSize(level);
  // The world cell this texel holds.
  const int3 origin = LoadMemoryOrigin(state, MEMORY_STATE_ORIGIN + 3u * level);
  const int3 size = int3(MEMORY_LEVEL_SIZE);
  const int3 wrapped = int3(texel.xy, texel.z % MEMORY_LEVEL_SIZE.z);
  const int3 cell = origin + ((wrapped - origin) % size + size) % size;
  // Cell centers are multiples of an eighth of a unit, exact in world space.
  const float3 center = (float3(cell) + 0.5) * cell_size;
  // In window points; the four around the center are interpolated.
  const float2 position = (center.xz - TerrainWindowOrigin(current_scene.eye_position.xyz)) / TERRAIN_SPACING;
  if (any(position < 0.0) || any(position >= float(TERRAIN_WINDOW - 1u))) return;
  const int2 base = int2(floor(position));
  const float2 weight = position - float2(base);
  const float h00 = window.Load(int3(base, 0));
  const float h10 = window.Load(int3(base + int2(1, 0), 0));
  const float h01 = window.Load(int3(base + int2(0, 1), 0));
  const float h11 = window.Load(int3(base + int2(1, 1), 0));
  if (min(min(h00, h10), min(h01, h11)) == TERRAIN_NONE) return;
  const float height = lerp(lerp(h00, h10, weight.x), lerp(h01, h11, weight.x), weight.y);
  const float2 slope = float2(lerp(h10 - h00, h11 - h01, weight.y), lerp(h01 - h00, h11 - h10, weight.x)) / TERRAIN_SPACING;
  const float above = center.y - height;
  // Distance to the surface's tangent plane, in cells.
  const float distance = abs(above) * rsqrt(1.0 + dot(slope, slope)) / cell_size;
  uint before;
  if (distance <= MEMORY_BAND) {
    InterlockedMin(grid[texel], uint(distance * 1024.0) << 16u, before);
    if (before >= MEMORY_SOLID) InterlockedAdd(state[uint2(MEMORY_STATE_NEAR_CELLS + level, 0u)], 1u);
    InterlockedAdd(state[uint2(MEMORY_STATE_TERRAIN_CELLS, 0u)], 1u);
  } else if (above < 0.0) {
    InterlockedCompareExchange(grid[texel], MEMORY_UNKNOWN, MEMORY_SOLID, before);
    if (before == MEMORY_UNKNOWN) InterlockedAdd(state[uint2(MEMORY_STATE_TERRAIN_CELLS, 0u)], 1u);
  }
}
