#include "fsr2_sm5_config.hlsli"

#include "../../../shared.h"

#define FSR2_BIND_SRV_DILATED_REACTIVE_MASKS   1
#define FSR2_BIND_SRV_DILATED_MOTION_VECTORS   2
#define FSR2_BIND_SRV_INTERNAL_UPSCALED        3
#define FSR2_BIND_SRV_LOCK_STATUS              4
#define FSR2_BIND_SRV_PREPARED_INPUT_COLOR     5
#define FSR2_BIND_SRV_LANCZOS_LUT              6
#define FSR2_BIND_SRV_UPSCALE_MAXIMUM_BIAS_LUT 7
#define FSR2_BIND_SRV_SCENE_LUMINANCE_MIPS     8
#define FSR2_BIND_SRV_AUTO_EXPOSURE            9
#define FSR2_BIND_SRV_LUMA_HISTORY             10

#define FSR2_BIND_UAV_INTERNAL_UPSCALED 0
#define FSR2_BIND_UAV_LOCK_STATUS       1
#define FSR2_BIND_UAV_NEW_LOCKS         3
#define FSR2_BIND_UAV_LUMA_HISTORY      4

#define FSR2_BIND_CB_FSR2 0

// Dependency order is significant: sample.h defines generators consumed by
// upsample, lock-status, and reprojection headers.
// clang-format off
#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_callbacks_hlsl.h"
#include "fsr2_native_resolution.hlsli"
#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_common.h"
#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_sample.h"
#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_upsample.h"
#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_postprocess_lock_status.h"
#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_reproject.h"
// clang-format on

Texture2D<float4> mgsv_current_linear_color : register(t11);
RWTexture2D<float4> mgsv_encoded_output : register(u2);

// FSR2 supplies final linear RGB here when sharpening is disabled. MGSV's
// downstream pre-DoF scene resource expects sRGB RGB and current scene alpha.
void StoreUpscaledOutput(FfxUInt32x2 pixel, FfxFloat32x3 linear_color) {
  const float scene_alpha = mgsv_current_linear_color.Load(int3(pixel, 0)).a;
  mgsv_encoded_output[pixel] = float4(
      renodx::color::srgb::Encode(max(0.f.xxx, linear_color)),
      scene_alpha);
}

#include "../vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/ffx_fsr2_accumulate.h"

[numthreads(8, 8, 1)]
void main(uint3 group_id: SV_GroupID, uint3 group_thread_id: SV_GroupThreadID) {
  const uint group_rows = (uint(DisplaySize().y) + 7u) / 8u;
  group_id.y = group_rows - group_id.y - 1u;
  const uint2 pixel = group_id.xy * 8u + group_thread_id.xy;
  if (any(pixel >= uint2(DisplaySize()))) return;
  Accumulate(pixel);
}
