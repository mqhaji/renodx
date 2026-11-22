// SPDX-License-Identifier: MIT
#ifndef SRC_GAMES_MGSV_ATMOSPHERE_FOG_FOG_READ_HLSLI_
#define SRC_GAMES_MGSV_ATMOSPHERE_FOG_FOG_READ_HLSLI_

// The game's shaders read the fog volume with one bilinear lookup in each of
// two slices. A cell is 1/160 of the view wide, and a bilinear read shows the
// grid as the fog moves across it: a lamp's glow changes shape from cell to
// cell. The ports read the same place through a cubic B-spline over 4 x 4
// cells instead, in four bilinear lookups, kept inside the slice's tile so no
// other slice bleeds in. The B-spline smooths a little more than the cells
// already are and adds no detail.
float4 FogVolumeSample(Texture2D<float4> volume, SamplerState linear_clamp, float2 uv) {
  float2 size;
  volume.GetDimensions(size.x, size.y);
  // The volume is 32 x 4 tiles, one per slice. The lookups stay between the
  // centers of the tile's first and last texels.
  const float2 tiles = float2(32.0, 4.0);
  const float2 tile = min(floor(saturate(uv) * tiles), tiles - 1.0);
  const float2 first_texel = tile * size / tiles;
  const float2 last_texel = (tile + 1.0) * size / tiles - 1.0;
  // Texel i's center is at position i.
  const float2 position = uv * size - 0.5;
  const float2 base = floor(position);
  const float2 f = position - base;
  const float2 f2 = f * f;
  const float2 f3 = f2 * f;
  const float2 w0 = (1.0 - 3.0 * f + 3.0 * f2 - f3) / 6.0;
  const float2 w1 = (4.0 - 6.0 * f2 + 3.0 * f3) / 6.0;
  const float2 w2 = (1.0 + 3.0 * f + 3.0 * f2 - 3.0 * f3) / 6.0;
  const float2 w3 = f3 / 6.0;
  // Each pair of weights becomes one bilinear lookup between its two texels.
  const float2 near_weight = w0 + w1;
  const float2 far_weight = w2 + w3;
  const float2 near_uv = (clamp(base - 1.0 + w1 / near_weight, first_texel, last_texel) + 0.5) / size;
  const float2 far_uv = (clamp(base + 1.0 + w3 / far_weight, first_texel, last_texel) + 0.5) / size;
  return near_weight.y * (near_weight.x * volume.SampleLevel(linear_clamp, near_uv, 0.0) + far_weight.x * volume.SampleLevel(linear_clamp, float2(far_uv.x, near_uv.y), 0.0))
         + far_weight.y * (near_weight.x * volume.SampleLevel(linear_clamp, float2(near_uv.x, far_uv.y), 0.0) + far_weight.x * volume.SampleLevel(linear_clamp, far_uv, 0.0));
}

#endif  // SRC_GAMES_MGSV_ATMOSPHERE_FOG_FOG_READ_HLSLI_
