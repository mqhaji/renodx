#include "./shared.h"
#include "./include/ColorGradingLUT.hlsl"
// #include "./DICE.hlsl"

// ---- Created with 3Dmigoto v1.3.16 on Mon Jun 03 21:42:02 2024

struct postfx_luminance_autoexposure_t
{
    float EngineLuminanceFactor;   // Offset:    0
    float LuminanceFactor;         // Offset:    4
    float MinLuminanceLDR;         // Offset:    8
    float MaxLuminanceLDR;         // Offset:   12
    float MiddleGreyLuminanceLDR;  // Offset:   16
    float EV;                      // Offset:   20
    float Fstop;                   // Offset:   24
    uint PeakHistogramValue;       // Offset:   28
};

cbuffer PerInstanceCB : register(b2)
{
  float4 cb_positiontoviewtexture : packoffset(c0);
  float4 cb_postfx_tonemapping_tonemappingparms : packoffset(c1);
  float4 cb_postfx_tonemapping_tonemappingcoeffs1 : packoffset(c2);
  float4 cb_postfx_tonemapping_tonemappingcoeffs0 : packoffset(c3);
  float4 cb_postfx_lensdirt_usedefault : packoffset(c4);
  float2 cb_env_tonemapping_gamma_brightness : packoffset(c5);
  uint2 cb_postfx_luminance_exposureindex : packoffset(c5.z);
  float cb_env_bloom_veil_strength : packoffset(c6);
  float cb_view_white_level : packoffset(c6.y);
  float cb_postfx_luminance_customevbias : packoffset(c6.z);
  float cb_postfx_lensflares_streakwidth : packoffset(c6.w);
  float cb_postfx_lensflares_streakradius : packoffset(c7);
  float cb_postfx_lensflares_streakopacity : packoffset(c7.y);
  float cb_postfx_lensflares_streakoffset : packoffset(c7.z);
  float cb_postfx_bloom_lensdirt_strength : packoffset(c7.w);
  float cb_postfx_bloom_lensdirt_blendweight : packoffset(c8);
  uint cb_postfx_bloom_enabled : packoffset(c8.y);
}

cbuffer PerViewCB : register(b1)
{
  float4 cb_alwaystweak : packoffset(c0);
  float4 cb_viewrandom : packoffset(c1);
  float4x4 cb_viewprojectionmatrix : packoffset(c2);
  float4x4 cb_viewmatrix : packoffset(c6);
  float4 cb_subpixeloffset : packoffset(c10);
  float4x4 cb_projectionmatrix : packoffset(c11);
  float4x4 cb_previousviewprojectionmatrix : packoffset(c15);
  float4x4 cb_previousviewmatrix : packoffset(c19);
  float4x4 cb_previousprojectionmatrix : packoffset(c23);
  float4 cb_mousecursorposition : packoffset(c27);
  float4 cb_mousebuttonsdown : packoffset(c28);
  float4 cb_jittervectors : packoffset(c29);
  float4x4 cb_inverseviewprojectionmatrix : packoffset(c30);
  float4x4 cb_inverseviewmatrix : packoffset(c34);
  float4x4 cb_inverseprojectionmatrix : packoffset(c38);
  float4 cb_globalviewinfos : packoffset(c42);
  float3 cb_wscamforwarddir : packoffset(c43);
  uint cb_alwaysone : packoffset(c43.w);
  float3 cb_wscamupdir : packoffset(c44);
  uint cb_usecompressedhdrbuffers : packoffset(c44.w);
  float3 cb_wscampos : packoffset(c45);
  float cb_time : packoffset(c45.w);
  float3 cb_wscamleftdir : packoffset(c46);
  float cb_systime : packoffset(c46.w);
  float2 cb_jitterrelativetopreviousframe : packoffset(c47);
  float2 cb_worldtime : packoffset(c47.z);
  float2 cb_shadowmapatlasslicedimensions : packoffset(c48);
  float2 cb_resolutionscale : packoffset(c48.z);
  float2 cb_parallelshadowmapslicedimensions : packoffset(c49);
  float cb_framenumber : packoffset(c49.z);
  uint cb_alwayszero : packoffset(c49.w);
}

SamplerState smp_linearclamp_s : register(s0);
Texture2D<float4> ro_postfx_bloom_lensdirt_to : register(t0);
Texture2D<float4> ro_postfx_bloom_lensdirt_from : register(t1);
Texture3D<float4> ro_tonemapping_finalcolorcube : register(t2);
Texture2D<float4> ro_postfx_lensflares_texlensflares : register(t3);
Texture2D<float4> ro_postfx_bloom_texbloom : register(t4);
Texture2D<float4> ro_viewcolormap : register(t5);
StructuredBuffer<postfx_luminance_autoexposure_t> ro_postfx_luminance_buffautoexposure : register(t6);


// 3Dmigoto declarations
#define cmp -

float3 applyUserTonemap(float3 inputColor, float vanillaMidGray) {
  float3 tonemapped;
  if (injectedData.toneMapType == 2) {
    float3 gammaCorrectedColor = renodx::color::correct::GammaSafe(inputColor);

    const float paperWhite = injectedData.toneMapGameNits / renodx::color::srgb::REFERENCE_WHITE;
    const float peakWhite = injectedData.toneMapPeakNits / renodx::color::srgb::REFERENCE_WHITE;
    const float highlightsShoulderStart = paperWhite * vanillaMidGray;
    float3 untonemapped = gammaCorrectedColor;

    untonemapped *= paperWhite;
    tonemapped = renodx::tonemap::dice::BT709(untonemapped, peakWhite, highlightsShoulderStart);
    tonemapped /= paperWhite;

    tonemapped = renodx::color::correct::GammaSafe(tonemapped, true);
  } else {
    const float peakWhite = injectedData.toneMapPeakNits / injectedData.toneMapGameNits;
    if (injectedData.toneMapType == 3) {
      tonemapped = renodx::tonemap::frostbite::BT709(inputColor, renodx::color::correct::GammaSafe(peakWhite, true));
    } else {
      tonemapped = renodx::tonemap::ReinhardScalable(inputColor, peakWhite);
    }
  }

    return tonemapped;
}

float3 applyVanillaTonemap(float3 inputColor)
{
    float4 r0, r1, r2, r3;

    r0.xyz = inputColor;

    // Begin tonemapper calculations
    r1.xyz = cmp(r0.xyz < cb_postfx_tonemapping_tonemappingparms.xxx);
    r2.xyzw = r1.xxxx ? cb_postfx_tonemapping_tonemappingcoeffs0.xyzw : cb_postfx_tonemapping_tonemappingcoeffs1.xyzw;
    r0.xw = r2.xy * r0.xx + r2.zw;
    r2.x = r0.x / r0.w;

    r3.xyzw = r1.yyyy ? cb_postfx_tonemapping_tonemappingcoeffs0.xyzw : cb_postfx_tonemapping_tonemappingcoeffs1.xyzw;
    r0.xy = r3.xy * r0.yy + r3.zw;
    r2.y = r0.x / r0.y;

    r1.xyzw = r1.zzzz ? cb_postfx_tonemapping_tonemappingcoeffs0.xyzw : cb_postfx_tonemapping_tonemappingcoeffs1.xyzw;
    r0.xy = r1.xy * r0.zz + r1.zw;
    r2.z = r0.x / r0.y;

    // The final tonemapped color is stored in r2.xyz
    return r2.xyz;
}



void main(
  float4 v0 : INTERP0,
  float4 v1 : SV_POSITION0,
  out float4 o0 : SV_TARGET0)
{
  float4 r0,r1,r2,r3,r4;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = (uint2)v1.xy;
  r0.zw = float2(0,0);
  r0.xyz = ro_viewcolormap.Load(r0.xyz).xyz;
  r0.w = ro_postfx_luminance_buffautoexposure[cb_postfx_luminance_exposureindex.y].EngineLuminanceFactor;
  r0.w = cb_view_white_level * r0.w;
  r1.xyz = r0.xyz * r0.www;
  r0.xyz = cb_usecompressedhdrbuffers ? r1.xyz : r0.xyz;
  if (cb_postfx_bloom_enabled != 0) {
    r1.xy = cb_resolutionscale.xy * v0.xy;
    r1.z = cmp(0 < cb_postfx_lensdirt_usedefault.x);
    
    r1.z = injectedData.fxLensDirt; // toggle dirty lens effect
    
    if (r1.z != 0) {
      r1.zw = cb_subpixeloffset.xy + v0.xy;
      r2.xyz = ro_postfx_bloom_lensdirt_from.SampleLevel(smp_linearclamp_s, r1.zw, 0).xyz;
    } else {
      r1.zw = cb_subpixeloffset.xy + v0.xy;
      r3.xyz = ro_postfx_bloom_lensdirt_from.SampleLevel(smp_linearclamp_s, r1.zw, 0).xyz;
      r4.xyz = ro_postfx_bloom_lensdirt_to.SampleLevel(smp_linearclamp_s, r1.zw, 0).xyz;
      r4.xyz = r4.xyz + -r3.xyz;
      r2.xyz = cb_postfx_bloom_lensdirt_blendweight * r4.xyz * injectedData.fxLensDirt + r3.xyz;
    }
    r1.z = cmp(0 < cb_env_bloom_veil_strength);
    if (r1.z != 0) {
      r3.xyz = ro_postfx_bloom_texbloom.SampleLevel(smp_linearclamp_s, r1.xy, 0).xyz;
      r4.xyz = r3.xyz * r0.www;
      r3.xyz = cb_usecompressedhdrbuffers ? r4.xyz : r3.xyz;

      float3 vanillaBloom = cb_env_bloom_veil_strength * r3.xyz;
      r3.xyz = injectedData.fxBloom * cb_env_bloom_veil_strength * r3.xyz;  // bloom strength

      if (injectedData.fxBloom != 1) {
        float vanillaBloomLum = renodx::color::y::from::BT709(vanillaBloom);
        r3.xyz = lerp(vanillaBloom.rgb, r3.xyz, saturate(vanillaBloomLum/0.18f));
      }
      
      r4.xyz = cb_postfx_bloom_lensdirt_strength * r2.xyz * injectedData.fxLensDirt; // lens dirt
      r3.xyz = r4.xyz * r3.xyz + r3.xyz;
      r0.xyz = r3.xyz + r0.xyz;
    }
    r1.zw = float2(-0.5,-0.5) + v0.xy;
    r2.w = cb_viewmatrix._m02 + cb_viewmatrix._m21;
    sincos(r2.w, r3.x, r4.x);
    r3.xy = float2(-0.5,0.5) * r3.xx;
    r3.z = r4.x;
    r4.x = dot(r3.zx, r1.zw);
    r4.y = dot(r3.yz, r1.zw);
    r1.z = dot(r4.xy, r4.xy);
    r1.w = min(abs(r4.x), abs(r4.y));
    r2.w = max(abs(r4.x), abs(r4.y));
    r2.w = 1 / r2.w;
    r1.w = r2.w * r1.w;
    r2.w = r1.w * r1.w;
    r3.x = r2.w * 0.0208350997 + -0.0851330012;
    r3.x = r2.w * r3.x + 0.180141002;
    r3.x = r2.w * r3.x + -0.330299497;
    r2.w = r2.w * r3.x + 0.999866009;
    r3.x = r2.w * r1.w;
    r3.y = cmp(abs(r4.y) < abs(r4.x));
    r3.x = r3.x * -2 + 1.57079637;
    r3.x = r3.y ? r3.x : 0;
    r1.w = r1.w * r2.w + r3.x;
    r2.w = cmp(r4.y < -r4.y);
    r2.w = r2.w ? -3.141593 : 0;
    r1.w = r2.w + r1.w;
    r2.w = min(r4.x, r4.y);
    r3.x = max(r4.x, r4.y);
    r2.w = cmp(r2.w < -r2.w);
    r3.x = cmp(r3.x >= -r3.x);
    r2.w = r2.w ? r3.x : 0;
    r1.w = r2.w ? -r1.w : r1.w;
    r1.w = 3.14159274 + r1.w;
    r2.w = 651.898621 * r1.w;
    r2.w = floor(r2.w);
    r3.x = sin(r2.w);
    r3.x = 43758.5469 * r3.x;
    r3.y = 1 + r2.w;
    r3.y = sin(r3.y);
    r3.y = 43758.5469 * r3.y;
    r3.xy = frac(r3.xy);
    r1.w = r1.w * 651.898621 + -r2.w;
    r2.w = r3.y + -r3.x;
    r1.w = r1.w * r2.w + r3.x;
    r1.w = r1.w * cb_postfx_lensflares_streakoffset + cb_postfx_lensflares_streakradius;
    r1.z = -r1.w * r1.w + r1.z;
    r1.z = -cb_postfx_lensflares_streakwidth + abs(r1.z);
    r1.w = 1 / -cb_postfx_lensflares_streakwidth;
    r1.z = saturate(r1.z * r1.w);
    r1.w = r1.z * -2 + 3;
    r1.z = r1.z * r1.z;
    r1.z = r1.w * r1.z;
    r2.xyz = r1.zzz * cb_postfx_lensflares_streakopacity + r2.xyz;
    r1.zw = float2(1,1) / cb_resolutionscale.xy;
    r1.xy = saturate(-cb_positiontoviewtexture.zw * r1.zw + r1.xy);
    r1.xyz = ro_postfx_lensflares_texlensflares.SampleLevel(smp_linearclamp_s, r1.xy, 0).xyz;
    r3.xyz = r1.xyz * r0.www;
    r1.xyz = cb_usecompressedhdrbuffers ? r3.xyz : r1.xyz;
    r2.xyz = r2.xyz * float3(0.800000012,0.800000012,0.800000012) + float3(0.200000003,0.200000003,0.200000003);
    r0.xyz = r1.xyz * r2.xyz + r0.xyz;
  }
  r0.xyz = v0.zzz * r0.xyz; // auto exposure
  float3 untonemapped = r0.xyz;

  float3 outputColor;
  if (injectedData.toneMapType == 0) {  // Vanilla code path
    r1.xyz = cmp(r0.xyz < cb_postfx_tonemapping_tonemappingparms.xxx);
    r2.xyzw = r1.xxxx ? cb_postfx_tonemapping_tonemappingcoeffs0.xyzw : cb_postfx_tonemapping_tonemappingcoeffs1.xyzw;
    r0.xw = r2.xy * r0.xx + r2.zw;
    r2.x = r0.x / r0.w;
    r3.xyzw = r1.yyyy ? cb_postfx_tonemapping_tonemappingcoeffs0.xyzw : cb_postfx_tonemapping_tonemappingcoeffs1.xyzw;
    r0.xy = r3.xy * r0.yy + r3.zw;
    r2.y = r0.x / r0.y;
    r1.xyzw = r1.zzzz ? cb_postfx_tonemapping_tonemappingcoeffs0.xyzw : cb_postfx_tonemapping_tonemappingcoeffs1.xyzw;
    r0.xy = r1.xy * r0.zz + r1.zw;
    r2.z = r0.x / r0.y;
    float3 vanillaTonemap = r2.xyz;

    r0.xyz = vanillaTonemap * float3(31,31,31) + float3(0.5,0.5,0.5);
    r0.xyz = float3(0.03125,0.03125,0.03125) * r0.xyz;
    r0.xyz = ro_tonemapping_finalcolorcube.SampleLevel(smp_linearclamp_s, r0.xyz, 0).xyz;

    outputColor = lerp(vanillaTonemap, r0.xyz, injectedData.colorGradeLUTStrength);
  } else if (injectedData.toneMapType >= 2) { // DICE, Frostbite, Reinhard Scalable

    float3 vanillaTonemap = applyVanillaTonemap(untonemapped);
    
    // apply color grading to vanillaTonemap so blending doesn't break sliders
    float3 adjustedVanillaTonemap = renodx::color::grade::UserColorGrading(
        vanillaTonemap,
        injectedData.colorGradeExposure,
        1.f,                                // highlight only applies to HDR color
        injectedData.colorGradeShadows,
        injectedData.colorGradeContrast,
        injectedData.colorGradeSaturation,
        injectedData.colorGradeBlowout);
    // declare SDR Lutted
    float3 sdrColor = SampleLUT(ro_tonemapping_finalcolorcube, smp_linearclamp_s, adjustedVanillaTonemap, 32u);
    sdrColor = lerp(adjustedVanillaTonemap, sdrColor, injectedData.colorGradeLUTStrength);

    // make HDR tonemap resemble SDR tonemap to facilitate better blending
    untonemapped = renodx::color::grade::UserColorGrading(
        untonemapped,
        injectedData.colorGradeExposure,
        injectedData.colorGradeHighlights * 1.04f,
        injectedData.colorGradeShadows * 1.18f,
        injectedData.colorGradeContrast * 1.2f,
        injectedData.colorGradeSaturation * 1.1f,
        injectedData.colorGradeBlowout,
        injectedData.toneMapHueCorrection,
        vanillaTonemap);
    float vanillaMidGray = renodx::color::y::from::BT709(applyVanillaTonemap(float3(0.18f, 0.18f, 0.18f)));
    untonemapped *= vanillaMidGray / 0.18f;
    
    // apply extrapolated LUT to HDR
    float3 hdrLUTOutput = SampleLUTWithExtrapolation(
        ro_tonemapping_finalcolorcube,        // LUT
        smp_linearclamp_s,                    // samplerState
        untonemapped,                         // neutralColor
        true,                                 // inputLinear
        true,                                 // lutInputLinear
        true,                                 // lutOutputLinear
        true,                                 // outputLinear
        false,                                // gammaCorrectionInput
        false,                                // gammaCorrectionOutput
        true,                                 // lutExtrapolation
        32                                    // lutSize
    );
    hdrLUTOutput = lerp(untonemapped, hdrLUTOutput, injectedData.colorGradeLUTStrength);

    // tonemap HDR Color
    float3 hdrColor = applyUserTonemap(hdrLUTOutput, vanillaMidGray);

    // blend HDR with SDR
    float3 negHDR = min(0, hdrColor); // save WCG
    float3 blendedColor = lerp(saturate(sdrColor), max(0, hdrColor), saturate(sdrColor));
    blendedColor += negHDR; // add back WCG

    if (injectedData.blackFloor) {  // blend back lower black floor from hdrColor
      float3 hdrLab = renodx::color::oklab::from::BT709(hdrColor);
      float3 sdrLab = renodx::color::oklab::from::BT709(sdrColor);

      sdrLab[0] = hdrLab[0];  // apply lightness from hdrColor to sdrColor

      float3 newBlackFloor = renodx::color::bt709::from::OkLab(sdrLab);
      float newBlackFloorLum = renodx::color::y::from::BT709(newBlackFloor);

      negHDR = min(0, blendedColor); // save WCG
      blendedColor = lerp(saturate(newBlackFloor), max(0, blendedColor), saturate(newBlackFloorLum / vanillaMidGray));
      blendedColor += negHDR; // add back WCG
    }

    outputColor = blendedColor;
  } else {  // No Tonemapper
    float3 vanillaTonemap = applyVanillaTonemap(untonemapped);

    untonemapped = renodx::color::grade::UserColorGrading(
        untonemapped,
        injectedData.colorGradeExposure,
        injectedData.colorGradeHighlights,
        injectedData.colorGradeShadows,
        injectedData.colorGradeContrast,
        injectedData.colorGradeSaturation,
        injectedData.colorGradeBlowout,
        injectedData.toneMapHueCorrection,
        vanillaTonemap);
    float3 vanillaMidGray = applyVanillaTonemap(float3(0.18f, 0.18f, 0.18f));
    untonemapped *= vanillaMidGray / 0.18f;
    
    float3 hdrLUTOutput = SampleLUTWithExtrapolation(
        ro_tonemapping_finalcolorcube,        // LUT
        smp_linearclamp_s,                    // samplerState
        untonemapped,                         // neutralColor
        true,                                 // inputLinear
        true,                                 // lutInputLinear
        true,                                 // lutOutputLinear
        true,                                 // outputLinear
        false,                                // gammaCorrectionInput
        false,                                // gammaCorrectionOutput
        true,                                 // lutExtrapolation
        32                                    // lutSize
    );

    outputColor = lerp(untonemapped, hdrLUTOutput, injectedData.colorGradeLUTStrength);
  }

  r0.xyz = cb_env_tonemapping_gamma_brightness.yyy * outputColor;
  o0.xyz = sign(r0.xyz) * pow(abs(r0.xyz), cb_env_tonemapping_gamma_brightness.xxx);
  o0.w = 1;
  return;
}