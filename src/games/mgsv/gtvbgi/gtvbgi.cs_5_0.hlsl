// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

// Search visible geometry in both directions along each slice. The same depth
// samples supply AO, diffuse bounce, and specular visibility before skin scattering.
// AO and GI share storage, but keep independent validity and application rules.
// Application darkens only native ambient, adds GI, and resolves specular
// visibility for deferred composition. See indirect_diffuse.hlsli for packing.

#include "../gbuffer.hlsli"
#include "../shared.h"
#include "indirect_diffuse.hlsli"
#include "source_radiance.hlsli"
#include "specular_brdf.hlsli"
#include "specular_occlusion.hlsli"
#include "specular_trace.hlsli"

#define AO_PI 3.14159265358979323846
// Each bit owns an equal share of the slice's weighted angular measure.
#define AO_SECTORS 32u
// Optional reductions cover 2, 4, 8, and 16 render pixels along each axis.
#define AO_DEPTH_MIP_LEVELS 4

Texture2D<float4> depth_buffer : register(t0);
Texture2D<float4> normal_buffer : register(t1);
Texture2D<float4> depth_mips : register(t2);
Texture2D<float4> material_palette : register(t5);
Texture2D<float4> brdf_table : register(t9);
Texture2D<float4> fresnel_table : register(t10);
RWTexture2D<float4> specular_gi_output : register(u0);
RWTexture2D<float4> specular_response_output : register(u4);
Texture2DArray<float2> fast_noise : register(t6);
Texture2D<float4> native_material : register(t7);
Texture2D<float4> environment_specular : register(t8);

Texture2D<float4> source_diffuse : register(t3);
Texture2D<float4> source_albedo : register(t4);
Texture2D<float4> clean_material : register(t11);
Texture2D<uint4> material_owner : register(t12);
Texture2D<float4> prepared_source : register(t13);
RWTexture2D<float4> material_output : register(u5);
RWTexture2D<float4> lighting_output : register(u1);
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
                max(int2(buffer_width, buffer_height) >> mip_level, 1) - 1);
  float owner_depth = sampled_depth;
  [loop] for (int level = mip_level; level > 0; --level) {
    const int2 parent_size = max(int2(buffer_width, buffer_height) >> level, 1);
    const int2 child_size = max(int2(buffer_width, buffer_height) >> (level - 1), 1);
    const int2 base = owner * 2;
    const int2 end = int2(owner.x + 1 == parent_size.x ? child_size.x : min(base.x + 2, child_size.x),
                          owner.y + 1 == parent_size.y ? child_size.y : min(base.y + 2, child_size.y));
    const int2 preferred_child = clamp(sample_pixel >> (level - 1), base, end - 1);
    int2 selected = preferred_child;
    [branch] if (level == 1) {
      owner_depth = depth_buffer.Load(int3(selected, 0)).x;
    }
    else {
      owner_depth = depth_mips.Load(int3(selected, level - 2)).x;
    }
    [loop] for (int y = base.y; y < end.y; ++y) {
      [loop] for (int x = base.x; x < end.x; ++x) {
        // The preferred child is already loaded and wins equal-depth ties.
        if (all(int2(x, y) == preferred_child)) continue;
        float candidate;
        [branch] if (level == 1) {
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
uint GetSectorMask(uint first, uint end) {
  if (end <= first || first >= AO_SECTORS) return 0u;
  uint high = 0xFFFFFFFFu;
  if (end < AO_SECTORS) high = (1u << end) - 1u;
  return high & ~((1u << first) - 1u);
}

// Importance-sample slice directions toward the projected surface normal, as
// in reference mode 3. This spends more samples where diffuse light matters.
// The asin form avoids cancellation in the reference's small-stretch formula.
float2 SampleCosineSliceDirection(float2 normal_xy, float phase) {
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
float GetSliceCosineCDF(float x, float normal_angle, float cos_n, bool direction_is_right) {
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
  [branch] if (mip_level == 0) {
    sampled_depth = depth_buffer.Load(int3(sample_pixel, 0)).x;
  }
  else {
    const int2 coarse_size = max(int2(buffer_width, buffer_height) >> mip_level, 1);
    const int2 coarse_pixel = clamp(sample_pixel >> mip_level, int2(0, 0), coarse_size - 1);
    sampled_depth = depth_mips.Load(int3(coarse_pixel, mip_level - 1)).x;
  }
  return sampled_depth;
}

uint GetSpecularCoverage(float2 horizon_cos, float side, float sector_offset, float4 interval) {
  float2 measure = side * (1.0 - horizon_cos);
  if (side < 0.0) measure = measure.yx;
  float2 fraction = saturate((measure - interval.x) * interval.z);
  fraction = (measure >= interval.y ? 1.0 : fraction);
  const uint2 sectors = (uint2)floor(saturate(fraction + sector_offset) * float(AO_SECTORS));
  return GetSectorMask(sectors.x, sectors.y);
}

uint3 GetCoverageMasks(float2 sample_position, float sample_depth, float thickness,
                       float3 position, float3 view_direction, float side, float normal_angle, float cos_n,
                       float sector_offset, float4 specular_interval, float4 transport_interval, bool3 unfinished) {
  // Treat each depth sample as a finite slab, not an infinitely thick wall.
  // Project its front/back into the diffuse measure and both reflection cones.
  // The existing visibility cone and the new material-mapped cone remain
  // separate until the new transport replaces the old specular resolve.
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
  if (unfinished.y && specular_interval.w > 0.0) {
    specular_mask = GetSpecularCoverage(horizon_cos, side, sector_offset, specular_interval);
  }
  uint transport_mask = 0u;
  if (unfinished.z && transport_interval.w > 0.0) {
    transport_mask = GetSpecularCoverage(horizon_cos, side, sector_offset, transport_interval);
  }
  uint diffuse_mask = 0u;
  [branch] if (unfinished.x) {
    float2 horizon = saturate(acos(horizon_cos) * (side / AO_PI) + normal_angle / AO_PI + 0.5);
    if (side < 0.0) horizon = horizon.yx;
    horizon.x = GetSliceCosineCDF(horizon.x, normal_angle, cos_n, side > 0.0);
    horizon.y = GetSliceCosineCDF(horizon.y, normal_angle, cos_n, side > 0.0);
    const uint2 sectors = (uint2)floor(saturate(horizon + sector_offset) * float(AO_SECTORS));
    diffuse_mask = GetSectorMask(sectors.x, sectors.y);
  }
  return uint3(diffuse_mask, specular_mask, transport_mask);
}

[numthreads(8, 8, 1)] void main(uint3 thread_id : SV_DispatchThreadID) {
  // 1. Select a representative full-resolution receiver for this lighting
  // pixel. Round the allocation up, but clamp its last sample to the scene edge.
  const uint2 ao_pixel = thread_id.xy;
  uint ao_width, ao_height;
  lighting_output.GetDimensions(ao_width, ao_height);
  if (ao_pixel.x >= ao_width || ao_pixel.y >= ao_height) return;
  const bool specular_active = CUSTOM_MGSV_SPECULAR_ACTIVE >= 0.5;
  const bool transport_active = CUSTOM_MGSV_SPECULAR_GI_ACTIVE >= 0.5;
  uint prepared_width, prepared_height, guide_width, guide_height;
  prepared_source.GetDimensions(prepared_width, prepared_height);
  material_output.GetDimensions(guide_width, guide_height);
  const bool prepared_available = prepared_width == buffer_width && prepared_height == buffer_height;
  const bool write_material = transport_active && guide_width == ao_width && guide_height == ao_height;
  if (write_material) material_output[ao_pixel] = float4(0.0, 0.0, 0.0, -1.0);
  if (transport_active) {
    specular_gi_output[ao_pixel] = float4(0.0, 0.0, 0.0, -1.0);
    specular_response_output[ao_pixel] = float4(0.0, 0.0, 0.0, -1.0);
  }
  const int diagnostic = (int)CUSTOM_MGSV_GT_DEBUG_VIEW;
  if (diagnostic >= 2 && diagnostic <= 11) {
    diagnostic_output[ao_pixel] = diagnostic == 4 ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 0.0, 0.0, 1.0);
    if (diagnostic >= 8 && diagnostic <= 10) diagnostic_output[ao_pixel] = 1.0;
  }

  const float scale = max(CUSTOM_MGSV_GT_RESOLUTION_DIVISOR, 1.0);
  const int2 receiver_pixel = min((int2)floor((float2(ao_pixel) + 0.5) * scale),
                                  int2(buffer_width, buffer_height) - 1);
  const float depth = depth_buffer.Load(int3(receiver_pixel, 0)).x;
  const float z = LinearZ(depth);
  if (!(depth > 0.0 && z > 0.0) || z > 1.0e6) {
    if (specular_active) {
      specular_output[ao_pixel] = float4(0.0, 0.0, 0.0, -1.0);
    }
    lighting_output[ao_pixel] = PackIndirectDiffuse(0.0, false, 1.0, true);
    return;
  }

  // 2. Reconstruct the receiver in camera space. Thin foliage may have shading
  // normals facing away from the camera; fold only their view component for
  // the diffuse integral. Native normals remain available for specular/source use.
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
    if (specular_active) {
      specular_output[ao_pixel] = float4(0.0, 0.0, 0.0, -1.0);
    }
    // Distinguish receiver rejection reasons in the geometry diagnostic.
    if (diagnostic == 4) {
      float3 reason = float3(0.0, 0.0, 1.0);
      if (any(!isfinite(normal)))
        reason = float3(1.0, 1.0, 0.0);
      else if (!(position.z > 0.0))
        reason = float3(0.0, 1.0, 1.0);
      diagnostic_output[ao_pixel] = float4(reason, 1.0);
    }
    lighting_output[ao_pixel] = PackIndirectDiffuse(0.0, false, 1.0, true);
    return;
  }

  const float2 ray_start = ((position.xy / (projection_parameters.xy * position.z) + camera_center_offset)
                                * float2(1.0, -1.0)
                            + 1.0)
                           * 0.5 * float2(buffer_width, buffer_height);
  const int slices = (int)max(CUSTOM_MGSV_GT_SLICE_COUNT, 1.0);
  const int steps = (int)max(CUSTOM_MGSV_GT_SAMPLES_PER_SIDE, 1.0);
  const float radius_px = max(CUSTOM_MGSV_GT_SEARCH_RADIUS_PIXELS, 1.0);
  const float thickness = max(CUSTOM_MGSV_GT_SURFACE_THICKNESS, 0.0);
  const float step_multiplier = pow(radius_px, 1.0 / float(steps));

  // Align slices with the view direction. Positive view depth avoids the
  // quaternion's singularity at the opposite direction.
  const float basis_scale = rcp(1.0 - view_direction.z);
  const float3 axis_x = float3(1.0 - view_direction.x * view_direction.x * basis_scale, -view_direction.x * view_direction.y * basis_scale, view_direction.x);
  const float3 axis_y = float3(-view_direction.x * view_direction.y * basis_scale, 1.0 - view_direction.y * view_direction.y * basis_scale, view_direction.y);
  const float2 normal_xy = float2(dot(normal, axis_x), dot(normal, axis_y));

  // 3. Prepare a roughness-dependent reflection cone. Its mask uses the same
  // geometry search, but a different angular measure from diffuse. Unsupported
  // capture or cone coverage will retain native specular AO during composition.
  bool specular_valid = false;
  float roughness = 0.0;
  float3 specular_normal = native_normal;
  float3 reflection = 0.0;
  float specular_radius = 0.0;
  float inverse_open_area = 0.0;
  float specular_support = 0.0;
  float2 specular_mass = 0.0;
  if (specular_active) {
    const float4 captured = environment_specular.Load(int3(receiver_pixel, 0));
    roughness = native_material.Load(int3(receiver_pixel, 0)).r;
    const float3 native_view = normalize(-ViewPos(pixel + 0.5, z));
    const float normal_view = abs(dot(native_normal, native_view));
    specular_normal *= GetSpecularSurfaceSide(native_normal, native_view);
    // The shared trace uses a biased diffuse origin. Its view must remain on
    // this side of the normal for the slice integral to be valid.
    if (captured.a > 0.5 && all(isfinite(captured)) && isfinite(roughness)
        && roughness >= 0.0 && roughness <= 1.0 && dot(specular_normal, view_direction) > 0.0) {
      reflection = reflect(-native_view, native_normal);
      specular_radius = GetSpecularConeRadius(roughness);
      const float open_area = GetSpecularOpenArea(specular_radius, dot(specular_normal, reflection));
      if (isfinite(open_area) && open_area > 1e-6) {
        inverse_open_area = rcp(open_area);
        // The selected side changes at grazing. Fade to native occlusion there
        // instead of exposing a discontinuous hemisphere change in the image.
        specular_support = smoothstep(0.0, 0.2, normal_view)
                           * GetSpecularSliceSupport(length(normal_xy), dot(reflection, view_direction), specular_radius, float(slices));
        const float view_difference = acos(clamp(dot(native_view, view_direction), -1.0, 1.0));
        specular_support *= 1.0 - smoothstep(0.1 * specular_radius, 0.25 * specular_radius, view_difference);
        specular_valid = true;
      }
    }
  }

  // Specular GI shares geometry with diffuse GI, but integrates a different
  // BRDF. Keep the native normal's reflection direction; the diffuse fold
  // above is only a two-sided diffuse approximation.
  SpecularMaterial material = (SpecularMaterial)0;
  if (transport_active) {
    material = LoadSpecularMaterial(material_palette, fresnel_table, native_material.Load(int3(receiver_pixel, 0)));
    if (write_material && material.valid) material_output[ao_pixel] = float4(material.f0, material.roughness);
  }
  float3 transport_view = normalize(-ViewPos(pixel + 0.5, z));
  float transport_radius = 0.0;
  float transport_support = 0.0;
  bool transport_valid = false;
  float3 screen_sum = 0.0;
  float2 cone_weights = 0.0;
  float2 open_weights = 0.0;
  if (transport_active && specular_valid) {
    transport_radius = GetSpecularConeRadius(material.roughness);
    transport_support = smoothstep(0.0, 0.2, abs(dot(native_normal, transport_view)))
                        * GetSpecularSliceSupport(length(normal_xy), dot(reflection, view_direction), transport_radius, float(slices));
    const float view_difference = acos(clamp(dot(transport_view, view_direction), -1.0, 1.0));
    transport_support *= 1.0 - smoothstep(0.1 * transport_radius, 0.25 * transport_radius, view_difference);
    transport_valid = material.valid;
  }
  const float transport_alpha = material.roughness * material.roughness;
  const float transport_alpha_squared = transport_alpha * transport_alpha;
  const bool transport_material_valid = transport_valid;
  const float broad_weight = smoothstep(0.35, 0.8, transport_support);

  float visibility = 0.0;
  float3 indirect = 0.0;
  float accepted_coverage = 0.0;
  bool gi_valid = true;

  const uint noise_frame = (CUSTOM_TAA != 0.0 || CUSTOM_MGSV_GI_TEMPORAL_ACTIVE > 0.5 || CUSTOM_MGSV_AO_TEMPORAL_ACTIVE > 0.5)
                               ? (uint)CUSTOM_MGSV_GT_NOISE_FRAME
                               : 0u;
  const uint2 noise_pixel = ao_pixel & 127u;

  // 4. Rotate each slice using IS-FAST noise on the lighting grid. Each slice
  // searches both screen directions with exponentially increasing distances.
  [loop] for (int slice = 0; slice < slices; ++slice) {
    // Use the stored uniform channels directly.
    // A separated R2 offset supplies the third variate, following EA's guidance.
    // Address the trace lattice, avoiding decimation of noise at reduced scale.
    const uint sample_index = (noise_frame * (uint)slices + (uint)slice) & 31u;
    float3 phases;
    phases.xy = fast_noise.Load(int4(noise_pixel, sample_index, 0));
    phases.z = fast_noise.Load(int4((noise_pixel + uint2(96u, 72u)) & 127u, sample_index, 0)).x;
    // Map the 256 stored ranks to bin centers, excluding exact zero/one.
    phases = (phases * 255.0 + 0.5) / 256.0;
    const float2 slice_direction = SampleCosineSliceDirection(normal_xy, (float(slice) + phases.x) / float(slices));
    const float3 slice_direction_3d = axis_x * slice_direction.x + axis_y * slice_direction.y;
    // Derivative of native ViewPos's inverse projection, including signed X and
    // the screen-Y flip. Unlike uniform screen angles, this preserves slice area.
    const float2 screen_direction = normalize(float2(
        float(buffer_width) / projection_parameters.x * (slice_direction_3d.x - position.x / position.z * slice_direction_3d.z),
        -float(buffer_height) / projection_parameters.y * (slice_direction_3d.y - position.y / position.z * slice_direction_3d.z)));
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
    // A set bit means a nearer sampled surface already owns this angular band.
    // Share one mask across both search sides so GI cannot count it twice.
    uint occupied = 0u;
    uint specular_occupied = 0u;
    uint transport_occupied = 0u;
    float4 transport_interval = 0.0;
    float4 specular_interval = 0.0;
    float specular_weight = 0.0;
    if (specular_valid) {
      specular_interval = GetSpecularSliceInterval(float2(dot(specular_normal, view_direction), dot(specular_normal, slice_direction_3d)),
                                                   float2(dot(reflection, view_direction), dot(reflection, slice_direction_3d)),
                                                   cos(specular_radius));
      if (specular_interval.w > 0.0) {
        const float normal_length = length(normal_xy);
        const float cosine_azimuth = normal_length > 0.0 ? saturate(dot(normal_xy, slice_direction) / normal_length) : 1.0;
        specular_weight = GetSpecularSliceWeight(normal_length, cosine_azimuth);
      }
    }

    if (transport_valid && broad_weight > 0.0) {
      transport_interval = GetSpecularSliceInterval(float2(dot(specular_normal, view_direction), dot(specular_normal, slice_direction_3d)),
                                                    float2(dot(reflection, view_direction), dot(reflection, slice_direction_3d)),
                                                    cos(transport_radius));
    }
    // Undo the diffuse-biased azimuth PDF for the BRDF integral. The signed
    // slice measure already contains sin(theta), so do not add another cosine
    // or inverse-square factor to outgoing source radiance.
    const float transport_slice_weight = transport_interval.w > 0.0
                                             ? GetSpecularSliceWeight(length(normal_xy), length(normal_xy) > 0.0 ? saturate(dot(normal_xy, slice_direction) / length(normal_xy)) : 1.0)
                                             : 0.0;
    [loop] for (int side = 0; side < 2 && (occupied != 0xFFFFFFFFu || (specular_interval.w > 0.0 && specular_occupied != 0xFFFFFFFFu) || (transport_valid && transport_interval.w > 0.0 && transport_occupied != 0xFFFFFFFFu)); ++side) {
      const float side_sign = side == 0 ? -1.0 : 1.0;
      const float2 ray_direction = screen_direction * side_sign;
      const float side_phase = side == 0 ? radial_phase : 1.0 - radial_phase;
      float distance_px = pow(step_multiplier, side_phase);
      [loop] for (int step = 0; step < steps; ++step) {
        const float sample_distance = distance_px;
        distance_px *= step_multiplier;
        // One shared geometry sample supplies both visibility and bounce.
        const float2 sample_position = ray_start + ray_direction * sample_distance;
        if (any(sample_position < 0.0) || any(sample_position >= float2(buffer_width, buffer_height))) break;

        const int2 sample_pixel = min((int2)floor(sample_position + 0.5), int2(buffer_width, buffer_height) - 1);
        int mip_level = 0;
        [branch] if (CUSTOM_MGSV_GT_DEPTH_MIPS_ENABLED > 0.5) {
          mip_level = clamp((int)floor(log2(max(sample_distance, 1.0)) - 3.3), 0, AO_DEPTH_MIP_LEVELS);
        }
        const float sample_depth = LoadTraceDepth(sample_pixel, mip_level);
        const uint3 coverage = GetCoverageMasks(sample_position, sample_depth, thickness, position, view_direction, side_sign,
                                                normal_angle, cos_n, sector_offset, specular_interval, transport_interval,
                                                bool3(occupied != 0xFFFFFFFFu, specular_occupied != 0xFFFFFFFFu, transport_valid && transport_occupied != 0xFFFFFFFFu));
        const uint covered = coverage.x;
        specular_occupied |= coverage.y;
        // 5. Gather radiance only for newly covered diffuse sectors. A coarse
        // depth has no color of its own, so descend to its full-size owner first.
        uint fresh = covered & ~occupied;
        // One missing source rejects the broad specular estimate. Stop its
        // remaining sector work then; diffuse/AO and the focused fallback still
        // run. Finishing a discarded estimate cannot improve the final image.
        uint fresh_transport = transport_valid ? coverage.z & ~transport_occupied : 0u;
        if (fresh != 0u || fresh_transport != 0u) {
          int2 owner;
          if (!ResolveDepthOwner(sample_pixel, mip_level, sample_depth, owner)) {
            if (fresh != 0u) gi_valid = false;
            if (fresh_transport != 0u) transport_valid = false;
          } else {
            float3 source_normal = 0.0;
            if ((CUSTOM_MGSV_GI_REJECT_BACKFACES > 0.5 && fresh != 0u) || fresh_transport != 0u) {
              source_normal = DecodeNormal(normal_buffer.Load(int3(owner, 0)).xyz);
            }
            if (CUSTOM_MGSV_GI_REJECT_BACKFACES > 0.5 && fresh != 0u) {
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
                cutoff = GetSliceCosineCDF(cutoff, normal_angle, cos_n,
                                           cutoff * AO_PI - AO_PI * 0.5 > normal_angle);
                const uint first = (uint)floor(saturate(cutoff + sector_offset) * float(AO_SECTORS));
                uint source_mask = GetSectorMask(first, AO_SECTORS);
                if (!flip_t) source_mask = ~source_mask;
                fresh &= source_mask;
              }
            }
            // A source faces the receiver according to its original normal.
            // A backface still blocks environment light; it contributes no
            // opaque outgoing radiance. Translucent source shading is future work.
            bool source_faces_receiver = true;
            if (fresh_transport != 0u) {
              if (any(!isfinite(source_normal))) {
                transport_valid = false;
                fresh_transport = 0u;
              }
              source_faces_receiver = all(isfinite(source_normal))
                                      && dot(source_normal, surface_position - ViewPos(float2(owner), LinearZ(sample_depth))) > 0.0;
            }
            if (fresh != 0u || fresh_transport != 0u) {
              // Albedo is already decoded by its sRGB SRV. Diffuse lighting
              // is linear and pre-exposed; source alpha is an SSS
              // mask and must not reject non-skin or valid black samples.
              float4 source;
              if (transport_active && prepared_available) {
                source = prepared_source.Load(int3(owner, 0));
              } else {
                source = LoadSourceRadiance(source_diffuse, source_albedo, native_material, clean_material, material_owner,
                                            owner, uint2(buffer_width, buffer_height), normal_buffer.Load(int3(owner, 0)), sample_depth, transport_active);
              }
              if (source.a < 0.5 && fresh_transport != 0u) {
                // Missing material color is not a black surface. Preserve
                // geometric occlusion, but reject its specular-light estimate.
                transport_valid = false;
                fresh_transport = 0u;
              }
              const float3 radiance = source.rgb;
              if (any(!isfinite(radiance))) {
                if (fresh != 0u) gi_valid = false;
                if (fresh_transport != 0u) transport_valid = false;
              } else {
                // Each newly claimed angular sector carries BRDF*cosine mass.
                // Accumulate the same mass even for valid black/backfacing
                // sources, otherwise an unlit blocker would leak environment.
                [loop] while (fresh_transport != 0u) {
                  const uint sector = firstbitlow(fresh_transport);
                  fresh_transport &= fresh_transport - 1u;
                  const float2 coefficients = GetSpecularSectorWeights(transport_interval, sector, sector_offset,
                                                                       view_direction, slice_direction_3d, specular_normal, transport_view,
                                                                       transport_alpha_squared)
                                              * transport_slice_weight;
                  cone_weights += coefficients;
                  if (source_faces_receiver) screen_sum += (material.f0 * coefficients.x + coefficients.y) * max(radiance, 0.0);
                }
                const float fresh_coverage = float(countbits(fresh)) / float(AO_SECTORS);
                indirect += max(radiance, 0.0) * fresh_coverage;
                accepted_coverage += fresh_coverage;
              }
            }
          }
        }
        // Rejected or invalid source radiance must not make geometry transparent.
        occupied |= covered;
        transport_occupied |= coverage.z;
        if (occupied == 0xFFFFFFFFu && (specular_interval.w <= 0.0 || specular_occupied == 0xFFFFFFFFu)
            && (!transport_valid || transport_interval.w <= 0.0 || transport_occupied == 0xFFFFFFFFu)) break;
      }
    }
    if (transport_valid && transport_interval.w > 0.0) {
      uint unclaimed = ~transport_occupied;
      [loop] while (unclaimed != 0u) {
        const uint sector = firstbitlow(unclaimed);
        unclaimed &= unclaimed - 1u;
        const float2 coefficients = GetSpecularSectorWeights(transport_interval, sector, sector_offset,
                                                             view_direction, slice_direction_3d, specular_normal, transport_view,
                                                             transport_alpha_squared)
                                    * transport_slice_weight;
        cone_weights += coefficients;
        open_weights += coefficients;
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

  // 6. Average the slices. Uncovered diffuse sectors remain ambient visibility;
  // covered sectors contribute source radiance once. Store specular visible/open
  // areas separately so later filtering can divide only after combining samples.
  const float occlusion = saturate(1.0 - visibility / float(slices));
  float4 specular_sample = float4(0.0, 0.0, 0.0, -1.0);
  if (specular_valid) {
    const float2 masses = specular_mass * (inverse_open_area / float(slices));
    if (all(isfinite(masses)) && all(masses >= 0.0) && all(masses <= 65504.0)) {
      specular_sample = float4(min(masses.x, masses.y), masses.y, roughness, specular_support);
    }
  }
  if (specular_active) {
    specular_output[ao_pixel] = specular_sample;
  }
  if (transport_active && transport_material_valid) {
    float3 broad_screen = 0.0;
    float3 broad_remaining = 1.0;
    bool broad_valid = false;
    [branch] if (transport_valid) {
      // Normalize only a usable broad estimate. A rejected one needs neither
      // the BRDF-table reads nor its remaining-energy calculation.
      const float4 integral = GetSpecularBRDFIntegral(brdf_table, abs(dot(native_normal, transport_view)), material.roughness);
      const float3 full_energy = material.f0 * integral.x + integral.y;
      const float3 cone_energy = material.f0 * integral.z + integral.w;
      const float3 sampled_energy = material.f0 * cone_weights.x + cone_weights.y;
      const float3 unclaimed_energy = material.f0 * open_weights.x + open_weights.y;
      const bool3 represented = sampled_energy > 1e-8 && full_energy > 1e-8;
      const float3 gain = represented ? cone_energy / max(sampled_energy, 1e-8) : 0.0;
      const float3 replacement = represented ? cone_energy : 0.0;
      broad_screen = screen_sum * gain / max(full_energy, 1e-8);
      broad_remaining = saturate((full_energy - replacement + unclaimed_energy * gain) / max(full_energy, 1e-8));
      broad_valid = all(isfinite(broad_screen)) && all(broad_screen >= 0.0)
                    && all(broad_screen <= 65504.0) && all(isfinite(broad_remaining));
    }
    float weight = broad_valid ? broad_weight : 0.0;
    float3 focused_screen = 0.0;
    float3 focused_remaining = 1.0;
    bool focused_valid = true;
    [branch] if (weight < 1.0) {
      // Narrow lobes need a focused ray. Sample a visible GGX normal using the
      // same roughness mapping, then share the broad estimator's source and
      // demodulated storage. This normalized split-sum estimator avoids large
      // inverse-PDF weights; the common DFG response is applied once at resolve.
      const float3 tangent = normalize(cross(abs(specular_normal.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0), specular_normal));
      const float3 bitangent = cross(specular_normal, tangent);
      const float3 local_view = float3(dot(transport_view, tangent), dot(transport_view, bitangent), dot(transport_view, specular_normal));
      const float2 sample = (fast_noise.Load(int4((noise_pixel + uint2(56u, 24u)) & 127u, noise_frame & 31u, 0)) * 255.0 + 0.5) / 256.0;
      const float3 local_half = SampleSpecularVisibleNormal(local_view, transport_alpha, sample);
      const float3 half_vector = tangent * local_half.x + bitangent * local_half.y + specular_normal * local_half.z;
      const float3 direction = reflect(-transport_view, half_vector);
      const float3 receiver_position = ViewPos(pixel + 0.5, z);
      const float bias = min(max(CUSTOM_MGSV_GT_SURFACE_OFFSET, 0.0), 0.5 * max(dot(specular_normal, -receiver_position), 0.0));
      const float3 origin = receiver_position + specular_normal * bias;
      if (dot(direction, specular_normal) > 0.0) {
        const SpecularRayHit hit = TraceSpecularRay(depth_buffer, depth_mips, origin, direction, receiver_pixel);
        if (hit.hit) {
          focused_remaining = 0.0;
          const float source_depth = depth_buffer.Load(int3(hit.pixel, 0)).r;
          const float4 source_normal = normal_buffer.Load(int3(hit.pixel, 0));
          const float3 source_position = ViewPos(float2(hit.pixel) + 0.5, LinearZ(source_depth));
          const float3 source_facing = DecodeNormal(source_normal.xyz);
          if (dot(source_facing, origin - source_position) > 0.0) {
            float4 source;
            if (prepared_available) {
              source = prepared_source.Load(int3(hit.pixel, 0));
            } else {
              source = LoadSourceRadiance(source_diffuse, source_albedo, native_material, clean_material, material_owner,
                                          hit.pixel, uint2(buffer_width, buffer_height), source_normal, source_depth, true);
            }
            focused_valid = source.a >= 0.5;
            if (focused_valid) focused_screen = max(source.rgb, 0.0);
          }
        }
      }
      if (!focused_valid && broad_valid) weight = 1.0;
    }
    const float3 screen = lerp(focused_screen, broad_screen, weight);
    const float3 remaining = lerp(focused_remaining, broad_remaining, weight);
    const float support = lerp(1.0, transport_support, weight);
    // These are complementary estimates of one lobe, not extra reflection
    // strengths. Valid black hits remove environment while adding zero color.
    if ((focused_valid || weight == 1.0) && all(isfinite(screen)) && all(screen >= 0.0)
        && all(screen <= 65504.0) && all(isfinite(remaining))) {
      specular_gi_output[ao_pixel] = float4(screen, material.roughness);
      specular_response_output[ao_pixel] = float4(saturate(remaining), support);
    }
  }
  const float applied = saturate(occlusion * CUSTOM_MGSV_AO_STRENGTH);
  if (diagnostic >= 8 && diagnostic <= 10) {
    diagnostic_output[ao_pixel] = float4((1.0 - applied).xxx, 1.0);
  }
  // No-hit and black-source results remain valid zero. Missing qualification
  // is separate from AO visibility. Bound only to the RGBA16F storage range.
  const float3 bounce = indirect / float(slices);
  gi_valid = gi_valid && all(isfinite(bounce));
  lighting_output[ao_pixel] = PackIndirectDiffuse(clamp(bounce, 0.0, 65504.0), gi_valid, 1.0 - applied, true);
  if (diagnostic >= 2 && diagnostic <= 5) {
    float3 value = occlusion.xxx;
    if (diagnostic == 3) value = (accepted_coverage / float(slices)).xxx;
    if (diagnostic == 4) value = gi_valid ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    if (diagnostic == 5) {
      value = source_albedo.Load(int3(receiver_pixel, 0)).rgb * source_diffuse.Load(int3(receiver_pixel, 0)).rgb;
      value = all(isfinite(value)) ? clamp(value, 0.0, 65504.0) : float3(1.0, 0.0, 0.0);
    }
    diagnostic_output[ao_pixel] = float4(value, 1.0);
  }
}
