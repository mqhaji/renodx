// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

// Search visible geometry in both directions along each slice. The same depth
// samples supply AO, diffuse bounce, and specular visibility after skin scattering.
// Keep the outputs separate: the application pass darkens native diffuse with AO,
// adds the bounce, and resolves specular visibility for deferred composition.

#include "../gbuffer.hlsli"
#include "../shared.h"
#include "specular_occlusion.hlsli"

#define AO_PI               3.14159265358979323846
#define AO_SECTORS          32u
#define AO_DEPTH_MIP_LEVELS 4

Texture2D<float4> depth_buffer : register(t0);
Texture2D<float4> normal_buffer : register(t1);
Texture2D<float4> depth_mips : register(t2);
Texture2DArray<float2> fast_noise : register(t6);
Texture2D<float4> native_material : register(t7);
Texture2D<float4> environment_specular : register(t8);
RWTexture2D<float2> ao_output : register(u0);

Texture2D<float4> source_diffuse : register(t3);
Texture2D<float4> source_albedo : register(t4);
Texture2D<float4> source_specular : register(t5);
RWTexture2D<float4> gi_output : register(u1);
RWTexture2D<float4> diagnostic_output : register(u2);
// RG stores visible/open angular areas relative to the whole reflection cone.
// B is native roughness; A is sampling confidence, with -1 for missing data.
RWTexture2D<float4> specular_output : register(u3);

// A coarse depth texel keeps the nearest surface in its block. Find that
// surface's full-resolution pixel before reading its color or normal. Match
// depth-pyramid edge handling and prefer the original sample when depths tie.
bool ResolveDepthOwner(int2 sample_pixel, int mip_level, float sampled_depth, out int2 owner) {
  owner = sample_pixel;
  if (mip_level == 0) return true;
  owner = clamp(sample_pixel >> mip_level, int2(0, 0),
                max(int2(cBufferWidth, cBufferHeight) >> mip_level, 1) - 1);
  float owner_depth = sampled_depth;
  [loop]
  for (int level = mip_level; level > 0; --level) {
    const int2 parent_size = max(int2(cBufferWidth, cBufferHeight) >> level, 1);
    const int2 child_size = max(int2(cBufferWidth, cBufferHeight) >> (level - 1), 1);
    const int2 base = owner * 2;
    const int2 end = int2(owner.x + 1 == parent_size.x ? child_size.x : min(base.x + 2, child_size.x),
                          owner.y + 1 == parent_size.y ? child_size.y : min(base.y + 2, child_size.y));
    const int2 preferred_child = clamp(sample_pixel >> (level - 1), base, end - 1);
    int2 selected = preferred_child;
    [branch]
    if (level == 1) {
      owner_depth = depth_buffer.Load(int3(selected, 0)).x;
    }
    else {
      owner_depth = depth_mips.Load(int3(selected, level - 2)).x;
    }
    [loop]
    for (int y = base.y; y < end.y; ++y) {
      [loop]
      for (int x = base.x; x < end.x; ++x) {
        // The preferred child is already loaded and wins equal-depth ties.
        if (all(int2(x, y) == preferred_child)) continue;
        float candidate;
        [branch]
        if (level == 1) {
          candidate = depth_buffer.Load(int3(x, y, 0)).x;
        }
        else {
          candidate = depth_mips.Load(int3(x, y, level - 2)).x;
        }
        if (candidate > owner_depth) {
          owner_depth = candidate;
          selected = int2(x, y);
        }
      }
    }
    owner = selected;
  }
  return owner_depth == sampled_depth;
}

// Sector ownership is [first, end). Guard empty/full intervals before shifting.
uint SectorMask(uint first, uint end) {
  if (end <= first || first >= AO_SECTORS) return 0u;
  uint high = 0xFFFFFFFFu;
  if (end < AO_SECTORS) high = (1u << end) - 1u;
  return high & ~((1u << first) - 1u);
}

// Reference mode 3, using its HQ angular mapping. The asin form is equivalent
// to (1 - acos(x * sin(stretch)) * 2 / PI) / s without cancellation near s=0.
float2 SampleCosineSliceDir(float2 normal_xy, float phase) {
  const float angle = phase * AO_PI;
  float2 direction = float2(cos(angle), sin(angle));
  const float normal_length = length(normal_xy);
  if (normal_length == 0.0) return direction;
  direction *= dot(direction, normal_xy) < 0.0 ? -1.0 : 1.0;
  const float2 aligned_normal = normal_xy / normal_length;
  const float x = direction.x * aligned_normal.y - direction.y * aligned_normal.x;
  float stretch_weight = saturate(normal_length);
  stretch_weight += (stretch_weight - stretch_weight * stretch_weight) * 0.15;
  const float stretch = stretch_weight * (AO_PI * 0.5);
  const float y = asin(clamp(x * sin(stretch), -1.0, 1.0)) / stretch;
  const float aligned_x = sqrt(saturate(1.0 - y * y));
  return float2(aligned_x * aligned_normal.x - y * aligned_normal.y, y * aligned_normal.x + aligned_x * aligned_normal.y);
}

// Cumulative cosine-weighted measure along a slice. x spans the receiver's
// hemisphere; endpoints must be handled before evaluating the interior formula.
float SliceRelCDFCos(float x, float normal_angle, float cos_n, bool direction_is_right) {
  if (x <= 0.0 || x >= 1.0) return x;
  const float phi = x * AO_PI - AO_PI * 0.5;
  const float constant_weight = direction_is_right ? 3.0 : 1.0;
  const float cosine_weight = direction_is_right ? -1.0 : 1.0;
  const float angle_weight = direction_is_right ? 4.0 : 0.0;
  const float sin_n = sin(normal_angle);
  return (constant_weight * cos_n + cosine_weight * cos(normal_angle - 2.0 * phi)
          + (angle_weight * normal_angle + cosine_weight * 2.0 * phi + AO_PI) * sin_n)
         / (4.0 * (cos_n + normal_angle * sin_n));
}

float LoadTraceDepth(int2 sample_pixel, int mip_level) {
  float sampled_depth = 0.0;
  [branch]
  if (mip_level == 0) {
    sampled_depth = depth_buffer.Load(int3(sample_pixel, 0)).x;
  }
  else {
    const int2 coarse_size = max(int2(cBufferWidth, cBufferHeight) >> mip_level, 1);
    const int2 coarse_pixel = clamp(sample_pixel >> mip_level, int2(0, 0), coarse_size - 1);
    sampled_depth = depth_mips.Load(int3(coarse_pixel, mip_level - 1)).x;
  }
  return sampled_depth;
}

uint2 CoverageMasks(float2 sample_position, float sample_depth, float thickness,
                    float3 position, float3 view_direction, float side, float normal_angle, float cos_n,
                    float sector_offset, float4 specular_interval, bool2 unfinished) {
  const float sample_z = LinearZ(sample_depth);
  // Native reverse-Z zero is background, not a finite surface slab.
  if (!(sample_depth > 0.0 && sample_z > 0.0) || sample_z > 1.0e6) return 0u;
  // Keep both endpoints in the perspective slice. A coarse depth remains a
  // conservative proxy even when its source radiance is read from the owner.
  const float3 front = ViewPos(sample_position, sample_z) - position;
  const float3 back = ViewPos(sample_position, sample_z + thickness) - position;
  const float2 lengths_squared = float2(dot(front, front), dot(back, back));
  if (any(lengths_squared < 1e-12)) return 0u;
  const float2 horizon_cos = clamp(float2(dot(front, view_direction), dot(back, view_direction)) * rsqrt(lengths_squared), -1.0, 1.0);
  uint specular_mask = 0u;
  [branch]
  if (unfinished.y && specular_interval.w > 0.0) {
    float2 measure = side * (1.0 - horizon_cos);
    if (side < 0.0) measure = measure.yx;
    float2 fraction = saturate((measure - specular_interval.x) * specular_interval.z);
    fraction = (measure >= specular_interval.y ? 1.0 : fraction);
    const uint2 sectors = (uint2)floor(saturate(fraction + sector_offset) * float(AO_SECTORS));
    specular_mask = SectorMask(sectors.x, sectors.y);
  }
  uint diffuse_mask = 0u;
  [branch]
  if (unfinished.x) {
    float2 horizon = saturate(acos(horizon_cos) * (side / AO_PI) + normal_angle / AO_PI + 0.5);
    if (side < 0.0) horizon = horizon.yx;
    horizon.x = SliceRelCDFCos(horizon.x, normal_angle, cos_n, side > 0.0);
    horizon.y = SliceRelCDFCos(horizon.y, normal_angle, cos_n, side > 0.0);
    const uint2 sectors = (uint2)floor(saturate(horizon + sector_offset) * float(AO_SECTORS));
    diffuse_mask = SectorMask(sectors.x, sectors.y);
  }
  return uint2(diffuse_mask, specular_mask);
}

[numthreads(8, 8, 1)]

void main(uint3 tid : SV_DispatchThreadID) {
  const uint2 ao_pixel = tid.xy;
  uint ao_width, ao_height;
  ao_output.GetDimensions(ao_width, ao_height);
  if (ao_pixel.x >= ao_width || ao_pixel.y >= ao_height) return;
  gi_output[ao_pixel] = 0.0;
  const bool specular_active = CUSTOM_MGSV_SPECULAR_ACTIVE >= 0.5;
  if (specular_active) {
    specular_output[ao_pixel] = float4(0.0, 0.0, 0.0, -1.0);
  }
  const int diagnostic = (int)CUSTOM_MGSV_GT_DEBUG_VIEW;
  if (diagnostic >= 2 && diagnostic <= 11) {
    diagnostic_output[ao_pixel] = diagnostic == 4 ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 0.0, 0.0, 1.0);
    if (diagnostic >= 8 && diagnostic <= 10) diagnostic_output[ao_pixel] = 1.0;
  }

  const float scale = max(CUSTOM_MGSV_GT_RESOLUTION_DIVISOR, 1.0);
  const int2 receiver_pixel = min((int2)floor((float2(ao_pixel) + 0.5) * scale),
                                  int2(cBufferWidth, cBufferHeight) - 1);
  const float depth = depth_buffer.Load(int3(receiver_pixel, 0)).x;
  const float z = LinearZ(depth);
  if (!(depth > 0.0 && z > 0.0) || z > 1.0e6) {
    ao_output[ao_pixel] = float2(1.0, 1.0);
    return;
  }

  const float2 pixel = float2(receiver_pixel);
  float3 normal = DecodeNormal(normal_buffer.Load(int3(receiver_pixel, 0)).xyz);
  const float3 native_normal = normal;
  const float3 surface_position = ViewPos(pixel, z);
  // Source normals and native shading retain their original orientation.
  const float3 surface_view = normalize(-surface_position);
  // Fold the view component continuously instead of reversing the entire normal.
  normal -= 2.0 * min(dot(normal, surface_view), 0.0) * surface_view;
  // Keep the biased point on the view-facing side of the receiver plane.
  // Half the available distance leaves a margin for nearly tangent normals.
  const float offset = min(max(CUSTOM_MGSV_GT_SURFACE_OFFSET, 0.0),
                           0.5 * max(dot(normal, -surface_position), 0.0));
  const float3 position = surface_position + normal * offset;
  const float3 view_direction = normalize(-position);
  // The reference CDF assumes a view-facing receiver. Do not extrapolate its
  // measure to backfacing shading normals; use neutral visibility instead.
  if (any(!isfinite(normal)) || !(position.z > 0.0) || dot(normal, view_direction) < 0.0) {
    // Distinguish receiver rejection reasons in the geometry diagnostic.
    if (diagnostic == 4) {
      float3 reason = float3(0.0, 0.0, 1.0);
      if (any(!isfinite(normal)))
        reason = float3(1.0, 1.0, 0.0);
      else if (!(position.z > 0.0))
        reason = float3(0.0, 1.0, 1.0);
      diagnostic_output[ao_pixel] = float4(reason, 1.0);
    }
    ao_output[ao_pixel] = float2(1.0, 1.0);
    return;
  }

  const float2 ray_start = ((position.xy / (cProjectionParam.xy * position.z) + cCameraCenterOffset)
                                * float2(1.0, -1.0)
                            + 1.0)
                           * 0.5 * float2(cBufferWidth, cBufferHeight);
  const int slices = (int)max(CUSTOM_MGSV_GT_SLICE_COUNT, 1.0);
  const int steps = (int)max(CUSTOM_MGSV_GT_SAMPLES_PER_SIDE, 1.0);
  const float radius_px = max(CUSTOM_MGSV_GT_SEARCH_RADIUS_PIXELS, 1.0);
  const float thickness = max(CUSTOM_MGSV_GT_SURFACE_THICKNESS, 0.0);
  const float step_multiplier = pow(radius_px, 1.0 / float(steps));
  const float specular_contribution = max(CUSTOM_MGSV_GI_SOURCE_SPECULAR_WEIGHT, 0.0);

  // Align slices with the view direction. Positive view depth avoids the
  // quaternion's singularity at the opposite direction.
  const float basis_scale = rcp(1.0 - view_direction.z);
  const float3 axis_x = float3(1.0 - view_direction.x * view_direction.x * basis_scale, -view_direction.x * view_direction.y * basis_scale, view_direction.x);
  const float3 axis_y = float3(-view_direction.x * view_direction.y * basis_scale, 1.0 - view_direction.y * view_direction.y * basis_scale, view_direction.y);
  const float2 normal_xy = float2(dot(normal, axis_x), dot(normal, axis_y));

  bool specular_valid = false;
  float roughness = 0.0;
  float3 reflection = 0.0;
  float specular_radius = 0.0;
  float inverse_open_area = 0.0;
  float specular_support = 0.0;
  float2 specular_mass = 0.0;
  if (specular_active) {
    const float4 captured = environment_specular.Load(int3(receiver_pixel, 0));
    roughness = native_material.Load(int3(receiver_pixel, 0)).r;
    const float3 native_view = normalize(-ViewPos(pixel + 0.5, z));
    const float normal_view = dot(native_normal, native_view);
    if (captured.a > 0.5 && all(isfinite(captured)) && isfinite(roughness)
        && roughness >= 0.0 && roughness <= 1.0 && normal_view > 0.0
        && dot(native_normal, view_direction) > 0.0 && dot(native_normal, normal) > 0.99999) {
      reflection = reflect(-native_view, native_normal);
      specular_radius = SpecularConeRadius(roughness);
      const float open_area = SpecularOpenArea(specular_radius, dot(native_normal, reflection));
      if (isfinite(open_area) && open_area > 1e-6) {
        inverse_open_area = rcp(open_area);
        specular_support = smoothstep(0.0, 0.2, normal_view)
                           * SpecularSliceSupport(length(normal_xy), dot(reflection, view_direction), specular_radius, float(slices));
        const float view_difference = acos(clamp(dot(native_view, view_direction), -1.0, 1.0));
        specular_support *= 1.0 - smoothstep(0.1 * specular_radius, 0.25 * specular_radius, view_difference);
        specular_valid = true;
      }
    }
  }

  float visibility = 0.0;
  float3 indirect = 0.0;
  float accepted_coverage = 0.0;
  bool gi_valid = true;

  const uint noise_frame = (CUSTOM_TAA != 0.0 || CUSTOM_MGSV_GI_TEMPORAL_ACTIVE > 0.5 || CUSTOM_MGSV_AO_TEMPORAL_ACTIVE > 0.5)
                               ? (uint)CUSTOM_MGSV_GT_NOISE_FRAME
                               : 0u;
  const uint2 noise_pixel = ao_pixel & 127u;

  [loop]

  for (int slice = 0; slice < slices; ++slice) {
    // Use the stored uniform channels directly.
    // A separated R2 offset supplies the third variate, following EA's guidance.
    // Address the trace lattice, avoiding decimation of noise at reduced scale.
    const uint sample_index = (noise_frame * (uint)slices + (uint)slice) & 31u;
    float3 phases;
    phases.xy = fast_noise.Load(int4(noise_pixel, sample_index, 0));
    phases.z = fast_noise.Load(int4((noise_pixel + uint2(96u, 72u)) & 127u, sample_index, 0)).x;
    // Map the 256 stored ranks to bin centers, excluding exact zero/one.
    phases = (phases * 255.0 + 0.5) / 256.0;
    const float2 slice_direction = SampleCosineSliceDir(normal_xy, (float(slice) + phases.x) / float(slices));
    const float3 slice_direction_3d = axis_x * slice_direction.x + axis_y * slice_direction.y;
    // Derivative of native ViewPos's inverse projection, including signed X and
    // the screen-Y flip. Unlike uniform screen angles, this preserves slice area.
    const float2 screen_direction = normalize(float2(
        float(cBufferWidth) / cProjectionParam.x * (slice_direction_3d.x - position.x / position.z * slice_direction_3d.z),
        -float(cBufferHeight) / cProjectionParam.y * (slice_direction_3d.y - position.y / position.z * slice_direction_3d.z)));
    const float3 slice_normal = cross(view_direction, slice_direction_3d);
    const float3 projected_normal = normal - slice_normal * dot(normal, slice_normal);
    const float projected_length_squared = dot(projected_normal, projected_normal);
    if (projected_length_squared < 1e-12) {
      visibility += 1.0;
      continue;
    }
    const float projected_inverse_length = rsqrt(projected_length_squared);
    const float3 projected_tangent = cross(slice_normal, projected_normal);
    const float cos_n = clamp(dot(projected_normal, view_direction) * projected_inverse_length, 0.0, 1.0);
    const float normal_sign = dot(view_direction, projected_tangent) < 0.0 ? -1.0 : 1.0;
    const float normal_angle = normal_sign * acos(cos_n);
    const float sector_offset = phases.z / float(AO_SECTORS);
    const float radial_phase = phases.y;
    uint occupied = 0u;
    uint specular_occupied = 0u;
    float4 specular_interval = 0.0;
    float specular_weight = 0.0;
    if (specular_valid) {
      specular_interval = SpecularSliceInterval(float2(dot(native_normal, view_direction), dot(native_normal, slice_direction_3d)),
                                                float2(dot(reflection, view_direction), dot(reflection, slice_direction_3d)),
                                                cos(specular_radius));
      if (specular_interval.w > 0.0) {
        const float normal_length = length(normal_xy);
        const float cosine_azimuth = normal_length > 0.0 ? saturate(dot(normal_xy, slice_direction) / normal_length) : 1.0;
        specular_weight = SpecularSliceWeight(normal_length, cosine_azimuth);
      }
    }

    [loop]

    for (int side = 0; side < 2 && (occupied != 0xFFFFFFFFu || (specular_interval.w > 0.0 && specular_occupied != 0xFFFFFFFFu)); ++side) {
      const float side_sign = side == 0 ? -1.0 : 1.0;
      const float2 ray_direction = screen_direction * side_sign;
      const float side_phase = side == 0 ? radial_phase : 1.0 - radial_phase;
      float distance_px = pow(step_multiplier, side_phase);
      [loop]
      for (int step = 0; step < steps; ++step) {
        const float sample_distance = distance_px;
        distance_px *= step_multiplier;
        // One shared geometry sample supplies both visibility and bounce.
        const float2 sample_position = ray_start + ray_direction * sample_distance;
        if (any(sample_position < 0.0) || any(sample_position >= float2(cBufferWidth, cBufferHeight))) break;

        const int2 sample_pixel = min((int2)floor(sample_position + 0.5), int2(cBufferWidth, cBufferHeight) - 1);
        int mip_level = 0;
        [branch]
        if (CUSTOM_MGSV_GT_DEPTH_MIPS_ENABLED > 0.5) {
          mip_level = clamp((int)floor(log2(max(sample_distance, 1.0)) - 3.3), 0, AO_DEPTH_MIP_LEVELS);
        }
        const float sample_depth = LoadTraceDepth(sample_pixel, mip_level);
        const uint2 coverage = CoverageMasks(sample_position, sample_depth, thickness, position, view_direction, side_sign,
                                             normal_angle, cos_n, sector_offset, specular_interval,
                                             bool2(occupied != 0xFFFFFFFFu, specular_occupied != 0xFFFFFFFFu));
        const uint covered = coverage.x;
        specular_occupied |= coverage.y;
        uint fresh = covered & ~occupied;
        if (fresh != 0u) {
          int2 owner;
          if (!ResolveDepthOwner(sample_pixel, mip_level, sample_depth, owner)) {
            gi_valid = false;
          } else {
            if (CUSTOM_MGSV_GI_REJECT_BACKFACES > 0.5) {
              const float3 source_normal = DecodeNormal(normal_buffer.Load(int3(owner, 0)).xyz);
              if (any(!isfinite(source_normal))) {
                gi_valid = false;
                fresh = 0u;
              }
              // A backfacing surface still occludes. Only its outgoing radiance
              // is masked, using the reference's projected source hemisphere.
              const float3 source_projected = source_normal - slice_normal * dot(source_normal, slice_normal);
              const float source_length_squared = dot(source_projected, source_projected);
              if (source_length_squared > 1e-12) {
                const bool flip_t = dot(projected_tangent, source_normal) < 0.0;
                const float source_cos = clamp(dot(projected_normal, source_projected)
                                                   * projected_inverse_length * rsqrt(source_length_squared),
                                               -1.0, 1.0);
                float cutoff = acos(source_cos) / AO_PI;
                if (flip_t) cutoff = 1.0 - cutoff;
                cutoff = SliceRelCDFCos(cutoff, normal_angle, cos_n,
                                        cutoff * AO_PI - AO_PI * 0.5 > normal_angle);
                const uint first = (uint)floor(saturate(cutoff + sector_offset) * float(AO_SECTORS));
                uint source_mask = SectorMask(first, AO_SECTORS);
                if (!flip_t) source_mask = ~source_mask;
                fresh &= source_mask;
              }
            }
            if (fresh != 0u) {
              // Albedo is already decoded by its sRGB SRV. Diffuse/specular
              // are native linear pre-exposed light; source alpha is an SSS
              // mask and must not reject non-skin or valid black samples.
              const float3 albedo = source_albedo.Load(int3(owner, 0)).rgb;
              const float3 diffuse = source_diffuse.Load(int3(owner, 0)).rgb;
              float3 radiance = albedo * diffuse;
              if (specular_contribution > 0.0) radiance += specular_contribution * source_specular.Load(int3(owner, 0)).rgb;
              if (any(!isfinite(radiance))) {
                gi_valid = false;
              } else {
                const float fresh_coverage = float(countbits(fresh)) / float(AO_SECTORS);
                indirect += max(radiance, 0.0) * fresh_coverage;
                accepted_coverage += fresh_coverage;
              }
            }
          }
        }
        // Rejected or invalid source radiance must not make geometry transparent.
        occupied |= covered;
        if (occupied == 0xFFFFFFFFu && (specular_interval.w <= 0.0 || specular_occupied == 0xFFFFFFFFu)) break;
      }
    }
    // Importance sampling already accounts for slice weight. Fractional band
    // compensation would no longer describe which surface owns each sector.
    visibility += 1.0 - float(countbits(occupied)) / float(AO_SECTORS);
    if (specular_valid) {
      const float open_mass = specular_interval.w * specular_weight;
      specular_mass += float2(1.0 - float(countbits(specular_occupied)) / 32.0, 1.0) * open_mass;
    }
  }

  const float occlusion = saturate(1.0 - visibility / float(slices));
  if (specular_valid) {
    const float2 masses = specular_mass * (inverse_open_area / float(slices));
    if (all(isfinite(masses)) && all(masses >= 0.0) && all(masses <= 65504.0)) {
      specular_output[ao_pixel] = float4(min(masses.x, masses.y), masses.y, roughness, specular_support);
    }
  }
  const float applied = saturate(occlusion * CUSTOM_MGSV_AO_STRENGTH);
  ao_output[ao_pixel] = float2(1.0 - applied, 1.0);
  if (diagnostic >= 8 && diagnostic <= 10) {
    diagnostic_output[ao_pixel] = float4((1.0 - applied).xxx, 1.0);
  }
  // No-hit and black-source results remain valid zero. Missing qualification
  // is separate from AO visibility. Bound only to the RGBA16F storage range.
  const float3 bounce = indirect / float(slices);
  gi_valid = gi_valid && all(isfinite(bounce));
  gi_output[ao_pixel] = gi_valid ? float4(clamp(bounce, 0.0, 65504.0), 1.0) : 0.0;
  if (diagnostic >= 2 && diagnostic <= 5) {
    float3 value = occlusion.xxx;
    if (diagnostic == 3) value = (accepted_coverage / float(slices)).xxx;
    if (diagnostic == 4) value = gi_valid ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    if (diagnostic == 5) {
      value = source_albedo.Load(int3(receiver_pixel, 0)).rgb * source_diffuse.Load(int3(receiver_pixel, 0)).rgb;
      if (specular_contribution > 0.0) value += specular_contribution * source_specular.Load(int3(receiver_pixel, 0)).rgb;
      value = all(isfinite(value)) ? clamp(value, 0.0, 65504.0) : float3(1.0, 0.0, 0.0);
    }
    diagnostic_output[ao_pixel] = float4(value, 1.0);
  }
}
