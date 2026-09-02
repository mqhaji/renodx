#include "fsr2_sm5_config.hlsli"

#define FSR2_BIND_SRV_INPUT_DEPTH 1
#define FSR2_BIND_SRV_INPUT_COLOR 2

#define FSR2_BIND_UAV_RECONSTRUCTED_PREV_NEAREST_DEPTH 0
#define FSR2_BIND_UAV_DILATED_MOTION_VECTORS 1
#define FSR2_BIND_UAV_DILATED_DEPTH 2
#define FSR2_BIND_UAV_LOCK_INPUT_LUMA 3

#define FSR2_BIND_CB_FSR2 0

#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_callbacks_hlsl.h"
#include "fsr2_native_resolution.hlsli"

Texture2D<float2> mgsv_input_motion_vectors : register(t0);

FfxFloat32x2 LoadInputMotionVector(FfxUInt32x2 pixel) {
  return mgsv_input_motion_vectors[pixel];
}

#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_common.h"
#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_sample.h"
#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_reconstruct_dilated_velocity_and_previous_depth.h"

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
  if (any(dispatch_thread_id.xy >= uint2(RenderSize()))) return;
  ReconstructAndDilate(dispatch_thread_id.xy);
}
