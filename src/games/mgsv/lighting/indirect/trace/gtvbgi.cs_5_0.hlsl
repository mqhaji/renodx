// SPDX-License-Identifier: MIT
// Copyright and license notices: ../README.md#credits-and-license.

// GT-VBGI trace (ground-truth visibility-bitmask global illumination), run at
// the diffuse lighting resolution before the game's skin-scattering passes.
//
// For each receiver, search the depth buffer in both screen directions along
// several slices around the view direction. Each depth sample covers a range of
// angles in the slice; a 32-bit mask records which angles are already blocked.
// The same samples give ambient visibility (AO) and the diffuse light bounced
// from newly blocked angles (GI).
//
// Reads native reverse-Z depth, encoded normals, the optional nearest-depth
// pyramid, IS-FAST noise, and linear pre-exposed source radiance (prepared by
// CopyBuffer, or rebuilt here from the source inputs when it is missing).
// Writes GI and AO to lighting_output, packed as in indirect_diffuse.hlsli, and
// debug views to diagnostic_output. The application pass (gtvbgi_add) applies
// AO only to native ambient and adds GI.

#include "../../../rendering/geometry.hlsli"
#include "../../../shared.h"
#include "../indirect_diffuse.hlsli"
#include "../source_radiance.hlsli"

#define AO_PI 3.14159265358979323846
// Each mask bit covers an equal share of the slice's cosine-weighted measure.
#define AO_SECTORS 32u
// Optional reductions cover 2, 4, 8, and 16 render pixels along each axis.
#define AO_DEPTH_MIP_LEVELS 4
// The most an occluder is taken to be thick, in game units. The automatic
// thickness grows with how far an occluder runs along the search, which on a
// long continuous surface (a Mother Base pillar and the deck it joins) reached
// tens of units. Slabs that deep, under the random scale, now and then swung
// across a shaded pixel's hemisphere and lit it with a sunlit surface it cannot
// see: red specks that flashed from frame to frame. Up to 4 units they are gone.
#define AO_MAX_OCCLUDER_THICKNESS 4.0

Texture2D<float4> depth_buffer : register(t0);
Texture2D<float4> normal_buffer : register(t1);
Texture2D<float4> depth_mips : register(t2);
Texture2DArray<float2> fast_noise : register(t6);
Texture2D<float4> native_material : register(t7);

Texture2D<float4> source_diffuse : register(t3);
Texture2D<float4> source_albedo : register(t4);
Texture2D<float4> clean_material : register(t11);
Texture2D<uint4> material_owner : register(t12);
Texture2D<float4> prepared_source : register(t13);
RWTexture2D<float4> lighting_output : register(u1);
RWTexture2D<float4> diagnostic_output : register(u2);

// A coarse depth texel keeps the nearest surface in its block. Find that
// surface's full-resolution pixel before reading its color or normal. Match
// depth-pyramid edge handling and prefer the original sample when depths tie.
// Returns false when the descent ends at a different depth than was sampled.
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

// Returns a mask with sectors [first, end) set. Empty and full ranges are
// handled first because shifting a 32-bit value by 32 is undefined.
uint GetSectorMask(uint first, uint end) {
  if (end <= first || first >= AO_SECTORS) return 0u;
  uint high = 0xFFFFFFFFu;
  if (end < AO_SECTORS) high = (1u << end) - 1u;
  return high & ~((1u << first) - 1u);
}

// Importance-sample slice directions toward the projected surface normal, as
// in mode 3 of the reference shader. This spends more slices where diffuse
// light matters. The asin form avoids the precision loss of the reference's
// formula when the stretch is small.
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

// Mip level 0 is full-resolution depth; level N is depth_mips slice N - 1.
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

// Returns the sectors one depth sample blocks (x) and those its own slab
// covers (y). The sample is treated as a slab from its depth to depth +
// thickness, not as an infinitely thick wall. For occlusion its back reaches
// at least as far as the back of the occluder it belongs to, taken from where
// that occluder started (occluder_start, view space). Those extra sectors show
// the occluder's hidden part, not the surface the sample saw, so only the
// sample's own slab sends its light (y).
uint2 GetCoverageMask(float3 sample_point, float thickness, float3 occluder_start,
                      float3 position, float3 view_direction, float side, float normal_angle, float cos_n,
                      float sector_offset) {
  // Project the slab's ends into the slice. A coarse depth still blocks
  // conservatively even though its radiance is read from its owner pixel.
  // Moving a view-space point back along its camera ray scales it.
  const float3 front = sample_point - position;
  const float3 back = sample_point * (1.0 + thickness / sample_point.z) - position;
  const float3 start_back = occluder_start * (1.0 + thickness / occluder_start.z) - position;
  const float3 lengths_squared = float3(dot(front, front), dot(back, back), dot(start_back, start_back));
  if (any(lengths_squared < 1e-12)) return 0u;
  const float3 cosines = clamp(float3(dot(front, view_direction), dot(back, view_direction), dot(start_back, view_direction)) * rsqrt(lengths_squared), -1.0, 1.0);
  // Front, own back, and the occluder's back.
  float3 horizon = saturate(acos(float3(cosines.x, cosines.y, min(cosines.y, cosines.z))) * (side / AO_PI) + normal_angle / AO_PI + 0.5);
  horizon.x = GetSliceCosineCDF(horizon.x, normal_angle, cos_n, side > 0.0);
  horizon.y = GetSliceCosineCDF(horizon.y, normal_angle, cos_n, side > 0.0);
  horizon.z = GetSliceCosineCDF(horizon.z, normal_angle, cos_n, side > 0.0);
  const uint3 sectors = (uint3)floor(saturate(horizon + sector_offset) * float(AO_SECTORS));
  // The search's direction decides which end is which.
  return side < 0.0 ? uint2(GetSectorMask(sectors.z, sectors.x), GetSectorMask(sectors.y, sectors.x))
                    : uint2(GetSectorMask(sectors.x, sectors.z), GetSectorMask(sectors.x, sectors.y));
}

// Linear, pre-exposed outgoing radiance of one full-resolution pixel (albedo
// was decoded by its sRGB view): the prepared source where it exists, else
// rebuilt from the source inputs. Alpha only flags a cube-material fallback.
float4 LoadTraceSource(int2 pixel, float depth, bool prepared_available) {
  // One return: FXC reports a value returned early from a branch as
  // potentially uninitialized.
  float4 source;
  [branch] if (prepared_available) {
    source = prepared_source.Load(int3(pixel, 0));
  } else {
    source = LoadSourceRadiance(source_diffuse, source_albedo, native_material, clean_material, material_owner,
                                pixel, uint2(buffer_width, buffer_height), normal_buffer.Load(int3(pixel, 0)), depth, true);
  }
  return source;
}

[numthreads(8, 8, 1)] void main(uint3 thread_id : SV_DispatchThreadID) {
  // 1. Pick one full-resolution receiver for this lighting pixel. The lighting
  // grid is rounded up, so clamp its last receiver to the render edge.
  const uint2 ao_pixel = thread_id.xy;
  uint ao_width, ao_height;
  lighting_output.GetDimensions(ao_width, ao_height);
  if (ao_pixel.x >= ao_width || ao_pixel.y >= ao_height) return;
  uint prepared_width, prepared_height;
  prepared_source.GetDimensions(prepared_width, prepared_height);
  const bool prepared_available = prepared_width == buffer_width && prepared_height == buffer_height;
  const int diagnostic = (int)CUSTOM_MGSV_INDIRECT_LIGHTING_DEBUG_VIEW;
  // Debug views 2 to 6 read diagnostic_output. Fill it first so pixels that
  // exit early still show a defined value.
  if (diagnostic >= 2 && diagnostic <= 6) {
    diagnostic_output[ao_pixel] = diagnostic == 4 ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 0.0, 0.0, 1.0);
    if (diagnostic == 6) diagnostic_output[ao_pixel] = 1.0;
  }

  const float scale = max(CUSTOM_MGSV_DIFFUSE_RESOLUTION_DIVISOR, 1.0);
  const int2 receiver_pixel = min((int2)floor((float2(ao_pixel) + 0.5) * scale),
                                  int2(buffer_width, buffer_height) - 1);
  const float depth = depth_buffer.Load(int3(receiver_pixel, 0)).x;
  const float z = LinearZ(depth);
  if (!(depth > 0.0 && z > 0.0) || z > 1.0e6) {
    lighting_output[ao_pixel] = PackIndirectDiffuse(0.0, false, 1.0, true);
    return;
  }

  // 2. Reconstruct the receiver in view space. Thin foliage may have shading
  // normals facing away from the camera. For this receiver's integral only,
  // mirror the normal's view component when it faces away; this changes
  // smoothly, unlike flipping the whole normal. Source normals read later keep
  // their native orientation.
  const float2 pixel = float2(receiver_pixel);
  float3 normal = DecodeNormal(normal_buffer.Load(int3(receiver_pixel, 0)).xyz);
  const float3 surface_position = ViewPos(pixel, z);
  const float3 surface_view = normalize(-surface_position);
  normal -= 2.0 * min(dot(normal, surface_view), 0.0) * surface_view;
  // The search's samples are rebuilt at the pixel centers whose depth they
  // read (below), so the receiver's own surface lies on its plane and does not
  // shade it. What is left to clear is rounding. The scene depth is a 32-bit
  // float, so the view depth it gives is good to about one part in 2^23: the
  // receiver and a neighbor can each be half of that off, and rebuilding their
  // positions rounds about as much again. Four parts in 2^23 of the view depth
  // cover both. Keep the biased point on the view-facing side of the receiver
  // plane; half the available distance leaves a margin for nearly tangent
  // normals.
  const float offset = min(4.76837158e-7 * z, 0.5 * max(dot(normal, -surface_position), 0.0));
  const float3 position = surface_position + normal * offset;
  const float3 view_direction = normalize(-position);
  // The reference CDF assumes a view-facing receiver. Do not extrapolate its
  // measure to backfacing shading normals; use neutral visibility instead.
  if (any(!isfinite(normal)) || !(position.z > 0.0) || dot(normal, view_direction) < 0.0) {
    // The Result Validity view colors each rejection reason.
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

  // Project the offset position back to render pixels; the search starts there.
  const float2 ray_start = ((position.xy / (projection_parameters.xy * position.z) + camera_center_offset)
                                * float2(1.0, -1.0)
                            + 1.0)
                           * 0.5 * float2(buffer_width, buffer_height);
  const int slices = (int)max(CUSTOM_MGSV_DIFFUSE_DIRECTION_COUNT, 1.0);
  const int steps = (int)max(CUSTOM_MGSV_DIFFUSE_SAMPLES_PER_SIDE, 1.0);
  const float radius_px = max(CUSTOM_MGSV_DIFFUSE_SEARCH_RADIUS_PIXELS, 1.0);
  const float step_multiplier = pow(radius_px, 1.0 / float(steps));

  // 3. Build a basis around the view direction; slices rotate about it. The
  // basis is singular only when view_direction is +Z. Positive view depth
  // keeps view_direction.z negative.
  const float basis_scale = rcp(1.0 - view_direction.z);
  const float3 axis_x = float3(1.0 - view_direction.x * view_direction.x * basis_scale, -view_direction.x * view_direction.y * basis_scale, view_direction.x);
  const float3 axis_y = float3(-view_direction.x * view_direction.y * basis_scale, 1.0 - view_direction.y * view_direction.y * basis_scale, view_direction.y);
  const float2 normal_xy = float2(dot(normal, axis_x), dot(normal, axis_y));

  float visibility = 0.0;
  float3 indirect = 0.0;
  float accepted_coverage = 0.0;
  bool gi_valid = true;

  // With reconstruction on, the noise changes with each camera sample so the
  // temporal upscaler averages it. With reconstruction off, a fixed pattern
  // keeps the image still.
  const uint noise_frame = CUSTOM_RECONSTRUCTION_MODE != 0.0 ? (uint)CUSTOM_MGSV_DIFFUSE_NOISE_FRAME : 0u;
  const uint2 noise_pixel = ao_pixel & 127u;

  // 4. Rotate each slice using IS-FAST noise (EA's importance-sampled
  // filter-adapted spatiotemporal noise). Each slice searches both screen
  // directions with exponentially increasing distances.
  [loop] for (int slice = 0; slice < slices; ++slice) {
    // The noise texel holds two uniform values: slice angle and step phase.
    // A second read at an R2-sequence offset supplies the sector offset and
    // the thickness scale, following EA's guidance. Noise is indexed by
    // lighting pixel, so a reduced lighting resolution does not skip noise
    // texels.
    const uint sample_index = (noise_frame * (uint)slices + (uint)slice) & 31u;
    float4 phases;
    phases.xy = fast_noise.Load(int4(noise_pixel, sample_index, 0));
    phases.zw = fast_noise.Load(int4((noise_pixel + uint2(96u, 72u)) & 127u, sample_index, 0));
    // Map the 256 stored ranks to bin centers, excluding exact zero/one.
    phases = (phases * 255.0 + 0.5) / 256.0;
    const float2 slice_direction = SampleCosineSliceDirection(normal_xy, (float(slice) + phases.x) / float(slices));
    const float3 slice_direction_3d = axis_x * slice_direction.x + axis_y * slice_direction.y;
    // Project the view-space slice direction to a screen direction with the
    // derivative of ViewPos's projection at this point (signed X, flipped
    // screen Y). Picking uniform screen angles instead would distort the slice
    // distribution chosen above.
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
    // Auto thickness, after bjornornorn's GT-VBAO thickness heuristic
    // (https://www.shadertoy.com/view/3clGWB, under its MIT option; see the
    // README's credits). Behind a visible surface the depth buffer shows
    // nothing, so how far back an occluder goes is a guess: it is taken to be
    // as thick as it is long along the search. That length is how far its
    // front surface has run since it started, plus the part the search
    // skipped before the sample where it first met it. The occluder's edge
    // lies anywhere between that sample and the one before, so the skipped
    // part is taken as half that gap, in view space at the sample's depth. A
    // new occluder starts where the search steps mostly toward or away from
    // the camera (a depth jump, not the same surface going on) or crosses
    // background. The first starts at the receiver itself, so a surface joined
    // to it without a depth jump (a floor running into a wall) blocks down to
    // the horizon.
    //
    // A scale of 2^(-1..1), per pixel, slice and camera sample, multiplies
    // every thickness and, as in the reference, the sideways part of the step
    // in the new-occluder test, so the heuristic's guesses average out under
    // temporal reconstruction instead of leaving bands. With reconstruction
    // off the noise, and this scale, hold still.
    const float thickness_scale = exp2(2.0 * phases.w - 1.0);
    // ViewPos is linear in depth and in the pixel, so a gap of g pixels along
    // this slice at depth z spans g * z * gap_scale in view space, on either
    // side.
    const float gap_scale = length(ViewPos(ray_start + screen_direction, 1.0) - ViewPos(ray_start, 1.0));
    // A set bit means a nearer sampled surface already owns this angular band.
    // Share one mask across both search sides so GI cannot count it twice.
    uint occupied = 0u;
    [loop] for (int side = 0; side < 2 && occupied != 0xFFFFFFFFu; ++side) {
      const float side_sign = side == 0 ? -1.0 : 1.0;
      const float2 ray_direction = screen_direction * side_sign;
      const float side_phase = side == 0 ? radial_phase : 1.0 - radial_phase;
      float distance_px = pow(step_multiplier, side_phase);
      float3 occluder_start = position;
      float3 previous_point = position;
      float previous_distance_px = 0.0;
      // Half the gap before the current occluder's first sample.
      float skipped_run = 0.0;
      // Whether the search crossed background (sky) since the last surface:
      // the next surface then starts a new occluder, so objects with sky
      // between them are not joined into one.
      bool crossed_background = false;
      [loop] for (int step = 0; step < steps; ++step) {
        const float sample_distance = distance_px;
        distance_px *= step_multiplier;
        const float gap_px = sample_distance - previous_distance_px;
        previous_distance_px = sample_distance;
        // One shared geometry sample supplies both visibility and bounce.
        const int2 sample_pixel = (int2)floor(ray_start + ray_direction * sample_distance + 0.5);
        if (any(sample_pixel < 0) || any(sample_pixel >= int2(buffer_width, buffer_height))) break;

        int mip_level = 0;
        [branch] if (CUSTOM_MGSV_DIFFUSE_COARSE_DEPTH > 0.5) {
          mip_level = clamp((int)floor(log2(max(sample_distance, 1.0)) - 3.3), 0, AO_DEPTH_MIP_LEVELS);
        }
        const float sample_depth = LoadTraceDepth(sample_pixel, mip_level);
        const float sample_z = LinearZ(sample_depth);
        uint covered = 0u;
        // The sectors whose light this sample sends: its own slab.
        uint lit_sectors = 0u;
        // Native reverse-Z zero is background, not a finite surface slab.
        if (!(sample_depth > 0.0 && sample_z > 0.0 && sample_z <= 1.0e6)) {
          crossed_background = true;
        } else {
          // At the center of the pixel whose depth was read, so a point of the
          // receiver's plane lies on that plane. Rebuilt at the search's own
          // position instead, it would be up to half a pixel off and sloped
          // surfaces would shade themselves. A coarse depth belongs to some
          // pixel of its block, so this is exact for full-resolution samples
          // only.
          const float3 sample_point = ViewPos(float2(sample_pixel), sample_z);
          float3 step_offset = sample_point - previous_point;
          step_offset.xy *= thickness_scale;
          const float toward_camera = dot(step_offset, view_direction);
          if (crossed_background || toward_camera * toward_camera > 0.81 * dot(step_offset, step_offset)) {
            occluder_start = sample_point;
            skipped_run = 0.5 * gap_px * sample_z * gap_scale;
          }
          crossed_background = false;
          previous_point = sample_point;
          const uint2 masks = GetCoverageMask(sample_point, min(thickness_scale * (skipped_run + length(sample_point - occluder_start)), AO_MAX_OCCLUDER_THICKNESS), occluder_start,
                                              position, view_direction, side_sign, normal_angle, cos_n, sector_offset);
          covered = masks.x;
          lit_sectors = masks.y;
        }
        // 5. Gather radiance only for newly covered diffuse sectors. A coarse
        // depth has no color, so descend to its full-size owner first.
        // Sectors the occluder blocks but its sample does not light still
        // block; they send no light.
        uint fresh = covered & lit_sectors & ~occupied;
        if (fresh != 0u) {
          int2 owner;
          if (!ResolveDepthOwner(sample_pixel, mip_level, sample_depth, owner)) {
            gi_valid = false;
          } else {
            if (CUSTOM_MGSV_DIFFUSE_REJECT_BACKFACES > 0.5) {
              const float3 source_normal = DecodeNormal(normal_buffer.Load(int3(owner, 0)).xyz);
              // A backfacing surface still occludes. Only its outgoing radiance
              // is masked, using the reference's projected source hemisphere.
              const float3 source_projected = source_normal - slice_normal * dot(source_normal, slice_normal);
              const float source_length_squared = dot(source_projected, source_projected);
              if (any(!isfinite(source_normal))) {
                gi_valid = false;
                fresh = 0u;
              } else if (source_length_squared > 1e-12) {
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
            if (fresh != 0u) {
              // Black is a valid source. One that is not finite adds no light
              // but still blocks: marking the result invalid instead would
              // void the GI of every receiver whose search reaches that pixel.
              const float3 radiance = LoadTraceSource(owner, sample_depth, prepared_available).rgb;
              if (all(isfinite(radiance))) {
                const float fresh_coverage = float(countbits(fresh)) / float(AO_SECTORS);
                indirect += max(radiance, 0.0) * fresh_coverage;
                accepted_coverage += fresh_coverage;
              }
            }
          }
        }
        // Rejected or invalid source radiance must not make geometry transparent.
        occupied |= covered;
        if (occupied == 0xFFFFFFFFu) break;
      }
    }

    // Importance sampling already weights the slices, so each counts equally.
    visibility += 1.0 - float(countbits(occupied)) / float(AO_SECTORS);
  }

  // 6. Average the slices. Open sectors count as ambient visibility; blocked
  // sectors have contributed their source radiance once.
  const float occlusion = saturate(1.0 - visibility / float(slices));
  const float applied = saturate(occlusion * CUSTOM_MGSV_AMBIENT_OCCLUSION_STRENGTH);
  if (diagnostic == 6) {
    diagnostic_output[ao_pixel] = float4((1.0 - applied).xxx, 1.0);
  }
  // No hits or only black sources give a valid zero. GI validity is stored
  // separately from AO. Clamp only to the RGBA16F range.
  const float3 bounce = indirect / float(slices);
  gi_valid = gi_valid && all(isfinite(bounce));
  lighting_output[ao_pixel] = PackIndirectDiffuse(clamp(bounce, 0.0, 65504.0), gi_valid, 1.0 - applied, true);
  if (diagnostic >= 2 && diagnostic <= 5) {
    float3 value = occlusion.xxx;
    if (diagnostic == 3) value = (accepted_coverage / float(slices)).xxx;
    if (diagnostic == 4) value = gi_valid ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    if (diagnostic == 5) {
      // What the search gathers from this pixel when it is a source.
      value = LoadTraceSource(receiver_pixel, depth, prepared_available).rgb;
      value = all(isfinite(value)) ? clamp(value, 0.0, 65504.0) : float3(1.0, 0.0, 0.0);
    }
    diagnostic_output[ao_pixel] = float4(value, 1.0);
  }
}
