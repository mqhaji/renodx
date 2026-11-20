#ifndef RENODX_GAMES_MGSV_RECONSTRUCTION_D3D12_HPP_
#define RENODX_GAMES_MGSV_RECONSTRUCTION_D3D12_HPP_

// Moves a scene's inputs from D3D11 to D3D12 for every reconstruction method.
// One D3D11 compute dispatch (prepare_reconstruction) decodes the scene color
// and writes color, motion and depth into textures shared with D3D12. The
// method's evaluation and the output encoding (encode_reconstruction) then run
// together on the D3D12 bridge's queue.

#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <embed/encode_reconstruction.h>
#include <embed/prepare_reconstruction.h>

#include "../engine/post_filter.hpp"
#include "../rendering/d3d12_compute.hpp"
#include "../rendering/d3d12_timing.hpp"
#include "frame_inputs.hpp"
#include "logging.hpp"
#include "probe_api.hpp"
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
  rendering::d3d12_bridge::Bridge* bridge = nullptr;
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

// Used only under the submission execution lock, including at Present and
// while DLSS probes the device. Each submission queues a D3D11 wait on the
// D3D12 work, so the next preparation cannot overwrite inputs D3D12 is still
// reading.
static Runtime runtime;

// Live replacement of the preparation shader for the lighting probe
// (probe_api.hpp). QueueProbeReload stores a request; the scene's dispatch applies
// it before the next reconstruction.
struct ProbeUpdate {
  std::vector<uint8_t> code;
  uint64_t revision = 0u;
  bool restore = false;
};
struct ProbeState {
  std::shared_mutex mutex;
  ProbeUpdate pending;
  probe::Status status;
  // The probe bytecode in use, empty for the built-in shader. Kept as bytes so
  // the shader can be rebuilt on a new device. Changed only during submission,
  // under the execution lock.
  std::vector<uint8_t> active_code;
  bool report_submission = false;
};
static ProbeState probe_state;
static std::atomic<bool> probe_pending = false;

inline bool QueueProbeReload(const probe::Request* request, probe::Status* status) {
  if (request == nullptr || status == nullptr
      || (!request->restore && (request->code == nullptr || request->size == 0u || request->size > 16u * 1024u * 1024u))) return false;
  try {
    const std::unique_lock lock(probe_state.mutex);
    if (request->expected_revision != probe_state.status.revision) return false;
    std::vector<uint8_t> code;
    if (!request->restore) {
      code.assign(request->code, request->code + request->size);
    }
    probe_state.pending.code = std::move(code);
    probe_state.pending.restore = request->restore;
    probe_state.pending.revision = ++probe_state.status.revision;
    probe_state.status.pending = true;
    probe_state.status.error = {};
    *status = probe_state.status;
    probe_pending.store(true, std::memory_order_release);
    return true;
  } catch (...) {
    return false;  // Never unwind an allocation failure through another DLL.
  }
}

inline void ReadProbeStatus(probe::Status* status) {
  if (status == nullptr) return;
  const std::shared_lock lock(probe_state.mutex);
  *status = probe_state.status;
  const auto counts = engine::post_filter::ReadCounts();
  status->post_filter = {
      .starts = counts.starts,
      .cuts = counts.cuts,
      .lists_open_at_start = counts.lists_open_at_start,
      .unused_starts = counts.unused_starts,
      .repeated_starts = counts.repeated_starts,
  };
}

inline void UseProbe(bool attach) {
  if (attach) {
    probe::shared.RegisterModule();
    if (probe::shared.data == nullptr) return;
    const std::unique_lock lock(probe::shared.data->mutex);
    probe::shared.data->request = QueueProbeReload;
    probe::shared.data->read = ReadProbeStatus;
  } else if (probe::shared.data != nullptr) {
    {
      const std::unique_lock lock(probe::shared.data->mutex);
      probe::shared.data->request = nullptr;
      probe::shared.data->read = nullptr;
    }
    probe::shared.UnregisterModule();
  }
}

inline rendering::d3d12_bridge::Bridge& GetBridge(ID3D11Device* device) {
  runtime.bridge = &rendering::d3d12_bridge::Get(device);
  return *runtime.bridge;
}

// Waits on the CPU until all queued D3D12 work is done. Only size, model and
// method changes and teardown wait; ordinary frames never do. Release SDK
// history only after its last queued use has completed.
inline void Drain() {
  rendering::d3d12_bridge::Drain();
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

// What a method's record callback gets for one scene: the D3D12 command list
// and the shared inputs and output, already in shader-readable states.
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
  // GPU timing of this recording, active only on a diagnostic request.
  rendering::d3d12_timing::Scope* timing;
};

// Writes an SDK's linear result to the output in the game's encoded scene
// format, with alpha from the native scene.
inline void Encode(const Evaluation& evaluation, ID3D12Resource* linear_output) {
  if (evaluation.timing != nullptr) {
    evaluation.timing->Mark("evaluate");
  }
  const UINT first = evaluation.frame->Allocate(3u);
  evaluation.device->CreateShaderResourceView(linear_output, nullptr, evaluation.frame->Cpu(first));
  // Only alpha is read from the shared color input. The game's depth of field
  // reads that alpha, and this avoids a second copy of the scene to keep it.
  evaluation.device->CreateShaderResourceView(evaluation.color, nullptr, evaluation.frame->Cpu(first + 1u));
  evaluation.device->CreateUnorderedAccessView(evaluation.output, nullptr, nullptr, evaluation.frame->Cpu(first + 2u));
  runtime.encode.Bind(evaluation.commands, *evaluation.frame, first);
  evaluation.commands->SetComputeRootConstantBufferView(1u, evaluation.constants);
  const auto target = evaluation.output->GetDesc();
  evaluation.commands->Dispatch((static_cast<UINT>(target.Width) + 7u) / 8u, (target.Height + 7u) / 8u, 1u);
}

// Prepares the scene's inputs on D3D11, then records `record` (the method's
// evaluation) on D3D12 and submits it. `reset` starts fresh history for this
// scene. Returns false when the bridge rejects the submission.
template <typename Record>
[[nodiscard]] bool Execute(const ValidatedFrameInputs& inputs, bool reset, Record record) {
  auto* device = reinterpret_cast<ID3D11Device*>(inputs.device->get_native());            // NOLINT(performance-no-int-to-ptr)
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(inputs.cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  const bool analytical = state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA;
  const UINT width = inputs.render_width;
  const UINT height = inputs.render_height;
  auto& bridge = GetBridge(device);
  if (runtime.prepare == nullptr) {
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> prepare;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constants;
    compute::Pipeline encode;
    const std::span<const uint8_t> code = (probe_state.active_code.empty() ? std::span<const uint8_t>(__prepare_reconstruction) : std::span<const uint8_t>(probe_state.active_code));
    Check(device->CreateComputeShader(code.data(), code.size(), nullptr, &prepare), "Create reconstruction preparation shader");
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
  // The shared inputs are reused until the render size changes.
  if (runtime.width != width || runtime.height != height) {
    Drain();
    // Create all three before replacing the old set, so a failure keeps it.
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
      .camera_reprojection_valid = !reset && inputs.camera.camera_reprojection_valid ? 1.f : 0.f,
      .analytical_taa = (analytical ? 1.f : 0.f),
      .render_size = {inputs.render_width, inputs.render_height},
      .reciprocal_render_size = {1.f / static_cast<float>(inputs.render_width), 1.f / static_cast<float>(inputs.render_height)},
      .current_to_previous_clip = inputs.camera.current_to_previous_clip,
  };
  // The preparation shader writes straight into the shared textures; there is
  // no extra copy.
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
  rendering::d3d12_timing::CpuSubmission::EndPart("input preparation");
  rendering::d3d12_bridge::SubmissionTiming submission_timing;
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
            .timing = dispatch.timing,
        };
        for (auto* input : {evaluation.color, evaluation.motion, evaluation.depth, evaluation.motion_debug}) {
          if (input != nullptr) {
            compute::Transition(dispatch.commands, input, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
          }
        }
        compute::Transition(dispatch.commands, dispatch.output, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        record(evaluation);
        // The Analytical resolve writes the encoded scene itself.
        dispatch.Mark(analytical ? "resolve and encode" : "encode");
        // Shared resources always return to COMMON before D3D11 resumes.
        compute::Transition(dispatch.commands, dispatch.output, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
        for (auto* input : {evaluation.color, evaluation.motion, evaluation.depth, evaluation.motion_debug}) {
          if (input != nullptr) {
            compute::Transition(dispatch.commands, input, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
          }
        }
      },
      runtime.color.allocation.get(), inputs.output.shared_texture.get(),
      rendering::d3d12_timing::CpuSubmission::IsTiming() ? &submission_timing : nullptr);
  rendering::d3d12_timing::CpuSubmission::AddPart("bridge slot", submission_timing.preparation_us / 1000.);
  rendering::d3d12_timing::CpuSubmission::AddPart("method recording", submission_timing.recording_us / 1000.);
  // Mostly the D3D11 signal, which makes the driver submit what the frame has queued so far.
  rendering::d3d12_timing::CpuSubmission::AddPart("queue calls", submission_timing.queue_us / 1000.);
  if (!succeeded && runtime.bridge->rejection != nullptr) {
    // A rejected submission never reached the method's callback, so only this
    // scene fails and its history restarts. If the bridge is marked failed
    // because a submission stopped partway, throw: the caller treats that as
    // a method failure.
    if (runtime.bridge->failed) throw std::runtime_error(runtime.bridge->rejection);
    static uint64_t last_rejection_log = std::numeric_limits<uint64_t>::max();
    if (logging::ShouldLogFrame(state::CurrentFrameToken(), last_rejection_log)) {
      logging::Warn("reconstruction submission rejected scene=", inputs.camera.scene, " reason=", runtime.bridge->rejection);
    }
  }
  return succeeded;
}

}  // namespace mgsv::reconstruction::dx12

#endif  // RENODX_GAMES_MGSV_RECONSTRUCTION_D3D12_HPP_
