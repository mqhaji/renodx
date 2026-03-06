#include "../common.hlsli"

//-----TONEMAPPING-----//
float3 ApplyVanillaToneMap(float3 color, float white_clip = 8.f) {
  float luma = dot(color, float3(0.27, 0.67, 0.06));
  float tonemapped_luma = renodx::tonemap::ReinhardExtended(luma, white_clip, 1.f, 0.f);
  float scale = (luma > 0.0f) ? (tonemapped_luma / luma) : 1.0f;

  return color * scale;
}

float3 ApplyToneMap(float3 color, float white_clip = 8.f) {
  if (RENODX_TONE_MAP_TYPE == 2.f || RENODX_TONE_MAP_TYPE == 1.f) {
    return color;
  } else {
    return saturate(ApplyVanillaToneMap(color, white_clip));
  }
}

float ComputeReinhardSmoothClampScale(float3 untonemapped, float rolloff_start = 0.18f, float output_max = 1.f) {
  float peak = renodx::math::Max(untonemapped.r, untonemapped.g, untonemapped.b);

  float mapped_peak = renodx::tonemap::ReinhardPiecewise(peak, output_max, rolloff_start);

  float scale = renodx::math::DivideSafe(mapped_peak, peak, 1.f);

  return scale;
}

float ComputeMaxChCompressionScale(float3 color) {
  if (RENODX_TONE_MAP_TYPE == 0.f) {
    return 1.f;
  } else {
    return ComputeReinhardSmoothClampScale(color, 0.1f);
  }
}

float3 Sample2DLUT(
    float3 input_color,
    Texture2D<float4> lut,
    SamplerState lut_sampler) {
  // 32^3 LUT packed into a 2D strip of 32 slices across X.
  const float lut_size = 32.0f;
  const float inv_lut_size = 1.0f / lut_size;  // 0.03125

  const float half_texel_x = 1.0f / 2048.0f;     // 0.00048828125
  const float min_v = 1.0f / 64.0f;              // 0.015625
  const float max_v = 63.0f / 64.0f;             // 0.984375
  const float max_u_in_tile = 63.0f / 2048.0f;   // 0.03076171875
  const float max_slice_offset = 31.0f / 32.0f;  // 0.96875

  // Map green to LUT V with the same bias/clamp behavior.
  float v = clamp(input_color.y + min_v, min_v, max_v);

  // Map red to U within a single slice.
  float u_in_tile = input_color.x * inv_lut_size + half_texel_x;
  u_in_tile = clamp(u_in_tile, half_texel_x, max_u_in_tile);

  // Blue selects the LUT slice, with linear interpolation between floor/ceil slices.
  float slice_pos = lut_size * input_color.z;
  float slice_t = frac(slice_pos);

  float slice_ceil_offset = ceil(slice_pos) * inv_lut_size;
  slice_ceil_offset = clamp(slice_ceil_offset, 0.0f, max_slice_offset);

  float slice_floor_offset = floor(slice_pos) * inv_lut_size;
  slice_floor_offset = clamp(slice_floor_offset, 0.0f, max_slice_offset);

  float2 uv_ceil = float2(u_in_tile + slice_ceil_offset, v);
  float3 color_ceil = lut.Sample(lut_sampler, uv_ceil).xyz;

  float2 uv_floor = float2(u_in_tile + slice_floor_offset, v);
  float3 color_floor = lut.Sample(lut_sampler, uv_floor).xyz;

  return lerp(color_floor, color_ceil, slice_t);
}

float3 Unclamp(float3 original_gamma, float3 black_gamma, float3 mid_gray_gamma, float3 neutral_gamma) {
  const float3 added_gamma = black_gamma;

  const float mid_gray_average = (mid_gray_gamma.r + mid_gray_gamma.g + mid_gray_gamma.b) / 3.f;

  // Remove from 0 to mid-gray
  const float shadow_length = mid_gray_average;
  const float shadow_stop = max(neutral_gamma.r, max(neutral_gamma.g, neutral_gamma.b));
  const float3 floor_remove = added_gamma * max(0, shadow_length - shadow_stop) / shadow_length;

  const float3 unclamped_gamma = max(0, original_gamma - floor_remove);
  return unclamped_gamma;
}

float3 ApplyColorGradingLUT(float3 color_input,
                            Texture2D<float4> lut,
                            SamplerState lut_sampler) {
  float3 color_input_encoded = renodx::color::srgb::EncodeSafe(color_input);
  float3 color_output_encoded = Sample2DLUT(color_input_encoded, lut, lut_sampler);

  if (RENODX_COLOR_GRADE_SCALING > 0.f) {
    float3 lut_black_encoded = Sample2DLUT(0.f, lut, lut_sampler);

    float lut_black_y = renodx::color::y::from::BT709(renodx::color::srgb::Decode(lut_black_encoded));
    if (lut_black_y > 0.f) {
      float3 lut_mid_encoded = Sample2DLUT(max(lut_black_encoded, renodx::color::srgb::EncodeSafe(0.01f)), lut, lut_sampler);

      float3 unclamped_gamma = Unclamp(
          color_output_encoded,
          lut_black_encoded,
          lut_mid_encoded,
          color_input_encoded);

      float3 unclamped_linear = renodx::color::srgb::DecodeSafe(unclamped_gamma);

      float3 color_output_linear = renodx::color::srgb::DecodeSafe(color_output_encoded);
      color_output_linear *= lerp(1.f, renodx::math::DivideSafe(LuminosityFromBT709(unclamped_linear), LuminosityFromBT709(color_output_linear), 1.f), RENODX_COLOR_GRADE_SCALING);

      color_output_encoded = renodx::color::srgb::EncodeSafe(color_output_linear);
    }
  }

  return color_output_encoded;
}

float3 SRGBEncodeAndSample2DLUT(float3 input, Texture2D<float4> g_sBaseColorCorrectionMap, SamplerState g_sBaseColorCorrectionMap_s) {
  float4 r0;
  float3 r1, r2;

  float scale = ComputeMaxChCompressionScale(input);

  float3 input_scaled = input * scale;

  r0.rgb = ApplyColorGradingLUT(input_scaled, g_sBaseColorCorrectionMap, g_sBaseColorCorrectionMap_s);

  r0.rgb = renodx::color::srgb::DecodeSafe(r0.rgb);
  r0.rgb /= scale;
  r0.rgb = lerp(input, r0.rgb, RENODX_COLOR_GRADE_STRENGTH);
  r0.rgb = renodx::color::srgb::EncodeSafe(r0.rgb);

  return r0.rgb;
}
