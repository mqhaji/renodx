cbuffer NativeObject : register(b0) {
  uint4 native_object[13];
}

cbuffer OutputDimensions : register(b1) {
  float4 output_buffer;
}

RWByteAddressBuffer output_object : register(u0);

// Keep the native Kawase kernel measured in pixels on the enlarged bloom grid.
// Copy the full native prefix, preserving every unrelated field as raw bits.
[numthreads(1, 1, 1)] void main() {
  const float2 native_inverse_size = asfloat(native_object[12].xy);
  const float2 scale = output_buffer.zw / native_inverse_size;
  [unroll]
  for (uint row = 0u; row < 13u; ++row) {
    uint4 values = native_object[row];
    if (row == 9u) {
      values = asuint(asfloat(values) * scale.xyxy);
    } else if (row == 12u) {
      values.xy = asuint(output_buffer.zw);
    }
    output_object.Store4(row * 16u, values);
  }
}
