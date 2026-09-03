#pragma once

/*
 * MGSV boundary adapter and lifetime owner for AMD FSR3 Upscaler 3.1.5.
 * AMD's host implementation owns the pass schedule; this file only prepares
 * proven game inputs, supplies caller-owned shared resources, and restores the
 * game's encoded scene-color contract after the linear upscaler output.
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include <embed/fsr3_encode_game_output.h>
#include <embed/fsr3_prepare_game_inputs.h>
#include <include/reshade.hpp>

#include "backend_dx11.hpp"
#include "ffx/upscalers/fsr3/include/ffx_fsr3upscaler.h"
#include "../runtime/logging.hpp"
#include "../runtime/projection_jitter.hpp"
#include "../runtime/resolve.hpp"
#include "../runtime/state.hpp"

#ifndef FFX_FSR3UPSCALER_DISABLE_WATERMARK
#define FFX_FSR3UPSCALER_DISABLE_WATERMARK 1
#endif

namespace taa::fsr3 {

inline constexpr uint32_t THREAD_GROUP_SIZE = 8u;

struct Texture {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
  FfxApiResourceDescription description = {};
};

struct alignas(16) PrepareConstants {
  std::array<float, 2> current_jitter_uv = {};
  float velocity_projection_jitter_scale = 1.f;
  float camera_reprojection_valid = 0.f;
  std::array<uint32_t, 2> render_size = {};
  std::array<float, 2> reciprocal_render_size = {};
  std::array<float, 16> current_to_previous_clip = {};
};

static_assert(sizeof(PrepareConstants) == 96u, "MGSV FSR3 input constants must occupy 24 dwords");

struct Resources {
  FfxFsr3UpscalerContext context = {};
  std::vector<std::max_align_t> backend_scratch;
  Microsoft::WRL::ComPtr<ID3D11Device> device;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> prepare_shader;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> encode_shader;
  Microsoft::WRL::ComPtr<ID3D11Buffer> prepare_constant_buffer;

  Texture linear_input;
  Texture motion_input;
  Texture linear_output;
  Texture encoded_output;
  Texture reconstructed_previous_nearest_depth;
  Texture dilated_depth;
  Texture dilated_motion;

  uint32_t width = 0u;
  uint32_t height = 0u;
  uint64_t settings_generation = std::numeric_limits<uint64_t>::max();
  bool context_created = false;
  bool initialized = false;
  std::chrono::steady_clock::time_point previous_dispatch_time;
};

inline Resources resources = {};
inline uint64_t last_failure_log = std::numeric_limits<uint64_t>::max();

inline bool LogEvery(uint64_t interval = 120u) {
  return logging::ShouldLogFrame(state::CurrentFrameToken(), last_failure_log, interval);
}

inline bool IsCameraDiscontinuity(const projection_jitter::AppliedJitter& native_jitter) {
  if (!native_jitter.camera_reprojection_valid) return true;

  // A valid matrix pair can still span a cut. Mid-depth center/corners catch
  // large lateral, FOV, and roll discontinuities without treating ordinary
  // nonlinear near/far depth motion as a cut.
  constexpr std::array<std::array<float, 3>, 5> clip_samples = {{
      {0.f, 0.f, 0.5f},
      {-1.f, -1.f, 0.5f},
      {1.f, -1.f, 0.5f},
      {-1.f, 1.f, 0.5f},
      {1.f, 1.f, 0.5f},
  }};
  const auto is_discontinuous = [&](const std::array<float, 3>& sample) {
    const auto& matrix = native_jitter.current_to_previous_clip;
    const float previous_x = (matrix[0] * sample[0]) + (matrix[1] * sample[1])
                             + (matrix[2] * sample[2]) + matrix[3];
    const float previous_y = (matrix[4] * sample[0]) + (matrix[5] * sample[1])
                             + (matrix[6] * sample[2]) + matrix[7];
    const float previous_w = (matrix[12] * sample[0]) + (matrix[13] * sample[1])
                             + (matrix[14] * sample[2]) + matrix[15];
    if (!std::isfinite(previous_w) || previous_w <= 1e-6f) return true;
    const float previous_x_ndc = previous_x / previous_w;
    const float previous_y_ndc = previous_y / previous_w;
    return !std::isfinite(previous_x_ndc)
           || !std::isfinite(previous_y_ndc)
           || std::abs(previous_w - 1.f) > 0.5f
           || std::abs(previous_x_ndc - sample[0]) > 0.75f
           || std::abs(previous_y_ndc - sample[1]) > 0.75f;
  };
  return std::any_of(clip_samples.begin(), clip_samples.end(), is_discontinuous);
}

inline DXGI_FORMAT ToDxgiFormat(FfxApiSurfaceFormat format) {
  switch (format) {
    case FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case FFX_API_SURFACE_FORMAT_R16G16_FLOAT: return DXGI_FORMAT_R16G16_FLOAT;
    case FFX_API_SURFACE_FORMAT_R32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
    case FFX_API_SURFACE_FORMAT_R32_UINT: return DXGI_FORMAT_R32_UINT;
    default: return DXGI_FORMAT_UNKNOWN;
  }
}

inline FfxApiResourceDescription MakeDescription(
    FfxApiSurfaceFormat format,
    uint32_t width,
    uint32_t height,
    FfxApiResourceUsage usage) {
  return FfxApiResourceDescription{
      .type = FFX_API_RESOURCE_TYPE_TEXTURE2D,
      .format = static_cast<uint32_t>(format),
      .width = width,
      .height = height,
      .depth = 1u,
      .mipCount = 1u,
      .flags = FFX_API_RESOURCE_FLAGS_NONE,
      .usage = static_cast<uint32_t>(usage),
  };
}

inline bool CreateTexture(
    ID3D11Device* device,
    Texture& output,
    const FfxApiResourceDescription& ffx_description,
    bool create_local_views) {
  if (device == nullptr || ffx_description.width == 0u || ffx_description.height == 0u) return false;
  const DXGI_FORMAT format = ToDxgiFormat(static_cast<FfxApiSurfaceFormat>(ffx_description.format));
  if (format == DXGI_FORMAT_UNKNOWN) return false;

  D3D11_TEXTURE2D_DESC description = {};
  description.Width = ffx_description.width;
  description.Height = ffx_description.height;
  description.MipLevels = 1u;
  description.ArraySize = 1u;
  description.Format = format;
  description.SampleDesc.Count = 1u;
  description.Usage = D3D11_USAGE_DEFAULT;
  description.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
  if (FAILED(device->CreateTexture2D(&description, nullptr, &output.texture))) return false;
  output.description = ffx_description;
  output.description.mipCount = 1u;

  if (!create_local_views) return true;
  if (FAILED(device->CreateShaderResourceView(output.texture.Get(), nullptr, &output.srv))) return false;
  if (FAILED(device->CreateUnorderedAccessView(output.texture.Get(), nullptr, &output.uav))) return false;
  return true;
}

inline void Destroy() {
  if (resources.context_created) {
    ffxFsr3UpscalerContextDestroy(&resources.context);
  }
  resources.context_created = false;
  std::memset(&resources.context, 0, sizeof(resources.context));
  std::vector<std::max_align_t>().swap(resources.backend_scratch);
  resources.device.Reset();
  resources.prepare_shader.Reset();
  resources.encode_shader.Reset();
  resources.prepare_constant_buffer.Reset();
  for (Texture* texture : std::array{
           &resources.linear_input,
           &resources.motion_input,
           &resources.linear_output,
           &resources.encoded_output,
           &resources.reconstructed_previous_nearest_depth,
           &resources.dilated_depth,
           &resources.dilated_motion,
       }) {
    texture->texture.Reset();
    texture->srv.Reset();
    texture->uav.Reset();
    texture->description = {};
  }
  resources.width = 0u;
  resources.height = 0u;
  resources.settings_generation = std::numeric_limits<uint64_t>::max();
  resources.initialized = false;
  resources.previous_dispatch_time = {};
}

inline void ReleaseTemporalResources(reshade::api::device* device) {
  (void)device;
  if (!resources.context_created) return;
  const uint32_t width = resources.width;
  const uint32_t height = resources.height;
  Destroy();
  logging::Info("released inactive AMD FSR3.1 resources size=", width, "x", height);
}

inline bool CreateComputeShader(
    ID3D11Device* device,
    std::span<const uint8_t> code,
    Microsoft::WRL::ComPtr<ID3D11ComputeShader>& output) {
  return device != nullptr
         && SUCCEEDED(device->CreateComputeShader(code.data(), code.size(), nullptr, &output));
}

inline bool CreateResources(ID3D11Device* device, uint32_t width, uint32_t height) {
  Destroy();
  if (device == nullptr || device->GetFeatureLevel() < D3D_FEATURE_LEVEL_11_0) return false;

  resources.device = device;
  resources.width = width;
  resources.height = height;
  const size_t scratch_size = dx11::GetScratchMemorySize(FFX_FSR3UPSCALER_CONTEXT_COUNT);
  resources.backend_scratch.resize(
      (scratch_size + sizeof(std::max_align_t) - 1u) / sizeof(std::max_align_t));

  FfxInterface backend_interface = {};
  FfxErrorCode result = dx11::GetInterface(
      &backend_interface,
      device,
      resources.backend_scratch.data(),
      resources.backend_scratch.size() * sizeof(std::max_align_t),
      FFX_FSR3UPSCALER_CONTEXT_COUNT);
  if (result != FFX_OK) {
    if (LogEvery(1u)) logging::Warn("failed to initialize FSR3.1 D3D11 backend error=", result);
    Destroy();
    return false;
  }

  FfxFsr3UpscalerContextDescription context_description = {};
  context_description.flags = FFX_FSR3UPSCALER_ENABLE_HIGH_DYNAMIC_RANGE
                              | FFX_FSR3UPSCALER_ENABLE_DEPTH_INVERTED;
  context_description.maxRenderSize = {width, height};
  context_description.maxUpscaleSize = {width, height};
  context_description.backendInterface = backend_interface;
  result = ffxFsr3UpscalerContextCreate(&resources.context, &context_description);
  if (result != FFX_OK) {
    if (LogEvery(1u)) {
      logging::Warn("FSR3.1 D3D11 context probe failed feature_level=",
                    static_cast<uint32_t>(device->GetFeatureLevel()),
                    " error=", result);
    }
    // The valid interface means failures occur after the backend context is
    // initialized, so the public destroy path safely releases partial state.
    ffxFsr3UpscalerContextDestroy(&resources.context);
    Destroy();
    return false;
  }
  resources.context_created = true;

  if (!CreateComputeShader(device, __fsr3_prepare_game_inputs, resources.prepare_shader)
      || !CreateComputeShader(device, __fsr3_encode_game_output, resources.encode_shader)) {
    if (LogEvery(1u)) logging::Warn("failed to create MGSV FSR3.1 boundary shaders");
    Destroy();
    return false;
  }

  D3D11_BUFFER_DESC constant_buffer_description = {};
  constant_buffer_description.ByteWidth = sizeof(PrepareConstants);
  constant_buffer_description.Usage = D3D11_USAGE_DYNAMIC;
  constant_buffer_description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  constant_buffer_description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  if (FAILED(device->CreateBuffer(
          &constant_buffer_description,
          nullptr,
          &resources.prepare_constant_buffer))) {
    Destroy();
    return false;
  }

  const FfxApiResourceUsage sampled_uav = static_cast<FfxApiResourceUsage>(
      FFX_API_RESOURCE_USAGE_READ_ONLY | FFX_API_RESOURCE_USAGE_UAV);
  if (!CreateTexture(
          device,
          resources.linear_input,
          MakeDescription(FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT, width, height, sampled_uav),
          true)
      || !CreateTexture(
          device,
          resources.motion_input,
          MakeDescription(FFX_API_SURFACE_FORMAT_R16G16_FLOAT, width, height, sampled_uav),
          true)
      || !CreateTexture(
          device,
          resources.linear_output,
          MakeDescription(FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT, width, height, sampled_uav),
          true)
      || !CreateTexture(
          device,
          resources.encoded_output,
          MakeDescription(FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT, width, height, sampled_uav),
          true)) {
    Destroy();
    return false;
  }

  FfxFsr3UpscalerSharedResourceDescriptions shared_descriptions = {};
  result = ffxFsr3UpscalerGetSharedResourceDescriptions(&resources.context, &shared_descriptions);
  if (result != FFX_OK
      || !CreateTexture(
          device,
          resources.reconstructed_previous_nearest_depth,
          shared_descriptions.reconstructedPrevNearestDepth.resourceDescription,
          false)
      || !CreateTexture(
          device,
          resources.dilated_depth,
          shared_descriptions.dilatedDepth.resourceDescription,
          false)
      || !CreateTexture(
          device,
          resources.dilated_motion,
          shared_descriptions.dilatedMotionVectors.resourceDescription,
          false)) {
    if (LogEvery(1u)) logging::Warn("failed to create FSR3.1 shared resources error=", result);
    Destroy();
    return false;
  }

  logging::Info("FSR3.1 D3D11 context probe succeeded feature_level=",
                static_cast<uint32_t>(device->GetFeatureLevel()),
                " size=", width, "x", height,
                " host_version=", FFX_FSR3UPSCALER_VERSION_MAJOR, ".",
                FFX_FSR3UPSCALER_VERSION_MINOR, ".", FFX_FSR3UPSCALER_VERSION_PATCH,
                " backend_interface=", FFX_SDK_VERSION_MAJOR, ".",
                FFX_SDK_VERSION_MINOR, ".", FFX_SDK_VERSION_PATCH);
  return true;
}

inline bool EnsureResources(ID3D11Device* device, uint32_t width, uint32_t height) {
  if (resources.context_created
      && resources.device.Get() == device
      && resources.width == width
      && resources.height == height) {
    return true;
  }
  return CreateResources(device, width, height);
}

inline void UnbindComputeResources(ID3D11DeviceContext* context) {
  static const std::array<ID3D11ShaderResourceView*, 16> NULL_SRVS = {};
  static const std::array<ID3D11UnorderedAccessView*, 8> NULL_UAVS = {};
  context->CSSetShaderResources(0u, static_cast<UINT>(NULL_SRVS.size()), NULL_SRVS.data());
  context->CSSetUnorderedAccessViews(0u, static_cast<UINT>(NULL_UAVS.size()), NULL_UAVS.data(), nullptr);
}

inline bool PrepareGameInputs(
    ID3D11DeviceContext* context,
    reshade::api::resource_view color_srv,
    const projection_jitter::AppliedJitter& native_jitter,
    bool reset) {
  auto& captured = resolve::resources;
  const std::array<ID3D11ShaderResourceView*, 4> srvs = {
      reinterpret_cast<ID3D11ShaderResourceView*>(color_srv.handle),  // NOLINT(performance-no-int-to-ptr)
      reinterpret_cast<ID3D11ShaderResourceView*>(captured.velocity_srv.handle),  // NOLINT(performance-no-int-to-ptr)
      reinterpret_cast<ID3D11ShaderResourceView*>(captured.depth_srv.handle),  // NOLINT(performance-no-int-to-ptr)
      reinterpret_cast<ID3D11ShaderResourceView*>(captured.object_velocity_srv.handle),  // NOLINT(performance-no-int-to-ptr)
  };
  const std::array<ID3D11UnorderedAccessView*, 2> uavs = {
      resources.linear_input.uav.Get(),
      resources.motion_input.uav.Get(),
  };
  const PrepareConstants constants = {
      .current_jitter_uv = {native_jitter.jitter_uv_x, native_jitter.jitter_uv_y},
      .velocity_projection_jitter_scale = state::GetProjectionJitterScale(state::ProjectionJitterPath::VELOCITY),
      .camera_reprojection_valid = !reset && native_jitter.camera_reprojection_valid ? 1.f : 0.f,
      .render_size = {resources.width, resources.height},
      .reciprocal_render_size = {
          1.f / static_cast<float>(resources.width),
          1.f / static_cast<float>(resources.height),
      },
      .current_to_previous_clip = native_jitter.current_to_previous_clip,
  };

  D3D11_MAPPED_SUBRESOURCE mapped = {};
  if (FAILED(context->Map(
          resources.prepare_constant_buffer.Get(),
          0u,
          D3D11_MAP_WRITE_DISCARD,
          0u,
          &mapped))) {
    return false;
  }
  std::memcpy(mapped.pData, &constants, sizeof(constants));
  context->Unmap(resources.prepare_constant_buffer.Get(), 0u);

  UnbindComputeResources(context);
  context->CSSetShaderResources(0u, static_cast<UINT>(srvs.size()), srvs.data());
  context->CSSetUnorderedAccessViews(0u, static_cast<UINT>(uavs.size()), uavs.data(), nullptr);
  ID3D11Buffer* constant_buffer = resources.prepare_constant_buffer.Get();
  context->CSSetConstantBuffers(0u, 1u, &constant_buffer);
  context->CSSetShader(resources.prepare_shader.Get(), nullptr, 0u);
  context->Dispatch(
      (resources.width + THREAD_GROUP_SIZE - 1u) / THREAD_GROUP_SIZE,
      (resources.height + THREAD_GROUP_SIZE - 1u) / THREAD_GROUP_SIZE,
      1u);
  UnbindComputeResources(context);
  return true;
}

inline bool EncodeGameOutput(
    ID3D11DeviceContext* context,
    reshade::api::resource_view encoded_scene_srv) {
  const std::array<ID3D11ShaderResourceView*, 2> srvs = {
      resources.linear_output.srv.Get(),
      reinterpret_cast<ID3D11ShaderResourceView*>(encoded_scene_srv.handle),  // NOLINT(performance-no-int-to-ptr)
  };
  ID3D11UnorderedAccessView* output = resources.encoded_output.uav.Get();
  UnbindComputeResources(context);
  context->CSSetShaderResources(0u, static_cast<UINT>(srvs.size()), srvs.data());
  context->CSSetUnorderedAccessViews(0u, 1u, &output, nullptr);
  context->CSSetShader(resources.encode_shader.Get(), nullptr, 0u);
  context->Dispatch(
      (resources.width + THREAD_GROUP_SIZE - 1u) / THREAD_GROUP_SIZE,
      (resources.height + THREAD_GROUP_SIZE - 1u) / THREAD_GROUP_SIZE,
      1u);
  UnbindComputeResources(context);
  return true;
}

inline float FrameDeltaMilliseconds(bool reset) {
  const auto now = std::chrono::steady_clock::now();
  float milliseconds = 1000.f / 60.f;
  if (!reset && resources.previous_dispatch_time.time_since_epoch().count() != 0) {
    milliseconds = std::chrono::duration<float, std::milli>(now - resources.previous_dispatch_time).count();
  }
  resources.previous_dispatch_time = now;
  return std::clamp(milliseconds, 1.f, 1000.f);
}

inline FfxApiResource MakeResource(
    const Texture& texture,
    FfxApiResourceState state_value) {
  return dx11::GetResource(texture.texture.Get(), texture.description, state_value);
}

inline bool Dispatch(
    reshade::api::command_list* cmd_list,
    reshade::api::resource_view color_srv,
    reshade::api::resource color_resource,
    reshade::api::resource_usage color_initial_usage,
    reshade::api::resource_usage color_final_usage,
    const projection_jitter::AppliedJitter& native_jitter,
    bool reset) {
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (context == nullptr) return false;
  const auto previous_compute_state = resolve::CaptureComputeState(cmd_list);
  auto& captured = resolve::resources;

  if (color_initial_usage != reshade::api::resource_usage::shader_resource) {
    cmd_list->barrier(color_resource, color_initial_usage, reshade::api::resource_usage::shader_resource);
  }
  cmd_list->barrier(
      captured.velocity_resource,
      reshade::api::resource_usage::render_target,
      reshade::api::resource_usage::shader_resource);

  bool succeeded = PrepareGameInputs(context, color_srv, native_jitter, reset);
  if (succeeded) {
    const auto depth_resource = cmd_list->get_device()->get_resource_from_view(captured.depth_srv);
    if (depth_resource.handle == 0u) {
      succeeded = false;
    } else {
      auto depth_description = dx11::GetResourceDescription(
          reinterpret_cast<ID3D11Resource*>(depth_resource.handle));  // NOLINT(performance-no-int-to-ptr)
      depth_description.format = FFX_API_SURFACE_FORMAT_R32_FLOAT;
      depth_description.usage = FFX_API_RESOURCE_USAGE_READ_ONLY;

      const float depth_x = native_jitter.device_to_view_depth[0];
      const float depth_y = native_jitter.device_to_view_depth[1];
      float camera_near = depth_y / (1.f - depth_x);
      float camera_far = -depth_y / depth_x;
      if (!std::isfinite(camera_near) || !std::isfinite(camera_far)
          || camera_near <= 0.f || camera_far <= camera_near) {
        camera_near = 0.1f;
        camera_far = 4000.f;
      }

      FfxFsr3UpscalerDispatchDescription dispatch = {};
      dispatch.commandList = context;
      dispatch.color = MakeResource(resources.linear_input, FFX_API_RESOURCE_STATE_COMPUTE_READ);
      dispatch.depth = dx11::GetResource(
          reinterpret_cast<ID3D11Resource*>(depth_resource.handle),  // NOLINT(performance-no-int-to-ptr)
          depth_description,
          FFX_API_RESOURCE_STATE_COMPUTE_READ);
      dispatch.motionVectors = MakeResource(resources.motion_input, FFX_API_RESOURCE_STATE_COMPUTE_READ);
      dispatch.dilatedDepth = MakeResource(resources.dilated_depth, FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
      dispatch.dilatedMotionVectors = MakeResource(resources.dilated_motion, FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
      dispatch.reconstructedPrevNearestDepth = MakeResource(
          resources.reconstructed_previous_nearest_depth,
          FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
      dispatch.output = MakeResource(resources.linear_output, FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
      dispatch.jitterOffset = {
          native_jitter.jitter_uv_x * static_cast<float>(resources.width),
          native_jitter.jitter_uv_y * static_cast<float>(resources.height),
      };
      dispatch.motionVectorScale = {
          static_cast<float>(resources.width),
          static_cast<float>(resources.height),
      };
      dispatch.renderSize = {resources.width, resources.height};
      dispatch.upscaleSize = {resources.width, resources.height};
      dispatch.enableSharpening = false;
      dispatch.sharpness = 0.f;
      dispatch.frameTimeDelta = FrameDeltaMilliseconds(reset);
      dispatch.preExposure = 1.f;
      dispatch.reset = reset;
      dispatch.cameraNear = camera_near;
      dispatch.cameraFar = camera_far;
      dispatch.cameraFovAngleVertical = 2.f * std::atan(std::abs(native_jitter.device_to_view_depth[3]));
      dispatch.viewSpaceToMetersFactor = 1.f;
      dispatch.flags = 0u;
      succeeded = ffxFsr3UpscalerContextDispatch(&resources.context, &dispatch) == FFX_OK;
    }
  }

  if (succeeded) {
    succeeded = EncodeGameOutput(context, color_srv);
  }
  if (succeeded) {
    cmd_list->barrier(
        color_resource,
        reshade::api::resource_usage::shader_resource,
        reshade::api::resource_usage::copy_dest);
    context->CopyResource(
        reinterpret_cast<ID3D11Resource*>(color_resource.handle),  // NOLINT(performance-no-int-to-ptr)
        resources.encoded_output.texture.Get());
    cmd_list->barrier(color_resource, reshade::api::resource_usage::copy_dest, color_final_usage);
  } else if (color_final_usage != reshade::api::resource_usage::shader_resource) {
    cmd_list->barrier(color_resource, reshade::api::resource_usage::shader_resource, color_final_usage);
  }
  cmd_list->barrier(
      captured.velocity_resource,
      reshade::api::resource_usage::shader_resource,
      reshade::api::resource_usage::render_target);
  resolve::RestoreComputeState(cmd_list, previous_compute_state);
  return succeeded;
}

inline bool TryResolve(
    reshade::api::command_list* cmd_list,
    reshade::api::resource_view color_srv,
    reshade::api::resource_usage color_initial_usage,
    reshade::api::resource_usage color_final_usage,
    const projection_jitter::AppliedJitter& native_jitter,
    const char* insertion_name) {
  auto* device = cmd_list != nullptr ? cmd_list->get_device() : nullptr;
  if (device == nullptr || device->get_api() != reshade::api::device_api::d3d11) return false;

  const auto color_resource = device->get_resource_from_view(color_srv);
  if (color_resource.handle == 0u) return false;
  const auto color_description = device->get_resource_desc(color_resource);
  const auto color_view_description = device->get_resource_view_desc(color_srv);
  const auto color_format = resolve::GetTypedViewFormat(device, color_srv);
  const auto depth_format = resolve::GetTypedViewFormat(device, resolve::resources.depth_srv);
  const bool compatible = color_description.type == reshade::api::resource_type::texture_2d
                          && color_description.texture.depth_or_layers == 1u
                          && color_description.texture.levels == 1u
                          && color_description.texture.samples == 1u
                          && color_view_description.type == reshade::api::resource_view_type::texture_2d
                          && color_view_description.texture.first_level == 0u
                          && color_view_description.texture.first_layer == 0u
                          && color_format == reshade::api::format::r16g16b16a16_float
                            && (depth_format == reshade::api::format::r32_float
                              || depth_format == reshade::api::format::r32_float_x8_uint);
  if (!compatible) {
    if (LogEvery(30u)) {
      logging::Warn("rejecting AMD FSR3.1 dispatch with incompatible resources insertion=", insertion_name,
                    " color_format=", static_cast<uint32_t>(color_format),
                    " depth_format=", static_cast<uint32_t>(depth_format));
    }
    return false;
  }
  if (native_jitter.width != color_description.texture.width
      || native_jitter.height != color_description.texture.height) {
    return false;
  }

  auto* native_device = reinterpret_cast<ID3D11Device*>(device->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (!EnsureResources(
          native_device,
          color_description.texture.width,
          color_description.texture.height)) {
    return false;
  }
  resolve::ReleaseHistory(device);

  const uint64_t settings_generation = state::RuntimeSettingsGeneration();
  const bool reset = !resources.initialized
                     || resources.settings_generation != settings_generation
                     || IsCameraDiscontinuity(native_jitter);
  if (!Dispatch(
          cmd_list,
          color_srv,
          color_resource,
          color_initial_usage,
          color_final_usage,
          native_jitter,
          reset)) {
    resources.initialized = false;
    if (LogEvery(30u)) logging::Warn("AMD FSR3.1 host dispatch failed insertion=", insertion_name);
    return false;
  }

  if (reset) {
    logging::Info("AMD FSR3.1 accumulation started insertion=", insertion_name,
                  " frame=", state::CurrentFrameToken(),
                  " native_frame=", native_jitter.frame_token,
                  " sample=", state::CurrentSampleIndex(),
                  " size=", resources.width, "x", resources.height);
  }
  resources.initialized = true;
  resources.settings_generation = settings_generation;

  if (!projection_jitter::CommitCameraMatrix(native_jitter.frame_token, state::CurrentSampleIndex())) {
    resources.initialized = false;
    resolve::InvalidateHistory("AMD FSR3.1 native camera matrix commit failed");
    logging::Warn("AMD FSR3.1 native camera matrix commit failed");
  }
  state::MarkTaaDispatched();
  return true;
}

}  // namespace taa::fsr3

// MGSV addons are intentionally built as a single translation unit. Keep the
// pinned host and custom backend local to this game without modifying core CMake.
#include "backend_dx11.cpp"
#include "shader_blobs.cpp"
#include "ffx/api/internal/ffx_assert.cpp"
#include "ffx/api/internal/ffx_message.cpp"
#include "ffx/api/internal/ffx_object_management.cpp"
#include "ffx/upscalers/fsr3/internal/ffx_fsr3upscaler.cpp"
