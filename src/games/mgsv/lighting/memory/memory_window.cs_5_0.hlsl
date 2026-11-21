#include "../reflections/common.hlsli"
#include "common.hlsli"

// Scene memory terrain window: copies the heights around the eye out of the
// engine's terrain textures, kept from a terrain draw with its constants.
// The lookup follows the terrain vertex shaders (NewTerrain_Depth_vs): a map
// coordinate picks a tile, the tile map gives that tile's page in the height
// atlas and its level, and the position inside the tile picks the texel.
// Mesh vertices sit on texel centers, so lattice points need no filtering.
// Layout of the window in common.hlsli.

// xyz: page x, page y and level, each as value / 255.
Texture2D<float4> tile_map : register(t0);
Texture2D<float> height_atlas : register(t1);
RWTexture2D<float> window : register(u0);

// cVSMaterial of a terrain draw.
cbuffer TerrainMaterial : register(b4) {
  // Tiles across the map (xy) and a page's size in atlas UV (zw).
  float4 terrain_tiles;
}
// cVSObject of a terrain draw: two matrices, m_useWeightCount, m_localParam.
cbuffer TerrainObject : register(b5) {
  float4 terrain_unused[10];
  // m_localParam[1]: the map's size in x and z, and its height range in y.
  float4 terrain_scale;
  // m_localParam[2]: the map's first corner in x and z, and its lowest height.
  float4 terrain_offset;
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
  if (any(id.xy >= TERRAIN_WINDOW)) return;
  uint atlas_width, atlas_height;
  height_atlas.GetDimensions(atlas_width, atlas_height);
  // Heights per tile side.
  const float2 page = terrain_tiles.zw * float2(atlas_width, atlas_height);
  // The window assumes the lattice the game's maps use. Constants not yet
  // copied read as zero and fail here too.
  const float2 spacing = terrain_scale.xz / max(terrain_tiles.xy * page, 1.0);
  if (!(terrain_scale.x > 0.0) || any(abs(spacing - TERRAIN_SPACING) > 1e-3)
      || any(frac(terrain_offset.xz / TERRAIN_SPACING) != 0.0)) {
    window[id.xy] = TERRAIN_NONE;
    return;
  }
  const float2 world = TerrainWindowOrigin(current_scene.eye_position.xyz) + float2(id.xy) * TERRAIN_SPACING;
  // In lattice points from the map's first corner.
  const float2 point_index = (world - terrain_offset.xz) / TERRAIN_SPACING;
  if (any(point_index < 0.0) || any(point_index >= terrain_tiles.xy * page)) {
    window[id.xy] = TERRAIN_NONE;
    return;
  }
  const uint2 lattice = uint2(point_index + 0.5);
  const uint2 points_per_tile = uint2(page + 0.5);
  const uint3 entry = uint3(tile_map.Load(int3(lattice / points_per_tile, 0)).xyz * 255.4);
  float height = TERRAIN_NONE;
  if (entry.z == 0u) {
    height = height_atlas.Load(int3(entry.xy * points_per_tile + lattice % points_per_tile, 0)) * terrain_scale.y + terrain_offset.y;
  }
  window[id.xy] = height;
}
