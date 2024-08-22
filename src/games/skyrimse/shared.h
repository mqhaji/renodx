#ifndef SRC_SKYRIMSE_SHARED_H_
#define SRC_SKYRIMSE_SHARED_H_

#ifndef __cplusplus
#include "../../shaders/renodx.hlsl"
#endif

// Must be 32bit aligned
// Should be 4x32
struct ShaderInjectData {
  float toneMapType;
  float toneMapPeakNits;
  float toneMapGameNits;
  float toneMapUINits;
  float toneMapGammaCorrection;
  float toneMapHueCorrection;
  float colorGradeExposure;
  float colorGradeHighlights;
  float colorGradeShadows;
  float colorGradeContrast;
  float colorGradeSaturation;
  float colorGradeLUTStrength;
  float colorGradeLUTScaling;
  float fxBloom;
  float fxAutoExposure;
  float fxSceneFilter;
  float colorGradeBlowout;
};

#ifndef __cplusplus
cbuffer cb11 : register(b11) {
  ShaderInjectData injectedData : packoffset(c0);
}
#endif

#endif  // SRC_SKYRIMSE_SHARED_H_
