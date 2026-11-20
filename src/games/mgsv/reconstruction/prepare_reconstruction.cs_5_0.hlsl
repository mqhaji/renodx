#include "../shared.h"

#include "prepare_constants.hlsli"
#include "../rendering/motion.hlsli"

// Input preparation shared by every reconstruction method. Runs at render size
// on the game's D3D11 immediate context, when the scene's job runs at its cut.
// Reads the engine's encoded scene color, composed motion, depth, and object
// motion (its mask in .r, its motion in .ba). Writes the shared D3D12 inputs:
// linear color with the game's alpha, jitter-free motion, a depth copy, and for
// Analytical TAA only, a diagnostic motion image.
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

  const float2 native_velocity = MotionPixels(native_velocity_texture.Load(int3(pixel, 0))) * reciprocal_render_size;
  // The engine's velocity passes draw with the jittered projection, so both
  // motion images contain the current jitter on every pixel. It is removed from
  // each value read from them.
  const float2 unjittered_native_velocity = native_velocity - current_jitter_uv;
  // Static pixels use camera motion from depth and the unjittered camera
  // matrices when camera history is valid, and the game's motion otherwise.
  float2 camera_velocity = unjittered_native_velocity;
  if (camera_reprojection_valid > 0.f) {
    if (!GetCameraVelocity(uv, depth, current_jitter_uv,
                           float4x4(current_to_previous_clip_row_0, current_to_previous_clip_row_1,
                                    current_to_previous_clip_row_2, current_to_previous_clip_row_3), camera_velocity)) camera_velocity = unjittered_native_velocity;
  }
  const float4 object_sample = object_velocity_texture.Load(int3(pixel, 0));
  if (analytical_taa > 0.f) {
    motion_debug_output[pixel] = float4(native_velocity, object_sample.r, 0.f);
  }
  // Object pixels keep the object's own motion, so bone and object movement
  // survive. It comes from the object image, because the combined image holds
  // the sprint blur effect's motion instead while that effect draws.
  const float2 object_velocity = MotionPixels(object_sample) * reciprocal_render_size - current_jitter_uv;
  const float2 velocity = (object_sample.r > 0.5f ? object_velocity : camera_velocity);
  // All consumers receive a previous-minus-current UV delta with jitter removed.
  motion_vector_output[pixel] = -velocity;
}
