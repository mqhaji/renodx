#include "common.hlsli"
// Builds one level of the two reflection depth pyramids from the level below.
// Depth is reverse-Z, so the maximum of each cell is its nearest surface and
// the minimum its farthest; background is 0, the farthest there is. The
// farthest pyramid has no level 0: there both are the depth itself.
Texture2D<float> input_depth : register(t0);
Texture2D<float> input_farthest : register(t1);
RWTexture2D<float> output_depth : register(u0);
RWTexture2D<float> output_farthest : register(u1);

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
  const bool from_depth = all(uint2(input_width, input_height) == reflection_size);
  float nearest = 0.0;
  float farthest = 1.0;
  for (uint y = first.y; y < last.y; ++y) {
    for (uint x = first.x; x < last.x; ++x) {
      const float depth = input_depth.Load(int3(x, y, 0));
      nearest = max(nearest, depth);
      farthest = min(farthest, from_depth ? depth : input_farthest.Load(int3(x, y, 0)));
    }
  }
  output_depth[id.xy] = nearest;
  output_farthest[id.xy] = farthest;
}
