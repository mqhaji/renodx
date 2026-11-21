#include "../reflections/common.hlsli"
#include "common.hlsli"

// Scene memory marking: each valid sample the engine did not move this frame
// writes, into the 4 x 4 x 4 cells around it, the distance from each cell
// center, keeping the smallest a cell has seen. It marks the finest level
// whose window holds that block, and with a chance of one in eight the next
// level that does too, so coarser levels keep what finer ones scroll out. At
// each level it marks, the sample also blends its color into the half cell
// holding it, and if it faces the light inside a cascade, its sun visibility:
// an old value (SUN_OLD_STEPS) is replaced, a newer one blended.
// Samples in one half cell can race; a lost blend only drops one sample.

Texture2D<uint4> samples : register(t0);
RWTexture2D<uint> state : register(u0);
RWTexture3D<uint> grid : register(u1);
RWTexture3D<uint> albedo : register(u2);
RWTexture3D<uint> sun : register(u3);

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
  if (any(id.xy >= MEMORY_SAMPLE_GRID)) return;
  if (state[uint2(MEMORY_STATE_MARKING, 0u)] == 0u) return;
  const uint4 first = samples[uint2(id.x * 2u, id.y)];
  if ((first.w & MEMORY_SAMPLE_VALID) == 0u || (first.w & MEMORY_SAMPLE_MOVING) != 0u) return;
  const float3 relative = asfloat(first.xyz);
  const uint4 second = samples[uint2(id.x * 2u + 1u, id.y)];
  const uint3 sample_color = uint3(second.y & 0xFFu, (second.y >> 8u) & 0xFFu, (second.y >> 16u) & 0xFFu);
  const uint sun_flags = MEMORY_SAMPLE_ATLAS | MEMORY_SAMPLE_SUN_COVERED | MEMORY_SAMPLE_SUN_FACING;
  const bool sun_measured = (first.w & sun_flags) == sun_flags;
  const uint epoch = state[uint2(MEMORY_STATE_SUN_EPOCH, 0u)];
  const float3 eye = current_scene.eye_position.xyz;
  const bool second_level = (MemoryHash(uint3(id.xy, memory_frame)).z & 7u) == 0u;
  uint levels_marked = 0u;
  for (uint level = 0u; level < MEMORY_LEVELS; ++level) {
    const float cell_size = MemoryCellSize(level);
    const int3 origin = LoadMemoryOrigin(state, MEMORY_STATE_ORIGIN + 3u * level);
    // In cells from the window's first cell. Both terms are small, so the
    // position keeps its precision far from the world origin.
    const float3 position = (eye - float3(origin) * cell_size + relative) / cell_size;
    if (any(position < 2.0) || any(position > float3(MEMORY_LEVEL_SIZE) - 2.0)) continue;
    const int3 base = int3(floor(position - 1.5));
    for (uint index = 0u; index < 64u; ++index) {
      const int3 local = base + int3(index & 3u, (index >> 2u) & 3u, index >> 4u);
      const float distance = length(float3(local) + 0.5 - position);
      if (distance > MEMORY_BAND) continue;
      uint before;
      InterlockedMin(grid[MemoryTexel(origin + local, level)], uint(distance * 1024.0) << 16u, before);
      if (before >= MEMORY_SOLID) InterlockedAdd(state[uint2(MEMORY_STATE_NEAR_CELLS + level, 0u)], 1u);
    }
    const uint3 half_texel = MemoryHalfTexel((origin + int3(floor(position))) >> 1, level);
    const uint stored_color = albedo[half_texel];
    const uint count = stored_color >> 24u;
    uint3 color = sample_color;
    if (count != 0u) {
      const int3 previous = int3(stored_color & 0xFFu, (stored_color >> 8u) & 0xFFu, (stored_color >> 16u) & 0xFFu);
      color = uint3(previous + (int3(sample_color) - previous) / int(min(count, 15u) + 1u));
    }
    albedo[half_texel] = color.r | (color.g << 8u) | (color.b << 16u) | (min(count + 1u, 255u) << 24u);
    if (sun_measured) {
      const uint stored_sun = sun[half_texel];
      uint visibility = second.z & 0xFFu;
      if ((stored_sun & MEMORY_SUN_SET) != 0u && SunEpochAge((stored_sun >> 8u) & 0xFFFFu, epoch) <= (SUN_OLD_STEPS << level)) {
        const int previous = int(stored_sun & 0xFFu);
        visibility = uint(previous + (int(visibility) - previous) / 4);
      }
      sun[half_texel] = visibility | ((epoch & 0xFFFFu) << 8u) | MEMORY_SUN_SET;
    }
    ++levels_marked;
    if (levels_marked == 2u || !second_level) break;
  }
  if (levels_marked != 0u) {
    InterlockedAdd(state[uint2(MEMORY_STATE_MARKED, 0u)], 1u);
    InterlockedAdd(state[uint2(MEMORY_STATE_ALBEDO_WRITTEN, 0u)], 1u);
    if (sun_measured) InterlockedAdd(state[uint2(MEMORY_STATE_SUN_WRITTEN, 0u)], 1u);
  }
}
