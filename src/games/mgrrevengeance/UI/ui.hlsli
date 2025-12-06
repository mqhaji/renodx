#include "../shared.h"

float3 ScaleUI(float3 color_ui) {
  color_ui = renodx::color::gamma::Decode(max(0, color_ui));
  color_ui *= RENODX_GRAPHICS_WHITE_SCALE;
  color_ui = renodx::color::gamma::Encode(color_ui);

  return color_ui;
}
