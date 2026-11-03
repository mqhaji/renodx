// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

// MGSV GT-VBGI - full-resolution diffuse lighting and specular visibility.
//
// Reconstruct the shared lighting at full resolution. Apply AO to native
// diffuse, add the bounce, and save specular visibility for deferred composition.
// Fog, tone curves, and display encoding still happen in the game's later passes.

#include "../gbuffer.hlsli"
#include "../shared.h"
#include "specular_occlusion.hlsli"

Texture2D<float4> giBuffer : register(t0);
Texture2D<float4> depthBuffer : register(t1);
Texture2D<float4> diagnosticBuffer : register(t2);
Texture2D<float2> aoBuffer : register(t3);
Texture2D<float4> normal_buffer : register(t4);
Texture2D<float4> environment_specular : register(t5);
Texture2D<float4> native_material : register(t6);
Texture2D<float4> raw_specular : register(t7);
Texture2D<float4> temporal_specular : register(t8);
Texture2D<float4> filtered_specular : register(t9);
RWTexture2D<float4> lightOutput : register(u0);
RWTexture2D<float2> specular_output : register(u1);

[numthreads(8, 8, 1)]

void main(uint3 tid : SV_DispatchThreadID) {
  const uint2 pixel = tid.xy;
  if (pixel.x >= cBufferWidth || pixel.y >= cBufferHeight) return;

  const float scale = max(CUSTOM_MGSV_GT_RESOLUTION_DIVISOR, 1.0);
  const float2 position = (float2(pixel) + 0.5) / scale - 0.5;
  const int2 base = (int2)floor(position);
  const float2 fraction = frac(position);

  const float depth = depthBuffer.Load(int3(pixel, 0)).x;
  const float z = LinearZ(depth);
  if (!(z > 0.0) || z > 1.0e6) {
    specular_output[pixel] = 0.0;
    return;
  }
  const bool giReceiverValid = depth > 0.0 && z < 1.0e6;
  const float inv_z = 1.0 / z;
  uint gi_width, gi_height;
  giBuffer.GetDimensions(gi_width, gi_height);

  float3 radiance = 0.0;
  float weight_sum = 0.0;
  float visibility = 0.0;
  float visibility_weight = 0.0;
  const int diagnostic = (int)clamp(CUSTOM_MGSV_GT_DEBUG_VIEW, 0.0, 18.0);
  const bool specular_active = CUSTOM_MGSV_SPECULAR_ACTIVE >= 0.5;
  float3 center_normal = float3(0.0, 0.0, -1.0);
  float3 center_reflection = 0.0;
  float3 center_lobe = 0.0;
  float center_roughness = 0.0;
  bool specular_receiver = false;
  float2 specular_mass_sum = 0.0;
  float specular_weight_sum = 0.0;
  float specular_support_sum = 0.0;
  if (specular_active) {
    center_normal = DecodeNormal(normal_buffer.Load(int3(pixel, 0)).xyz);
  }
  if (specular_active && all(isfinite(center_normal))) {
    const float4 captured = environment_specular.Load(int3(pixel, 0));
    center_roughness = native_material.Load(int3(pixel, 0)).r;
    if (captured.a > 0.5 && all(isfinite(captured)) && isfinite(center_roughness)
        && center_roughness >= 0.0 && center_roughness <= 1.0) {
      const float3 view = normalize(-ViewPos(float2(pixel) + 0.5, z));
      center_reflection = reflect(-view, center_normal);
      center_lobe = SpecularLobeParameters(center_roughness);
      specular_receiver = dot(center_normal, view) > 0.0;
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
                              int2((int)cBufferWidth - 1, (int)cBufferHeight - 1));
      const float qdepth = depthBuffer.Load(int3(full, 0)).x;
      const float qz = LinearZ(qdepth);
      const float depth_match = qz > 0.0 ? 1.0 / (1.0 + 32.0 * abs(qz - z) * inv_z) : 1.0;

      const float weight = bilinear * depth_match;
      const float ao = aoBuffer.Load(int3(q, 0)).r;
      visibility += (isfinite(ao) ? saturate(ao) : 1.0) * weight;
      visibility_weight += weight;
      // Preserve the AO resolve's neutral background taps. GI has stricter
      // geometry validity and must keep a separate normalization sum.
      if (!giReceiverValid || !(qdepth > 0.0 && qz > 0.0 && qz < 1.0e6)) continue;
      // Valid black remains in the weight sum; missing/invalid results do not.
      const float4 tap = giBuffer.Load(int3(q, 0));
      if (tap.a > 0.5 && all(isfinite(tap.rgb))) {
        radiance += tap.rgb * weight;
        weight_sum += weight;
      }
      // Invalid GI must remain visible in the validity view. Diagnostic RGB
      // therefore has its own weights and never feeds the radiance result.
      if ((diagnostic >= 2 && diagnostic <= 11)
          || (diagnostic == 17 && CUSTOM_MGSV_SPECULAR_HISTORY_VALID >= 0.5)) {
        diagnostic_sum += diagnosticBuffer.Load(int3(q, 0)).rgb * weight;
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
          if (ValidSpecularMasses(sample)) {
            const float3 view = normalize(-ViewPos(float2(full) + 0.5, qz));
            const float3 reflection = reflect(-view, sample_normal);
            const float lobe_weight = SpecularLobeWeight(center_reflection, center_lobe, reflection, sample.z);
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
  const float4 centre = lightOutput.Load(pixel);
  const float ao = visibility_weight > 1.0e-4 ? visibility / visibility_weight : 1.0;
  const float3 shadedDiffuse = centre.rgb * saturate(ao);
  if (!giReceiverValid) {
    lightOutput[pixel] = float4(shadedDiffuse, centre.a);
    specular_output[pixel] = 0.0;
    return;
  }
  // Debug views replace diffuse only; surface color, native specular, fog,
  // and later image processing still affect how they look.
  // AO affects existing diffuse only. One FP16 store avoids rounding the AO
  // product before GI is added, while preserving the game's alpha exactly.
  float3 output = (diagnostic == 1) ? bounce : shadedDiffuse + bounce;
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
  // Preserve finite values in the native RGBA16F destination at high strengths.
  lightOutput[pixel] = float4(min(output, 65504.0), centre.a);
  specular_output[pixel] = specular;
}
