// Material color before the native cubemap approximation is added. The game
// still receives its original three outputs; these helpers serve extra outputs.
#ifndef MGSV_MATERIAL_CAPTURE_HLSLI_
#define MGSV_MATERIAL_CAPTURE_HLSLI_

#include "material_source.hlsli"

Texture2D<uint> material_capture_parameters : register(t20);

// This is a per-pixel compiler temporary, not shared mutable GPU state. Native
// material code is evaluated with its cube, then with cube RGB set to zero.
// The compiler can reuse the common texture and geometry work between them.
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
