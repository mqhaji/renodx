// GI and AO spatial filtering adapted from Toru77/RenoDX falcomengine-plus:
// https://github.com/Toru77/renodx/blob/3360c8dff84ae8acf3558ddb80b00f3cbd1ac90e/src/games/falcomengine-plus/world/shaders/world_rtgi_filter.cs_5_0.hlsl
// Copyright (c) 2025 Carlos Lopez Jr.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "../gbuffer.hlsli"
#include "../shared.h"

Texture2D<float4> giInput : register(t0);
Texture2D<float4> depthBuffer : register(t1);
Texture2D<float4> normalBuffer : register(t2);
Texture2D<float2> aoInput : register(t3);
RWTexture2D<float4> giOutput : register(u0);
RWTexture2D<float2> aoOutput : register(u1);
RWTexture2D<float4> diagnosticOutput : register(u2);

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

  [branch]
  if (CUSTOM_MGSV_GT_SPATIAL_ENABLED >= 0.5 && (giValid || aoValid)) {
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
                                    3.0, atrous ? 9.0 : 17.0) | 1u;
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
          if (!giSampleValid && !aoSampleValid) continue;

          float kernelWeight;
          [branch]
          if (atrous) {
            kernelWeight = BINOMIAL_WEIGHTS[abs(x)][halfTaps - 1] * BINOMIAL_WEIGHTS[abs(y)][halfTaps - 1];
          } else {
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
        }
      }
      if (giWeightSum > 0.0 && all(isfinite(giSum))) {
        giResult = float4(min(giCenter.rgb + giSum / giWeightSum, 65504.0), giCenter.a);
      }
      if (aoWeightSum > 0.0 && isfinite(aoSum)) {
        aoResult = float2(saturate(currentVisibility + aoSum / aoWeightSum), aoCenter.g);
      }
    }
  }

  giOutput[pixel] = giResult;
  aoOutput[pixel] = aoResult;
  if (CUSTOM_MGSV_GI_DEBUG_VIEW == 11.0) diagnosticOutput[pixel] = float4(aoResult.rrr, 1.0);
}
