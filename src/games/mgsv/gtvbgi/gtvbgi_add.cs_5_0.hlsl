// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

// MGSV GT-VBGI - full-resolution diffuse lighting and specular visibility.
//
// Reconstruct ambient and shared lighting before skin scattering. AO changes
// ambient diffuse only; direct light and visibility-integrated GI stay separate.
// Fog, tone curves, and display encoding still happen in the game's later passes.

#include "../gbuffer.hlsli"
#include "../shared.h"
#include "specular_occlusion.hlsli"

Texture2D<float4> gi_buffer : register(t0);
Texture2D<float4> depth_buffer : register(t1);
Texture2D<float4> diagnostic_buffer : register(t2);
Texture2D<float2> ao_buffer : register(t3);
Texture2D<float4> normal_buffer : register(t4);
Texture2D<float4> environment_specular : register(t5);
Texture2D<float4> native_material : register(t6);
Texture2D<float4> raw_specular : register(t7);
Texture2D<float4> temporal_specular : register(t8);
Texture2D<float4> filtered_specular : register(t9);
Texture2D<float4> ambient_diffuse : register(t10);
Texture2D<float4> ambient_weights : register(t11);
SamplerState ambient_point_wrap : register(s0);
cbuffer AmbientSampling : register(b3) {
  float4 ambient_object_unused[9];
  float4 ambient_local_params[4];
}
RWTexture2D<float4> light_output : register(u0);
RWTexture2D<float2> specular_output : register(u1);
RWTexture2D<float4> scattering_copy : register(u2);

[numthreads(8, 8, 1)]

void main(uint3 thread_id : SV_DispatchThreadID) {
  const uint2 pixel = thread_id.xy;
  if (pixel.x >= buffer_width || pixel.y >= buffer_height) return;

  const float scale = max(CUSTOM_MGSV_GT_RESOLUTION_DIVISOR, 1.0);
  const float2 position = (float2(pixel) + 0.5) / scale - 0.5;
  const int2 base = (int2)floor(position);
  const float2 fraction = frac(position);

  const float depth = depth_buffer.Load(int3(pixel, 0)).x;
  const float z = LinearZ(depth);
  if (!(z > 0.0) || z > 1.0e6) {
    specular_output[pixel] = 0.0;
    return;
  }
  const bool gi_receiver_valid = depth > 0.0 && z < 1.0e6;
  const float inv_z = 1.0 / z;
  uint gi_width, gi_height;
  gi_buffer.GetDimensions(gi_width, gi_height);

  float3 radiance = 0.0;
  float weight_sum = 0.0;
  float visibility = 0.0;
  float visibility_weight = 0.0;
  const int diagnostic = (int)clamp(CUSTOM_MGSV_GT_DEBUG_VIEW, 0.0, 22.0);
  const bool specular_active = CUSTOM_MGSV_SPECULAR_ACTIVE >= 0.5;
  float3 center_normal = float3(0.0, 0.0, -1.0);
  float3 center_reflection = 0.0;
  float3 center_lobe = 0.0;
  float center_side = 1.0;
  bool specular_receiver = false;
  float2 specular_mass_sum = 0.0;
  float specular_weight_sum = 0.0;
  float specular_support_sum = 0.0;
  if (specular_active) {
    center_normal = DecodeNormal(normal_buffer.Load(int3(pixel, 0)).xyz);
  }
  if (specular_active && all(isfinite(center_normal))) {
    const float4 captured = environment_specular.Load(int3(pixel, 0));
    const float center_roughness = native_material.Load(int3(pixel, 0)).r;
    if (captured.a > 0.5 && all(isfinite(captured)) && isfinite(center_roughness)
        && center_roughness >= 0.0 && center_roughness <= 1.0) {
      const float3 view = normalize(-ViewPos(float2(pixel) + 0.5, z));
      center_reflection = reflect(-view, center_normal);
      center_lobe = GetSpecularLobeParameters(center_roughness);
      center_side = GetSpecularSurfaceSide(center_normal, view);
      specular_receiver = true;
    }
  }
  float3 diagnostic_sum = 0.0;
  float diagnostic_weight = 0.0;
  [unroll]
  for (int y = 0; y < 2; ++y) {
    [unroll]
    for (int x = 0; x < 2; ++x) {
      const int2 q = clamp(base + int2(x, y), int2(0, 0), int2(gi_width, gi_height) - 1);
      const float bilinear = (x != 0 ? fraction.x : 1.0 - fraction.x)
                             * (y != 0 ? fraction.y : 1.0 - fraction.y);

      const int2 full = clamp((int2)floor((float2(q) + 0.5) * scale), int2(0, 0),
                              int2((int)buffer_width - 1, (int)buffer_height - 1));
      const float qdepth = depth_buffer.Load(int3(full, 0)).x;
      const float qz = LinearZ(qdepth);
      const float depth_match = qz > 0.0 ? 1.0 / (1.0 + 32.0 * abs(qz - z) * inv_z) : 1.0;

      const float weight = bilinear * depth_match;
      const float ao = ao_buffer.Load(int3(q, 0)).r;
      visibility += (isfinite(ao) ? saturate(ao) : 1.0) * weight;
      visibility_weight += weight;
      // Preserve the AO resolve's neutral background taps. GI has stricter
      // geometry validity and must keep a separate normalization sum.
      if (!gi_receiver_valid || !(qdepth > 0.0 && qz > 0.0 && qz < 1.0e6)) continue;
      // Valid black remains in the weight sum; missing/invalid results do not.
      const float4 tap = gi_buffer.Load(int3(q, 0));
      if (tap.a > 0.5 && all(isfinite(tap.rgb))) {
        radiance += tap.rgb * weight;
        weight_sum += weight;
      }
      // Invalid GI must remain visible in the validity view. Diagnostic RGB
      // therefore has its own weights and never feeds the radiance result.
      if ((diagnostic >= 2 && diagnostic <= 11)
          || (diagnostic == 17 && CUSTOM_MGSV_SPECULAR_HISTORY_VALID >= 0.5)) {
        diagnostic_sum += diagnostic_buffer.Load(int3(q, 0)).rgb * weight;
        diagnostic_weight += weight;
      }
      if (specular_receiver) {
        const float3 sample_normal = DecodeNormal(normal_buffer.Load(int3(full, 0)).xyz);
        if (all(isfinite(sample_normal))) {
          float normal_weight = saturate(dot(center_normal, sample_normal));
          normal_weight *= normal_weight;
          normal_weight *= normal_weight;
          normal_weight *= normal_weight;
          normal_weight *= normal_weight;
          const float4 sample = (diagnostic == 14   ? raw_specular.Load(int3(q, 0))
                                 : diagnostic == 15 ? temporal_specular.Load(int3(q, 0))
                                                    : filtered_specular.Load(int3(q, 0)));
          if (IsSpecularSampleValid(sample)) {
            const float3 view = normalize(-ViewPos(float2(full) + 0.5, qz));
            if (GetSpecularSurfaceSide(sample_normal, view) != center_side) continue;
            const float3 reflection = reflect(-view, sample_normal);
            const float lobe_weight = GetSpecularLobeWeight(center_reflection, center_lobe, reflection, sample.z);
            const float specular_weight = weight * normal_weight * lobe_weight;
            specular_mass_sum += sample.xy * specular_weight;
            specular_support_sum += sample.a * specular_weight;
            specular_weight_sum += specular_weight;
          }
        }
      }
    }
  }

  const float3 bounce = radiance / max(weight_sum, 1.0e-4) * CUSTOM_MGSV_GI_STRENGTH;
  const float4 center_light = light_output.Load(pixel);
  const float ao = visibility_weight > 1.0e-4 ? visibility / visibility_weight : 1.0;
  float3 ambient = 0.0;
  bool ambient_valid = false;
  uint ambient_width, ambient_height;
  ambient_diffuse.GetDimensions(ambient_width, ambient_height);
  if (ambient_width == buffer_width / 2u && ambient_height == buffer_height / 2u
      && ambient_width > 0u && ambient_height > 0u) {
    // Match BilateralUpscale2x2: point-wrap sampling, four pixel phases, and
    // raw-depth weights. The GT resolve above uses a different reconstruction.
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
    // Native ambient was rounded to FP16 before direct lighting was added.
    // Reject larger mismatches; absorb only the small rounding uncertainty.
    const float3 rounding = max(abs(ambient), abs(center_light.rgb)) * (2.0 / 1024.0) + 1.2e-7;
    ambient_valid = all(float4(a.a, b.a, c.a, d.a) > 0.5)
                    && all(isfinite(ambient)) && all(ambient >= 0.0)
                    && all(ambient <= center_light.rgb + rounding)
                    && all(abs(size - full_size) < 0.5);
    ambient = ambient_valid ? min(ambient, max(center_light.rgb, 0.0)) : 0.0;
  }
  const float3 direct = center_light.rgb - ambient;
  const float3 shaded_diffuse = direct + ambient * saturate(ao);
  if (!gi_receiver_valid) {
    light_output[pixel] = float4(shaded_diffuse, center_light.a);
    specular_output[pixel] = 0.0;
    return;
  }
  // Debug views replace diffuse only; surface color, native specular, fog,
  // and later image processing still affect how they look.
  // Both native destinations receive the same RGB before scattering, with
  // their own alpha preserved: accumulation alpha and the skin-material mask.
  float3 output = (diagnostic == 1) ? bounce : shaded_diffuse + bounce;
  if (diagnostic >= 2 && diagnostic <= 11) output = diagnostic_sum / max(diagnostic_weight, 1.0e-4);
  if (diagnostic == 6) output *= CUSTOM_MGSV_GI_STRENGTH;
  float2 specular = 0.0;
  if (diagnostic >= 14 && diagnostic <= 18) output = float3(1.0, 0.0, 1.0);
  if (specular_receiver && specular_weight_sum > 1e-4) {
    const float2 masses = specular_mass_sum / specular_weight_sum;
    const float support = saturate(specular_support_sum / specular_weight_sum)
                          * saturate(specular_weight_sum / max(visibility_weight, 1e-4));
    const bool has_mass = masses.y > 1e-7;
    if (has_mass) {
      const float mass_support = 1.0 - smoothstep(0.35, 0.75, abs(log2(masses.y)));
      // Deferred composition keeps native AO wherever our estimate lacks
      // coverage. A valid black estimate still has positive confidence.
      specular = float2(saturate(masses.x / masses.y), support * mass_support);
    }
    if (diagnostic >= 14 && diagnostic <= 16) output = (has_mass ? specular.xxx : float3(0.0, 1.0, 1.0));
    if (diagnostic == 18) output = specular.yyy;
  }
  if (diagnostic == 17 && specular_active) output = diagnostic_sum / max(diagnostic_weight, 1e-4);
  if (diagnostic == 12 || diagnostic == 13) {
    // Missing capture must be distinct from a valid black environment result.
    output = float3(1.0, 0.0, 1.0);
    const float4 captured = environment_specular.Load(int3(pixel, 0));
    if (captured.a > 0.5 && all(isfinite(captured))) {
      output = (diagnostic == 12 ? native_material.Load(int3(pixel, 0)).rrr : captured.rgb);
    }
  }
  if (diagnostic == 19) output = ambient_valid ? ambient : float3(1.0, 0.0, 1.0);
  if (diagnostic == 20) output = ambient_valid ? direct : float3(1.0, 0.0, 1.0);
  if (diagnostic == 21) output = shaded_diffuse + bounce;
  if (diagnostic == 22) output = ambient_valid ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 1.0);
  // Preserve finite values in the native RGBA16F destination at high strengths.
  light_output[pixel] = float4(min(output, 65504.0), center_light.a);
  uint copy_width, copy_height;
  scattering_copy.GetDimensions(copy_width, copy_height);
  if (copy_width == buffer_width && copy_height == buffer_height) {
    const float4 original = scattering_copy.Load(pixel);
    if (original.a > 0.0) scattering_copy[pixel] = float4(min(output, 65504.0), original.a);
  }
  specular_output[pixel] = specular;
}
