// GI and AO temporal accumulation adapted from Toru77/RenoDX falcomengine-plus:
// https://github.com/Toru77/renodx/blob/3360c8dff84ae8acf3558ddb80b00f3cbd1ac90e/src/games/falcomengine-plus/world/shaders/world_rtgi_temporal.cs_5_0.hlsl
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

Texture2D<float4> rawGI : register(t0);
Texture2D<float4> depthBuffer : register(t1);
Texture2D<float4> normalBuffer : register(t2);
Texture2D<float4> velocityBuffer : register(t3);
Texture2D<float4> objectVelocityBuffer : register(t4);
Texture2D<float4> previousGI : register(t5);
Texture2D<uint2> previousGeometry : register(t6);
Texture2D<float2> rawAO : register(t7);
Texture2D<float2> previousAO : register(t8);
RWTexture2D<float4> giOutput : register(u0);
RWTexture2D<uint2> geometryOutput : register(u1);
RWTexture2D<float4> diagnosticOutput : register(u2);
RWTexture2D<float2> aoOutput : register(u3);

// GPU copies of the native 480-byte scene block. These are scene data, not
// another settings buffer. m_view has different meanings in native passes;
// derive the camera transform from projectionView and projection instead.
struct NativeScene {
  float4x4 projectionView;
  float4x4 projection;
  float4x4 view;
  float4x4 shadowProjection;
  float4x4 shadowProjection2;
  float4 eyePosition;
  float4 projectionParameters;
  float4 viewportSize;
  float4 exposure;
  float4 fogParameters[3];
  float4 fogColor;
  float4 cameraCenterOffset;
  float4 shadowMapResolutions;
};

cbuffer CurrentSceneConstants : register(b1) {
  NativeScene currentScene : packoffset(c0);
}

cbuffer PreviousSceneConstants : register(b2) {
  NativeScene previousScene : packoffset(c0);
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  uint width, height;
  rawGI.GetDimensions(width, height);
  const int2 pixel = int2(tid.xy);
  if (any(pixel >= int2(width, height))) return;

  const float scale = max(CUSTOM_MGSV_GT_RESOLUTION_DIVISOR, 1.0);
  const float2 fullSize = float2(cBufferWidth, cBufferHeight);
  const int2 fullPixel = min(int2(floor((float2(pixel) + 0.5) * scale)), int2(fullSize) - 1);
  const float4 currentGI = rawGI.Load(int3(pixel, 0));
  const float2 currentAO = rawAO.Load(int3(pixel, 0));
  const float depth = depthBuffer.Load(int3(fullPixel, 0)).x;
  const float z = LinearZ(depth);
  const float3 normal = DecodeNormal(normalBuffer.Load(int3(fullPixel, 0)).xyz);
  const bool geometryValid = depth > 0.0 && z > 0.0 && z < 1.0e6 && all(isfinite(normal));
  const bool giValid = geometryValid && currentGI.a > 0.5 && all(isfinite(currentGI.rgb));
  const bool aoValid = geometryValid && currentAO.g > 0.5 && isfinite(currentAO.r);
  const float3 currentRadiance = giValid ? min(currentGI.rgb, 65504.0) : 0.0;
  const float currentVisibility = aoValid ? saturate(currentAO.r) : 1.0;
  const int diagnostic = (int)CUSTOM_MGSV_GI_DEBUG_VIEW;
  giOutput[pixel] = float4(currentRadiance, giValid ? 1.0 : 0.0);
  aoOutput[pixel] = float2(currentVisibility, aoValid ? 1.0 : 0.0);
  geometryOutput[pixel] = uint2(0u, 0u);
  if (diagnostic == 6) diagnosticOutput[pixel] = float4(currentRadiance, 1.0);
  if (diagnostic == 9 || diagnostic == 10) diagnosticOutput[pixel] = float4(currentVisibility.xxx, 1.0);
  if (diagnostic == 8 || diagnostic == 12) diagnosticOutput[pixel] = float4(0.0, 0.0, 0.0, 1.0);
  if (!geometryValid) return;

  // Geometry belongs to the receiver, independently of either signal's
  // validity. GI alpha and AO green determine whether their values can be used.
  float3 oct = normal / (abs(normal.x) + abs(normal.y) + abs(normal.z));
  if (oct.z < 0.0) {
    oct.xy = (1.0 - abs(oct.yx)) * float2(oct.x >= 0.0 ? 1.0 : -1.0, oct.y >= 0.0 ? 1.0 : -1.0);
  }
  const uint2 packedNormal = uint2(round(saturate(oct.xy * 0.5 + 0.5) * 65535.0));
  geometryOutput[pixel] = uint2(asuint(z), packedNormal.x | (packedNormal.y << 16u));

  bool accumulateGI = giValid && CUSTOM_MGSV_GT_TEMPORAL_ENABLED >= 0.5
                      && CUSTOM_MGSV_GI_TEMPORAL_ACTIVE >= 0.5 && CUSTOM_MGSV_GI_HISTORY_VALID >= 0.5
                      && CUSTOM_MGSV_GI_HISTORY_WEIGHT > 0.0;
  const bool accumulateAO = aoValid && CUSTOM_MGSV_GT_TEMPORAL_ENABLED >= 0.5
                            && CUSTOM_MGSV_AO_TEMPORAL_ACTIVE >= 0.5 && CUSTOM_MGSV_AO_HISTORY_VALID >= 0.5
                            && CUSTOM_MGSV_AO_HISTORY_WEIGHT > 0.0;
  // Cold or disabled signals keep their current values. No previous texture
  // is read unless at least one signal has usable history.
  [branch]
  if (!accumulateGI && !accumulateAO) return;

  float exposureRatio = 1.0;
  if (accumulateGI) {
    const float currentExposure = currentScene.exposure.z;
    const float previousExposure = previousScene.exposure.z;
    if (currentExposure > 0.0 && previousExposure > 0.0
        && isfinite(currentExposure) && isfinite(previousExposure)) {
      exposureRatio = currentExposure / previousExposure;
      accumulateGI = isfinite(exposureRatio);
    } else {
      accumulateGI = false;
    }
  }
  if (!accumulateGI && !accumulateAO) return;

  // These snapshots must describe the same ordinary perspective view as the
  // scatter's geometry constants. A special-view or stale scene block may be
  // finite while still being incompatible; keep the current sample then.
  if (!all(isfinite(float4(currentScene.projection._m23, currentScene.projection._m33,
                            previousScene.projection._m23, previousScene.projection._m33)))
      || abs(currentScene.projection._m23 - 1.0) > 1.0e-5 || abs(currentScene.projection._m33) > 1.0e-5
      || abs(previousScene.projection._m23 - 1.0) > 1.0e-5 || abs(previousScene.projection._m33) > 1.0e-5) return;
  const float4 currentProjection = float4(1.0 / currentScene.projection._m00,
                                          1.0 / currentScene.projection._m11,
                                          currentScene.projection._m32, currentScene.projection._m22);
  const float4 previousProjection = float4(1.0 / previousScene.projection._m00,
                                           1.0 / previousScene.projection._m11,
                                           previousScene.projection._m32, previousScene.projection._m22);
  if (!all(isfinite(currentProjection)) || !all(isfinite(previousProjection))
      || !all(isfinite(currentScene.projectionParameters)) || !all(isfinite(previousScene.projectionParameters))
      || any(abs(currentProjection - cProjectionParam) > 1.0e-3 * max(abs(cProjectionParam), 1.0e-5))
      || any(abs(currentScene.projectionParameters - cProjectionParam) > 1.0e-3 * max(abs(cProjectionParam), 1.0e-5))
      || any(abs(previousProjection - previousScene.projectionParameters)
             > 1.0e-3 * max(abs(previousScene.projectionParameters), 1.0e-5))
      || !all(isfinite(currentScene.cameraCenterOffset.xy)) || !all(isfinite(previousScene.cameraCenterOffset.xy))
      || any(abs(currentScene.cameraCenterOffset.xy - cCameraCenterOffset) > 1.0e-5)) return;

  // Native matrices use row-vector multiplication: projectionView = view *
  // projection. Recover view columns while removing the projection shear.
  const float4 currentZ = currentScene.projectionView._m03_m13_m23_m33;
  const float4 currentX = (currentScene.projectionView._m00_m10_m20_m30
                           - currentScene.projection._m20 * currentZ) / currentScene.projection._m00;
  const float4 currentY = (currentScene.projectionView._m01_m11_m21_m31
                           - currentScene.projection._m21 * currentZ) / currentScene.projection._m11;
  const float4 previousZ = previousScene.projectionView._m03_m13_m23_m33;
  const float4 previousX = (previousScene.projectionView._m00_m10_m20_m30
                            - previousScene.projection._m20 * previousZ) / previousScene.projection._m00;
  const float4 previousY = (previousScene.projectionView._m01_m11_m21_m31
                            - previousScene.projection._m21 * previousZ) / previousScene.projection._m11;
  if (!all(isfinite(currentX)) || !all(isfinite(currentY)) || !all(isfinite(currentZ))
      || !all(isfinite(previousX)) || !all(isfinite(previousY)) || !all(isfinite(previousZ))) return;
  const float3 currentLengths = float3(dot(currentX.xyz, currentX.xyz), dot(currentY.xyz, currentY.xyz), dot(currentZ.xyz, currentZ.xyz));
  const float3 previousLengths = float3(dot(previousX.xyz, previousX.xyz), dot(previousY.xyz, previousY.xyz), dot(previousZ.xyz, previousZ.xyz));
  const float3 currentAngles = float3(dot(currentX.xyz, currentY.xyz), dot(currentX.xyz, currentZ.xyz), dot(currentY.xyz, currentZ.xyz));
  const float3 previousAngles = float3(dot(previousX.xyz, previousY.xyz), dot(previousX.xyz, previousZ.xyz), dot(previousY.xyz, previousZ.xyz));
  if (any(abs(currentLengths - 1.0) > 1.0e-3) || any(abs(previousLengths - 1.0) > 1.0e-3)
      || any(abs(currentAngles) > 1.0e-3) || any(abs(previousAngles) > 1.0e-3)) return;

  const float2 currentJitter = float2(CUSTOM_MGSV_GT_CURRENT_JITTER_X, CUSTOM_MGSV_GT_CURRENT_JITTER_Y);
  const float2 previousJitter = float2(CUSTOM_MGSV_GT_PREVIOUS_JITTER_X, CUSTOM_MGSV_GT_PREVIOUS_JITTER_Y);
  const float3 viewPosition = ViewPos(float2(fullPixel) - currentJitter * fullSize, z);
  const float3 worldPosition = currentX.xyz * (viewPosition.x - currentX.w)
                               + currentY.xyz * (viewPosition.y - currentY.w)
                               + currentZ.xyz * (viewPosition.z - currentZ.w);
  const float3 previousPosition = float3(dot(float4(worldPosition, 1.0), previousX),
                                         dot(float4(worldPosition, 1.0), previousY),
                                         dot(float4(worldPosition, 1.0), previousZ));
  if (!(previousPosition.z > 0.0) || !all(isfinite(previousPosition))) return;
  const float3 worldNormal = currentX.xyz * normal.x + currentY.xyz * normal.y + currentZ.xyz * normal.z;
  const float3 previousNormal = normalize(float3(dot(worldNormal, previousX.xyz),
                                                  dot(worldNormal, previousY.xyz),
                                                  dot(worldNormal, previousZ.xyz)));
  if (!all(isfinite(previousNormal))) return;

  const float2 previousNdc = previousPosition.xy / previousPosition.z / previousScene.projectionParameters.xy
                             + previousScene.cameraCenterOffset.xy;
  float2 previousPixel = (previousNdc * float2(0.5, -0.5) + 0.5 + previousJitter) * fullSize;
  const float objectMask = objectVelocityBuffer.Load(int3(fullPixel, 0)).r;
  if (objectMask > 0.5) {
    const float2 encodedVelocity = velocityBuffer.Load(int3(fullPixel, 0)).ba;
    const float2 velocity = (encodedVelocity * 2.0 - 1.0) * 64.0 / fullSize
                            - currentJitter * CUSTOM_MGSV_GT_VELOCITY_JITTER_SCALE;
    previousPixel = float2(fullPixel) - velocity * fullSize + (previousJitter - currentJitter) * fullSize;
  }
  // Object motion supplies screen displacement, not a previous 3D position or
  // normal. The geometric test remains conservative for motion in depth and
  // deformation; incompatible history falls back to the current GI sample.
  if (!all(isfinite(previousPixel)) || any(previousPixel < 0.0) || any(previousPixel > fullSize - 1.0)) return;

  // Invert the trace's representative-pixel map rather than multiplying UV by
  // ceil(width / scale). The last interval can be shorter at odd dimensions.
  const float centerOffset = floor(0.5 * scale);
  float2 historyPosition = (previousPixel - centerOffset) / scale;
  const int2 last = int2(width, height) - 1;
  const float2 penultimatePixel = float2(last - 1) * scale + centerOffset;
  const float2 lastPixel = min(float2(last) * scale + centerOffset, fullSize - 1.0);
  [unroll]
  for (int axis = 0; axis < 2; ++axis) {
    if (last[axis] == 0) {
      historyPosition[axis] = 0.0;
    } else if (previousPixel[axis] >= penultimatePixel[axis]) {
      historyPosition[axis] = float(last[axis] - 1)
                              + (previousPixel[axis] - penultimatePixel[axis])
                                    / max(lastPixel[axis] - penultimatePixel[axis], 1.0);
    }
  }
  const int2 base = int2(floor(historyPosition));
  const float2 fraction = frac(historyPosition);
  float3 giHistorySum = 0.0;
  float aoHistorySum = 0.0;
  float giAcceptedWeight = 0.0;
  float aoAcceptedWeight = 0.0;
  [unroll]
  for (int i = 0; i < 4; ++i) {
    const int2 offset = int2(i & 1, i >> 1);
    const int2 q = base + offset;
    if (any(q < 0) || any(q > last)) continue;
    const float bilinear = (offset.x != 0 ? fraction.x : 1.0 - fraction.x)
                           * (offset.y != 0 ? fraction.y : 1.0 - fraction.y);
    if (bilinear <= 0.0) continue;
    const uint2 geometry = previousGeometry.Load(int3(q, 0));
    const float historyZ = asfloat(geometry.x);
    if (!(historyZ > 0.0) || !isfinite(historyZ)) continue;

    const float2 octXY = float2(geometry.y & 65535u, geometry.y >> 16u) * (2.0 / 65535.0) - 1.0;
    float3 historyNormal = float3(octXY, 1.0 - abs(octXY.x) - abs(octXY.y));
    if (historyNormal.z < 0.0) {
      historyNormal.xy = (1.0 - abs(historyNormal.yx))
                          * float2(historyNormal.x >= 0.0 ? 1.0 : -1.0, historyNormal.y >= 0.0 ? 1.0 : -1.0);
    }
    historyNormal = normalize(historyNormal);
    const float normalDot = dot(historyNormal, previousNormal);
    const float2 normalWeights = saturate((normalDot
                                           - (float2(CUSTOM_MGSV_GI_NORMAL_REJECTION, CUSTOM_MGSV_AO_NORMAL_REJECTION) - 0.4)) / 0.4);
    const float depthDifference = abs(historyZ - previousPosition.z);
    const float referenceDepth = max(previousPosition.z, 1.0e-4);
    if (accumulateGI && normalWeights.x > 0.0
        && depthDifference <= CUSTOM_MGSV_GI_DEPTH_REJECTION * referenceDepth) {
      const float4 history = previousGI.Load(int3(q, 0));
      if (history.a > 0.5 && all(isfinite(history.rgb))) {
        const float3 exposedHistory = history.rgb * exposureRatio;
        if (all(isfinite(exposedHistory))) {
          const float weight = bilinear * normalWeights.x;
          giHistorySum += exposedHistory * weight;
          giAcceptedWeight += weight;
        }
      }
    }
    if (accumulateAO && normalWeights.y > 0.0
        && depthDifference <= CUSTOM_MGSV_AO_DEPTH_REJECTION * referenceDepth) {
      const float2 history = previousAO.Load(int3(q, 0));
      if (history.g > 0.5 && isfinite(history.r)) {
        const float weight = bilinear * normalWeights.y;
        aoHistorySum += history.r * weight;
        aoAcceptedWeight += weight;
      }
    }
  }

  const bool giHistoryValid = giAcceptedWeight > 1.0e-4 && all(isfinite(giHistorySum));
  const bool aoHistoryValid = aoAcceptedWeight > 1.0e-4 && isfinite(aoHistorySum);
  if (!giHistoryValid && !aoHistoryValid) return;
  float3 giHistory = currentRadiance;
  float aoHistory = currentVisibility;
  if (giHistoryValid) giHistory = giHistorySum / giAcceptedWeight;
  if (aoHistoryValid) aoHistory = aoHistorySum / aoAcceptedWeight;

  const bool clampGI = giHistoryValid && CUSTOM_MGSV_GI_HISTORY_CLAMP > 0.0;
  const bool clampAO = aoHistoryValid && CUSTOM_MGSV_AO_HISTORY_CLAMP > 0.0;
  if (clampGI || clampAO) {
    float3 giMean = 0.0;
    float3 giSquaredDifference = 0.0;
    float giCount = 0.0;
    float aoMean = 0.0;
    float aoSquaredDifference = 0.0;
    float aoCount = 0.0;
    [unroll]
    for (int y = -1; y <= 1; ++y) {
      [unroll]
      for (int x = -1; x <= 1; ++x) {
        const int2 q = pixel + int2(x, y);
        if (any(q < 0) || any(q > last)) continue;
        // Welford updates keep FP32 HDR statistics stable. Validity and
        // sample counts remain separate; invalid GI cannot poison AO.
        if (clampGI) {
          const float4 sampleValue = rawGI.Load(int3(q, 0));
          if (sampleValue.a > 0.5 && all(isfinite(sampleValue.rgb))) {
            giCount += 1.0;
            const float3 difference = sampleValue.rgb - giMean;
            giMean += difference / giCount;
            giSquaredDifference += difference * (sampleValue.rgb - giMean);
          }
        }
        if (clampAO) {
          const float2 sampleValue = rawAO.Load(int3(q, 0));
          if (sampleValue.g > 0.5 && isfinite(sampleValue.r)) {
            aoCount += 1.0;
            const float difference = sampleValue.r - aoMean;
            aoMean += difference / aoCount;
            aoSquaredDifference += difference * (sampleValue.r - aoMean);
          }
        }
      }
    }
    if (clampGI) {
      const float3 sigma = sqrt(max(giSquaredDifference / max(giCount, 1.0), 0.0));
      giHistory = clamp(giHistory, giMean - CUSTOM_MGSV_GI_HISTORY_CLAMP * sigma,
                        giMean + CUSTOM_MGSV_GI_HISTORY_CLAMP * sigma);
    }
    if (clampAO) {
      const float sigma = sqrt(max(aoSquaredDifference / max(aoCount, 1.0), 0.0));
      aoHistory = clamp(aoHistory, aoMean - CUSTOM_MGSV_AO_HISTORY_CLAMP * sigma,
                        aoMean + CUSTOM_MGSV_AO_HISTORY_CLAMP * sigma);
    }
  }

  if (giHistoryValid) {
    const float blend = saturate(CUSTOM_MGSV_GI_HISTORY_WEIGHT) * saturate(giAcceptedWeight);
    const float3 result = lerp(currentRadiance, giHistory, blend);
    if (all(isfinite(result))) {
      giOutput[pixel] = float4(min(result, 65504.0), 1.0);
      if (diagnostic == 6) diagnosticOutput[pixel] = float4(result, 1.0);
      if (diagnostic == 8) diagnosticOutput[pixel] = float4(saturate(giAcceptedWeight).xxx, 1.0);
    }
  }
  if (aoHistoryValid) {
    const float blend = saturate(CUSTOM_MGSV_AO_HISTORY_WEIGHT) * saturate(aoAcceptedWeight);
    const float result = saturate(lerp(currentVisibility, aoHistory, blend));
    if (isfinite(result)) {
      aoOutput[pixel] = float2(result, 1.0);
      if (diagnostic == 10) diagnosticOutput[pixel] = float4(result.xxx, 1.0);
      if (diagnostic == 12) diagnosticOutput[pixel] = float4(saturate(aoAcceptedWeight).xxx, 1.0);
    }
  }
}
