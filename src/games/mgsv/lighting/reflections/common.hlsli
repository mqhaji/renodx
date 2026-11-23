// SPDX-License-Identifier: MIT
#ifndef MGSV_REFLECTIONS_COMMON_HLSLI_
#define MGSV_REFLECTIONS_COMMON_HLSLI_

#include "../../materials/brdf.hlsli"
#include "../../third_party/nrd/NRD.hlsli"

// Shared declarations for the enhanced reflection passes.
//
// The game's scene constants (480 bytes), copied on the GPU from the deferred
// composition draw: prepare_reflections exports the rows and the host copies
// them into this buffer. They describe the submitted scene, not the current CPU
// camera, which a render thread may already have moved to another view.
struct ReflectionScene {
  float4x4 projection_view;
  float4x4 projection;
  row_major float4x4 view_to_world;
  float4x4 shadow_projection;
  float4x4 shadow_projection2;
  float4 eye_position;
  float4 projection_parameters;
  float4 viewport_size;
  float4 exposure;
  float4 fog_parameters[3];
  float4 fog_color;
  float4 camera_center_offset;
  float4 shadow_map_resolutions;
};

cbuffer ReflectionConstants : register(b0) {
  uint2 reflection_size;
  uint reflection_sample;
  uint maximum_mip;
  float2 current_jitter;
  float environment_occlusion_strength;
  // Bit 0: history continues from the previous camera. Bit 1: the NRD
  // Validation debug view is active.
  uint reflection_flags;
  row_major float4x4 current_to_previous_clip;
  float3 hit_distance_parameters;
  float velocity_projection_jitter_scale;
  uint reflection_rays_per_pixel;
};
cbuffer CurrentReflectionScene : register(b1) { ReflectionScene current_scene; }

// How deep a surface is taken to be behind the depth the camera sees, in game
// units: the slab a reflection ray can hit, and how far in front of a surface
// a forward-drawn layer still counts as part of it.
static const float REFLECTION_SURFACE_THICKNESS = 0.05;

// Positive linear view depth from native reverse-Z depth.
float GetReflectionDepth(float depth) {
  return current_scene.projection_parameters.z / max(depth - current_scene.projection_parameters.w, 1e-7);
}

float3 GetReflectionPosition(float2 pixel, float depth) {
  // The saved native camera center includes projection jitter. Subtracting it
  // gives the true camera ray through this depth pixel.
  const float2 ndc = (pixel / float2(reflection_size) * 2.0 - 1.0) * float2(1.0, -1.0);
  const float z = GetReflectionDepth(depth);
  return float3((ndc - current_scene.camera_center_offset.xy) * current_scene.projection_parameters.xy * z, z);
}

// Decodes the game's G-buffer normal (view space). z is stored as
// sqrt((z + 1) / 2); xy stores only the direction of the normal's xy part, and
// its length is rebuilt from z.
float3 DecodeReflectionNormal(float3 encoded) {
  float3 normal;
  normal.z = encoded.z * encoded.z * 2.0 - 1.0;
  const float radius_squared = saturate(1.0 - normal.z * normal.z);
  const float2 xy = encoded.xy * 2.0 - 1.0;
  normal.xy = xy * (radius_squared * rsqrt(radius_squared * dot(xy, xy) + 1e-7));
  return normalize(normal);
}

#endif
