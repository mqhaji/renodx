#include "../../../../shaders/color/srgb.hlsl"

#include "copy_constants.hlsli"

Texture2D<float4> input_color : register(t0);
RWTexture2D<float4> output_color : register(u0);

// This fallback uses this submission's current scene when temporal work was
// canceled. Decode before interpolation; the next native pass expects encoding.
[numthreads(8, 8, 1)] void main(uint3 thread_id : SV_DispatchThreadID) {
  if (any(thread_id.xy >= output_size)) return;
  const float2 position = ((float2(thread_id.xy) + 0.5f) / float2(output_size) + jitter_uv) * float2(input_size) - 0.5f;
  const int2 base = int2(floor(position));
  const float2 weight = frac(position);
  float4 samples[4];
  [unroll] for (uint i = 0u; i < 4u; ++i) {
    const int2 pixel = clamp(base + int2(i & 1u, i >> 1u), int2(0, 0), int2(input_size) - 1);
    samples[i] = input_color.Load(int3(pixel, 0));
    samples[i].rgb = renodx::color::srgb::DecodeSafe(samples[i].rgb);
  }
  float4 result = lerp(lerp(samples[0], samples[1], weight.x), lerp(samples[2], samples[3], weight.x), weight.y);
  result.rgb = renodx::color::srgb::EncodeSafe(result.rgb);
  output_color[thread_id.xy] = result;
}
