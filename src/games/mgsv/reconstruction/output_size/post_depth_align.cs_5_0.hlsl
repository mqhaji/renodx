#include "copy_constants.hlsli"

Texture2D<float> input_depth : register(t0);
RWTexture2D<float> output_depth : register(u0);

// Keep depth discontinuities intact. This is current-frame geometry resampling,
// not a new temporal depth history or an interpolation across different surfaces.
[numthreads(8, 8, 1)] void main(uint3 thread_id : SV_DispatchThreadID) {
  if (any(thread_id.xy >= output_size)) return;
  const float2 uv = (float2(thread_id.xy) + 0.5f) / float2(output_size) + jitter_uv;
  const uint2 pixel = uint2(clamp(uv * float2(input_size), 0.f, float2(input_size - 1u)));
  output_depth[thread_id.xy] = input_depth.Load(int3(pixel, 0));
}
