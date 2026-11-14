#ifndef RENODX_GAMES_MGSV_RECONSTRUCTION_FSR3_RUNTIME_HPP_
#define RENODX_GAMES_MGSV_RECONSTRUCTION_FSR3_RUNTIME_HPP_

// AMD's pinned host implementation schedules FSR 3.1.5. Inputs, frame command
// storage, encoding, and GPU handoff are shared with DLSS and analytical TAA.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include "../d3d12.hpp"
#include "backend_dx12.hpp"
#include "ffx/upscalers/fsr3/include/ffx_fsr3upscaler.h"

#ifndef FFX_FSR3UPSCALER_DISABLE_WATERMARK
#define FFX_FSR3UPSCALER_DISABLE_WATERMARK 1
#endif

namespace mgsv::reconstruction::fsr3 {

struct Resources {
  std::unique_ptr<FfxFsr3UpscalerContext> context;
  std::unique_ptr<backend::Backend> backend;
  Microsoft::WRL::ComPtr<ID3D12Resource> linear_output;
  Microsoft::WRL::ComPtr<ID3D12Resource> reconstructed_depth;
  Microsoft::WRL::ComPtr<ID3D12Resource> dilated_depth;
  Microsoft::WRL::ComPtr<ID3D12Resource> dilated_motion;
  uint32_t render_width = 0u;
  uint32_t render_height = 0u;
  uint32_t output_width = 0u;
  uint32_t output_height = 0u;
  bool context_created = false;
  bool initialized = false;
  bool recording_failed = false;
  std::chrono::steady_clock::time_point previous_dispatch_time;
};

static Resources resources;

inline void Destroy() {
  dx12::Drain();
  if (resources.context_created) {
    ffxFsr3UpscalerContextDestroy(resources.context.get());
  }
  resources = {};
}

inline void InvalidateHistory() {
  resources.initialized = false;
}

inline void ReleaseTemporalResources(reshade::api::device* /*device*/) {
  // Keep the last bundle across method switches, just as DLSS does. Present
  // does not wait for or destroy histories still used by submitted commands.
  InvalidateHistory();
}

inline void CheckFsr(FfxErrorCode result, const char* operation) {
  if (result != FFX_OK) throw std::runtime_error(std::string(operation) + ": " + std::to_string(static_cast<int>(result)));
}

inline bool Dispatch(const ValidatedFrameInputs& inputs) {
  const bool succeeded = dx12::Execute(inputs, !resources.initialized, [&](const dx12::Evaluation& evaluation) {
    if (!resources.context_created || resources.recording_failed || resources.render_width != inputs.render_width || resources.render_height != inputs.render_height
        || resources.output_width != inputs.output.width || resources.output_height != inputs.output.height) {
      Destroy();
      resources.backend = std::make_unique<backend::Backend>();
      resources.context = std::make_unique<FfxFsr3UpscalerContext>();
      FfxFsr3UpscalerContextDescription description = {};
      description.flags = FFX_FSR3UPSCALER_ENABLE_HIGH_DYNAMIC_RANGE | FFX_FSR3UPSCALER_ENABLE_DEPTH_INVERTED;
      description.maxRenderSize = {.width = inputs.render_width, .height = inputs.render_height};
      description.maxUpscaleSize = {.width = inputs.output.width, .height = inputs.output.height};
      resources.backend->device = evaluation.device;
      auto& backend_interface = description.backendInterface;
      backend_interface.fpGetSDKVersion = backend::GetSdkVersion;
      backend_interface.fpGetEffectGpuMemoryUsage = backend::GetMemoryUsage;
      backend_interface.fpCreateBackendContext = backend::CreateContext;
      backend_interface.fpGetDeviceCapabilities = backend::GetCapabilities;
      backend_interface.fpDestroyBackendContext = backend::DestroyContext;
      backend_interface.fpCreateResource = backend::CreateResource;
      backend_interface.fpRegisterResource = backend::RegisterResource;
      backend_interface.fpGetResource = backend::GetResource;
      backend_interface.fpUnregisterResources = backend::UnregisterResources;
      backend_interface.fpGetResourceDescription = backend::GetDescription;
      backend_interface.fpDestroyResource = backend::DestroyResource;
      backend_interface.fpStageConstantBufferDataFunc = backend::StageConstants;
      backend_interface.fpCreatePipeline = backend::CreatePipeline;
      backend_interface.fpDestroyPipeline = backend::DestroyPipeline;
      backend_interface.fpScheduleGpuJob = backend::ScheduleJob;
      backend_interface.fpExecuteGpuJobs = backend::ExecuteJobs;
      backend_interface.scratchBuffer = resources.backend.get();
      backend_interface.scratchBufferSize = sizeof(backend::Backend);
      backend_interface.device = evaluation.device;

      // Retain partial state for the public SDK destroy path if creation fails.
      resources.context_created = true;
      CheckFsr(ffxFsr3UpscalerContextCreate(resources.context.get(), &description), "Create D3D12 FSR context");
      float shading_change_scale = 0.15f;
      float accumulation_added_per_frame = 1.f / 3.f;
      ffxFsr3UpscalerSetConstant(resources.context.get(), FFX_FSR3UPSCALER_CONFIGURE_UPSCALE_KEY_FSHADINGCHANGESCALE, &shading_change_scale);
      ffxFsr3UpscalerSetConstant(resources.context.get(), FFX_FSR3UPSCALER_CONFIGURE_UPSCALE_KEY_FACCUMULATIONADDEDPERFRAME, &accumulation_added_per_frame);
      FfxFsr3UpscalerSharedResourceDescriptions shared = {};
      CheckFsr(ffxFsr3UpscalerGetSharedResourceDescriptions(resources.context.get(), &shared), "Get FSR shared resource descriptions");
      const std::array descriptions = {shared.reconstructedPrevNearestDepth.resourceDescription, shared.dilatedDepth.resourceDescription, shared.dilatedMotionVectors.resourceDescription};
      const std::array outputs = {std::addressof(resources.reconstructed_depth), std::addressof(resources.dilated_depth), std::addressof(resources.dilated_motion)};
      for (size_t index = 0u; index < outputs.size(); ++index) {
        const auto& texture = descriptions[index];
        *outputs[index] = dx12::compute::CreateTexture(evaluation.device, texture.width, texture.height, backend::ToDxgiFormat(static_cast<FfxApiSurfaceFormat>(texture.format)));
      }
      resources.linear_output = dx12::compute::CreateTexture(evaluation.device, inputs.output.width, inputs.output.height, DXGI_FORMAT_R16G16B16A16_FLOAT);
      resources.render_width = inputs.render_width;
      resources.render_height = inputs.render_height;
      resources.output_width = inputs.output.width;
      resources.output_height = inputs.output.height;
      logging::Info("created D3D12 FSR 3.1.5 context render=", inputs.render_width, "x", inputs.render_height,
                    " output=", inputs.output.width, "x", inputs.output.height);
    }
    // The SDK advances its CPU bookkeeping while recording. If that list is
    // discarded, recreate the context before attempting another evaluation.
    resources.recording_failed = true;
    resources.backend->frame = evaluation.frame;
    const bool reset = !resources.initialized;
    const auto now = std::chrono::steady_clock::now();
    const float frame_ms = reset ? 1000.f / 60.f : std::clamp(std::chrono::duration<float, std::milli>(now - resources.previous_dispatch_time).count(), 1.f, 1000.f);
    resources.previous_dispatch_time = now;

    // Reverse-Z coefficients provide the real camera range and field of view.
    const float depth_x = inputs.camera.device_to_view_depth[0];
    const float depth_y = inputs.camera.device_to_view_depth[1];
    float camera_near = depth_y / (1.f - depth_x);
    float camera_far = -depth_y / depth_x;
    if (!std::isfinite(camera_near) || !std::isfinite(camera_far) || camera_near <= 0.f || camera_far <= camera_near) {
      camera_near = 0.1f;
      camera_far = 4000.f;
    }
    FfxFsr3UpscalerDispatchDescription dispatch = {};
    dispatch.commandList = evaluation.commands;
    dispatch.color = backend::Describe(evaluation.color, FFX_API_RESOURCE_STATE_COMPUTE_READ);
    dispatch.depth = backend::Describe(evaluation.depth, FFX_API_RESOURCE_STATE_COMPUTE_READ);
    dispatch.motionVectors = backend::Describe(evaluation.motion, FFX_API_RESOURCE_STATE_COMPUTE_READ);
    dispatch.dilatedDepth = backend::Describe(resources.dilated_depth.Get(), FFX_API_RESOURCE_STATE_COMMON);
    dispatch.dilatedMotionVectors = backend::Describe(resources.dilated_motion.Get(), FFX_API_RESOURCE_STATE_COMMON);
    dispatch.reconstructedPrevNearestDepth = backend::Describe(resources.reconstructed_depth.Get(), FFX_API_RESOURCE_STATE_COMMON);
    dispatch.output = backend::Describe(resources.linear_output.Get(), FFX_API_RESOURCE_STATE_COMMON);
    dispatch.jitterOffset = {.x = inputs.camera.jitter_uv_x * static_cast<float>(inputs.render_width), .y = inputs.camera.jitter_uv_y * static_cast<float>(inputs.render_height)};
    dispatch.motionVectorScale = {.x = static_cast<float>(inputs.render_width), .y = static_cast<float>(inputs.render_height)};
    dispatch.renderSize = {.width = inputs.render_width, .height = inputs.render_height};
    dispatch.upscaleSize = {.width = inputs.output.width, .height = inputs.output.height};
    dispatch.frameTimeDelta = frame_ms;
    dispatch.preExposure = 1.f;
    dispatch.reset = reset;
    dispatch.cameraNear = camera_near;
    dispatch.cameraFar = camera_far;
    dispatch.cameraFovAngleVertical = 2.f * std::atan(std::abs(inputs.camera.device_to_view_depth[3]));
    dispatch.viewSpaceToMetersFactor = 1.f;
    CheckFsr(ffxFsr3UpscalerContextDispatch(resources.context.get(), &dispatch), "Evaluate D3D12 FSR");
    // UnregisterResources restores external inputs to their supplied states.
    dx12::compute::Transition(evaluation.commands, resources.linear_output.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    dx12::Encode(evaluation, resources.linear_output.Get());
    dx12::compute::Transition(evaluation.commands, resources.linear_output.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
  });
  if (succeeded) {
    resources.recording_failed = false;
    resources.initialized = true;
  }
  return succeeded;
}

}  // namespace mgsv::reconstruction::fsr3

// Keep the pinned SDK host implementation local to MGSV's single translation unit.
#include "ffx/api/internal/ffx_assert.cpp"
#include "ffx/api/internal/ffx_message.cpp"
#include "ffx/api/internal/ffx_object_management.cpp"
#include "ffx/upscalers/fsr3/internal/ffx_fsr3upscaler.cpp"
#include "shader_blobs.cpp"

#endif  // RENODX_GAMES_MGSV_RECONSTRUCTION_FSR3_RUNTIME_HPP_
