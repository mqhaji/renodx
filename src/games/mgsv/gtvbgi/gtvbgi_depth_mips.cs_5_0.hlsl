// GT-VBGI reverse-Z depth reduction: nearest depth wins.
// Each dispatch writes one mip. Odd trailing rows/columns join the last footprint.

Texture2D<float4> sourceDepth : register(t0);
RWTexture2D<float> destinationLevel : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 tid: SV_DispatchThreadID) {
  uint destination_width, destination_height;
  destinationLevel.GetDimensions(destination_width, destination_height);

  const uint2 destination = tid.xy;
  if (destination.x >= destination_width || destination.y >= destination_height) return;

  uint source_width, source_height;
  sourceDepth.GetDimensions(source_width, source_height);
  const uint2 base = destination * 2u;
  const uint2 end = uint2(destination.x + 1u == destination_width ? source_width : min(base.x + 2u, source_width),
                          destination.y + 1u == destination_height ? source_height : min(base.y + 2u, source_height));

  float nearest = 0.0;
  [loop]
  for (uint y = base.y; y < end.y; ++y) {
    [loop]
    for (uint x = base.x; x < end.x; ++x) {
      nearest = max(nearest, sourceDepth.Load(int3(uint2(x, y), 0)).x);
    }
  }
  destinationLevel[destination] = nearest;
}
