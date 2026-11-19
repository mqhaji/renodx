cbuffer NativeSystem : register(b0) {
  uint4 native_system[4];
}

cbuffer OutputDimensions : register(b1) {
  float2 output_buffer_size;
  float2 padding;
}

RWByteAddressBuffer output_system : register(u0);

// Copy the full native prefix so the caller can install it in one operation.
// Integer copies preserve all unrelated fields, including packed values.
[numthreads(1, 1, 1)] void main() {
  // Preserve the view-to-buffer ratio for cropped and subview draws.
  const float2 view = asfloat(native_system[1].xy) * (output_buffer_size / asfloat(native_system[2].xy));
  const float4 buffer = float4(output_buffer_size, rcp(output_buffer_size));
  output_system.Store4(0u, native_system[0]);
  output_system.Store4(16u, uint4(asuint(view), native_system[1].zw));
  output_system.Store4(32u, asuint(buffer));
  output_system.Store4(48u, native_system[3]);
}
