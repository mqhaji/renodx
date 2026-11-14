#include "../../shared.h"

// Resolve in linear light. Shared input preparation already removed gamma and
// decoded motion. History stays private to D3D12; this pass also encodes the
// completed scene directly into the shared game destination.

Texture2D<float4> current_color_texture : register(t0);
Texture2D<float4> previous_history_texture : register(t1);
Texture2D<float2> velocity_texture : register(t2);
Texture2D<float4> depth_texture : register(t3);
Texture2D<float4> motion_debug_texture : register(t4);

SamplerState point_sampler : register(s0);

groupshared float3 current_tile[100];

RWTexture2D<float4> current_history_output : register(u0);
RWTexture2D<float4> encoded_scene_output : register(u1);

cbuffer TaaResolveConstants : register(b1) {
  float diagnostic_view;
  float velocity_visualization_range;
  float camera_reprojection_valid;
  float object_motion_mode;
  float2 current_jitter_uv;
  float2 previous_jitter_uv;
  float velocity_projection_jitter_scale;
  float clip_tightness;
  float history_clip_strength;
  float current_frame_blend;
  float4 current_to_previous_clip_row_0;
  float4 current_to_previous_clip_row_1;
  float4 current_to_previous_clip_row_2;
  float4 current_to_previous_clip_row_3;
};

struct Neighborhood {
  float3 filtered_color;
  float3 clip_min;
  float3 clip_max;
};

struct CurrentTap {
  float2 offset;
  float weight;
  bool tight_bounds;
};

struct VelocitySelection {
  float2 velocity;
  float object_mask;
  float2 uv;
  float depth;
};

float3 EncodeSceneColor(float3 color) {
  return renodx::color::srgb::Encode(max(0.f.xxx, color));
}

// Filters the current frame and builds broad/tight color bounds for history clipping.
Neighborhood BuildCurrentNeighborhood(uint2 group_thread, float3 center_color) {
  static const float CURRENT_FILTER_EXPONENT = 2.29f;
  static const float CURRENT_DIAGONAL_WEIGHT = exp(-CURRENT_FILTER_EXPONENT * 2.f);
  static const float CURRENT_CARDINAL_WEIGHT = exp(-CURRENT_FILTER_EXPONENT);
  static const float CURRENT_CENTER_WEIGHT = 1.f;
  static const float CURRENT_NORMALIZATION = 1.f / (CURRENT_CENTER_WEIGHT + 4.f * CURRENT_CARDINAL_WEIGHT + 4.f * CURRENT_DIAGONAL_WEIGHT);
  static const float INITIAL_BOUNDS_MIN = 100000.f;
  static const float INITIAL_BOUNDS_MAX = -100000.f;

  // Current filter uses a full 3x3 pixel neighborhood.
  //  a b c
  //  d e f
  //  g h i
  //
  // Tight clipping bounds use the cross only.
  //    b
  //  d e f
  //    h
  static const CurrentTap CURRENT_TAPS[9] = {
      {float2(-1.f, -1.f), CURRENT_DIAGONAL_WEIGHT, false},
      {float2(0.f, -1.f), CURRENT_CARDINAL_WEIGHT, true},
      {float2(1.f, -1.f), CURRENT_DIAGONAL_WEIGHT, false},
      {float2(-1.f, 0.f), CURRENT_CARDINAL_WEIGHT, true},
      {float2(0.f, 0.f), CURRENT_CENTER_WEIGHT, true},
      {float2(1.f, 0.f), CURRENT_CARDINAL_WEIGHT, true},
      {float2(-1.f, 1.f), CURRENT_DIAGONAL_WEIGHT, false},
      {float2(0.f, 1.f), CURRENT_CARDINAL_WEIGHT, true},
      {float2(1.f, 1.f), CURRENT_DIAGONAL_WEIGHT, false},
  };

  float3 filtered_sum = 0.f.xxx;
  float3 broad_min = INITIAL_BOUNDS_MIN.xxx;
  float3 broad_max = INITIAL_BOUNDS_MAX.xxx;
  float3 tight_min = INITIAL_BOUNDS_MIN.xxx;
  float3 tight_max = INITIAL_BOUNDS_MAX.xxx;

  [unroll] for (uint i = 0u; i < 9u; ++i) {
    const CurrentTap tap = CURRENT_TAPS[i];
    const float3 sample_color =
        i == 4u
            ? center_color
            : current_tile[(group_thread.y + 1 + int(tap.offset.y)) * 10 + group_thread.x + 1 + int(tap.offset.x)];

    filtered_sum += sample_color * tap.weight;
    broad_min = min(sample_color, broad_min);
    broad_max = max(sample_color, broad_max);

    if (tap.tight_bounds) {
      tight_min = min(sample_color, tight_min);
      tight_max = max(sample_color, tight_max);
    }
  }

  Neighborhood neighborhood;
  neighborhood.filtered_color = filtered_sum * CURRENT_NORMALIZATION;
  neighborhood.clip_min = lerp(broad_min, tight_min, clip_tightness);
  neighborhood.clip_max = lerp(broad_max, tight_max, clip_tightness);
  return neighborhood;
}

// The optional diagnostic input preserves native UV motion and object coverage.
float2 DecodeVelocity(float2 uv) {
  return motion_debug_texture.SampleLevel(point_sampler, uv, 0).rg;
}

// MGSV uses positive reverse-Z here, so the largest raw depth identifies the
// nearest visible surface. This selection preserves thin foreground geometry
// that was lost when the farther, smallest-absolute-depth sample supplied motion.
VelocitySelection SelectNearestVelocity(
    float2 uv,
    float2 inv_screen_size,
    float2 center_velocity) {
  static const float2 VELOCITY_OFFSETS[5] = {
      float2(0.f, 0.f),
      float2(1.f, 1.f),
      float2(-1.f, 1.f),
      float2(1.f, -1.f),
      float2(-1.f, -1.f),
  };

  VelocitySelection selection;
  selection.velocity = center_velocity;
  selection.object_mask = motion_debug_texture.SampleLevel(point_sampler, uv, 0).b;
  selection.uv = uv;
  selection.depth = depth_texture.SampleLevel(point_sampler, uv, 0).x;

  // Mode 7 isolates spatial selection only: keep Auto's composite motion,
  // but take its velocity, mask and camera depth/UV from the output pixel.
  if (object_motion_mode >= 6.5f) return selection;

  [unroll] for (uint i = 1u; i < 5u; ++i) {
    const float2 candidate_uv = uv + VELOCITY_OFFSETS[i] * inv_screen_size;
    const float candidate_depth = depth_texture.SampleLevel(point_sampler, candidate_uv, 0).x;

    if (candidate_depth >= selection.depth) {
      selection.velocity = DecodeVelocity(candidate_uv);
      selection.object_mask = motion_debug_texture.SampleLevel(point_sampler, candidate_uv, 0).b;
      selection.uv = candidate_uv;
      selection.depth = candidate_depth;
    }
  }

  return selection;
}

float4 CatmullRomWeights(float fraction) {
  const float fraction_squared = fraction * fraction;
  const float fraction_cubed = fraction_squared * fraction;
  return float4(
      fraction_squared - 0.5f * (fraction_cubed + fraction),
      1.f + 1.5f * fraction_cubed - 2.5f * fraction_squared,
      0.5f * fraction + 2.f * fraction_squared - 1.5f * fraction_cubed,
      0.5f * (fraction_cubed - fraction_squared));
}

// Interpolate linear history directly. Keep point reads for the sixteen taps:
// hardware bilinear weights can move a sample across a narrow clipping bound.
// The signed outer lobes preserve detail without decoding each history tap.
float3 SampleHistoryCatmullRom(float2 uv, float2 screen_size) {
  const float2 position = uv * screen_size - 0.5f;
  const int2 base = int2(floor(position));
  const float2 fraction = frac(position);
  const float4 wx = CatmullRomWeights(fraction.x);
  const float4 wy = CatmullRomWeights(fraction.y);
  float3 color = 0.f;
  float weight_sum = 0.f;
  [unroll] for (int y = 0; y < 4; ++y) {
    [unroll] for (int x = 0; x < 4; ++x) {
      const float weight = wx[x] * wy[y];
      const int2 pixel = clamp(base + int2(x - 1, y - 1), 0, int2(screen_size) - 1);
      color += previous_history_texture.Load(int3(pixel, 0)).rgb * weight;
      weight_sum += weight;
    }
  }
  return renodx::math::DivideSafe(color, weight_sum, 0.f.xxx);
}

// Rebuilds camera motion from MGSV's native no-jitter projection/view state.
// The depth came from the jittered frame, but projection jitter changes only
// clip X/Y, so Z/W remains valid. Removing the current offset before applying
// the no-jitter current-to-previous matrix keeps static-camera velocity zero.
bool ComputeMatrixCameraVelocity(float2 uv, float depth, out float2 velocity) {
  const float2 current_ndc =
      uv * float2(2.f, -2.f) + float2(-1.f, 1.f)
      - float2(2.f * current_jitter_uv.x, -2.f * current_jitter_uv.y);
  const float4 current_clip = float4(current_ndc, depth, 1.f);
  const float4 previous_clip = float4(
      dot(current_to_previous_clip_row_0, current_clip),
      dot(current_to_previous_clip_row_1, current_clip),
      dot(current_to_previous_clip_row_2, current_clip),
      dot(current_to_previous_clip_row_3, current_clip));
  if (abs(previous_clip.w) <= 1e-8f) {
    velocity = 0.f.xx;
    return false;
  }

  const float2 current_no_jitter_uv = current_ndc * float2(0.5f, -0.5f) + 0.5f.xx;
  const float2 previous_uv = previous_clip.xy / previous_clip.w * float2(0.5f, -0.5f) + 0.5f.xx;
  velocity = current_no_jitter_uv - previous_uv;
  return all(isfinite(velocity));
}

// Rejects reprojected history samples that land outside normalized screen UVs.
bool IsInsideScreen(float2 uv) {
  return all(abs(uv - 0.5f.xx) < 0.5f.xx);
}

// Clips reprojected history toward the filtered current color inside the neighborhood color box.
float3 ClipHistory(float3 history_color, float3 filtered_color, float3 clip_min, float3 clip_max) {
  const float3 history_delta = history_color - filtered_color;
  const float3 max_scale = renodx::math::DivideSafe(clip_max - filtered_color, history_delta);
  const float3 min_scale = renodx::math::DivideSafe(clip_min - filtered_color, history_delta);
  const float3 clip_scale_rgb = max(min_scale, max_scale);
  const float clip_scale = saturate(renodx::math::Min(clip_scale_rgb));

  return history_delta * clip_scale + filtered_color;
}

// Computes how much clipped history survives based on luma distance and subpixel velocity.
float ComputeHistoryBlend(float3 history_color, float3 clip_min, float3 clip_max, float2 velocity, float2 screen_size) {
  const float history_luma = renodx::color::yf::from::BT709(history_color);
  const float min_luma = renodx::color::yf::from::BT709(clip_min);
  const float max_luma = renodx::color::yf::from::BT709(clip_max);
  const float luma_range = max_luma - min_luma;
  const float luma_edge_factor = renodx::math::DivideSafe(
      min(abs(history_luma - min_luma), abs(history_luma - max_luma)),
      luma_range,
      0.f);

  float2 subpixel_weight = frac(abs(velocity) * screen_size);
  subpixel_weight = 0.5f.xx - abs(subpixel_weight - 0.5f.xx);

  return saturate(current_frame_blend * luma_edge_factor * (1.f + subpixel_weight.x + subpixel_weight.y));
}

[numthreads(8, 8, 1)] void main(uint3 dispatch_thread_id : SV_DispatchThreadID, uint3 group_thread_id : SV_GroupThreadID, uint3 group_id : SV_GroupID) {
  uint screen_width, screen_height;
  current_color_texture.GetDimensions(screen_width, screen_height);
  // Load the 8x8 block and its one-pixel border cooperatively. Edge lanes must
  // reach the barrier even when the image dimensions are not multiples of eight.
  for (uint index = group_thread_id.y * 8u + group_thread_id.x; index < 100u; index += 64u) {
    const int2 pixel = clamp(int2(group_id.xy * 8u) + int2(index % 10u, index / 10u) - 1, 0, int2(screen_width, screen_height) - 1);
    current_tile[index] = current_color_texture.Load(int3(pixel, 0)).rgb;
  }
  GroupMemoryBarrierWithGroupSync();
  if (dispatch_thread_id.x >= screen_width || dispatch_thread_id.y >= screen_height) return;

  const float2 screen_size = float2(screen_width, screen_height);
  const float2 inv_screen_size = 1.f.xx / screen_size;
  const float2 uv = (float2(dispatch_thread_id.xy) + 0.5f.xx) * inv_screen_size;
  const float4 raw_current = current_color_texture.SampleLevel(point_sampler, uv, 0);
  const float3 current_center = raw_current.xyz;

  const float2 raw_velocity = DecodeVelocity(uv);
  const float raw_object_mask = motion_debug_texture.SampleLevel(point_sampler, uv, 0).b;
  const Neighborhood neighborhood = BuildCurrentNeighborhood(group_thread_id.xy, current_center);
  const VelocitySelection velocity_selection =
      SelectNearestVelocity(uv, inv_screen_size, raw_velocity);
  const float2 native_velocity = velocity_selection.velocity;
  float2 matrix_camera_velocity = native_velocity;
  if (camera_reprojection_valid > 0.f) {
    ComputeMatrixCameraVelocity(velocity_selection.uv, velocity_selection.depth, matrix_camera_velocity);
  }

  const float2 velocity = -velocity_texture.SampleLevel(point_sampler, velocity_selection.uv, 0).xy;
  // Do not add previous-current jitter here. That follows the current jittered
  // scene sample instead of accumulating the selected jitter phases at a fixed output
  // pixel, which makes the projection pattern visible as whole-screen shake.
  const float2 history_uv = uv - velocity;

  const float3 history_color = IsInsideScreen(history_uv)
                                   ? SampleHistoryCatmullRom(history_uv, screen_size)
                                   : neighborhood.filtered_color;
  const float3 clipped_history = lerp(
      history_color,
      ClipHistory(
          history_color,
          neighborhood.filtered_color,
          neighborhood.clip_min,
          neighborhood.clip_max),
      history_clip_strength);
  const float blend = ComputeHistoryBlend(
      history_color,
      neighborhood.clip_min, neighborhood.clip_max,
      velocity,
      screen_size);

  const float3 resolved = lerp(clipped_history, neighborhood.filtered_color, blend);
  float3 output_color = resolved;
  bool exact_raw_current = false;
  if (diagnostic_view >= 0.5f && diagnostic_view < 1.5f) {
    exact_raw_current = true;
  } else if (diagnostic_view >= 1.5f && diagnostic_view < 2.5f) {
    output_color = neighborhood.filtered_color;
  } else if (diagnostic_view >= 2.5f && diagnostic_view < 3.5f) {
    const float2 normalized_velocity = raw_velocity * screen_size / max(velocity_visualization_range, 0.01f);
    output_color = saturate(float3(normalized_velocity * 0.5f + 0.5f.xx, 0.5f));
  } else if (diagnostic_view >= 3.5f && diagnostic_view < 4.5f) {
    const float velocity_magnitude = length(raw_velocity * screen_size);
    output_color = saturate(velocity_magnitude / max(velocity_visualization_range, 0.01f)).xxx;
  } else if (diagnostic_view >= 4.5f && diagnostic_view < 5.5f) {
    output_color = raw_object_mask.xxx;
  } else if (diagnostic_view >= 5.5f && diagnostic_view < 6.5f) {
    output_color = velocity_selection.object_mask.xxx;
  } else if (diagnostic_view >= 6.5f && diagnostic_view < 7.5f) {
    const float2 normalized_velocity = velocity * screen_size / max(velocity_visualization_range, 0.01f);
    output_color = saturate(float3(normalized_velocity * 0.5f + 0.5f.xx, 0.5f));
  } else if (diagnostic_view >= 7.5f && diagnostic_view < 8.5f) {
    output_color = saturate(length(velocity * screen_size) / max(velocity_visualization_range, 0.01f)).xxx;
  } else if (diagnostic_view >= 8.5f && diagnostic_view < 9.5f) {
    const float2 residual = native_velocity - matrix_camera_velocity;
    const float2 normalized_residual = residual * screen_size / max(velocity_visualization_range, 0.01f);
    output_color = velocity_selection.object_mask > 0.5f
                       ? saturate(float3(normalized_residual * 0.5f + 0.5f.xx, 0.5f))
                       : 0.5f.xxx;
  } else if (diagnostic_view >= 9.5f) {
    const float residual_magnitude = length((native_velocity - matrix_camera_velocity) * screen_size);
    output_color = velocity_selection.object_mask > 0.5f
                       ? saturate(residual_magnitude / max(velocity_visualization_range, 0.01f)).xxx
                       : 0.f.xxx;
  }

  const float3 linear_output = exact_raw_current ? raw_current.rgb : output_color;
  current_history_output[dispatch_thread_id.xy] = float4(max(0.f.xxx, linear_output), raw_current.a);
  encoded_scene_output[dispatch_thread_id.xy] = float4(EncodeSceneColor(linear_output), raw_current.a);
}
