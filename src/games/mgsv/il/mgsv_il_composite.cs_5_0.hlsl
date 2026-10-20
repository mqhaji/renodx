// MGSV screen-space diffuse gather - the composite half.
//
// The gather writes the neighbourhood's mean radiance into its own target; this pass does the
// two things that cannot happen there: it denoises that mean, and it adds the result to the
// light. Splitting them is what makes a denoiser possible at all - the gather's output has to
// exist as a texture before anything can filter it.
//
// The filter is a 5x5 bilateral in screen space, weighted by normal similarity and relative
// depth difference. A plain blur would smear light across silhouettes, which reads as a halo;
// the depth and normal weights keep it inside a surface. Samples the gather rejected (alpha
// zero) are skipped, so a starved pixel stays dark instead of being filled by its neighbours.

#include "../shared.h"
#include "../gbuffer.hlsli"

Texture2D<float4> bounceBuffer : register(t0);  // rgb mean radiance, a validity
Texture2D<float4> depthBuffer : register(t1);
Texture2D<float4> normalBuffer : register(t2);
RWTexture2D<float4> lightOutput : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  const uint2 pixel = tid.xy;
  if (pixel.x >= cBufferWidth || pixel.y >= cBufferHeight) return;

  const float z = LinearZ(depthBuffer.Load(int3(pixel, 0)).x);
  if (!(z > 0.0) || z > 1.0e6) return;

  const float3 N = DecodeNormal(normalBuffer.Load(int3(pixel, 0)).xyz);
  const float inv_z = 1.0 / z;

  float3 sum = 0.0;
  float weight_sum = 0.0;

  [unroll] for (int dy = -2; dy <= 2; ++dy) {
    [unroll] for (int dx = -2; dx <= 2; ++dx) {
      const int2 q = int2(pixel) + int2(dx, dy);
      if (q.x < 0 || q.y < 0 || q.x >= (int)cBufferWidth || q.y >= (int)cBufferHeight) continue;

      const float4 gathered = bounceBuffer.Load(int3(q, 0));
      if (gathered.a < 0.5) continue;  // the gather had too few contributors here

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

  if (weight_sum <= 0.0) return;
  const float3 bounce = (sum / weight_sum) * CUSTOM_MGSV_IL_AMOUNT * bounceBuffer.Load(int3(pixel, 0)).a;

  const float4 centre = lightOutput.Load(int3(pixel, 0));
  const float3 output = CUSTOM_MGSV_IL_DEBUG > 0.5 ? bounce : centre.rgb + bounce;
  lightOutput[pixel] = float4(output, centre.a);
}
