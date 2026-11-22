// SPDX-License-Identifier: MIT
#ifndef SRC_GAMES_MGSV_ATMOSPHERE_FOG_FOG_LIGHTS_HLSLI_
#define SRC_GAMES_MGSV_ATMOSPHERE_FOG_FOG_LIGHTS_HLSLI_

// The frame's local lights, as the contact-shadow batch gathers them at their
// draws (lighting/shadows/batch.hpp), and the bins the fog sorts them into.
//
// The buffer starts with a header vector whose x holds the light count as raw
// bits. Each light then has 17 vectors: the 11 of its draw's light constants
// (cPSLight), a kind vector, 4 of its shadow-map projection, and its falloff
// limits (m_materials[7] of its draw). Positions and directions are in view
// space. Of the light constants: [3] position, [4] color, [5] the direction a
// spot points, [6] falloff coefficients, [7] a spot's cone (the cosine its
// light starts at, a scale, a power), [8].y the scale of its diffuse light.
// The kind's x is 1 or 3 for a spot and 2 or 4 for a point light; 3 and 4
// fall off as 1 - k d^16 where 1 and 2 fall off as 1 - k d^4.

#include "./fog_volume.hlsli"

static const uint FOG_LIGHT_VECTORS = 17u;
static const uint FOG_LIGHT_KIND = 11u;
static const uint FOG_LIGHT_FALLOFF_LIMITS = 16u;

// A bin is this many cells a side and this many slices deep, and keeps at
// most this many lights: the ones that light it most.
static const uint FOG_BIN_CELLS = 8u;
static const uint FOG_BIN_SLICES = 4u;
static const uint FOG_BIN_LIGHTS = 12u;
static const uint FOG_BIN_END = 0xFFFFFFFFu;
// A bin entry: the light's index in its low 10 bits, then 5 bits of its kept
// shadow map's tile plus one (zero without one), and in its high 16 bits the
// light's reach (FogLightReach) as a 16-bit float. While the bins pass ranks
// a bin's lights, those bits hold the light's strength there instead.
static const uint FOG_BIN_LIGHT_MASK = 0x3FFu;
static const uint FOG_BIN_SHADOW_SHIFT = 10u;
static const uint FOG_BIN_SHADOW_MASK = 0x1Fu;

// The spot shadow maps kept for the fog: this many, as tiles of a square
// atlas this many a side. A record per tile holds the light's position and
// the four vectors of its shadow projection, copied at the light's draw; a
// tile no light took this frame has a zero projection.
static const uint FOG_SPOT_SHADOWS = 16u;
static const uint FOG_SPOT_SHADOW_TILES = 4u;
static const uint FOG_SPOT_SHADOW_VECTORS = 5u;

// A light is followed out to where its light on a surface facing it falls
// under this, in the scene's exposed units, when its falloff reaches farther.
static const float FOG_LIGHT_FLOOR = 0.004;

Buffer<float4> fog_lights : register(t7);
Buffer<float4> fog_spot_shadows : register(t9);

float4 FogLightVector(uint light, uint vector_index) {
  return fog_lights.Load(1u + light * FOG_LIGHT_VECTORS + vector_index);
}

// The bins along x and y for a volume of this many cells per slice.
uint2 FogBins(uint2 cells) {
  return (cells + FOG_BIN_CELLS - 1u) / FOG_BIN_CELLS;
}

// How far from a light the fog follows it: to where its falloff ends, or
// sooner, to where it is too dim to show. The fog fades the light out toward
// this distance with the light's falloff curve, so nothing ends abruptly.
float FogLightReach(uint light, uint type) {
  const float4 falloff = FogLightVector(light, 6u);
  const float coefficient = type >= 3u ? falloff.z : falloff.w;
  const float3 color = FogLightVector(light, 4u).xyz * (FogLightVector(light, 8u).y * fog_scene.m_exposure.z);
  float reach = sqrt(max(color.r, max(color.g, color.b)) / FOG_LIGHT_FLOOR);
  // A coefficient too small to end the falloff leaves only the dimness limit.
  if (coefficient > 1.0e-30) {
    reach = min(reach, type >= 3u ? pow(coefficient, -1.0 / 16.0) : pow(coefficient, -0.25));
  }
  return reach;
}

#endif  // SRC_GAMES_MGSV_ATMOSPHERE_FOG_FOG_LIGHTS_HLSLI_
