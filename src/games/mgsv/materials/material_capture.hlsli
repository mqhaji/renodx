// Helpers for the material ports in native/. Each port also publishes its color
// without the baked cube map. The game still receives its original three outputs.
#ifndef MGSV_MATERIAL_CAPTURE_HLSLI_
#define MGSV_MATERIAL_CAPTURE_HLSLI_

#include "material_source.hlsli"

Texture2D<uint> material_capture_parameters : register(t20);

// A per-pixel temporary, not shared GPU state. Each port runs the native material
// code with its cube, then again with the cube's RGB set to zero. The compiler can
// share the common texture and geometry work between the two runs.
static bool material_clean_pass = false;

float4 SampleMaterialCube(TextureCube<float4> image, SamplerState image_sampler, float3 direction) {
  const float4 sampled = image.Sample(image_sampler, direction);
  return float4(material_clean_pass ? 0.0 : sampled.rgb, sampled.a);
}

float4 SampleMaterialCubeLevel(TextureCube<float4> image, SamplerState image_sampler, float3 direction, float level) {
  const float4 sampled = image.SampleLevel(image_sampler, direction, level);
  return float4(material_clean_pass ? 0.0 : sampled.rgb, sampled.a);
}

#endif  // MGSV_MATERIAL_CAPTURE_HLSLI_
