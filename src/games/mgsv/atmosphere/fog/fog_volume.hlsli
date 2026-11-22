// SPDX-License-Identifier: MIT
#ifndef SRC_GAMES_MGSV_ATMOSPHERE_FOG_FOG_VOLUME_HLSLI_
#define SRC_GAMES_MGSV_ATMOSPHERE_FOG_FOG_VOLUME_HLSLI_

// The fog volume's layout and the constants the fog passes read.
//
// The volume holds 128 depth slices as a 32 x 4 grid of tiles. Readers address
// a tile by fractions that put the screen's edges half a native cell (1/32 and
// 1/128 of the tile) inside it, whatever the tile's resolution; cells beyond
// lie outside the screen. Slice s is at view depth 2^(s / 128 / k), with k in
// m_fogParam[1].x, and holds the light gathered from the camera to that depth.

static const float2 FOG_TILES = float2(32.0, 4.0);
static const float FOG_SLICES = 128.0;
static const float2 FOG_CLIP_SCALE = float2(16.0 / 15.0, 64.0 / 63.0);

static const uint FOG_FLAG_SHADOW_MAPS = 1u;   // The view's sun or moon draw has cascade shadow maps.
static const uint FOG_FLAG_CLOUD_SHADOW = 2u;  // And a cloud shadow texture.
static const uint FOG_FLAG_HISTORY = 4u;       // Last frame's visibility is usable.
static const uint FOG_FLAG_WORLD_SHADOW_MASK = 8u;  // That draw fades its shadows to a world shadow mask.
static const uint FOG_FLAG_TERRAIN = 16u;  // The view's map has terrain and its shadow image is filled.
static const uint FOG_FLAG_LOCAL_LIGHTS = 32u;  // The view's local lights are gathered and sorted into bins.
static const uint FOG_FLAG_TERRAIN_MAP = 64u;  // The view's map has terrain, and the terrain draw's constants describe it.

// The terrain shadow image stores how far away the ground that casts each
// shadow is, in sweep steps over this count.
static const float FOG_TERRAIN_SWEEP_STEPS = 512.0;

// The game's scene constants (cPSScene), as the fog draw and the sun draw bind
// them. Matrices are used with the vector on the left, as the game's shaders do.
struct FogScene {
  float4x4 m_projectionView;
  float4x4 m_projection;
  float4x4 m_view;  // View space to world space.
  float4x4 m_shadowProjection;
  float4x4 m_shadowProjection2;
  float4 m_eyepos;
  float4 m_projectionParam;
  float4 m_viewportSize;
  float4 m_exposure;
  float4 m_fogParam[3];
  float4 m_fogColor;
  float4 m_cameraCenterOffset;
  float4 m_shadowMapResolutions;
};

// The game's object constants (cPSObject and cVSObject).
struct FogObject {
  float4x4 m_viewWorld;
  float4x4 m_world;
  float4 m_useWeightCount;
  float4 m_localParam[4];
};

// From the fog draw.
cbuffer FogDrawSystem : register(b0) {
  float4 m_param;
  float4 m_renderInfo;
  float4 m_renderBuffer;
  float4 m_dominantLightDir;
}

cbuffer FogDrawScene : register(b2) {
  FogScene fog_scene;
}

// The fog material. [0]: density, constant share of the forward lobe, start
// distance, sun intensity. [1]: sky color, distance exponent. [2]: forward
// scattering, lobe sharpness. [3]: blue scattering, sky-lit scattering.
// [4].xyw: the rain-occlusion map's center and top. [7]: self-luminance.
cbuffer FogDrawMaterial : register(b4) {
  float4 fog_materials[8];
}

// m_localParam[0] is the eye position, [1] the direction toward the sun or
// moon as the sky draws it.
cbuffer FogDrawObject : register(b5) {
  FogObject fog_object;
}

// From the shadowed sun or moon draw. Its light constants hold the direction
// toward the light in view space ([0]), the sphere the shadows end at ([8].x
// is its center's view depth, [8].y its radius, [6].x the fade's rate) and,
// for SSLighting2_Sun_LargestCascade, the world shadow mask's scale ([3].zw).
// Its scene constants hold the cascade matrices.
cbuffer SunDrawLight : register(b3) {
  float4 sun_light_params[11];
}

cbuffer SunDrawScene : register(b6) {
  FogScene sun_scene;
}

// From a terrain draw's vertex stage. The material's first vector holds the
// map's tiles across (xy) and a page's share of the height atlas (zw). The
// object's m_localParam[1] is the map's size in x and z with its height range
// in y, and [2] the map's first corner in x and z with its lowest height in y.
cbuffer TerrainDrawMaterial : register(b1) {
  float4 terrain_tiles;
}

cbuffer TerrainDrawObject : register(b8) {
  FogObject terrain_object;
}

cbuffer FogPass : register(b10) {
  uint fog_frame;  // Counts the pass's executions.
  uint fog_flags;
  float fog_shadow_strength;  // The Fog Shadow Strength setting, 0 to 1.
  uint fog_debug_view;        // The Fog Debug View setting.
  float fog_local_lights;     // The Local Lights in Fog setting, 0 to 1.
  uint fog_cells_x;           // The volume's cells per slice.
  uint fog_cells_y;
  float fog_height_falloff;   // The Fog Height Falloff setting: the share of the fog lost per unit of height.
  float fog_time;             // Seconds, wrapped to a few hours, for the drift of uneven fog.
  float fog_unevenness;       // The Fog Unevenness setting, 0 to 1.
}

// The clip position (before the perspective divide by view depth) of a point
// at fractions (0 to 1) of a tile.
float2 FogClip(float2 tile_fraction) {
  return (tile_fraction * 2.0 - 1.0) * FOG_CLIP_SCALE;
}

float FogSliceDepth(float slice) {
  return exp2(slice / FOG_SLICES / fog_scene.m_fogParam[1].x);
}

// The view-space position at a clip position and view depth.
float3 FogViewPosition(float2 clip, float view_depth) {
  return float3(clip * fog_scene.m_projectionParam.xy * view_depth, view_depth);
}

// The world distance between two neighboring texels of a terrain image of
// this many texels. Texel (i, j) lies at the map's first corner plus (i, j)
// times this.
float2 FogTerrainSpacing(float2 texels) {
  return terrain_object.m_localParam[1].xz / texels;
}

#endif  // SRC_GAMES_MGSV_ATMOSPHERE_FOG_FOG_VOLUME_HLSLI_
