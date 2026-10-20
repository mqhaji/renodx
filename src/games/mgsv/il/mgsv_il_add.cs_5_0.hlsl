// MGSV screen-space diffuse gather - the add half.
//
// Runs at full resolution, upsamples the filtered gather with a 2x2 bilinear tap, scales it by
// the amount, and adds it to the light. Splitting this from the filter is what lets the two
// expensive stages (gather and filter) run at half resolution while the image still gets a
// smooth, full-resolution result.

#include "../shared.h"
#include "../gbuffer.hlsli"

Texture2D<float4> filteredBuffer : register(t0);
RWTexture2D<float4> lightOutput : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  const uint2 pixel = tid.xy;
  if (pixel.x >= cBufferWidth || pixel.y >= cBufferHeight) return;

  // Texel-space position in the filtered target, so the upsample is exact at any scale. At
  // full resolution the fraction is zero and this collapses to a single load.
  const float scale = max(CUSTOM_MGSV_IL_SCALE, 1.0);
  const float2 position = (float2(pixel) + 0.5) / scale - 0.5;
  const int2 base = (int2)floor(position);
  const float2 fraction = frac(position);

  float3 radiance = 0.0;
  float validity = 0.0;
  [unroll] for (int y = 0; y <= 1; ++y) {
    [unroll] for (int x = 0; x <= 1; ++x) {
      const float4 tap = filteredBuffer.Load(int3(max(base + int2(x, y), int2(0, 0)), 0));
      const float weight = (x != 0 ? fraction.x : 1.0 - fraction.x)
                         * (y != 0 ? fraction.y : 1.0 - fraction.y);
      radiance += tap.rgb * weight;
      validity += tap.a * weight;
    }
  }

  const float3 bounce = radiance * CUSTOM_MGSV_IL_AMOUNT * saturate(validity);
  const float4 centre = lightOutput.Load(int3(pixel, 0));
  const float3 output = CUSTOM_MGSV_IL_DEBUG > 0.5 ? bounce : centre.rgb + bounce;
  lightOutput[pixel] = float4(output, centre.a);
}
