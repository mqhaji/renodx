cbuffer NativeSystem : register(b0) {
  uint4 native_system[4];
}

cbuffer OutputDimensions : register(b1) {
  float2 output_buffer_size;
  float2 padding;
}

RWByteAddressBuffer output_system : register(u0);

// Preserve the native view-to-buffer ratio for cropped and subview draws.
[numthreads(1, 1, 1)] void main() {
  const float2 view = asfloat(native_system[1].xy) * (output_buffer_size / asfloat(native_system[2].xy));
  const float4 buffer = float4(output_buffer_size, rcp(output_buffer_size));
  output_system.Store2(0u, asuint(view));
  output_system.Store4(16u, asuint(buffer));
}
