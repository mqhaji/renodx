#ifndef MGSV_SOURCE_RADIANCE_HLSLI_
#define MGSV_SOURCE_RADIANCE_HLSLI_

#include "../../materials/material_source.hlsli"

// Outgoing diffuse radiance of one source pixel for GT-VBGI: material color
// times the pixel's diffuse light. With use_clean, materials that bake a cube
// map into albedo use their recorded cube-free color (materials/README.md).
//
// RGB is linear and pre-exposed. Alpha is 1 where the color has no baked cube
// in it and 0 where native albedo had to stand in (a cube surface changed
// after its draw, or use_clean is false). Zero RGB is a valid black source;
// the caller checks finiteness.
float4 LoadSourceRadiance(Texture2D<float4> diffuse, Texture2D<float4> albedo,
                          Texture2D<float4> material, Texture2D<float4> clean, Texture2D<uint4> owner,
                          int2 pixel, uint2 size, float4 normal, float depth, bool use_clean) {
  const float4 native_albedo = albedo.Load(int3(pixel, 0));
  float4 color = float4(native_albedo.rgb, 0.0);
  if (use_clean) {
    const float4 captured = LoadCleanMaterial(clean, owner, pixel, size, native_albedo.rgb, normal,
                                              material.Load(int3(pixel, 0)), depth);
    if (captured.a >= 0.0) color = float4(captured.rgb, 1.0);
  }
  return float4(color.rgb * diffuse.Load(int3(pixel, 0)).rgb, color.a);
}

#endif  // MGSV_SOURCE_RADIANCE_HLSLI_
