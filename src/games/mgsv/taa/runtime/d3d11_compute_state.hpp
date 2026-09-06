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
  std::array<Microsoft::WRL::ComPtr<ID3D11SamplerState>, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT> samplers;
  std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>
      shader_resources;
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>
      vertex_shader_resources;
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>
      hull_shader_resources;
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>
      domain_shader_resources;
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>
      geometry_shader_resources;
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>
      pixel_shader_resources;
  std::array<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>, 8> unordered_access_views;
  std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT> constant_buffers;
  std::array<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> render_targets;
  UINT render_target_count = 0u;
  std::array<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>, D3D11_PS_CS_UAV_REGISTER_COUNT>
      output_merger_unordered_access_views;
  Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth_stencil;
  Microsoft::WRL::ComPtr<ID3D11BlendState> blend_state;
  std::array<float, 4> blend_factor = {};
  UINT sample_mask = 0u;
  Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_stencil_state;
  UINT stencil_reference = 0u;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> compute_shader;
  std::vector<Microsoft::WRL::ComPtr<ID3D11ClassInstance>> class_instances;
  bool native_descriptors_captured = false;
  bool legacy_compute_only = false;
};

inline State Capture(reshade::api::command_list* cmd_list, bool legacy_compute_only = false) {
  State result = {};
  result.legacy_compute_only = legacy_compute_only;
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

  std::array<ID3D11SamplerState*, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT> samplers = {};
  std::array<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> shader_resources = {};
  std::array<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> vertex_shader_resources = {};
  std::array<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> hull_shader_resources = {};
  std::array<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> domain_shader_resources = {};
  std::array<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> geometry_shader_resources = {};
  std::array<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> pixel_shader_resources = {};
  std::array<ID3D11UnorderedAccessView*, 8> unordered_access_views = {};
  std::array<ID3D11Buffer*, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT> constant_buffers = {};
  std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> render_targets = {};
  std::array<ID3D11UnorderedAccessView*, D3D11_PS_CS_UAV_REGISTER_COUNT> output_merger_unordered_access_views = {};
  ID3D11DepthStencilView* depth_stencil = nullptr;
  ID3D11BlendState* blend_state = nullptr;
  ID3D11DepthStencilState* depth_stencil_state = nullptr;
  std::array<ID3D11ClassInstance*, 256> class_instances = {};
  ID3D11ComputeShader* compute_shader = nullptr;
  UINT class_instance_count = static_cast<UINT>(class_instances.size());
  // These are the exact native call ranges used by mgsv-old. Only the FSR
  // diagnostic selects them; the extended path and NGX remain unchanged.
  const UINT sampler_count = legacy_compute_only ? 2u : static_cast<UINT>(samplers.size());
  const UINT shader_resource_count = legacy_compute_only ? 16u : static_cast<UINT>(shader_resources.size());
  const UINT constant_buffer_count = legacy_compute_only ? 3u : static_cast<UINT>(constant_buffers.size());
  context->CSGetShader(&compute_shader, class_instances.data(), &class_instance_count);
  context->CSGetSamplers(0u, sampler_count, samplers.data());
  context->CSGetShaderResources(0u, shader_resource_count, shader_resources.data());
  if (!legacy_compute_only) {
    context->VSGetShaderResources(0u, static_cast<UINT>(vertex_shader_resources.size()), vertex_shader_resources.data());
    context->HSGetShaderResources(0u, static_cast<UINT>(hull_shader_resources.size()), hull_shader_resources.data());
    context->DSGetShaderResources(0u, static_cast<UINT>(domain_shader_resources.size()), domain_shader_resources.data());
    context->GSGetShaderResources(0u, static_cast<UINT>(geometry_shader_resources.size()), geometry_shader_resources.data());
    context->PSGetShaderResources(0u, static_cast<UINT>(pixel_shader_resources.size()), pixel_shader_resources.data());
  }
  context->CSGetUnorderedAccessViews(
      0u,
      static_cast<UINT>(unordered_access_views.size()),
      unordered_access_views.data());
  context->CSGetConstantBuffers(0u, constant_buffer_count, constant_buffers.data());
  if (!legacy_compute_only) {
    context->OMGetRenderTargets(
        static_cast<UINT>(render_targets.size()),
        render_targets.data(),
        &depth_stencil);
    context->OMGetRenderTargetsAndUnorderedAccessViews(
        0u,
        nullptr,
        nullptr,
        0u,
        static_cast<UINT>(output_merger_unordered_access_views.size()),
        output_merger_unordered_access_views.data());
    context->OMGetBlendState(&blend_state, result.blend_factor.data(), &result.sample_mask);
    context->OMGetDepthStencilState(&depth_stencil_state, &result.stencil_reference);
  }
  for (size_t index = 0u; index < sampler_count; ++index) {
    result.samplers[index].Attach(samplers[index]);
  }
  for (size_t index = 0u; index < shader_resource_count; ++index) {
    result.shader_resources[index].Attach(shader_resources[index]);
    if (!legacy_compute_only) {
      result.vertex_shader_resources[index].Attach(vertex_shader_resources[index]);
      result.hull_shader_resources[index].Attach(hull_shader_resources[index]);
      result.domain_shader_resources[index].Attach(domain_shader_resources[index]);
      result.geometry_shader_resources[index].Attach(geometry_shader_resources[index]);
      result.pixel_shader_resources[index].Attach(pixel_shader_resources[index]);
    }
  }
  for (size_t index = 0u; index < unordered_access_views.size(); ++index) {
    result.unordered_access_views[index].Attach(unordered_access_views[index]);
  }
  for (size_t index = 0u; index < constant_buffer_count; ++index) {
    result.constant_buffers[index].Attach(constant_buffers[index]);
  }
  for (size_t index = 0u; index < render_targets.size(); ++index) {
    result.render_targets[index].Attach(render_targets[index]);
    if (render_targets[index] != nullptr) result.render_target_count = static_cast<UINT>(index + 1u);
  }
  for (size_t index = 0u; index < output_merger_unordered_access_views.size(); ++index) {
    result.output_merger_unordered_access_views[index].Attach(output_merger_unordered_access_views[index]);
  }
  result.depth_stencil.Attach(depth_stencil);
  result.blend_state.Attach(blend_state);
  result.depth_stencil_state.Attach(depth_stencil_state);
  result.compute_shader.Attach(compute_shader);
  result.class_instances.resize(class_instance_count);
  for (size_t index = 0u; index < result.class_instances.size(); ++index) {
    result.class_instances[index].Attach(class_instances[index]);
  }
  result.native_descriptors_captured = true;
  if (legacy_compute_only) return result;

  constexpr std::array<ID3D11UnorderedAccessView*, D3D11_PS_CS_UAV_REGISTER_COUNT>
      null_output_merger_unordered_access_views = {};
  context->OMSetRenderTargetsAndUnorderedAccessViews(
      0u,
      nullptr,
      nullptr,
      0u,
      static_cast<UINT>(null_output_merger_unordered_access_views.size()),
      null_output_merger_unordered_access_views.data(),
      nullptr);
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
  std::array<ID3D11SamplerState*, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT> samplers = {};
  std::array<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> shader_resources = {};
  std::array<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> vertex_shader_resources = {};
  std::array<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> hull_shader_resources = {};
  std::array<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> domain_shader_resources = {};
  std::array<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> geometry_shader_resources = {};
  std::array<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> pixel_shader_resources = {};
  std::array<ID3D11UnorderedAccessView*, 8> unordered_access_views = {};
  std::array<ID3D11Buffer*, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT> constant_buffers = {};
  std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> render_targets = {};
  std::array<ID3D11UnorderedAccessView*, D3D11_PS_CS_UAV_REGISTER_COUNT> output_merger_unordered_access_views = {};
  const UINT sampler_count = state.legacy_compute_only ? 2u : static_cast<UINT>(samplers.size());
  const UINT shader_resource_count = state.legacy_compute_only ? 16u : static_cast<UINT>(shader_resources.size());
  const UINT constant_buffer_count = state.legacy_compute_only ? 3u : static_cast<UINT>(constant_buffers.size());
  for (size_t index = 0u; index < sampler_count; ++index) {
    samplers[index] = state.samplers[index].Get();
  }
  for (size_t index = 0u; index < shader_resource_count; ++index) {
    shader_resources[index] = state.shader_resources[index].Get();
    if (!state.legacy_compute_only) {
      vertex_shader_resources[index] = state.vertex_shader_resources[index].Get();
      hull_shader_resources[index] = state.hull_shader_resources[index].Get();
      domain_shader_resources[index] = state.domain_shader_resources[index].Get();
      geometry_shader_resources[index] = state.geometry_shader_resources[index].Get();
      pixel_shader_resources[index] = state.pixel_shader_resources[index].Get();
    }
  }
  for (size_t index = 0u; index < unordered_access_views.size(); ++index) {
    unordered_access_views[index] = state.unordered_access_views[index].Get();
  }
  for (size_t index = 0u; index < constant_buffer_count; ++index) {
    constant_buffers[index] = state.constant_buffers[index].Get();
  }
  for (size_t index = 0u; index < render_targets.size(); ++index) {
    render_targets[index] = state.render_targets[index].Get();
  }
  for (size_t index = 0u; index < output_merger_unordered_access_views.size(); ++index) {
    output_merger_unordered_access_views[index] = state.output_merger_unordered_access_views[index].Get();
  }
  std::vector<ID3D11ClassInstance*> class_instances(state.class_instances.size());
  for (size_t index = 0u; index < class_instances.size(); ++index) {
    class_instances[index] = state.class_instances[index].Get();
  }

  constexpr std::array<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>
      null_shader_resources = {};
  constexpr std::array<ID3D11UnorderedAccessView*, 8> null_unordered_access_views = {};
  context->CSSetShaderResources(
      0u,
      shader_resource_count,
      null_shader_resources.data());
  context->CSSetUnorderedAccessViews(
      0u,
      static_cast<UINT>(null_unordered_access_views.size()),
      null_unordered_access_views.data(),
      nullptr);
  context->CSSetSamplers(0u, sampler_count, samplers.data());
  context->CSSetShaderResources(
      0u,
      shader_resource_count,
      shader_resources.data());
  if (!state.legacy_compute_only) {
    context->VSSetShaderResources(
      0u,
      static_cast<UINT>(vertex_shader_resources.size()),
      vertex_shader_resources.data());
    context->HSSetShaderResources(
      0u,
      static_cast<UINT>(hull_shader_resources.size()),
      hull_shader_resources.data());
    context->DSSetShaderResources(
      0u,
      static_cast<UINT>(domain_shader_resources.size()),
      domain_shader_resources.data());
    context->GSSetShaderResources(
      0u,
      static_cast<UINT>(geometry_shader_resources.size()),
      geometry_shader_resources.data());
    context->PSSetShaderResources(
      0u,
      static_cast<UINT>(pixel_shader_resources.size()),
      pixel_shader_resources.data());
  }
  context->CSSetUnorderedAccessViews(
      0u,
      static_cast<UINT>(unordered_access_views.size()),
      unordered_access_views.data(),
      nullptr);
  context->CSSetConstantBuffers(
      0u,
      constant_buffer_count,
      constant_buffers.data());
  context->CSSetShader(
      state.compute_shader.Get(),
      class_instances.empty() ? nullptr : class_instances.data(),
      static_cast<UINT>(class_instances.size()));
  if (state.legacy_compute_only) return;

  context->OMSetRenderTargetsAndUnorderedAccessViews(
      state.render_target_count,
      render_targets.data(),
      state.depth_stencil.Get(),
      state.render_target_count,
      static_cast<UINT>(output_merger_unordered_access_views.size()) - state.render_target_count,
      output_merger_unordered_access_views.data() + state.render_target_count,
      nullptr);
  context->OMSetBlendState(state.blend_state.Get(), state.blend_factor.data(), state.sample_mask);
  context->OMSetDepthStencilState(state.depth_stencil_state.Get(), state.stencil_reference);
}

}  // namespace taa::d3d11_compute_state
