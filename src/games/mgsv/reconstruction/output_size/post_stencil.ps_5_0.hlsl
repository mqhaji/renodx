#include "copy_constants.hlsli"

Texture2D<uint2> input_stencil : register(t0);

// Each draw selects one fixed bit; dimensions and jitter stay shared in b0.
cbuffer StencilCopyBit : register(b1) {
  uint copy_bit;
}

void main(float4 position : SV_Position) {
  const uint2 pixel = uint2(clamp((position.xy / float2(output_size) + jitter_uv) * float2(input_size), 0.f, float2(input_size - 1u)));
  if ((input_stencil.Load(int3(pixel, 0)).y & copy_bit) == 0u) discard;
}
