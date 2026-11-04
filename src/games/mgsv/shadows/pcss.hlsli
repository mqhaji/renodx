// SPDX-License-Identifier: MIT
#ifndef SRC_GAMES_MGSV_SHADOWS_PCSS_HLSLI_
#define SRC_GAMES_MGSV_SHADOWS_PCSS_HLSLI_

// Shared PCSS sampling, diagnostics, and light-specific depth conversions.
struct PcssResult {
  float visibility;
  float blocker_gap;
  float radius;  // Soft edge radius in game units, before projection to texture UV.
  uint region;   // Sun cascade or point-light hemisphere, for diagnostics.
  bool valid;
};

// Golden-angle directions form a disk when combined with the radial spacing below.
// Precomputing them avoids trigonometry for every sample.
static const float2 PCSS_DIRECTIONS[32] = {
    float2(1.00000000, 0.00000000),
    float2(-0.73736888, 0.67549029),
    float2(0.08742572, -0.99617104),
    float2(0.60843886, 0.79360075),
    float2(-0.98471349, -0.17418195),
    float2(0.84375529, -0.53672805),
    float2(-0.25960430, 0.96571507),
    float2(-0.46090702, -0.88744843),
    float2(0.93932130, 0.34303863),
    float2(-0.92434556, 0.38155641),
    float2(0.42384600, -0.90573427),
    float2(0.29928386, 0.95416412),
    float2(-0.86521121, -0.50140758),
    float2(0.97667577, -0.21471943),
    float2(-0.57512943, 0.81806243),
    float2(-0.12851069, -0.99170812),
    float2(0.76464900, 0.64444698),
    float2(-0.99914605, 0.04131783),
    float2(0.70882941, -0.70537994),
    float2(-0.04619145, 0.99893261),
    float2(-0.64070914, -0.76778369),
    float2(0.99106941, 0.13334699),
    float2(-0.82085834, 0.57113185),
    float2(0.21948137, -0.97561669),
    float2(0.49718087, 0.86764692),
    float2(-0.95269278, -0.30393498),
    float2(0.90779113, -0.41942253),
    float2(-0.38606108, 0.92247322),
    float2(-0.33845228, -0.94098356),
    float2(0.88518944, 0.46523076),
    float2(-0.96697001, 0.25489019),
    float2(0.54083774, -0.84112695)};

Texture2DArray<float2> shadow_noise : register(t20);

float2 PcssDisk(uint index, uint count, float2 rotation, float radial_jitter) {
  float2 direction = PCSS_DIRECTIONS[index];
  // Each sample gets a radial band. Square-root spacing spreads them over disk area.
  return float2(direction.x * rotation.x - direction.y * rotation.y,
                direction.x * rotation.y + direction.y * rotation.x)
         * sqrt((float(index) + radial_jitter) / float(count));
}

float2 PcssNoise(float2 pixel) {
  // RG chooses disk rotation and radial offset. The 32-layer sequence repeats;
  // this sample jitter is independent of camera-projection jitter.
  return shadow_noise.Load(int4(uint2(pixel) & 127u, uint(CUSTOM_SHADOW_NOISE_FRAME) & 31u, 0));
}

float2 PcssDepthGradient(float2 uv_dx, float2 uv_dy, float depth_dx, float depth_dy) {
  float determinant = uv_dx.x * uv_dy.y - uv_dx.y * uv_dy.x;
  if (abs(determinant) <= 1e-20) return 0.0;
  return float2(depth_dx * uv_dy.y - depth_dy * uv_dx.y,
                depth_dy * uv_dx.x - depth_dx * uv_dy.x)
         / determinant;
}

float PcssReceiverBias(float2 depth_gradient, float2 texel) {
  // Two D24 depth steps plus the half-texel slope footprint keep the receiver
  // from shadowing itself. Positive bias moves reverse depth toward the light.
  return 2.0 / 16777215.0 + 0.5 * dot(abs(depth_gradient), texel);
}

float3 PcssDebug(PcssResult value, uint view, float max_radius) {
  if (!value.valid) return float3(1.0, 0.0, 1.0);
  switch (view) {
    case 1u: return value.blocker_gap / (1.0 + value.blocker_gap);
    case 2u: return value.radius / max(max_radius, 1e-6);
    case 3u: return value.visibility;
    case 4u: {
      const float3 colors[4] = {float3(1.0, 0.2, 0.2), float3(0.2, 1.0, 0.2),
                                float3(0.2, 0.4, 1.0), float3(1.0, 0.8, 0.2)};
      return colors[value.region];
    }
  }
  return 0.0;
}

PcssResult FilterSunShadow(Texture2D<float4> shadow_map, SamplerComparisonState comparison,
                           float3 position, float3 position_dx, float3 position_dy,
                           float2 native_uv, float native_visibility,
                           float4x4 projection, float4x4 cascades, float2 noise) {
  PcssResult result = (PcssResult)0;
  result.visibility = native_visibility;
  result.region = min(uint(native_uv.x * 2.0), 1u) + 2u * min(uint(native_uv.y * 2.0), 1u);

  uint width, height;
  shadow_map.GetDimensions(width, height);
  float2 texel = rcp(float2(width, height));
  float3 projected = mul(float4(position, 1.0), projection).xyz;
  float3 projected_dx = mul(float4(position_dx, 0.0), projection).xyz;
  float3 projected_dy = mul(float4(position_dy, 0.0), projection).xyz;
  float2 base_scale = 0.25 * float2(length(projection._m00_m10_m20), length(projection._m01_m11_m21));
  float depth_scale = length(projection._m02_m12_m22) * abs(cascades._m30);
  float max_radius = max(CUSTOM_SUN_PCSS_MAX_RADIUS, 0.0);
  if (width < 4u || height < 4u || min(base_scale.x, base_scale.y) <= 0.0 || depth_scale <= 0.0) return result;

  float4 cascade_transform = float4(0.0, 0.0, 0.0, 1.0);
  float3 clip = projected;
  float2 uv_scale = base_scale;
  float2 slack = 0.0;
  // Use the next cascade only when the bounded search would leave this tile.
  [loop]
  for (uint cascade = result.region; cascade < 4u; ++cascade) {
    cascade_transform = cascade == 0u ? float4(0.0, 0.0, 0.0, 1.0) : cascades[cascade - 1u];
    clip = projected * cascade_transform.w + cascade_transform.xyz;
    uv_scale = base_scale * abs(cascade_transform.w);
    slack = 0.25 - abs(clip.xy) * 0.25 - texel;
    result.region = cascade;
    if (all(slack >= max_radius * uv_scale)) break;
  }

  float receiver_depth = (1.0 + clip.z) * cascades._m30;
  float depth_per_unit = depth_scale * abs(cascade_transform.w);
  if (any(slack < 0.0) || receiver_depth < 0.0 || receiver_depth > 1.0 || depth_per_unit <= 0.0) return result;
  max_radius = min(max_radius, min(slack.x / uv_scale.x, slack.y / uv_scale.y));
  float2 tile = float2(result.region & 1u, result.region >> 1u) * 0.5;
  float2 uv = tile + 0.25 + clip.xy * float2(0.25, -0.25) + 0.5 * texel;
  float2 tile_min = tile + 0.5 * texel;
  float2 tile_max = tile + 0.5 - 0.5 * texel;

  // Project screen derivatives through this one cascade. Differentiating the
  // selected atlas UV would mistake cascade jumps for a steep receiver.
  float3 dx = projected_dx * cascade_transform.w;
  float3 dy = projected_dy * cascade_transform.w;
  float2 uv_dx = dx.xy * float2(0.25, -0.25);
  float2 uv_dy = dy.xy * float2(0.25, -0.25);
  float2 depth_gradient = PcssDepthGradient(uv_dx, uv_dy, dx.z * cascades._m30, dy.z * cascades._m30);
  float bias = PcssReceiverBias(depth_gradient, texel);
  float2 rotation;
  sincos(noise.x * 6.28318530718, rotation.y, rotation.x);
  float radial_jitter = clamp(noise.y, 0.001, 0.999);
  uint blocker_samples = uint(clamp(CUSTOM_SUN_PCSS_BLOCKER_SAMPLES, 1.0, 32.0));
  uint filter_samples = uint(clamp(CUSTOM_SUN_PCSS_FILTER_SAMPLES, 1.0, 32.0));
  float gap_sum = 0.0;
  float closest_blocker_gap = 1.0;
  uint blockers = 0u;
  result.valid = true;
  [loop]
  for (uint sample_index = 0u; sample_index < blocker_samples; ++sample_index) {
    float2 offset = PcssDisk(sample_index, blocker_samples, rotation, radial_jitter) * max_radius * uv_scale;
    float2 sample_uv = clamp(uv + offset, tile_min, tile_max);
    int2 pixel = int2(sample_uv * float2(width, height));
    float2 center = (float2(pixel) + 0.5) * texel;
    float reference = receiver_depth + dot(depth_gradient, center - uv) + bias;
    float blocker_depth = shadow_map.Load(int3(pixel, 0)).x;
    float depth_gap = blocker_depth - receiver_depth;
    if (blocker_depth > reference && depth_gap > 0.0) {
      // Slope correction identifies self-shadowing; it must not move the actual
      // receiver when measuring how far away a blocker is.
      gap_sum += depth_gap;
      closest_blocker_gap = min(closest_blocker_gap, depth_gap);
      ++blockers;
    }
  }
  // Sparse searches can miss thin casters. Keep the native filter on a miss.
  if (blockers == 0u) return result;
  result.blocker_gap = gap_sum / (float(blockers) * depth_per_unit);
  result.radius = min(max_radius, result.blocker_gap * tan(max(CUSTOM_SUN_PCSS_ANGLE, 0.0)));
  float2 uv_radius = result.radius * uv_scale;
  if (max(uv_radius.x / texel.x, uv_radius.y / texel.y) <= 0.5) return result;
  // A local plane is unreliable if extending it across the filter reaches a
  // blocker. Retain native filtering instead of erasing that shadow with bias.
  if (dot(abs(depth_gradient), uv_radius) + bias >= closest_blocker_gap) return result;

  float occlusion = 0.0;
  [loop]
  for (uint filter_index = 0u; filter_index < filter_samples; ++filter_index) {
    float2 offset = PcssDisk(filter_index, filter_samples, rotation, radial_jitter) * uv_radius;
    float2 sample_uv = clamp(uv + offset, tile_min, tile_max);
    float reference = receiver_depth + dot(depth_gradient, sample_uv - uv) + bias;
    occlusion += shadow_map.SampleCmpLevelZero(comparison, sample_uv, reference).x;
  }
  result.visibility = saturate(1.0 - occlusion / float(filter_samples));
  return result;
}

PcssResult FilterSpotShadow(Texture2D<float4> shadow_map, SamplerComparisonState comparison,
                            float4 projected, float4 projected_dx, float4 projected_dy,
                            float4x4 projection, float native_visibility, float2 noise) {
  PcssResult result = (PcssResult)0;
  result.visibility = native_visibility;
  uint width, height;
  shadow_map.GetDimensions(width, height);
  float3 w_axis = projection._m03_m13_m23;
  float w_scale_squared = dot(w_axis, w_axis);
  if (width < 2u || height < 2u || projected.w <= 0.0 || w_scale_squared <= 0.0) return result;
  float w_scale = sqrt(w_scale_squared);
  // For a rotated, translated perspective matrix, depth = a + b / w.
  // Reading its diagonal would mix camera rotation and translation into depth.
  float a = dot(projection._m02_m12_m22, w_axis) / w_scale_squared;
  float b = projection._m32 - a * projection._m33;
  if (b <= 0.0) return result;
  float3 coordinate = projected.xyz / projected.w;
  if (any(coordinate.xy < 0.0) || any(coordinate.xy > 1.0) || coordinate.z < 0.0 || coordinate.z > 1.0) return result;
  float receiver_distance = projected.w / w_scale;
  float3 u_axis = projection._m00_m10_m20;
  float3 v_axis = projection._m01_m11_m21;
  float2 focal = float2(length(u_axis - w_axis * (dot(u_axis, w_axis) / w_scale_squared)),
                        length(v_axis - w_axis * (dot(v_axis, w_axis) / w_scale_squared)))
                 / w_scale;
  float2 texel = rcp(float2(width, height));
  float3 dx = (projected_dx.xyz - coordinate * projected_dx.w) / projected.w;
  float3 dy = (projected_dy.xyz - coordinate * projected_dy.w) / projected.w;
  float2 gradient = PcssDepthGradient(dx.xy, dy.xy, dx.z, dy.z);
  float bias = PcssReceiverBias(gradient, texel);
  float2 rotation;
  sincos(noise.x * 6.28318530718, rotation.y, rotation.x);
  float radial_jitter = clamp(noise.y, 0.001, 0.999);
  float max_radius = max(CUSTOM_SPOT_PCSS_MAX_RADIUS, 0.0);
  float light_radius = max(CUSTOM_SPOT_PCSS_LIGHT_RADIUS, 0.0);
  uint blocker_samples = uint(clamp(CUSTOM_SPOT_PCSS_BLOCKER_SAMPLES, 1.0, 32.0));
  uint filter_samples = uint(clamp(CUSTOM_SPOT_PCSS_FILTER_SAMPLES, 1.0, 32.0));
  float2 uv_per_unit = focal / receiver_distance;
  float blocker_sum = 0.0;
  uint blockers = 0u;
  result.valid = true;
  if (light_radius == 0.0 || max_radius == 0.0) return result;
  [loop]
  for (uint blocker_index = 0u; blocker_index < blocker_samples; ++blocker_index) {
    float2 offset = PcssDisk(blocker_index, blocker_samples, rotation, radial_jitter) * max_radius * uv_per_unit;
    float2 uv = clamp(coordinate.xy + offset, 0.5 * texel, 1.0 - 0.5 * texel);
    int2 pixel = int2(uv * float2(width, height));
    float2 center = (float2(pixel) + 0.5) * texel;
    float reference = coordinate.z + dot(gradient, center - coordinate.xy) + bias;
    float depth = shadow_map.Load(int3(pixel, 0)).x;
    if (depth > reference && depth > a) {
      blocker_sum += b / ((depth - a) * w_scale);
      ++blockers;
    }
  }
  if (blockers == 0u) return result;
  float blocker_distance = blocker_sum / float(blockers);
  result.blocker_gap = max(receiver_distance - blocker_distance, 0.0);
  result.radius = min(max_radius, light_radius * result.blocker_gap / max(blocker_distance, 1e-6));
  float2 uv_radius = result.radius * uv_per_unit;
  if (max(uv_radius.x / texel.x, uv_radius.y / texel.y) <= 0.5) return result;
  float occlusion = 0.0;
  [loop]
  for (uint filter_index = 0u; filter_index < filter_samples; ++filter_index) {
    float2 uv = clamp(coordinate.xy + PcssDisk(filter_index, filter_samples, rotation, radial_jitter) * uv_radius,
                      0.5 * texel, 1.0 - 0.5 * texel);
    float reference = coordinate.z + dot(gradient, uv - coordinate.xy) + bias;
    occlusion += shadow_map.SampleCmpLevelZero(comparison, uv, reference).x;
  }
  result.visibility = saturate(1.0 - occlusion / float(filter_samples));
  return result;
}

// Match the native hemisphere shrink and half-texel offset. Projecting each
// light-space sample selects the correct half even when it crosses the equator.
float2 ProjectPointShadow(float3 ray, float2 size) {
  float hemisphere = ray.z > 0.0 ? 1.0 : -1.0;
  float2 disk = ray.xy / (length(ray) + abs(ray.z));
  float2 shrink = 1.0 - float2(4.0, 2.0) / size;
  return float2(0.5 + 0.25 * (disk.x * shrink.x + hemisphere),
                0.5 - 0.5 * disk.y * shrink.y)
         - 0.5 / size;
}

float3 UnprojectPointShadow(float2 uv, float2 size) {
  float hemisphere = uv.x + 0.5 / size.x < 0.5 ? -1.0 : 1.0;
  float2 disk = (uv + 0.5 / size - float2(0.5 + 0.25 * hemisphere, 0.5))
                / (float2(0.25, -0.5) * (1.0 - float2(4.0, 2.0) / size));
  float radius_squared = dot(disk, disk);
  return float3(2.0 * disk, hemisphere * (1.0 - radius_squared)) / (1.0 + radius_squared);
}

PcssResult FilterPointShadow(Texture2D<float4> shadow_map, SamplerComparisonState comparison,
                             float3 position, float3 position_dx, float3 position_dy,
                             float inverse_range, float native_visibility, float2 noise) {
  PcssResult result = (PcssResult)0;
  result.visibility = native_visibility;
  result.region = position.z > 0.0 ? 1u : 0u;
  uint width, height;
  shadow_map.GetDimensions(width, height);
  float receiver_distance = length(position);
  if (width < 8u || height < 4u || inverse_range <= 0.0 || receiver_distance <= 1e-6) return result;
  float2 size = float2(width, height);
  float2 texel = rcp(size);
  float3 direction = position / receiver_distance;
  float3 tangent = normalize(abs(direction.z) < 0.99 ? cross(direction, float3(0.0, 0.0, 1.0))
                                                     : cross(direction, float3(0.0, 1.0, 0.0)));
  float3 bitangent = cross(direction, tangent);
  float3 plane_normal = cross(position_dx, position_dy);
  float plane_distance = dot(plane_normal, position);
  if (abs(plane_distance) <= 1e-20) return result;
  float denominator = receiver_distance + abs(position.z);
  float2 disk = position.xy / denominator;
  float hemisphere = position.z > 0.0 ? 1.0 : -1.0;
  float radius_dx = dot(direction, position_dx);
  float radius_dy = dot(direction, position_dy);
  float2 uv_scale = float2(0.25, -0.5) * (1.0 - float2(4.0, 2.0) / size);
  float2 uv_dx = uv_scale * (position_dx.xy - disk * (radius_dx + hemisphere * position_dx.z)) / denominator;
  float2 uv_dy = uv_scale * (position_dy.xy - disk * (radius_dy + hemisphere * position_dy.z)) / denominator;
  float2 gradient = PcssDepthGradient(uv_dx, uv_dy, -radius_dx * inverse_range, -radius_dy * inverse_range);
  float bias = PcssReceiverBias(gradient, texel);
  float2 rotation;
  sincos(noise.x * 6.28318530718, rotation.y, rotation.x);
  float radial_jitter = clamp(noise.y, 0.001, 0.999);
  float max_radius = max(CUSTOM_POINT_PCSS_MAX_RADIUS, 0.0);
  float light_radius = max(CUSTOM_POINT_PCSS_LIGHT_RADIUS, 0.0);
  uint blocker_samples = uint(clamp(CUSTOM_POINT_PCSS_BLOCKER_SAMPLES, 1.0, 32.0));
  uint filter_samples = uint(clamp(CUSTOM_POINT_PCSS_FILTER_SAMPLES, 1.0, 32.0));
  float blocker_sum = 0.0;
  uint blockers = 0u;
  result.valid = true;
  if (light_radius == 0.0 || max_radius == 0.0) return result;
  // Rotate the tangent frame once rather than each disk point in both loops.
  float3 rotated_tangent = tangent * rotation.x + bitangent * rotation.y;
  bitangent = bitangent * rotation.x - tangent * rotation.y;
  tangent = rotated_tangent;
  // Bound the angular footprint before projecting it. This also avoids a
  // search spanning most of the sphere when a receiver is inside the light.
  float search_spread = min(max_radius / receiver_distance, 0.5);
  [loop]
  for (uint blocker_index = 0u; blocker_index < blocker_samples; ++blocker_index) {
    float2 offset = PcssDisk(blocker_index, blocker_samples, float2(1.0, 0.0), radial_jitter) * search_spread;
    float3 ray = direction + tangent * offset.x + bitangent * offset.y;
    float2 uv = clamp(ProjectPointShadow(ray, size), 0.5 * texel, 1.0 - 0.5 * texel);
    int2 pixel = int2(uv * size);
    float3 sample_direction = UnprojectPointShadow((float2(pixel) + 0.5) * texel, size);
    float plane_dot = dot(plane_normal, sample_direction);
    if (abs(plane_dot) <= 1e-20) continue;
    float sample_distance = plane_distance / plane_dot;
    if (sample_distance <= 0.0) continue;
    float depth = shadow_map.Load(int3(pixel, 0)).x;
    if (depth > 1.0 - sample_distance * inverse_range + bias) {
      blocker_sum += (1.0 - depth) / inverse_range;
      ++blockers;
    }
  }
  if (blockers == 0u) return result;
  float blocker_distance = blocker_sum / float(blockers);
  result.blocker_gap = max(receiver_distance - blocker_distance, 0.0);
  result.radius = min(max_radius, light_radius * result.blocker_gap / max(blocker_distance, 1e-6));
  float filter_spread = min(result.radius / receiver_distance, search_spread);
  // The native four taps cover smaller footprints more reliably.
  if (filter_spread * min(float(width) * 0.25, float(height) * 0.5) <= 0.5) return result;
  float occlusion = 0.0;
  uint samples = 0u;
  [loop]
  for (uint filter_index = 0u; filter_index < filter_samples; ++filter_index) {
    float2 offset = PcssDisk(filter_index, filter_samples, float2(1.0, 0.0), radial_jitter) * filter_spread;
    float3 ray = direction + tangent * offset.x + bitangent * offset.y;
    float plane_dot = dot(plane_normal, ray);
    if (abs(plane_dot) <= 1e-20) continue;
    float sample_distance = plane_distance * length(ray) / plane_dot;
    if (sample_distance <= 0.0) continue;
    float2 uv = clamp(ProjectPointShadow(ray, size), 0.5 * texel, 1.0 - 0.5 * texel);
    occlusion += shadow_map.SampleCmpLevelZero(comparison, uv, 1.0 - sample_distance * inverse_range + bias).x;
    ++samples;
  }
  if (samples > 0u) result.visibility = saturate(1.0 - occlusion / float(samples));
  return result;
}

#endif  // SRC_GAMES_MGSV_SHADOWS_PCSS_HLSLI_
