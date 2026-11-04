// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

#include "../gbuffer.hlsli"
#include "../shared.h"
#include "specular_occlusion.hlsli"

// Smooth nearby lighting without crossing depth, normal, or reflection edges.
// Specular visible/open areas are averaged together, then divided during resolve.

Texture2D<float4> gi_input : register(t0);
Texture2D<float4> depth_buffer : register(t1);
Texture2D<float4> normal_buffer : register(t2);
Texture2D<float2> ao_input : register(t3);
Texture2D<float4> specular_input : register(t4);
Texture2DArray<float2> filter_noise : register(t5);
RWTexture2D<float4> gi_output : register(u0);
RWTexture2D<float2> ao_output : register(u1);
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
void main(uint3 tid : SV_DispatchThreadID) {
  uint width, height;
  gi_input.GetDimensions(width, height);
  const int2 pixel = int2(tid.xy);
  if (any(pixel >= int2(width, height))) return;

  const float4 gi_center = gi_input.Load(int3(pixel, 0));
  const float2 ao_center = ao_input.Load(int3(pixel, 0));
  const bool gi_valid = gi_center.a > 0.5 && all(isfinite(gi_center.rgb));
  const bool ao_valid = ao_center.g > 0.5 && isfinite(ao_center.r);
  const float current_visibility = ao_valid ? saturate(ao_center.r) : 1.0;
  float4 gi_result = gi_valid ? gi_center : 0.0;
  float2 ao_result = float2(current_visibility, ao_valid ? ao_center.g : 0.0);
  const bool specular_active = CUSTOM_MGSV_SPECULAR_ACTIVE >= 0.5;
  float4 specular_center = float4(0.0, 0.0, 0.0, -1.0);
  if (specular_active) {
    specular_center = specular_input.Load(int3(pixel, 0));
  }
  const bool specular_valid = specular_active && ValidSpecularMasses(specular_center);
  float4 specular_result = specular_valid ? specular_center : float4(0.0, 0.0, 0.0, -1.0);

  [branch]
  if (CUSTOM_MGSV_GT_SPATIAL_ENABLED >= 0.5 && (gi_valid || ao_valid || specular_valid)) {
    const float scale = max(CUSTOM_MGSV_GT_RESOLUTION_DIVISOR, 1.0);
    const int2 full_size = int2(cBufferWidth, cBufferHeight);
    const int2 center_pixel = min(int2(floor((float2(pixel) + 0.5) * scale)), full_size - 1);
    const float center_depth = depth_buffer.Load(int3(center_pixel, 0)).x;
    const float center_z = LinearZ(center_depth);
    const float3 center_normal = DecodeNormal(normal_buffer.Load(int3(center_pixel, 0)).xyz);
    if (center_depth > 0.0 && center_z > 0.0 && center_z < 1.0e6 && all(isfinite(center_normal))) {
      const float3 center_position = ViewPos(float2(center_pixel), center_z);
      float3 center_reflection = 0.0;
      float3 center_lobe = 0.0;
      if (specular_valid) {
        center_reflection = reflect(normalize(ViewPos(float2(center_pixel) + 0.5, center_z)), center_normal);
        center_lobe = SpecularLobeParameters(specular_center.z);
      }
      const float plane_scale = max(0.02 * length(center_position), 1.0e-6);
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
          const float4 gi_sample = gi_input.Load(int3(q, 0));
          const float2 ao_sample = ao_input.Load(int3(q, 0));
          const bool gi_sample_valid = gi_valid && gi_sample.a > 0.5 && all(isfinite(gi_sample.rgb));
          const bool ao_sample_valid = ao_valid && ao_sample.g > 0.5 && isfinite(ao_sample.r);
          float4 specular_sample = float4(0.0, 0.0, 0.0, -1.0);
          if (specular_valid) {
            specular_sample = specular_input.Load(int3(q, 0));
          }
          const bool specular_sample_valid = specular_valid && ValidSpecularMasses(specular_sample);
          if (!gi_sample_valid && !ao_sample_valid && !specular_sample_valid) continue;

          const float kernel_weight = BINOMIAL_WEIGHTS[abs(x)][half_width - 1] * BINOMIAL_WEIGHTS[abs(y)][half_width - 1];
          float lobe_weight = 1.0;
          const int2 sample_pixel = min(int2(floor((float2(q) + 0.5) * scale)), full_size - 1);
          const float sample_depth = depth_buffer.Load(int3(sample_pixel, 0)).x;
          const float sample_z = LinearZ(sample_depth);
          const float3 sample_normal = DecodeNormal(normal_buffer.Load(int3(sample_pixel, 0)).xyz);
          if (!(sample_depth > 0.0 && sample_z > 0.0 && sample_z < 1.0e6) || !all(isfinite(sample_normal))) continue;
          const float3 sample_position = ViewPos(float2(sample_pixel), sample_z);
          const float plane = dot(center_normal, sample_position - center_position) / plane_scale;
          float normal_weight = saturate(dot(center_normal, sample_normal));
          normal_weight *= normal_weight;
          normal_weight *= normal_weight;
          normal_weight *= normal_weight;
          normal_weight *= normal_weight;
          const float edge_weight = exp(-0.5 * plane * plane) * normal_weight;
          if (specular_sample_valid) {
            const float3 reflection = reflect(normalize(ViewPos(float2(sample_pixel) + 0.5, sample_z)), sample_normal);
            lobe_weight = SpecularLobeWeight(center_reflection, center_lobe, reflection, specular_sample.z);
          }
          const float weight = kernel_weight * edge_weight * 4.0;
          if (gi_sample_valid) {
            gi_sum += (gi_sample.rgb - gi_center.rgb) * weight;
            gi_weight_sum += weight;
          }
          if (ao_sample_valid) {
            ao_sum += (ao_sample.r - current_visibility) * weight;
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
        gi_result = float4(min(gi_center.rgb + gi_sum / gi_weight_sum, 65504.0), gi_center.a);
      }
      if (ao_weight_sum > 0.0 && isfinite(ao_sum)) {
        ao_result = float2(saturate(current_visibility + ao_sum / ao_weight_sum), ao_center.g);
      }
      if (specular_weight_sum > 0.0 && all(isfinite(specular_sum))) {
        // Average angular areas, not their ratios. Keep this pixel's roughness
        // and confidence so filtering cannot spread them onto other surfaces.
        const float2 masses = max(specular_center.xy + specular_sum / specular_weight_sum, 0.0);
        specular_result.xy = float2(min(masses.x, masses.y), masses.y);
      }
    }
  }

  gi_output[pixel] = gi_result;
  ao_output[pixel] = ao_result;
  if (specular_active) {
    specular_output[pixel] = specular_result;
  }
  if (CUSTOM_MGSV_GT_DEBUG_VIEW == 10.0) diagnostic_output[pixel] = float4(ao_result.rrr, 1.0);
}
