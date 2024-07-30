#include "./shared.h"

Texture3D<float4> t6 : register(t6);

Texture3D<float4> t5 : register(t5);

Texture3D<float4> t4 : register(t4);

Texture3D<float4> t3 : register(t3);

Texture2D<float4> t0 : register(t0);

SamplerState s6_s : register(s6);

SamplerState s5_s : register(s5);

SamplerState s4_s : register(s4);

SamplerState s3_s : register(s3);

SamplerState s0_s : register(s0);

cbuffer cb2 : register(b2) {
  float4 cb2[2];
}

// 3Dmigoto declarations
#define cmp -

void main(float4 v0 : SV_POSITION0, float2 v1 : TEXCOORD0, out float4 o0 : SV_Target0) {
  float4 r0, r1, r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = t0.Sample(s0_s, v1.xy).xyzw;

  o0.w = r0.w;


  float3 tonemapped;
  if (injectedData.toneMapType != 4) {
  float renoDRTContrast = 1.0f;
  float renoDRTFlare = 0.f;
  float renoDRTShadows = 1.0f;
  float renoDRTDechroma = injectedData.colorGradeBlowout;
  float renoDRTSaturation = 1.0f;
  float renoDRTHighlights = 1.0f;
  float vanillaMidGray = renodx::tonemap::ApplyCurve(0.18f * 2.f, 0.15f, 0.50f, 0.10f, 0.20f, 0.02f, 0.30f)
                         / renodx::tonemap::ApplyCurve(11.2f, 0.15f, 0.50f, 0.10f, 0.20f, 0.02f, 0.30f);

  float4 vanillaColor;
  vanillaColor.rgb = renodx::tonemap::ApplyCurve(r0.xyz * 2.f, 0.15f, 0.50f, 0.10f, 0.20f, 0.02f, 0.30f)
                     / renodx::tonemap::ApplyCurve(11.2f, 0.15f, 0.50f, 0.10f, 0.20f, 0.02f, 0.30f);
  vanillaColor.rgb = saturate(vanillaColor.rgb);
  vanillaColor.a = injectedData.toneMapHueCorrection;
  if (injectedData.toneMapType == 0) {
    vanillaColor.a = 0;
  }

    tonemapped = renodx::tonemap::config::Apply(
        r0.rgb,
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
            vanillaColor));
  }
  else {
    tonemapped = r0.rgb;
  }

  float3 hdrColor = tonemapped;
  float3 sdrColor = saturate(tonemapped);

  renodx::lut::Config lut_config = renodx::lut::config::Create(
      s4_s,
      injectedData.colorGradeLUTStrength,
      injectedData.colorGradeLUTScaling,  // Cleans up raised black floor
      renodx::lut::config::type::GAMMA_2_2,
      renodx::lut::config::type::GAMMA_2_2,
      16);

  float3 outputColor = hdrColor;
  if (injectedData.colorGradeLUTStrength != 0.f && injectedData.toneMapType != 1.f) {
    r0.xyz = sdrColor;
    
    r1.xyz = renodx::lut::Sample(t4, lut_config, r0.xyz); // r1.xyz = t4.Sample(s4_s, r0.xyz).xyz;

    r1.xyz = cb2[1].yyy * r1.xyz;

    lut_config.lut_sampler = s3_s;
    r2.xyz = renodx::lut::Sample(t3, lut_config, r0.xyz); // r2.xyz = t3.Sample(s3_s, r0.xyz).xyz;

    r1.xyz = r2.xyz * cb2[1].xxx + r1.xyz;

    lut_config.lut_sampler = s5_s;
    r2.xyz = renodx::lut::Sample(t5, lut_config, r0.xyz); // r2.xyz = t5.Sample(s5_s, r0.xyz).xyz;
    
    lut_config.lut_sampler = s6_s;
    r0.xyz = renodx::lut::Sample(t6, lut_config, r0.xyz); // r0.xyz = t6.Sample(s6_s, r0.xyz).xyz;

    r1.xyz = r2.xyz * cb2[1].zzz + r1.xyz;
    float3 postProcessColor = r0.xyz * cb2[1].www + r1.xyz;

    if (injectedData.toneMapType == 0.f) {
      outputColor = lerp(outputColor, postProcessColor, lut_config.strength);
    } else {
      outputColor = renodx::tonemap::UpgradeToneMap(hdrColor, sdrColor, postProcessColor, lut_config.strength);
    }
  }
  o0.rgb = outputColor;
  if (injectedData.fxFilmGrain) {
    float3 grainedColor = renodx::effects::ApplyFilmGrain(
        o0.rgb,
        v1.xy,
        frac(injectedData.elapsedTime / 1000.f),
        injectedData.fxFilmGrain * 0.03f,
        1.f);
    o0.xyz = grainedColor;
  }

  if (injectedData.toneMapGammaCorrection == 0) {
    o0.rgb = renodx::color::correct::GammaSafe(o0.rgb, true);
  }

  o0.rgb *= injectedData.toneMapGameNits / 80.f;
  return;
}
