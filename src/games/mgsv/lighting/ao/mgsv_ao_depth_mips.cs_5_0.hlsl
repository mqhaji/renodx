// One level of the AO pass's depth mip chain.
//
// The pass marches a fixed number of samples along each slice direction, so an occluder that
// falls *between* two sample positions is invisible however many slices are added. That was
// measured as the binding constraint: at 2 slices x 4 steps 89% of pixels found no occluder at
// all, and going to 12 steps raised the bounce's mean 2.4x without moving its peak. Fetching a
// step from a coarser level makes it cover a footprint instead of a point, which is the
// technique the reference implementation uses (XeGTAO's depth mip sampling, GT-VBAO's
// DEPTH_MIP_SAMPLING_OFFSET).
//
// Raw depth, not linearised. The pass linearises as `z / (depth - w)`, so a larger raw value is
// *nearer* and the background sits next to `w` at the small end. Reducing with max therefore
// already means "nearest wins, and a real surface beats background", which is the conservative
// direction - and it keeps the projection constants out of this pass entirely, so the chain
// cannot disagree with the shader about them.
//
// Sizes come from the destination view, which is a single-mip view and so reports its own
// level. Reading them back from the *source* would be wrong: D3D11 reports the resource's base
// size for a view that starts at a mip other than zero, which silently reduced the lower levels
// into the wrong area. Constants were the other option and are worse - a push constant at
// register 0 rebinds b0, which the AO pass is using for its own camera constants.
//
// The source is always the level directly below, so it is at most 2x the destination and a
// source texel past the edge reads zero, which a max-reduce cannot pick over real geometry.

Texture2D<float4> sourceDepth : register(t0);
RWTexture2D<float> destinationLevel : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  uint destination_width, destination_height;
  destinationLevel.GetDimensions(destination_width, destination_height);

  const uint2 destination = tid.xy;
  if (destination.x >= destination_width || destination.y >= destination_height) return;

  const uint2 source_size = uint2(destination_width, destination_height) * 2u;
  const uint2 base = destination * 2u;

  float nearest = 0.0;
  [unroll] for (int y = 0; y < 2; ++y) {
    [unroll] for (int x = 0; x < 2; ++x) {
      // A source level with an odd dimension has no fourth neighbour; clamping keeps the
      // reduction inside the texture rather than reading the wrap.
      const uint2 texel = min(base + uint2((uint)x, (uint)y), source_size - 1u);
      nearest = max(nearest, sourceDepth.Load(int3(texel, 0)).x);
    }
  }
  destinationLevel[destination] = nearest;
}
