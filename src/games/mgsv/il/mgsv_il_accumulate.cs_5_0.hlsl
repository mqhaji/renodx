// MGSV screen-space diffuse gather - the temporal half.
//
// Accumulates the gather across frames so the filter only has to clean the residual instead of
// carrying all the noise. Reprojection uses the velocity buffer the game already writes for its
// own temporal reconstruction, decoded exactly as the TAA pass decodes it: MGSV stores a
// current-minus-previous UV delta in .ba as 0.5 + 0.5 * (ndc * size / 128), so inverting the
// scale and the NDC mapping gives (encoded * 2 - 1) * 64 / screen_size.
//
// History is clamped to the current frame's 3x3 neighbourhood before blending. That is the same
// rejection a TAA uses, and it is what stops a mis-reprojected history - a disocclusion, a fast
// edge, a camera cut - from smearing across the image.

#include "../shared.h"
#include "../gbuffer.hlsli"

Texture2D<float4> currentBuffer : register(t0);   // this frame's gather
Texture2D<float4> historyBuffer : register(t1);   // previous accumulated result
Texture2D<float4> velocityBuffer : register(t2);  // MGSV camera velocity, .ba
Texture2D<float4> depthBuffer : register(t3);
Texture2D<float4> normalBuffer : register(t4);
RWTexture2D<float4> accumulatedOutput : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  const uint2 pixel = tid.xy;
  const float scale = max(CUSTOM_MGSV_IL_SCALE, 1.0);
  const int2 gather_size = int2(float2(cBufferWidth, cBufferHeight) / scale);
  if (pixel.x >= (uint)gather_size.x || pixel.y >= (uint)gather_size.y) return;

  const int2 centre = (int2)floor((float2(pixel) + 0.5) * scale);
  if (centre.x >= (int)cBufferWidth || centre.y >= (int)cBufferHeight) return;

  const float4 current = currentBuffer.Load(int3(pixel, 0));
  const float blend = CUSTOM_MGSV_IL_HISTORY * saturate(CUSTOM_MGSV_IL_HISTORY_VALID);

  const float4 raw = velocityBuffer.Load(int3(centre, 0));
  const float2 velocity = (raw.ba * 2.0 - 1.0) * 64.0 / float2(cBufferWidth, cBufferHeight);

  // Reproject in gather space, then fetch the history with a manual bilinear tap so no sampler
  // is needed in this layout.
  const float2 uv = (float2(pixel) + 0.5) / float2(gather_size);
  const float2 previous = (uv - velocity) * float2(gather_size) - 0.5;
  const int2 base = (int2)floor(previous);
  const float2 fraction = frac(previous);
  float3 history = 0.0;
  float history_valid = 0.0;
  [unroll] for (int y = 0; y <= 1; ++y) {
    [unroll] for (int x = 0; x <= 1; ++x) {
      const int2 q = clamp(base + int2(x, y), int2(0, 0), gather_size - 1);
      const float4 tap = historyBuffer.Load(int3(q, 0));
      const float weight = (x != 0 ? fraction.x : 1.0 - fraction.x)
                         * (y != 0 ? fraction.y : 1.0 - fraction.y);
      history += tap.rgb * weight;
      history_valid += tap.a * weight;
    }
  }

  // Neighbourhood clip: the history may only move the result within what this frame's
  // neighbourhood allows, so a stale or mis-reprojected sample cannot smear.
  float3 minimum = current.rgb;
  float3 maximum = current.rgb;
  [unroll] for (int dy = -1; dy <= 1; ++dy) {
    [unroll] for (int dx = -1; dx <= 1; ++dx) {
      const int2 q = clamp(int2(pixel) + int2(dx, dy), int2(0, 0), gather_size - 1);
      const float3 neighbour = currentBuffer.Load(int3(q, 0)).rgb;
      minimum = min(minimum, neighbour);
      maximum = max(maximum, neighbour);
    }
  }

  const float weight = blend * saturate(history_valid);
  const float3 result = lerp(current.rgb, clamp(history, minimum, maximum), weight);
  accumulatedOutput[pixel] = float4(result, current.a);
}
