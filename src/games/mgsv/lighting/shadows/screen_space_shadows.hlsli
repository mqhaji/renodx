// SPDX-License-Identifier: MIT
#ifndef MGSV_SCREEN_SPACE_SHADOWS_HLSLI_
#define MGSV_SCREEN_SPACE_SHADOWS_HLSLI_

#include "../../shared.h"
#include "pcss.hlsli"

// Contact-shadow lookup for the native light shader replacements.
// bend_shadow_mask is the atlas written by contact/bend_trace: R is visibility,
// G is 0.25 + 0.75 * r / (1 + r) for a soft-edge radius r in pixels (or a debug
// value), and G below 0.24 means no data. bend_shadow_data is this light's
// setup record (layout in contact/bend_common.hlsli).
Texture2D<float4> bend_shadow_mask : register(t18);
ByteAddressBuffer bend_shadow_data : register(t19);

// Setup caches these five floats in the record, after the indirect arguments.
// It fills them for every light with a state (1 traced, 2 not traced), so a
// light the spotlight limit leaves out still has them.
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

// Each light owns one packed rectangle of the shared atlas: its first texel and
// the atlas width, a power of two. Keep screen bounds for geometry and filter
// rejection, then map accepted texels into the atlas.
float2 ReadBendMask(int2 pixel, uint4 bounds, uint2 atlas) {
  const uint2 local = uint2(pixel - int2(bounds.xy));
  const uint texel_index = atlas.x + local.y * (bounds.z - bounds.x) + local.x;
  return bend_shadow_mask.Load(int3(texel_index & (atlas.y - 1u), texel_index >> firstbithigh(atlas.y), 0)).xy;
}

// Resolve once before shadow-map filtering. A fully blocked direct light can
// skip PCSS, while missing contact-shadow data keeps the map filter available.
BendShadow ReadBendShadow(float2 pixel, uint light_type, Texture2D<float4> depth) {
  BendShadow result = (BendShadow)0;
  result.visibility = 1.0;
  const bool directional = light_type == 0;
  if (!CUSTOM_MGSV_ENHANCED_SHADOWS) return result;
  const uint debug = (uint)CUSTOM_CONTACT_SHADOW_DEBUG_VIEW;
  // The bypass view keeps the same compute work for performance comparisons.
  if (debug == 4) return result;
  const bool diagnostic = ((debug >= 1 && debug <= 3) || debug == 5 || debug == 6) && light_type == (uint)CUSTOM_SHADOW_DEBUG_LIGHT;
  result.diagnostic = diagnostic;
  const uint4 metadata = bend_shadow_data.Load4(32);
  // The spotlight limit or culling can omit this light's clear and trace. Its mask
  // still contains another light's pixels, so never read it in this state.
  if (metadata.z == 2) {
    result.valid = diagnostic;
    result.debug_value = debug == 1 ? 1.0 : 0.0;
    return result;
  }
  if (metadata.z != 1u) return result;
  // Bounds belong to this GPU invocation. Equal image dimensions or a retained
  // texture view alone would not prove that these pixels belong to this light.
  const uint4 bounds = bend_shadow_data.Load4(16);
  const uint2 atlas = bend_shadow_data.Load2(368u);
  const uint2 position = uint2(pixel);
  const bool inside_bounds = all(position >= bounds.xy) && all(position < bounds.zw);
  bool supported = inside_bounds;
  float2 sample = supported ? ReadBendMask(int2(position), bounds, atlas) : float2(1.0, 0.0);
  // For local lights, pixels outside the receiver bounds or left unwritten by
  // the trace are outside the light's reach. Treat them as unshadowed.
  if (!directional && (!supported || sample.y < 0.24)) {
    supported = true;
    sample = float2(1.0, 0.25);
  }
  result.valid = supported && sample.y >= 0.24 && all(isfinite(sample));
  result.visibility = result.valid ? sample.x : 1.0;
  if (result.valid && inside_bounds && (!diagnostic || debug == 1u) && CUSTOM_SHADOW_SOFTNESS > 0.0) {
    // Search and filter share these nine mask reads. A caster's footprint is
    // considered on both sides of its edge, including currently lit pixels.
    float2 neighborhood[9];
    float packed_radius = 0.25;
    float minimum_visibility = sample.x;
    float maximum_visibility = sample.x;
    [unroll]
    for (int index = 0; index < 9; ++index) {
      const int2 offset = int2(index % 3 - 1, index / 3 - 1);
      const int2 neighbor = int2(position) + offset;
      const bool inside = all(neighbor >= int2(bounds.xy)) && all(neighbor < int2(bounds.zw));
      const float2 value = index == 4 ? sample : (inside ? ReadBendMask(neighbor, bounds, atlas) : 0.0);
      const bool valid = value.y >= 0.24 && all(isfinite(value));
      neighborhood[index] = valid ? value : 0.0;
      if (valid) {
        packed_radius = max(packed_radius, value.y);
        minimum_visibility = min(minimum_visibility, value.x);
        maximum_visibility = max(maximum_visibility, value.x);
      }
    }
    // The packed value increases with radius, so decode only the largest one,
    // and this pixel's own. A shadowed pixel far from its caster carries a
    // radius the nine texels cannot span; it takes taps over a disk instead.
    const float2 radii = (float2(packed_radius, sample.y) - 0.25) / max(1.0 - float2(packed_radius, sample.y), 0.001);
    const float radius = min(radii.x, SHADOW_CONTACT_FILTER_RADIUS);
    const float wide_radius = min(radii.y, SHADOW_CONTACT_WIDE_RADIUS);
    const bool wide = sample.x < 1.0 && wide_radius > SHADOW_CONTACT_FILTER_RADIUS;
    // How far a soft edge beside this pixel has gone past the small filter's
    // radius. The wide filter keeps its soft edge inside the shadow, so a lit
    // pixel beside it stays lit; over one render pixel past that radius both
    // sides change over from the small filter, so the edge does not jump.
    const float2 widening = saturate(min(radii, SHADOW_CONTACT_WIDE_RADIUS) - SHADOW_CONTACT_FILTER_RADIUS);
    [branch]
    if (wide || (radius >= 0.5 && maximum_visibility > minimum_visibility)) {
      const float receiver_depth = depth.Load(int3(position, 0)).x;
      const int2 last_pixel = int2(metadata.xy) - 1;
      const float left_depth = depth.Load(int3(max(int2(position) - int2(1, 0), 0), 0)).x;
      const float right_depth = depth.Load(int3(min(int2(position) + int2(1, 0), last_pixel), 0)).x;
      const float upper_depth = depth.Load(int3(max(int2(position) - int2(0, 1), 0), 0)).x;
      const float lower_depth = depth.Load(int3(min(int2(position) + int2(0, 1), last_pixel), 0)).x;
      // Prefer the continuous side of an edge. A quad derivative can span two
      // objects and mistake their depth jump for a steep plane. Opposite
      // slopes indicate a thin peak or gap, where neither neighbor is a plane.
      const float2 backward_difference = receiver_depth - float2(left_depth, upper_depth);
      const float2 forward_difference = float2(right_depth, lower_depth) - receiver_depth;
      float2 depth_gradient = abs(backward_difference) < abs(forward_difference) ? backward_difference : forward_difference;
      depth_gradient = backward_difference * forward_difference >= 0.0 ? depth_gradient : 0.0;
      const float tolerance = max(receiver_depth * 0.005, 2e-7);
      float visibility_sum = 0.0;
      float weight_sum = 0.0;
      if (wide) {
        // Eight taps over a disk of the pixel's radius, turned by a different
        // angle at each pixel and frame so temporal reconstruction fills the
        // disk in. The Gaussian is the same as below (sigma = radius / 2).
        // The filter runs on shadowed pixels only, so the soft edge lies
        // inside the shadow: where lit taps hold half the weight, the pixel is
        // lit. A shadow narrower than its soft edge is mostly lost.
        const float angle = 6.28318530718 * frac(52.9829189 * frac(dot(pixel + 5.588238 * CUSTOM_SHADOW_NOISE_FRAME, float2(0.06711056, 0.00583715))));
        const float2 turn = float2(cos(angle), sin(angle));
        const float falloff = 2.88539008178 / (wide_radius * wide_radius);  // 2 / ln(2), for exp2.
        visibility_sum = sample.x;
        weight_sum = 1.0;
        float lit_sum = 0.0;
        [unroll]
        for (uint tap = 0u; tap < 8u; ++tap) {
          const float2 direction = PCSS_DIRECTIONS[tap];
          const int2 offset = int2(round(float2(direction.x * turn.x - direction.y * turn.y, direction.x * turn.y + direction.y * turn.x)
                                         * (wide_radius * sqrt((float(tap) + 0.5) / 8.0))));
          const int2 neighbor = int2(position) + offset;
          // A tap rounded onto the center would count it twice.
          if (all(offset == 0) || any(neighbor < int2(bounds.xy)) || any(neighbor >= int2(bounds.zw))) continue;
          const float2 value = ReadBendMask(neighbor, bounds, atlas);
          const float neighbor_depth = depth.Load(int3(neighbor, 0)).x;
          const float plane_weight = saturate(1.0 - abs(neighbor_depth - (receiver_depth + dot(depth_gradient, float2(offset)))) / tolerance);
          const float weight = value.y >= 0.24 && neighbor_depth > 0.0
                                   ? exp2(-dot(float2(offset), float2(offset)) * falloff) * plane_weight
                                   : 0.0;
          visibility_sum += value.x * weight;
          lit_sum += value.x >= 0.999 ? weight : 0.0;
          weight_sum += weight;
        }
        result.visibility = lerp(saturate(visibility_sum / weight_sum), 1.0, saturate(2.0 * lit_sum / weight_sum) * widening.y);
      } else {
        // Approximate the source footprint with a Gaussian (sigma = radius / 2),
        // truncated to neighboring texels. Normalized weights preserve constant
        // visibility without moving the light or animating a whole shadow edge.
        const float falloff = 2.88539008178 / (radius * radius);  // 2 / ln(2), for exp2.
        // This 3-by-3 kernel has only three distances: center, axis, and diagonal.
        const float2 spatial_weights = exp2(-float2(1.0, 2.0) * falloff);
        [unroll]
        for (int index = 0; index < 9; ++index) {
          const int2 offset = int2(index % 3 - 1, index / 3 - 1);
          const float2 value = neighborhood[index];
          if (value.y < 0.24) continue;
          float neighbor_depth;
          switch (index) {
            case 1:  neighbor_depth = upper_depth; break;
            case 3:  neighbor_depth = left_depth; break;
            case 4:  neighbor_depth = receiver_depth; break;
            case 5:  neighbor_depth = right_depth; break;
            case 7:  neighbor_depth = lower_depth; break;
            default: neighbor_depth = depth.Load(int3(int2(position) + offset, 0)).x; break;
          }
          const float expected_depth = receiver_depth + dot(depth_gradient, float2(offset));
          const float plane_weight = saturate(1.0 - abs(neighbor_depth - expected_depth) / tolerance);
          const float spatial_weight = index == 4 ? 1.0 : ((index & 1) != 0 ? spatial_weights.x : spatial_weights.y);
          const float weight = neighbor_depth > 0.0 ? spatial_weight * plane_weight : 0.0;
          visibility_sum += value.x * weight;
          weight_sum += weight;
        }
        if (weight_sum > 0.0) {
          // Normalize the accepted taps so a constant visibility stays constant.
          // Depth rejection removes samples instead of introducing dark borders.
          result.visibility = saturate(visibility_sum / weight_sum);
        }
        if (sample.x >= 1.0) {
          result.visibility = lerp(result.visibility, 1.0, widening.x);
        }
      }
    }
  }
  result.debug_value = debug == 1 ? result.visibility : saturate((sample.y - 0.25) / 0.75);
  if (debug == 6 && result.valid) {
    // The Toward Light view draws a grid of arrows pointing at the light.
    // Behind-camera sources project to the opposite side. W restores the
    // physical direction toward the light, including MGSV's signed X axis.
    const float4 light = asfloat(bend_shadow_data.Load4(0));
    const float2 to_light = (light.xy - pixel) * light.w;
    const float2 direction = to_light * rsqrt(max(dot(to_light, to_light), 1e-12));
    const float2 cell = frac(pixel / 64.0) * 64.0 - 32.0;
    const float along = dot(cell, direction);
    const float across = abs(dot(cell, float2(-direction.y, direction.x)));
    const bool shaft = along >= -18.0 && along <= 8.0 && across < 1.5;
    const bool head = along >= 5.0 && along <= 18.0 && across < (18.0 - along) * 0.65;
    result.debug_value = shaft || head ? 1.0 : 0.08;
  }
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

// Applies a light's contact shadow to its finished direct light, for a light
// without a shadow map. A pixel the light does not reach (outside its cone or
// range, or facing away) has nothing to darken, so its mask is not read; most
// pixels of a local light's draw are such pixels. The contact debug views
// write every pixel.
void ApplyBendShadow(float2 pixel, uint light_type, Texture2D<float4> depth, inout float3 diffuse, inout float3 specular) {
  [branch]
  if (any(diffuse != 0.0) || any(specular != 0.0) || CUSTOM_CONTACT_SHADOW_DEBUG_VIEW != 0.0) {
    const BendShadow contact_shadow = ReadBendShadow(pixel, light_type, depth);
    diffuse *= contact_shadow.visibility;
    specular *= contact_shadow.visibility;
    ApplyBendDebug(contact_shadow, diffuse, specular);
  }
}

#endif
