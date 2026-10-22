// MGSV screen-space diffuse gather - the full-resolution resolve.
//
// The a-trous runs at the gather's resolution so it can be cheap; this pass brings the result
// back to the screen. It is a joint bilateral, not a bilinear: a plain bilinear taps across a
// silhouette would pull the background's bounce onto the foreground (and the reverse), which
// reads as a halo exactly where the eye is. Weighting each tap by how well its depth matches the
// output pixel's keeps the resolve inside the surface.
//
// At full gather resolution the fractional position collapses and this is a single load.

#include "../shared.h"
#include "../gbuffer.hlsli"

Texture2D<float4> filteredBuffer : register(t0);
Texture2D<float4> depthBuffer : register(t1);
RWTexture2D<float4> lightOutput : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  const uint2 pixel = tid.xy;
  if (pixel.x >= cBufferWidth || pixel.y >= cBufferHeight) return;

  const float scale = max(CUSTOM_MGSV_IL_SCALE, 1.0);
  const float2 position = (float2(pixel) + 0.5) / scale - 0.5;
  const int2 base = (int2)floor(position);
  const float2 fraction = frac(position);

  const float z = LinearZ(depthBuffer.Load(int3(pixel, 0)).x);
  const bool has_geometry = z > 0.0 && z < 1.0e6;
  const float inv_z = has_geometry ? 1.0 / z : 0.0;

  float3 radiance = 0.0;
  float weight_sum = 0.0;
  [unroll] for (int y = 0; y <= 1; ++y) {
    [unroll] for (int x = 0; x <= 1; ++x) {
      const int2 q = max(base + int2(x, y), int2(0, 0));
      const float4 tap = filteredBuffer.Load(int3(q, 0));
      const float bilinear = (x != 0 ? fraction.x : 1.0 - fraction.x)
                           * (y != 0 ? fraction.y : 1.0 - fraction.y);

      const int2 full = clamp((int2)floor((float2(q) + 0.5) * scale), int2(0, 0),
                              int2((int)cBufferWidth - 1, (int)cBufferHeight - 1));
      const float qz = LinearZ(depthBuffer.Load(int3(full, 0)).x);
      const float depth_match = (has_geometry && qz > 0.0)
                                    ? 1.0 / (1.0 + 32.0 * abs(qz - z) * inv_z)
                                    : 1.0;

      const float weight = bilinear * depth_match;
      radiance += tap.rgb * weight;
      weight_sum += weight;
    }
  }

  const float3 bounce = radiance / max(weight_sum, 1.0e-4) * CUSTOM_MGSV_IL_AMOUNT;
  const float4 centre = lightOutput.Load(int3(pixel, 0));
  const float3 output = CUSTOM_MGSV_IL_DEBUG > 0.5 ? bounce : centre.rgb + bounce;
  lightOutput[pixel] = float4(output, centre.a);
}
