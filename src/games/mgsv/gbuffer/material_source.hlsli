// Clean color is usable only while the native surface still matches its writer.
#ifndef MGSV_MATERIAL_SOURCE_HLSLI_
#define MGSV_MATERIAL_SOURCE_HLSLI_

#include "../../../shaders/color.hlsl"

uint PackMaterialUNorm(float4 value) {
  const uint4 bytes = (uint4)round(saturate(value) * 255.0);
  return bytes.x | (bytes.y << 8u) | (bytes.z << 16u) | (bytes.w << 24u);
}

bool MatchMaterialUNorm(uint stored, uint current) {
  if (stored == current) return true;
  // UNORM render-target conversion can choose the adjacent code at a rounding
  // boundary. Shader round() and the hardware store need not resolve ties alike.
  const uint4 shift = uint4(0u, 8u, 16u, 24u);
  return all(abs(int4((stored >> shift) & 255u) - int4((current >> shift) & 255u)) <= 1);
}

// RGB is linear material color, including native diffuse allocation. Alpha is
// valid when nonnegative; -1 means missing or invalid data.
// Compare all native channels within one UNORM code and match depth exactly.
float4 LoadCleanMaterial(Texture2D<float4> color, Texture2D<uint4> owner,
                         int2 pixel, uint2 size, float4 albedo, float4 normal, float4 material, float depth) {
  uint width, height;
  color.GetDimensions(width, height);
  if (width != size.x || height != size.y) return float4(0.0, 0.0, 0.0, -1.0);
  const float4 clean = color.Load(int3(pixel, 0));
  if (clean.a < 0.0 || !all(isfinite(clean)) || any(clean.rgb < 0.0)) return float4(0.0, 0.0, 0.0, -1.0);
  // Native albedo is sampled through an sRGB view. Re-encode only for matching
  // its original UNORM codes; lighting continues to use the decoded RGB.
  const uint4 expected = uint4(PackMaterialUNorm(float4(renodx::color::srgb::Encode(albedo.rgb), albedo.a)),
                               PackMaterialUNorm(normal), PackMaterialUNorm(material), asuint(depth));
  const uint4 captured = owner.Load(int3(pixel, 0));
  if (captured.w != expected.w || !MatchMaterialUNorm(captured.x, expected.x)
      || !MatchMaterialUNorm(captured.y, expected.y) || !MatchMaterialUNorm(captured.z, expected.z)) {
    return float4(0.0, 0.0, 0.0, -1.0);
  }
  return clean;
}

#endif  // MGSV_MATERIAL_SOURCE_HLSLI_
