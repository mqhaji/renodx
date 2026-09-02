#include "fsr2_sm5_config.hlsli"

#define FSR2_BIND_SRV_INPUT_COLOR 0

#define FSR2_BIND_UAV_SPD_GLOBAL_ATOMIC 0
#define FSR2_BIND_UAV_EXPOSURE_MIP_LUMA_CHANGE 1
#define FSR2_BIND_UAV_EXPOSURE_MIP_5 2
#define FSR2_BIND_UAV_AUTO_EXPOSURE 3

#define FSR2_BIND_CB_FSR2 0
#define FSR2_BIND_CB_SPD 1

#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_callbacks_hlsl.h"
#include "fsr2_native_resolution.hlsli"
#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_common.h"
#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_compute_luminance_pyramid.h"

[numthreads(256, 1, 1)]
void main(uint3 work_group_id : SV_GroupID, uint local_thread_index : SV_GroupIndex) {
  ComputeAutoExposure(work_group_id, local_thread_index);
}
