// Perspective-correct traversal of camera-visible surfaces. A nearest-depth
// block can be skipped only while the whole ray segment is in front of it.
#ifndef MGSV_SPECULAR_TRACE_HLSLI_
#define MGSV_SPECULAR_TRACE_HLSLI_

static const uint SPECULAR_RAY_STEPS = 128u;

struct SpecularRayHit {
  bool hit;
  int2 pixel;
};

SpecularRayHit TraceSpecularRay(Texture2D<float4> depth, Texture2D<float4> mips,
                                float3 origin, float3 direction, int2 receiver) {
  SpecularRayHit result = (SpecularRayHit)0;
  const float near_z = projection_parameters.z / max(1.0 - projection_parameters.w, 1e-6);
  float distance = min(LinearZ(0.0), 1.0e6);
  if (direction.z < -1e-6) distance = min(distance, (near_z - origin.z) / direction.z);
  if (!(distance > 0.0) || !all(isfinite(direction))) return result;
  const float3 endpoint = origin + direction * distance;
  if (!(endpoint.z > 0.0)) return result;
  const float2 size = float2(buffer_width, buffer_height);
  const float2 start = ((origin.xy / (projection_parameters.xy * origin.z) + camera_center_offset)
                            * float2(1.0, -1.0)
                        + 1.0)
                       * 0.5 * size;
  const float2 end = ((endpoint.xy / (projection_parameters.xy * endpoint.z) + camera_center_offset)
                          * float2(1.0, -1.0)
                      + 1.0)
                     * 0.5 * size;
  const float2 delta = end - start;
  const float length_pixels = length(delta);
  if (!(length_pixels > 1e-4) || !all(isfinite(delta))) return result;
  // Keep the projected ray's reciprocal direction for every visited cell.
  // Parallel axes never select a boundary, so they use a harmless zero here.
  const bool2 moving = abs(delta) >= 1e-8;
  const float2 inverse_delta = moving ? rcp(delta) : 0.0;

  // Clip before traversal. Reciprocal view depth is affine along the projected
  // segment; interpolating view depth itself would put reflections out of place.
  float first = 0.0;
  float last = min(1.0, max(CUSTOM_MGSV_GT_SEARCH_RADIUS_PIXELS, 1.0) / length_pixels);
  [unroll]
  for (uint axis = 0u; axis < 2u; ++axis) {
    if (!moving[axis]) {
      if (start[axis] < 0.0 || start[axis] >= size[axis]) return result;
    } else {
      const float a = -start[axis] * inverse_delta[axis];
      const float b = (size[axis] - start[axis]) * inverse_delta[axis];
      first = max(first, min(a, b));
      last = min(last, max(a, b));
    }
  }
  if (first >= last) return result;
  const float inverse_start = rcp(origin.z);
  const float inverse_end = rcp(endpoint.z);
  const float inverse_depth_scale = rcp(projection_parameters.z);
  const float epsilon = 1e-3 / length_pixels;
  float current = first + epsilon;
  const int maximum_mip = CUSTOM_MGSV_GT_DEPTH_MIPS_ENABLED > 0.5 ? 4 : 0;
  [loop]
  for (uint step = 0u; step < SPECULAR_RAY_STEPS && current < last; ++step) {
    const int2 pixel = clamp((int2)floor(start + delta * current), 0, int2(size) - 1);
    const float inverse_z0 = lerp(inverse_start, inverse_end, current);
    float next = current;
    [loop]
    for (int level = maximum_mip; level >= 0; --level) {
      const int block_size = 1 << level;
      const int2 mip_size = max(int2(size) >> level, 1);
      const int2 block = min(pixel >> level, mip_size - 1);
      const float2 lower = float2(block * block_size);
      // The last reduced texel also owns an odd dimension's trailing pixels.
      const float2 upper = float2(block.x + 1 == mip_size.x ? buffer_width : (block.x + 1) * block_size,
                                  block.y + 1 == mip_size.y ? buffer_height : (block.y + 1) * block_size);
      const float2 boundary = float2(delta.x > 0.0 ? upper.x : lower.x, delta.y > 0.0 ? upper.y : lower.y);
      const float2 crossing = moving ? (boundary - start) * inverse_delta : 1e30;
      next = min(last, max(min(crossing.x, crossing.y), current + epsilon));
      const float raw_depth = level == 0 ? depth.Load(int3(pixel, 0)).r : mips.Load(int3(block, level - 1)).r;
      if (!(raw_depth > 0.0)) break;
      // Positive view depth is monotonic in its reciprocal. Compare the coarse
      // cell in that space, avoiding three divisions at each hierarchy level.
      // The fine test converts the slab thickness to the same reciprocal units.
      const float surface_inverse_z = (raw_depth - projection_parameters.w) * inverse_depth_scale;
      const float inverse_z1 = lerp(inverse_start, inverse_end, next);
      if (min(inverse_z0, inverse_z1) > surface_inverse_z) break;
      if (level == 0 && max(inverse_z0, inverse_z1) * (1.0 + max(CUSTOM_MGSV_GT_SURFACE_THICKNESS, 0.0) * surface_inverse_z) >= surface_inverse_z
          && max(abs(pixel.x - receiver.x), abs(pixel.y - receiver.y)) > 1) {
        result.hit = true;
        result.pixel = pixel;
        return result;
      }
    }
    current = next + epsilon;
  }
  return result;
}

#endif  // MGSV_SPECULAR_TRACE_HLSLI_
