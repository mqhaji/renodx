// SPDX-License-Identifier: MIT
#ifndef MGSV_MEMORY_COMMON_HLSLI_
#define MGSV_MEMORY_COMMON_HLSLI_

// Scene memory samples: each frame, one pixel from each tile of a 128 x 64
// grid over the render. Each sample takes two texels of a 256 x 64
// RGBA32_UINT image, at (2 x, y) and (2 x + 1, y):
//   first:  xyz world position relative to the camera (float bits), w flags
//   second: y albedo (8:8:8, square root of linear), z sun visibility (0 to
//           255); x and w unused
static const uint2 MEMORY_SAMPLE_GRID = uint2(128u, 64u);
static const uint MEMORY_SAMPLE_COUNT = MEMORY_SAMPLE_GRID.x * MEMORY_SAMPLE_GRID.y;

// The pixel holds geometry: depth in front of the sky and a decodable normal.
static const uint MEMORY_SAMPLE_VALID = 1u;
// The object-velocity mask marks the pixel: the engine moved its model this frame.
static const uint MEMORY_SAMPLE_MOVING = 2u;
// The sun draw sampled a cascade atlas this frame (set on every sample).
static const uint MEMORY_SAMPLE_ATLAS = 4u;
// A cascade covers the sample, so its sun visibility is measured.
static const uint MEMORY_SAMPLE_SUN_COVERED = 8u;
// The surface faces the sun or moon.
static const uint MEMORY_SAMPLE_SUN_FACING = 16u;

// Scene memory state, one R32_UINT word per texel of a 64 x 1 image the
// D3D12 passes keep. Sample counts are for the latest frame.
static const uint MEMORY_STATE_VALID = 0u;
static const uint MEMORY_STATE_MOVING = 1u;
static const uint MEMORY_STATE_ATLAS = 2u;
static const uint MEMORY_STATE_COVERED = 3u;
static const uint MEMORY_STATE_FACING = 4u;
// Half-degree steps (SUN_EPOCH_ANGLE) the light direction has turned: each
// time it has turned more than one step from the epoch's direction, the
// count grows by the whole steps turned, so a switch between sun and moon
// adds hundreds.
static const uint MEMORY_STATE_SUN_EPOCH = 5u;
static const uint MEMORY_STATE_SUN_DIRECTION = 6u;     // 3 words, world space, toward the light
static const uint MEMORY_STATE_EYE = 9u;               // 3 words, absolute world position
static const uint MEMORY_STATE_EPOCH_DIRECTION = 12u;  // 3 words, the direction the epoch began with
// Distance between the scene block's eye and the translation of its camera
// matrix; both should name the same point.
static const uint MEMORY_STATE_EYE_MISMATCH = 15u;
static const uint MEMORY_STATE_FRAMES = 16u;
// Near cells per level (4 words), kept by marking and scrolling.
static const uint MEMORY_STATE_NEAR_CELLS = 17u;
// Samples the mark pass stored this frame, and whether marking ran.
static const uint MEMORY_STATE_MARKED = 21u;
static const uint MEMORY_STATE_MARKING = 22u;
// Cells the carve pass emptied this frame.
static const uint MEMORY_STATE_CARVED = 23u;
// Each level's origin, the world cell index of its window's first cell (4 x
// 3 words, int), and the origins the frame began with, for the scroll pass.
static const uint MEMORY_STATE_ORIGIN = 24u;
static const uint MEMORY_STATE_PREVIOUS_ORIGIN = 36u;
// Known (not unknown) cells per level (4 words), counted while the census
// flag is set, after this update's scrolling.
static const uint MEMORY_STATE_KNOWN_CELLS = 48u;
// Samples that wrote surface color and sun visibility this frame.
static const uint MEMORY_STATE_ALBEDO_WRITTEN = 52u;
static const uint MEMORY_STATE_SUN_WRITTEN = 53u;
// Cells the terrain pass found within the band or made solid this frame.
static const uint MEMORY_STATE_TERRAIN_CELLS = 54u;
// For the probe: the camera's forward direction (3 words, world space) and
// the scene's exposure scale, which tells day from night.
static const uint MEMORY_STATE_FORWARD = 55u;
static const uint MEMORY_STATE_EXPOSURE = 58u;
static const uint MEMORY_STATE_WORDS = 64u;

static const float SUN_EPOCH_ANGLE = 0.5 * 0.01745329252;
// A stored sun visibility taken more than this many steps ago, doubled per
// level, is old: a shadow edge drifts by the caster's distance times the
// angle turned, about one finest half cell for a caster 15 units away. Old
// values stay stored with their epoch; a new sample replaces one instead of
// blending with it, and readers decide how far to trust it.
static const uint SUN_OLD_STEPS = 4u;

// Scene memory grids: four nested levels of 128 x 64 x 128 cells (x, height,
// z) with cells of 0.25, 0.5, 1 and 2 units, stacked along z in one
// 128 x 64 x 512 R32_UINT texture. A level's window starts at its origin and
// wraps, so world cell c lives at texel c modulo the level size.
static const uint3 MEMORY_LEVEL_SIZE = uint3(128u, 64u, 128u);
static const uint MEMORY_LEVELS = 4u;
static const float MEMORY_FINEST_CELL = 0.25;
// Cell values, ordered so marking keeps the smaller with InterlockedMin:
// unknown, then known empty, then solid (below the terrain, beyond the band),
// then near a surface, which holds the distance from the cell center to the
// nearest sample or to the terrain in 1/1024 cells (bits 31..16). Values
// below MEMORY_SOLID are near.
static const uint MEMORY_UNKNOWN = 0xFFFFFFFFu;
static const uint MEMORY_EMPTY = 0xFFFE0000u;
static const uint MEMORY_SOLID = 0xFFFD0000u;
// Marking reaches cells within this many cells of a sample.
static const float MEMORY_BAND = 2.0;

// Surface color and sun visibility: two 64 x 32 x 256 R32_UINT textures at
// half the resolution of the distance grids, with the same origins (always
// even), so half cell h covers cells 2 h and 2 h + 1 of its level.
//   color: rgb 8:8:8 (square root of linear, as the samples) and a sample
//          count in bits 31..24; 0 holds nothing
//   sun:   visibility 0 to 255 (bits 7..0), the epoch it was taken at
//          (bits 23..8, the low 16 bits of the count), and 1 << 24 once set
static const uint3 MEMORY_HALF_SIZE = MEMORY_LEVEL_SIZE / 2u;
static const uint MEMORY_SUN_SET = 1u << 24u;

// Constants of every scene memory pass (b2).
cbuffer MemoryConstants : register(b2) {
  uint memory_flags;
  // Advances once per update; the sample pass and the mark pass hash it.
  uint memory_frame;
  // The lighting debug view the inspect pass draws.
  uint memory_view;
}
// Forget every cell and recenter every level (first update, after a gap).
static const uint MEMORY_FLAG_CLEAR = 1u;
// Game time is stopped, or stopped within the last few updates; nothing is
// marked.
static const uint MEMORY_FLAG_PAUSED = 2u;
// The pause word did not verify; a frame without moving samples marks nothing.
static const uint MEMORY_FLAG_PAUSE_UNKNOWN = 4u;
// Count the known cells of every level (the probe asks for it).
static const uint MEMORY_FLAG_CENSUS = 8u;

// Terrain window: heights the D3D11 window pass copies out of the engine's
// terrain textures each update, for the D3D12 terrain pass: R32_FLOAT, 256 x
// 256 points of the terrain's 2-unit lattice (points sit on even world
// coordinates), centered on the eye. Each texel holds the world height where
// the engine has it at full resolution, and TERRAIN_NONE elsewhere (outside
// the map, or a coarse whole-map page).
static const uint TERRAIN_WINDOW = 256u;
static const float TERRAIN_SPACING = 2.0;
static const float TERRAIN_NONE = -1e30;

// World xz of the window's first point.
float2 TerrainWindowOrigin(float3 eye) {
  return (floor(eye.xz / TERRAIN_SPACING) - float(TERRAIN_WINDOW / 2u)) * TERRAIN_SPACING;
}

// The view-space direction toward the sun or moon, from the scene's shadow
// matrices: stored depth grows toward the light along the projection's z axis
// (cascade 0 has scale 1), and the depth scale's sign gives the way.
float3 SunTowardLight(float4x4 shadow_projection, float4x4 cascades) {
  return normalize(shadow_projection._m02_m12_m22) * (cascades._m30 >= 0.0 ? 1.0 : -1.0);
}

float MemoryCellSize(uint level) { return MEMORY_FINEST_CELL * float(1u << level); }

uint3 MemoryTexel(int3 cell, uint level) {
  const int3 size = int3(MEMORY_LEVEL_SIZE);
  const int3 wrapped = (cell % size + size) % size;
  return uint3(wrapped.xy, level * MEMORY_LEVEL_SIZE.z + uint(wrapped.z));
}

uint3 MemoryHalfTexel(int3 half_cell, uint level) {
  const int3 size = int3(MEMORY_HALF_SIZE);
  const int3 wrapped = (half_cell % size + size) % size;
  return uint3(wrapped.xy, level * MEMORY_HALF_SIZE.z + uint(wrapped.z));
}

// Steps between a stored epoch (16 bits) and the current count.
uint SunEpochAge(uint stored, uint current) { return (current - stored) & 0xFFFFu; }

int3 LoadMemoryOrigin(RWTexture2D<uint> state, uint word) {
  return int3(asint(state[uint2(word, 0u)]), asint(state[uint2(word + 1u, 0u)]), asint(state[uint2(word + 2u, 0u)]));
}

uint3 MemoryHash(uint3 value) {
  // PCG3D (Jarzynski and Olano, "Hash Functions for GPU Rendering", 2020).
  value = value * 1664525u + 1013904223u;
  value.x += value.y * value.z;
  value.y += value.z * value.x;
  value.z += value.x * value.y;
  value ^= value >> 16u;
  value.x += value.y * value.z;
  value.y += value.z * value.x;
  value.z += value.x * value.y;
  return value;
}

#endif
