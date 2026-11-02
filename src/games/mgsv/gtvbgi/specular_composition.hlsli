// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

#ifndef MGSV_SPECULAR_COMPOSITION_HLSLI_
#define MGSV_SPECULAR_COMPOSITION_HLSLI_

#include "../shared.h"

Texture2D<float4> captured_environment : register(t16);
Texture2D<float2> resolved_specular_visibility : register(t17);

// The native term is O * (direct + environment). Replace only the environment
// visibility, before fog/tonemapping, without applying native AO a second time.
float3 EnvironmentSpecularCorrection(float2 uv, float native_visibility) {
  if (CUSTOM_MGSV_LIGHTING_ENABLED == 0.0 || CUSTOM_MGSV_SPECULAR_OCCLUSION_STRENGTH <= 0.0) return 0.0;
  uint width, height;
  captured_environment.GetDimensions(width, height);
  if (width == 0 || height == 0) return 0.0;
  const int2 pixel = clamp((int2)floor(uv * float2(width, height)), int2(0, 0), int2(width, height) - 1);
  const float4 environment = captured_environment.Load(int3(pixel, 0));
  const float2 visibility = resolved_specular_visibility.Load(int3(pixel, 0));
  if (environment.a < 0.5 || !all(isfinite(environment)) || !all(isfinite(visibility)) || visibility.y <= 0.0) return 0.0;
  return (saturate(visibility.x) - native_visibility) * environment.rgb
         * (CUSTOM_MGSV_SPECULAR_OCCLUSION_STRENGTH * saturate(visibility.y));
}

#endif  // MGSV_SPECULAR_COMPOSITION_HLSLI_
