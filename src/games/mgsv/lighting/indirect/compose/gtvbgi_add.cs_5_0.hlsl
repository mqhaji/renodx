// SPDX-License-Identifier: MIT
// Copyright and license notices: ../README.md#credits-and-license.

// GT-VBGI application, run at full render resolution just before the game's
// first skin-scattering pass.
//
// Reads the lighting-resolution trace result (packed as in
// indirect_diffuse.hlsli), depth, and the game's half-resolution ambient
// diffuse with its upsample weights. Updates the native diffuse light buffer
// and the skin-scattering copy in place, in linear pre-exposed light. AO
// darkens only the native ambient part, direct light is kept, and GI is added
// once. Fog, tone curves, and display encoding happen in later game passes.
// It also records, for the next frame's source pass (gtvbgi_source), how much
// it changed each pixel's light, with the pixel's view depth; 0 depth where
// it records nothing.

#include "../../../rendering/geometry.hlsli"
#include "../../../shared.h"
#include "../indirect_diffuse.hlsli"

Texture2D<float4> lighting_buffer : register(t0);
Texture2D<float4> depth_buffer : register(t1);
Texture2D<float4> diagnostic_buffer : register(t2);
Texture2D<float4> ambient_diffuse : register(t10);
Texture2D<float4> ambient_weights : register(t11);
SamplerState ambient_point_wrap : register(s0);
cbuffer AmbientSampling : register(b3) {
  float4 ambient_object_unused[9];
  float4 ambient_local_params[4];
}
RWTexture2D<float4> light_output : register(u0);
RWTexture2D<float4> light_change : register(u1);
RWTexture2D<float4> scattering_copy : register(u2);

[numthreads(8, 8, 1)] void main(uint3 thread_id : SV_DispatchThreadID) {
  // 1. Reconstruct full-resolution lighting from four low-resolution taps.
  // Depth weights keep foreground bounce from spreading into the background.
  // AO and GI are each normalized over their own valid taps.
  const uint2 pixel = thread_id.xy;
  if (pixel.x >= buffer_width || pixel.y >= buffer_height) return;
  // The trace placed lighting pixel q at full-resolution pixel
  // floor((q + 0.5) * scale), which is q * scale + floor(scale / 2) for the
  // divisors 1, 2, 4 and 8, so that pixel takes q's result alone.
  const float scale = max(CUSTOM_MGSV_LIGHTING_RESOLUTION_DIVISOR, 1.0);
  const float2 position = (float2(pixel) - floor(0.5 * scale)) / scale;
  const int2 base = (int2)floor(position);
  const float2 fraction = frac(position);

  const float depth = depth_buffer.Load(int3(pixel, 0)).x;
  const float z = LinearZ(depth);
  light_change[pixel] = 0.0;
  if (!(z > 0.0) || z > 1.0e6) {
    return;
  }
  const bool gi_receiver_valid = depth > 0.0 && z < 1.0e6;
  const float inv_z = 1.0 / z;
  uint gi_width, gi_height;
  lighting_buffer.GetDimensions(gi_width, gi_height);

  float3 radiance = 0.0;
  float weight_sum = 0.0;
  float visibility = 0.0;
  float visibility_weight = 0.0;
  const int diagnostic = (int)clamp(CUSTOM_MGSV_LIGHTING_DEBUG_VIEW, 0.0, 17.0);
  float3 diagnostic_sum = 0.0;
  float diagnostic_weight = 0.0;
  [unroll] for (int y = 0; y < 2; ++y) {
    [unroll] for (int x = 0; x < 2; ++x) {
      const int2 q = clamp(base + int2(x, y), int2(0, 0), int2(gi_width, gi_height) - 1);
      const float bilinear = (x != 0 ? fraction.x : 1.0 - fraction.x)
                             * (y != 0 ? fraction.y : 1.0 - fraction.y);

      const int2 full = clamp((int2)floor((float2(q) + 0.5) * scale), int2(0, 0),
                              int2((int)buffer_width - 1, (int)buffer_height - 1));
      const float qdepth = depth_buffer.Load(int3(full, 0)).x;
      const float qz = LinearZ(qdepth);
      const float depth_match = qz > 0.0 ? 1.0 / (1.0 + 32.0 * abs(qz - z) * inv_z) : 1.0;

      const float weight = bilinear * depth_match;
      const float4 tap = lighting_buffer.Load(int3(q, 0));
      visibility += (HasAmbientVisibility(tap) ? saturate(tap.a) : 1.0) * weight;
      visibility_weight += weight;
      // AO keeps background taps, which carry neutral visibility. GI skips taps
      // without valid geometry and keeps its own weight sum.
      if (!gi_receiver_valid || !(qdepth > 0.0 && qz > 0.0 && qz < 1.0e6)) continue;
      // Valid black stays in the weight sum; missing or invalid results do not.
      if (HasDiffuseGI(tap)) {
        radiance += tap.rgb * weight;
        weight_sum += weight;
      }
      // Debug views keep their own weights so invalid GI still shows in the
      // validity view. They never feed the radiance result.
      if (diagnostic >= 2 && diagnostic <= 6) {
        diagnostic_sum += diagnostic_buffer.Load(int3(q, 0)).rgb * weight;
        diagnostic_weight += weight;
      }
    }
  }

  // 2. Rebuild the native ambient diffuse exactly as the game upsampled it, so
  // subtracting it from the light buffer leaves direct light unchanged. The
  // trace upsample above differs and cannot be reused for this.
  const float3 bounce = radiance / max(weight_sum, 1.0e-4) * CUSTOM_MGSV_DIFFUSE_GI_STRENGTH;
  const float4 center_light = light_output.Load(pixel);
  const float ao = visibility_weight > 1.0e-4 ? visibility / visibility_weight : 1.0;
  float3 ambient = 0.0;
  bool ambient_valid = false;
  uint ambient_width, ambient_height;
  ambient_diffuse.GetDimensions(ambient_width, ambient_height);
  if (ambient_width == buffer_width / 2u && ambient_height == buffer_height / 2u
      && ambient_width > 0u && ambient_height > 0u) {
    // Match the game's BilateralUpscale2x2: point-wrap sampling, four pixel
    // phases, and weights from raw depth differences.
    const float2 full_size = float2(buffer_width, buffer_height);
    const float2 size = ambient_local_params[0].xy;
    const float2 inverse_size = ambient_local_params[0].zw;
    const float2 vertex_position = (float2(pixel) + 0.5) / full_size * size;
    const float center_depth = depth_buffer.SampleLevel(ambient_point_wrap, vertex_position * inverse_size, 0).r;
    float4 weights = ambient_weights.SampleLevel(ambient_point_wrap, vertex_position * 0.25, 0);
    const float2 block = floor(float2(pixel) * 0.5) * 2.0;
    const float2 lower = (block + 0.5) * inverse_size;
    const float2 upper = (block + 2.5) * inverse_size;
    const float4 depths = float4(
        depth_buffer.SampleLevel(ambient_point_wrap, lower, 0).r,
        depth_buffer.SampleLevel(ambient_point_wrap, float2(upper.x, lower.y), 0).r,
        depth_buffer.SampleLevel(ambient_point_wrap, float2(lower.x, upper.y), 0).r,
        depth_buffer.SampleLevel(ambient_point_wrap, upper, 0).r);
    weights /= abs(depths - center_depth) + 1e-6;
    weights /= max(dot(weights, 1.0.xxxx), 1e-20);
    const float4 a = ambient_diffuse.SampleLevel(ambient_point_wrap, lower, 0);
    const float4 b = ambient_diffuse.SampleLevel(ambient_point_wrap, float2(upper.x, lower.y), 0);
    const float4 c = ambient_diffuse.SampleLevel(ambient_point_wrap, float2(lower.x, upper.y), 0);
    const float4 d = ambient_diffuse.SampleLevel(ambient_point_wrap, upper, 0);
    ambient = a.rgb * weights.x + b.rgb * weights.y + c.rgb * weights.z + d.rgb * weights.w;
    // Native ambient was rounded to FP16 before direct lighting was added, so
    // allow that rounding error when checking ambient <= total light. Reject
    // larger mismatches.
    const float3 rounding = max(abs(ambient), abs(center_light.rgb)) * (2.0 / 1024.0) + 1.2e-7;
    ambient_valid = all(float4(a.a, b.a, c.a, d.a) > 0.5)
                    && all(isfinite(ambient)) && all(ambient >= 0.0)
                    && all(ambient <= center_light.rgb + rounding)
                    && all(abs(size - full_size) < 0.5);
    ambient = ambient_valid ? min(ambient, max(center_light.rgb, 0.0)) : 0.0;
  }
  // 3. AO attenuates ambient alone. GI already integrates sampled visibility,
  // so multiplying it by AO again would double-count the same obstruction.
  // Receiving albedo is applied once later by native deferred composition.
  const float3 direct = center_light.rgb - ambient;
  const float3 shaded_diffuse = direct + ambient * saturate(ao);
  if (!gi_receiver_valid) {
    light_output[pixel] = float4(shaded_diffuse, center_light.a);
    return;
  }
  const float3 lit = shaded_diffuse + bounce;
  if (all(isfinite(lit))) light_change[pixel] = float4(clamp(lit - center_light.rgb, -65504.0, 65504.0), z);
  // Debug views replace diffuse only; surface color, native specular, fog,
  // and later image processing still affect how they look.
  float3 output = (diagnostic == 1) ? bounce : shaded_diffuse + bounce;
  if (diagnostic >= 2 && diagnostic <= 6) output = diagnostic_sum / max(diagnostic_weight, 1.0e-4);
  if (diagnostic == 7) output = ambient_valid ? ambient : float3(1.0, 0.0, 1.0);
  if (diagnostic == 8) output = ambient_valid ? direct : float3(1.0, 0.0, 1.0);
  if (diagnostic == 9) output = shaded_diffuse + bounce;
  if (diagnostic == 10) output = ambient_valid ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 1.0);
  // 4. Write the same RGB to both buffers the skin-scattering passes read. Each
  // keeps its own alpha: accumulation alpha in the light buffer, the skin
  // mask in the copy. Only skin pixels of the copy are written. Clamp to the
  // RGBA16F maximum so high strengths stay finite.
  light_output[pixel] = float4(min(output, 65504.0), center_light.a);
  uint copy_width, copy_height;
  scattering_copy.GetDimensions(copy_width, copy_height);
  if (copy_width == buffer_width && copy_height == buffer_height) {
    const float4 original = scattering_copy.Load(pixel);
    if (original.a > 0.0) scattering_copy[pixel] = float4(min(output, 65504.0), original.a);
  }
}
