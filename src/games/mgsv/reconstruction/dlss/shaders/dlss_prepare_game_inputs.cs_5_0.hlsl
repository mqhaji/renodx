#include "../../../shared.h"

#define MGSV_PREPARE_CONSTANTS DlssPrepareConstants
#include "../../prepare_inputs.hlsli"
#undef MGSV_PREPARE_CONSTANTS

RWTexture2D<float> depth_output : register(u2);

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id: SV_DispatchThreadID) {
  if (any(dispatch_thread_id.xy >= render_size)) return;

  const uint2 pixel = dispatch_thread_id.xy;
  const float2 uv = (float2(pixel) + 0.5f.xx) * reciprocal_render_size;
  const float4 encoded_color = encoded_color_texture.Load(int3(pixel, 0));
  linear_color_output[pixel] = float4(
      renodx::color::srgb::Decode(max(0.f.xxx, encoded_color.rgb)),
      encoded_color.a);

  const float depth = depth_texture.Load(int3(pixel, 0));
  depth_output[pixel] = depth;

  const float2 native_velocity = DecodeNativeVelocity(pixel);
  float2 camera_velocity = native_velocity;
  if (camera_reprojection_valid > 0.f) {
    if (!ComputeMatrixCameraVelocity(uv, depth, camera_velocity)) camera_velocity = native_velocity;
  }

  const float object_mask = object_velocity_texture.Load(int3(pixel, 0)).r;
  const float2 object_velocity = native_velocity - current_jitter_uv * velocity_projection_jitter_scale;
  const float2 current_minus_previous = object_mask > 0.5f ? object_velocity : camera_velocity;

  // DLSS locates the previous sample by adding motion to the current pixel.
  motion_vector_output[pixel] = -current_minus_previous;
}
