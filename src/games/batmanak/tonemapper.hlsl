#include "./shared.h"

#define DRAW_TONEMAPPER 0

float3 applyUserToneMap(float3 untonemapped, Texture2D lutTexture, SamplerState lutSampler, float3 correctColor) {
  float3 outputColor = untonemapped;

  float vanillaMidGray = renodx::tonemap::uncharted2::BT709(0.18f, 2.2f);
  float4 vanillaColor;
  vanillaColor.rgb = correctColor.rgb;
  vanillaColor.a = injectedData.toneMapHueCorrection;

  float renoDRTContrast = 1.12f;
  float renoDRTFlare = 0.f;
  float renoDRTShadows = 1.f;
  float renoDRTDechroma = injectedData.colorGradeBlowout;
  float renoDRTSaturation = 1.05f;
  float renoDRTHighlights = 1.2f;

  if (injectedData.toneMapType == 4) {
    renoDRTSaturation = 1.113f;
  }

  outputColor = renodx::tonemap::config::Apply(
      untonemapped,
      renodx::tonemap::config::Create(
          min(3.f, injectedData.toneMapType),
          injectedData.toneMapPeakNits,
          injectedData.toneMapGameNits,
          injectedData.toneMapGammaCorrection - 1,  // -1 == srgb
          injectedData.colorGradeExposure,
          injectedData.colorGradeHighlights,
          injectedData.colorGradeShadows,
          injectedData.colorGradeContrast,
          injectedData.colorGradeSaturation,
          vanillaMidGray,
          vanillaMidGray * 100.f,
          renoDRTHighlights,
          renoDRTShadows,
          renoDRTContrast,
          renoDRTSaturation,
          renoDRTDechroma,
          renoDRTFlare),
      renodx::lut::config::Create(
          lutSampler,
          injectedData.colorGradeLUTStrength,
          0.f,  // Lut scaling not needed
          renodx::lut::config::type::GAMMA_2_2,
          renodx::lut::config::type::GAMMA_2_2,
          16.f),
      lutTexture);

  if (injectedData.toneMapType == 4) {
    float vanillaLum = renodx::color::y::from::BT709(saturate(vanillaColor.rgb));
    outputColor = lerp(vanillaColor.rgb, outputColor, saturate(vanillaLum));  // combine tonemappers
  }

  if (injectedData.toneMapHueCorrection) {
    outputColor = lerp(outputColor, renodx::color::correct::Hue(outputColor, vanillaColor), injectedData.toneMapHueCorrection);
  }

  if (injectedData.toneMapGammaCorrection == 0.f) {
    outputColor = renodx::color::correct::GammaSafe(outputColor, true);
  }

  return outputColor;
}
