#ifndef SRC_AVOWED_COMMON_HLSLI_
#define SRC_AVOWED_COMMON_HLSLI_
#include "./shared.h"

float3 ApplyUnrealDefaultACES(float3 untonemapped_ap1, float _767, float _768, float _769) {
  // Color Correction controls
  static const float BlueCorrection = 0.6f;
  static const float ToneCurveAmount = 1.0;

  // ACES settings
  static const float FilmSlope = 0.88f;
  static const float FilmToe = 0.55f;
  static const float FilmShoulder = 0.26f;
  static const float FilmBlackClip = 0.0f;
  static const float FilmWhiteClip = 0.04f;

  float _898 = untonemapped_ap1.x;
  float _899 = untonemapped_ap1.y;
  float _900 = untonemapped_ap1.z;
  float _938;

  float _901 = dot(float3(_898, _899, _900), float3(0.2722287178039551f, 0.6740817427635193f, 0.053689517080783844f));
  float _916 = (FilmBlackClip + 1.0f) - FilmToe;
  float _918 = FilmWhiteClip + 1.0f;
  float _920 = _918 - FilmShoulder;
  if (FilmToe > 0.800000011920929f) {
    _938 = (((0.8199999928474426f - FilmToe) / FilmSlope) + -0.7447274923324585f);
  } else {
    float _929 = (FilmBlackClip + 0.18000000715255737f) / _916;
    _938 = (-0.7447274923324585f - ((log2(_929 / (2.0f - _929)) * 0.3465735912322998f) * (_916 / FilmSlope)));
  }
  float _941 = ((1.0f - FilmToe) / FilmSlope) - _938;
  float _943 = (FilmShoulder / FilmSlope) - _941;
  float _947 = log2(lerp(_901, _898, 0.9599999785423279f)) * 0.3010300099849701f;
  float _948 = log2(lerp(_901, _899, 0.9599999785423279f)) * 0.3010300099849701f;
  float _949 = log2(lerp(_901, _900, 0.9599999785423279f)) * 0.3010300099849701f;
  float _953 = FilmSlope * (_947 + _941);
  float _954 = FilmSlope * (_948 + _941);
  float _955 = FilmSlope * (_949 + _941);
  float _956 = _916 * 2.0f;
  float _958 = (FilmSlope * -2.0f) / _916;
  float _959 = _947 - _938;
  float _960 = _948 - _938;
  float _961 = _949 - _938;
  float _980 = _920 * 2.0f;
  float _982 = (FilmSlope * 2.0f) / _920;
  float _1007 = select((_947 < _938), ((_956 / (exp2((_959 * 1.4426950216293335f) * _958) + 1.0f)) - FilmBlackClip), _953);
  float _1008 = select((_948 < _938), ((_956 / (exp2((_960 * 1.4426950216293335f) * _958) + 1.0f)) - FilmBlackClip), _954);
  float _1009 = select((_949 < _938), ((_956 / (exp2((_961 * 1.4426950216293335f) * _958) + 1.0f)) - FilmBlackClip), _955);
  float _1016 = _943 - _938;
  float _1020 = saturate(_959 / _1016);
  float _1021 = saturate(_960 / _1016);
  float _1022 = saturate(_961 / _1016);
  bool _1023 = (_943 < _938);
  float _1027 = select(_1023, (1.0f - _1020), _1020);
  float _1028 = select(_1023, (1.0f - _1021), _1021);
  float _1029 = select(_1023, (1.0f - _1022), _1022);
  float _1048 = (((_1027 * _1027) * (select((_947 > _943), (_918 - (_980 / (exp2(((_947 - _943) * 1.4426950216293335f) * _982) + 1.0f))), _953) - _1007)) * (3.0f - (_1027 * 2.0f))) + _1007;
  float _1049 = (((_1028 * _1028) * (select((_948 > _943), (_918 - (_980 / (exp2(((_948 - _943) * 1.4426950216293335f) * _982) + 1.0f))), _954) - _1008)) * (3.0f - (_1028 * 2.0f))) + _1008;
  float _1050 = (((_1029 * _1029) * (select((_949 > _943), (_918 - (_980 / (exp2(((_949 - _943) * 1.4426950216293335f) * _982) + 1.0f))), _955) - _1009)) * (3.0f - (_1029 * 2.0f))) + _1009;
  float _1051 = dot(float3(_1048, _1049, _1050), float3(0.2722287178039551f, 0.6740817427635193f, 0.053689517080783844f));
  float _1071 = (ToneCurveAmount * (max(0.0f, (lerp(_1051, _1048, 0.9300000071525574f))) - _767)) + _767;
  float _1072 = (ToneCurveAmount * (max(0.0f, (lerp(_1051, _1049, 0.9300000071525574f))) - _768)) + _768;
  float _1073 = (ToneCurveAmount * (max(0.0f, (lerp(_1051, _1050, 0.9300000071525574f))) - _769)) + _769;
  float _1089 = ((mad(-0.06537103652954102f, _1073, mad(1.451815478503704e-06f, _1072, (_1071 * 1.065374732017517f))) - _1071) * BlueCorrection) + _1071;
  float _1090 = ((mad(-0.20366770029067993f, _1073, mad(1.2036634683609009f, _1072, (_1071 * -2.57161445915699e-07f))) - _1072) * BlueCorrection) + _1072;
  float _1091 = ((mad(0.9999996423721313f, _1073, mad(2.0954757928848267e-08f, _1072, (_1071 * 1.862645149230957e-08f))) - _1073) * BlueCorrection) + _1073;

  float3 tonemapped_ap1 = float3(_1089, _1090, _1091);
  return tonemapped_ap1;
}

float3 ApplyACESToneMap(float3 untonemapped_ap1) {
  const float ACES_MIN = 0.0001f;
  float aces_min = ACES_MIN / RENODX_DIFFUSE_WHITE_NITS;
  float aces_max = (RENODX_PEAK_WHITE_NITS / RENODX_DIFFUSE_WHITE_NITS);

  if (true) {  // gamma correction
    aces_max = renodx::color::correct::Gamma(aces_max, true);
    aces_min = renodx::color::correct::Gamma(aces_min, true);
  }

  // exposure, highlights, shadows, contrast
  if (RENODX_TONE_MAP_EXPOSURE != 1.f || RENODX_TONE_MAP_HIGHLIGHTS != 1.f || RENODX_TONE_MAP_SHADOWS != 1.f || RENODX_TONE_MAP_CONTRAST != 1.f) {
    untonemapped_ap1 = renodx::color::ap1::from::BT709(
        renodx::color::grade::UserColorGrading(
            renodx::color::bt709::from::AP1(untonemapped_ap1),
            RENODX_TONE_MAP_EXPOSURE,
            RENODX_TONE_MAP_HIGHLIGHTS,
            RENODX_TONE_MAP_SHADOWS,
            RENODX_TONE_MAP_CONTRAST,
            1.f,
            0.f,
            0.f));
  }

  float3 tonemapped_ap1 = renodx::tonemap::aces::ODT(untonemapped_ap1, aces_min * 48.f, aces_max * 48.f, renodx::color::IDENTITY_MAT) / 48.f;

  // saturation, blowout, hue correction
  if (RENODX_TONE_MAP_SATURATION != 1.f || RENODX_TONE_MAP_BLOWOUT != 0.f || RENODX_TONE_MAP_HUE_CORRECTION != 0.f) {
    tonemapped_ap1 = renodx::color::ap1::from::BT709(
        renodx::color::grade::UserColorGrading(
            renodx::color::bt709::from::AP1(tonemapped_ap1),
            1.f,
            1.f,
            1.f,
            1.f,
            RENODX_TONE_MAP_SATURATION,
            RENODX_TONE_MAP_BLOWOUT,
            RENODX_TONE_MAP_HUE_CORRECTION,
            renodx::color::bt709::from::AP1(untonemapped_ap1)));
  }

  return tonemapped_ap1;
}

float3 DeltaToneMap(float3 untonemapped_ap1, float3 vanilla_tonemapped_ap1, float _767, float _768, float _769) {
  float3 aces_tonemapped_ap1 = ApplyACESToneMap(untonemapped_ap1);
  float3 unreal_default_aces_ap1 = ApplyUnrealDefaultACES(untonemapped_ap1, _767, _768, _769);

  float3 aces_tonemapped_bt709 = renodx::color::bt709::from::AP1(aces_tonemapped_ap1);
  float3 unreal_default_aces_bt709 = renodx::color::bt709::from::AP1(unreal_default_aces_ap1);
  float3 vanilla_tonemapped_bt709 = renodx::color::bt709::from::AP1(vanilla_tonemapped_ap1);

  float3 upgraded_tonemapped_bt709 = renodx::tonemap::UpgradeToneMap(aces_tonemapped_bt709, min(1.f, unreal_default_aces_bt709), vanilla_tonemapped_bt709, 1.f, 1.f);

  float3 upgraded_tonemapped_ap1 = renodx::color::ap1::from::BT709(upgraded_tonemapped_bt709);

  return upgraded_tonemapped_ap1;
}

float3 HueCorrectAP1(float3 incorrect_color_ap1, float3 correct_color_ap1, float hue_correct_strength = 0.5f) {
  float3 incorrect_color_bt709 = renodx::color::bt709::from::AP1(incorrect_color_ap1);
  float3 correct_color_bt709 = renodx::color::bt709::from::AP1(correct_color_ap1);

  float3 corrected_color_bt709 = renodx::color::correct::Hue(incorrect_color_bt709, correct_color_bt709, hue_correct_strength, 0u);
  float3 corrected_color_ap1 = renodx::color::ap1::from::BT709(corrected_color_bt709);
  return corrected_color_ap1;
}

float3 GameScale(float3 color) {
  color = renodx::color::gamma::DecodeSafe(color, 2.2f);
  color *= RENODX_DIFFUSE_WHITE_NITS / RENODX_GRAPHICS_WHITE_NITS;
  color = renodx::color::gamma::EncodeSafe(color, 2.2f);

  return color;
}

#endif  // SRC_AVOWED_COMMON_HLSLI_
