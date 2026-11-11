// SPDX-License-Identifier: MIT
// Copyright and license notices: ../README.md#credits-and-license.

// GT-VBGI reverse-Z depth reduction: nearest depth wins.
// Each dispatch writes one mip. Odd trailing rows/columns join the last footprint.

Texture2D<float4> source_depth : register(t0);
RWTexture2D<float> destination_level : register(u0);

[numthreads(8, 8, 1)]

void main(uint3 thread_id : SV_DispatchThreadID) {
  uint destination_width, destination_height;
  destination_level.GetDimensions(destination_width, destination_height);

  const uint2 destination = thread_id.xy;
  if (destination.x >= destination_width || destination.y >= destination_height) return;

  uint source_width, source_height;
  source_depth.GetDimensions(source_width, source_height);
  const uint2 base = destination * 2u;
  const uint2 end = uint2(destination.x + 1u == destination_width ? source_width : min(base.x + 2u, source_width),
                          destination.y + 1u == destination_height ? source_height : min(base.y + 2u, source_height));

  float nearest = 0.0;
  [loop]
  for (uint y = base.y; y < end.y; ++y) {
    [loop]
    for (uint x = base.x; x < end.x; ++x) {
      nearest = max(nearest, source_depth.Load(int3(uint2(x, y), 0)).x);
    }
  }
  destination_level[destination] = nearest;
}
