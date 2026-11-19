// Carry a cube material's record through the native wet-surface adjustment.
#ifndef MGSV_RAIN_MATERIAL_CAPTURE_HLSLI_
#define MGSV_RAIN_MATERIAL_CAPTURE_HLSLI_

#include "material_source.hlsli"

// Copies of the records as they were before this draw. The draw writes the
// same record images, so it cannot read them directly.
Texture2D<float4> rain_material_color : register(t18);
Texture2D<uint4> rain_material_owner : register(t19);
Texture2D<uint> rain_capture_parameters : register(t20);

// `albedo` is the dry value this pass read. A record that describes it is
// carried to the wet values; rain keeps normal and depth. Any other record is
// kept as it is, so it still reads as changed or as another surface.
void CaptureRainMaterial(int2 pixel, float4 albedo, float3 multiplier, float4 wet_albedo, float4 wet_material,
                         out float4 color, out uint4 owner) {
  color = float4(0.0, 0.0, 0.0, -1.0);
  owner = 0u;
  if (rain_capture_parameters.Load(int3(0, 0, 0)) == 0u) return;
  color = rain_material_color.Load(int3(pixel, 0));
  owner = rain_material_owner.Load(int3(pixel, 0));
  if (owner.w == 0u || color.a < 0.0 || !all(isfinite(color))
      || !MatchAlbedoUNorm(owner.x, PackMaterialUNorm(float4(renodx::color::srgb::Encode(saturate(albedo.rgb)), 0.0)))) {
    return;
  }
  color = float4(max(color.rgb * multiplier, 0.0), 1.0);
  owner = uint4(PackMaterialUNorm(wet_albedo), owner.y, PackMaterialUNorm(wet_material), owner.w);
}

#endif  // MGSV_RAIN_MATERIAL_CAPTURE_HLSLI_
