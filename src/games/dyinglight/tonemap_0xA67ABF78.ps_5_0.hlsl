#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Sat May 25 22:39:29 2024
Texture2D<float4> t2 : register(t2);

Texture2D<float4> t1 : register(t1);

Texture2D<float4> t0 : register(t0);

SamplerState s2_s : register(s2);

SamplerState s1_s : register(s1);

SamplerState s0_s : register(s0);




// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_POSITION0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_TARGET0)
{
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = t0.SampleLevel(s0_s, v1.xy, 0).xy;
  r0.xy = v1.xy + -r0.xy;
  r0.zw = r0.xy * float2(2,2) + float2(-1,-1);
  r1.xyzw = t2.SampleLevel(s2_s, r0.xy, 0).xyzw;
  r0.x = max(abs(r0.z), abs(r0.w));
  r0.x = cmp(r0.x >= 1);
  r0.y = r1.w * r1.w;
  r2.xyzw = t1.SampleLevel(s1_s, v1.xy, 0).xyzw;

  float3 untonemapped = r2.xyz;
  
  r0.y = r2.w * r2.w + -r0.y;
  r0.y = 0.200000003 * abs(r0.y);
  r0.y = sqrt(r0.y);
  r0.y = -r0.y * 30 + 1;
  r0.y = max(0, r0.y);
  r0.y = 0.5 * r0.y;
  r0.x = r0.x ? 0 : r0.y;
  r0.yzw = -r2.xyz + r1.xyz;
  o0.xyz = r0.xxx * r0.yzw + r2.xyz;
  o0.w = r2.w;
  // vanilla tonemapper causes highlights to flicker, clamped in a later shader

  float3 vanillaColor = o0.rgb;
  float vanillaMidGray = 0.178f;

  if (injectedData.toneMapType == 0) {
    untonemapped = o0.xyz;
  }

  float renoDRTContrast = 1.f;
  float renoDRTFlare = 0.f;
  float renoDRTShadows = 1.f;
  float renoDRTDechroma = injectedData.colorGradeBlowout;
  float renoDRTSaturation = 1.f;
  float renoDRTHighlights = 1.04f;
  if (injectedData.toneMapType != 4) {
    float3 tonemapped = renodx::tonemap::config::Apply(
        untonemapped,
        renodx::tonemap::config::Create(
            injectedData.toneMapType,
            injectedData.toneMapPeakNits,
            injectedData.toneMapGameNits,
            injectedData.toneMapGammaCorrection,
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
            renoDRTFlare,
            renodx::tonemap::config::hue_correction_type::CUSTOM,
            injectedData.toneMapHueCorrection,
            vanillaColor.rgb));
    o0.xyz = tonemapped;
  }
  else {
    float3 tonemapped = renodx::tonemap::config::Apply(
        untonemapped,
        renodx::tonemap::config::Create(
            3.f,
            injectedData.toneMapPeakNits,
            injectedData.toneMapGameNits,
            injectedData.toneMapGammaCorrection - 1,
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
            renodx::tonemap::config::hue_correction_type::CUSTOM,
            injectedData.toneMapHueCorrection,
            vanillaColor.rgb));
    o0.xyz = tonemapped;

    float vanillaLum = renodx::color::y::from::BT709(vanillaColor.rgb);
    o0.xyz = lerp(vanillaColor.rgb, o0.xyz, saturate(vanillaLum));  // combine tonemappers

    // allow for user adjustments
    o0.xyz = renodx::color::grade::UserColorGrading(
        o0.xyz,
        injectedData.colorGradeExposure,
        1.f,
        injectedData.colorGradeShadows,
        injectedData.colorGradeContrast,
        injectedData.colorGradeSaturation);
  }
  return;
}