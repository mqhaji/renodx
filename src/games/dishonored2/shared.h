#ifndef SRC_DISHONORED2_SHARED_H_
#define SRC_DISHONORED2_SHARED_H_

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
  float colorGradeLUTStrength;
  float colorGradeLUTColorBoost;
  float lensDirt;
  float fxBloom;
  float fxAutoExposure;
  float fxDoF;
  float fxFilmGrain;
  float elapsedTime;
  float midGray;
  float colorGradeBlowout;
  float renoDRTFlare;
  float toneMapHueCorrection;
};

#ifndef __cplusplus
cbuffer cb13 : register(b13) {
  ShaderInjectData injectedData : packoffset(c0);
}
#endif

#endif  // SRC_DISHONORED2_SHARED_H_
