#include "../reflections/common.hlsli"
#include "common.hlsli"

// Scene memory state, one group per frame: counts the frame's samples, keeps
// the light direction and its epoch, records the eye, the camera's forward
// direction and the exposure, places the grids and decides whether this frame
// marks. Runs before the other scene memory passes; the host copies the state
// to a readback buffer after them. Layout in common.hlsli.

Texture2D<uint4> samples : register(t0);
RWTexture2D<uint> state : register(u0);

static const uint COUNTED = 5u;
groupshared uint counts[COUNTED];

[numthreads(1024, 1, 1)] void main(uint lane : SV_GroupIndex) {
  if (lane < COUNTED) counts[lane] = 0u;
  GroupMemoryBarrierWithGroupSync();
  uint local[COUNTED] = {0u, 0u, 0u, 0u, 0u};
  for (uint index = lane; index < MEMORY_SAMPLE_COUNT; index += 1024u) {
    const uint2 tile = uint2(index % MEMORY_SAMPLE_GRID.x, index / MEMORY_SAMPLE_GRID.x);
    const uint flags = samples[uint2(tile.x * 2u, tile.y)].w;
    const bool valid = (flags & MEMORY_SAMPLE_VALID) != 0u;
    const bool covered = valid && (flags & MEMORY_SAMPLE_SUN_COVERED) != 0u;
    const bool facing = valid && (flags & MEMORY_SAMPLE_SUN_FACING) != 0u;
    local[MEMORY_STATE_VALID] += valid ? 1u : 0u;
    local[MEMORY_STATE_MOVING] += valid && (flags & MEMORY_SAMPLE_MOVING) != 0u ? 1u : 0u;
    local[MEMORY_STATE_ATLAS] += (flags & MEMORY_SAMPLE_ATLAS) != 0u ? 1u : 0u;
    local[MEMORY_STATE_COVERED] += covered ? 1u : 0u;
    local[MEMORY_STATE_FACING] += facing ? 1u : 0u;
  }
  [unroll] for (uint count = 0u; count < COUNTED; ++count) {
    InterlockedAdd(counts[count], local[count]);
  }
  GroupMemoryBarrierWithGroupSync();
  if (lane != 0u) return;

  [unroll] for (uint word = 0u; word < COUNTED; ++word) {
    state[uint2(word, 0u)] = counts[word];
  }
  // The light direction is known only on frames whose sun draw used the atlas.
  if (counts[MEMORY_STATE_ATLAS] != 0u) {
    const float3 toward_light = SunTowardLight(current_scene.shadow_projection, current_scene.shadow_projection2);
    const float3 direction = normalize(mul((float3x3)current_scene.view_to_world, toward_light));
    if (all(isfinite(direction))) {
      [unroll] for (uint axis = 0u; axis < 3u; ++axis) {
        state[uint2(MEMORY_STATE_SUN_DIRECTION + axis, 0u)] = asuint(direction[axis]);
      }
      const float3 epoch_direction = float3(asfloat(state[uint2(MEMORY_STATE_EPOCH_DIRECTION, 0u)]),
                                            asfloat(state[uint2(MEMORY_STATE_EPOCH_DIRECTION + 1u, 0u)]),
                                            asfloat(state[uint2(MEMORY_STATE_EPOCH_DIRECTION + 2u, 0u)]));
      const float turned = acos(clamp(dot(direction, epoch_direction), -1.0, 1.0));
      // The first direction meets a zeroed image and counts as 90 degrees turned.
      if (turned > SUN_EPOCH_ANGLE) {
        state[uint2(MEMORY_STATE_SUN_EPOCH, 0u)] = state[uint2(MEMORY_STATE_SUN_EPOCH, 0u)] + uint(turned / SUN_EPOCH_ANGLE);
        [unroll] for (uint axis = 0u; axis < 3u; ++axis) {
          state[uint2(MEMORY_STATE_EPOCH_DIRECTION + axis, 0u)] = asuint(direction[axis]);
        }
      }
    }
  }
  const float3 eye = current_scene.eye_position.xyz;
  [unroll] for (uint axis = 0u; axis < 3u; ++axis) {
    state[uint2(MEMORY_STATE_EYE + axis, 0u)] = asuint(eye[axis]);
  }
  state[uint2(MEMORY_STATE_EYE_MISMATCH, 0u)] = asuint(length(eye - current_scene.view_to_world._m03_m13_m23));
  state[uint2(MEMORY_STATE_FRAMES, 0u)] = state[uint2(MEMORY_STATE_FRAMES, 0u)] + 1u;

  // Each window moves in steps of two cells, so its edges stay on the next
  // level's cell boundaries, once it is more than two cells off centered on
  // the eye. A clear recenters every level; the scroll pass then forgets
  // every cell.
  const bool clear = (memory_flags & MEMORY_FLAG_CLEAR) != 0u;
  [unroll] for (uint level = 0u; level < MEMORY_LEVELS; ++level) {
    const int3 centered = 2 * int3(floor((eye / MemoryCellSize(level) - float3(MEMORY_LEVEL_SIZE) * 0.5) * 0.5));
    const int3 previous = (clear ? centered : LoadMemoryOrigin(state, MEMORY_STATE_ORIGIN + 3u * level));
    const int3 origin = (any(abs(centered - previous) > 2) ? centered : previous);
    [unroll] for (uint axis = 0u; axis < 3u; ++axis) {
      state[uint2(MEMORY_STATE_PREVIOUS_ORIGIN + 3u * level + axis, 0u)] = asuint(previous[axis]);
      state[uint2(MEMORY_STATE_ORIGIN + 3u * level + axis, 0u)] = asuint(origin[axis]);
    }
    if (clear) state[uint2(MEMORY_STATE_NEAR_CELLS + level, 0u)] = 0u;
  }
  // While game time is stopped the engine marks nothing as moving, so the
  // frame's characters would be stored. Without the pause word, a frame with
  // no moving samples is taken as paused.
  const bool paused = (memory_flags & MEMORY_FLAG_PAUSED) != 0u
                      || ((memory_flags & MEMORY_FLAG_PAUSE_UNKNOWN) != 0u && counts[MEMORY_STATE_MOVING] == 0u);
  state[uint2(MEMORY_STATE_MARKING, 0u)] = (paused ? 0u : 1u);
  state[uint2(MEMORY_STATE_MARKED, 0u)] = 0u;
  state[uint2(MEMORY_STATE_CARVED, 0u)] = 0u;
  state[uint2(MEMORY_STATE_ALBEDO_WRITTEN, 0u)] = 0u;
  state[uint2(MEMORY_STATE_SUN_WRITTEN, 0u)] = 0u;
  state[uint2(MEMORY_STATE_TERRAIN_CELLS, 0u)] = 0u;
  const float3 forward = normalize(current_scene.view_to_world._m02_m12_m22);
  [unroll] for (uint forward_axis = 0u; forward_axis < 3u; ++forward_axis) {
    state[uint2(MEMORY_STATE_FORWARD + forward_axis, 0u)] = asuint(forward[forward_axis]);
  }
  state[uint2(MEMORY_STATE_EXPOSURE, 0u)] = asuint(current_scene.exposure.z);
  [unroll] for (uint census_level = 0u; census_level < MEMORY_LEVELS; ++census_level) {
    state[uint2(MEMORY_STATE_KNOWN_CELLS + census_level, 0u)] = 0u;
  }
}
