// FSR2 2.3.4 D3D11/SM5 port configuration.
//
// The algorithm sources are vendored unchanged from AMD FSR SDK v2.3.0.
// These definitions select FP32, groupshared SPD, explicit register bindings,
// reverse-Z, render-resolution motion, and the HDR accumulation path.

#define FFX_GPU 1
#define FFX_HLSL 1
#define FFX_HLSL_SM 50
#define FFX_HALF 0
#define FFX_NO_16_BIT_CAST 1

#define FFX_IMPLICIT_SHADER_REGISTER_BINDING_HLSL 0
#define FFX_FSR2_EMBED_ROOTSIG 0
#define FFX_SPD_NO_WAVE_OPERATIONS 1

#define FFX_FSR2_OPTION_UPSAMPLE_SAMPLERS_USE_DATA_HALF 0
#define FFX_FSR2_OPTION_ACCUMULATE_SAMPLERS_USE_DATA_HALF 0
#define FFX_FSR2_OPTION_REPROJECT_SAMPLERS_USE_DATA_HALF 0
#define FFX_FSR2_OPTION_POSTPROCESSLOCKSTATUS_SAMPLERS_USE_DATA_HALF 0

#define FFX_FSR2_OPTION_UPSAMPLE_USE_LANCZOS_TYPE 2
#define FFX_FSR2_OPTION_REPROJECT_USE_LANCZOS_TYPE 2

#define FFX_FSR2_OPTION_HDR_COLOR_INPUT 1
#define FFX_FSR2_OPTION_LOW_RESOLUTION_MOTION_VECTORS 1
#define FFX_FSR2_OPTION_JITTERED_MOTION_VECTORS 0
#define FFX_FSR2_OPTION_INVERTED_DEPTH 1
// No RCAS pipeline is created. FSR2 writes unsharpened final color through
// the MGSV output callback in fsr2_accumulate.cs_5_0.hlsl.
#define FFX_FSR2_OPTION_APPLY_SHARPENING 0
