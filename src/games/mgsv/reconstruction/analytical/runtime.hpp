#ifndef RENODX_GAMES_MGSV_RECONSTRUCTION_ANALYTICAL_RUNTIME_HPP_
#define RENODX_GAMES_MGSV_RECONSTRUCTION_ANALYTICAL_RUNTIME_HPP_

// Analytical TAA owns only its resolve pipeline and two linear history images.
// Input preparation, shared textures, command storage, and fences are common to
// all reconstruction methods.

#include <array>
#include <cstdint>

#include <d3d12.h>
#include <wrl/client.h>

#include <embed/mgsv_taa.h>

#include "../d3d12.hpp"

namespace mgsv::reconstruction::analytical {

struct alignas(16) ResolveConstants {
  float diagnostic_view = state::DEFAULT_DIAGNOSTIC_VIEW;
  float velocity_visualization_range = state::DEFAULT_VELOCITY_VISUALIZATION_RANGE;
  float camera_reprojection_valid = 0.f;
  float object_motion_mode = 0.f;
  std::array<float, 2> current_jitter_uv = {0.f, 0.f};
  std::array<float, 2> previous_jitter_uv = {0.f, 0.f};
  float velocity_projection_jitter_scale = 0.f;
  float clip_tightness = state::DEFAULT_CLIP_TIGHTNESS;
  float history_clip_strength = state::DEFAULT_HISTORY_CLIP_STRENGTH;
  float current_frame_blend = state::DEFAULT_CURRENT_FRAME_BLEND;
  std::array<float, 16> current_to_previous_clip = {};
};

static_assert(sizeof(ResolveConstants) == 112u, "TAA resolve constants must occupy seven 16-byte registers");

struct Resources {
  dx12::compute::Pipeline pipeline;
  std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> history;
  uint32_t width = 0u;
  uint32_t height = 0u;
  uint32_t index = 0u;
  bool initialized = false;
};

static Resources resources;

inline void InvalidateHistory(const char* /*reason*/) {
  resources.initialized = false;
}

inline void Destroy(reshade::api::device* /*device*/) {
  if (resources.pipeline.state == nullptr && resources.history[0] == nullptr && resources.history[1] == nullptr) return;
  dx12::Drain();
  resources = {};
}

inline bool Dispatch(const ValidatedFrameInputs& inputs) {
  if (!inputs.HasNativeOutputSize()) return false;
  if (resources.width != inputs.render_width || resources.height != inputs.render_height) {
    resources.initialized = false;
  }
  const ResolveConstants constants = {
      .diagnostic_view = state::GetDiagnosticView(),
      .velocity_visualization_range = state::GetVelocityVisualizationRange(),
      .camera_reprojection_valid = resources.initialized && inputs.camera.camera_reprojection_valid ? 1.f : 0.f,
      .object_motion_mode = static_cast<float>(state::GetObjectMotionMode()),
      .current_jitter_uv = {inputs.camera.jitter_uv_x, inputs.camera.jitter_uv_y},
      .previous_jitter_uv = {
          inputs.camera.previous_jitter_uv_x,
          inputs.camera.previous_jitter_uv_y,
      },
      .velocity_projection_jitter_scale = state::GetProjectionJitterScale(state::ProjectionJitterPath::VELOCITY),
      .clip_tightness = state::GetClipTightness(),
      .history_clip_strength = state::GetHistoryClipStrength(),
      .current_frame_blend = state::GetCurrentFrameBlend(),
      .current_to_previous_clip = inputs.camera.current_to_previous_clip,
  };

  const bool succeeded = dx12::Execute(inputs, !resources.initialized, [&](const dx12::Evaluation& evaluation) {
    if (resources.pipeline.state == nullptr) {
      resources.pipeline.Create(evaluation.device, __mgsv_taa, 5u, 2u, 2u);
      logging::Info("created D3D12 TAA pipeline");
    }
    if (resources.width != inputs.render_width || resources.height != inputs.render_height) {
      dx12::Drain();
      for (auto& history : resources.history) {
        history = dx12::compute::CreateTexture(evaluation.device, inputs.render_width, inputs.render_height, DXGI_FORMAT_R16G16B16A16_FLOAT);
      }
      resources.width = inputs.render_width;
      resources.height = inputs.render_height;
      resources.index = 0u;
      resources.initialized = false;
    }
    const UINT first = evaluation.frame->Allocate(7u);
    // A cold resolve reads the current linear input as history, matching the
    // old seed operation without two full-image copies.
    auto* previous = resources.initialized ? resources.history[1u - resources.index].Get() : evaluation.color;
    auto* current = resources.history[resources.index].Get();
    const std::array<ID3D12Resource*, 5> srvs = {
        evaluation.color,
        previous,
        evaluation.motion,
        evaluation.depth,
        evaluation.motion_debug,
    };
    for (UINT index = 0u; index < srvs.size(); ++index) {
      evaluation.device->CreateShaderResourceView(srvs[index], nullptr, evaluation.frame->Cpu(first + index));
    }
    evaluation.device->CreateUnorderedAccessView(current, nullptr, nullptr, evaluation.frame->Cpu(first + 5u));
    evaluation.device->CreateUnorderedAccessView(evaluation.output, nullptr, nullptr, evaluation.frame->Cpu(first + 6u));
    dx12::compute::Transition(evaluation.commands, current, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    if (resources.initialized) {
      dx12::compute::Transition(evaluation.commands, previous, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }
    resources.pipeline.Bind(evaluation.commands, *evaluation.frame, first);
    evaluation.commands->SetComputeRootConstantBufferView(1u, evaluation.constants);
    evaluation.commands->SetComputeRootConstantBufferView(2u, evaluation.frame->Upload(&constants, sizeof(constants)));
    evaluation.commands->Dispatch((inputs.render_width + 7u) / 8u, (inputs.render_height + 7u) / 8u, 1u);
    dx12::compute::Transition(evaluation.commands, current, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
    if (resources.initialized) {
      dx12::compute::Transition(evaluation.commands, previous, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
    }
  });
  if (succeeded) {
    resources.index = 1u - resources.index;
    resources.initialized = true;
  }
  return succeeded;
}

}  // namespace mgsv::reconstruction::analytical

#endif  // RENODX_GAMES_MGSV_RECONSTRUCTION_ANALYTICAL_RUNTIME_HPP_
