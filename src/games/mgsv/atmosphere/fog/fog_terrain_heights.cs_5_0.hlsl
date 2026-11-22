// SPDX-License-Identifier: MIT

// The map's ground heights as one image, for the terrain shadow pass.
//
// The engine keeps the ground as pages of 16-bit heights in an atlas, at full
// detail around the player and coarser pages for the rest of the map. Every
// terrain draw binds the atlas and a tile map that gives each tile's page and
// level, and its vertex shader (NewTerrain_Depth_vs) looks a height up through
// them. This pass does the same lookup for a grid of points across the whole
// map. Heights stay as the atlas stores them: a share of the map's height
// range above its lowest point.

#include "./fog_volume.hlsli"

// xyz: a tile's page column, page row and level, each as value / 255.
Texture2D<float4> tile_map : register(t0);
Texture2D<float> height_atlas : register(t1);
RWTexture2D<float> heights : register(u0);

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
  uint width, height;
  heights.GetDimensions(width, height);
  if (id.x >= width || id.y >= height) return;
  uint atlas_width, atlas_height;
  height_atlas.GetDimensions(atlas_width, atlas_height);
  uint tiles_width, tiles_height;
  tile_map.GetDimensions(tiles_width, tiles_height);
  // A page holds this many heights a side and covers one tile at level 0,
  // twice as many a side with each level.
  const uint2 page = uint2(terrain_tiles.zw * float2(atlas_width, atlas_height) + 0.5);
  const uint2 tiles = uint2(terrain_tiles.xy + 0.5);
  // Constants not yet copied read as zero. The lowest height casts no shadow.
  if (any(page == 0u) || any(tiles == 0u) || tiles.x > tiles_width || tiles.y > tiles_height) {
    heights[id.xy] = 0.0;
    return;
  }
  // The texel's point, counted in level 0 heights from the map's first corner.
  const uint2 point_index = id.xy * (tiles * page) / uint2(width, height);
  const uint3 entry = uint3(tile_map.Load(int3(point_index / page, 0)).xyz * 255.4);
  heights[id.xy] = height_atlas.Load(int3(entry.xy * page + (point_index >> entry.z) % page, 0));
}
