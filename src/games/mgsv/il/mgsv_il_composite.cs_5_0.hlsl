// MGSV screen-space diffuse gather - the denoise half.
//
// The gather writes the neighbourhood's mean radiance into its own target; this pass filters
// it at the same resolution and leaves the result for the add pass. It runs at the gather's
// resolution, so half-resolution GI costs a quarter here as well - which matters, because the
// filter is the most expensive stage per pixel.
//
// The filter is a bilateral in screen space, weighted by normal similarity and relative depth
// difference. A plain blur smears light across silhouettes, which reads as a halo; the depth
// and normal weights keep it inside a surface. Samples the gather rejected (alpha zero) are
// skipped, so a starved pixel stays dark instead of being filled by its neighbours.

#include "../shared.h"
#include "../gbuffer.hlsli"

Texture2D<float4> bounceBuffer : register(t0);  // rgb mean radiance, a validity
Texture2D<float4> depthBuffer : register(t1);
Texture2D<float4> normalBuffer : register(t2);
// x = running mean luminance. Zero means this pixel has never had a single valid gather
// sample, which is the only case where its radiance must not be averaged in.
Texture2D<float4> momentsBuffer : register(t3);
RWTexture2D<float4> filteredOutput : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  const uint2 pixel = tid.xy;

  // Full-resolution coordinate of this gather pixel: depth and normals only exist there.
  const float scale = max(CUSTOM_MGSV_IL_SCALE, 1.0);
  const int2 centre = (int2)floor((float2(pixel) + 0.5) * scale);
  if (centre.x >= (int)cBufferWidth || centre.y >= (int)cBufferHeight) return;

  const float z = LinearZ(depthBuffer.Load(int3(centre, 0)).x);
  if (!(z > 0.0) || z > 1.0e6) {
    // Sky and any pixel without geometry get no bounce. Writing zero matters: returning here
    // leaves the target holding whatever it held, and the texels that were never written are
    // the zeros from creation - which is how black patches grew over the image.
    filteredOutput[pixel] = 0.0;
    return;
  }

  const float3 N = DecodeNormal(normalBuffer.Load(int3(centre, 0)).xyz);
  const float inv_z = 1.0 / z;
  const int radius = (int)clamp(CUSTOM_MGSV_IL_FILTER_RADIUS, 1.0, 3.0);

  float3 sum = 0.0;
  float weight_sum = 0.0;

  [loop] for (int dy = -radius; dy <= radius; ++dy) {
    [loop] for (int dx = -radius; dx <= radius; ++dx) {
      const int2 bounce_pixel = int2(pixel) + int2(dx, dy);
      if (bounce_pixel.x < 0 || bounce_pixel.y < 0) continue;

      // Out-of-range loads return zero, and alpha zero is rejected below, so the shader does
      // not need the gather target's size.
      // Alpha carries the history length, not validity, so only a *never written* texel is
      // rejected. Rejecting anything under half would black out every disocclusion for 15 frames.
      // Starved gather pixels hold zero radiance. Averaging that in turns every starved patch
      // into a black blob, so taps that never had a sample are skipped and filled by their
      // neighbours instead.
      if (momentsBuffer.Load(int3(bounce_pixel, 0)).x <= 0.0) continue;
      const float4 gathered = bounceBuffer.Load(int3(bounce_pixel, 0));

      const int2 q = (int2)floor((float2(bounce_pixel) + 0.5) * scale);
      const float qz = LinearZ(depthBuffer.Load(int3(q, 0)).x);
      if (!(qz > 0.0) || qz > 1.0e6) continue;

      const float3 Nq = DecodeNormal(normalBuffer.Load(int3(q, 0)).xyz);
      const float normal_match = saturate(dot(N, Nq));
      const float depth_match = 1.0 / (1.0 + 32.0 * abs(qz - z) * inv_z);
      const float weight = normal_match * normal_match * normal_match * normal_match * depth_match;
      sum += gathered.rgb * weight;
      weight_sum += weight;
    }
  }

  if (weight_sum <= 1.0e-4) {
    filteredOutput[pixel] = bounceBuffer.Load(int3(pixel, 0));
    return;
  }
  // A near-zero weight sum divides the accumulated radiance up into the thousands, and one
  // non-finite value poisons the history for every later frame (the temporal blend keeps it) -
  // which is what a black patch actually is.
  const float3 filtered = sum / weight_sum;
  filteredOutput[pixel] = float4(clamp(filtered, 0.0, 1.0e4), 1.0);
}
