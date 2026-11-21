#include "../../shared.h"
#include "../../materials/material_source.hlsli"
#include "../reflections/common.hlsli"
#include "../shadows/pcss.hlsli"
#include "common.hlsli"

// Scene memory samples, taken at the reflection composite right after
// reflection preparation, with its inputs still bound. One pixel per tile,
// chosen anew each frame, is turned into a world-space sample: position,
// albedo, whether the engine moved it this frame, whether it faces the light,
// and how much sun the cascade atlas gives it. Layout in common.hlsli.

Texture2D<float4> native_albedo : register(t0);
Texture2D<float4> native_normal : register(t2);
Texture2D<float4> native_material : register(t3);
Texture2D<float> native_depth : register(t4);
Texture2D<float4> clean_material : register(t5);
Texture2D<uint4> material_owner : register(t6);
Texture2D<float4> object_velocity : register(t11);
// The sun's cascade atlas (R24 depth view); unbound when the sun draw used none.
Texture2D<float> sun_shadow : register(t12);

RWTexture2D<uint4> samples : register(u0);

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
  if (any(id.xy >= MEMORY_SAMPLE_GRID)) return;
  const float2 tile_size = float2(reflection_size) / float2(MEMORY_SAMPLE_GRID);
  const float2 offset = float2(MemoryHash(uint3(id.xy, memory_frame)).xy >> 8u) * (1.0 / 16777216.0);
  const int2 pixel = int2(min(uint2((float2(id.xy) + offset) * tile_size), reflection_size - 1u));
  const uint2 first = uint2(id.x * 2u, id.y);

  uint atlas_width, atlas_height;
  sun_shadow.GetDimensions(atlas_width, atlas_height);
  const bool atlas = atlas_width >= 8u && atlas_height >= 8u;
  uint flags = atlas ? MEMORY_SAMPLE_ATLAS : 0u;

  const float depth = native_depth.Load(int3(pixel, 0));
  const float4 packed_normal = native_normal.Load(int3(pixel, 0));
  float3 normal = DecodeReflectionNormal(packed_normal.xyz);
  if (!(depth > 0.0) || !all(isfinite(normal))) {
    samples[first] = uint4(0u, 0u, 0u, flags);
    samples[first + uint2(1u, 0u)] = 0u;
    return;
  }
  flags |= MEMORY_SAMPLE_VALID;
  // Without the mask (the velocity pass did not record), moving objects
  // cannot be told apart, so every sample counts as moving.
  uint mask_width, mask_height;
  object_velocity.GetDimensions(mask_width, mask_height);
  if (mask_width == 0u || object_velocity.Load(int3(pixel, 0)).r > 0.5) flags |= MEMORY_SAMPLE_MOVING;
  const float3 view_position = GetReflectionPosition(float2(pixel) + 0.5, depth);
  // Normals facing away from the camera are flipped, as reflections do.
  if (dot(normal, view_position) > 0.0) normal = -normal;

  float visibility = 0.0;
  if (atlas) {
    const float4x4 projection = current_scene.shadow_projection;
    const float4x4 cascades = current_scene.shadow_projection2;
    if (dot(normal, SunTowardLight(projection, cascades)) > 0.0) flags |= MEMORY_SAMPLE_SUN_FACING;
    // Atlas UV per game unit in cascade 0; a cascade's scale multiplies it.
    const float uv_per_unit = 0.25 * length(projection._m00_m10_m20);
    // A cascade texel spans 4 / atlas_width of its clip range; keep two of them clear of its edge.
    const float edge = 1.0 - 8.0 / float(atlas_width);
    float4 transform = float4(0.0, 0.0, 0.0, 1.0);
    for (uint cascade = 0u; cascade < 4u; transform = cascades[cascade], ++cascade) {
      // Offset along the normal by one and a half texels of this cascade, so
      // the surface does not shadow itself.
      const float texel = 1.0 / (float(atlas_width) * uv_per_unit * abs(transform.w));
      const float3 projected = mul(float4(view_position + normal * (1.5 * texel), 1.0), projection).xyz;
      const float3 clip = projected * transform.w + transform.xyz;
      const float reference = (1.0 + clip.z) * cascades._m30;
      if (any(abs(clip.xy) > edge) || reference < 0.0 || reference > 1.0) continue;
      flags |= MEMORY_SAMPLE_SUN_COVERED;
      // Bilinear 2 x 2 comparison. Larger stored depth is nearer the light,
      // so the receiver is lit where it is not below the stored depth.
      const float2 position = SunAtlasUv(cascade, clip.xy) * float2(atlas_width, atlas_height) - 0.5;
      const int2 base = int2(floor(position));
      const float2 weight = position - float2(base);
      const float4 lit = float4(reference >= sun_shadow.Load(int3(base, 0)),
                                reference >= sun_shadow.Load(int3(base + int2(1, 0), 0)),
                                reference >= sun_shadow.Load(int3(base + int2(0, 1), 0)),
                                reference >= sun_shadow.Load(int3(base + int2(1, 1), 0)));
      visibility = lerp(lerp(lit.x, lit.y, weight.x), lerp(lit.z, lit.w, weight.x), weight.y);
      break;
    }
  }

  // Positions stay relative to the camera: world coordinates are large.
  const float3 relative = mul((float3x3)current_scene.view_to_world, view_position);
  // Albedo without the cube material's share where that is known, otherwise
  // the native albedo.
  const float3 native = native_albedo.Load(int3(pixel, 0)).rgb;
  const float4 clean = LoadCleanMaterial(clean_material, material_owner, pixel, reflection_size, native, packed_normal,
                                         native_material.Load(int3(pixel, 0)), depth);
  const float3 albedo = clean.a >= 0.0 ? clean.rgb : native;
  const uint3 encoded_albedo = uint3(round(sqrt(saturate(albedo)) * 255.0));
  samples[first] = uint4(asuint(relative), flags);
  samples[first + uint2(1u, 0u)] = uint4(0u, encoded_albedo.r | (encoded_albedo.g << 8u) | (encoded_albedo.b << 16u),
                                         uint(round(visibility * 255.0)), 0u);
}
