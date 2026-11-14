#ifndef RENODX_GAMES_MGSV_RECONSTRUCTION_D3D12_HPP_
#define RENODX_GAMES_MGSV_RECONSTRUCTION_D3D12_HPP_

// Shared reconstruction transport. Native inputs stay on D3D11 until one
// preparation dispatch decodes color and writes the shared color/motion/depth.
// Evaluation and output encoding then run together on the bridge's D3D12 queue.

#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <embed/encode_reconstruction.h>
#include <embed/prepare_reconstruction.h>

#include "../rendering/d3d12_compute.hpp"
#include "frame_inputs.hpp"
#include "logging.hpp"
#include "state.hpp"

namespace mgsv::reconstruction::dx12 {

namespace compute = rendering::d3d12_compute;
using compute::Check;

struct InputTexture {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
  std::shared_ptr<rendering::SharedTexture> allocation;
  rendering::d3d12_bridge::SharedImage shared;
};

struct Runtime {
  std::unique_ptr<rendering::d3d12_bridge::Bridge> bridge;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> prepare;
  Microsoft::WRL::ComPtr<ID3D11Buffer> constants;
  InputTexture color;
  InputTexture motion;
  InputTexture depth;
  InputTexture motion_debug;
  compute::Pipeline encode;
  std::array<compute::Frame, rendering::d3d12_bridge::Bridge::MAX_IN_FLIGHT> frames;
  UINT width = 0u;
  UINT height = 0u;
};

// Access is serialized by the reconstruction coordinator, including Present
// transitions and NGX discovery. The D3D11 wait from each submission also keeps
// the next input preparation from overwriting images D3D12 is still reading.
static Runtime runtime;

inline rendering::d3d12_bridge::Bridge& GetBridge(ID3D11Device* device) {
  if (runtime.bridge == nullptr) {
    runtime.bridge = std::make_unique<rendering::d3d12_bridge::Bridge>(device);
    logging::Info("created shared D3D12 reconstruction backend");
  }
  if (runtime.bridge->device_identity != rendering::d3d12_bridge::GetDeviceIdentity(device)) {
    throw std::runtime_error("Reconstruction device changed without teardown");
  }
  return *runtime.bridge;
}

// Size/model/method changes and teardown may wait; ordinary frames never wait on the
// CPU. Do not release SDK histories until their last queued use has completed.
inline void Drain() {
  if (runtime.bridge != nullptr && !runtime.bridge->Drain()) {
    throw std::runtime_error("D3D12 reconstruction is still in use; retaining resources");
  }
}

inline void Destroy() {
  Drain();
  runtime = {};
}

inline InputTexture CreateInput(DXGI_FORMAT format, UINT width, UINT height) {
  auto& bridge = *runtime.bridge;
  const D3D11_TEXTURE2D_DESC description = {
      .Width = width,
      .Height = height,
      .MipLevels = 1u,
      .ArraySize = 1u,
      .Format = format,
      .SampleDesc = {.Count = 1u},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
      .MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE,
  };
  InputTexture input;
  Check(bridge.device11->CreateTexture2D(&description, nullptr, &input.texture), "Create shared reconstruction input");
  Check(bridge.device11->CreateUnorderedAccessView(input.texture.Get(), nullptr, &input.uav), "Create reconstruction input UAV");
  input.allocation = rendering::ExportTexture(input.texture.Get());
  if (input.allocation == nullptr) throw std::runtime_error("Cannot export reconstruction input");
  input.shared = rendering::d3d12_bridge::SharedImage(bridge.device11.Get(), bridge.device12.Get(), input.texture.Get(), description,
                                                      D3D11_BIND_SHADER_RESOURCE, input.allocation.get(), &bridge.statistics.image_imports);
  if (!input.shared.direct) throw std::runtime_error("Reconstruction input was not directly shared");
  return input;
}

struct Evaluation {
  ID3D12Device* device;
  ID3D12GraphicsCommandList* commands;
  compute::Frame* frame;
  ID3D12Resource* color;
  ID3D12Resource* motion;
  ID3D12Resource* depth;
  ID3D12Resource* motion_debug;
  ID3D12Resource* output;
  D3D12_GPU_VIRTUAL_ADDRESS constants;
};

inline void Encode(const Evaluation& evaluation, ID3D12Resource* linear_output) {
  const UINT first = evaluation.frame->Allocate(3u);
  evaluation.device->CreateShaderResourceView(linear_output, nullptr, evaluation.frame->Cpu(first));
  // Only alpha is read from this input. Its RGB is already linear, so there is
  // no second scene import or copy just to preserve the game's DoF mask.
  evaluation.device->CreateShaderResourceView(evaluation.color, nullptr, evaluation.frame->Cpu(first + 1u));
  evaluation.device->CreateUnorderedAccessView(evaluation.output, nullptr, nullptr, evaluation.frame->Cpu(first + 2u));
  runtime.encode.Bind(evaluation.commands, *evaluation.frame, first);
  evaluation.commands->SetComputeRootConstantBufferView(1u, evaluation.constants);
  const auto target = evaluation.output->GetDesc();
  evaluation.commands->Dispatch((static_cast<UINT>(target.Width) + 7u) / 8u, (target.Height + 7u) / 8u, 1u);
}

template <typename Record>
[[nodiscard]] bool Execute(const ValidatedFrameInputs& inputs, bool reset, Record record) {
  auto* device = reinterpret_cast<ID3D11Device*>(inputs.device->get_native());            // NOLINT(performance-no-int-to-ptr)
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(inputs.cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  const bool analytical = state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA;
  const UINT width = inputs.render_width;
  const UINT height = inputs.render_height;
  // Reuse the complete input bundle until the render dimensions change.
  auto& bridge = GetBridge(device);
  if (runtime.prepare == nullptr) {
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> prepare;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constants;
    compute::Pipeline encode;
    Check(device->CreateComputeShader(__prepare_reconstruction.data(), __prepare_reconstruction.size(), nullptr, &prepare), "Create reconstruction preparation shader");
    const D3D11_BUFFER_DESC description = {
        .ByteWidth = sizeof(PrepareConstants),
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    Check(device->CreateBuffer(&description, nullptr, &constants), "Create reconstruction constants");
    encode.Create(bridge.device12.Get(), __encode_reconstruction, 2u, 1u);
    runtime.prepare = std::move(prepare);
    runtime.constants = std::move(constants);
    runtime.encode = std::move(encode);
  }
  if (runtime.width != width || runtime.height != height) {
    Drain();
    // Construct all inputs before replacing the last usable set.
    auto color = CreateInput(DXGI_FORMAT_R16G16B16A16_FLOAT, width, height);
    auto motion = CreateInput(DXGI_FORMAT_R16G16_FLOAT, width, height);
    auto depth = CreateInput(DXGI_FORMAT_R32_FLOAT, width, height);
    runtime.color = std::move(color);
    runtime.motion = std::move(motion);
    runtime.depth = std::move(depth);
    runtime.motion_debug = {};
    runtime.width = width;
    runtime.height = height;
    logging::Info("created shared reconstruction inputs size=", width, "x", height);
  }
  if (analytical && runtime.motion_debug.texture == nullptr) {
    runtime.motion_debug = CreateInput(DXGI_FORMAT_R16G16B16A16_FLOAT, width, height);
  }

  const PrepareConstants constants = {
      .current_jitter_uv = {inputs.camera.jitter_uv_x, inputs.camera.jitter_uv_y},
      .velocity_projection_jitter_scale = state::GetProjectionJitterScale(state::ProjectionJitterPath::VELOCITY),
      .camera_reprojection_valid = !reset && inputs.camera.camera_reprojection_valid ? 1.f : 0.f,
      .render_size = {inputs.render_width, inputs.render_height},
      .reciprocal_render_size = {1.f / static_cast<float>(inputs.render_width), 1.f / static_cast<float>(inputs.render_height)},
      .current_to_previous_clip = inputs.camera.current_to_previous_clip,
      .previous_jitter_uv = {inputs.camera.previous_jitter_uv_x, inputs.camera.previous_jitter_uv_y},
      .object_motion_mode = static_cast<float>(state::GetObjectMotionMode()),
      .analytical_taa = (analytical ? 1.f : 0.f),
  };
  // Decode and prepare directly into shared textures, without a transfer copy.
  D3D11_MAPPED_SUBRESOURCE mapped = {};
  Check(context->Map(runtime.constants.Get(), 0u, D3D11_MAP_WRITE_DISCARD, 0u, &mapped), "Map reconstruction constants");
  std::memcpy(mapped.pData, &constants, sizeof(constants));
  context->Unmap(runtime.constants.Get(), 0u);
  // ReShade's D3D11 handles are native COM pointers. Only this boundary touches
  // game bindings; the SDK adapters receive D3D12 resources and commands.
  // NOLINTBEGIN(performance-no-int-to-ptr)
  const std::array<ID3D11ShaderResourceView*, 4> srvs = {
      reinterpret_cast<ID3D11ShaderResourceView*>(inputs.color_srv.handle),
      reinterpret_cast<ID3D11ShaderResourceView*>(inputs.velocity_srv.handle),
      reinterpret_cast<ID3D11ShaderResourceView*>(inputs.depth_srv.handle),
      reinterpret_cast<ID3D11ShaderResourceView*>(inputs.object_velocity_srv.handle),
  };
  // NOLINTEND(performance-no-int-to-ptr)
  std::array<ID3D11UnorderedAccessView*, 4> uavs = {
      runtime.color.uav.Get(),
      runtime.motion.uav.Get(),
      runtime.depth.uav.Get(),
      analytical ? runtime.motion_debug.uav.Get() : nullptr,
  };
  context->CSSetShader(runtime.prepare.Get(), nullptr, 0u);
  context->CSSetConstantBuffers(0u, 1u, runtime.constants.GetAddressOf());
  context->CSSetShaderResources(0u, static_cast<UINT>(srvs.size()), srvs.data());
  context->CSSetUnorderedAccessViews(0u, static_cast<UINT>(uavs.size()), uavs.data(), nullptr);
  context->Dispatch((inputs.render_width + 7u) / 8u, (inputs.render_height + 7u) / 8u, 1u);
  uavs.fill(nullptr);
  context->CSSetUnorderedAccessViews(0u, static_cast<UINT>(uavs.size()), uavs.data(), nullptr);
  const std::array<ID3D11ShaderResourceView*, 4> empty = {};
  context->CSSetShaderResources(0u, static_cast<UINT>(empty.size()), empty.data());
  const bool succeeded = runtime.bridge->Execute(
      context, runtime.color.texture.Get(),
      reinterpret_cast<ID3D11Texture2D*>(inputs.output.resource.handle),  // NOLINT(performance-no-int-to-ptr)
      {.id = inputs.camera.scene, .generation = inputs.camera.owner_generation},
      [&](const rendering::d3d12_bridge::Dispatch& dispatch) {
        auto& frame = runtime.frames[dispatch.slot_index];
        frame.Reset(runtime.bridge->device12.Get());
        const Evaluation evaluation = {
            .device = runtime.bridge->device12.Get(),
            .commands = dispatch.commands,
            .frame = &frame,
            .color = dispatch.input,
            .motion = runtime.motion.shared.resource.Get(),
            .depth = runtime.depth.shared.resource.Get(),
            .motion_debug = analytical ? runtime.motion_debug.shared.resource.Get() : nullptr,
            .output = dispatch.output,
            .constants = frame.Upload(&constants, sizeof(constants)),
        };
        for (auto* input : {evaluation.color, evaluation.motion, evaluation.depth, evaluation.motion_debug}) {
          if (input != nullptr) {
            compute::Transition(dispatch.commands, input, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
          }
        }
        compute::Transition(dispatch.commands, dispatch.output, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        record(evaluation);
        // Shared resources always return to COMMON before D3D11 resumes.
        compute::Transition(dispatch.commands, dispatch.output, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
        for (auto* input : {evaluation.color, evaluation.motion, evaluation.depth, evaluation.motion_debug}) {
          if (input != nullptr) {
            compute::Transition(dispatch.commands, input, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
          }
        }
      },
      runtime.color.allocation.get(), inputs.output.shared_texture.get());
  if (!succeeded && runtime.bridge->rejection != nullptr) {
    // Rejected bindings never reach the SDK callback. They invalidate this
    // scene's history, not the installed SDK. A partial GPU submission is fatal.
    if (runtime.bridge->failed) throw std::runtime_error(runtime.bridge->rejection);
    static uint64_t last_rejection_log = std::numeric_limits<uint64_t>::max();
    if (logging::ShouldLogFrame(state::CurrentFrameToken(), last_rejection_log)) {
      logging::Warn("reconstruction submission rejected insertion=", inputs.insertion_name,
                    " scene=", inputs.camera.scene, " reason=", runtime.bridge->rejection);
    }
  }
  return succeeded;
}

}  // namespace mgsv::reconstruction::dx12

#endif  // RENODX_GAMES_MGSV_RECONSTRUCTION_D3D12_HPP_
