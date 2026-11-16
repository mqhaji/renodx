#ifndef RENODX_GAMES_MGSV_RENDERING_MOTION_HLSLI_
#define RENODX_GAMES_MGSV_RENDERING_MOTION_HLSLI_

// Native BA stores current-minus-previous motion in units of 64 input pixels.
// This is data, not color: neither decoding nor reprojection uses a gamma curve.
float2 DecodeNativeMotion(float2 encoded, float2 reciprocal_size) {
  return (encoded * 2.0 - 1.0) * 64.0 * reciprocal_size;
}

bool GetCameraVelocity(float2 uv, float depth, float2 jitter, float4x4 current_to_previous, out float2 velocity) {
  const float2 current_ndc = uv * float2(2.0, -2.0) + float2(-1.0, 1.0) - jitter * float2(2.0, -2.0);
  const float4 previous_clip = mul(current_to_previous, float4(current_ndc, depth, 1.0));
  velocity = 0.0;
  if (abs(previous_clip.w) <= 1e-8) return false;
  const float2 previous_uv = previous_clip.xy / previous_clip.w * float2(0.5, -0.5) + 0.5;
  const float2 current_uv = current_ndc * float2(0.5, -0.5) + 0.5;
  velocity = current_uv - previous_uv;
  return all(isfinite(velocity));
}
#endif
