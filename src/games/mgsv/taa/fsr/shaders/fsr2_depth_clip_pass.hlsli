#include "fsr2_sm5_config.hlsli"

#ifndef MGSV_FSR2_ZERO_INPUT_MASKS
#define MGSV_FSR2_ZERO_INPUT_MASKS 0
#endif

#define FSR2_BIND_SRV_RECONSTRUCTED_PREV_NEAREST_DEPTH 0
#define FSR2_BIND_SRV_DILATED_MOTION_VECTORS 1
#define FSR2_BIND_SRV_DILATED_DEPTH 2
#define FSR2_BIND_SRV_REACTIVE_MASK 3
#define FSR2_BIND_SRV_TRANSPARENCY_AND_COMPOSITION_MASK 4
#define FSR2_BIND_SRV_PREVIOUS_DILATED_MOTION_VECTORS 5
#define FSR2_BIND_SRV_INPUT_COLOR 7
#define FSR2_BIND_SRV_INPUT_DEPTH 8

#define FSR2_BIND_UAV_DILATED_REACTIVE_MASKS 0
#define FSR2_BIND_UAV_PREPARED_INPUT_COLOR 1

#define FSR2_BIND_CB_FSR2 0

cbuffer MgsvFsr2DepthClipConstants : register(b2) {
  float mgsv_depth_separation_scale;
  float mgsv_depth_clip_power;
  float mgsv_depth_clip_power_is_three;
  float mgsv_depth_clip_padding;
};

#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_callbacks_hlsl.h"
#include "fsr2_native_resolution.hlsli"

Texture2D<float2> mgsv_input_motion_vectors : register(t6);

FfxFloat32x2 LoadInputMotionVector(FfxUInt32x2 pixel) {
  return mgsv_input_motion_vectors[pixel];
}

#if MGSV_FSR2_ZERO_INPUT_MASKS
#define LoadReactiveMask(pixel) FfxFloat32(0.f)
#define LoadTransparencyAndCompositionMask(pixel) FfxFloat32(0.f)
#endif

#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_common.h"
#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_sample.h"
#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_depth_clip.h"
#include "fsr2_depth_clip_optimized.hlsli"

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
  if (any(dispatch_thread_id.xy >= uint2(RenderSize()))) return;
  DepthClipOptimized(dispatch_thread_id.xy);
}