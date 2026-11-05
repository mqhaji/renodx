cbuffer NativeObject : register(b0) {
  uint4 native_object[13];
}

cbuffer OutputDimensions : register(b1) {
  float4 output_buffer;
}

RWByteAddressBuffer output_object : register(u0);

// Keep the native Kawase kernel measured in pixels on the enlarged bloom grid.
// Its weight and unrelated object parameters remain in the original buffer.
[numthreads(1, 1, 1)] void main() {
  const float2 native_inverse_size = asfloat(native_object[12].xy);
  const float2 scale = output_buffer.zw / native_inverse_size;
  output_object.Store4(0u, asuint(asfloat(native_object[9]) * scale.xyxy));
  output_object.Store2(16u, asuint(output_buffer.zw));
}
