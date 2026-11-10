// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

#ifndef MGSV_SPECULAR_COMPOSITION_HLSLI_
#define MGSV_SPECULAR_COMPOSITION_HLSLI_

#include "../shared.h"

Texture2D<float4> captured_environment : register(t16);
Texture2D<float2> resolved_specular_visibility : register(t17);

// Separate the captured environment from direct highlights. Only environment
// reflections use ambient occlusion; direct light keeps its own shadows.
float3 GetEnvironmentSpecularCorrection(float2 uv, float native_visibility, float3 native_specular) {
  if (CUSTOM_MGSV_LIGHTING_ENABLED == 0.0) return 0.0;
  uint width, height;
  captured_environment.GetDimensions(width, height);
  if (width == 0 || height == 0) return 0.0;
  const int2 pixel = clamp((int2)floor(uv * float2(width, height)), int2(0, 0), int2(width, height) - 1);
  const float4 environment = captured_environment.Load(int3(pixel, 0));
  const float2 visibility = resolved_specular_visibility.Load(int3(pixel, 0));
  if (environment.a < 0.5 || !all(isfinite(environment)) || !all(isfinite(native_specular))) return 0.0;
  // The capture must be a component of total specular. Allow two FP16 relative
  // rounding steps plus a small absolute tolerance, not an unrelated brighter image.
  const float3 rounding = max(abs(environment.rgb), abs(native_specular)) * (2.0 / 1024.0) + 1.2e-7;
  if (any(environment.rgb > native_specular + rounding)) return 0.0;
  const float3 direct = max(native_specular - environment.rgb, 0.0);
  // The caller already applied native AO to total specular. Add a delta that
  // restores direct highlights, then changes only environment occlusion:
  // direct + environment * lerp(native AO, custom visibility, strength * confidence).
  float3 correction = (1.0 - native_visibility) * direct;
  if (all(isfinite(visibility)) && visibility.y > 0.0) {
    correction += (saturate(visibility.x) - native_visibility) * environment.rgb
                  * (CUSTOM_MGSV_SPECULAR_OCCLUSION_STRENGTH * saturate(visibility.y));
  }
  return correction;
}

#endif  // MGSV_SPECULAR_COMPOSITION_HLSLI_
