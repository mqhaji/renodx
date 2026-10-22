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
// Previous frame's geometry and history length. The radiance history cannot carry this: its
// alpha is already the length, and a rejection test needs depth and normal, not colour.
Texture2D<float4> historyAuxBuffer : register(t5);
// Running luminance moments. xy = mean and mean-square, so the variance the denoiser needs is
// m2 - m1^2 at any pixel, without storing a second history of the signal itself.
Texture2D<float4> momentsBuffer : register(t6);
RWTexture2D<float4> historyOutput : register(u0);    // next frame's history, always the real result
RWTexture2D<float4> accumulatedOutput : register(u1); // what the filter reads, may show a probe
RWTexture2D<float4> historyAuxOutput : register(u2);  // z, normal.xy, length for the next frame
RWTexture2D<float4> momentsOutput : register(u3);     // m1, m2, variance

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  const uint2 pixel = tid.xy;
  const float scale = max(CUSTOM_MGSV_IL_SCALE, 1.0);
  const int2 gather_size = int2(float2(cBufferWidth, cBufferHeight) / scale);
  if (pixel.x >= (uint)gather_size.x || pixel.y >= (uint)gather_size.y) return;

  const int2 centre = (int2)floor((float2(pixel) + 0.5) * scale);
  if (centre.x >= (int)cBufferWidth || centre.y >= (int)cBufferHeight) return;

  const float4 current = currentBuffer.Load(int3(pixel, 0));
  const float z = LinearZ(depthBuffer.Load(int3(centre, 0)).x);
  const float3 N = DecodeNormal(normalBuffer.Load(int3(centre, 0)).xyz);

  const float4 raw = velocityBuffer.Load(int3(centre, 0));
  // Plain decode, no jitter term. Measured: b0's cCameraCenterOffset is 0.000 px at this hook
  // while the TAA jitter is ~0.4 px, and the decoded velocity at a still camera is 0.031 px -
  // the velocity pass already runs with the jittered projection, so the jitter cancels in the
  // buffer instead of appearing in it.
  const float2 velocity = (raw.ba * 2.0 - 1.0) * 64.0 / float2(cBufferWidth, cBufferHeight);

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

  // A pixel whose gather found too few valid samples writes zero with alpha zero. Blending that
  // in darkens the pixel a little every frame until it goes black, and it is also what made the
  // variance explode: zeros alternating with real values.
  const bool has_sample = current.a > 0.5;

  // Neighbourhood clip: the history may only move the result within what this frame's
  // neighbourhood allows, so a stale or mis-reprojected sample cannot smear. Starved neighbours
  // are skipped, otherwise the range always includes zero and the history can only darken.
  float3 minimum = current.rgb;
  float3 maximum = current.rgb;
  [unroll] for (int dy = -1; dy <= 1; ++dy) {
    [unroll] for (int dx = -1; dx <= 1; ++dx) {
      const int2 q = clamp(int2(pixel) + int2(dx, dy), int2(0, 0), gather_size - 1);
      const float4 neighbour = currentBuffer.Load(int3(q, 0));
      if (neighbour.a <= 0.5) continue;
      minimum = min(minimum, neighbour.rgb);
      maximum = max(maximum, neighbour.rgb);
    }
  }

  // Rejection: compare this frame's geometry against the geometry the history was accumulated
  // on. A disocclusion or a silhouette cannot be recognised from colour alone, which is why the
  // aux target exists.
  const int2 aux_pixel = clamp((int2)round(previous), int2(0, 0), gather_size - 1);
  const float4 previous_aux = historyAuxBuffer.Load(int3(aux_pixel, 0));
  const float previous_z = previous_aux.x;
  const float2 previous_normal_xy = previous_aux.yz;
  // Only the xy is stored, and the packed normal's nz is negative by construction
  // (nz = enc.z^2 - 1), so reconstruct it with this frame's sign instead of a blind +sqrt.
  const float previous_normal_z = (N.z < 0.0 ? -1.0 : 1.0)
                                  * sqrt(saturate(1.0 - dot(previous_normal_xy, previous_normal_xy)));
  const float3 previous_normal = float3(previous_normal_xy, previous_normal_z);
  const bool inside = previous.x >= 0.0 && previous.y >= 0.0
                      && previous.x <= (float)(gather_size.x - 1)
                      && previous.y <= (float)(gather_size.y - 1);
  const bool depth_matches = previous_z > 0.0 && abs(z - previous_z) <= 0.02 * max(z, 1.0e-6);
  const bool normal_matches = dot(N, previous_normal) >= 0.9;
  const bool has_history = inside && depth_matches && normal_matches && history_length > 0.5;

  // A freshly reset pixel trusts only the current sample and ramps back up; a converged one is
  // limited by the user's ceiling.
  const float adaptive = 1.0 - 1.0 / max(history_length, 1.0);
  const float blend = has_history
                          ? min(CUSTOM_MGSV_IL_HISTORY, adaptive) * saturate(CUSTOM_MGSV_IL_HISTORY_VALID)
                          : 0.0;
  const float3 result = has_sample
                            ? lerp(current.rgb, clamp(history, minimum, maximum), blend)
                            : history;

  const float length = has_sample
                           ? (has_history ? min(history_length + 1.0, HISTORY_MAX_LENGTH) : 1.0)
                           : max(history_length, 1.0);
  const float4 real = float4(result, length / HISTORY_MAX_LENGTH);

  // The history always receives the real result: a probe written into it would poison the next
  // frame and make the view self-fulfilling.
  historyOutput[pixel] = real;
  historyAuxOutput[pixel] = float4(z, N.x, N.y, length / HISTORY_MAX_LENGTH);

  // Luminance moments of the incoming sample, not of the blended result: the variance has to
  // describe how much the gather itself fluctuates, otherwise it only measures the output's
  // already-filtered wobble and the denoiser gets no signal about where the noise is.
  const float sample_luminance = dot(current.rgb, float3(0.2126, 0.7152, 0.0722));
  const float2 previous_moments = momentsBuffer.Load(int3(aux_pixel, 0)).xy;
  float m1 = previous_moments.x;
  float m2 = previous_moments.y;
  if (current.a > 0.5) {
    if (has_history) {
      m1 = lerp(sample_luminance, m1, blend);
      m2 = lerp(sample_luminance * sample_luminance, m2, blend);
    } else {
      m1 = sample_luminance;
      m2 = sample_luminance * sample_luminance;
    }
  }
  const float variance = max(m2 - m1 * m1, 0.0);
  // Brightness-relative spread: absolute variance is meaningless across a dark corridor and a
  // lit wall, and the a-trous needs a weight that behaves the same in both.
  const float relative_variance = variance / max(m1 * m1, 1.0e-6);
  momentsOutput[pixel] = float4(m1, m2, variance, relative_variance);

  if (CUSTOM_MGSV_IL_DEBUG > 4.5) {
    // Variance: black is a clean average, mid grey is a 100% relative spread, white is worse.
    // The rational mapping keeps the whole range visible instead of saturating everywhere.
    const float display = relative_variance / (1.0 + relative_variance);
    accumulatedOutput[pixel] = float4(display, 0.0, 0.0, 1.0);
  } else if (CUSTOM_MGSV_IL_DEBUG > 3.5) {
    // Reprojection offset in pixels. R/G are the jitter the engine advertises in b0, in signed
    // pixels; B is the raw velocity magnitude. If R/G are zero while the scene renders, b0 is
    // not where the jitter lives for this pass.
    const float2 raw_velocity = (raw.ba * 2.0 - 1.0) * 64.0 / float2(cBufferWidth, cBufferHeight);
    accumulatedOutput[pixel] = float4(cCameraCenterOffset.x * (float)cBufferWidth,
                                      cCameraCenterOffset.y * (float)cBufferHeight,
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
