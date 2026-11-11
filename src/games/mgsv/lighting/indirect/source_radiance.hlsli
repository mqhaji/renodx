#ifndef MGSV_SOURCE_RADIANCE_HLSLI_
#define MGSV_SOURCE_RADIANCE_HLSLI_

#include "../../materials/material_source.hlsli"

// RGB is outgoing diffuse radiance in linear, pre-exposed scene units. Alpha
// distinguishes qualified material color from the native albedo fallback.
// Zero RGB is a valid black source; finiteness is checked by the consumer.
float4 LoadSourceRadiance(Texture2D<float4> diffuse, Texture2D<float4> albedo,
                          Texture2D<float4> material, Texture2D<float4> clean, Texture2D<uint4> owner,
                          int2 pixel, uint2 size, float4 normal, float depth, bool use_clean) {
  const float4 native_albedo = albedo.Load(int3(pixel, 0));
  float4 color = float4(native_albedo.rgb, 0.0);
  if (use_clean) {
    const float4 captured = LoadCleanMaterial(clean, owner, pixel, size, native_albedo, normal,
                                              material.Load(int3(pixel, 0)), depth);
    if (captured.a >= 0.0) color = float4(captured.rgb, 1.0);
  }
  return float4(color.rgb * diffuse.Load(int3(pixel, 0)).rgb, color.a);
}

#endif  // MGSV_SOURCE_RADIANCE_HLSLI_
