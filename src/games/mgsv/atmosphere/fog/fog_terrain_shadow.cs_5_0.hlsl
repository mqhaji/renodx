// SPDX-License-Identifier: MIT

// The terrain's shadow over the whole map: for each point of the height
// image, the height below which the ground toward the sun or moon hides the
// light. A point in the air is lit when it is above that height, so a fog
// cell anywhere on the map needs one lookup, however far away the hill that
// shades it is.
//
// Each texel walks across the height image toward the light, one texel a
// step. The light's ray climbs by a fixed amount each step, so ground at step
// n shades everything below its height less n climbs. The walk ends at the
// map's edge, or once the highest ground a map can hold would no longer reach
// above the shadow found so far.
//
// x: the shadow's top as the height image stores heights. y: the steps to the
// ground that casts it, over FOG_TERRAIN_SWEEP_STEPS.

#include "./fog_volume.hlsli"

Texture2D<float> heights : register(t0);
SamplerState linear_clamp : register(s0);
RWTexture2D<float2> shadow : register(u0);

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
  uint width, height;
  shadow.GetDimensions(width, height);
  if (id.x >= width || id.y >= height) return;
  const float2 texels = float2(width, height);
  const float3 toward_light = fog_object.m_localParam[1].xyz;
  const float2 spacing = FogTerrainSpacing(texels);
  const float across = length(toward_light.xz);
  float2 top = 0.0;
  // Around sunrise the game lights the fog from a sun held on the horizon
  // (seen at 06:18 in Afghanistan). A light at or below the horizon is taken
  // as on it: everything below the highest ground toward it is in shadow. A
  // light straight overhead casts none.
  if (across > 1.0e-4 && terrain_object.m_localParam[1].y > 0.0) {
    const float step_length = min(spacing.x, spacing.y);
    const float2 step_texels = toward_light.xz / across * step_length / spacing;
    const float climb = step_length * max(toward_light.y, 0.0) / across / terrain_object.m_localParam[1].y;
    [loop] for (float step_index = 1.0; step_index <= FOG_TERRAIN_SWEEP_STEPS; ++step_index) {
      const float2 uv = (float2(id.xy) + 0.5 + step_index * step_texels) / texels;
      if (any(uv < 0.0) || any(uv > 1.0) || 1.0 - step_index * climb <= top.x) break;
      const float reaches = heights.SampleLevel(linear_clamp, uv, 0.0) - step_index * climb;
      if (reaches > top.x) {
        top = float2(reaches, step_index / FOG_TERRAIN_SWEEP_STEPS);
      }
    }
  }
  shadow[id.xy] = top;
}
