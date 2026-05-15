#pragma once

/*
 * Per-command-list descriptor tracker for the MGSV TAA runtime.
 *
 * The draw router identifies camera cbuffers from verified shader/register
 * pairs. This module only records the current per-command-list descriptor
 * state so draw-time code can capture color/depth resources safely.
 *
 * Current snapshot facts:
 *   - Insertion draws bind HDR scene color at pixel t0.
 *   - MotionBlurCameraVelocity writes velocity to RTV0 and reads depth at
 *     pixel t2.
 */

#include <cstdint>
#include <utility>

#include <include/reshade.hpp>

#include "../../../../utils/bitwise.hpp"
#include "../../../../utils/descriptor.hpp"
#include "../../../../utils/pipeline_layout.hpp"

namespace taa::descriptor_tracker {

using RegisterSlot = std::pair<uint32_t, uint32_t>;

struct __declspec(uuid("9F1A3A38-9D2A-4E5B-B5A3-3F2D0F38CD33")) CommandListData {
  // Draw-time pixel SRVs used by the TAA router.
  reshade::api::resource_view pixel_srv_t0 = {0};  // HDR color at insertion draws
  reshade::api::resource_view pixel_srv_t1 = {0};  // Depth at DOF_ScatterBakeFirst
  reshade::api::resource_view pixel_srv_t2 = {0};  // Depth at MotionBlurCameraVelocity
  reshade::api::resource_view pixel_srv_t3 = {0};

  // Camera cbuffer at vertex b2 / pixel b2 — jitter patches on unmap.
  reshade::api::buffer_range vertex_cb_b2 = {};
  reshade::api::buffer_range pixel_cb_b2 = {};

  // Deferred same-frame insertion marker. LocalReflectionAddBuffer draw
  // callbacks run before the game draw, so taa.hpp records RTV0 here and
  // resolves it from the next draw callback on the same command list.
  reshade::api::resource_view pending_local_reflection_rtv = {0};
  uint64_t pending_local_reflection_frame = UINT64_MAX;
};

inline CommandListData* Get(reshade::api::command_list* cmd_list) {
  if (cmd_list == nullptr) return nullptr;
  auto* data = cmd_list->get_private_data<CommandListData>();
  return data != nullptr ? data : cmd_list->create_private_data<CommandListData>();
}

inline bool ResolveRegister(
    reshade::api::pipeline_layout layout,
    uint32_t layout_param,
    const reshade::api::descriptor_table_update& update,
    uint32_t descriptor_index,
    RegisterSlot& slot) {
  const auto* layout_data = renodx::utils::pipeline_layout::GetPipelineLayoutData(layout);
  if (layout_data == nullptr || layout_param >= layout_data->params.size()) return false;

  const auto& param = layout_data->params[layout_param];
  const uint32_t binding = update.binding + descriptor_index;

  switch (param.type) {
    case reshade::api::pipeline_layout_param_type::push_descriptors:
      slot = {param.push_descriptors.dx_register_index + binding, param.push_descriptors.dx_register_space};
      return true;
    case reshade::api::pipeline_layout_param_type::descriptor_table:
    case reshade::api::pipeline_layout_param_type::push_descriptors_with_ranges:
    case reshade::api::pipeline_layout_param_type::push_descriptors_with_static_samplers:
    case reshade::api::pipeline_layout_param_type::descriptor_table_with_static_samplers: {
      if (layout_param >= layout_data->ranges.size()) return false;
      for (const auto& range : layout_data->ranges[layout_param]) {
        const bool in_range = binding >= range.binding
                              && (range.count == UINT32_MAX || binding < range.binding + range.count);
        if (!in_range) continue;
        slot = {range.dx_register_index + (binding - range.binding), range.dx_register_space};
        return true;
      }
      return false;
    }
    default:
      return false;
  }
}

inline void StoreViewByStage(
    CommandListData* data,
    reshade::api::shader_stage stages,
    const RegisterSlot& slot,
    reshade::api::resource_view view) {
  if (!renodx::utils::bitwise::HasFlag(stages, reshade::api::shader_stage::pixel)) return;
  if (slot.second != 0u) return;

  switch (slot.first) {
    case 0:
      data->pixel_srv_t0 = view;
      break;
    case 1:
      data->pixel_srv_t1 = view;
      break;
    case 2:
      data->pixel_srv_t2 = view;
      break;
    case 3:
      data->pixel_srv_t3 = view;
      break;
    default:
      break;
  }
}

inline void StoreConstantBufferByStage(
    CommandListData* data,
    reshade::api::shader_stage stages,
    const RegisterSlot& slot,
    reshade::api::buffer_range range) {
  if (slot.second != 0u || slot.first != 2u) return;

  const bool tracks_vertex = renodx::utils::bitwise::HasFlag(stages, reshade::api::shader_stage::vertex);
  const bool tracks_pixel = renodx::utils::bitwise::HasFlag(stages, reshade::api::shader_stage::pixel);

  if (tracks_vertex) data->vertex_cb_b2 = range;
  if (tracks_pixel) data->pixel_cb_b2 = range;
}

inline void OnInitCommandList(reshade::api::command_list* cmd_list) {
  cmd_list->create_private_data<CommandListData>();
}

inline void OnDestroyCommandList(reshade::api::command_list* cmd_list) {
  cmd_list->destroy_private_data<CommandListData>();
}

inline void OnResetCommandList(reshade::api::command_list* cmd_list) {
  auto* data = Get(cmd_list);
  if (data != nullptr) *data = {};
}

inline void OnPushDescriptors(
    reshade::api::command_list* cmd_list,
    reshade::api::shader_stage stages,
    reshade::api::pipeline_layout layout,
    uint32_t layout_param,
    const reshade::api::descriptor_table_update& update) {
  auto* data = Get(cmd_list);
  if (data == nullptr) return;

  const bool tracks_pixel = renodx::utils::bitwise::HasFlag(stages, reshade::api::shader_stage::pixel);
  const bool tracks_vertex = renodx::utils::bitwise::HasFlag(stages, reshade::api::shader_stage::vertex);

  switch (update.type) {
    case reshade::api::descriptor_type::sampler_with_resource_view:
    case reshade::api::descriptor_type::shader_resource_view:
    case reshade::api::descriptor_type::buffer_shader_resource_view:
      if (!tracks_pixel) return;
      break;
    case reshade::api::descriptor_type::constant_buffer:
      if (!tracks_vertex && !tracks_pixel) return;
      break;
    default:
      return;
  }

  for (uint32_t i = 0; i < update.count; ++i) {
    RegisterSlot slot = {};
    if (!ResolveRegister(layout, layout_param, update, i, slot)) continue;

    switch (update.type) {
      case reshade::api::descriptor_type::sampler_with_resource_view:
      case reshade::api::descriptor_type::shader_resource_view:
      case reshade::api::descriptor_type::buffer_shader_resource_view: {
        const auto view = renodx::utils::descriptor::GetResourceViewFromDescriptorUpdate(update, i);
        StoreViewByStage(data, stages, slot, view);
        break;
      }
      case reshade::api::descriptor_type::constant_buffer: {
        const auto range = static_cast<const reshade::api::buffer_range*>(update.descriptors)[i];
        StoreConstantBufferByStage(data, stages, slot, range);
        break;
      }
      default:
        break;
    }
  }
}

}  // namespace taa::descriptor_tracker
