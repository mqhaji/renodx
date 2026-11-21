// SPDX-License-Identifier: MIT
// Copyright and license notices: ../README.md#credits-and-license.

// GT-VBGI source radiance with the previous frame's indirect light, run at
// full render resolution before the trace, so each frame's gather adds one
// more bounce.
//
// The game's diffuse light holds direct light and its own unoccluded ambient
// light. The application pass (gtvbgi_add) records, per pixel, how much it
// changed that light (AO on ambient, plus bounce light), with the pixel's view
// depth. Here that change is carried to this frame's pixels through the
// camera's motion, or the object's where the engine moved it, and added to
// the source as material color times light. Taps whose recorded depth does
// not match where the surface was are left out, so newly revealed surfaces
// and the edges of moving objects take one bounce until the next frame.
//
// The recorded change is in the previous frame's pre-exposure, so while the
// exposure adapts, the light beyond the first bounce lags it by a frame. The
// host runs this pass only when that record can be used.

#include "../../../rendering/geometry.hlsli"
#include "../../../rendering/motion.hlsli"
#include "../../../shared.h"
#include "../source_radiance.hlsli"

Texture2D<float4> depth_buffer : register(t0);
Texture2D<float4> normal_buffer : register(t1);
Texture2D<float4> source_diffuse : register(t3);
Texture2D<float4> source_albedo : register(t4);
Texture2D<float4> previous_change : register(t5);
Texture2D<float4> native_material : register(t7);
Texture2D<float4> object_velocity_buffer : register(t9);
Texture2D<float4> clean_material : register(t11);
Texture2D<uint4> material_owner : register(t12);
Texture2D<float4> prepared_source : register(t13);
RWTexture2D<float4> source_output : register(u0);

cbuffer BounceHistory : register(b1) {
  row_major float4x4 current_to_previous_clip;
  float2 current_jitter_uv;
  float2 previous_jitter_uv;
  float velocity_projection_jitter_scale;
  uint3 bounce_padding;
}

// A tap counts when its recorded view depth is within this share of the
// depth the surface had then, plus how much the surface's depth changes
// across one pixel, since taps lie up to a pixel from that point. Object
// motion gives no previous depth, so it is compared with the current depth,
// more loosely.
static const float CAMERA_DEPTH_TOLERANCE = 0.05;
static const float OBJECT_DEPTH_TOLERANCE = 0.15;

[numthreads(8, 8, 1)] void main(uint3 thread_id : SV_DispatchThreadID) {
  const int2 pixel = int2(thread_id.xy);
  if (pixel.x >= int(buffer_width) || pixel.y >= int(buffer_height)) return;
  const uint2 size = uint2(buffer_width, buffer_height);
  const float depth = depth_buffer.Load(int3(pixel, 0)).x;
  const float4 normal = normal_buffer.Load(int3(pixel, 0));
  uint prepared_width, prepared_height;
  prepared_source.GetDimensions(prepared_width, prepared_height);
  float4 source = (prepared_width == buffer_width && prepared_height == buffer_height
                       ? prepared_source.Load(int3(pixel, 0))
                       : LoadSourceRadiance(source_diffuse, source_albedo, native_material, clean_material,
                                            material_owner, pixel, size, normal, depth, true));
  const float z = LinearZ(depth);
  if (!(depth > 0.0 && z > 0.0 && z < 1.0e6)) {
    source_output[pixel] = source;
    return;
  }
  // The depth change across one pixel in each direction, taken on the side
  // that continues the surface so an edge does not widen the test. The cap
  // applies where neither side continues it, as on a one-pixel-wide object.
  float slope = 0.0;
  [unroll] for (uint axis = 0u; axis < 2u; ++axis) {
    const int2 along = int2(axis == 0u, axis == 1u);
    const float before = LinearZ(depth_buffer.Load(int3(clamp(pixel - along, 0, int2(size) - 1), 0)).x);
    const float after = LinearZ(depth_buffer.Load(int3(clamp(pixel + along, 0, int2(size) - 1), 0)).x);
    slope += min(abs(after - z), abs(z - before));
  }
  slope = min(slope, 0.5 * z);

  // 1. Where this surface was in the previous frame, as an unjittered
  // position, and the view depth it had there.
  const float2 uv = (float2(pixel) + 0.5) / float2(size);
  const float2 current_ndc = uv * float2(2.0, -2.0) + float2(-1.0, 1.0) - current_jitter_uv * float2(2.0, -2.0);
  float2 previous_uv;
  float expected_depth;
  float tolerance;
  const float4 object_motion = object_velocity_buffer.Load(int3(pixel, 0));
  if (object_motion.r > 0.5) {
    const float2 velocity = MotionPixels(object_motion) / float2(size)
                            - current_jitter_uv * velocity_projection_jitter_scale;
    previous_uv = (uv - current_jitter_uv) - velocity;
    expected_depth = z;
    tolerance = OBJECT_DEPTH_TOLERANCE * z + slope;
  } else {
    const float4 previous_clip = mul(current_to_previous_clip, float4(current_ndc, depth, 1.0));
    if (!(previous_clip.w > 1e-6)) {
      source_output[pixel] = source;
      return;
    }
    previous_uv = previous_clip.xy / previous_clip.w * float2(0.5, -0.5) + 0.5;
    // The point went in with w = 1 rather than its view depth, so w comes out
    // as the ratio of the two frames' view depths.
    expected_depth = z * previous_clip.w;
    tolerance = (CAMERA_DEPTH_TOLERANCE * z + slope) * previous_clip.w;
  }

  // 2. The previous frame's pixels sampled with that frame's jitter.
  const float2 position = (previous_uv + previous_jitter_uv) * float2(size) - 0.5;
  const int2 base = int2(floor(position));
  const float2 fraction = position - float2(base);
  float3 change = 0.0;
  float accepted = 0.0;
  [unroll] for (uint tap = 0u; tap < 4u; ++tap) {
    const int2 offset = int2(tap & 1u, tap >> 1u);
    const int2 at = base + offset;
    if (any(at < 0) || any(at >= int2(size))) continue;
    const float4 recorded = previous_change.Load(int3(at, 0));
    if (!(recorded.a > 0.0) || abs(recorded.a - expected_depth) > tolerance) continue;
    const float2 bilinear = lerp(1.0 - fraction, fraction, float2(offset));
    change += bilinear.x * bilinear.y * recorded.rgb;
    accepted += bilinear.x * bilinear.y;
  }
  // The output is RGBA16F, so the sum is kept within its range.
  if (accepted > 0.25 && all(isfinite(change))) {
    const float4 color = LoadSourceColor(source_albedo, native_material, clean_material, material_owner, pixel, size,
                                         normal, depth, true);
    source.rgb = clamp(source.rgb + color.rgb * change / accepted, 0.0, 65504.0);
  }
  source_output[pixel] = source;
}
