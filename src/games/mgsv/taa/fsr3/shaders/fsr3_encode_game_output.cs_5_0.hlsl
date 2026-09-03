#include "../../../shared.h"

Texture2D<float4> linear_fsr_output : register(t0);
Texture2D<float4> encoded_scene_input : register(t1);
RWTexture2D<float4> encoded_scene_output : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
  uint width;
  uint height;
  encoded_scene_output.GetDimensions(width, height);
  if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height) return;

  const uint2 pixel = dispatch_thread_id.xy;
  const float3 linear_color = linear_fsr_output.Load(int3(pixel, 0)).rgb;
  const float scene_alpha = encoded_scene_input.Load(int3(pixel, 0)).a;
  encoded_scene_output[pixel] = float4(
      renodx::color::srgb::Encode(max(0.f.xxx, linear_color)),
      scene_alpha);
}