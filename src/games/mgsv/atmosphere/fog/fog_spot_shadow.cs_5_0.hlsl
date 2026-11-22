// SPDX-License-Identifier: MIT

// Keeps a shadowed spot light's shadow map for the fog.
//
// The game renders every shadowed spot light's shadows into one map, one
// light after another, and each light's draw reads the map while it holds
// that light. By the time the fog's passes run only the last light's shadows
// are left. So at each such draw this pass copies the map, reduced, into the
// light's tile of an atlas the fog reads later: one stored depth per block of
// the map, taken at the block's middle.

#include "./fog_lights.hlsli"

cbuffer SpotShadowTile : register(b11) {
  uint spot_shadow_tile;
}

Texture2D<float> shadow_map : register(t0);
RWTexture2D<float> atlas : register(u0);

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
  uint atlas_width, atlas_height;
  atlas.GetDimensions(atlas_width, atlas_height);
  const uint2 tile_size = uint2(atlas_width, atlas_height) / FOG_SPOT_SHADOW_TILES;
  if (id.x >= tile_size.x || id.y >= tile_size.y) return;
  uint width, height;
  shadow_map.GetDimensions(width, height);
  atlas[uint2(spot_shadow_tile % FOG_SPOT_SHADOW_TILES, spot_shadow_tile / FOG_SPOT_SHADOW_TILES) * tile_size + id.xy] =
      shadow_map.Load(int3((float2(id.xy) + 0.5) * float2(width, height) / float2(tile_size), 0));
}
