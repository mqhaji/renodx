Texture2D<float4> encoded_color_texture : register(t0);
Texture2D<float4> native_velocity_texture : register(t1);
Texture2D<float> depth_texture : register(t2);
Texture2D<float4> object_velocity_texture : register(t3);

RWTexture2D<float4> linear_color_output : register(u0);
RWTexture2D<float2> motion_vector_output : register(u1);

cbuffer MGSV_PREPARE_CONSTANTS : register(b0) {
  float2 current_jitter_uv;
  float velocity_projection_jitter_scale;
  float camera_reprojection_valid;
  uint2 render_size;
  float2 reciprocal_render_size;
  float4 current_to_previous_clip_row_0;
  float4 current_to_previous_clip_row_1;
  float4 current_to_previous_clip_row_2;
  float4 current_to_previous_clip_row_3;
};

float2 DecodeNativeVelocity(uint2 pixel) {
  const float2 encoded = native_velocity_texture.Load(int3(pixel, 0)).ba;
  return (encoded * 2.f - 1.f) * 64.f * reciprocal_render_size;
}

bool ComputeMatrixCameraVelocity(float2 uv, float depth, out float2 velocity) {
  const float2 current_ndc =
      uv * float2(2.f, -2.f) + float2(-1.f, 1.f)
      - float2(2.f * current_jitter_uv.x, -2.f * current_jitter_uv.y);
  const float4 current_clip = float4(current_ndc, depth, 1.f);
  const float4 previous_clip = float4(
      dot(current_to_previous_clip_row_0, current_clip),
      dot(current_to_previous_clip_row_1, current_clip),
      dot(current_to_previous_clip_row_2, current_clip),
      dot(current_to_previous_clip_row_3, current_clip));
  if (abs(previous_clip.w) <= 1e-8f) {
    velocity = 0.f.xx;
    return false;
  }

  const float2 current_no_jitter_uv = current_ndc * float2(0.5f, -0.5f) + 0.5f.xx;
  const float2 previous_uv = previous_clip.xy / previous_clip.w * float2(0.5f, -0.5f) + 0.5f.xx;
  velocity = current_no_jitter_uv - previous_uv;
  return all(isfinite(velocity));
}