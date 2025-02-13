#include "./common.hlsl"
#include "./include/CBuffer_DefaultPSC.hlsl"
#include "./include/CBuffer_DefaultXSC.hlsl"
#include "./include/CBuffer_UbershaderXSC.hlsl"

float UpgradeToneMapRatio(float color_hdr, float color_sdr, float post_process_color) {
  [branch]
  if (color_hdr < color_sdr) {
    // If subtracting (user contrast or paperwhite) scale down instead
    // Should only apply on mismatched HDR
    return color_hdr / color_sdr;
  } else {
    float delta = color_hdr - color_sdr;
    delta = max(0, delta);  // Cleans up NaN
    const float ap1_new = post_process_color + delta;

    const bool ap1_valid = (post_process_color > 0);  // Cleans up NaN and ignore black
    return ap1_valid ? (ap1_new / post_process_color) : 0;
  }
}

float3 UpgradeToneMapPerChannel(float3 color_hdr, float3 color_sdr, float3 post_process_color, float post_process_strength, uint working_color_space = 2u, uint hue_processor = 2u) {
  // float ratio = 1.f;

  float3 working_hdr, working_sdr, working_post_process;

  [branch]
  if (working_color_space == 2u) {
    working_hdr = max(0, renodx::color::ap1::from::BT709(color_hdr));
    working_sdr = max(0, renodx::color::ap1::from::BT709(color_sdr));
    working_post_process = max(0, renodx::color::ap1::from::BT709(post_process_color));
  } else
    [branch] if (working_color_space == 1u) {
      working_hdr = max(0, renodx::color::bt2020::from::BT709(color_hdr));
      working_sdr = max(0, renodx::color::bt2020::from::BT709(color_sdr));
      working_post_process = max(0, renodx::color::bt2020::from::BT709(post_process_color));
    }
  else {
    working_hdr = max(0, color_hdr);
    working_sdr = max(0, color_sdr);
    working_post_process = max(0, post_process_color);
  }

  float3 ratio = float3(
      UpgradeToneMapRatio(working_hdr.r, working_sdr.r, working_post_process.r),
      UpgradeToneMapRatio(working_hdr.g, working_sdr.g, working_post_process.g),
      UpgradeToneMapRatio(working_hdr.b, working_sdr.b, working_post_process.b));

  float3 color_scaled = max(0, working_post_process * ratio);

  [branch]
  if (working_color_space == 2u) {
    color_scaled = renodx::color::bt709::from::AP1(color_scaled);
  } else
    [branch] if (working_color_space == 1u) {
      color_scaled = renodx::color::bt709::from::BT2020(color_scaled);
    }

  float peak_correction;
  [branch]
  if (working_color_space == 2u) {
    peak_correction = saturate(1.f - renodx::color::y::from::AP1(working_post_process));
  } else
    [branch] if (working_color_space == 1u) {
      peak_correction = saturate(1.f - renodx::color::y::from::BT2020(working_post_process));
    }
  else {
    peak_correction = saturate(1.f - renodx::color::y::from::BT709(working_post_process));
  }

  [branch]
  if (hue_processor == 2u) {
    color_scaled = renodx::color::correct::HuedtUCS(color_scaled, post_process_color, peak_correction);
  } else
    [branch] if (hue_processor == 1u) {
      color_scaled = renodx::color::correct::HueICtCp(color_scaled, post_process_color, peak_correction);
    }
  else {
    color_scaled = renodx::color::correct::HueOKLab(color_scaled, post_process_color, peak_correction);
  }
  return lerp(color_hdr, color_scaled, post_process_strength);
}

float3 UpgradeToneMapPerceptual(float3 untonemapped, float3 tonemapped, float3 post_processed, float strength) {
  float3 lab_untonemapped = renodx::color::ictcp::from::BT709(untonemapped);
  float3 lab_tonemapped = renodx::color::ictcp::from::BT709(tonemapped);
  float3 lab_post_processed = renodx::color::ictcp::from::BT709(post_processed);

  float3 lch_untonemapped = renodx::color::oklch::from::OkLab(lab_untonemapped);
  float3 lch_tonemapped = renodx::color::oklch::from::OkLab(lab_tonemapped);
  float3 lch_post_processed = renodx::color::oklch::from::OkLab(lab_post_processed);

  float3 lch_upgraded = lch_untonemapped;
  lch_upgraded.xz *= renodx::math::DivideSafe(lch_post_processed.xz, lch_tonemapped.xz, 0.f);

  float3 lab_upgraded = renodx::color::oklab::from::OkLCh(lch_upgraded);

  float c_untonemapped = length(lab_untonemapped.yz);
  float c_tonemapped = length(lab_tonemapped.yz);
  float c_post_processed = length(lab_post_processed.yz);

  if (c_untonemapped > 0) {
    float new_chrominance = c_untonemapped;
    new_chrominance = min(max(c_untonemapped, 0.25f), c_untonemapped * (c_post_processed / c_tonemapped));
    if (new_chrominance > 0) {
      lab_upgraded.yz *= new_chrominance / c_untonemapped;
    }
  }

  float3 upgraded = renodx::color::bt709::from::ICtCp(lab_upgraded);
  return lerp(untonemapped, upgraded, strength);
}

float3 UpgradeToneMap(float3 color_hdr, float3 color_sdr, float3 post_process_color, float post_process_strength) {
  if (injectedData.colorGradeRestorationMethod == 1.f) {
    return UpgradeToneMapPerChannel(color_hdr, color_sdr, post_process_color, post_process_strength);
  } else if (injectedData.colorGradeRestorationMethod == 2.f) {
    return UpgradeToneMapPerceptual(color_hdr, color_sdr, post_process_color, post_process_strength);
  } else {
    return renodx::tonemap::UpgradeToneMap(color_hdr, color_sdr, post_process_color, post_process_strength);
  }
}

float3 applyFilmGrain(float3 input_color, Texture2D<float4> SamplerNoise_TEX,
                      SamplerState SamplerNoise_SMP_s, float4 v1) {
  float3 grained_color;
  if (injectedData.fxFilmGrainType == 0.f) {  // Noise
    float4 r0, r2;
    float3 r1, r3;
    r0.rgb = input_color;

    r1.xyz = SamplerNoise_TEX.Sample(SamplerNoise_SMP_s, v1.xy).xyz;
    r1.xyz = float3(-0.5, -0.5, -0.5) + r1.xyz;
    r0.w = dot(float3(0.298999995, 0.587000012, 0.114), r0.xyz);
    r2.xyz = float3(0, 0.5, 1) + -r0.www;
    r2.xyz = saturate(rp_parameter_ps[7].xyz + -abs(r2.xyz));
    r2.xyz = r2.xyz / rp_parameter_ps[7].xyz;
    r2.xyz = rp_parameter_ps[8].xyz * r2.xyz;
    r3.xyz = r2.yyy * r1.xyz;
    r2.xyw = r1.xyz * r2.xxx + r3.xyz;
    r1.xyz = r1.xyz * r2.zzz + r2.xyw;
    r0.xyz = injectedData.fxFilmGrain * r1.xyz + r0.xyz;

    grained_color = r0.rgb;
  } else {  // Film Grain, applied in linear space
    float3 linear_color = renodx::color::gamma::DecodeSafe(input_color, 2.2f);
    if (injectedData.fxFilmGrainType == 1.f) {  // B&W
      float random_seed = SamplerNoise_TEX.Sample(SamplerNoise_SMP_s, v1.xy).y;
      grained_color = renodx::effects::ApplyFilmGrain(
          linear_color,
          v1.xy,        // Screen-space coordinates
          random_seed,  // Sample noise tex for random seed
          injectedData.fxFilmGrain * .02f);
    } else {  // Colored
      float3 random_seed = SamplerNoise_TEX.Sample(SamplerNoise_SMP_s, v1.xy).rgb;
      grained_color = renodx::effects::ApplyFilmGrainColored(
          linear_color,
          v1.xy,        // Screen-space coordinates
          random_seed,  // Sample noise tex for random seed
          injectedData.fxFilmGrain * .02f);
    }
    grained_color = renodx::color::gamma::EncodeSafe(grained_color, 2.2f);
  }
  return grained_color;
}

renodx::tonemap::config::DualToneMap ToneMap(float3 color, float vanilla_mid_gray) {
  color *= vanilla_mid_gray / 0.18f;  // high mid gray values break tonemapping
  renodx::tonemap::Config config = renodx::tonemap::config::Create();
  config.type = injectedData.toneMapType;
  config.peak_nits = injectedData.toneMapPeakNits;
  config.game_nits = injectedData.toneMapGameNits;
  config.gamma_correction = 0.f;
  config.mid_gray_value = 0.18f;
  config.mid_gray_nits = 0.18f * 100.f;
  config.exposure = injectedData.colorGradeExposure;
  config.shadows = injectedData.colorGradeShadows;
  config.contrast = injectedData.colorGradeContrast;
  config.highlights = injectedData.colorGradeHighlights;
  config.saturation = injectedData.colorGradeSaturation;
  config.hue_correction_type = renodx::tonemap::config::hue_correction_type::INPUT;
  config.hue_correction_strength = injectedData.toneMapHueCorrection;

  // RenoDRT Settings
  config.reno_drt_per_channel = injectedData.toneMapPerChannel != 0;
  config.reno_drt_highlights = 1.2f;
  config.reno_drt_shadows = 0.8f;
  config.reno_drt_contrast = 0.84f;
  config.reno_drt_saturation = 1.f;
  config.reno_drt_blowout = -1.f * (injectedData.colorGradeHighlightSaturation - 1.f);
  config.reno_drt_dechroma = injectedData.colorGradeBlowout;
  config.reno_drt_flare = 0.10f * pow(injectedData.colorGradeFlare, 10.f);
  config.reno_drt_working_color_space = 0u;
  config.reno_drt_tone_map_method = renodx::tonemap::renodrt::config::tone_map_method::DANIELE;
  config.reno_drt_hue_correction_method = renodx::tonemap::renodrt::config::hue_correction_method::OKLAB;

  // ToneMap Blend
  if (injectedData.toneMapBlend) {
    config.exposure = 1.f;
    config.shadows = 1.f;
    config.contrast = 1.f;
    config.saturation = 1.f;
    config.reno_drt_flare = 0.f;
    config.reno_drt_blowout = -1.f * (injectedData.colorGradeHighlightSaturation - 1.f);
    config.reno_drt_dechroma = 0.f;
  }

  renodx::tonemap::Config sdr_config = config;
  sdr_config.reno_drt_highlights /= config.highlights;
  sdr_config.reno_drt_shadows /= config.shadows;
  sdr_config.reno_drt_contrast /= config.contrast;
  sdr_config.gamma_correction = 0;
  sdr_config.peak_nits = 100.f;
  sdr_config.game_nits = 100.f;

  renodx::tonemap::config::DualToneMap dual_tone_map = renodx::tonemap::config::ApplyToneMaps(color, config, sdr_config);
  dual_tone_map.color_sdr = dual_tone_map.color_sdr;

  return dual_tone_map;
}

float3 ToneMapBlend(float3 hdr_color, float3 sdr_color) {
  float3 negHDR = min(0, hdr_color);  // save WCG
  float3 blended_color = lerp(sdr_color, max(0, hdr_color), saturate(sdr_color));
  blended_color += negHDR;  // add back WCG

  return blended_color;
}
