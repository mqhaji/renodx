#ifndef SRC_NORESTWICKED_SHARED_H_
#define SRC_NORESTWICKED_SHARED_H_

#ifndef __cplusplus
#include "../../shaders/renodx.hlsl"
#endif

// Must be 32bit aligned
// Should be 4x32
struct ShaderInjectData {
  float toneMapType;
  float toneMapPeakNits;
  float toneMapGameNits;
  float toneMapGammaCorrection;
  float toneMapHueCorrection;
  float colorGradeStrength;
};

#ifndef __cplusplus
cbuffer cb11 : register(b11) {
  ShaderInjectData injectedData : packoffset(c0);
}
#endif

#endif  // SRC_NORESTWICKED_SHARED_H_
