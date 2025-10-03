#ifndef SRC_GOT_SHARED_H_
#define SRC_GOT_SHARED_H_

#define RENODX_PEAK_WHITE_NITS      400.f
#define RENODX_DIFFUSE_WHITE_NITS   100.f
#define RENODX_GRAPHICS_WHITE_NITS  100.f
#define RENODX_GAMMA_CORRECTION     1u
#define RENODX_TONE_MAP_TYPE        1u
#define RENODX_TONE_MAP_EXPOSURE    1.f
#define RENODX_TONE_MAP_HIGHLIGHTS  1.f
#define RENODX_TONE_MAP_SHADOWS     1.f
#define RENODX_TONE_MAP_CONTRAST    1.f
#define RENODX_TONE_MAP_SATURATION  1.f
#define RENODX_TONE_MAP_BLOWOUT     0.f
#define RENODX_TONE_MAP_HUE_SHIFT   0.2f
#define RENODX_COLOR_GRADE_STRENGTH 1.f
#define RENODX_COLOR_GRADE_SCALING  1.f
#define CUSTOM_BLOOM                1.f
#define RENODX_USE_PQ_ENCODING      1u
#define RENODX_OVERRIDE_BRIGHTNESS  1u

#ifndef __cplusplus

#include "../../shaders/renodx.hlsl"

#endif

#endif  // SRC_GOT_SHARED_H_
