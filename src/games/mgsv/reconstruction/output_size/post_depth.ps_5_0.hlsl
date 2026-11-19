#include "copy_constants.hlsli"

Texture2D<float> input_depth : register(t0);

float main(float4 position : SV_Position) : SV_Depth {
  const uint2 pixel = uint2(clamp((position.xy / float2(output_size) + jitter_uv) * float2(input_size), 0.f, float2(input_size - 1u)));
  return input_depth.Load(int3(pixel, 0));
}
