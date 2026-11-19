// SPDX-License-Identifier: MIT
#ifndef MGSV_SPECULAR_COMPOSITION_HLSLI_
#define MGSV_SPECULAR_COMPOSITION_HLSLI_

#include "../../materials/material_source.hlsli"
#include "../../shared.h"
Texture2D<float4> captured_environment : register(t16);
Texture2D<uint4> resolved_material_owner : register(t17);
Texture2D<float4> resolved_normal : register(t18);
Texture2D<float4> resolved_material : register(t19);
Texture2D<float4> resolved_material_color : register(t20);

// The host binds the environment capture only for the view whose diffuse gather
// and late reflections also run. Other views draw with these same shaders and
// keep the game's own occlusion and environment light. So does every view under
// the vanilla tone mapper, where the mode is off.
bool UsesEnhancedIndirectLighting() {
  if (!CUSTOM_MGSV_ENHANCED_INDIRECT_LIGHTING) return false;
  uint width, height;
  captured_environment.GetDimensions(width, height);
  return width != 0u && height != 0u;
}

// Publish the part of this scene color that late SSR may replace. Direct
// highlights keep their own shadows; ambient visibility only affects the
// environment. The caller applies exactly its native receiver fog to this pair.
float3 GetEnvironmentSpecularCorrection(float2 uv, Texture2D<float4> scene_depth, float native_visibility, float3 native_specular,
    float3 native_albedo, float3 diffuse, out float4 environment_output, float3 wet_environment = 0.0) {
  environment_output = float4(0.0, 0.0, 0.0, -1.0);
  if (!UsesEnhancedIndirectLighting()) return 0.0;
  uint width, height;
  captured_environment.GetDimensions(width, height);
  const int2 pixel = clamp(int2(uv * float2(width, height)), 0, int2(width, height) - 1);
  const float4 environment = captured_environment.Load(int3(pixel, 0));
  if (environment.a < 0.5 || !all(isfinite(environment)) || !all(isfinite(native_specular))) return 0.0;
  const float3 rounding = max(abs(environment.rgb), abs(native_specular)) * (2.0 / 1024.0) + 1.2e-7;
  if (any(environment.rgb > native_specular + rounding)) return 0.0;
  const float3 direct = max(native_specular - environment.rgb, 0.0);
  const float4 material = LoadCleanMaterial(resolved_material_color, resolved_material_owner, pixel, uint2(width, height),
                                           native_albedo, resolved_normal.Load(int3(pixel, 0)),
                                           resolved_material.Load(int3(pixel, 0)), scene_depth.Load(int3(pixel, 0)).x);
  if (material.a >= 0.0) {
    // A material cube is packed into albedo, while rain contributes another
    // cube after the lighting sum. Both are already present in scene color.
    // Diffuse light carries no native occlusion here: the gather already applied it.
    const float3 material_cube = max(native_albedo - material.rgb, 0.0) * diffuse;
    environment_output = float4(native_visibility * environment.rgb + material_cube + wet_environment, 1.0);
  }
  return (1.0 - native_visibility) * direct;
}
#endif
