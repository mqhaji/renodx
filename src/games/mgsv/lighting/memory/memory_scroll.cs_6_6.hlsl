#include "common.hlsli"

// Scene memory scrolling: after the state pass moves a level's window, the
// texels that now hold a cell the old window did not cover are reset to
// unknown, and the color and sun grids forget their half cells the same way.
// A clear resets every texel. One thread per texel of the distance grids; the
// threads at even coordinates also take the half-resolution texel there.
// Levels that did not move touch no memory unless the census flag asks for the
// count of known cells, which this pass then takes.

RWTexture2D<uint> state : register(u0);
RWTexture3D<uint> grid : register(u1);
RWTexture3D<uint> albedo : register(u2);
RWTexture3D<uint> sun : register(u3);

[numthreads(8, 8, 1)] void main(uint3 texel : SV_DispatchThreadID) {
  const uint level = texel.z / MEMORY_LEVEL_SIZE.z;
  const bool clear = (memory_flags & MEMORY_FLAG_CLEAR) != 0u;
  const bool half_texel = all((texel & 1u) == 0u);
  bool reset = clear;
  bool reset_half = clear && half_texel;
  if (!clear) {
    const int3 origin = LoadMemoryOrigin(state, MEMORY_STATE_ORIGIN + 3u * level);
    const int3 previous = LoadMemoryOrigin(state, MEMORY_STATE_PREVIOUS_ORIGIN + 3u * level);
    if (any(origin != previous)) {
      // The world cell this texel holds in the moved window.
      const int3 size = int3(MEMORY_LEVEL_SIZE);
      const int3 wrapped = int3(texel.xy, texel.z % MEMORY_LEVEL_SIZE.z);
      const int3 cell = origin + ((wrapped - origin) % size + size) % size;
      reset = any(cell < previous) || any(cell >= previous + size);
      if (half_texel) {
        // Origins are even, so halving them gives the half grids' windows.
        const int3 half_size = int3(MEMORY_HALF_SIZE);
        const int3 half_wrapped = wrapped / 2;
        const int3 half_cell = origin / 2 + ((half_wrapped - origin / 2) % half_size + half_size) % half_size;
        reset_half = any(half_cell < previous / 2) || any(half_cell >= previous / 2 + half_size);
      }
    }
  }
  if (reset_half) {
    const uint3 half_texel_index = uint3(texel.xy / 2u, level * MEMORY_HALF_SIZE.z + (texel.z % MEMORY_LEVEL_SIZE.z) / 2u);
    albedo[half_texel_index] = 0u;
    sun[half_texel_index] = 0u;
  }
  // The census flag is the same for every thread, so whole waves reach the
  // count below together.
  const bool census = (memory_flags & MEMORY_FLAG_CENSUS) != 0u;
  if (!reset && !census) return;
  const uint value = grid[texel];
  if (reset) {
    if (!clear && value < MEMORY_SOLID) InterlockedAdd(state[uint2(MEMORY_STATE_NEAR_CELLS + level, 0u)], 0xFFFFFFFFu);
    grid[texel] = MEMORY_UNKNOWN;
  }
  if (census) {
    // A group's threads share one slice, so one level.
    const uint known = WaveActiveCountBits(!reset && value != MEMORY_UNKNOWN);
    if (WaveIsFirstLane() && known != 0u) InterlockedAdd(state[uint2(MEMORY_STATE_KNOWN_CELLS + level, 0u)], known);
  }
}
