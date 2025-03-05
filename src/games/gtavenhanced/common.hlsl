#include "./shared.h"

float3 UIScale(float3 color) {
  color = renodx::color::gamma::Decode(color, 2.2f);
  color *= RENODX_GRAPHICS_WHITE_NITS / RENODX_DIFFUSE_WHITE_NITS;
  color = renodx::color::gamma::Encode(color, 2.2f);
  return color;
}

float3 FinalizeOutput(float3 color) {
  color = renodx::color::gamma::DecodeSafe(color, 2.2f);
  color *= RENODX_DIFFUSE_WHITE_NITS / renodx::color::srgb::REFERENCE_WHITE;
  return color;
}
