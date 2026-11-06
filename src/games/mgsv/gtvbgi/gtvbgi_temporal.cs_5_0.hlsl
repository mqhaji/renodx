// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

#include "../gbuffer.hlsli"
#include "../shared.h"
#include "specular_occlusion.hlsli"

// Reuse matching pixels from the previous frame before spatial filtering.
// AO, GI, and specular visibility share motion and geometry checks, but each
// keeps its own history, accepted weight, and missing-data rules.

Texture2D<float4> raw_gi : register(t0);
Texture2D<float4> depth_buffer : register(t1);
Texture2D<float4> normal_buffer : register(t2);
Texture2D<float4> velocity_buffer : register(t3);
Texture2D<float4> object_velocity_buffer : register(t4);
Texture2D<float4> previous_gi : register(t5);
Texture2D<uint2> previous_geometry : register(t6);
Texture2D<float2> raw_ao : register(t7);
Texture2D<float2> previous_ao : register(t8);
Texture2D<float4> raw_specular : register(t9);
Texture2D<float4> previous_specular : register(t10);
RWTexture2D<float4> gi_output : register(u0);
RWTexture2D<uint2> geometry_output : register(u1);
RWTexture2D<float4> diagnostic_output : register(u2);
RWTexture2D<float2> ao_output : register(u3);
RWTexture2D<float4> specular_output : register(u4);

// GPU copies of the native 480-byte scene block. These are scene data, not
// another settings buffer. m_view has different meanings in native passes;
// derive the camera transform from projection_view and projection instead.
struct NativeScene {
  float4x4 projection_view;
  float4x4 projection;
  float4x4 view;
  float4x4 shadow_projection;
  float4x4 shadow_projection2;
  float4 eye_position;
  float4 projection_parameters;
  float4 viewport_size;
  float4 exposure;
  float4 fog_parameters[3];
  float4 fog_color;
  float4 camera_center_offset;
  float4 shadow_map_resolutions;
};

cbuffer CurrentSceneConstants : register(b1) {
  NativeScene current_scene : packoffset(c0);
}

cbuffer PreviousSceneConstants : register(b2) {
  NativeScene previous_scene : packoffset(c0);
}

[numthreads(8, 8, 1)]

void main(uint3 thread_id : SV_DispatchThreadID) {
  uint width, height;
  raw_gi.GetDimensions(width, height);
  const int2 pixel = int2(thread_id.xy);
  if (any(pixel >= int2(width, height))) return;

  const float scale = max(CUSTOM_MGSV_GT_RESOLUTION_DIVISOR, 1.0);
  const float2 full_size = float2(buffer_width, buffer_height);
  const int2 full_pixel = min(int2(floor((float2(pixel) + 0.5) * scale)), int2(full_size) - 1);
  const float4 current_gi = raw_gi.Load(int3(pixel, 0));
  const float2 current_ao = raw_ao.Load(int3(pixel, 0));
  const float depth = depth_buffer.Load(int3(full_pixel, 0)).x;
  const float z = LinearZ(depth);
  const float3 normal = DecodeNormal(normal_buffer.Load(int3(full_pixel, 0)).xyz);
  const bool geometry_valid = depth > 0.0 && z > 0.0 && z < 1.0e6 && all(isfinite(normal));
  const bool gi_valid = geometry_valid && current_gi.a > 0.5 && all(isfinite(current_gi.rgb));
  const bool ao_valid = geometry_valid && current_ao.g > 0.5 && isfinite(current_ao.r);
  const bool specular_active = CUSTOM_MGSV_SPECULAR_ACTIVE >= 0.5;
  float4 current_specular = float4(0.0, 0.0, 0.0, -1.0);
  if (specular_active) {
    current_specular = raw_specular.Load(int3(pixel, 0));
  }
  const bool specular_valid = specular_active && geometry_valid && IsSpecularSampleValid(current_specular);
  if (specular_active) {
    specular_output[pixel] = specular_valid ? current_specular : float4(0.0, 0.0, 0.0, -1.0);
  }
  const float3 current_radiance = gi_valid ? min(current_gi.rgb, 65504.0) : 0.0;
  const float current_visibility = ao_valid ? saturate(current_ao.r) : 1.0;
  const int diagnostic = (int)CUSTOM_MGSV_GT_DEBUG_VIEW;
  gi_output[pixel] = float4(current_radiance, gi_valid ? 1.0 : 0.0);
  ao_output[pixel] = float2(current_visibility, ao_valid ? 1.0 : 0.0);
  geometry_output[pixel] = uint2(0u, 0u);
  if (diagnostic == 6) diagnostic_output[pixel] = float4(current_radiance, 1.0);
  if (diagnostic == 8 || diagnostic == 9) diagnostic_output[pixel] = float4(current_visibility.xxx, 1.0);
  if (diagnostic == 7 || diagnostic == 11) diagnostic_output[pixel] = float4(0.0, 0.0, 0.0, 1.0);
  if (diagnostic == 17) diagnostic_output[pixel] = float4(0.0, 0.0, 0.0, 1.0);
  if (!geometry_valid) return;

  // Store this surface even when one lighting result is missing. Each signal
  // carries its own validity, so missing GI cannot invalidate AO or specular.
  float3 oct = normal / (abs(normal.x) + abs(normal.y) + abs(normal.z));
  if (oct.z < 0.0) {
    oct.xy = (1.0 - abs(oct.yx)) * float2(oct.x >= 0.0 ? 1.0 : -1.0, oct.y >= 0.0 ? 1.0 : -1.0);
  }
  const uint2 packed_normal = uint2(round(saturate(oct.xy * 0.5 + 0.5) * 65535.0));
  geometry_output[pixel] = uint2(asuint(z), packed_normal.x | (packed_normal.y << 16u));

  bool accumulate_gi = gi_valid && CUSTOM_MGSV_GT_TEMPORAL_ENABLED >= 0.5
                       && CUSTOM_MGSV_GI_TEMPORAL_ACTIVE >= 0.5 && CUSTOM_MGSV_GI_HISTORY_VALID >= 0.5
                       && CUSTOM_MGSV_GI_HISTORY_WEIGHT > 0.0;
  const bool accumulate_ao = ao_valid && CUSTOM_MGSV_GT_TEMPORAL_ENABLED >= 0.5
                             && CUSTOM_MGSV_AO_TEMPORAL_ACTIVE >= 0.5 && CUSTOM_MGSV_AO_HISTORY_VALID >= 0.5
                             && CUSTOM_MGSV_OCCLUSION_HISTORY_WEIGHT > 0.0;
  const bool accumulate_specular = specular_valid && CUSTOM_MGSV_GT_TEMPORAL_ENABLED >= 0.5
                                   && CUSTOM_MGSV_AO_TEMPORAL_ACTIVE >= 0.5
                                   && CUSTOM_MGSV_SPECULAR_HISTORY_VALID >= 0.5
                                   && CUSTOM_MGSV_OCCLUSION_HISTORY_WEIGHT > 0.0;
  // Cold or disabled signals keep their current values. No previous texture
  // is read unless at least one signal has usable history.
  [branch]
  if (!accumulate_gi && !accumulate_ao && !accumulate_specular) return;

  float exposure_ratio = 1.0;
  if (accumulate_gi) {
    const float current_exposure = current_scene.exposure.z;
    const float previous_exposure = previous_scene.exposure.z;
    if (current_exposure > 0.0 && previous_exposure > 0.0
        && isfinite(current_exposure) && isfinite(previous_exposure)) {
      exposure_ratio = current_exposure / previous_exposure;
      accumulate_gi = isfinite(exposure_ratio);
    } else {
      accumulate_gi = false;
    }
  }
  if (!accumulate_gi && !accumulate_ao && !accumulate_specular) return;

  // These snapshots must describe the same ordinary perspective view as the
  // scatter's geometry constants. A special-view or stale scene block may be
  // finite while still being incompatible; keep the current sample then.
  if (!all(isfinite(float4(current_scene.projection._m23, current_scene.projection._m33,
                           previous_scene.projection._m23, previous_scene.projection._m33)))
      || abs(current_scene.projection._m23 - 1.0) > 1.0e-5 || abs(current_scene.projection._m33) > 1.0e-5
      || abs(previous_scene.projection._m23 - 1.0) > 1.0e-5 || abs(previous_scene.projection._m33) > 1.0e-5) return;
  const float4 current_projection = float4(1.0 / current_scene.projection._m00,
                                           1.0 / current_scene.projection._m11,
                                           current_scene.projection._m32, current_scene.projection._m22);
  const float4 previous_projection = float4(1.0 / previous_scene.projection._m00,
                                            1.0 / previous_scene.projection._m11,
                                            previous_scene.projection._m32, previous_scene.projection._m22);
  if (!all(isfinite(current_projection)) || !all(isfinite(previous_projection))
      || !all(isfinite(current_scene.projection_parameters)) || !all(isfinite(previous_scene.projection_parameters))
      || any(abs(current_projection - projection_parameters) > 1.0e-3 * max(abs(projection_parameters), 1.0e-5))
      || any(abs(current_scene.projection_parameters - projection_parameters) > 1.0e-3 * max(abs(projection_parameters), 1.0e-5))
      || any(abs(previous_projection - previous_scene.projection_parameters)
             > 1.0e-3 * max(abs(previous_scene.projection_parameters), 1.0e-5))
      || !all(isfinite(current_scene.camera_center_offset.xy)) || !all(isfinite(previous_scene.camera_center_offset.xy))
      || any(abs(current_scene.camera_center_offset.xy - camera_center_offset) > 1.0e-5)) return;

  // Native matrices use row-vector multiplication: projection_view = view *
  // projection. Recover view columns while removing the projection shear.
  const float4 current_z = current_scene.projection_view._m03_m13_m23_m33;
  const float4 current_x = (current_scene.projection_view._m00_m10_m20_m30
                            - current_scene.projection._m20 * current_z)
                           / current_scene.projection._m00;
  const float4 current_y = (current_scene.projection_view._m01_m11_m21_m31
                            - current_scene.projection._m21 * current_z)
                           / current_scene.projection._m11;
  const float4 previous_z = previous_scene.projection_view._m03_m13_m23_m33;
  const float4 previous_x = (previous_scene.projection_view._m00_m10_m20_m30
                             - previous_scene.projection._m20 * previous_z)
                            / previous_scene.projection._m00;
  const float4 previous_y = (previous_scene.projection_view._m01_m11_m21_m31
                             - previous_scene.projection._m21 * previous_z)
                            / previous_scene.projection._m11;
  if (!all(isfinite(current_x)) || !all(isfinite(current_y)) || !all(isfinite(current_z))
      || !all(isfinite(previous_x)) || !all(isfinite(previous_y)) || !all(isfinite(previous_z))) return;
  const float3 current_lengths = float3(dot(current_x.xyz, current_x.xyz), dot(current_y.xyz, current_y.xyz), dot(current_z.xyz, current_z.xyz));
  const float3 previous_lengths = float3(dot(previous_x.xyz, previous_x.xyz), dot(previous_y.xyz, previous_y.xyz), dot(previous_z.xyz, previous_z.xyz));
  const float3 current_angles = float3(dot(current_x.xyz, current_y.xyz), dot(current_x.xyz, current_z.xyz), dot(current_y.xyz, current_z.xyz));
  const float3 previous_angles = float3(dot(previous_x.xyz, previous_y.xyz), dot(previous_x.xyz, previous_z.xyz), dot(previous_y.xyz, previous_z.xyz));
  if (any(abs(current_lengths - 1.0) > 1.0e-3) || any(abs(previous_lengths - 1.0) > 1.0e-3)
      || any(abs(current_angles) > 1.0e-3) || any(abs(previous_angles) > 1.0e-3)) return;

  const float2 current_jitter = float2(CUSTOM_MGSV_GT_CURRENT_JITTER_X, CUSTOM_MGSV_GT_CURRENT_JITTER_Y);
  const float2 previous_jitter = float2(CUSTOM_MGSV_GT_PREVIOUS_JITTER_X, CUSTOM_MGSV_GT_PREVIOUS_JITTER_Y);
  const float3 view_position = ViewPos(float2(full_pixel) - current_jitter * full_size, z);
  const float3 world_position = current_x.xyz * (view_position.x - current_x.w)
                                + current_y.xyz * (view_position.y - current_y.w)
                                + current_z.xyz * (view_position.z - current_z.w);
  const float3 previous_position = float3(dot(float4(world_position, 1.0), previous_x),
                                          dot(float4(world_position, 1.0), previous_y),
                                          dot(float4(world_position, 1.0), previous_z));
  if (!(previous_position.z > 0.0) || !all(isfinite(previous_position))) return;
  const float3 world_normal = current_x.xyz * normal.x + current_y.xyz * normal.y + current_z.xyz * normal.z;
  const float3 previous_normal = normalize(float3(dot(world_normal, previous_x.xyz),
                                                  dot(world_normal, previous_y.xyz),
                                                  dot(world_normal, previous_z.xyz)));
  if (!all(isfinite(previous_normal))) return;

  const float2 previous_ndc = previous_position.xy / previous_position.z / previous_scene.projection_parameters.xy
                              + previous_scene.camera_center_offset.xy;
  float2 previous_pixel = (previous_ndc * float2(0.5, -0.5) + 0.5 + previous_jitter) * full_size;
  const float object_mask = object_velocity_buffer.Load(int3(full_pixel, 0)).r;
  if (object_mask > 0.5) {
    const float2 encoded_velocity = velocity_buffer.Load(int3(full_pixel, 0)).ba;
    const float2 velocity = (encoded_velocity * 2.0 - 1.0) * 64.0 / full_size
                            - current_jitter * CUSTOM_MGSV_GT_VELOCITY_JITTER_SCALE;
    previous_pixel = float2(full_pixel) - velocity * full_size + (previous_jitter - current_jitter) * full_size;
  }
  // Object motion supplies screen displacement, not a previous 3D position or
  // normal. The geometric test remains conservative for motion in depth and
  // deformation; incompatible history falls back to the current samples.
  if (!all(isfinite(previous_pixel)) || any(previous_pixel < 0.0) || any(previous_pixel > full_size - 1.0)) return;

  // Invert the trace's representative-pixel map rather than multiplying UV by
  // ceil(width / scale). The last interval can be shorter at odd dimensions.
  const float center_offset = floor(0.5 * scale);
  float2 history_position = (previous_pixel - center_offset) / scale;
  const int2 last = int2(width, height) - 1;
  const float2 penultimate_pixel = float2(last - 1) * scale + center_offset;
  const float2 last_pixel = min(float2(last) * scale + center_offset, full_size - 1.0);
  [unroll]
  for (int axis = 0; axis < 2; ++axis) {
    if (last[axis] == 0) {
      history_position[axis] = 0.0;
    } else if (previous_pixel[axis] >= penultimate_pixel[axis]) {
      history_position[axis] = float(last[axis] - 1)
                               + (previous_pixel[axis] - penultimate_pixel[axis])
                                     / max(last_pixel[axis] - penultimate_pixel[axis], 1.0);
    }
  }
  const int2 base = int2(floor(history_position));
  const float2 fraction = frac(history_position);
  float3 gi_history_sum = 0.0;
  float ao_history_sum = 0.0;
  float gi_accepted_weight = 0.0;
  float ao_accepted_weight = 0.0;
  float2 specular_history_sum = 0.0;
  float specular_accepted_weight = 0.0;
  float3 current_reflection = 0.0;
  float3 current_lobe = 0.0;
  float current_side = 1.0;
  if (accumulate_specular) {
    const float3 view = normalize(-ViewPos(float2(full_pixel) + 0.5, z));
    current_reflection = reflect(-view, normal);
    current_lobe = GetSpecularLobeParameters(current_specular.z);
    current_side = GetSpecularSurfaceSide(normal, view);
  }
  [unroll]
  for (int i = 0; i < 4; ++i) {
    const int2 offset = int2(i & 1, i >> 1);
    const int2 q = base + offset;
    if (any(q < 0) || any(q > last)) continue;
    const float bilinear = (offset.x != 0 ? fraction.x : 1.0 - fraction.x)
                           * (offset.y != 0 ? fraction.y : 1.0 - fraction.y);
    if (bilinear <= 0.0) continue;
    const uint2 geometry = previous_geometry.Load(int3(q, 0));
    const float history_z = asfloat(geometry.x);
    if (!(history_z > 0.0) || !isfinite(history_z)) continue;

    const float2 oct_xy = float2(geometry.y & 65535u, geometry.y >> 16u) * (2.0 / 65535.0) - 1.0;
    float3 history_normal = float3(oct_xy, 1.0 - abs(oct_xy.x) - abs(oct_xy.y));
    if (history_normal.z < 0.0) {
      history_normal.xy = (1.0 - abs(history_normal.yx))
                          * float2(history_normal.x >= 0.0 ? 1.0 : -1.0, history_normal.y >= 0.0 ? 1.0 : -1.0);
    }
    history_normal = normalize(history_normal);
    const float normal_dot = dot(history_normal, previous_normal);
    const float2 normal_weights = saturate((normal_dot
                                            - (float2(CUSTOM_MGSV_GI_NORMAL_REJECTION, CUSTOM_MGSV_OCCLUSION_NORMAL_REJECTION) - 0.4))
                                           / 0.4);
    const float depth_difference = abs(history_z - previous_position.z);
    const float reference_depth = max(previous_position.z, 1.0e-4);
    if (accumulate_gi && normal_weights.x > 0.0
        && depth_difference <= CUSTOM_MGSV_GI_DEPTH_REJECTION * reference_depth) {
      const float4 history = previous_gi.Load(int3(q, 0));
      if (history.a > 0.5 && all(isfinite(history.rgb))) {
        const float3 exposed_history = history.rgb * exposure_ratio;
        if (all(isfinite(exposed_history))) {
          const float weight = bilinear * normal_weights.x;
          gi_history_sum += exposed_history * weight;
          gi_accepted_weight += weight;
        }
      }
    }
    if (accumulate_ao && normal_weights.y > 0.0
        && depth_difference <= CUSTOM_MGSV_OCCLUSION_DEPTH_REJECTION * reference_depth) {
      const float2 history = previous_ao.Load(int3(q, 0));
      if (history.g > 0.5 && isfinite(history.r)) {
        const float weight = bilinear * normal_weights.y;
        ao_history_sum += history.r * weight;
        ao_accepted_weight += weight;
      }
    }
    if (accumulate_specular && normal_weights.y > 0.0
        && depth_difference <= CUSTOM_MGSV_OCCLUSION_DEPTH_REJECTION * reference_depth) {
      const float4 history = previous_specular.Load(int3(q, 0));
      if (IsSpecularSampleValid(history)) {
        // Reconstruct the old receiver's actual reflected direction, then
        // rotate it into this camera basis. Current view direction cannot
        // stand in for the previous one when a glossy receiver moves.
        const int2 history_pixel = min(int2(floor((float2(q) + 0.5) * scale)), int2(full_size) - 1);
        const float2 history_ndc = ((float2(history_pixel) + 0.5) / full_size - 0.5) * float2(2.0, -2.0);
        const float3 history_position = float3((history_ndc - previous_scene.camera_center_offset.xy)
                                                   * previous_projection.xy * history_z,
                                               history_z);
        const float3 history_view = normalize(-history_position);
        if (GetSpecularSurfaceSide(history_normal, history_view) != current_side) continue;
        const float3 history_reflection = reflect(-history_view, history_normal);
        const float3 world_reflection = previous_x.xyz * history_reflection.x
                                        + previous_y.xyz * history_reflection.y + previous_z.xyz * history_reflection.z;
        const float3 reflection = float3(dot(world_reflection, current_x.xyz),
                                         dot(world_reflection, current_y.xyz), dot(world_reflection, current_z.xyz));
        const float lobe_weight = GetSpecularLobeWeight(current_reflection, current_lobe, reflection, history.z);
        const float weight = bilinear * normal_weights.y * lobe_weight;
        specular_history_sum += history.xy * weight;
        specular_accepted_weight += weight;
      }
    }
  }

  const bool gi_history_valid = gi_accepted_weight > 1.0e-4 && all(isfinite(gi_history_sum));
  const bool ao_history_valid = ao_accepted_weight > 1.0e-4 && isfinite(ao_history_sum);
  const bool specular_history_valid = specular_accepted_weight > 1.0e-4 && all(isfinite(specular_history_sum));
  if (!gi_history_valid && !ao_history_valid && !specular_history_valid) return;
  float3 gi_history = current_radiance;
  float ao_history = current_visibility;
  if (gi_history_valid) gi_history = gi_history_sum / gi_accepted_weight;
  if (ao_history_valid) ao_history = ao_history_sum / ao_accepted_weight;
  float2 specular_history = current_specular.xy;
  if (specular_history_valid) {
    specular_history = specular_history_sum / specular_accepted_weight;
  }

  const bool clamp_gi = gi_history_valid && CUSTOM_MGSV_GI_HISTORY_CLAMP > 0.0;
  const bool clamp_ao = ao_history_valid && CUSTOM_MGSV_AO_HISTORY_CLAMP > 0.0;
  const bool clamp_specular = specular_history_valid && CUSTOM_MGSV_SPECULAR_HISTORY_CLAMP > 0.0;
  if (clamp_gi || clamp_ao || clamp_specular) {
    float3 gi_mean = 0.0;
    float3 gi_squared_difference = 0.0;
    float gi_count = 0.0;
    float ao_mean = 0.0;
    float ao_squared_difference = 0.0;
    float ao_count = 0.0;
    float specular_mean = 0.0;
    float specular_squared_difference = 0.0;
    float specular_open_sum = 0.0;
    const float3 receiver_position = ViewPos(float2(full_pixel), z);
    const float specular_plane_scale = max(0.02 * length(receiver_position), 1e-6);
    [unroll]
    for (int y = -1; y <= 1; ++y) {
      [unroll]
      for (int x = -1; x <= 1; ++x) {
        const int2 q = pixel + int2(x, y);
        if (any(q < 0) || any(q > last)) continue;
        // Welford updates keep FP32 HDR statistics stable. Validity and
        // sample counts remain separate; invalid GI cannot poison AO.
        if (clamp_gi) {
          const float4 sample_value = raw_gi.Load(int3(q, 0));
          if (sample_value.a > 0.5 && all(isfinite(sample_value.rgb))) {
            gi_count += 1.0;
            const float3 difference = sample_value.rgb - gi_mean;
            gi_mean += difference / gi_count;
            gi_squared_difference += difference * (sample_value.rgb - gi_mean);
          }
        }
        if (clamp_ao) {
          const float2 sample_value = raw_ao.Load(int3(q, 0));
          if (sample_value.g > 0.5 && isfinite(sample_value.r)) {
            ao_count += 1.0;
            const float difference = sample_value.r - ao_mean;
            ao_mean += difference / ao_count;
            ao_squared_difference += difference * (sample_value.r - ao_mean);
          }
        }
        if (clamp_specular) {
          const float4 sample_value = raw_specular.Load(int3(q, 0));
          if (IsSpecularSampleValid(sample_value) && sample_value.y > 1e-7) {
            const int2 sample_pixel = min(int2(floor((float2(q) + 0.5) * scale)), int2(full_size) - 1);
            const float sample_depth = depth_buffer.Load(int3(sample_pixel, 0)).x;
            const float sample_z = LinearZ(sample_depth);
            const float3 sample_normal = DecodeNormal(normal_buffer.Load(int3(sample_pixel, 0)).xyz);
            if (sample_depth > 0.0 && sample_z > 0.0 && sample_z < 1e6 && all(isfinite(sample_normal))) {
              const float3 sample_position = ViewPos(float2(sample_pixel), sample_z);
              const float plane = dot(normal, sample_position - receiver_position) / specular_plane_scale;
              const float3 view = normalize(-ViewPos(float2(sample_pixel) + 0.5, sample_z));
              if (GetSpecularSurfaceSide(sample_normal, view) != current_side) continue;
              const float3 reflection = reflect(-view, sample_normal);
              // Four squarings avoid log/exp work in this neighborhood loop.
              float normal_weight = saturate(dot(normal, sample_normal));
              normal_weight *= normal_weight;
              normal_weight *= normal_weight;
              normal_weight *= normal_weight;
              normal_weight *= normal_weight;
              const float weight = exp(-0.5 * plane * plane) * normal_weight
                                   * GetSpecularLobeWeight(current_reflection, current_lobe, reflection, sample_value.z);
              // Weight each visibility ratio by its sampled open area. This
              // gives better-covered neighbors more influence on the clamp.
              const float open_mass = weight * sample_value.y;
              if (open_mass > 0.0) {
                specular_open_sum += open_mass;
                const float ratio = sample_value.x / sample_value.y;
                const float difference = ratio - specular_mean;
                specular_mean += difference * (open_mass / specular_open_sum);
                specular_squared_difference += open_mass * difference * (ratio - specular_mean);
              }
            }
          }
        }
      }
    }
    if (clamp_gi) {
      const float3 sigma = sqrt(max(gi_squared_difference / max(gi_count, 1.0), 0.0));
      gi_history = clamp(gi_history, gi_mean - CUSTOM_MGSV_GI_HISTORY_CLAMP * sigma,
                         gi_mean + CUSTOM_MGSV_GI_HISTORY_CLAMP * sigma);
    }
    if (clamp_ao) {
      const float sigma = sqrt(max(ao_squared_difference / max(ao_count, 1.0), 0.0));
      ao_history = clamp(ao_history, ao_mean - CUSTOM_MGSV_AO_HISTORY_CLAMP * sigma,
                         ao_mean + CUSTOM_MGSV_AO_HISTORY_CLAMP * sigma);
    }
    if (clamp_specular && specular_open_sum > 1e-7 && specular_history.y > 1e-7) {
      const float sigma = sqrt(max(specular_squared_difference / specular_open_sum, 0.0));
      const float ratio = clamp(specular_history.x / specular_history.y,
                                specular_mean - CUSTOM_MGSV_SPECULAR_HISTORY_CLAMP * sigma,
                                specular_mean + CUSTOM_MGSV_SPECULAR_HISTORY_CLAMP * sigma);
      // The sampled open area changes between frames. Clamp visibility only,
      // keeping that area so history can fill gaps in the current samples.
      specular_history.x = saturate(ratio) * specular_history.y;
    }
  }

  if (gi_history_valid) {
    const float blend = saturate(CUSTOM_MGSV_GI_HISTORY_WEIGHT) * saturate(gi_accepted_weight);
    const float3 result = lerp(current_radiance, gi_history, blend);
    if (all(isfinite(result))) {
      gi_output[pixel] = float4(min(result, 65504.0), 1.0);
      if (diagnostic == 6) diagnostic_output[pixel] = float4(result, 1.0);
      if (diagnostic == 7) diagnostic_output[pixel] = float4(saturate(gi_accepted_weight).xxx, 1.0);
    }
  }
  if (ao_history_valid) {
    const float blend = saturate(CUSTOM_MGSV_OCCLUSION_HISTORY_WEIGHT) * saturate(ao_accepted_weight);
    const float result = saturate(lerp(current_visibility, ao_history, blend));
    if (isfinite(result)) {
      ao_output[pixel] = float2(result, 1.0);
      if (diagnostic == 9) diagnostic_output[pixel] = float4(result.xxx, 1.0);
      if (diagnostic == 11) diagnostic_output[pixel] = float4(saturate(ao_accepted_weight).xxx, 1.0);
    }
  }
  if (specular_history_valid) {
    const float blend = saturate(CUSTOM_MGSV_OCCLUSION_HISTORY_WEIGHT) * saturate(specular_accepted_weight);
    const float2 masses = lerp(current_specular.xy, specular_history, blend);
    if (all(isfinite(masses))) {
      specular_output[pixel] = float4(masses, current_specular.zw);
      if (diagnostic == 17) diagnostic_output[pixel] = float4(saturate(specular_accepted_weight).xxx, 1.0);
    }
  }
}
