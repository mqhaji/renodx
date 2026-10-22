// MGSV screen-space diffuse gather - the denoise half.
//
// A-trous edge-aware filter, one dispatch per iteration, 5x5 taps whose stride doubles each
// step (1, 2, 4). Three iterations reach a 16-pixel radius with 75 taps instead of a 33x33
// kernel, and the weights are the same at every scale: depth, normal and luminance.
//
// The luminance weight divides the neighbour difference by the measured per-pixel noise from
// the temporal moments, so a converged surface is smoothed hard and a just-reset pixel is
// barely touched - which is what keeps edges from smearing during movement.
//
// Pixels with no valid gather sample are skipped, not averaged in: they hold zero radiance, and
// averaging that in is what turned starved patches into black blobs.

#include "../shared.h"
#include "../gbuffer.hlsli"

Texture2D<float4> bounceBuffer : register(t0);  // radiance to filter
Texture2D<float4> depthBuffer : register(t1);
Texture2D<float4> normalBuffer : register(t2);
// x = mean luminance (zero means never sampled), z = variance, w = relative variance.
Texture2D<float4> momentsBuffer : register(t3);
RWTexture2D<float4> filteredOutput : register(u0);

#define FILTER_MAX_RADIANCE 1.0e4

float Luminance(float3 color) {
  return dot(color, float3(0.2126, 0.7152, 0.0722));
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  const uint2 pixel = tid.xy;
  const float scale = max(CUSTOM_MGSV_IL_SCALE, 1.0);
  const int2 gather_size = int2(float2(cBufferWidth, cBufferHeight) / scale);
  if (pixel.x >= (uint)gather_size.x || pixel.y >= (uint)gather_size.y) return;

  // Depth and normals only exist at full resolution.
  const int2 centre = (int2)floor((float2(pixel) + 0.5) * scale);
  if (centre.x >= (int)cBufferWidth || centre.y >= (int)cBufferHeight) return;

  const float4 centre_sample = bounceBuffer.Load(int3(pixel, 0));
  const float z = LinearZ(depthBuffer.Load(int3(centre, 0)).x);
  if (!(z > 0.0) || z > 1.0e6) {
    // Sky and anything without geometry gets no bounce. Writing matters: a skipped write would
    // leave the texel holding whatever it held, including the zeros from creation.
    filteredOutput[pixel] = 0.0;
    return;
  }

  const float3 N = DecodeNormal(normalBuffer.Load(int3(centre, 0)).xyz);
  const float inv_z = 1.0 / z;

  const float4 centre_moments = momentsBuffer.Load(int3(pixel, 0));
  const float centre_luminance = Luminance(centre_sample.rgb);
  // A floor on the noise estimate: on a clean surface the measured variance can be small enough
  // that the weight would reject every neighbour and nothing would be smoothed at all.
  const float sigma_luminance =
      sqrt(max(centre_moments.z, 0.0)) + 0.02 + 0.1 * abs(centre_luminance);

  const int stride = 1 << (int)clamp(CUSTOM_MGSV_IL_FILTER_STEP, 0.0, 4.0);
  float3 sum = 0.0;
  float weight_sum = 0.0;

  [unroll] for (int dy = -2; dy <= 2; ++dy) {
    [unroll] for (int dx = -2; dx <= 2; ++dx) {
      const int2 q = int2(pixel) + int2(dx, dy) * stride;
      if (q.x < 0 || q.y < 0 || q.x >= gather_size.x || q.y >= gather_size.y) continue;

      // Never-sampled pixels carry zero radiance; averaging that in is what darkens neighbours.
      if (momentsBuffer.Load(int3(q, 0)).x <= 0.0) continue;

      const int2 full = (int2)floor((float2(q) + 0.5) * scale);
      const float qz = LinearZ(depthBuffer.Load(int3(full, 0)).x);
      if (!(qz > 0.0) || qz > 1.0e6) continue;

      const float3 neighbour = bounceBuffer.Load(int3(q, 0)).rgb;
      if (any(!isfinite(neighbour))) continue;

      const float3 Nq = DecodeNormal(normalBuffer.Load(int3(full, 0)).xyz);
      const float normal_match = saturate(dot(N, Nq));
      const float depth_match = 1.0 / (1.0 + 32.0 * abs(qz - z) * inv_z);
      const float luminance_match =
          1.0 / (1.0 + abs(Luminance(neighbour) - centre_luminance) / sigma_luminance);

      const float weight = normal_match * normal_match * normal_match * normal_match
                         * depth_match * luminance_match;
      sum += neighbour * weight;
      weight_sum += weight;
    }
  }

  if (weight_sum <= 1.0e-4) {
    filteredOutput[pixel] = centre_sample;
    return;
  }
  // A near-zero weight sum divides the radiance up into the thousands; that value then lives in
  // the history forever and renders black, so both the division and the result are bounded.
  filteredOutput[pixel] = float4(clamp(sum / weight_sum, 0.0, FILTER_MAX_RADIANCE), 1.0);
}
