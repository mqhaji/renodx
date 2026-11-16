#include "../shared.h"

#include "prepare_constants.hlsli"
#include "../rendering/motion.hlsli"

Texture2D<float4> encoded_color_texture : register(t0);
Texture2D<float4> native_velocity_texture : register(t1);
Texture2D<float> depth_texture : register(t2);
Texture2D<float4> object_velocity_texture : register(t3);

RWTexture2D<float4> linear_color_output : register(u0);
RWTexture2D<float2> motion_vector_output : register(u1);

RWTexture2D<float> depth_output : register(u2);
RWTexture2D<float4> motion_debug_output : register(u3);

[numthreads(8, 8, 1)] void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
  if (any(dispatch_thread_id.xy >= render_size)) return;
  const uint2 pixel = dispatch_thread_id.xy;
  const float2 uv = (float2(pixel) + 0.5f.xx) * reciprocal_render_size;

  // Decode once while writing the shared input. Alpha is the game's current
  // highlight mask, not color; DoF needs it unchanged after reconstruction.
  const float4 encoded = encoded_color_texture.Load(int3(pixel, 0));
  linear_color_output[pixel] = float4(renodx::color::srgb::Decode(max(0.f.xxx, encoded.rgb)), encoded.a);
  const float depth = depth_texture.Load(int3(pixel, 0));
  depth_output[pixel] = depth;

  const float2 native_velocity = DecodeNativeMotion(native_velocity_texture.Load(int3(pixel, 0)).ba, reciprocal_render_size);
  float2 camera_velocity = native_velocity;
  if (camera_reprojection_valid > 0.f) {
    if (!GetCameraVelocity(uv, depth, current_jitter_uv,
                           float4x4(current_to_previous_clip_row_0, current_to_previous_clip_row_1,
                                    current_to_previous_clip_row_2, current_to_previous_clip_row_3), camera_velocity)) camera_velocity = native_velocity;
  }
  const float4 object_sample = object_velocity_texture.Load(int3(pixel, 0));
  // Native object motion contains current projection jitter; matrix camera
  // motion is already unjittered. Preserve deformation motion on object pixels.
  const float2 object_velocity = native_velocity - current_jitter_uv;
  if (analytical_taa > 0.f) {
    motion_debug_output[pixel] = float4(native_velocity, object_sample.r, 0.f);
  }
  const float2 velocity = (object_sample.r > 0.5f ? object_velocity : camera_velocity);
  // All consumers receive a previous-minus-current UV delta with jitter removed.
  motion_vector_output[pixel] = -velocity;
}
