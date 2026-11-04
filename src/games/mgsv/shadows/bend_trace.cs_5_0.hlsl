// Copyright 2023 Sony Interactive Entertainment.
// SPDX-License-Identifier: Apache-2.0
// Adapted from Bend's shared-depth screen-space shadows. See BEND_LICENSE.md.

#include "bend_common.hlsli"

Texture2D<float4> bend_depth : register(t0);
SamplerState bend_border : register(s0);

groupshared float shared_depth[((BEND_MAX_SAMPLES + BEND_GROUP_SIZE - 1) / BEND_GROUP_SIZE + 1) * BEND_GROUP_SIZE];

[numthreads(BEND_GROUP_SIZE, 1, 1)]
void main(uint3 group : SV_GroupID, uint lane : SV_GroupIndex) {
  const uint4 metadata = bend_dispatch.Load4(32);
  if (metadata.z != 1) return;
  const bool directional = CUSTOM_SS_LIGHT_TYPE < 0.5;
  const bool spot = CUSTOM_SS_LIGHT_TYPE != 2.0;
  const bool receiver_culling = !directional && CUSTOM_SS_LOCAL_CULLING >= 0.5
                                && all(isfinite(bend_projection_param)) && bend_projection_param.z > 0.0
                                && all(isfinite(bend_camera_center.xy));
  float local_range = -1.0;
  if (receiver_culling) {
    const bool distance_spot = CUSTOM_SS_LIGHT_TYPE == 3.0;
    const float attenuation = distance_spot ? bend_light_params[6].z : bend_light_params[6].w;
    if (isfinite(attenuation) && attenuation > 0.0
        && (!distance_spot || attenuation >= 1.17549435e-38)) {
      local_range = (distance_spot ? rsqrt(sqrt(sqrt(sqrt(attenuation)))) : rsqrt(sqrt(attenuation)))
                    + (spot ? 0.0 : abs(bend_light_params[6].x));
      if (!isfinite(local_range)) {
        local_range = -1.0;
      }
    }
  }
  const int4 bounds = asint(bend_dispatch.Load4(16));
  const float4 light = asfloat(bend_dispatch.Load4(0));
  uint tile = group.y + (group.z << BEND_TILE_ROW_BITS);
  uint first_tile = 0;
  uint sector = 0;
  [loop]
  for (; sector < metadata.w; ++sector) {
    const uint last_tile = bend_dispatch.Load(BEND_DISPATCH_HEADER + sector * BEND_DISPATCH_STRIDE + 20);
    if (tile < last_tile) break;
    first_tile = last_tile;
  }
  if (sector == metadata.w) return;
  tile -= first_tile;
  const uint address = BEND_DISPATCH_HEADER + sector * BEND_DISPATCH_STRIDE;
  const uint columns = bend_dispatch.Load(address + 4);
  group.yz = uint2(tile % columns, tile / columns);
  const int2 wave_offset = asint(bend_dispatch.Load2(address + 12));
  int2 relative = int2(group.yz) * BEND_GROUP_SIZE + wave_offset;
  const int2 signs = sign(relative);
  const bool horizontal = abs(relative.x + signs.y) < abs(relative.y - signs.x);
  const int2 axis = horizontal ? int2(signs.y, 0) : int2(0, -signs.x);
  relative += axis * (int)group.x;
  const int2 light_grid = int2(floor(light.xy));
  const precise float2 light_fraction = frac(light.xy) - 0.5;
  const bool major_x = abs(relative.x) > abs(relative.y);
  const precise float major = major_x ? relative.x : relative.y;
  const precise float fraction = (major > 0.0 ? -1.0 : 1.0) * (major_x ? light_fraction.x : light_fraction.y);
  const precise float major_start = abs(major);
  const precise float start_distance = major_start + fraction;
  if (abs(start_distance) < 1e-7) return;
  // Cancel large light coordinates as integers before converting to pixels.
  // The equivalent lerp from the projected light loses pixels near the horizon.
  const precise float2 start = float2(relative + light_grid) + 0.5;
  const precise float2 pixel_step = (float2(relative) - light_fraction) / start_distance;
  if (local_range > 0.0) {
    // All receivers lie in this small bundle of camera rays. If the entire
    // light sphere is outside any side, this group cannot illuminate a pixel.
    // The decision is uniform and precedes all shared-memory/depth work.
    const float2 last = start - pixel_step * float(BEND_GROUP_SIZE - 1);
    if (BendSphereOutsideRays(bend_light_params[3].xyz, local_range,
                              (min(start, last) - 2.0),
                              (max(start, last) + 2.0), float2(metadata.xy))) return;
  }
  const precise float2 delta = pixel_step * BEND_GROUP_SIZE;
  const uint thread_step = lane ^ (light.w > 0.0 ? 0u : (BEND_GROUP_SIZE - 1u));
  precise float2 sample_pixel = start - pixel_step * float(thread_step);
  const int2 output_pixel = int2(floor(sample_pixel));
  const precise float pixel_distance = major_start - thread_step + fraction;
  const float direction = -light.w;
  const uint samples = (uint)clamp(isfinite(CUSTOM_SS_SHADOW_REACH) ? CUSTOM_SS_SHADOW_REACH : 60.0,
                                   16.0, float(BEND_MAX_SAMPLES));
  const float thickness = 0.005;
  // The last receiver reads through shared index 63 + samples.
  const uint reads = ((samples + BEND_GROUP_SIZE - 1u) >> 6) + 1u;
  const float2 inverse_size = rcp(float2(metadata.xy));
  float receiver_depth = 0.0;
  bool edge = false;

  // Each thread supplies a few depths for many neighboring receivers. Stream
  // them into group memory instead of keeping a large per-thread depth array.
  [loop]
  for (uint read = 0; read < reads; ++read) {
    const float2 texel = floor(sample_pixel) + 0.5;
    const float minor = major_x ? sample_pixel.y : sample_pixel.x;
    const float interpolation = frac(minor) - 0.5;
    const float bias = interpolation > 0.0 ? 1.0 : -1.0;
    const float2 offset = major_x ? float2(0.0, bias) : float2(bias, 0.0);
    float first = bend_depth.SampleLevel(bend_border, texel * inverse_size, 0).x;
    float second = bend_depth.SampleLevel(bend_border, (texel + offset) * inverse_size, 0).x;
    first = isfinite(first) && first > 0.0 ? first : 0.0;
    second = isfinite(second) && second > 0.0 ? second : 0.0;
    // Bend's default 2% threshold avoids joining separate surfaces.
    const bool depth_edge = abs(first - second) > abs(first) * 0.02;
    // Reverse-Z: moving the casting depth farther away means subtracting.
    const float casting_depth = depth_edge ? first : first - abs(first - second);
    const float distance = pixel_distance + BEND_GROUP_SIZE * read * direction;
    float stored = (casting_depth - light.z) / distance;
    if (abs(distance) < 1e-7 || (read != 0 && distance <= 0.0) || !isfinite(stored)) {
      stored = 1e20;
    }
    shared_depth[read * BEND_GROUP_SIZE + lane] = stored;
    if (read == 0) {
      receiver_depth = first;
      edge = depth_edge;
    }
    sample_pixel += delta * direction;
  }
  // Out-of-bounds threads still supply depths needed by in-bounds receivers.
  GroupMemoryBarrierWithGroupSync();
  if (any(output_pixel < bounds.xy) || any(output_pixel >= bounds.zw)) return;
  // Projected rays can land on the same pixel. Select one writer after every
  // thread has supplied its depths, so group execution order cannot change it.
  if (major_start == 0.0) return;
  const int2 center_grid = output_pixel - light_grid;
  const precise float2 center_direction = float2(center_grid) - light_fraction;
  const float2 extension = float2(int(major_start) - abs(center_grid)) / max(abs(center_direction), 1e-20);
  // Intersect the pixel-center ray with this group's square ring, then round
  // to its nearest shared ray. Subtract integers first to retain precision for
  // distant projected lights, including a sun on the camera's horizon.
  const precise float2 endpoint_delta = float2(center_grid - relative)
                                        + center_direction * min(extension.x, extension.y);
  if (any(floor(endpoint_delta + 0.5) != 0.0)) return;
  if (!(receiver_depth > 0.0) || abs(pixel_distance) < 1e-7) {
    bend_output[output_pixel] = float4(1.0, 0.25, receiver_depth, 0.0);
    return;
  }

  if (receiver_culling) {
    const float denominator = receiver_depth - bend_projection_param.w;
    const float view_depth = bend_projection_param.z / denominator;
    if (denominator > 0.0 && isfinite(view_depth)) {
      const float2 pixel = (float2(output_pixel) + 0.5);
      const float2 ray = (pixel * inverse_size * float2(2.0, -2.0) + float2(-1.0, 1.0)
                          - bend_camera_center.xy)
                         * bend_projection_param.xy;
      const float3 position = float3(ray * view_depth, view_depth);
      const float3 delta = position - bend_light_params[3].xyz;
      const float distance = length(delta);
      // Keep uncertain pixels near the light boundary.
      const float margin = length(2.0 * bend_projection_param.xy * view_depth * inverse_size)
                           + view_depth * 0.00001 + distance * 0.00002;
      bool lit = local_range <= 0.0 || distance <= local_range + margin;
      const float3 axis = bend_light_params[5].xyz;
      const float3 cone = bend_light_params[7].xyz;
      if (spot && all(isfinite(axis)) && all(isfinite(cone))
          && cone.x > 0.0 && cone.y > 0.0 && cone.z > 0.0) {
        lit = lit && dot(axis, delta) + length(axis) * margin >= cone.x * max(0.0, distance - margin);
      }
      if (!lit) {
        bend_output[output_pixel] = float4(1.0, 0.25, 0.0, 0.0);
        return;
      }
    }
  }

  const float depth_scale = min(pixel_distance + direction, rcp(thickness)) * pixel_distance / receiver_depth;
  const float reference = (receiver_depth - light.z) / pixel_distance * depth_scale + 1.0;
  const uint first_sample = lane + 1;
  const uint hard_samples = 4u;
  const uint fade_samples = 8u;
  float hard = 1.0;
  [unroll(4)]
  for (uint hard_index = 0; hard_index < hard_samples; ++hard_index) {
    hard = min(hard, abs(reference - shared_depth[first_sample + hard_index] * depth_scale));
  }
  const float contrast = 4.0;
  const float hard_visibility = saturate(hard * contrast + 1.0 - contrast);
  float soft_visibility = 1.0;
  // Shared depth reads are complete. Fully blocked contacts need no farther reduction.
  [branch]
  if (hard_visibility > 0.0) {
    float4 soft = 1.0;
    // Four independent minima permit four contiguous loads per iteration, with
    // no dynamic vector indexing or four-way mask work for every single sample.
    uint soft_index = hard_samples;
    [loop]
    for (; soft_index + 3u < samples - fade_samples; soft_index += 4u) {
      const uint address = first_sample + soft_index;
      const float4 depths = float4(shared_depth[address], shared_depth[address + 1u],
                                   shared_depth[address + 2u], shared_depth[address + 3u]);
      float4 separation = abs(reference - depths * depth_scale);
      soft = min(soft, separation);
    }
    // Typed reach values need not end on a four-sample boundary.
    [loop]
    for (; soft_index < samples - fade_samples; ++soft_index) {
      const float separation = abs(reference - shared_depth[first_sample + soft_index] * depth_scale);
      switch (soft_index & 3u) {
        case 0u: soft.x = min(soft.x, separation); break;
        case 1u: soft.y = min(soft.y, separation); break;
        case 2u: soft.z = min(soft.z, separation); break;
        case 3u: soft.w = min(soft.w, separation); break;
      }
    }
    // The eight fade samples form two groups of four. Align their minima once
    // with the existing groups instead of selecting a component for every sample.
    float4 fade = 1.0;
    [unroll(2)]
    for (uint fade_block = 0; fade_block < fade_samples / 4u; ++fade_block) {
      const uint address = first_sample + samples - fade_samples + fade_block * 4u;
      const float4 depths = float4(shared_depth[address], shared_depth[address + 1u],
                                   shared_depth[address + 2u], shared_depth[address + 3u]);
      const float4 fade_offset = (float4(1, 2, 3, 4) + fade_block * 4u) / float(fade_samples + 1u) * 0.75;
      fade = min(fade, abs(reference - depths * depth_scale) + fade_offset);
    }
    // Match the fade lanes to the same four minima as the preceding samples.
    switch ((samples - fade_samples) & 3u) {
      case 1u: fade = fade.wxyz; break;
      case 2u: fade = fade.zwxy; break;
      case 3u: fade = fade.yzwx; break;
    }
    soft = min(soft, fade);
    soft_visibility = dot(saturate(soft * contrast + 1.0 - contrast), 0.25);
  }
  float visibility = min(hard_visibility, soft_visibility);
  float diagnostic = edge ? 1.0 : 0.0;
  if (CUSTOM_SS_DEBUG_VIEW == 3.0) {
    diagnostic = frac(float(group.x) / BEND_GROUP_SIZE);
  }
  if (CUSTOM_SS_DEBUG_VIEW == 5.0) {
    diagnostic = 1.0;
  }
  // G below 0.25 means the clear ran but no sweep wrote this pixel. Keep native
  // lighting there. The remaining G range carries the selected debug signal.
  bend_output[output_pixel] = float4(isfinite(visibility) ? visibility : 1.0, 0.25 + 0.75 * diagnostic,
                                     0.0, 0.0);
}
