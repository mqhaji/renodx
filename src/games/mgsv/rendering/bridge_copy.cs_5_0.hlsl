Texture2D<float4> input_color : register(t0);
RWTexture2D<float4> output_color : register(u0);

[numthreads(8, 8, 1)] void main(uint3 dispatch_id : SV_DispatchThreadID) {
  uint width, height;
  output_color.GetDimensions(width, height);
  if (dispatch_id.x >= width || dispatch_id.y >= height) return;

  // Preserve the game's encoding and alpha across the API boundary.
  output_color[dispatch_id.xy] = input_color.Load(int3(dispatch_id.xy, 0));
}
