#ifndef SRC_CP2077_CP2077_H_
#define SRC_CP2077_CP2077_H_

// Must be 32bit aligned
// Should be 4x32
struct ShaderInjectData {
  float toneMapType;
  float toneMapPeakNits;
  float toneMapGameNits;
  float toneMapGammaCorrection;

  float colorGradeExposure;
  float colorGradeHighlights;
  float colorGradeShadows;
  float colorGradeContrast;
  float colorGradeSaturation;
  float colorGradeBlowout;
  float colorGradeWhitePoint;
  float colorGradeLUTStrength;
  float colorGradeSceneGrading;

  float fxBloom;
  float fxVignette;
  float fxFilmGrain;

  float processingLUTCorrection;
  float processingLUTOrder;
  float processingInternalSampling;
};

#define TONE_MAPPER_TYPE__VANILLA 0.f
#define TONE_MAPPER_TYPE__NONE    1.f
#define TONE_MAPPER_TYPE__ACES    2.f
#define TONE_MAPPER_TYPE__RENODX  3.f

#define OUTPUT_TYPE_SRGB8  0u
#define OUTPUT_TYPE_PQ     1u
#define OUTPUT_TYPE_SCRGB  2u
#define OUTPUT_TYPE_SRGB10 3u

#endif  // SRC_CP2077_CP2077_H_
