// MGSV screen-space diffuse gather - the temporal half.
//
// Accumulates the gather across frames so the filter only has to clean the residual instead of
// carrying all the noise. The gather's noise is temporally zero-mean - which is why DLSS
// already improves it - so accumulation is the right operation.
//
// Step 1 of the staged plan: plumbing only. Two history targets alternating as read/write, plus a
// separate accumulated target for the filter, so the pass never reads what it writes. The alpha
// channel carries the history length, normalized, which the length debug view shows and which
// later steps will use to weight the blend.
//
// Two debug views exist to verify this without guessing:
//   2 = |history - current|, which must go near-black when the camera is still
//   3 = history length, which must climb on a static surface

#include "../shared.h"
#include "../gbuffer.hlsli"

#define HISTORY_MAX_LENGTH 32.0

Texture2D<float4> currentBuffer : register(t0);  // this frame's gather
Texture2D<float4> historyBuffer : register(t1);  // previous accumulated result
Texture2D<float4> velocityBuffer : register(t2); // MGSV camera velocity, .ba
Texture2D<float4> depthBuffer : register(t3);
Texture2D<float4> normalBuffer : register(t4);
RWTexture2D<float4> historyOutput : register(u0);    // next frame's history, always the real result
RWTexture2D<float4> accumulatedOutput : register(u1); // what the filter reads, may show a probe

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  const uint2 pixel = tid.xy;
  const float scale = max(CUSTOM_MGSV_IL_SCALE, 1.0);
  const int2 gather_size = int2(float2(cBufferWidth, cBufferHeight) / scale);
  if (pixel.x >= (uint)gather_size.x || pixel.y >= (uint)gather_size.y) return;

  const int2 centre = (int2)floor((float2(pixel) + 0.5) * scale);
  if (centre.x >= (int)cBufferWidth || centre.y >= (int)cBufferHeight) return;

  const float4 current = currentBuffer.Load(int3(pixel, 0));

  const float4 raw = velocityBuffer.Load(int3(centre, 0));
  // The buffer still carries the projection jitter this frame was sampled with, so a static
  // camera reports a small constant motion in every pixel. Subtracting the jitter (the engine
  // stores it in NDC, hence the half) is what makes a still image reproject onto itself.
  const float2 velocity = (raw.ba * 2.0 - 1.0) * 64.0 / float2(cBufferWidth, cBufferHeight)
                        - cCameraCenterOffset * 0.5;

  // Reproject in gather space, then fetch the history with a manual bilinear tap so no sampler
  // is needed in this layout.
  const float2 uv = (float2(centre) + 0.5) / float2(cBufferWidth, cBufferHeight);
  const float2 previous = (uv - velocity) * float2(gather_size) - 0.5;
  const int2 base = (int2)floor(previous);
  const float2 fraction = frac(previous);
  float3 history = 0.0;
  float history_length = 0.0;
  [unroll] for (int y = 0; y <= 1; ++y) {
    [unroll] for (int x = 0; x <= 1; ++x) {
      const int2 q = clamp(base + int2(x, y), int2(0, 0), gather_size - 1);
      const float4 tap = historyBuffer.Load(int3(q, 0));
      const float weight = (x != 0 ? fraction.x : 1.0 - fraction.x)
                         * (y != 0 ? fraction.y : 1.0 - fraction.y);
      history += tap.rgb * weight;
      history_length += tap.a * HISTORY_MAX_LENGTH * weight;
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

  const bool has_history = history_length > 0.5;
  const float blend = CUSTOM_MGSV_IL_HISTORY * saturate(CUSTOM_MGSV_IL_HISTORY_VALID)
                    * (has_history ? 1.0 : 0.0);
  const float3 result = lerp(current.rgb, clamp(history, minimum, maximum), blend);

  const float length = has_history ? min(history_length + 1.0, HISTORY_MAX_LENGTH) : 1.0;
  const float4 real = float4(result, length / HISTORY_MAX_LENGTH);

  // The history always receives the real result: a probe written into it would poison the next
  // frame and make the view self-fulfilling.
  historyOutput[pixel] = real;

  if (CUSTOM_MGSV_IL_DEBUG > 3.5) {
    // Reprojection offset in pixels. R/G are the corrected offset, B is the raw buffer value,
    // so the two can be compared in one frame: the correction must shrink the typical offset.
    const float2 raw_velocity = (raw.ba * 2.0 - 1.0) * 64.0 / float2(cBufferWidth, cBufferHeight);
    accumulatedOutput[pixel] = float4(abs(velocity.x) * (float)cBufferWidth,
                                      abs(velocity.y) * (float)cBufferHeight,
                                      abs(raw_velocity.x) * (float)cBufferWidth, 1.0);
  } else if (CUSTOM_MGSV_IL_DEBUG > 2.5) {
    // History length: dark where the history was just reset, bright where it has accumulated.
    // Green marks pixels that saw no history at all - if the whole view is green, the read is
    // dead, not the accumulation.
    accumulatedOutput[pixel] = float4(length / HISTORY_MAX_LENGTH, has_history ? 0.0 : 1.0, 0.0, 1.0);
  } else if (CUSTOM_MGSV_IL_DEBUG > 1.5) {
    // History delta: must be near-black with a static camera.
    accumulatedOutput[pixel] = float4(abs(history - current.rgb) * 10.0, 1.0);
  } else {
    accumulatedOutput[pixel] = real;
  }
}
