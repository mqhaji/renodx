#include "common.hlsli"
// Builds one level of the reflection depth pyramid from the level below. Depth
// is reverse-Z, so the maximum of each cell is its nearest surface.
Texture2D<float> input_depth : register(t0);
RWTexture2D<float> output_depth : register(u0);

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
  uint width, height;
  output_depth.GetDimensions(width, height);
  if (any(id.xy >= uint2(width, height))) return;
  uint input_width, input_height;
  input_depth.GetDimensions(input_width, input_height);
  // At odd sizes the last cell also takes the trailing texel. Every source
  // texel then belongs to a cell, so thin surfaces cannot drop out.
  const uint2 first = id.xy * 2u;
  const uint2 last = min(first + 2u + uint2(id.x + 1u == width, id.y + 1u == height), uint2(input_width, input_height));
  float nearest = 0.0;
  for (uint y = first.y; y < last.y; ++y) {
    for (uint x = first.x; x < last.x; ++x) {
      nearest = max(nearest, input_depth.Load(int3(x, y, 0)));
    }
  }
  output_depth[id.xy] = nearest;
}
