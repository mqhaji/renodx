// Fixed Shader Model 5 blob provider for the MGSV FSR3.1 D3D11 backend.

#ifndef FFX_FSR3UPSCALER_DISABLE_WATERMARK
#define FFX_FSR3UPSCALER_DISABLE_WATERMARK 1
#endif

#include <array>
#include <cstdint>
#include <span>

#include "ffx/upscalers/fsr3/internal/ffx_fsr3upscaler_private.h"
#include "ffx/upscalers/fsr3/internal/ffx_fsr3upscaler_shaderblobs.h"


#include <embed/fsr3sdk_accumulate.h>
#include <embed/fsr3sdk_accumulate_sharpen.h>
#include <embed/fsr3sdk_debug_view.h>
#include <embed/fsr3sdk_generate_reactive.h>
#include <embed/fsr3sdk_luma_instability.h>
#include <embed/fsr3sdk_luma_pyramid.h>
#include <embed/fsr3sdk_prepare_inputs.h>
#include <embed/fsr3sdk_prepare_reactivity.h>
#include <embed/fsr3sdk_rcas.h>
#include <embed/fsr3sdk_shading_change.h>
#include <embed/fsr3sdk_shading_change_pyramid.h>

namespace {

constexpr uint32_t NORMAL_PERMUTATION = FSR3UPSCALER_SHADER_PERMUTATION_HDR_COLOR_INPUT
                                        | FSR3UPSCALER_SHADER_PERMUTATION_LOW_RES_MOTION_VECTORS
                                        | FSR3UPSCALER_SHADER_PERMUTATION_DEPTH_INVERTED;
constexpr uint32_t SHARPEN_PERMUTATION = NORMAL_PERMUTATION
                                         | FSR3UPSCALER_SHADER_PERMUTATION_ENABLE_SHARPENING;
struct Bindings {
  const char* const* srv_names = nullptr;
  const uint32_t* srv_slots = nullptr;
  uint32_t srv_count = 0u;
  const char* const* uav_names = nullptr;
  const uint32_t* uav_slots = nullptr;
  uint32_t uav_count = 0u;
  const char* const* cb_names = nullptr;
  const uint32_t* cb_slots = nullptr;
  uint32_t cb_count = 0u;
};

const char** ToFfxNames(const char* const* names) {
  // FfxShaderBlob does not mark the pointer array itself const, but the host only reads it.
  return const_cast<const char**>(names);  // NOLINT(cppcoreguidelines-pro-type-const-cast)
}

FfxShaderBlob MakeBlob(std::span<const uint8_t> code, const Bindings& bindings) {
  FfxShaderBlob blob = {};
  blob.data = code.data();
  blob.size = static_cast<uint32_t>(code.size());
  blob.cbvCount = bindings.cb_count;
  blob.srvTextureCount = bindings.srv_count;
  blob.uavTextureCount = bindings.uav_count;
  blob.boundConstantBufferNames = ToFfxNames(bindings.cb_names);
  blob.boundConstantBuffers = bindings.cb_slots;
  blob.boundSRVTextureNames = ToFfxNames(bindings.srv_names);
  blob.boundSRVTextures = bindings.srv_slots;
  blob.boundUAVTextureNames = ToFfxNames(bindings.uav_names);
  blob.boundUAVTextures = bindings.uav_slots;
  return blob;
}

constexpr std::array<const char*, 3> PREPARE_INPUTS_SRVS = {
    "r_input_motion_vectors",
    "r_input_depth",
    "r_input_color_jittered",
};
constexpr std::array<uint32_t, 3> PREPARE_INPUTS_SRV_SLOTS = {0u, 1u, 2u};
constexpr std::array<const char*, 5> PREPARE_INPUTS_UAVS = {
    "rw_dilated_motion_vectors",
    "rw_dilated_depth",
    "rw_reconstructed_previous_nearest_depth",
    "rw_farthest_depth",
    "rw_current_luma",
};
constexpr std::array<uint32_t, 5> PREPARE_INPUTS_UAV_SLOTS = {0u, 1u, 2u, 3u, 4u};
constexpr std::array<const char*, 1> FSR3_CBS = {"cbFSR3Upscaler"};
constexpr std::array<uint32_t, 1> FSR3_CB_SLOTS = {0u};

constexpr std::array<const char*, 2> LUMA_PYRAMID_SRVS = {"r_current_luma", "r_farthest_depth"};
constexpr std::array<uint32_t, 2> LUMA_PYRAMID_SRV_SLOTS = {0u, 1u};
constexpr std::array<const char*, 4> LUMA_PYRAMID_UAVS = {
    "rw_spd_global_atomic",
    "rw_frame_info",
    "rw_spd_mip5",
    "rw_farthest_depth_mip1",
};
constexpr std::array<uint32_t, 4> LUMA_PYRAMID_UAV_SLOTS = {0u, 1u, 2u, 3u};
constexpr std::array<const char*, 2> FSR3_SPD_CBS = {"cbFSR3Upscaler", "cbSPD"};
constexpr std::array<uint32_t, 2> FSR3_SPD_CB_SLOTS = {0u, 1u};

constexpr std::array<const char*, 4> SHADING_PYRAMID_SRVS = {
    "r_current_luma",
    "r_previous_luma",
    "r_dilated_motion_vectors",
    "r_input_exposure",
};
constexpr std::array<uint32_t, 4> SHADING_PYRAMID_SRV_SLOTS = {0u, 1u, 2u, 3u};
constexpr std::array<const char*, 7> SHADING_PYRAMID_UAVS = {
    "rw_spd_global_atomic",
    "rw_spd_mip0",
    "rw_spd_mip1",
    "rw_spd_mip2",
    "rw_spd_mip3",
    "rw_spd_mip4",
    "rw_spd_mip5",
};
constexpr std::array<uint32_t, 7> SHADING_PYRAMID_UAV_SLOTS = {0u, 1u, 2u, 3u, 4u, 5u, 6u};

constexpr std::array<const char*, 1> SHADING_CHANGE_SRVS = {"r_spd_mips"};
constexpr std::array<uint32_t, 1> SHADING_CHANGE_SRV_SLOTS = {0u};
constexpr std::array<const char*, 1> SHADING_CHANGE_UAVS = {"rw_shading_change"};
constexpr std::array<uint32_t, 1> SHADING_CHANGE_UAV_SLOTS = {0u};

constexpr std::array<const char*, 9> PREPARE_REACTIVITY_SRVS = {
    "r_reconstructed_previous_nearest_depth",
    "r_dilated_motion_vectors",
    "r_dilated_depth",
    "r_reactive_mask",
    "r_transparency_and_composition_mask",
    "r_accumulation",
    "r_shading_change",
    "r_current_luma",
    "r_input_exposure",
};
constexpr std::array<uint32_t, 9> PREPARE_REACTIVITY_SRV_SLOTS = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
constexpr std::array<const char*, 3> PREPARE_REACTIVITY_UAVS = {
    "rw_dilated_reactive_masks",
    "rw_new_locks",
    "rw_accumulation",
};
constexpr std::array<uint32_t, 3> PREPARE_REACTIVITY_UAV_SLOTS = {0u, 1u, 2u};

constexpr std::array<const char*, 5> LUMA_INSTABILITY_SRVS = {
    "r_input_exposure",
    "r_dilated_reactive_masks",
    "r_dilated_motion_vectors",
    "r_luma_history",
    "r_current_luma",
};
constexpr std::array<uint32_t, 5> LUMA_INSTABILITY_SRV_SLOTS = {0u, 1u, 2u, 4u, 6u};
constexpr std::array<const char*, 2> LUMA_INSTABILITY_UAVS = {"rw_luma_history", "rw_luma_instability"};
constexpr std::array<uint32_t, 2> LUMA_INSTABILITY_UAV_SLOTS = {0u, 1u};

constexpr std::array<const char*, 7> ACCUMULATE_SRVS = {
    "r_input_exposure",
    "r_dilated_reactive_masks",
    "r_dilated_motion_vectors",
    "r_internal_upscaled_color",
    "r_farthest_depth_mip1",
    "r_luma_instability",
    "r_input_color_jittered",
};
constexpr std::array<uint32_t, 7> ACCUMULATE_SRV_SLOTS = {0u, 1u, 2u, 3u, 5u, 7u, 8u};
constexpr std::array<const char*, 3> ACCUMULATE_UAVS = {
    "rw_internal_upscaled_color",
    "rw_upscaled_output",
    "rw_new_locks",
};
constexpr std::array<uint32_t, 3> ACCUMULATE_UAV_SLOTS = {0u, 1u, 2u};
constexpr std::array<const char*, 2> ACCUMULATE_SHARPEN_UAVS = {
    "rw_internal_upscaled_color",
    "rw_new_locks",
};
constexpr std::array<uint32_t, 2> ACCUMULATE_SHARPEN_UAV_SLOTS = {0u, 2u};

constexpr std::array<const char*, 2> RCAS_SRVS = {"r_input_exposure", "r_rcas_input"};
constexpr std::array<uint32_t, 2> RCAS_SRV_SLOTS = {0u, 1u};
constexpr std::array<const char*, 1> OUTPUT_UAVS = {"rw_upscaled_output"};
constexpr std::array<uint32_t, 1> OUTPUT_UAV_SLOTS = {0u};
constexpr std::array<const char*, 1> RCAS_CBS = {"cbRCAS"};
constexpr std::array<uint32_t, 1> RCAS_CB_SLOTS = {1u};

constexpr std::array<const char*, 2> GENERATE_REACTIVE_SRVS = {"r_input_opaque_only", "r_input_color_jittered"};
constexpr std::array<uint32_t, 2> GENERATE_REACTIVE_SRV_SLOTS = {0u, 1u};
constexpr std::array<const char*, 1> GENERATE_REACTIVE_UAVS = {"rw_output_autoreactive"};
constexpr std::array<uint32_t, 1> GENERATE_REACTIVE_UAV_SLOTS = {0u};
constexpr std::array<const char*, 1> GENERATE_REACTIVE_CBS = {"cbGenerateReactive"};
constexpr std::array<uint32_t, 1> GENERATE_REACTIVE_CB_SLOTS = {1u};

constexpr std::array<const char*, 4> DEBUG_VIEW_SRVS = {
    "r_dilated_reactive_masks",
    "r_dilated_motion_vectors",
    "r_dilated_depth",
    "r_internal_upscaled_color",
};
constexpr std::array<uint32_t, 4> DEBUG_VIEW_SRV_SLOTS = {0u, 1u, 2u, 3u};

constexpr Bindings PREPARE_INPUTS_BINDINGS = {
    .srv_names = PREPARE_INPUTS_SRVS.data(),
    .srv_slots = PREPARE_INPUTS_SRV_SLOTS.data(),
    .srv_count = static_cast<uint32_t>(PREPARE_INPUTS_SRVS.size()),
    .uav_names = PREPARE_INPUTS_UAVS.data(),
    .uav_slots = PREPARE_INPUTS_UAV_SLOTS.data(),
    .uav_count = static_cast<uint32_t>(PREPARE_INPUTS_UAVS.size()),
    .cb_names = FSR3_CBS.data(),
    .cb_slots = FSR3_CB_SLOTS.data(),
    .cb_count = static_cast<uint32_t>(FSR3_CBS.size()),
};
constexpr Bindings LUMA_PYRAMID_BINDINGS = {
    .srv_names = LUMA_PYRAMID_SRVS.data(),
    .srv_slots = LUMA_PYRAMID_SRV_SLOTS.data(),
    .srv_count = static_cast<uint32_t>(LUMA_PYRAMID_SRVS.size()),
    .uav_names = LUMA_PYRAMID_UAVS.data(),
    .uav_slots = LUMA_PYRAMID_UAV_SLOTS.data(),
    .uav_count = static_cast<uint32_t>(LUMA_PYRAMID_UAVS.size()),
    .cb_names = FSR3_SPD_CBS.data(),
    .cb_slots = FSR3_SPD_CB_SLOTS.data(),
    .cb_count = static_cast<uint32_t>(FSR3_SPD_CBS.size()),
};
constexpr Bindings SHADING_PYRAMID_BINDINGS = {
    .srv_names = SHADING_PYRAMID_SRVS.data(),
    .srv_slots = SHADING_PYRAMID_SRV_SLOTS.data(),
    .srv_count = static_cast<uint32_t>(SHADING_PYRAMID_SRVS.size()),
    .uav_names = SHADING_PYRAMID_UAVS.data(),
    .uav_slots = SHADING_PYRAMID_UAV_SLOTS.data(),
    .uav_count = static_cast<uint32_t>(SHADING_PYRAMID_UAVS.size()),
    .cb_names = FSR3_SPD_CBS.data(),
    .cb_slots = FSR3_SPD_CB_SLOTS.data(),
    .cb_count = static_cast<uint32_t>(FSR3_SPD_CBS.size()),
};
constexpr Bindings SHADING_CHANGE_BINDINGS = {
    .srv_names = SHADING_CHANGE_SRVS.data(),
    .srv_slots = SHADING_CHANGE_SRV_SLOTS.data(),
    .srv_count = static_cast<uint32_t>(SHADING_CHANGE_SRVS.size()),
    .uav_names = SHADING_CHANGE_UAVS.data(),
    .uav_slots = SHADING_CHANGE_UAV_SLOTS.data(),
    .uav_count = static_cast<uint32_t>(SHADING_CHANGE_UAVS.size()),
    .cb_names = FSR3_CBS.data(),
    .cb_slots = FSR3_CB_SLOTS.data(),
    .cb_count = static_cast<uint32_t>(FSR3_CBS.size()),
};
constexpr Bindings PREPARE_REACTIVITY_BINDINGS = {
    .srv_names = PREPARE_REACTIVITY_SRVS.data(),
    .srv_slots = PREPARE_REACTIVITY_SRV_SLOTS.data(),
    .srv_count = static_cast<uint32_t>(PREPARE_REACTIVITY_SRVS.size()),
    .uav_names = PREPARE_REACTIVITY_UAVS.data(),
    .uav_slots = PREPARE_REACTIVITY_UAV_SLOTS.data(),
    .uav_count = static_cast<uint32_t>(PREPARE_REACTIVITY_UAVS.size()),
    .cb_names = FSR3_CBS.data(),
    .cb_slots = FSR3_CB_SLOTS.data(),
    .cb_count = static_cast<uint32_t>(FSR3_CBS.size()),
};
constexpr Bindings LUMA_INSTABILITY_BINDINGS = {
    .srv_names = LUMA_INSTABILITY_SRVS.data(),
    .srv_slots = LUMA_INSTABILITY_SRV_SLOTS.data(),
    .srv_count = static_cast<uint32_t>(LUMA_INSTABILITY_SRVS.size()),
    .uav_names = LUMA_INSTABILITY_UAVS.data(),
    .uav_slots = LUMA_INSTABILITY_UAV_SLOTS.data(),
    .uav_count = static_cast<uint32_t>(LUMA_INSTABILITY_UAVS.size()),
    .cb_names = FSR3_CBS.data(),
    .cb_slots = FSR3_CB_SLOTS.data(),
    .cb_count = static_cast<uint32_t>(FSR3_CBS.size()),
};
constexpr Bindings ACCUMULATE_BINDINGS = {
    .srv_names = ACCUMULATE_SRVS.data(),
    .srv_slots = ACCUMULATE_SRV_SLOTS.data(),
    .srv_count = static_cast<uint32_t>(ACCUMULATE_SRVS.size()),
    .uav_names = ACCUMULATE_UAVS.data(),
    .uav_slots = ACCUMULATE_UAV_SLOTS.data(),
    .uav_count = static_cast<uint32_t>(ACCUMULATE_UAVS.size()),
    .cb_names = FSR3_CBS.data(),
    .cb_slots = FSR3_CB_SLOTS.data(),
    .cb_count = static_cast<uint32_t>(FSR3_CBS.size()),
};
constexpr Bindings ACCUMULATE_SHARPEN_BINDINGS = {
    .srv_names = ACCUMULATE_SRVS.data(),
    .srv_slots = ACCUMULATE_SRV_SLOTS.data(),
    .srv_count = static_cast<uint32_t>(ACCUMULATE_SRVS.size()),
    .uav_names = ACCUMULATE_SHARPEN_UAVS.data(),
    .uav_slots = ACCUMULATE_SHARPEN_UAV_SLOTS.data(),
    .uav_count = static_cast<uint32_t>(ACCUMULATE_SHARPEN_UAVS.size()),
    .cb_names = FSR3_CBS.data(),
    .cb_slots = FSR3_CB_SLOTS.data(),
    .cb_count = static_cast<uint32_t>(FSR3_CBS.size()),
};
constexpr Bindings RCAS_BINDINGS = {
    .srv_names = RCAS_SRVS.data(),
    .srv_slots = RCAS_SRV_SLOTS.data(),
    .srv_count = static_cast<uint32_t>(RCAS_SRVS.size()),
    .uav_names = OUTPUT_UAVS.data(),
    .uav_slots = OUTPUT_UAV_SLOTS.data(),
    .uav_count = static_cast<uint32_t>(OUTPUT_UAVS.size()),
    .cb_names = RCAS_CBS.data(),
    .cb_slots = RCAS_CB_SLOTS.data(),
    .cb_count = static_cast<uint32_t>(RCAS_CBS.size()),
};
constexpr Bindings GENERATE_REACTIVE_BINDINGS = {
    .srv_names = GENERATE_REACTIVE_SRVS.data(),
    .srv_slots = GENERATE_REACTIVE_SRV_SLOTS.data(),
    .srv_count = static_cast<uint32_t>(GENERATE_REACTIVE_SRVS.size()),
    .uav_names = GENERATE_REACTIVE_UAVS.data(),
    .uav_slots = GENERATE_REACTIVE_UAV_SLOTS.data(),
    .uav_count = static_cast<uint32_t>(GENERATE_REACTIVE_UAVS.size()),
    .cb_names = GENERATE_REACTIVE_CBS.data(),
    .cb_slots = GENERATE_REACTIVE_CB_SLOTS.data(),
    .cb_count = static_cast<uint32_t>(GENERATE_REACTIVE_CBS.size()),
};
constexpr Bindings DEBUG_VIEW_BINDINGS = {
    .srv_names = DEBUG_VIEW_SRVS.data(),
    .srv_slots = DEBUG_VIEW_SRV_SLOTS.data(),
    .srv_count = static_cast<uint32_t>(DEBUG_VIEW_SRVS.size()),
    .uav_names = OUTPUT_UAVS.data(),
    .uav_slots = OUTPUT_UAV_SLOTS.data(),
    .uav_count = static_cast<uint32_t>(OUTPUT_UAVS.size()),
    .cb_names = FSR3_CBS.data(),
    .cb_slots = FSR3_CB_SLOTS.data(),
    .cb_count = static_cast<uint32_t>(FSR3_CBS.size()),
};

}  // namespace

// NOLINTBEGIN(readability-identifier-naming)
FfxErrorCode fsr3UpscalerGetPermutationBlobByIndex(
    FfxFsr3UpscalerPass passId,
    uint32_t permutationOptions,
    FfxShaderBlob* outBlob) {
  if (outBlob == nullptr) return FFX_ERROR_INVALID_POINTER;
  const uint32_t expected_options = passId == FFX_FSR3UPSCALER_PASS_ACCUMULATE_SHARPEN
                                        ? SHARPEN_PERMUTATION
                                        : NORMAL_PERMUTATION;
  if (permutationOptions != expected_options) return FFX_ERROR_INVALID_ARGUMENT;

  switch (passId) {
    case FFX_FSR3UPSCALER_PASS_PREPARE_INPUTS:
      *outBlob = MakeBlob(__fsr3sdk_prepare_inputs, PREPARE_INPUTS_BINDINGS);
      break;
    case FFX_FSR3UPSCALER_PASS_LUMA_PYRAMID:
      *outBlob = MakeBlob(__fsr3sdk_luma_pyramid, LUMA_PYRAMID_BINDINGS);
      break;
    case FFX_FSR3UPSCALER_PASS_SHADING_CHANGE_PYRAMID:
      *outBlob = MakeBlob(__fsr3sdk_shading_change_pyramid, SHADING_PYRAMID_BINDINGS);
      break;
    case FFX_FSR3UPSCALER_PASS_SHADING_CHANGE:
      *outBlob = MakeBlob(__fsr3sdk_shading_change, SHADING_CHANGE_BINDINGS);
      break;
    case FFX_FSR3UPSCALER_PASS_PREPARE_REACTIVITY:
      *outBlob = MakeBlob(__fsr3sdk_prepare_reactivity, PREPARE_REACTIVITY_BINDINGS);
      break;
    case FFX_FSR3UPSCALER_PASS_LUMA_INSTABILITY:
      *outBlob = MakeBlob(__fsr3sdk_luma_instability, LUMA_INSTABILITY_BINDINGS);
      break;
    case FFX_FSR3UPSCALER_PASS_ACCUMULATE:
      *outBlob = MakeBlob(__fsr3sdk_accumulate, ACCUMULATE_BINDINGS);
      break;
    case FFX_FSR3UPSCALER_PASS_ACCUMULATE_SHARPEN:
      *outBlob = MakeBlob(__fsr3sdk_accumulate_sharpen, ACCUMULATE_SHARPEN_BINDINGS);
      break;
    case FFX_FSR3UPSCALER_PASS_RCAS:
      *outBlob = MakeBlob(__fsr3sdk_rcas, RCAS_BINDINGS);
      break;
    case FFX_FSR3UPSCALER_PASS_GENERATE_REACTIVE:
      *outBlob = MakeBlob(__fsr3sdk_generate_reactive, GENERATE_REACTIVE_BINDINGS);
      break;
    case FFX_FSR3UPSCALER_PASS_DEBUG_VIEW:
      *outBlob = MakeBlob(__fsr3sdk_debug_view, DEBUG_VIEW_BINDINGS);
      break;
    default:
      return FFX_ERROR_INVALID_ENUM;
  }
  return FFX_OK;
}

FfxErrorCode fsr3UpscalerIsWave64(uint32_t permutationOptions, bool& isWave64) {
  isWave64 = (permutationOptions & FSR3UPSCALER_SHADER_PERMUTATION_FORCE_WAVE64) != 0u;
  return FFX_OK;
}
// NOLINTEND(readability-identifier-naming)
