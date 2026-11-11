#ifndef MGSV_REFLECTION_OWNERSHIP_HLSLI_
#define MGSV_REFLECTION_OWNERSHIP_HLSLI_

#include "../shared.h"

Texture2D<float> composed_specular : register(t20);

// This mask is written by deferred composition itself. It lets the compositor
// preserve GT-VBGI when native SSR has no hit. It must not suppress tracing:
// a valid GT-VBGI result can still miss detail that the native trace finds.
bool HasComposedSpecular(float2 uv) {
  if (CUSTOM_MGSV_LIGHTING_ENABLED == 0.0 || CUSTOM_MGSV_GT_DEBUG_VIEW != 0.0) return false;
  uint width, height;
  composed_specular.GetDimensions(width, height);
  if (width == 0u || height == 0u) return false;
  const int2 pixel = clamp((int2)floor(uv * float2(width, height)), 0, int2(width, height) - 1);
  return composed_specular.Load(int3(pixel, 0)) > 0.5;
}

#endif  // MGSV_REFLECTION_OWNERSHIP_HLSLI_
