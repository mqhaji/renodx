// A baked material cube can be separated from albedo only while its record
// still describes the albedo that is visible.
#ifndef MGSV_MATERIAL_SOURCE_HLSLI_
#define MGSV_MATERIAL_SOURCE_HLSLI_

#include "../../../shaders/color.hlsl"

uint PackMaterialUNorm(float4 value) {
  const uint4 bytes = (uint4)round(saturate(value) * 255.0);
  return bytes.x | (bytes.y << 8u) | (bytes.z << 16u) | (bytes.w << 24u);
}

// UNORM render-target conversion can choose the adjacent code at a rounding
// boundary, and shader round() need not resolve ties the same way.
bool MatchMaterialUNorm(uint stored, uint current) {
  const uint4 shift = uint4(0u, 8u, 16u, 24u);
  return all(abs(int4((stored >> shift) & 255u) - int4((current >> shift) & 255u)) <= 1);
}

// Color bytes only: decals blend albedo color and never write its alpha.
bool MatchAlbedoUNorm(uint stored, uint current) {
  return MatchMaterialUNorm(stored | 0xFF000000u, current | 0xFF000000u);
}

// Only material shaders that bake a cube into albedo write a record: their
// color without the cube, the three values they wrote, and their depth. RGB
// returns linear material color with the cube removed, including native
// diffuse allocation. Alpha is 1 when that color is known and -1 when it is not.
//
// - No record, or a record that is not the visible surface: that surface never
//   had a cube, so its albedo is the answer.
// - A record whose albedo still matches: the capture's cube-free color.
// - A record whose albedo no longer matches: a later writer, such as a decal,
//   changed this surface, and the cube's remaining share is unknown.
//
// A record is the visible surface when it was written at this depth. A cube
// draw that does not write depth leaves a record at another depth; it is the
// visible surface only while all three values it wrote are still there.
//
// `albedo` is the native value as decoded by its sRGB view.
float4 LoadCleanMaterial(Texture2D<float4> color, Texture2D<uint4> owner, int2 pixel, uint2 size,
                         float3 albedo, float4 normal, float4 material, float depth) {
  uint width, height;
  color.GetDimensions(width, height);
  if (width != size.x || height != size.y) return float4(albedo, 1.0);
  const uint4 record = owner.Load(int3(pixel, 0));
  if (record.w == 0u) return float4(albedo, 1.0);
  const bool albedo_matches = MatchAlbedoUNorm(record.x, PackMaterialUNorm(float4(renodx::color::srgb::Encode(saturate(albedo)), 0.0)));
  if (record.w != asuint(depth)
      && !(albedo_matches && MatchMaterialUNorm(record.y, PackMaterialUNorm(normal)) && MatchMaterialUNorm(record.z, PackMaterialUNorm(material)))) {
    return float4(albedo, 1.0);
  }
  const float4 clean = color.Load(int3(pixel, 0));
  if (albedo_matches && clean.a >= 0.0 && all(isfinite(clean)) && all(clean.rgb >= 0.0)) return float4(clean.rgb, 1.0);
  return float4(0.0, 0.0, 0.0, -1.0);
}

#endif  // MGSV_MATERIAL_SOURCE_HLSLI_
