// SPDX-License-Identifier: MIT
#ifndef MGSV_REFLECTIONS_COMPOSITION_HLSLI_
#define MGSV_REFLECTIONS_COMPOSITION_HLSLI_

#include "../../../../shaders/color/srgb.hlsl"
#include "../../common.hlsli"

// Composes the enhanced reflection result in the game's reflection compositors
// (LocalReflectionAddBuffer and LocalReflectionComposite). reflection_ready
// holds a per-pixel status byte; the other three images come from
// sssr_resolve and the saved environment.
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

// Loads the result once; callers use it both to choose the path and to
// compose. The status is written on the GPU for this submitted scene, so it
// does not depend on the CPU's current state.
ReflectionResult LoadReflectionResult(float2 uv) {
  ReflectionResult result = (ReflectionResult)0;
  uint width, height, ready_width, ready_height;
  reflection_ready.GetDimensions(ready_width, ready_height);
  if (ready_width == 0u || ready_height == 0u) return result;
  result.replaces_native = true;
  const int2 pixel = clamp(int2(uv * float2(ready_width, ready_height)), 0, int2(ready_width, ready_height) - 1);
  result.status = (int)round(reflection_ready.Load(int3(pixel, 0)) * 255.0);
  // Any other status means the enhanced images may be stale; do not read them.
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

// Both native compositors read and write extended sRGB in the FP16 targets.
// Only the replacement happens in linear light. Scene alpha and the native
// destination are left to the caller. Debug views 11 to 17 show the
// reflection inputs; magenta marks a pixel without valid reflection data.
float3 ComposeReflections(float3 encoded_scene, ReflectionResult result) {
  const int debug_view = (int)CUSTOM_MGSV_INDIRECT_LIGHTING_DEBUG_VIEW;
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
      // Visibility and the environment fallback are already in the estimate.
      // Restore receiver fog transmission (environment alpha) once, then swap
      // the saved environment specular for the new result. Direct light,
      // diffuse light, and fog in-scattering in the scene stay.
      const float3 reflected = light.rgb * environment.a;
      scene += CUSTOM_MGSV_REFLECTION_STRENGTH * (reflected - environment.rgb);
    }
  } else if (debug_view >= 11 && debug_view <= 17) {
    scene = float3(1.0, 0.0, 1.0);
  }
  return renodx::color::srgb::Encode(max(scene, 0.0));
}

#endif
