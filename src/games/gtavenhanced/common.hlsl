#include "./shared.h"

float3 UIScale(float3 color) {
  color = renodx::color::gamma::Decode(max(0, color), 2.2f);
  color *= RENODX_GRAPHICS_WHITE_NITS / RENODX_DIFFUSE_WHITE_NITS;
  color = renodx::color::gamma::Encode(color, 2.2f);
  return color;
}

float4 FinalizeOutput(float4 color) {
  color.rgb = renodx::color::gamma::DecodeSafe(color.rgb, 2.2f);
  color.rgb *= RENODX_DIFFUSE_WHITE_NITS / renodx::color::srgb::REFERENCE_WHITE;
  color.a = saturate(color.a);
  return color;
}
