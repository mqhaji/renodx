// SPDX-License-Identifier: MIT
#ifndef MGSV_REFLECTIONS_COMPOSITION_HLSLI_
#define MGSV_REFLECTIONS_COMPOSITION_HLSLI_

#include "../../../../shaders/color/srgb.hlsl"
#include "../../common.hlsli"

Texture2D<float4> reflection_light : register(t18);
Texture2D<float4> reflection_environment_weight : register(t19);
Texture2D<float4> reflection_environment : register(t20);
Texture2D<float> reflection_ready : register(t21);

struct ReflectionResult {
  float4 light;
  float4 environment_weight;
  float4 environment;
  int status;
  bool valid;
  // The host bound a result for this view, so its native trace was canceled.
  // Views without one draw with the same compositor and keep the native path.
  bool replaces_native;
};

// Load the result once for both routing and composition. The GPU-written mode
// belongs to this submitted scene, rather than the worker's current CPU state.
ReflectionResult LoadReflectionResult(float2 uv) {
  ReflectionResult result = (ReflectionResult)0;
  uint width, height, ready_width, ready_height;
  reflection_ready.GetDimensions(ready_width, ready_height);
  if (ready_width == 0u || ready_height == 0u) return result;
  result.replaces_native = true;
  const int2 pixel = clamp(int2(uv * float2(ready_width, ready_height)), 0, int2(ready_width, ready_height) - 1);
  result.status = (int)round(reflection_ready.Load(int3(pixel, 0)) * 255.0);
  // Native-only and unavailable states must not sample stale enhanced images.
  if (result.status != MGSV_REFLECTION_ENHANCED) return result;
  reflection_light.GetDimensions(width, height);
  if (width != ready_width || height != ready_height) return result;
  result.light = reflection_light.Load(int3(pixel, 0));
  result.environment_weight = reflection_environment_weight.Load(int3(pixel, 0));
  result.environment = reflection_environment.Load(int3(pixel, 0));
  result.valid = result.environment.a >= 0.0 && result.light.a > 0.5
                 && all(isfinite(result.light)) && all(isfinite(result.environment_weight)) && all(isfinite(result.environment));
  return result;
}

// Both native compositors read extended sRGB and write extended sRGB in our
// float views. Perform only the replacement in linear light. Scene alpha and
// the compositor's native destination remain the caller's responsibility.
float3 ComposeReflections(float3 encoded_scene, ReflectionResult result) {
  const int debug_view = (int)CUSTOM_MGSV_LIGHTING_DEBUG_VIEW;
  float3 scene = renodx::color::srgb::Decode(max(encoded_scene, 0.0));
  const float4 light = result.light;
  const float4 environment_weight = result.environment_weight;
  const float4 environment = result.environment;
  if (result.valid) {
    if (debug_view == 11)
      scene = light.rgb;
    else if (debug_view == 12)
      scene = environment_weight.rgb;
    else if (debug_view == 13)
      scene = float3(0.0, 1.0, 0.0);
    else if (debug_view == 14)
      scene = environment.rgb;
    else if (debug_view == 15)
      scene = environment_weight.aaa;
    else if (debug_view == 16)
      scene = light.rgb;
    else if (debug_view == 17)
      scene = environment_weight.rgb;
    else {
      // Visibility and fallback selection were part of the complete estimator
      // before denoising. Restore receiver fog transmission once, then replace
      // only the saved specular term. Direct light, diffuse and fog scatter stay.
      const float3 reflected = light.rgb * environment.a;
      // Replace the captured environment once, preserving direct/diffuse light
      // and fog in-scattering already present in the rest of the scene.
      scene += CUSTOM_MGSV_REFLECTION_STRENGTH * (reflected - environment.rgb);
    }
  } else if (debug_view >= 11 && debug_view <= 17) {
    scene = float3(1.0, 0.0, 1.0);
  }
  return renodx::color::srgb::Encode(max(scene, 0.0));
}

#endif
