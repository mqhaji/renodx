// SPDX-License-Identifier: MIT
#ifndef MGSV_SCREEN_SPACE_SHADOWS_HLSLI_
#define MGSV_SCREEN_SPACE_SHADOWS_HLSLI_

#include "../shared.h"
#include "pcss.hlsli"

Texture2D<float4> bend_shadow_mask : register(t18);
ByteAddressBuffer bend_shadow_data : register(t19);

// Setup stores these five floats in unused space after the indirect arguments.
// State 2 still has valid light data even when contact tracing was budgeted out.
PcssProjection ReadPcssProjection(float4x4 projection, float4x4 cascades, bool directional) {
  if (bend_shadow_data.Load(40) != 0u) {
    float4 values = asfloat(bend_shadow_data.Load4(PCSS_PROJECTION_OFFSET));
    float depth_b = asfloat(bend_shadow_data.Load(PCSS_PROJECTION_OFFSET + 16u));
    if (all(isfinite(values)) && isfinite(depth_b) && values.z > 0.0) {
      PcssProjection result;
      result.uv_scale = values.xy;
      result.depth_scale = values.z;
      result.depth_a = values.w;
      result.depth_b = depth_b;
      return result;
    }
  }
  return CalculatePcssProjection(projection, cascades, directional);
}

struct BendShadow {
  float visibility;
  float debug_value;
  bool valid;
  bool diagnostic;
};

// Resolve once before shadow-map filtering. A fully blocked direct light can
// skip PCSS, while missing contact-shadow data keeps the map filter available.
BendShadow ReadBendShadow(float2 pixel, uint light_type) {
  BendShadow result = (BendShadow)0;
  result.visibility = 1.0;
  const bool directional = light_type == 0;
  if (!CUSTOM_ENHANCED_SHADOWS_ENABLED) return result;
  const uint debug = (uint)CUSTOM_SS_DEBUG_VIEW;
  // The bypass view keeps the same compute work for performance comparisons.
  if (debug == 4) return result;
  const bool diagnostic = ((debug >= 1 && debug <= 3) || debug == 5) && light_type == (uint)CUSTOM_SS_DEBUG_LIGHT;
  result.diagnostic = diagnostic;
  const uint4 metadata = bend_shadow_data.Load4(32);
  // Budget or culling can omit this light's clear and trace. Its mask
  // still contains another light's pixels, so never read it in this state.
  if (metadata.z == 2) {
    result.valid = diagnostic;
    result.debug_value = debug == 1 ? 1.0 : 0.0;
    return result;
  }
  const uint4 bounds = bend_shadow_data.Load4(16);
  const uint2 position = uint2(pixel);
  bool supported = metadata.z == 1 && all(position >= bounds.xy) && all(position < bounds.zw);
  float2 sample = supported ? bend_shadow_mask.Load(int3(position, 0)).xy : float2(1.0, 0.0);
  // Whole unlit regions can retain their neutral clear value without a trace.
  if (!directional && metadata.z == 1 && CUSTOM_SS_LOCAL_CULLING >= 0.5 && (!supported || sample.y < 0.24)) {
    supported = true;
    sample = float2(1.0, 0.25);
  }
  result.valid = supported && sample.y >= 0.24 && all(isfinite(sample));
  result.visibility = result.valid ? sample.x : 1.0;
  result.debug_value = debug == 1 ? sample.x : saturate((sample.y - 0.25) / 0.75);
  return result;
}

// Normal rendering combines contact and map visibility before coloring the light.
// Diagnostics replace only direct RGB; environment reflections and alpha stay intact.
void ApplyBendDebug(BendShadow shadow, inout float3 diffuse, inout float3 specular) {
  if (shadow.diagnostic) {
    diffuse = shadow.valid ? shadow.debug_value.xxx : float3(1.0, 0.0, 1.0);
    specular = 0.0;
  }
}

#endif
