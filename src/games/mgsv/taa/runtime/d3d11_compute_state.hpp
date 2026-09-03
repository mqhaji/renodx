#pragma once

/* D3D11 compute-state preservation shared by temporal reconstruction methods. */

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include <include/reshade.hpp>

#include "../../../../utils/state.hpp"

namespace taa::d3d11_compute_state {

struct State {
  std::array<std::pair<reshade::api::pipeline_stage, reshade::api::pipeline>, 4> pipelines = {};
  uint32_t pipeline_count = 0u;
  reshade::api::pipeline_layout layout = {0};
  std::vector<reshade::api::descriptor_table> descriptor_tables;
  std::array<Microsoft::WRL::ComPtr<ID3D11SamplerState>, 2> samplers;
  std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 16> shader_resources;
  std::array<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>, 8> unordered_access_views;
  std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, 3> constant_buffers;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> compute_shader;
  std::vector<Microsoft::WRL::ComPtr<ID3D11ClassInstance>> class_instances;
  bool native_descriptors_captured = false;
};

inline State Capture(reshade::api::command_list* cmd_list) {
  State result = {};
  const auto* tracked_state = renodx::utils::state::GetCurrentState(cmd_list);
  if (tracked_state != nullptr) {
    for (const auto& [stage, pipeline] : tracked_state->pipelines) {
      const bool is_compute = (static_cast<uint32_t>(stage)
                               & static_cast<uint32_t>(reshade::api::pipeline_stage::all_compute))
                              != 0u;
      if (!is_compute || result.pipeline_count >= result.pipelines.size()) continue;
      result.pipelines[result.pipeline_count++] = {stage, pipeline};
    }
    result.layout = tracked_state->compute_pipeline_layout;
    result.descriptor_tables = tracked_state->compute_descriptor_tables;
  }

  if (cmd_list == nullptr || cmd_list->get_device()->get_api() != reshade::api::device_api::d3d11) return result;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (context == nullptr) return result;

  std::array<ID3D11SamplerState*, 2> samplers = {};
  std::array<ID3D11ShaderResourceView*, 16> shader_resources = {};
  std::array<ID3D11UnorderedAccessView*, 8> unordered_access_views = {};
  std::array<ID3D11Buffer*, 3> constant_buffers = {};
  std::array<ID3D11ClassInstance*, 256> class_instances = {};
  ID3D11ComputeShader* compute_shader = nullptr;
  UINT class_instance_count = static_cast<UINT>(class_instances.size());
  context->CSGetShader(&compute_shader, class_instances.data(), &class_instance_count);
  context->CSGetSamplers(0u, static_cast<UINT>(samplers.size()), samplers.data());
  context->CSGetShaderResources(0u, static_cast<UINT>(shader_resources.size()), shader_resources.data());
  context->CSGetUnorderedAccessViews(
      0u,
      static_cast<UINT>(unordered_access_views.size()),
      unordered_access_views.data());
  context->CSGetConstantBuffers(0u, static_cast<UINT>(constant_buffers.size()), constant_buffers.data());
  for (size_t index = 0u; index < samplers.size(); ++index) {
    result.samplers[index].Attach(samplers[index]);
  }
  for (size_t index = 0u; index < shader_resources.size(); ++index) {
    result.shader_resources[index].Attach(shader_resources[index]);
  }
  for (size_t index = 0u; index < unordered_access_views.size(); ++index) {
    result.unordered_access_views[index].Attach(unordered_access_views[index]);
  }
  for (size_t index = 0u; index < constant_buffers.size(); ++index) {
    result.constant_buffers[index].Attach(constant_buffers[index]);
  }
  result.compute_shader.Attach(compute_shader);
  result.class_instances.resize(class_instance_count);
  for (size_t index = 0u; index < result.class_instances.size(); ++index) {
    result.class_instances[index].Attach(class_instances[index]);
  }
  result.native_descriptors_captured = true;
  return result;
}

inline void Restore(reshade::api::command_list* cmd_list, const State& state) {
  for (uint32_t index = 0u; index < state.pipeline_count; ++index) {
    cmd_list->bind_pipeline(state.pipelines[index].first, state.pipelines[index].second);
  }
  if (state.layout.handle != 0u) {
    cmd_list->bind_descriptor_tables(
        reshade::api::shader_stage::all_compute,
        state.layout,
        0,
        static_cast<uint32_t>(state.descriptor_tables.size()),
        state.descriptor_tables.data());
  }
  if (!state.native_descriptors_captured
      || cmd_list == nullptr
      || cmd_list->get_device()->get_api() != reshade::api::device_api::d3d11) {
    return;
  }

  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (context == nullptr) return;
  std::array<ID3D11SamplerState*, 2> samplers = {};
  std::array<ID3D11ShaderResourceView*, 16> shader_resources = {};
  std::array<ID3D11UnorderedAccessView*, 8> unordered_access_views = {};
  std::array<ID3D11Buffer*, 3> constant_buffers = {};
  for (size_t index = 0u; index < samplers.size(); ++index) {
    samplers[index] = state.samplers[index].Get();
  }
  for (size_t index = 0u; index < shader_resources.size(); ++index) {
    shader_resources[index] = state.shader_resources[index].Get();
  }
  for (size_t index = 0u; index < unordered_access_views.size(); ++index) {
    unordered_access_views[index] = state.unordered_access_views[index].Get();
  }
  for (size_t index = 0u; index < constant_buffers.size(); ++index) {
    constant_buffers[index] = state.constant_buffers[index].Get();
  }
  std::vector<ID3D11ClassInstance*> class_instances(state.class_instances.size());
  for (size_t index = 0u; index < class_instances.size(); ++index) {
    class_instances[index] = state.class_instances[index].Get();
  }

    constexpr std::array<ID3D11ShaderResourceView*, 16> null_shader_resources = {};
    constexpr std::array<ID3D11UnorderedAccessView*, 8> null_unordered_access_views = {};
  context->CSSetShaderResources(
      0u,
      static_cast<UINT>(null_shader_resources.size()),
      null_shader_resources.data());
  context->CSSetUnorderedAccessViews(
      0u,
      static_cast<UINT>(null_unordered_access_views.size()),
      null_unordered_access_views.data(),
      nullptr);
  context->CSSetSamplers(0u, static_cast<UINT>(samplers.size()), samplers.data());
  context->CSSetShaderResources(
      0u,
      static_cast<UINT>(shader_resources.size()),
      shader_resources.data());
  context->CSSetUnorderedAccessViews(
      0u,
      static_cast<UINT>(unordered_access_views.size()),
      unordered_access_views.data(),
      nullptr);
  context->CSSetConstantBuffers(
      0u,
      static_cast<UINT>(constant_buffers.size()),
      constant_buffers.data());
  context->CSSetShader(
      state.compute_shader.Get(),
      class_instances.empty() ? nullptr : class_instances.data(),
      static_cast<UINT>(class_instances.size()));
}

}  // namespace taa::d3d11_compute_state
