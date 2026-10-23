// MGSV VBAO - the full-resolution resolve.
//
// The AO pass computes the un-occluded fraction at a fraction of the pixels and writes it to its
// own target; this pass brings it back to the screen and multiplies the light. A plain bilinear
// upsample would bleed occlusion across silhouettes - a foreground edge would darken the wall
// behind it - so each tap is weighted by how well its depth matches the output pixel. That is the
// same joint-bilateral shape the indirect lighting resolve uses, and it is what makes half
// resolution acceptable at all.
//
// The light is read and written through the same UAV: a typed UAV load of the same texel is not
// an aliasing hazard, while binding the light as an SRV and a UAV at once silently drops the SRV.
// Alpha is written through untouched, because the retail chain reads it as its "pixel was shaded"
// flag.

#include "../shared.h"
#include "../gbuffer.hlsli"

Texture2D<float4> aoBuffer : register(t0);
Texture2D<float4> depthBuffer : register(t1);
RWTexture2D<float4> lightBuffer : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  const uint2 pixel = tid.xy;
  if (pixel.x >= cBufferWidth || pixel.y >= cBufferHeight) return;

  const float z = LinearZ(depthBuffer.Load(int3(pixel, 0)).x);
  if (!(z > 0.0) || z > 1.0e6) return;

  const float scale = max(CUSTOM_MGSV_AO_SCALE, 1.0);
  const float2 position = (float2(pixel) + 0.5) / scale - 0.5;
  const int2 base = (int2)floor(position);
  const float2 fraction = frac(position);
  const float inv_z = 1.0 / z;

  float factor = 0.0;
  float weight_sum = 0.0;
  [unroll] for (int y = 0; y <= 1; ++y) {
    [unroll] for (int x = 0; x <= 1; ++x) {
      const int2 q = max(base + int2(x, y), int2(0, 0));
      const float bilinear = (x != 0 ? fraction.x : 1.0 - fraction.x)
                           * (y != 0 ? fraction.y : 1.0 - fraction.y);

      // Depth of this tap, fetched at full resolution so the test is exact.
      const int2 full = clamp((int2)floor((float2(q) + 0.5) * scale), int2(0, 0),
                              int2((int)cBufferWidth - 1, (int)cBufferHeight - 1));
      const float qz = LinearZ(depthBuffer.Load(int3(full, 0)).x);
      const float depth_match = qz > 0.0
                                    ? 1.0 / (1.0 + 32.0 * abs(qz - z) * inv_z)
                                    : 1.0;

      const float weight = bilinear * depth_match;
      factor += aoBuffer.Load(int3(q, 0)).r * weight;
      weight_sum += weight;
    }
  }

  const float applied = weight_sum > 1.0e-4 ? factor / weight_sum : 1.0;
  const float4 light = lightBuffer.Load(int3(pixel, 0));
  lightBuffer[pixel] = float4(light.rgb * saturate(applied), light.a);
}
