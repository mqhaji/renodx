// Game Render + LUT

#include "./shared.h"
#include "./include/ColorGradingLUT.hlsl"

Texture2D<float4> t1 : register(t1);

Texture2D<float4> t0 : register(t0);

SamplerState s1_s : register(s1);

SamplerState s0_s : register(s0);

cbuffer cb0 : register(b0) {
  float4 cb0[7];
}

// 3Dmigoto declarations
#define cmp -

void main(float4 v0 : SV_POSITION0, float4 v1 : TEXCOORD0, float4 v2 : TEXCOORD1, float4 v3 : TEXCOORD2, out float4 o0 : SV_Target0) {
  float4 r0, r1, r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = t0.Sample(s0_s, v3.xy).xyzw;
  o0.w = r0.w;

  r0.xyz = cb0[6].yyy * r0.xyz;  // scale
  float3 untonemapped = r0.rgb;


  float3 outputColor;
  if (injectedData.toneMapType < 4) {
    untonemapped = max(0, renodx::color::bt709::from::SRGB(untonemapped));
    renodx::tonemap::Config config = renodx::tonemap::config::Create();

    config.type = injectedData.toneMapType;
    config.peak_nits = injectedData.toneMapPeakNits;
    config.game_nits = injectedData.toneMapGameNits;
    config.exposure = injectedData.colorGradeExposure;
    config.highlights = injectedData.colorGradeHighlights;
    config.shadows = injectedData.colorGradeShadows;
    config.contrast = injectedData.colorGradeContrast;
    config.saturation = injectedData.colorGradeSaturation;
    config.hue_correction_type = renodx::tonemap::config::hue_correction_type::CUSTOM;
    config.hue_correction_color = lerp(
        untonemapped,
        renodx::tonemap::Reinhard(untonemapped),
        injectedData.toneMapHueCorrection);

    config.reno_drt_contrast = 1.1f;
    config.reno_drt_saturation = 1.05f;
    config.reno_drt_dechroma = injectedData.colorGradeBlowout;

    renodx::lut::Config lut_config = renodx::lut::config::Create(
        s1_s,
        injectedData.colorGradeLUTStrength,
        injectedData.colorGradeLUTScaling,
        renodx::lut::config::type::SRGB,
        renodx::lut::config::type::SRGB,
        32.f);

    if (injectedData.toneMapType == 0.f) {
      untonemapped = saturate(untonemapped);
    }

    outputColor = renodx::tonemap::config::Apply(untonemapped, config, lut_config, t1);
    outputColor = sign(outputColor) * pow(abs(outputColor), 1.f / 2.2f);
  } else {

    // if (injectedData.toneMapHueCorrection) {
    //     float3 correct_color = untonemapped;
    //     float3 incorrect_color = untonemapped;
          
    //     float avg_correct = renodx::math::Average(correct_color);

    //     // Clamp correct_color and calculate average channel values for the clamped version
    //     float3 clamped_correct_color = saturate(correct_color);
    //     float avg_clamped_correct = renodx::math::Average(clamped_correct_color);

    //     // Compute the hue clipping percentage based on the difference in averages
    //     float hue_clip_percentage = saturate((avg_correct - avg_clamped_correct) / max(avg_correct, .0000001f));  // Prevent division by zero

    //     // Interpolate hue components (a, b in OkLab) based on correct_amount using clamped color
    //     float3 correct_lab = renodx::color::oklab::from::BT709(clamped_correct_color);
    //     float3 incorrect_lab = renodx::color::oklab::from::BT709(incorrect_color);
    //     float3 new_lab = incorrect_lab;

    //     // Apply hue correction based on clipping percentage and interpolate based on correct_amount
    //     new_lab.yz = lerp(incorrect_lab.yz, correct_lab.yz, hue_clip_percentage);
    //     new_lab.yz = lerp(incorrect_lab.yz, new_lab.yz, abs(injectedData.toneMapHueCorrection));

    //     // Restore original chrominance from incorrect_color in OkLCh space
    //     float3 incorrect_lch = renodx::color::oklch::from::OkLab(incorrect_lab);
    //     float3 new_lch = renodx::color::oklch::from::OkLab(new_lab);
    //     new_lch[1] = incorrect_lch[1];

    //     // Convert back to linear BT.709 space
    //     untonemapped = renodx::color::bt709::from::OkLCh(new_lch);
    // }


    LUTExtrapolationData extrapolationData = DefaultLUTExtrapolationData();
    extrapolationData.inputColor = untonemapped;
    extrapolationData.vanillaInputColor = saturate(untonemapped);

    LUTExtrapolationSettings extrapolationSettings = DefaultLUTExtrapolationSettings();
    extrapolationSettings.lutSize = 32u;
    extrapolationSettings.inputLinear = false;
    extrapolationSettings.lutInputLinear = false;
    extrapolationSettings.lutOutputLinear = false;
    extrapolationSettings.outputLinear = false;
    extrapolationSettings.transferFunctionIn = LUT_EXTRAPOLATION_TRANSFER_FUNCTION_GAMMA_2_2;
    extrapolationSettings.transferFunctionOut = LUT_EXTRAPOLATION_TRANSFER_FUNCTION_SRGB;
    extrapolationSettings.samplingQuality = 1;
    extrapolationSettings.neutralLUTRestorationAmount = 0;
    // extrapolationSettings.vanillaLUTRestorationAmount = 0;
    extrapolationSettings.vanillaLUTRestorationAmount = injectedData.toneMapHueCorrection;
    extrapolationSettings.enableExtrapolation = true;
    extrapolationSettings.extrapolationQuality = 1;
    // extrapolationSettings.backwardsAmount = 0.5;
    extrapolationSettings.backwardsAmount = injectedData.colorGradeLUTScaling;
    extrapolationSettings.whiteLevelNits = Rec709_WhiteLevelNits;
    extrapolationSettings.inputTonemapToPeakWhiteNits = 0;
    extrapolationSettings.clampedLUTRestorationAmount = 0;
    extrapolationSettings.fixExtrapolationInvalidColors = true;


    outputColor = SampleLUTWithExtrapolation(t1, s1_s, extrapolationData, extrapolationSettings);

    outputColor = lerp(untonemapped, outputColor, injectedData.colorGradeLUTStrength);
  }


  o0.rgb = outputColor.rgb;
  return;
}
