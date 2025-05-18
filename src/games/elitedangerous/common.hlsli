#include "./shared.h"

/// Elite Dangerous vanilla SDR tonemapper.
/// Output is in gamma space.
#define APPLY_VANILLA_TONEMAP_GENERATOR(T)                        \
  T ApplyVanillaTonemap(T untonemapped) {                         \
    const float N0 = 8.46800041f;                                 \
    const float N1 = 1.0f;                                        \
    const float N2 = -0.00295699993f;                             \
    const float N3 = 0.000100400001f;                             \
    const float N4 = -1.274e-7f;                                  \
                                                                  \
    const float D0 = 8.3604002f;                                  \
    const float D1 = 1.82270002f;                                 \
    const float D2 = 0.218899995f;                                \
    const float D3 = -0.00211700005f;                             \
    const float D4 = 3.67300017e-5f;                              \
                                                                  \
    T x = untonemapped;                                           \
    T numerator = (((x * N0 + N1) * x + N2) * x + N3) * x + N4;   \
    T denominator = (((x * D0 + D1) * x + D2) * x + D3) * x + D4; \
    return max((T)0.0f, numerator / denominator);                 \
  }

APPLY_VANILLA_TONEMAP_GENERATOR(float)
APPLY_VANILLA_TONEMAP_GENERATOR(float3)
#undef APPLY_VANILLA_TONEMAP_GENERATOR

float3 GameScaleAndGrainAndDisplayMap(float3 color, float2 screen_position) {
  color = renodx::color::gamma::DecodeSafe(color, 2.2f);
  color = renodx::effects::ApplyFilmGrain(
      color,
      screen_position,
      CUSTOM_RANDOM,
      CUSTOM_GRAIN_STRENGTH * 0.03f,
      1.f);
  if (RENODX_TONE_MAP_TYPE == 3.f) {
    color = max(0, renodx::color::bt2020::from::BT709(color));
    color = exp2(renodx::tonemap::ExponentialRollOff(log2(color * RENODX_DIFFUSE_WHITE_NITS), log2(RENODX_PEAK_WHITE_NITS * 0.375f), log2(RENODX_PEAK_WHITE_NITS))) / RENODX_DIFFUSE_WHITE_NITS;
    color = renodx::color::bt709::from::BT2020(color);
  }

  color *= RENODX_DIFFUSE_WHITE_NITS / RENODX_GRAPHICS_WHITE_NITS;
  color = renodx::color::gamma::EncodeSafe(color, 2.2f);
  return color;
}

float3 GameScale(float3 color) {
  color = renodx::color::gamma::DecodeSafe(color, 2.2f);
  color *= RENODX_DIFFUSE_WHITE_NITS / RENODX_GRAPHICS_WHITE_NITS;
  color = renodx::color::gamma::EncodeSafe(color, 2.2f);
  return color;
}

float3 FinalizeOutput(float3 color) {
  color = renodx::color::gamma::DecodeSafe(color, 2.2f);
  color = renodx::color::bt709::clamp::AP1(color);
  color *= RENODX_GRAPHICS_WHITE_NITS;

  color /= 80.f;
  return color;
}

/// Applies Exponential Roll-Off tonemapping using the maximum channel.
/// Used to fit the color into a 0–output_max range for SDR LUT compatibility.
float3 ToneMapMaxCLL(float3 color, float rolloff_start = 0.375f, float output_max = 1.f) {
  color = min(color, 100.f);
  float peak = max(color.r, max(color.g, color.b));
  peak = min(peak, 100.f);
  float log_peak = log2(peak);

  // Apply exponential shoulder in log space
  float log_mapped = renodx::tonemap::ExponentialRollOff(log_peak, log2(rolloff_start), log2(output_max));
  float scale = exp2(log_mapped - log_peak);  // How much to compress all channels

  return min(output_max, color * scale);
}

void ApplyUserDualToneMap(float3 untonemapped, inout float3 color_hdr, inout float3 color_sdr) {
  const float vanilla_mid_gray = pow(ApplyVanillaTonemap(0.18f), 2.2f);  // ~ 0.28f

  if (RENODX_TONE_MAP_TYPE == 2.f) {
    const float ACES_MIN = 0.0001f;
    const float ACES_MID_GRAY = 0.10f;
    const float MID_GRAY_SCALE = vanilla_mid_gray / ACES_MID_GRAY;

    float aces_min = ACES_MIN / RENODX_DIFFUSE_WHITE_NITS / MID_GRAY_SCALE;
    float aces_max = (RENODX_PEAK_WHITE_NITS / RENODX_DIFFUSE_WHITE_NITS) / MID_GRAY_SCALE;

    // exposure, highlights, shadows, contrast
    untonemapped = renodx::color::grade::UserColorGrading(
        untonemapped,
        RENODX_TONE_MAP_EXPOSURE,
        RENODX_TONE_MAP_HIGHLIGHTS,
        RENODX_TONE_MAP_SHADOWS,
        RENODX_TONE_MAP_CONTRAST,
        1.f,
        0.f,
        0.f);

    color_hdr = renodx::tonemap::aces::ODT(renodx::color::ap1::from::BT709(untonemapped), aces_min * 48.f, aces_max * 48.f) / 48.f * MID_GRAY_SCALE;
    color_sdr = renodx::tonemap::aces::ODT(renodx::color::ap1::from::BT709(untonemapped), aces_min * 48.f, 48.f / MID_GRAY_SCALE) / 48.f * MID_GRAY_SCALE;

    color_hdr =
        renodx::color::grade::UserColorGrading(
            color_hdr,
            1.f,
            1.f,
            1.f,
            1.f,
            RENODX_TONE_MAP_SATURATION,
            RENODX_TONE_MAP_BLOWOUT,
            RENODX_TONE_MAP_HUE_CORRECTION,
            untonemapped);

    color_sdr =
        renodx::color::grade::UserColorGrading(
            color_sdr,
            1.f,
            1.f,
            1.f,
            1.f,
            RENODX_TONE_MAP_SATURATION,
            RENODX_TONE_MAP_BLOWOUT,
            RENODX_TONE_MAP_HUE_CORRECTION,
            untonemapped);
    color_sdr = min(1.f, color_sdr);

  } else {
    renodx::tonemap::Config config = renodx::tonemap::config::Create();
    config.type = RENODX_TONE_MAP_TYPE;
    config.peak_nits = 10000.f;
    config.game_nits = 100.f;
    config.gamma_correction = 0.f;
    config.mid_gray_value = vanilla_mid_gray;
    config.mid_gray_nits = config.mid_gray_value * 100.f;
    config.exposure = RENODX_TONE_MAP_EXPOSURE;
    config.highlights = RENODX_TONE_MAP_HIGHLIGHTS;
    config.shadows = RENODX_TONE_MAP_SHADOWS;
    config.contrast = RENODX_TONE_MAP_CONTRAST;
    config.saturation = RENODX_TONE_MAP_SATURATION;

    // RenoDRT Settings
    config.reno_drt_per_channel = RENODX_TONE_MAP_PER_CHANNEL != 0;
    config.reno_drt_highlights = 0.795f;
    config.reno_drt_contrast = 1.24f;
    config.reno_drt_saturation = 1.2f;
    config.reno_drt_flare = 0.00125 * pow(RENODX_TONE_MAP_FLARE, 10.f);
    config.reno_drt_shadows = 0.575f;
    config.reno_drt_working_color_space = 0u;
    config.reno_drt_blowout = RENODX_TONE_MAP_HIGHLIGHT_SATURATION;
    config.reno_drt_dechroma = RENODX_TONE_MAP_BLOWOUT;
    config.reno_drt_tone_map_method = renodx::tonemap::renodrt::config::tone_map_method::REINHARD;
    config.reno_drt_hue_correction_method = renodx::tonemap::renodrt::config::hue_correction_method::OKLAB;
    config.hue_correction_type = renodx::tonemap::config::hue_correction_type::INPUT;
    config.hue_correction_strength = RENODX_TONE_MAP_HUE_CORRECTION;

    float3 tonemapped = renodx::tonemap::config::Apply(untonemapped, config);

    color_hdr = tonemapped;
    color_sdr = renodx::color::bt709::from::BT2020(ToneMapMaxCLL(max(0, renodx::color::bt2020::from::BT709(tonemapped))));
  }

  return;
}
