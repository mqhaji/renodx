#include "fsr3sdk_sm5_config.hlsli"

#define FSR3UPSCALER_BIND_SRV_CURRENT_LUMA    0
#define FSR3UPSCALER_BIND_SRV_FARTHEST_DEPTH 1

#define FSR3UPSCALER_BIND_UAV_SPD_GLOBAL_ATOMIC   0
#define FSR3UPSCALER_BIND_UAV_FRAME_INFO          1
#define FSR3UPSCALER_BIND_UAV_FARTHEST_DEPTH_MIP1 3

#define FSR3UPSCALER_BIND_CB_FSR3UPSCALER 0
#define FSR3UPSCALER_BIND_CB_SPD          1

// The luma algorithm only persists SPD mip 5. Compacting the otherwise sparse
// u0-u8 stock layout keeps this shader within FL11_0's eight-UAV limit.
#include "../ffx/upscalers/fsr3/include/gpu/fsr3upscaler/ffx_fsr3upscaler_callbacks_hlsl.h"
RWTexture2D<float2> renodx_spd_mip5 : register(u2);

FfxFloat32x2 RWLoadPyramid(FfxInt32x2 pixel, FfxUInt32 index) {
  return index == 5u ? renodx_spd_mip5[pixel] : 0.f.xx;
}

void StorePyramid(FfxInt32x2 pixel, FfxFloat32x2 value, FfxUInt32 index) {
  if (index == 5u) renodx_spd_mip5[pixel] = value;
}

#include "../ffx/upscalers/fsr3/include/gpu/fsr3upscaler/ffx_fsr3upscaler_common.h"
#include "../ffx/upscalers/fsr3/include/gpu/fsr3upscaler/ffx_fsr3upscaler_luma_pyramid.h"

[numthreads(256, 1, 1)]
void main(uint3 group_id : SV_GroupID, uint local_thread_index : SV_GroupIndex) {
  ComputeAutoExposure(group_id, local_thread_index);
}