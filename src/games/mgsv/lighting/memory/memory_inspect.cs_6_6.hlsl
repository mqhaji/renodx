#include "../reflections/common.hlsli"
#include "common.hlsli"

// Scene memory debug views, written over the reflection result the composite
// shows. Each pixel's camera ray is marched through the finest level that
// holds it, across known-empty cells, until it reaches a stored surface or an
// unknown cell or leaves the grids. Marching starts MARCH_START from the eye:
// carving cannot judge the cells around the camera, so they stay unknown.
//   18 Scene Memory: a stored surface shows its stored color, shaded by its
//      distance field's normal; without a color yet it is tinted by level
//      (white, yellow, cyan, pink from finest to coarsest); magenta is unknown
//      space; dark blue means the ray left the grids through known-empty space.
//   19 Behind Camera: the same with each ray turned half a turn about the
//      vertical, as if the camera had turned around.
//   20 Agreement: memory against the surface the pixel shows, with gaps
//      measured along that surface's normal, since a ray grazing the ground
//      crosses the stored shell well before the visible point. Green: memory
//      holds that surface; red: memory holds a surface in front of it (a
//      ghost); blue: memory holds that surface's place as empty (missing);
//      magenta: the ray met unknown space first; dark green: the ray left the
//      grids through empty space before reaching the surface or the sky.
//   21 Sun Visibility: a stored surface shows its stored sun visibility from
//      black (shadowed) to white (lit); red where none was stored (surfaces
//      facing away from the light store none), orange where it is old
//      (SUN_OLD_STEPS); other colors as in view 18.

Texture3D<uint> grid : register(t0);
Texture2D<float> depth : register(t1);
// View-space G-buffer normal (xyz), zero where it is not valid.
Texture2D<float4> normal : register(t2);
Texture3D<uint> albedo : register(t3);
Texture3D<uint> sun : register(t4);
RWTexture2D<uint> state : register(u0);
RWTexture2D<float4> view : register(u1);

static const float HIT_DISTANCE = 0.75;  // cells
static const uint MAX_STEPS = 768u;
static const float MARCH_START = 1.0;  // units
static const float3 LEVEL_TINT[MEMORY_LEVELS] = {float3(1.0, 1.0, 1.0), float3(1.0, 0.85, 0.45), float3(0.45, 0.9, 1.0),
                                                 float3(1.0, 0.55, 0.85)};
static const float3 UNKNOWN_COLOR = float3(0.5, 0.0, 0.5);
static const uint MARCH_HIT = 0u;
static const uint MARCH_UNKNOWN = 1u;
static const uint MARCH_LEFT = 2u;

// Distance to the nearest surface in cells: solid ground is a surface wherever
// a ray reaches it; empty and unknown cells count as the band's edge.
float StoredDistance(uint value) {
  if (value < MEMORY_SOLID) return float(value >> 16u) / 1024.0;
  return (value == MEMORY_SOLID ? 0.0 : MEMORY_BAND);
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
  if (any(id.xy >= reflection_size)) return;
  const float2 ndc = ((float2(id.xy) + 0.5) / float2(reflection_size) * 2.0 - 1.0) * float2(1.0, -1.0);
  const float3 view_ray = float3((ndc - current_scene.camera_center_offset.xy) * current_scene.projection_parameters.xy, 1.0);
  float3 direction = normalize(mul((float3x3)current_scene.view_to_world, view_ray));
  if (memory_view == 19u) direction.xz = -direction.xz;
  // The surface this pixel shows, as a distance along the ray.
  const float native_depth = depth.Load(int3(id.xy, 0));
  const float surface = (native_depth > 0.0 ? GetReflectionDepth(native_depth) * length(view_ray) : 1e30);
  // Distance along the ray per unit of distance from the surface's plane.
  const float3 surface_normal = normal.Load(int3(id.xy, 0)).xyz;
  const float slant = (dot(surface_normal, surface_normal) > 0.25 ? max(abs(dot(normalize(view_ray), normalize(surface_normal))), 0.05) : 1.0);

  // The eye in cells from each window's first cell.
  const float3 eye = current_scene.eye_position.xyz;
  int3 origins[MEMORY_LEVELS];
  float3 eye_cells[MEMORY_LEVELS];
  [unroll] for (uint level = 0u; level < MEMORY_LEVELS; ++level) {
    origins[level] = LoadMemoryOrigin(state, MEMORY_STATE_ORIGIN + 3u * level);
    eye_cells[level] = (eye - float3(origins[level]) * MemoryCellSize(level)) / MemoryCellSize(level);
  }

  uint outcome = MARCH_LEFT;
  uint hit_level = 0u;
  int3 hit_cell = 0;
  float facing = 0.5;
  float traveled = MARCH_START;  // units
  for (uint step = 0u; step < MAX_STEPS; ++step) {
    uint level = MEMORY_LEVELS;
    float3 position = 0.0;
    for (uint candidate = 0u; candidate < MEMORY_LEVELS; ++candidate) {
      position = eye_cells[candidate] + direction * (traveled / MemoryCellSize(candidate));
      if (all(position >= 1.0) && all(position < float3(MEMORY_LEVEL_SIZE) - 1.0)) {
        level = candidate;
        break;
      }
    }
    if (level == MEMORY_LEVELS) break;
    const int3 cell = int3(floor(position));
    const uint value = grid.Load(int4(MemoryTexel(origins[level] + cell, level), 0));
    if (value == MEMORY_UNKNOWN) {
      outcome = MARCH_UNKNOWN;
      break;
    }
    const float near = StoredDistance(value);
    if (near <= HIT_DISTANCE) {
      float neighbors[6];
      [unroll] for (uint side = 0u; side < 6u; ++side) {
        int3 offset = 0;
        offset[side >> 1u] = ((side & 1u) != 0u ? 1 : -1);
        neighbors[side] = StoredDistance(grid.Load(int4(MemoryTexel(origins[level] + cell + offset, level), 0)));
      }
      const float3 gradient = float3(neighbors[1] - neighbors[0], neighbors[3] - neighbors[2], neighbors[5] - neighbors[4]);
      facing = (dot(gradient, gradient) > 1e-6 ? abs(dot(normalize(gradient), direction)) : 0.5);
      outcome = MARCH_HIT;
      hit_level = level;
      hit_cell = origins[level] + cell;
      break;
    }
    // The agreement view needs nothing past the surface the pixel shows.
    if (memory_view == 20u && (traveled - surface) * slant > 2.0 * MemoryCellSize(level)) break;
    // An empty or band cell has no sample within the band of its center, so
    // one-cell steps cannot jump a surface; inside the band the ray closes in
    // by the distance left, less the cell's half diagonal.
    traveled += max(near - 1.0, 0.5) * MemoryCellSize(level);
  }

  float3 color;
  if (memory_view == 20u) {
    const float tolerance = 2.0 * MemoryCellSize(hit_level);
    if (outcome == MARCH_UNKNOWN) {
      color = UNKNOWN_COLOR;
    } else if (outcome == MARCH_HIT) {
      color = ((surface - traveled) * slant > tolerance ? float3(0.9, 0.1, 0.1) : float3(0.1, 0.8, 0.1));
    } else if (traveled < surface) {
      // Empty as far as the grids reach, and the surface lies beyond them.
      color = float3(0.05, 0.35, 0.05);
    } else {
      color = float3(0.1, 0.25, 0.95);
    }
  } else if (outcome == MARCH_HIT) {
    const uint3 half_texel = MemoryHalfTexel(hit_cell >> 1, hit_level);
    if (memory_view == 21u) {
      const uint stored = sun.Load(int4(half_texel, 0));
      const uint age = SunEpochAge((stored >> 8u) & 0xFFFFu, state[uint2(MEMORY_STATE_SUN_EPOCH, 0u)]);
      if ((stored & MEMORY_SUN_SET) == 0u) {
        color = float3(0.8, 0.0, 0.0);
      } else if (age > (SUN_OLD_STEPS << hit_level)) {
        color = float3(0.9, 0.45, 0.0);
      } else {
        color = float(stored & 0xFFu) / 255.0;
      }
    } else {
      const uint stored = albedo.Load(int4(half_texel, 0));
      const float3 encoded = float3(stored & 0xFFu, (stored >> 8u) & 0xFFu, (stored >> 16u) & 0xFFu) / 255.0;
      // Square root of linear, as the samples store it.
      const float3 surface_color = ((stored >> 24u) != 0u ? encoded * encoded : LEVEL_TINT[hit_level]);
      color = surface_color * (0.2 + 0.8 * facing);
    }
  } else if (outcome == MARCH_UNKNOWN) {
    color = UNKNOWN_COLOR;
  } else {
    color = float3(0.05, 0.1, 0.35);
  }
  view[id.xy] = float4(color, 1.0);
}
