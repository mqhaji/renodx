#include "../../shared.h"

#define CLIP_TIGHTNESS 0.5f

// MGSV temporal resolve.
//
// Inputs (read live off the bindings of the insertion pixel-shader draw):
//   t0 - current scene color (RGBA16F clone, already sRGB-encoded by MGSV)
//   t1 - previous history (RGBA16F, stored sRGB-encoded like the scene color)
//   t2 - camera velocity (RGBA16F preferred, BGRA8 fallback; encoded in .ba)
//   t3 - depth (selects silhouette velocity from center + diagonal taps)
//   t4 - object velocity (RGBA16F clone; .r marks pixels with object motion)
//   s0 - point sampler (current color, velocity, depth, object mask)
//   b0 - resolve diagnostic controls
//
// Output:
//   u0 - current history (RGBA16F, sRGB-encoded RGB plus current scene alpha), copied back into scene color
//        after dispatch

Texture2D<float4> current_color_texture : register(t0);
Texture2D<float4> previous_history_texture : register(t1);
Texture2D<float4> velocity_texture : register(t2);
Texture2D<float4> depth_texture : register(t3);
Texture2D<float4> object_velocity_texture : register(t4);

SamplerState point_sampler : register(s0);

RWTexture2D<float4> current_history_output : register(u0);

cbuffer TaaResolveConstants : register(b0) {
  float diagnostic_view;
  float velocity_visualization_range;
  float camera_reprojection_valid;
  float padding_0;
  float2 current_jitter_uv;
  float2 padding_1;
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

float3 DecodeSceneColor(float3 color) {
  return renodx::color::srgb::Decode(max(0.f.xxx, color));
}

float3 EncodeSceneColor(float3 color) {
  return renodx::color::srgb::Encode(max(0.f.xxx, color));
}

// Filters the current frame and builds broad/tight color bounds for history clipping.
Neighborhood BuildCurrentNeighborhood(float2 uv, float2 inv_screen_size, float3 center_color) {
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
    { float2(-1.f, -1.f), CURRENT_DIAGONAL_WEIGHT, false },
    { float2(0.f, -1.f), CURRENT_CARDINAL_WEIGHT, true },
    { float2(1.f, -1.f), CURRENT_DIAGONAL_WEIGHT, false },
    { float2(-1.f, 0.f), CURRENT_CARDINAL_WEIGHT, true },
    { float2(0.f, 0.f), CURRENT_CENTER_WEIGHT, true },
    { float2(1.f, 0.f), CURRENT_CARDINAL_WEIGHT, true },
    { float2(-1.f, 1.f), CURRENT_DIAGONAL_WEIGHT, false },
    { float2(0.f, 1.f), CURRENT_CARDINAL_WEIGHT, true },
    { float2(1.f, 1.f), CURRENT_DIAGONAL_WEIGHT, false },
  };

  float3 filtered_sum = 0.f.xxx;
  float3 broad_min = INITIAL_BOUNDS_MIN.xxx;
  float3 broad_max = INITIAL_BOUNDS_MAX.xxx;
  float3 tight_min = INITIAL_BOUNDS_MIN.xxx;
  float3 tight_max = INITIAL_BOUNDS_MAX.xxx;

  [unroll]
  for (uint i = 0u; i < 9u; ++i) {
    const CurrentTap tap = CURRENT_TAPS[i];
    const float3 sample_color =
        i == 4u
            ? center_color
            : DecodeSceneColor(current_color_texture.SampleLevel(point_sampler, uv + tap.offset * inv_screen_size, 0).xyz);

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
  neighborhood.clip_min = lerp(broad_min, tight_min, CLIP_TIGHTNESS);
  neighborhood.clip_max = lerp(broad_max, tight_max, CLIP_TIGHTNESS);
  return neighborhood;
}

// Decodes MGSV's camera-velocity buffer (.ba = encoded velocity) into
// a UV-space delta. The velocity does not contain native projection jitter,
// which is what fixed-output-grid temporal accumulation requires: each frame
// contributes a different subpixel sample to the same output pixel.
//
// MGSV's MotionBlurCameraVelocity_ps writes:
//   o0.b = 0.5 + 0.5 * (vNDC.x * m_renderInfo.x / 128)
//   o0.a = 0.5 + 0.5 * (-vNDC.y * m_renderInfo.y / 128)
//
// Decoding to UV space inverts the * (size / 128) scale and the NDC→UV /2
// factor, giving (encoded * 2 - 1) * 64 / screen_size. The shader's Y sign
// flip is cancelled by UV.y being inverted vs NDC.y, so no extra negate.
//
float2 DecodeVelocity(float2 uv, float2 screen_size) {
  const float4 raw = velocity_texture.SampleLevel(point_sampler, uv, 0);
  const float2 encoded = raw.ba;
  return (encoded * 2.f - 1.f) * 64.f / screen_size;
}

// MGSV uses positive reverse-Z here, so the largest raw depth identifies the
// nearest visible surface. This selection preserves thin foreground geometry
// that was lost when the farther, smallest-absolute-depth sample supplied motion.
VelocitySelection SelectNearestVelocity(
    float2 uv,
    float2 inv_screen_size,
    float2 screen_size,
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
  selection.object_mask = object_velocity_texture.SampleLevel(point_sampler, uv, 0).r;
  selection.uv = uv;
  selection.depth = depth_texture.SampleLevel(point_sampler, uv, 0).x;

  [unroll]
  for (uint i = 1u; i < 5u; ++i) {
    const float2 candidate_uv = uv + VELOCITY_OFFSETS[i] * inv_screen_size;
    const float candidate_depth = depth_texture.SampleLevel(point_sampler, candidate_uv, 0).x;

    if (candidate_depth >= selection.depth) {
      selection.velocity = DecodeVelocity(candidate_uv, screen_size);
      selection.object_mask = object_velocity_texture.SampleLevel(point_sampler, candidate_uv, 0).r;
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

// History is stored in MGSV's encoded scene domain. Decode every texel before
// Catmull-Rom interpolation so reconstruction happens in linear light; decoding
// after hardware interpolation visibly damaged thin-detail stability.
float3 SampleHistoryDecodedCatmullRom(float2 uv, float2 screen_size) {
  const float2 texel_position = uv * screen_size - 0.5f.xx;
  const int2 base_pixel = int2(floor(texel_position));
  const float2 fraction = frac(texel_position);
  const int2 maximum_pixel = int2(screen_size) - 1;
  const int4 sample_x = clamp(base_pixel.x + int4(-1, 0, 1, 2), 0, maximum_pixel.x);
  const int4 sample_y = clamp(base_pixel.y + int4(-1, 0, 1, 2), 0, maximum_pixel.y);
  const float4 weight_x = CatmullRomWeights(fraction.x);
  const float4 weight_y = CatmullRomWeights(fraction.y);
  float3 history_color = 0.f.xxx;
  float weight_sum = 0.f;
  [unroll]
  for (int y = 0; y < 4; ++y) {
    [unroll]
    for (int x = 0; x < 4; ++x) {
      const float weight = weight_x[x] * weight_y[y];
      const int2 pixel = int2(sample_x[x], sample_y[y]);
      history_color += DecodeSceneColor(previous_history_texture.Load(int3(pixel, 0)).xyz) * weight;
      weight_sum += weight;
    }
  }
  return renodx::math::DivideSafe(history_color, weight_sum, 0.f.xxx);
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

  return saturate(0.15f * luma_edge_factor * (1.f + subpixel_weight.x + subpixel_weight.y));
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id: SV_DispatchThreadID) {
  uint screen_width, screen_height;
  current_color_texture.GetDimensions(screen_width, screen_height);
  if (dispatch_thread_id.x >= screen_width || dispatch_thread_id.y >= screen_height) return;

  const float2 screen_size = float2(screen_width, screen_height);
  const float2 inv_screen_size = 1.f.xx / screen_size;
  const float2 uv = (float2(dispatch_thread_id.xy) + 0.5f.xx) * inv_screen_size;
  const float4 raw_current = current_color_texture.SampleLevel(point_sampler, uv, 0);
  const float3 current_center = DecodeSceneColor(raw_current.xyz);

  const float2 raw_velocity = DecodeVelocity(uv, screen_size);
  const Neighborhood neighborhood = BuildCurrentNeighborhood(uv, inv_screen_size, current_center);
  const VelocitySelection velocity_selection =
      SelectNearestVelocity(uv, inv_screen_size, screen_size, raw_velocity);
  float2 velocity = velocity_selection.velocity;
  if (camera_reprojection_valid > 0.f && velocity_selection.object_mask <= 0.5f) {
    float2 matrix_camera_velocity = 0.f.xx;
    if (ComputeMatrixCameraVelocity(velocity_selection.uv, velocity_selection.depth, matrix_camera_velocity)) {
      velocity = matrix_camera_velocity;
    }
  }
  // Do not add previous-current jitter here. That follows the current jittered
  // scene sample instead of accumulating the selected jitter phases at a fixed output
  // pixel, which makes the projection pattern visible as whole-screen shake.
  const float2 history_uv = uv - velocity;

  const float3 history_color = IsInsideScreen(history_uv)
                                   ? SampleHistoryDecodedCatmullRom(history_uv, screen_size)
                                   : neighborhood.filtered_color;
  const float3 clipped_history = ClipHistory(
      history_color,
      neighborhood.filtered_color,
      neighborhood.clip_min, neighborhood.clip_max);
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
  }

  current_history_output[dispatch_thread_id.xy] = exact_raw_current
                                                      ? raw_current
                                                      : float4(EncodeSceneColor(output_color), raw_current.a);
}
