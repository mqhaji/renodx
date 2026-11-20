#ifndef RENODX_GAMES_MGSV_RENDERING_MOTION_HLSLI_
#define RENODX_GAMES_MGSV_RENDERING_MOTION_HLSLI_

// The engine's two motion images, object motion and the combined image, store
// motion (current minus previous position) in B and A. The game's form maps
// -64..64 render pixels to 0..1, which in the 16-bit float targets rounds small
// motion to 1/32 or 1/16 of a pixel. While temporal reconstruction is on, the
// mod's ports of the velocity shaders write precise form instead: signed render
// pixels (y down), unrounded and without the 64-pixel limit, marked with 1 in G.
// The game's shaders, its 8-bit motion blur tiles and its clears leave G at 0,
// so each pixel says which form it holds. In the object image R is the
// moving-object mask in both forms. Motion is data, not color, so no gamma
// curve applies.
static const float PRECISE_MOTION = 1.0;

bool IsPreciseMotion(float4 motion) {
  return motion.g > 0.5;
}

// Motion in render pixels (y down), from either form.
float2 MotionPixels(float4 motion) {
  return IsPreciseMotion(motion) ? motion.ba : (motion.ba * 2.0 - 1.0) * 64.0;
}

// Motion as the game's motion blur passes use it: in units of 64 render pixels
// and at most 1 long, which is what the game's form holds.
float2 MotionBlurVelocity(float4 motion) {
  if (!IsPreciseMotion(motion)) return motion.ba * 2.0 - 1.0;
  const float2 scaled = motion.ba / 64.0;
  const float scaled_length = length(scaled);
  return (scaled_length > 1.0) ? scaled / scaled_length : scaled;
}

// Motion in render pixels made safe to store in precise form. Motion that is
// not finite, which a previous position at or behind the camera plane can
// cause, becomes no motion; the exponent test does not depend on the compiler
// keeping comparisons with NaN. Motion longer than twice the larger render
// dimension only says the point was off screen. Bounding it there keeps its
// direction, keeps it far inside the 16-bit float range of the image, and keeps
// later arithmetic from overflowing. The direction comes from the largest
// component first, because the length of a huge finite value can itself
// overflow.
float2 BoundPreciseMotion(float2 pixels, float2 render_size) {
  if (any((asuint(pixels) & 0x7F800000u) == 0x7F800000u)) return 0.0;
  const float max_pixels = 2.0 * max(render_size.x, render_size.y);
  const float largest = max(abs(pixels.x), abs(pixels.y));
  if (largest <= max_pixels) return pixels;
  const float2 direction = pixels / largest;
  return direction * (max_pixels / length(direction));
}

// For the G-buffer velocity ports: the object motion pixel in precise form.
// positions holds the current and previous clip-space x and y, and w their w.
float4 ComputePreciseObjectMotion(float4 positions, float2 w, float2 render_size) {
  const float4 screen = positions / w.xxyy;
  const float2 delta = screen.xy - screen.zw;
  return float4(1.0, PRECISE_MOTION, BoundPreciseMotion(float2(delta.x, -delta.y) * render_size * 0.5, render_size));
}

// Camera-only motion for one pixel: remove the current jitter, then reproject its
// depth with the current-to-previous clip matrix. Returns false if unusable.
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
