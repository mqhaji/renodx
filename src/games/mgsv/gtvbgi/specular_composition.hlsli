// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

#ifndef MGSV_SPECULAR_COMPOSITION_HLSLI_
#define MGSV_SPECULAR_COMPOSITION_HLSLI_

#include "../shared.h"

Texture2D<float4> captured_environment : register(t16);
Texture2D<float2> resolved_specular_visibility : register(t17);
Texture2D<float4> resolved_specular_light : register(t18);
Texture2D<float4> resolved_environment_response : register(t19);
Texture2D<float4> resolved_material_color : register(t20);
Texture2D<float> resolved_specular_ready : register(t21);
static bool specular_replacement_applied = false;

// Separate the captured environment from direct highlights. Only environment
// reflections use ambient occlusion; direct light keeps its own shadows.
float3 GetEnvironmentSpecularCorrection(float2 uv, float native_visibility, float3 native_specular,
                                        float3 native_albedo, float3 diffuse) {
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
  uint result_width, result_height, color_width, color_height, ready_width, ready_height;
  resolved_specular_light.GetDimensions(result_width, result_height);
  resolved_material_color.GetDimensions(color_width, color_height);
  resolved_specular_ready.GetDimensions(ready_width, ready_height);
  if (CUSTOM_MGSV_GT_DEBUG_VIEW == 0.0 && result_width == width && result_height == height
      && color_width == width && color_height == height && ready_width == width && ready_height == height
      && resolved_specular_ready.Load(int3(pixel, 0)) > 0.5) {
    // Native surface writes reset readiness without touching HDR RGB. A failed
    // or skipped application therefore cannot expose a previous reflection.
    const float4 screen = resolved_specular_light.Load(int3(pixel, 0));
    const float4 response = resolved_environment_response.Load(int3(pixel, 0));
    const float4 material = resolved_material_color.Load(int3(pixel, 0));
    if (screen.a > 0.5 && material.a >= 0.0 && all(isfinite(screen)) && all(isfinite(response))
        && all(isfinite(material)) && all(response.rgb >= 0.0) && all(response.rgb <= 1.0)) {
      // Native cube color was stored inside albedo. Move that contribution
      // beside native environment lighting before replacing either with hits.
      // This fallback retains MGSV's integrated material response; it is not
      // raw GGX environment radiance recovered from the authored LDR cube.
      const float3 native_cube = max(native_albedo - material.rgb, 0.0) * diffuse;
      const float3 fallback_environment = environment.rgb + native_cube;
      const float3 remaining = lerp(1.0, response.rgb, CUSTOM_MGSV_SPECULAR_OCCLUSION_STRENGTH);
      // Apply strength after filtering, so comparisons keep the same history.
      // Zero removes added light while retaining the chosen specular occlusion.
      const float3 indirect = screen.rgb * CUSTOM_MGSV_SPECULAR_GI_STRENGTH + fallback_environment * remaining;
      specular_replacement_applied = true;
      return (material.rgb - native_albedo) * diffuse
             + (1.0 - native_visibility) * direct + indirect - native_visibility * environment.rgb;
    }
  }
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
