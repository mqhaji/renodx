#include "copy_constants.hlsli"

// Restores one stencil bit in the output-sized depth/stencil target. The draw's
// stencil state writes that bit wherever this shader does not discard, so eight
// draws restore all eight bits.
Texture2D<uint2> input_stencil : register(t0);

// The bit this draw restores; dimensions and jitter come from b0.
cbuffer StencilCopyBit : register(b1) {
  uint copy_bit;
}

void main(float4 position : SV_Position) {
  const uint2 pixel = uint2(clamp((position.xy / float2(output_size) + jitter_uv) * float2(input_size), 0.f, float2(input_size - 1u)));
  if ((input_stencil.Load(int3(pixel, 0)).y & copy_bit) == 0u) discard;
}
