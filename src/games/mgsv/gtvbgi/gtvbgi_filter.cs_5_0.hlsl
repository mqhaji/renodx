// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

#include "../gbuffer.hlsli"
#include "../shared.h"
#include "directional_visibility.hlsli"

Texture2D<float4> giInput : register(t0);
Texture2D<float4> depthBuffer : register(t1);
Texture2D<float4> normalBuffer : register(t2);
Texture2D<float2> aoInput : register(t3);
Texture2D<float4> bent_normal_input : register(t4);
RWTexture2D<float4> giOutput : register(u0);
RWTexture2D<float2> aoOutput : register(u1);
RWTexture2D<float4> diagnosticOutput : register(u2);
RWTexture2D<float4> bent_normal_output : register(u3);

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
  giInput.GetDimensions(width, height);
  const int2 pixel = int2(tid.xy);
  if (any(pixel >= int2(width, height))) return;

  const float4 giCenter = giInput.Load(int3(pixel, 0));
  const float2 aoCenter = aoInput.Load(int3(pixel, 0));
  const bool giValid = giCenter.a > 0.5 && all(isfinite(giCenter.rgb));
  const bool aoValid = aoCenter.g > 0.5 && isfinite(aoCenter.r);
  const float currentVisibility = aoValid ? saturate(aoCenter.r) : 1.0;
  float4 giResult = giValid ? giCenter : 0.0;
  float2 aoResult = float2(currentVisibility, aoValid ? aoCenter.g : 0.0);
  const bool directional_active = CUSTOM_MGSV_DIRECTIONAL_ACTIVE >= 0.5;
  float4 bent_center = float4(0.0, 0.0, 0.0, -1.0);
  if (directional_active) {
    bent_center = bent_normal_input.Load(int3(pixel, 0));
  }
  const bool bent_valid = directional_active && ValidDirectionalVisibility(bent_center);
  float4 bent_result = (bent_valid ? bent_center : float4(0.0, 0.0, 0.0, -1.0));

  [branch]
  if (CUSTOM_MGSV_GT_SPATIAL_ENABLED >= 0.5 && (giValid || aoValid || bent_valid)) {
    const float scale = max(CUSTOM_MGSV_GT_RESOLUTION_DIVISOR, 1.0);
    const int2 fullSize = int2(cBufferWidth, cBufferHeight);
    const int2 centerPixel = min(int2(floor((float2(pixel) + 0.5) * scale)), fullSize - 1);
    const float centerDepth = depthBuffer.Load(int3(centerPixel, 0)).x;
    const float centerZ = LinearZ(centerDepth);
    const float3 centerNormal = DecodeNormal(normalBuffer.Load(int3(centerPixel, 0)).xyz);
    if (centerDepth > 0.0 && centerZ > 0.0 && centerZ < 1.0e6 && all(isfinite(centerNormal))) {
      const float3 centerPosition = ViewPos(float2(centerPixel), centerZ);
      const float planeScale = max(0.02 * length(centerPosition), 1.0e-6);
      const bool atrous = CUSTOM_MGSV_GT_FILTER_TYPE >= 0.5;
      // The table covers every supported odd A-Trous width. Separable taps
      // use their Gaussian weights and do not access the table.
      const uint taps = (uint)clamp(atrous ? CUSTOM_MGSV_GT_FILTER_TAPS_ATROUS : CUSTOM_MGSV_GT_FILTER_TAPS_SEPARABLE,
                                    3.0, atrous ? 9.0 : 17.0)
                        | 1u;
      const int halfTaps = int(taps / 2u);
      const int step = max((int)CUSTOM_MGSV_GT_FILTER_STEP, 1);
      const int direction = (int)CUSTOM_MGSV_GT_FILTER_DIRECTION;
      const float sigma = max(CUSTOM_MGSV_GT_FILTER_RADIUS * 0.5, 0.5);

      // Center-relative FP32 sums preserve constant input exactly through
      // FP16 stores. Each signal keeps its own accepted weights and validity.
      precise float3 giSum = 0.0;
      precise float aoSum = 0.0;
      float giWeightSum = 0.0;
      float aoWeightSum = 0.0;
      precise float4 bent_sum = 0.0;
      float bent_weight_sum = 0.0;
      const int firstRow = atrous ? -halfTaps : 0;
      const int lastRow = atrous ? halfTaps : 0;
      [loop]
      for (int y = firstRow; y <= lastRow; ++y) {
        [loop]
        for (int x = -halfTaps; x <= halfTaps; ++x) {
          const int2 offset = atrous ? int2(x, y) * step
                                     : (direction == 0 ? int2(x, 0) : int2(0, x));
          const int2 q = pixel + offset;
          if (any(q < 0) || any(q >= int2(width, height))) continue;
          const float4 giSample = giInput.Load(int3(q, 0));
          const float2 aoSample = aoInput.Load(int3(q, 0));
          const bool giSampleValid = giValid && giSample.a > 0.5 && all(isfinite(giSample.rgb));
          const bool aoSampleValid = aoValid && aoSample.g > 0.5 && isfinite(aoSample.r);
          float4 bent_sample = float4(0.0, 0.0, 0.0, -1.0);
          if (bent_valid) {
            bent_sample = bent_normal_input.Load(int3(q, 0));
          }
          const bool bent_sample_valid = bent_valid && ValidDirectionalVisibility(bent_sample);
          if (!giSampleValid && !aoSampleValid && !bent_sample_valid) continue;

          float kernelWeight;
          [branch]
          if (atrous) {
            kernelWeight = BINOMIAL_WEIGHTS[abs(x)][halfTaps - 1] * BINOMIAL_WEIGHTS[abs(y)][halfTaps - 1];
          }
          else {
            kernelWeight = exp(-0.5 * float(x * x) / (sigma * sigma));
          }
          float edgeWeight = 1.0;
          if (any(offset != 0)) {
            const int2 samplePixel = min(int2(floor((float2(q) + 0.5) * scale)), fullSize - 1);
            const float sampleDepth = depthBuffer.Load(int3(samplePixel, 0)).x;
            const float sampleZ = LinearZ(sampleDepth);
            const float3 sampleNormal = DecodeNormal(normalBuffer.Load(int3(samplePixel, 0)).xyz);
            if (!(sampleDepth > 0.0 && sampleZ > 0.0 && sampleZ < 1.0e6) || !all(isfinite(sampleNormal))) continue;
            const float3 samplePosition = ViewPos(float2(samplePixel), sampleZ);
            const float plane = dot(centerNormal, samplePosition - centerPosition) / planeScale;
            float normalWeight = saturate(dot(centerNormal, sampleNormal));
            normalWeight *= normalWeight;
            normalWeight *= normalWeight;
            normalWeight *= normalWeight;
            normalWeight *= normalWeight;
            edgeWeight = exp(-0.5 * plane * plane) * normalWeight;
          }
          const float weight = kernelWeight * edgeWeight;
          if (giSampleValid) {
            giSum += (giSample.rgb - giCenter.rgb) * weight;
            giWeightSum += weight;
          }
          if (aoSampleValid) {
            aoSum += (aoSample.r - currentVisibility) * weight;
            aoWeightSum += weight;
          }
          if (bent_sample_valid) {
            bent_sum += (bent_sample - bent_center) * weight;
            bent_weight_sum += weight;
          }
        }
      }
      if (giWeightSum > 0.0 && all(isfinite(giSum))) {
        giResult = float4(min(giCenter.rgb + giSum / giWeightSum, 65504.0), giCenter.a);
      }
      if (aoWeightSum > 0.0 && isfinite(aoSum)) {
        aoResult = float2(saturate(currentVisibility + aoSum / aoWeightSum), aoCenter.g);
      }
      if (bent_weight_sum > 0.0 && all(isfinite(bent_sum))) {
        const float3 receiver_normal = BentReceiverNormal(centerNormal, centerPosition);
        bent_result = BoundDirectionalVisibility(bent_center + bent_sum / bent_weight_sum, receiver_normal);
      }
    }
  }

  giOutput[pixel] = giResult;
  aoOutput[pixel] = aoResult;
  if (directional_active) {
    bent_normal_output[pixel] = bent_result;
  }
  if (CUSTOM_MGSV_GI_DEBUG_VIEW == 11.0) diagnosticOutput[pixel] = float4(aoResult.rrr, 1.0);
}
