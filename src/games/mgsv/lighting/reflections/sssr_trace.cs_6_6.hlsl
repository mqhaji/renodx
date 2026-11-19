// SPDX-License-Identifier: MIT
#include "common.hlsli"

// Enhanced reflection trace. For each receiver, sample 1 to 4 GGX reflection
// directions and march each through the nearest-depth pyramid. A hit gathers
// linear scene radiance; the rest falls back to the saved environment.
// reflected_light holds the averaged demodulated light (material response
// divided out) and normalized hit distance in NRD REBLUR packing.
// environment_weight holds the importance weight left for the environment in
// RGB and, in alpha, the fraction of rays not blocked by geometry.

Texture2D<float> depth_pyramid : register(t0);
Texture2D<float4> source_radiance : register(t1);
Texture2D<float4> normals : register(t2);
Texture2D<float4> materials : register(t3);
Texture2D<float4> brdf_table : register(t4);
Texture2D<float2> sampling_noise : register(t5);
Texture2D<float4> saved_environment : register(t6);
RWTexture2D<float4> reflected_light : register(u0);
RWTexture2D<float4> environment_weight : register(u1);
SamplerState linear_clamp : register(s1);

static const uint MAX_TRACE_STEPS = 128u;

// Trace the projected segment through nearest-depth cells, moving up a level
// after an empty cell and down when a cell may hold a hit. Reciprocal view
// depth is linear along the screen-space segment; view depth itself is not.
bool TraceReflection(float3 origin, float3 direction, int2 receiver, out float2 hit_uv, out float distance) {
  hit_uv = 0.0;
  distance = 0.0;
  const float near_z = GetReflectionDepth(1.0);
  float ray_length = min(GetReflectionDepth(0.0), 1e6);
  if (direction.z < -1e-6) ray_length = min(ray_length, (near_z - origin.z) / direction.z);
  const float3 endpoint = origin + direction * ray_length;
  if (!(ray_length > 0.0) || !(endpoint.z > 0.0)) return false;
  const float2 size = float2(reflection_size);
  const float2 start = ((origin.xy / (origin.z * current_scene.projection_parameters.xy) + current_scene.camera_center_offset.xy)
                            * float2(0.5, -0.5)
                        + 0.5)
                       * size;
  const float2 end = ((endpoint.xy / (endpoint.z * current_scene.projection_parameters.xy) + current_scene.camera_center_offset.xy)
                          * float2(0.5, -0.5)
                      + 0.5)
                     * size;
  const float2 delta = end - start;
  if (!all(isfinite(delta)) || dot(delta, delta) < 1e-8) return false;
  const bool2 moving = abs(delta) >= 1e-8;
  const float2 inverse_delta = select(moving, rcp(delta), 0.0);
  float last = 1.0;
  [unroll] for (uint axis = 0u; axis < 2u; ++axis) {
    if (moving[axis]) last = min(last, max(-start[axis] * inverse_delta[axis], (size[axis] - start[axis]) * inverse_delta[axis]));
  }
  const float epsilon = 1e-3 / max(length(delta), 1.0);
  const float inverse_start = rcp(origin.z);
  const float inverse_end = rcp(endpoint.z);
  float current = epsilon;
  int level = 0;
  [loop] for (uint step = 0u; step < MAX_TRACE_STEPS && current < last; ++step) {
    const float2 position = start + current * delta;
    if (any(position < 0.0) || any(position >= size)) return false;
    const int2 pixel = int2(position);
    const int2 mip_size = max(int2(reflection_size) >> level, 1);
    const int2 cell = min(pixel >> level, mip_size - 1);
    const float2 lower = float2(cell << level);
    const float2 upper = float2(cell.x + 1 == mip_size.x ? reflection_size.x : (cell.x + 1) << level,
                                cell.y + 1 == mip_size.y ? reflection_size.y : (cell.y + 1) << level);
    const float2 boundary = float2(delta.x > 0.0 ? upper.x : lower.x, delta.y > 0.0 ? upper.y : lower.y);
    const float2 crossing = select(moving, (boundary - start) * inverse_delta, 1e30);
    const float next = min(last, max(min(crossing.x, crossing.y), current + epsilon));
    const float raw_depth = depth_pyramid.Load(int3(cell, level));
    const float surface_inverse_z = (raw_depth - current_scene.projection_parameters.w) / current_scene.projection_parameters.z;
    const float inverse_z0 = lerp(inverse_start, inverse_end, current);
    const float inverse_z1 = lerp(inverse_start, inverse_end, next);
    if (raw_depth <= 0.0 || min(inverse_z0, inverse_z1) > surface_inverse_z) {
      current = next + epsilon;
      level = min(level + 1, int(maximum_mip));
      continue;
    }
    if (level > 0) {
      --level;
      continue;
    }
    // A single depth layer has no back surface. Use a small view-space slab,
    // expanded by one pixel's footprint, rather than accepting any crossed depth.
    const float surface_z = GetReflectionDepth(raw_depth);
    const float thickness = 0.05 + surface_z * abs(current_scene.projection_parameters.y) * (2.0 / size.y);
    if (max(inverse_z0, inverse_z1) * (1.0 + thickness * surface_inverse_z) >= surface_inverse_z
        && max(abs(pixel.x - receiver.x), abs(pixel.y - receiver.y)) > 1) {
      const float t = saturate((surface_inverse_z - inverse_start) / (inverse_end - inverse_start));
      const float2 hit = abs(inverse_end - inverse_start) > 1e-10 ? start + clamp(t, current, next) * delta : position;
      hit_uv = clamp(hit / size, 0.5 / size, 1.0 - 0.5 / size);
      distance = length(GetReflectionPosition(hit_uv * size, raw_depth) - origin);
      return true;
    }
    current = next + epsilon;
  }
  // A ray that remains in view all the way to the far plane can see the drawn
  // sky. Do not turn an off-screen exit or an exhausted step budget into sky.
  if (current >= 1.0 && last >= 1.0 && direction.z > 0.0
      && all(end >= 0.0) && all(end < size)
      && depth_pyramid.Load(int3(int2(end), 0)) <= 0.0) {
    hit_uv = clamp(end / size, 0.5 / size, 1.0 - 0.5 / size);
    distance = 0.0;  // Sky has an angular footprint, not a surface distance.
    return true;
  }
  return false;
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
  if (any(id.xy >= reflection_size)) return;
  const int2 pixel = id.xy;
  reflected_light[pixel] = 0.0;
  environment_weight[pixel] = float4(0.0, 0.0, 0.0, 1.0);
  const float4 guide = normals.Load(int3(pixel, 0));
  const float4 material = materials.Load(int3(pixel, 0));
  if (guide.a < 0.0 || material.a <= 0.0) return;

  // The receiver, material response and tangent frame are shared by all rays.
  // Directions are sampled from the GGX lobe, so each hit distance tracks the
  // specular reflection; the radiance estimate still evaluates both single
  // and multiple scattering below.
  const float3 position = GetReflectionPosition(float2(pixel) + 0.5, depth_pyramid.Load(int3(pixel, 0)));
  const float3 view = normalize(-position);
  const float3 normal = guide.xyz * (dot(guide.xyz, view) < 0.0 ? -1.0 : 1.0);
  const float3 tangent = normalize(cross(abs(normal.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0), normal));
  const float3 bitangent = cross(normal, tangent);
  const float3 local_view = float3(dot(view, tangent), dot(view, bitangent), dot(view, normal));
  // Read the FAST noise atlas on this pass's pixel grid. Its 32 layers are
  // stacked vertically; the scene's camera sample, not Present, picks the
  // layer. Decode each byte at its bin center to keep the GGX input strictly
  // inside (0, 1).
  const uint2 noise_pixel = uint2(id.x & 127u, (id.y & 127u) + (reflection_sample & 31u) * 128u);
  const float2 noise = (sampling_noise.Load(int3(noise_pixel, 0)) * 255.0 + 0.5) / 256.0;
  const float no_v = saturate(local_view.z);
  const float3 integral = GetSpecularBRDFIntegral(brdf_table, no_v, guide.a);
  const SpecularResponse response = GetSpecularResponse(material.rgb, integral);
  const float alpha = max(guide.a * guide.a, SPECULAR_MIN_ALPHA);
  const float a2 = alpha * alpha;
  const float view_root = sqrt(a2 + (1.0 - a2) * no_v * no_v);

  // A single nonnegative estimate includes both screen and environment light.
  // Demodulate the saved environment by this receiver's material response and
  // fog transmission; the resolve restores that response exactly once.
  const float4 environment = saved_environment.Load(int3(pixel, 0));
  const float3 energy = response.energy * material.a * environment.a;
  const float3 fallback = select(energy > 1e-20, environment.rgb / max(energy, 1e-20), 0.0);
  const float bias = max(1e-4, abs(position.z) * 1e-5);
  float3 light_sum = 0.0;
  float3 remaining_sum = 0.0;
  float visibility_sum = 0.0;
  float tracking_distance = NRD_FrontEnd_SpecHitDistAveraging_Begin();
  [loop] for (uint ray = 0u; ray < reflection_rays_per_pixel; ++ray) {
    // Rays 1 to 4 shift the noise by half a period in a 2x2 pattern. A shifted
    // uniform sample is still uniform, so no bright samples or BRDF tails are
    // cut at any ray count.
    const float2 sample = frac(noise + float2(ray & 1u, ray >> 1u) * 0.5);
    const float3 local_half = SampleSpecularVisibleNormal(local_view, alpha, sample);
    const float3 sampled_half = tangent * local_half.x + bitangent * local_half.y + normal * local_half.z;
    const float3 direction = reflect(-view, sampled_half);
    const float no_l = dot(normal, direction);
    if (no_l <= 0.0) {
      // A direction below the surface contributes zero radiance and no hit.
      visibility_sum += 1.0;
      continue;
    }

    // Evaluate the full BRDF (single plus reciprocal multiple scattering) over
    // the visible-normal GGX sampling PDF.
    const float3 half_vector = normalize(view + direction);
    const float no_h = saturate(dot(normal, half_vector));
    const float light_root = sqrt(a2 + (1.0 - a2) * no_l * no_l);
    // Avoid subtracting nearly equal values at sharp normal incidence.
    const float denominator = (1.0 - no_h) * (1.0 + no_h) + a2 * no_h * no_h;
    const float distribution = a2 / (SPECULAR_BRDF_PI * denominator * denominator);
    const float pdf = distribution / (2.0 * (no_v + view_root));
    const float cosine_pdf = no_l / SPECULAR_BRDF_PI;
    const float fresnel = pow(1.0 - saturate(dot(view, half_vector)), 5.0);
    const float3 single = (material.rgb * (1.0 - fresnel) + fresnel) * distribution * no_l
                          / max(2.0 * (no_l * view_root + no_v * light_root), 1e-20);
    const float3 light_integral = GetSpecularBRDFIntegral(brdf_table, no_l, guide.a);
    const float3 multiple = response.multiple_scattering * (1.0 - integral.x - integral.y)
                            * (1.0 - light_integral.x - light_integral.y) * cosine_pdf / max(1.0 - integral.z, 1e-6);
    // Demodulate by the complete response. Importance weights can exceed one;
    // capping them would discard valid reflected energy.
    const float3 weight = select(response.energy > 0.0, (single + multiple) / max(pdf * response.energy, 1e-20), 0.0);
    float3 light = 0.0;
    float3 remaining_weight = weight;
    float visibility = 1.0;
    float2 hit_uv;
    float hit_distance;
    const bool hit = TraceReflection(position + normal * bias, direction, pixel, hit_uv, hit_distance);
    float first_hit_distance = NRD_FP16_MAX;
    if (hit) {
      const int2 hit_pixel = min(int2(hit_uv * reflection_size), int2(reflection_size) - 1);
      const float4 hit_normal = normals.Load(int3(hit_pixel, 0));
      const bool sky_hit = depth_pyramid.Load(int3(hit_pixel, 0)) <= 0.0;
      visibility = sky_hit ? 1.0 : 0.0;
      // A rejected source color still represents a geometric blocker. Sky and
      // unresolved exits use a long distance, never a false zero-distance hit.
      if (!sky_hit) first_hit_distance = hit_distance;
      if (sky_hit || (all(isfinite(hit_normal.xyz)) && dot(hit_normal.xyz, -direction) > 0.0)) {
        const float4 source = source_radiance.SampleLevel(linear_clamp, hit_uv, 0.0);
        if (source.a > 0.0 && all(isfinite(source))) {
          // Fade screen light out near the screen edge; the rest of the
          // weight falls back to the environment.
          const float edge = min(min(hit_uv.x, hit_uv.y), min(1.0 - hit_uv.x, 1.0 - hit_uv.y));
          const float confidence = smoothstep(0.0, 0.025, edge);
          light = source.rgb * weight * confidence;
          remaining_weight = weight * (1.0 - confidence * saturate(source.a));
        }
      }
    }
    // Confirmed geometry blocks only the unresolved environment contribution.
    // Accepted screen radiance already contains visibility.
    light_sum += light + fallback * remaining_weight * lerp(1.0, visibility, environment_occlusion_strength);
    remaining_sum += remaining_weight;
    visibility_sum += visibility;
    NRD_FrontEnd_SpecHitDistAveraging_Add(tracking_distance, first_hit_distance);
  }
  // Average linear radiance before packing. NRD's multi-path helper retains
  // the nearest nonzero hit for reflection tracking, rather than averaging
  // unrelated surface distances. Normalize that geometric distance only once.
  NRD_FrontEnd_SpecHitDistAveraging_End(tracking_distance);
  const float normalized_distance = REBLUR_FrontEnd_GetNormHitDist(tracking_distance, position.z, hit_distance_parameters, guide.a);
  reflected_light[pixel] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(light_sum / float(reflection_rays_per_pixel), normalized_distance, true);
  environment_weight[pixel] = float4(remaining_sum, visibility_sum) / float(reflection_rays_per_pixel);
}
