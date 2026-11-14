#include "../shared.h"

#include "prepare_inputs.hlsli"

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

  const float2 native_velocity = DecodeNativeVelocity(pixel);
  float2 camera_velocity = native_velocity;
  if (camera_reprojection_valid > 0.f) {
    if (!ComputeMatrixCameraVelocity(uv, depth, camera_velocity)) camera_velocity = native_velocity;
  }
  const float4 object_sample = object_velocity_texture.Load(int3(pixel, 0));
  float2 object_velocity = native_velocity - current_jitter_uv * velocity_projection_jitter_scale;
  if (analytical_taa > 0.f) {
    // Keep the existing TAA motion diagnostics. Normal SDK operation uses the
    // same corrected composite motion as TAA's Auto mode.
    if (object_motion_mode >= 5.5f && object_motion_mode < 6.5f) {
      object_velocity = (object_sample.ba * 2.f - 1.f) * 64.f * reciprocal_render_size
                        - current_jitter_uv * velocity_projection_jitter_scale;
    } else if (object_motion_mode >= 1.5f && object_motion_mode < 2.5f) {
      object_velocity = native_velocity - current_jitter_uv;
    } else if (object_motion_mode >= 2.5f && object_motion_mode < 3.5f) {
      object_velocity = native_velocity + current_jitter_uv;
    } else if (object_motion_mode >= 3.5f && object_motion_mode < 4.5f) {
      object_velocity = native_velocity - current_jitter_uv + previous_jitter_uv;
    } else if (object_motion_mode >= 4.5f && object_motion_mode < 5.5f) {
      object_velocity = native_velocity + current_jitter_uv - previous_jitter_uv;
    }
    motion_debug_output[pixel] = float4(native_velocity, object_sample.r, 0.f);
  }
  float2 velocity = object_sample.r > 0.5f ? object_velocity : camera_velocity;
  if (analytical_taa > 0.f && object_motion_mode >= 0.5f && object_motion_mode < 1.5f) velocity = camera_velocity;
  // All consumers receive a previous-minus-current UV delta with jitter removed.
  motion_vector_output[pixel] = -velocity;
}
