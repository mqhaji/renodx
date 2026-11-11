// Carry clean material color through the native wet-surface adjustment.
#ifndef MGSV_RAIN_MATERIAL_CAPTURE_HLSLI_
#define MGSV_RAIN_MATERIAL_CAPTURE_HLSLI_

#include "material_source.hlsli"

Texture2D<float4> rain_material_color : register(t18);
Texture2D<uint4> rain_material_owner : register(t19);
Texture2D<uint> rain_capture_parameters : register(t20);

void CaptureRainMaterial(int2 pixel, float4 albedo, float4 material, float depth,
                         float3 multiplier, float4 wet_albedo, float4 wet_material,
                         out float4 color, out uint4 owner) {
  color = float4(0.0, 0.0, 0.0, -1.0);
  owner = 0u;
  if (rain_capture_parameters.Load(int3(0, 0, 0)) == 0u) return;
  const float4 source = rain_material_color.Load(int3(pixel, 0));
  const uint4 previous = rain_material_owner.Load(int3(pixel, 0));
  const uint packed_albedo = PackMaterialUNorm(float4(renodx::color::srgb::Encode(albedo.rgb), albedo.a));
  if (source.a < 0.0 || !all(isfinite(source)) || !MatchMaterialUNorm(previous.x, packed_albedo)
      || !MatchMaterialUNorm(previous.z, PackMaterialUNorm(material)) || previous.w != asuint(depth)) return;
  color = float4(max(source.rgb * multiplier, 0.0), 1.0);
  // Rain preserves normal and depth. The later lighting read also checks the
  // normal against the current G-buffer before accepting this material color.
  owner = uint4(PackMaterialUNorm(wet_albedo), previous.y, PackMaterialUNorm(wet_material), previous.w);
}

#endif  // MGSV_RAIN_MATERIAL_CAPTURE_HLSLI_
