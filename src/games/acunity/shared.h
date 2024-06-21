#ifndef SRC_AC_UNITY_SHARED_H_
#define SRC_AC_UNITY_SHARED_H_

// Must be 32bit aligned
// Should be 4x32
struct ShaderInjectData {
  float toneMapType;
  float toneMapPeakNits;
  float toneMapGameNits;
  float toneMapUINits;
  float toneMapGammaCorrection;
  float colorGradeExposure;
  float colorGradeHighlights;
  float colorGradeShadows;
  float colorGradeContrast;
  float colorGradeSaturation;
  float colorGradeBlowout;
  float colorGradeLUTStrength;
  float colorGradeLUTScaling;
  float fxBloom;
  float fxLensFlare;
  float fxVignette;
  float fxFilmGrain;
  float renoDRTFlare;
  float midGray;
  float elapsedTime;
};

#ifndef __cplusplus
cbuffer cb13 : register(b13) {
  ShaderInjectData injectedData : packoffset(c0);
}
#endif

#endif  // SRC_AC_UNITY_SHARED_H_
