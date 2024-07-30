#include "./shared.h"

float3 applyUserToneMap(float3 untonemapped, float3 correctColor, float toeNum) {

    float4 vanillaColor;
    vanillaColor.rgb = correctColor.rgb;
    vanillaColor.a = injectedData.toneMapHueCorrection;

    float vanillaMidGray = renodx::tonemap::ApplyCurve(0.18f * 2.f, 0.15f, 0.50f, 0.10f, 0.20f, toeNum, 0.30f)
                            / renodx::tonemap::ApplyCurve(11.2f, 0.15f, 0.50f, 0.10f, 0.20f, toeNum, 0.30f);

    float renoDRTContrast = 1.0f;
    float renoDRTFlare = 0.f;
    float renoDRTShadows = 1.0f;
    float renoDRTDechroma = injectedData.colorGradeBlowout;
    float renoDRTSaturation = 1.0f;
    float renoDRTHighlights = 1.0f;
    float3 tonemapped;
    tonemapped = renodx::tonemap::config::Apply(
        untonemapped.rgb,
        renodx::tonemap::config::Create(
            3.f,
            injectedData.toneMapPeakNits,
            injectedData.toneMapGameNits,
            injectedData.toneMapGammaCorrection,
            1.f,
            injectedData.colorGradeHighlights,
            1.f,
            1.f,
            1.f,
            vanillaMidGray,
            vanillaMidGray * 100.f,
            renoDRTHighlights,
            renoDRTShadows,
            renoDRTContrast,
            renoDRTSaturation,
            renoDRTDechroma,
            renoDRTFlare,
            vanillaColor));

    float3 negHDR = min(0, tonemapped);
    tonemapped = lerp(vanillaColor.rgb, max(0, tonemapped), saturate(vanillaColor.rgb));  // combine tonemappers
    tonemapped += negHDR;

    // allow for user adjustments
    tonemapped = renodx::color::grade::UserColorGrading(
        tonemapped,
        injectedData.colorGradeExposure,
        1.f,
        injectedData.colorGradeShadows,
        injectedData.colorGradeContrast,
        injectedData.colorGradeSaturation);


    return tonemapped;
}