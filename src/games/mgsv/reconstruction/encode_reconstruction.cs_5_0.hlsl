#include "../shared.h"
#include "prepare_constants.hlsli"

Texture2D<float4> linear_reconstruction : register(t0);
Texture2D<float4> scene_alpha_input : register(t1);
RWTexture2D<float4> encoded_scene_output : register(u0);

[numthreads(8, 8, 1)] void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
  uint width;
  uint height;
  encoded_scene_output.GetDimensions(width, height);
  if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height) return;

  const uint2 pixel = dispatch_thread_id.xy;
  const float3 linear_color = linear_reconstruction.Load(int3(pixel, 0)).rgb;
  uint input_width;
  uint input_height;
  scene_alpha_input.GetDimensions(input_width, input_height);
  float scene_alpha;
  if (input_width == width && input_height == height) {
    scene_alpha = scene_alpha_input.Load(int3(pixel, 0)).a;
  } else {
    // DoF uses this current-frame highlight value. Interpolate on the input
    // grid, clamping each tap so the last row and column retain their energy.
    const float2 input_position = ((float2(pixel) + 0.5f) / float2(width, height) + current_jitter_uv) * float2(input_width, input_height) - 0.5f;
    const int2 base_pixel = int2(floor(input_position));
    const float2 fraction = frac(input_position);
    const int2 last_pixel = int2(input_width, input_height) - 1;
    const float a00 = scene_alpha_input.Load(int3(clamp(base_pixel, 0, last_pixel), 0)).a;
    const float a10 = scene_alpha_input.Load(int3(clamp(base_pixel + int2(1, 0), 0, last_pixel), 0)).a;
    const float a01 = scene_alpha_input.Load(int3(clamp(base_pixel + int2(0, 1), 0, last_pixel), 0)).a;
    const float a11 = scene_alpha_input.Load(int3(clamp(base_pixel + int2(1, 1), 0, last_pixel), 0)).a;
    scene_alpha = lerp(lerp(a00, a10, fraction.x), lerp(a01, a11, fraction.x), fraction.y);
  }
  encoded_scene_output[pixel] = float4(
      renodx::color::srgb::Encode(max(0.f.xxx, linear_color)),
      scene_alpha);
}
