cbuffer NativeSystem : register(b0) {
  uint4 native_system[4];
}

cbuffer OutputDimensions : register(b1) {
  float2 output_buffer_size;
  float2 padding;
}

RWByteAddressBuffer output_system : register(u0);

// Adjusts the game's system constants (b0 of the draw) for an output-sized
// target: c2 receives the output buffer size and its reciprocal, and the view
// size in c1.xy is scaled to keep its ratio to the buffer. Writes all 64 bytes
// to scratch so the caller installs them with one copy. Unrelated fields are
// copied as raw bits.
[numthreads(1, 1, 1)] void main() {
  // Keeps cropped and partial-view draws covering the same share of the buffer.
  const float2 view = asfloat(native_system[1].xy) * (output_buffer_size / asfloat(native_system[2].xy));
  const float4 buffer = float4(output_buffer_size, rcp(output_buffer_size));
  output_system.Store4(0u, native_system[0]);
  output_system.Store4(16u, uint4(asuint(view), native_system[1].zw));
  output_system.Store4(32u, asuint(buffer));
  output_system.Store4(48u, native_system[3]);
}
