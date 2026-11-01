// MGSV GT-VBGI - full-resolution AO/GI resolve and application.
//
// The shared trace runs at AO resolution. This pass uses depth to resolve its
// bounce at output resolution without mixing freely across surface edges.

#include "../gbuffer.hlsli"
#include "../shared.h"

Texture2D<float4> giBuffer : register(t0);
Texture2D<float4> depthBuffer : register(t1);
Texture2D<float4> diagnosticBuffer : register(t2);
Texture2D<float2> aoBuffer : register(t3);
RWTexture2D<float4> lightOutput : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  const uint2 pixel = tid.xy;
  if (pixel.x >= cBufferWidth || pixel.y >= cBufferHeight) return;

  const float scale = max(CUSTOM_MGSV_GT_RESOLUTION_DIVISOR, 1.0);
  const float2 position = (float2(pixel) + 0.5) / scale - 0.5;
  const int2 base = (int2)floor(position);
  const float2 fraction = frac(position);

  const float depth = depthBuffer.Load(int3(pixel, 0)).x;
  const float z = LinearZ(depth);
  if (!(z > 0.0) || z > 1.0e6) return;
  const bool giReceiverValid = depth > 0.0 && z < 1.0e6;
  const float inv_z = 1.0 / z;
  uint gi_width, gi_height;
  giBuffer.GetDimensions(gi_width, gi_height);

  float3 radiance = 0.0;
  float weight_sum = 0.0;
  float visibility = 0.0;
  float visibility_weight = 0.0;
  const int diagnostic = (int)clamp(CUSTOM_MGSV_GI_DEBUG_VIEW, 0.0, 12.0);
  float3 diagnostic_sum = 0.0;
  float diagnostic_weight = 0.0;
  [unroll]
  for (int y = 0; y < 2; ++y) {
    [unroll]
    for (int x = 0; x < 2; ++x) {
      const int2 q = clamp(base + int2(x, y), int2(0, 0), int2(gi_width, gi_height) - 1);
      const float4 tap = giBuffer.Load(int3(q, 0));
      const float bilinear = (x != 0 ? fraction.x : 1.0 - fraction.x)
                             * (y != 0 ? fraction.y : 1.0 - fraction.y);

      const int2 full = clamp((int2)floor((float2(q) + 0.5) * scale), int2(0, 0),
                              int2((int)cBufferWidth - 1, (int)cBufferHeight - 1));
      const float qdepth = depthBuffer.Load(int3(full, 0)).x;
      const float qz = LinearZ(qdepth);
      const float depth_match = qz > 0.0 ? 1.0 / (1.0 + 32.0 * abs(qz - z) * inv_z) : 1.0;

      const float weight = bilinear * depth_match;
      const float ao = aoBuffer.Load(int3(q, 0)).r;
      visibility += (isfinite(ao) ? saturate(ao) : 1.0) * weight;
      visibility_weight += weight;
      // Preserve the AO resolve's neutral background taps. GI has stricter
      // geometry validity and must keep a separate normalization sum.
      if (!giReceiverValid || !(qdepth > 0.0 && qz > 0.0 && qz < 1.0e6)) continue;
      // Valid black remains in the weight sum; missing/invalid results do not.
      if (tap.a > 0.5 && all(isfinite(tap.rgb))) {
        radiance += tap.rgb * weight;
        weight_sum += weight;
      }
      // Invalid GI must remain visible in the validity view. Diagnostic RGB
      // therefore has its own weights and never feeds the radiance result.
      if (diagnostic >= 2 && diagnostic != 7) {
        diagnostic_sum += diagnosticBuffer.Load(int3(q, 0)).rgb * weight;
        diagnostic_weight += weight;
      }
    }
  }

  const float3 bounce = radiance / max(weight_sum, 1.0e-4) * CUSTOM_MGSV_GI_STRENGTH;
  const float4 centre = lightOutput.Load(pixel);
  const float ao = visibility_weight > 1.0e-4 ? visibility / visibility_weight : 1.0;
  const float3 shadedDiffuse = centre.rgb * saturate(ao);
  if (!giReceiverValid) {
    lightOutput[pixel] = float4(shadedDiffuse, centre.a);
    return;
  }
  // This diagnostic replaces diffuse only; native specular, receiver albedo,
  // fog, and later scene treatment still apply in deferred composition.
  // AO affects existing diffuse only. One FP16 store avoids rounding the AO
  // product before GI is added, while preserving the game's alpha exactly.
  float3 output = (diagnostic == 1 || diagnostic == 7) ? bounce : shadedDiffuse + bounce;
  if (diagnostic >= 2 && diagnostic != 7) output = diagnostic_sum / max(diagnostic_weight, 1.0e-4);
  if (diagnostic == 6) output *= CUSTOM_MGSV_GI_STRENGTH;
  // Preserve finite values in the native RGBA16F destination at high strengths.
  lightOutput[pixel] = float4(min(output, 65504.0), centre.a);
}
