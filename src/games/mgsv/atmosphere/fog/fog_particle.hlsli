// SPDX-License-Identifier: MIT
#ifndef SRC_GAMES_MGSV_ATMOSPHERE_FOG_FOG_PARTICLE_HLSLI_
#define SRC_GAMES_MGSV_ATMOSPHERE_FOG_FOG_PARTICLE_HLSLI_

// For the ports of lit particle shaders the game draws without the sun's
// shadow maps (the NS and Fast variants of Prim_Poly_Lit*): how much of the
// sun's or moon's light reaches a particle, from the fog volume's visibility
// image. The host binds this frame's image at t22 for the main view's draws
// (atmosphere/fog/volume.hpp); it has the fog volume's layout, 128 slices as
// 32 x 4 tiles, with a cell's value covering its column between its slice and
// the one before.

Texture2D<float> fog_particle_visibility : register(t22);

// pixel: SV_Position.xy. view_depth: the particle's depth from the camera.
// render_size: g_psSystem.m_renderInfo.xy. slice_scale: g_psScene.m_fogParam[1].x.
float FogParticleSunVisibility(float2 pixel, float view_depth, float2 render_size, float slice_scale, SamplerState linear_clamp) {
  // Readers place the screen's edges half a native cell inside a tile: the
  // screen spans 15/16 of the tile's width and 63/64 of its height. A tile's
  // rows run up the screen, as view space does.
  const float2 in_tile = 0.5 + (pixel / render_size - 0.5) * float2(15.0 / 16.0, -63.0 / 64.0);
  const float coordinate = clamp(128.0 * slice_scale * log2(max(view_depth, 1.0e-4)) + 0.5, 0.0, 127.0);
  const float lower = floor(coordinate);
  const float upper = min(lower + 1.0, 127.0);
  return lerp(
      fog_particle_visibility.SampleLevel(linear_clamp, (float2(lower % 32.0, floor(lower / 32.0)) + in_tile) / float2(32.0, 4.0), 0.0),
      fog_particle_visibility.SampleLevel(linear_clamp, (float2(upper % 32.0, floor(upper / 32.0)) + in_tile) / float2(32.0, 4.0), 0.0),
      coordinate - lower);
}

#endif  // SRC_GAMES_MGSV_ATMOSPHERE_FOG_FOG_PARTICLE_HLSLI_
