#include "../reflections/common.hlsli"
#include "common.hlsli"

// Scene memory carving: points along camera rays, in front of the surface each
// ray shows, empty the cell around them. A cell is emptied only when its
// bounding sphere lies wholly in front of the nearest depth over its screen
// footprint; emptying replaces unknown cells and surfaces that are gone alike
// (an opened door). The footprint is capped at MAX_FOOTPRINT pixels around the
// cell's center and clipped to the screen: near the camera a cell covers
// hundreds of pixels, which nearly always hold something nearer, so uncapped
// it would never empty. A surface the cap leaves out gets marked again by its
// own samples, since the mark pass runs next. Growing the sphere by the band
// would fail the same way.
// Moving objects need no exception: they hide what is behind them but are
// never stored. Each point empties the finest level that holds it, and with a
// chance of one in eight the next level too. Runs every update, paused or not.

Texture2D<float> depth : register(t0);
RWTexture2D<uint> state : register(u0);
RWTexture3D<uint> grid : register(u1);

// One point per tile of this grid over the render, each update.
static const uint2 CARVE_GRID = uint2(256u, 128u);
// Points start this far from the eye; cells closer than it are not tested.
static const float NEAREST_POINT = 0.25;
// Beyond the coarsest window's reach.
static const float FARTHEST_POINT = 512.0;
// Half the diagonal of a cell, in cells.
static const float CELL_HALF_DIAGONAL = 0.8660254;
// Footprint radius limit, in pixels.
static const float MAX_FOOTPRINT = 16.0;

float ViewDepth(float native) { return (native > 0.0 ? GetReflectionDepth(native) : 1e30); }

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
  if (any(id.xy >= CARVE_GRID)) return;
  const uint3 random = MemoryHash(uint3(id.xy, memory_frame + 0x9E3779B9u));
  const float2 jitter = float2(random.xy >> 8u) * (1.0 / 16777216.0);
  const float2 tile = float2(reflection_size) / float2(CARVE_GRID);
  const int2 pixel = int2(min(uint2((float2(id.xy) + jitter) * tile), reflection_size - 1u));
  const float farthest = min(ViewDepth(depth.Load(int3(pixel, 0))), FARTHEST_POINT);
  if (farthest <= NEAREST_POINT) return;
  // Log-uniform in depth, so each level gets about the same share of points.
  const float fraction = float(random.z >> 8u) * (1.0 / 16777216.0);
  const float z = NEAREST_POINT * pow(farthest / NEAREST_POINT, fraction);
  const float2 ndc = ((float2(pixel) + 0.5) / float2(reflection_size) * 2.0 - 1.0) * float2(1.0, -1.0);
  const float3 view_ray = float3((ndc - current_scene.camera_center_offset.xy) * current_scene.projection_parameters.xy, 1.0);
  const float3 relative = mul((float3x3)current_scene.view_to_world, view_ray * z);
  const float3 eye = current_scene.eye_position.xyz;
  const float2 focal = float2(reflection_size) * 0.5 / abs(current_scene.projection_parameters.xy);
  const bool second_level = (random.z & 7u) == 0u;
  uint levels_tested = 0u;
  for (uint level = 0u; level < MEMORY_LEVELS; ++level) {
    const float cell_size = MemoryCellSize(level);
    const int3 origin = LoadMemoryOrigin(state, MEMORY_STATE_ORIGIN + 3u * level);
    const float3 eye_cells = (eye - float3(origin) * cell_size) / cell_size;
    const float3 position = eye_cells + relative / cell_size;
    if (any(position < 0.0) || any(position >= float3(MEMORY_LEVEL_SIZE))) continue;
    ++levels_tested;
    const int3 cell = int3(floor(position));
    // The cell's center in view space; mul(v, M) applies the transpose of
    // the view-to-world rotation.
    const float3 center = mul((float3(cell) + 0.5 - eye_cells) * cell_size, (float3x3)current_scene.view_to_world);
    const float radius = CELL_HALF_DIAGONAL * cell_size;
    if (center.z - radius > NEAREST_POINT) {
      const float2 center_ndc = center.xy / (current_scene.projection_parameters.xy * center.z) + current_scene.camera_center_offset.xy;
      const float2 center_pixel = (center_ndc * float2(0.5, -0.5) + 0.5) * float2(reflection_size);
      const float extent = min(radius / (center.z - radius) * max(focal.x, focal.y), MAX_FOOTPRINT);
      const float2 low = max(center_pixel - extent, 0.0);
      const float2 high = min(center_pixel + extent, float2(reflection_size) - 1.0);
      if (all(low <= high)) {
        // At this level the footprint spans at most 2 x 2 texels.
        const uint mip = min(uint(ceil(log2(max(2.0 * extent, 1.0)))), maximum_mip);
        const int2 last_texel = int2(max(reflection_size >> mip, 1u)) - 1;
        const int2 first = min(int2(floor(low / exp2(float(mip)))), last_texel);
        const int2 last = min(int2(floor(high / exp2(float(mip)))), last_texel);
        float nearest = 0.0;
        for (int y = first.y; y <= last.y; ++y) {
          for (int x = first.x; x <= last.x; ++x) {
            nearest = max(nearest, depth.Load(int3(x, y, mip)));
          }
        }
        if (center.z + radius < ViewDepth(nearest)) {
          uint before;
          InterlockedExchange(grid[MemoryTexel(origin + cell, level)], MEMORY_EMPTY, before);
          if (before != MEMORY_EMPTY) InterlockedAdd(state[uint2(MEMORY_STATE_CARVED, 0u)], 1u);
          if (before < MEMORY_SOLID) InterlockedAdd(state[uint2(MEMORY_STATE_NEAR_CELLS + level, 0u)], 0xFFFFFFFFu);
        }
      }
    }
    if (levels_tested == 2u || !second_level) break;
  }
}
