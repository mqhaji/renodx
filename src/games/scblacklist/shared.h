#ifndef SRC_SCBLACKLIST_SHARED_H_
#define SRC_SCBLACKLIST_SHARED_H_

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
  float colorGradeLUTScaling;
  float fxBloom;
  float fxDoF;
  float fxFilmGrain;
  float midGray;
  float colorGradeDechroma;
  float renoDRTFlare;
};

#ifndef __cplusplus
cbuffer cb11 : register(b11) {
  ShaderInjectData injectedData : packoffset(c0);
}
#endif

#endif  // SRC_SCBLACKLIST_SHARED_H_