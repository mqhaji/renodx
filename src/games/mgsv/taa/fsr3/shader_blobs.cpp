// Fixed Shader Model 5 blob provider for the MGSV FSR3.1 D3D11 backend.

#include "ffx/upscalers/fsr3/internal/ffx_fsr3upscaler_shaderblobs.h"
#include "ffx/upscalers/fsr3/internal/ffx_fsr3upscaler_private.h"

#include <cstdint>
#include <span>

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

constexpr uint32_t kNormalPermutation = FSR3UPSCALER_SHADER_PERMUTATION_HDR_COLOR_INPUT
                                        | FSR3UPSCALER_SHADER_PERMUTATION_LOW_RES_MOTION_VECTORS
                                        | FSR3UPSCALER_SHADER_PERMUTATION_DEPTH_INVERTED;
constexpr uint32_t kSharpenPermutation = kNormalPermutation
                                         | FSR3UPSCALER_SHADER_PERMUTATION_ENABLE_SHARPENING;
constexpr uint32_t kCounts[16] = {
    1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u,
    1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u,
};
constexpr uint32_t kSpaces[16] = {};
const char* kSamplerNames[] = {"s_PointClamp", "s_LinearClamp"};
constexpr uint32_t kSamplerSlots[] = {0u, 1u};

struct Bindings {
  const char** srv_names = nullptr;
  const uint32_t* srv_slots = nullptr;
  uint32_t srv_count = 0u;
  const char** uav_names = nullptr;
  const uint32_t* uav_slots = nullptr;
  uint32_t uav_count = 0u;
  const char** cb_names = nullptr;
  const uint32_t* cb_slots = nullptr;
  uint32_t cb_count = 0u;
};

FfxShaderBlob MakeBlob(std::span<const uint8_t> code, const Bindings& bindings) {
  FfxShaderBlob blob = {};
  blob.data = code.data();
  blob.size = static_cast<uint32_t>(code.size());
  blob.entryName = "main";
  blob.cbvCount = bindings.cb_count;
  blob.srvTextureCount = bindings.srv_count;
  blob.uavTextureCount = bindings.uav_count;
  blob.samplerCount = 2u;
  blob.boundConstantBufferNames = bindings.cb_names;
  blob.boundConstantBuffers = bindings.cb_slots;
  blob.boundConstantBufferCounts = kCounts;
  blob.boundConstantBufferSpaces = kSpaces;
  blob.boundSRVTextureNames = bindings.srv_names;
  blob.boundSRVTextures = bindings.srv_slots;
  blob.boundSRVTextureCounts = kCounts;
  blob.boundSRVTextureSpaces = kSpaces;
  blob.boundUAVTextureNames = bindings.uav_names;
  blob.boundUAVTextures = bindings.uav_slots;
  blob.boundUAVTextureCounts = kCounts;
  blob.boundUAVTextureSpaces = kSpaces;
  blob.boundSamplerNames = kSamplerNames;
  blob.boundSamplers = kSamplerSlots;
  blob.boundSamplerCounts = kCounts;
  blob.boundSamplerSpaces = kSpaces;
  return blob;
}

const char* kPrepareInputsSrvs[] = {
    "r_input_motion_vectors",
    "r_input_depth",
    "r_input_color_jittered",
};
constexpr uint32_t kPrepareInputsSrvSlots[] = {0u, 1u, 2u};
const char* kPrepareInputsUavs[] = {
    "rw_dilated_motion_vectors",
    "rw_dilated_depth",
    "rw_reconstructed_previous_nearest_depth",
    "rw_farthest_depth",
    "rw_current_luma",
};
constexpr uint32_t kPrepareInputsUavSlots[] = {0u, 1u, 2u, 3u, 4u};
const char* kFsr3Cb[] = {"cbFSR3Upscaler"};
constexpr uint32_t kFsr3CbSlots[] = {0u};

const char* kLumaPyramidSrvs[] = {"r_current_luma", "r_farthest_depth"};
constexpr uint32_t kLumaPyramidSrvSlots[] = {0u, 1u};
const char* kLumaPyramidUavs[] = {
    "rw_spd_global_atomic",
    "rw_frame_info",
    "rw_spd_mip5",
    "rw_farthest_depth_mip1",
};
constexpr uint32_t kLumaPyramidUavSlots[] = {0u, 1u, 2u, 3u};
const char* kFsr3SpdCbs[] = {"cbFSR3Upscaler", "cbSPD"};
constexpr uint32_t kFsr3SpdCbSlots[] = {0u, 1u};

const char* kShadingPyramidSrvs[] = {
    "r_current_luma",
    "r_previous_luma",
    "r_dilated_motion_vectors",
    "r_input_exposure",
};
constexpr uint32_t kShadingPyramidSrvSlots[] = {0u, 1u, 2u, 3u};
const char* kShadingPyramidUavs[] = {
    "rw_spd_global_atomic",
    "rw_spd_mip0",
    "rw_spd_mip1",
    "rw_spd_mip2",
    "rw_spd_mip3",
    "rw_spd_mip4",
    "rw_spd_mip5",
};
constexpr uint32_t kShadingPyramidUavSlots[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u};

const char* kShadingChangeSrvs[] = {"r_spd_mips"};
constexpr uint32_t kShadingChangeSrvSlots[] = {0u};
const char* kShadingChangeUavs[] = {"rw_shading_change"};
constexpr uint32_t kShadingChangeUavSlots[] = {0u};

const char* kPrepareReactivitySrvs[] = {
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
constexpr uint32_t kPrepareReactivitySrvSlots[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
const char* kPrepareReactivityUavs[] = {
    "rw_dilated_reactive_masks",
    "rw_new_locks",
    "rw_accumulation",
};
constexpr uint32_t kPrepareReactivityUavSlots[] = {0u, 1u, 2u};

const char* kLumaInstabilitySrvs[] = {
    "r_input_exposure",
    "r_dilated_reactive_masks",
    "r_dilated_motion_vectors",
    "r_luma_history",
    "r_current_luma",
};
constexpr uint32_t kLumaInstabilitySrvSlots[] = {0u, 1u, 2u, 4u, 6u};
const char* kLumaInstabilityUavs[] = {"rw_luma_history", "rw_luma_instability"};
constexpr uint32_t kLumaInstabilityUavSlots[] = {0u, 1u};

const char* kAccumulateSrvs[] = {
    "r_input_exposure",
    "r_dilated_reactive_masks",
    "r_dilated_motion_vectors",
    "r_internal_upscaled_color",
    "r_farthest_depth_mip1",
    "r_luma_instability",
    "r_input_color_jittered",
};
  constexpr uint32_t kAccumulateSrvSlots[] = {0u, 1u, 2u, 3u, 5u, 7u, 8u};
const char* kAccumulateUavs[] = {
    "rw_internal_upscaled_color",
    "rw_upscaled_output",
    "rw_new_locks",
};
constexpr uint32_t kAccumulateUavSlots[] = {0u, 1u, 2u};
  const char* kAccumulateSharpenUavs[] = {
    "rw_internal_upscaled_color",
    "rw_new_locks",
  };
  constexpr uint32_t kAccumulateSharpenUavSlots[] = {0u, 2u};

const char* kRcasSrvs[] = {"r_input_exposure", "r_rcas_input"};
constexpr uint32_t kRcasSrvSlots[] = {0u, 1u};
const char* kOutputUav[] = {"rw_upscaled_output"};
constexpr uint32_t kOutputUavSlots[] = {0u};
const char* kRcasCbs[] = {"cbRCAS"};
constexpr uint32_t kRcasCbSlots[] = {1u};

const char* kGenerateReactiveSrvs[] = {"r_input_opaque_only", "r_input_color_jittered"};
constexpr uint32_t kGenerateReactiveSrvSlots[] = {0u, 1u};
const char* kGenerateReactiveUavs[] = {"rw_output_autoreactive"};
constexpr uint32_t kGenerateReactiveUavSlots[] = {0u};
const char* kGenerateReactiveCbs[] = {"cbGenerateReactive"};
constexpr uint32_t kGenerateReactiveCbSlots[] = {1u};

const char* kDebugViewSrvs[] = {
    "r_dilated_reactive_masks",
    "r_dilated_motion_vectors",
    "r_dilated_depth",
    "r_internal_upscaled_color",
};
  constexpr uint32_t kDebugViewSrvSlots[] = {0u, 1u, 2u, 3u};

const Bindings kPrepareInputsBindings = {
    kPrepareInputsSrvs, kPrepareInputsSrvSlots, 3u,
    kPrepareInputsUavs, kPrepareInputsUavSlots, 5u,
    kFsr3Cb, kFsr3CbSlots, 1u,
};
const Bindings kLumaPyramidBindings = {
    kLumaPyramidSrvs, kLumaPyramidSrvSlots, 2u,
    kLumaPyramidUavs, kLumaPyramidUavSlots, 4u,
    kFsr3SpdCbs, kFsr3SpdCbSlots, 2u,
};
const Bindings kShadingPyramidBindings = {
    kShadingPyramidSrvs, kShadingPyramidSrvSlots, 4u,
    kShadingPyramidUavs, kShadingPyramidUavSlots, 7u,
    kFsr3SpdCbs, kFsr3SpdCbSlots, 2u,
};
const Bindings kShadingChangeBindings = {
    kShadingChangeSrvs, kShadingChangeSrvSlots, 1u,
    kShadingChangeUavs, kShadingChangeUavSlots, 1u,
    kFsr3Cb, kFsr3CbSlots, 1u,
};
const Bindings kPrepareReactivityBindings = {
    kPrepareReactivitySrvs, kPrepareReactivitySrvSlots, 9u,
    kPrepareReactivityUavs, kPrepareReactivityUavSlots, 3u,
    kFsr3Cb, kFsr3CbSlots, 1u,
};
const Bindings kLumaInstabilityBindings = {
  kLumaInstabilitySrvs, kLumaInstabilitySrvSlots, 5u,
    kLumaInstabilityUavs, kLumaInstabilityUavSlots, 2u,
    kFsr3Cb, kFsr3CbSlots, 1u,
};
const Bindings kAccumulateBindings = {
  kAccumulateSrvs, kAccumulateSrvSlots, 7u,
    kAccumulateUavs, kAccumulateUavSlots, 3u,
    kFsr3Cb, kFsr3CbSlots, 1u,
};
const Bindings kAccumulateSharpenBindings = {
  kAccumulateSrvs, kAccumulateSrvSlots, 7u,
  kAccumulateSharpenUavs, kAccumulateSharpenUavSlots, 2u,
  kFsr3Cb, kFsr3CbSlots, 1u,
};
const Bindings kRcasBindings = {
    kRcasSrvs, kRcasSrvSlots, 2u,
    kOutputUav, kOutputUavSlots, 1u,
  kRcasCbs, kRcasCbSlots, 1u,
};
const Bindings kGenerateReactiveBindings = {
    kGenerateReactiveSrvs, kGenerateReactiveSrvSlots, 2u,
    kGenerateReactiveUavs, kGenerateReactiveUavSlots, 1u,
    kGenerateReactiveCbs, kGenerateReactiveCbSlots, 1u,
};
const Bindings kDebugViewBindings = {
  kDebugViewSrvs, kDebugViewSrvSlots, 4u,
    kOutputUav, kOutputUavSlots, 1u,
    kFsr3Cb, kFsr3CbSlots, 1u,
};

}  // namespace

FfxErrorCode fsr3UpscalerGetPermutationBlobByIndex(
    FfxFsr3UpscalerPass pass_id,
    uint32_t permutation_options,
    FfxShaderBlob* output) {
  if (output == nullptr) return FFX_ERROR_INVALID_POINTER;
  const uint32_t expected_options = pass_id == FFX_FSR3UPSCALER_PASS_ACCUMULATE_SHARPEN
                                        ? kSharpenPermutation
                                        : kNormalPermutation;
  if (permutation_options != expected_options) return FFX_ERROR_INVALID_ARGUMENT;

  switch (pass_id) {
    case FFX_FSR3UPSCALER_PASS_PREPARE_INPUTS:
      *output = MakeBlob(__fsr3sdk_prepare_inputs, kPrepareInputsBindings);
      break;
    case FFX_FSR3UPSCALER_PASS_LUMA_PYRAMID:
      *output = MakeBlob(__fsr3sdk_luma_pyramid, kLumaPyramidBindings);
      break;
    case FFX_FSR3UPSCALER_PASS_SHADING_CHANGE_PYRAMID:
      *output = MakeBlob(__fsr3sdk_shading_change_pyramid, kShadingPyramidBindings);
      break;
    case FFX_FSR3UPSCALER_PASS_SHADING_CHANGE:
      *output = MakeBlob(__fsr3sdk_shading_change, kShadingChangeBindings);
      break;
    case FFX_FSR3UPSCALER_PASS_PREPARE_REACTIVITY:
      *output = MakeBlob(__fsr3sdk_prepare_reactivity, kPrepareReactivityBindings);
      break;
    case FFX_FSR3UPSCALER_PASS_LUMA_INSTABILITY:
      *output = MakeBlob(__fsr3sdk_luma_instability, kLumaInstabilityBindings);
      break;
    case FFX_FSR3UPSCALER_PASS_ACCUMULATE:
      *output = MakeBlob(__fsr3sdk_accumulate, kAccumulateBindings);
      break;
    case FFX_FSR3UPSCALER_PASS_ACCUMULATE_SHARPEN:
      *output = MakeBlob(__fsr3sdk_accumulate_sharpen, kAccumulateSharpenBindings);
      break;
    case FFX_FSR3UPSCALER_PASS_RCAS:
      *output = MakeBlob(__fsr3sdk_rcas, kRcasBindings);
      break;
    case FFX_FSR3UPSCALER_PASS_GENERATE_REACTIVE:
      *output = MakeBlob(__fsr3sdk_generate_reactive, kGenerateReactiveBindings);
      break;
    case FFX_FSR3UPSCALER_PASS_DEBUG_VIEW:
      *output = MakeBlob(__fsr3sdk_debug_view, kDebugViewBindings);
      break;
    default:
      return FFX_ERROR_INVALID_ENUM;
  }
  return FFX_OK;
}

FfxErrorCode fsr3UpscalerIsWave64(uint32_t permutation_options, bool& is_wave64) {
  is_wave64 = (permutation_options & FSR3UPSCALER_SHADER_PERMUTATION_FORCE_WAVE64) != 0u;
  return FFX_OK;
}
