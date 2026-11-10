// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

#include "../gbuffer.hlsli"
#include "../shared.h"
#include "indirect_diffuse.hlsli"
#include "specular_occlusion.hlsli"

// Smooth nearby lighting without crossing depth, normal, or reflection edges.
// Specular visible/open areas are averaged together, then divided during resolve.

Texture2D<float4> lighting_input : register(t0);
Texture2D<float4> depth_buffer : register(t1);
Texture2D<float4> normal_buffer : register(t2);
Texture2D<float4> specular_input : register(t4);
Texture2DArray<float2> filter_noise : register(t5);
RWTexture2D<float4> lighting_output : register(u0);
RWTexture2D<float4> diagnostic_output : register(u2);
RWTexture2D<float4> specular_output : register(u3);

// Columns contain the exact 3-, 5-, 7-, and 9-tap binomial coefficients.
// Rows are the absolute offset from the center, so both axes share this table.
static const float4 BINOMIAL_WEIGHTS[5] = {
    float4(2.0, 6.0, 20.0, 70.0),
    float4(1.0, 4.0, 15.0, 56.0),
    float4(0.0, 1.0, 6.0, 28.0),
    float4(0.0, 0.0, 1.0, 8.0),
    float4(0.0, 0.0, 0.0, 1.0),
};

[numthreads(8, 8, 1)]
void main(uint3 thread_id : SV_DispatchThreadID) {
  // 1. Start from the center's current result. Disabled filtering and rejected
  // geometry copy it through; neighbors cannot turn a missing center into valid
  // data or remove a valid black/fully blocked sample from the weight sum.
  uint width, height;
  lighting_input.GetDimensions(width, height);
  const int2 pixel = int2(thread_id.xy);
  if (any(pixel >= int2(width, height))) return;

  const float4 center = lighting_input.Load(int3(pixel, 0));
  const bool gi_valid = HasDiffuseGI(center);
  const bool ao_valid = HasAmbientVisibility(center);
  const float current_visibility = ao_valid ? saturate(center.a) : 1.0;
  float3 gi_result = gi_valid ? center.rgb : 0.0;
  float ao_result = current_visibility;
  const bool specular_active = CUSTOM_MGSV_SPECULAR_ACTIVE >= 0.5;
  float4 specular_center = float4(0.0, 0.0, 0.0, -1.0);
  if (specular_active) {
    specular_center = specular_input.Load(int3(pixel, 0));
  }
  const bool specular_valid = specular_active && IsSpecularSampleValid(specular_center);
  float4 specular_result = specular_valid ? specular_center : float4(0.0, 0.0, 0.0, -1.0);

  [branch]
  if (CUSTOM_MGSV_GT_SPATIAL_ENABLED >= 0.5 && (gi_valid || ao_valid || specular_valid)) {
    // 2. Reconstruct native receiver geometry at the same representative pixel
    // used by the trace. Specular also retains its reflected direction and side.
    const float scale = max(CUSTOM_MGSV_GT_RESOLUTION_DIVISOR, 1.0);
    const int2 full_size = int2(buffer_width, buffer_height);
    const int2 center_pixel = min(int2(floor((float2(pixel) + 0.5) * scale)), full_size - 1);
    const float center_depth = depth_buffer.Load(int3(center_pixel, 0)).x;
    const float center_z = LinearZ(center_depth);
    const float3 center_normal = DecodeNormal(normal_buffer.Load(int3(center_pixel, 0)).xyz);
    if (center_depth > 0.0 && center_z > 0.0 && center_z < 1.0e6 && all(isfinite(center_normal))) {
      const float3 center_position = ViewPos(float2(center_pixel), center_z);
      float3 center_reflection = 0.0;
      float3 center_lobe = 0.0;
      float center_side = 1.0;
      if (specular_valid) {
        const float3 view = normalize(-ViewPos(float2(center_pixel) + 0.5, center_z));
        center_reflection = reflect(-view, center_normal);
        center_lobe = GetSpecularLobeParameters(specular_center.z);
        center_side = GetSpecularSurfaceSide(center_normal, view);
      }
      const float plane_scale = max(0.02 * length(center_position), 1.0e-6);
      // A-Trous increases spacing rather than allocating progressively smaller
      // images. The host supplies this pass's spacing in lighting pixels.
      const uint taps = (uint)clamp(CUSTOM_MGSV_GT_FILTER_TAPS, 3.0, 9.0) | 1u;
      const int half_width = int(taps / 2u);
      const int step = max((int)CUSTOM_MGSV_GT_FILTER_STEP, 1);

      // Center-relative FP32 sums preserve constant input exactly through
      // FP16 stores. Each signal keeps its own accepted weights and validity.
      precise float3 gi_sum = 0.0;
      precise float ao_sum = 0.0;
      // Always keep the center, then sample one of four neighbor lattices.
      const float center_weight = BINOMIAL_WEIGHTS[0][half_width - 1] * BINOMIAL_WEIGHTS[0][half_width - 1];
      float gi_weight_sum = gi_valid ? center_weight : 0.0;
      float ao_weight_sum = ao_valid ? center_weight : 0.0;
      precise float2 specular_sum = 0.0;
      float specular_weight_sum = specular_valid ? center_weight : 0.0;
      // This pattern is separate from the trace. Scene reconstruction smooths
      // its frame-to-frame changes; lighting history is saved before filtering.
      const uint frame = CUSTOM_TAA != 0.0 ? (uint)CUSTOM_MGSV_GT_NOISE_FRAME & 31u : 0u;
      const float noise = filter_noise.Load(int4((uint2(pixel) + uint2(37, 89)) & 127u, frame, 0)).x;
      // 3. Sample one parity pair of rows/columns. Multiply neighbor weights by
      // four to compensate for their selection probability; retain the center
      // exactly once. This is sparse filtering, not a partial-resolution output.
      const uint phase = min(uint(noise * 4.0), 3u);
      const int first_row = -half_width + int(((phase >> 1u) + uint(half_width)) & 1u);
      const int first_column = -half_width + int((phase + uint(half_width)) & 1u);
      [loop]
      for (int y = first_row; y <= half_width; y += 2) {
        [loop]
        for (int x = first_column; x <= half_width; x += 2) {
          if (x == 0 && y == 0) continue;
          const int2 offset = int2(x, y) * step;
          const int2 q = pixel + offset;
          if (any(q < 0) || any(q >= int2(width, height))) continue;
          const float4 sample = lighting_input.Load(int3(q, 0));
          const bool gi_sample_valid = gi_valid && HasDiffuseGI(sample);
          const bool ao_sample_valid = ao_valid && HasAmbientVisibility(sample);
          float4 specular_sample = float4(0.0, 0.0, 0.0, -1.0);
          if (specular_valid) {
            specular_sample = specular_input.Load(int3(q, 0));
          }
          const bool specular_sample_valid = specular_valid && IsSpecularSampleValid(specular_sample);
          if (!gi_sample_valid && !ao_sample_valid && !specular_sample_valid) continue;

          const float kernel_weight = BINOMIAL_WEIGHTS[abs(x)][half_width - 1] * BINOMIAL_WEIGHTS[abs(y)][half_width - 1];
          float lobe_weight = 1.0;
          const int2 sample_pixel = min(int2(floor((float2(q) + 0.5) * scale)), full_size - 1);
          const float sample_depth = depth_buffer.Load(int3(sample_pixel, 0)).x;
          const float sample_z = LinearZ(sample_depth);
          const float3 sample_normal = DecodeNormal(normal_buffer.Load(int3(sample_pixel, 0)).xyz);
          if (!(sample_depth > 0.0 && sample_z > 0.0 && sample_z < 1.0e6) || !all(isfinite(sample_normal))) continue;
          const float3 sample_position = ViewPos(float2(sample_pixel), sample_z);
          // Suppress taps off the receiver's plane or with a different normal.
          // Specular has an additional roughness/direction test because matching
          // surface geometry alone does not imply matching reflected lighting.
          const float plane = dot(center_normal, sample_position - center_position) / plane_scale;
          float normal_weight = saturate(dot(center_normal, sample_normal));
          normal_weight *= normal_weight;
          normal_weight *= normal_weight;
          normal_weight *= normal_weight;
          normal_weight *= normal_weight;
          const float edge_weight = exp(-0.5 * plane * plane) * normal_weight;
          if (specular_sample_valid) {
            const float3 view = normalize(-ViewPos(float2(sample_pixel) + 0.5, sample_z));
            const float3 reflection = reflect(-view, sample_normal);
            lobe_weight = (GetSpecularSurfaceSide(sample_normal, view) == center_side
                               ? GetSpecularLobeWeight(center_reflection, center_lobe, reflection, specular_sample.z)
                               : 0.0);
          }
          const float weight = kernel_weight * edge_weight * 4.0;
          if (gi_sample_valid) {
            gi_sum += (sample.rgb - center.rgb) * weight;
            gi_weight_sum += weight;
          }
          if (ao_sample_valid) {
            ao_sum += (sample.a - current_visibility) * weight;
            ao_weight_sum += weight;
          }
          if (specular_sample_valid) {
            const float specular_weight = weight * lobe_weight;
            specular_sum += (specular_sample.xy - specular_center.xy) * specular_weight;
            specular_weight_sum += specular_weight;
          }
        }
      }
      if (gi_weight_sum > 0.0 && all(isfinite(gi_sum))) {
        gi_result = min(center.rgb + gi_sum / gi_weight_sum, 65504.0);
      }
      if (ao_weight_sum > 0.0 && isfinite(ao_sum)) {
        ao_result = saturate(current_visibility + ao_sum / ao_weight_sum);
      }
      if (specular_weight_sum > 0.0 && all(isfinite(specular_sum))) {
        // Average angular areas, not their ratios. Keep this pixel's roughness
        // and confidence so filtering cannot spread them onto other surfaces.
        const float2 masses = max(specular_center.xy + specular_sum / specular_weight_sum, 0.0);
        specular_result.xy = float2(min(masses.x, masses.y), masses.y);
      }
    }
  }

  // 4. Store results normalized by each signal's own accepted weights. Write every
  // dispatched pixel so no stale data survives in a reused filter destination.
  lighting_output[pixel] = PackIndirectDiffuse(gi_result, gi_valid, ao_result, ao_valid);
  if (specular_active) {
    specular_output[pixel] = specular_result;
  }
  if (CUSTOM_MGSV_GT_DEBUG_VIEW == 10.0) diagnostic_output[pixel] = float4(ao_result.xxx, 1.0);
}
