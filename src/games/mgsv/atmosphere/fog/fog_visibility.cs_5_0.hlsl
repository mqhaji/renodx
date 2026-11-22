// SPDX-License-Identifier: MIT

// How much of each fog cell the sun or moon reaches this frame: 1 where its
// light arrives, 0 in shadow. A cell covers its column between its slice and
// the one before.
//
// Each cell takes a few points spread through it, at places that change every
// frame, and asks two things of each: the light's cascade shadow maps, as the
// game shadows a surface there, and the terrain shadow image, which covers
// the whole map. The result is one frame's estimate; the volume pass blends
// it over time.

#include "./fog_volume.hlsli"

// The shadowed sun or moon draw's object constants: the cloud shadow's
// projection ([0] and [1]), threshold and strength ([3].z and [3].w).
cbuffer SunDrawObject : register(b7) {
  FogObject sun_object;
}

Texture2D<float> shadow_map : register(t0);
Texture2D<float4> cloud_shadow : register(t1);
Texture2D<float4> world_shadow_mask : register(t5);
Texture2D<float2> terrain_shadow : register(t6);
SamplerState linear_clamp : register(s0);
SamplerComparisonState less_linear_clamp : register(s1);
SamplerState linear_wrap : register(s2);
RWTexture2D<float> estimate : register(u0);

static const uint TAPS = 4u;
// A terrain shadow fades in over at least this many world units of height
// below its top: the height image's points are far apart and the ground
// between them is a guess. At and above the top a point is fully lit, so
// fog over open ground stays the game's.
static const float TERRAIN_EDGE = 8.0;
// The sun's and the moon's width in the sky, in radians. The fade widens by
// this much of the distance to the ground that casts the shadow.
static const float LIGHT_WIDTH = 0.0093;

// Three values in [0, 1) per cell (Jarzynski and Olano's pcg3d).
float3 CellRandom(uint2 cell) {
  uint3 v = uint3(cell, 0x9E3779B9u) * 1664525u + 1013904223u;
  v.x += v.y * v.z;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  v ^= v >> 16u;
  v.x += v.y * v.z;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  return float3(v) * (1.0 / 4294967296.0);
}

// The share of the sun's or moon's light reaching a view-space point, as the
// game's shadowed sun shaders (SunLight_DL_CS, SunLight_DL_CS_ES and
// SSLighting2_Sun_LargestCascade) shadow a surface there. They take the first
// cascade whose square holds the point and whose depth range does not lie
// toward the light from it; the last cascade is their region's edge, beyond
// which the game lights surfaces without shadow maps. The shadow is faded out
// toward the sphere the shadows end at: to lit, or for LargestCascade to its
// world shadow mask. Then the cloud shadow. The cascade lookup is one
// comparison where those shaders take four. The terms those shaders add that
// do not shadow (a visibility offset, a fade with view depth, a least
// visibility for specular light) are left out, so fog in open sun stays the
// game's.
float SunVisibility(float3 view_position) {
  const float3 projected = mul(float4(view_position, 1.0), sun_scene.m_shadowProjection).xyz;
  uint width, height;
  shadow_map.GetDimensions(width, height);
  for (uint cascade = 0u; cascade < 4u; ++cascade) {
    const float4 transform = cascade == 0u ? float4(0.0, 0.0, 0.0, 1.0) : sun_scene.m_shadowProjection2[max(cascade, 1u) - 1u];
    const float3 clip = projected * transform.w + transform.xyz;
    // Stored depth grows toward the light, so a larger stored depth blocks.
    const float depth = (1.0 + clip.z) * sun_scene.m_shadowProjection2._m30;
    if (1.0 - max(abs(clip.x), abs(clip.y)) <= 4.0 / float(width) || depth <= 0.0) continue;
    const float2 uv = float2(cascade & 1u, cascade >> 1u) * 0.5 + 0.25 + clip.xy * float2(0.25, -0.25) + 0.5 / float2(width, height);
    float lit = 1.0 - shadow_map.SampleCmpLevelZero(less_linear_clamp, uv, depth);
    lit *= lit;
    const float3 from_center = view_position - float3(0.0, 0.0, sun_light_params[8].x);
    const float fade = from_center.z >= 0.0 ? saturate(sun_light_params[6].x * max(length(from_center) - sun_light_params[8].y, 0.0)) : 0.0;
    float beyond = 1.0;
    if ((fog_flags & FOG_FLAG_WORLD_SHADOW_MASK) != 0u) {
      beyond = world_shadow_mask.SampleLevel(linear_wrap, sun_light_params[3].zw * mul(float4(view_position, 1.0), sun_scene.m_view).xz + 0.5, 0.0).x;
    }
    lit = saturate(lit + fade * fade * (beyond - lit));
    if ((fog_flags & FOG_FLAG_CLOUD_SHADOW) != 0u) {
      const float cloud = cloud_shadow.SampleLevel(
          linear_wrap, float2(dot(sun_object.m_localParam[0], float4(view_position, 1.0)), dot(sun_object.m_localParam[1], float4(view_position, 1.0))), 0.0).x;
      lit *= saturate(smoothstep(0.0, 1.0, cloud / max(sun_object.m_localParam[3].z, 1.0e-6)) * sun_object.m_localParam[3].w);
    }
    return lit;
  }
  return 1.0;
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
  uint width, height;
  estimate.GetDimensions(width, height);
  if (id.x >= width || id.y >= height) return;

  // 1. Whose shadows these are. Around dawn and dusk the shadow maps belong
  // to one body (the moon, or a sun the engine holds well above the horizon)
  // while the fog is lit from another direction (the rising or setting sun).
  // The maps' shadows apply in full while the two directions are within 30
  // degrees and not at all beyond 45. The terrain's shadows are always cast
  // along the fog's light direction.
  const float3 toward_light = fog_object.m_localParam[1].xyz;
  const float from_maps = (fog_flags & FOG_FLAG_SHADOW_MAPS) != 0u
                              ? fog_shadow_strength * smoothstep(0.7071, 0.8660, dot(toward_light, normalize(mul(float4(sun_light_params[0].xyz, 0.0), sun_scene.m_view).xyz)))
                              : 0.0;
  const float from_terrain = (fog_flags & FOG_FLAG_TERRAIN) != 0u ? fog_shadow_strength : 0.0;
  if (from_maps <= 0.0 && from_terrain <= 0.0) {
    estimate[id.xy] = 1.0;
    return;
  }

  const float2 cells = float2(width, height) / FOG_TILES;
  const float2 tile = floor(float2(id.xy) / cells);
  const float2 cell = float2(id.xy) - tile * cells;
  const float slice = tile.y * FOG_TILES.x + tile.x;
  const float far_depth = FogSliceDepth(slice);
  const float near_depth = slice == 0.0 ? 0.0 : FogSliceDepth(slice - 1.0);

  uint terrain_width, terrain_height;
  terrain_shadow.GetDimensions(terrain_width, terrain_height);
  const float2 terrain_texels = float2(terrain_width, terrain_height);
  const float2 terrain_spacing = FogTerrainSpacing(terrain_texels);

  // 2. The points move every frame along a low-discrepancy sequence (Roberts'
  // R3), offset per cell.
  const float3 offset = frac(CellRandom(id.xy) + float(fog_frame & 1023u) * float3(0.8191725134, 0.6710436067, 0.5497004779));
  float lit = 0.0;
  for (uint tap = 0u; tap < TAPS; ++tap) {
    // A second sequence spreads the taps across the cell while the first
    // coordinate strides its depth.
    const float2 across = frac(offset.xy + float(tap) * float2(0.7548776662, 0.5698402910));
    const float3 view_position = FogViewPosition(FogClip((cell + across) / cells), lerp(near_depth, far_depth, (float(tap) + offset.z) / float(TAPS)));
    float reached = 1.0;
    if (from_maps > 0.0) {
      reached = lerp(1.0, SunVisibility(view_position), from_maps);
    }
    if (from_terrain > 0.0) {
      // A point is lit at and above the shadow's top at its place on the
      // map. Past the map's edge nothing is known to shade it.
      const float3 world_position = mul(float4(view_position, 1.0), fog_scene.m_view).xyz;
      const float2 map_fraction = (world_position.xz - terrain_object.m_localParam[2].xz) / terrain_object.m_localParam[1].xz;
      if (all(map_fraction >= 0.0) && all(map_fraction <= 1.0)) {
        const float2 top = terrain_shadow.SampleLevel(linear_clamp, map_fraction + 0.5 / terrain_texels, 0.0);
        const float edge = TERRAIN_EDGE + top.y * FOG_TERRAIN_SWEEP_STEPS * min(terrain_spacing.x, terrain_spacing.y) * LIGHT_WIDTH;
        reached *= lerp(1.0, smoothstep(-edge, 0.0, world_position.y - (top.x * terrain_object.m_localParam[1].y + terrain_object.m_localParam[2].y)), from_terrain);
      }
    }
    lit += reached;
  }
  estimate[id.xy] = lit / float(TAPS);
}
