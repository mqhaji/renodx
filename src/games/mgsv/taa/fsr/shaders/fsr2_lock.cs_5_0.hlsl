#include "fsr2_sm5_config.hlsli"

#define FSR2_BIND_SRV_LOCK_INPUT_LUMA 0

#define FSR2_BIND_UAV_NEW_LOCKS 0
#define FSR2_BIND_UAV_RECONSTRUCTED_PREV_NEAREST_DEPTH 1

#define FSR2_BIND_CB_FSR2 0

#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_callbacks_hlsl.h"
#include "fsr2_native_resolution.hlsli"
#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_common.h"
#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_sample.h"
#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_lock.h"

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
  if (any(dispatch_thread_id.xy >= uint2(RenderSize()))) return;
  ComputeLock(dispatch_thread_id.xy);
}
