#pragma once

/*
 * MGSV temporal resolve implementation.
 *
 * Reads HDR color, camera velocity, and depth straight off the insertion
 * draw's bound state (DOF_ScatterBakeFirst or a CRB fallback), dispatches
 * the embedded compute shader, ping-pongs history textures, and copies the
 * resolved result back into the HDR color resource before the insertion
 * draw runs.
 *
 *   - EnsureComputePipeline: layout / pipeline / samplers, one-time setup.
 *   - EnsureHistory: ping-pong RGBA16F textures sized to current color.
 *   - DispatchCompute: barriers, bind, dispatch, copy-back, restore state.
 *   - MaybeRun: enable + inputs gate, then run.
 *
 * History format follows the HDR scene clone (RGBA16F). addon.cpp already
 * clone-upgrades the typeless BGRA8 underlying resource, so the clone SRV
 * is what we sample and the clone resource is what we copy back into.
 *
 * MGSV differs from the Alien Isolation port in two ways:
 *   1. Velocity / depth SRVs are bound at the same insertion draw (pixel t2 /
 *      t3), so there is no separate capture pass. We borrow the SRVs and
 *      do not barrier velocity ourselves — it is already shader_resource at
 *      the host draw.
 *   2. The compute shader reads the camera-velocity buffer's encoded form
 *      directly (0.5 + 0.5 * scaled in .ba). No CPU-side decode or
 *      reinterpretation.
 */

#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <embed/shaders.h>
#include <include/reshade.hpp>

#include "../../../../utils/resource.hpp"
#include "../../../../utils/state.hpp"
#include "../../shared.h"
#include "./constant_buffers.hpp"
#include "./descriptor_tracker.hpp"
#include "./logging.hpp"

namespace taa::resolve {

struct HistoryTexture {
  reshade::api::resource resource = {0};
  reshade::api::resource_view srv = {0};
  reshade::api::resource_view uav = {0};
};

struct Resources {
  std::array<HistoryTexture, 2> history = {};
  uint32_t width = 0u;
  uint32_t height = 0u;
  reshade::api::format resource_format = reshade::api::format::unknown;
  reshade::api::format view_format = reshade::api::format::unknown;
  uint32_t accum_index = 0u;
  bool initialized = false;

  // Stable cache from the last successful insertion so steady state does
  // not have to re-query resource descs every frame.
  reshade::api::resource_view color_srv = {0};
  reshade::api::resource color_resource = {0};

  // Velocity / depth SRVs borrowed from the active insertion draw. We do
  // not own these — they are bound by the game on the host draw.
  reshade::api::resource_view velocity_srv = {0};
  reshade::api::resource_view depth_srv = {0};

  reshade::api::pipeline_layout compute_layout = {0};
  reshade::api::pipeline compute_pipeline = {0};
  std::array<reshade::api::sampler, 2> samplers = {};
};

inline Resources resources;
inline ShaderInjectData* shader_injection = nullptr;
inline uint64_t last_dispatch_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_missing_inputs_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_compute_fail_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_history_format_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_history_create_fail_log = std::numeric_limits<uint64_t>::max();

inline bool LogEvery(uint64_t& last_frame, uint64_t interval = 120u) {
  return logging::ShouldLogFrame(constant_buffers::frame_state.frame_index, last_frame, interval);
}

struct PreviousComputeState {
  std::array<std::pair<reshade::api::pipeline_stage, reshade::api::pipeline>, 4> pipelines = {};
  uint32_t pipeline_count = 0u;
  reshade::api::pipeline_layout layout = {0};
  std::vector<reshade::api::descriptor_table> descriptor_tables;
};

inline bool IsComputePipelineStage(reshade::api::pipeline_stage stage) {
  return (static_cast<uint32_t>(stage) & static_cast<uint32_t>(reshade::api::pipeline_stage::all_compute)) != 0u;
}

inline PreviousComputeState CaptureComputeState(reshade::api::command_list* cmd_list) {
  PreviousComputeState result = {};
  const auto* state = renodx::utils::state::GetCurrentState(cmd_list);
  if (state == nullptr) return result;

  for (const auto& [stage, pipeline] : state->pipelines) {
    if (!IsComputePipelineStage(stage) || result.pipeline_count >= result.pipelines.size()) continue;
    result.pipelines[result.pipeline_count++] = {stage, pipeline};
  }
  result.layout = state->compute_pipeline_layout;
  result.descriptor_tables = state->compute_descriptor_tables;
  return result;
}

inline void RestoreComputeState(reshade::api::command_list* cmd_list, const PreviousComputeState& state) {
  for (uint32_t i = 0u; i < state.pipeline_count; ++i) {
    cmd_list->bind_pipeline(state.pipelines[i].first, state.pipelines[i].second);
  }
  if (state.layout.handle != 0u) {
    cmd_list->bind_descriptor_tables(
        reshade::api::shader_stage::all_compute,
        state.layout,
        0,
        static_cast<uint32_t>(state.descriptor_tables.size()),
        state.descriptor_tables.data());
  }
}

inline void DestroyCompute(reshade::api::device* device) {
  if (device == nullptr) return;

  if (resources.compute_pipeline.handle != 0u) {
    device->destroy_pipeline(resources.compute_pipeline);
    resources.compute_pipeline = {0};
  }
  if (resources.compute_layout.handle != 0u) {
    device->destroy_pipeline_layout(resources.compute_layout);
    resources.compute_layout = {0};
  }
  for (auto& sampler : resources.samplers) {
    if (sampler.handle != 0u) device->destroy_sampler(sampler);
    sampler = {0};
  }
}

inline void DestroyHistory(reshade::api::device* device) {
  if (device == nullptr) return;
  for (auto& item : resources.history) {
    if (item.srv.handle != 0u) device->destroy_resource_view(item.srv);
    if (item.uav.handle != 0u) device->destroy_resource_view(item.uav);
    if (item.resource.handle != 0u) device->destroy_resource(item.resource);
    item = {};
  }
  resources.width = 0u;
  resources.height = 0u;
  resources.resource_format = reshade::api::format::unknown;
  resources.view_format = reshade::api::format::unknown;
  resources.color_srv = {0};
  resources.color_resource = {0};
  resources.accum_index = 0u;
  resources.initialized = false;
}

inline void Destroy(reshade::api::device* device) {
  DestroyCompute(device);
  DestroyHistory(device);
  resources = {};
}

// Layout mirrors the compute shader registers:
//   s0-s1 samplers, t0-t3 SRVs, u0 UAV, b13 push constants (ShaderInjectData).
// b13 matches the cbuffer register the rest of MGSV's pixel shaders use for
// shader_injection (see shared.h cb13).
inline bool EnsureComputePipeline(reshade::api::command_list* cmd_list) {
  auto* device = cmd_list != nullptr ? cmd_list->get_device() : nullptr;
  if (device == nullptr) return false;

  if (resources.compute_layout.handle != 0u && resources.compute_pipeline.handle != 0u
      && resources.samplers[0].handle != 0u && resources.samplers[1].handle != 0u) {
    return true;
  }

  DestroyCompute(device);

  std::array<reshade::api::pipeline_layout_param, 4> params = {};
  params[0].type = reshade::api::pipeline_layout_param_type::push_descriptors;
  params[0].push_descriptors.count = 2;
  params[0].push_descriptors.type = reshade::api::descriptor_type::sampler;
  params[0].push_descriptors.dx_register_index = 0;
  params[0].push_descriptors.dx_register_space = 0;

  params[1].type = reshade::api::pipeline_layout_param_type::push_descriptors;
  params[1].push_descriptors.count = 4;
  params[1].push_descriptors.type = reshade::api::descriptor_type::texture_shader_resource_view;
  params[1].push_descriptors.dx_register_index = 0;
  params[1].push_descriptors.dx_register_space = 0;

  params[2].type = reshade::api::pipeline_layout_param_type::push_descriptors;
  params[2].push_descriptors.count = 1;
  params[2].push_descriptors.type = reshade::api::descriptor_type::texture_unordered_access_view;
  params[2].push_descriptors.dx_register_index = 0;
  params[2].push_descriptors.dx_register_space = 0;

  params[3].type = reshade::api::pipeline_layout_param_type::push_constants;
  params[3].push_constants.count = sizeof(ShaderInjectData) / sizeof(uint32_t);
  params[3].push_constants.dx_register_index = 13;
  params[3].push_constants.dx_register_space = 0;
  params[3].push_constants.visibility = reshade::api::shader_stage::compute;

  if (!device->create_pipeline_layout(static_cast<uint32_t>(params.size()), params.data(), &resources.compute_layout)) {
    if (LogEvery(last_compute_fail_log)) logging::Warn("failed to create TAA compute pipeline layout");
    DestroyCompute(device);
    return false;
  }

  const std::array<reshade::api::sampler_desc, 2> sampler_descs = {
      reshade::api::sampler_desc{.filter = reshade::api::filter_mode::min_mag_mip_linear},
      reshade::api::sampler_desc{.filter = reshade::api::filter_mode::min_mag_mip_point},
  };

  for (size_t i = 0; i < sampler_descs.size(); ++i) {
    if (!device->create_sampler(sampler_descs[i], &resources.samplers[i])) {
      if (LogEvery(last_compute_fail_log)) logging::Warn("failed to create TAA sampler index=", i);
      DestroyCompute(device);
      return false;
    }
  }

  reshade::api::shader_desc cs_desc = {
      .code = __mgsv_taa.data(),
      .code_size = __mgsv_taa.size(),
  };
  reshade::api::pipeline_subobject cs = {
      .type = reshade::api::pipeline_subobject_type::compute_shader,
      .count = 1,
      .data = &cs_desc,
  };

  if (!device->create_pipeline(resources.compute_layout, 1, &cs, &resources.compute_pipeline)) {
    if (LogEvery(last_compute_fail_log)) logging::Warn("failed to create TAA compute pipeline");
    DestroyCompute(device);
    return false;
  }

  logging::Info("created TAA compute pipeline layout=", logging::Hex{resources.compute_layout.handle},
                " pipeline=", logging::Hex{resources.compute_pipeline.handle});
  return true;
}

// MGSV's scene-color resource is upgraded by addon.cpp to RGBA16F. We accept
// the upgraded typed format and the typeless variant that backs it.
inline bool GetSupportedHistoryFormat(
    const reshade::api::resource_desc& color_desc,
    const reshade::api::resource_view_desc& color_view_desc,
    reshade::api::format& resource_format,
    reshade::api::format& view_format) {
  resource_format = color_desc.texture.format;
  view_format = color_view_desc.format != reshade::api::format::unknown
                    ? color_view_desc.format
                    : reshade::api::format_to_default_typed(resource_format);

  if (resource_format == reshade::api::format::r16g16b16a16_typeless
      || resource_format == reshade::api::format::r16g16b16a16_float) {
    view_format = reshade::api::format::r16g16b16a16_float;
    return true;
  }

  // Color resource is the BGRA8 typeless original; the SRV format reveals the
  // upgrade clone is in play.
  if (view_format == reshade::api::format::r16g16b16a16_float) {
    resource_format = reshade::api::format::r16g16b16a16_typeless;
    return true;
  }
  return false;
}

inline bool EnsureHistory(
    reshade::api::command_list* cmd_list,
    reshade::api::resource_view color_srv,
    reshade::api::resource& color_resource) {
  auto* device = cmd_list != nullptr ? cmd_list->get_device() : nullptr;
  if (device == nullptr || color_srv.handle == 0u) return false;

  // Steady-state fast path: same color SRV as last successful dispatch.
  if (resources.history[0].resource.handle != 0u
      && resources.color_srv.handle == color_srv.handle
      && resources.color_resource.handle != 0u) {
    color_resource = resources.color_resource;
    return true;
  }

  color_resource = device->get_resource_from_view(color_srv);
  if (color_resource.handle == 0u) return false;

  const auto color_desc = device->get_resource_desc(color_resource);
  const auto color_view_desc = device->get_resource_view_desc(color_srv);
  reshade::api::format resource_format = reshade::api::format::unknown;
  reshade::api::format view_format = reshade::api::format::unknown;
  if (!GetSupportedHistoryFormat(color_desc, color_view_desc, resource_format, view_format)) {
    if (LogEvery(last_history_format_log)) {
      logging::Warn("unsupported TAA color format resource_format=", static_cast<uint32_t>(color_desc.texture.format),
                    " view_format=", static_cast<uint32_t>(color_view_desc.format),
                    " frame=", constant_buffers::frame_state.frame_index);
    }
    return false;
  }

  // Reuse history if dimensions and formats match — no rebuild on every
  // recapture of the same scene resource.
  const bool matches = resources.history[0].resource.handle != 0u
                       && resources.width == color_desc.texture.width
                       && resources.height == color_desc.texture.height
                       && resources.resource_format == resource_format
                       && resources.view_format == view_format;
  if (matches) {
    resources.color_srv = color_srv;
    resources.color_resource = color_resource;
    return true;
  }

  DestroyHistory(device);

  reshade::api::resource_desc desc = {};
  desc.type = reshade::api::resource_type::texture_2d;
  desc.texture = {
      color_desc.texture.width,
      color_desc.texture.height,
      1,
      1,
      resource_format,
      1,
  };
  desc.heap = reshade::api::memory_heap::gpu_only;
  desc.usage = reshade::api::resource_usage::shader_resource
               | reshade::api::resource_usage::unordered_access
               | reshade::api::resource_usage::copy_source
               | reshade::api::resource_usage::copy_dest;
  desc.flags = reshade::api::resource_flags::none;

  const auto srv_desc = reshade::api::resource_view_desc(
      reshade::api::resource_view_type::texture_2d, view_format, 0, 1, 0, 1);
  const auto uav_desc = reshade::api::resource_view_desc(
      reshade::api::resource_view_type::texture_2d, view_format, 0, 1, 0, 1);

  for (auto& item : resources.history) {
    if (!device->create_resource(desc, nullptr, reshade::api::resource_usage::shader_resource, &item.resource)) {
      if (LogEvery(last_history_create_fail_log)) logging::Warn("failed to create TAA history resource");
      DestroyHistory(device);
      return false;
    }
    if (!device->create_resource_view(item.resource, reshade::api::resource_usage::shader_resource, srv_desc, &item.srv)) {
      if (LogEvery(last_history_create_fail_log)) logging::Warn("failed to create TAA history SRV");
      DestroyHistory(device);
      return false;
    }
    if (!device->create_resource_view(item.resource, reshade::api::resource_usage::unordered_access, uav_desc, &item.uav)) {
      if (LogEvery(last_history_create_fail_log)) logging::Warn("failed to create TAA history UAV");
      DestroyHistory(device);
      return false;
    }
  }

  resources.width = color_desc.texture.width;
  resources.height = color_desc.texture.height;
  resources.resource_format = resource_format;
  resources.view_format = view_format;
  resources.color_srv = color_srv;
  resources.color_resource = color_resource;
  resources.accum_index = 0u;
  resources.initialized = true;

  // Seed both history textures from current color so the first resolve does
  // not blend with uninitialized memory.
  cmd_list->barrier(color_resource, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::copy_source);
  for (auto& item : resources.history) {
    cmd_list->barrier(item.resource, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::copy_dest);
    cmd_list->copy_resource(color_resource, item.resource);
    cmd_list->barrier(item.resource, reshade::api::resource_usage::copy_dest, reshade::api::resource_usage::shader_resource);
  }
  cmd_list->barrier(color_resource, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::shader_resource);

  logging::Info("created TAA history ", resources.width, "x", resources.height,
                " resource_format=", static_cast<uint32_t>(resources.resource_format),
                " view_format=", static_cast<uint32_t>(resources.view_format));
  return true;
}

inline bool DispatchCompute(
    reshade::api::command_list* cmd_list,
    reshade::api::resource_view color_srv,
    reshade::api::resource_view velocity_srv,
    reshade::api::resource_view depth_srv,
    reshade::api::resource color_resource,
    uint32_t current,
    uint32_t previous) {
  if (shader_injection == nullptr) {
    if (LogEvery(last_compute_fail_log)) logging::Warn("TAA compute dispatch missing shader injection data");
    return false;
  }

  const PreviousComputeState previous_compute_state = CaptureComputeState(cmd_list);

  const std::array<reshade::api::resource_view, 4> srvs = {
      color_srv,
      resources.history[previous].srv,
      velocity_srv,
      depth_srv,
  };
  const std::array<reshade::api::resource_view, 1> uavs = {
      resources.history[current].uav,
  };

  const std::array<reshade::api::descriptor_table_update, 3> updates = {
      reshade::api::descriptor_table_update{
          .table = {},
          .binding = 0,
          .array_offset = 0,
          .count = static_cast<uint32_t>(resources.samplers.size()),
          .type = reshade::api::descriptor_type::sampler,
          .descriptors = resources.samplers.data(),
      },
      reshade::api::descriptor_table_update{
          .table = {},
          .binding = 0,
          .array_offset = 0,
          .count = static_cast<uint32_t>(srvs.size()),
          .type = reshade::api::descriptor_type::texture_shader_resource_view,
          .descriptors = srvs.data(),
      },
      reshade::api::descriptor_table_update{
          .table = {},
          .binding = 0,
          .array_offset = 0,
          .count = static_cast<uint32_t>(uavs.size()),
          .type = reshade::api::descriptor_type::texture_unordered_access_view,
          .descriptors = uavs.data(),
      },
  };

  // Velocity and depth are already shader_resource at the insertion draw —
  // they are bound as pixel SRVs. No barrier needed on those.
  cmd_list->barrier(resources.history[current].resource, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::unordered_access);

  cmd_list->push_descriptors(reshade::api::shader_stage::all_compute, resources.compute_layout, 0, updates[0]);
  cmd_list->push_descriptors(reshade::api::shader_stage::all_compute, resources.compute_layout, 1, updates[1]);
  cmd_list->push_descriptors(reshade::api::shader_stage::all_compute, resources.compute_layout, 2, updates[2]);
  cmd_list->push_constants(
      reshade::api::shader_stage::all_compute,
      resources.compute_layout,
      3,
      0,
      sizeof(ShaderInjectData) / sizeof(uint32_t),
      shader_injection);
  cmd_list->bind_pipeline(reshade::api::pipeline_stage::all_compute, resources.compute_pipeline);
  cmd_list->dispatch((resources.width + 7u) / 8u, (resources.height + 7u) / 8u, 1u);

  cmd_list->barrier(resources.history[current].resource, reshade::api::resource_usage::unordered_access, reshade::api::resource_usage::copy_source);
  cmd_list->barrier(color_resource, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::copy_dest);
  cmd_list->copy_resource(resources.history[current].resource, color_resource);
  cmd_list->barrier(color_resource, reshade::api::resource_usage::copy_dest, reshade::api::resource_usage::shader_resource);
  cmd_list->barrier(resources.history[current].resource, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::shader_resource);

  RestoreComputeState(cmd_list, previous_compute_state);
  return true;
}

inline bool MaybeRun(
    reshade::api::command_list* cmd_list,
    const descriptor_tracker::CommandListData& command_data) {
  if (!constant_buffers::IsEnabled()) return false;
  if (constant_buffers::frame_state.taa_ran_this_frame) return false;

  const auto color_srv = command_data.pixel_srv_t0;
  const auto velocity_srv = command_data.pixel_srv_t2;
  const auto depth_srv = command_data.pixel_srv_t3;
  if (color_srv.handle == 0u || velocity_srv.handle == 0u || depth_srv.handle == 0u) {
    if (LogEvery(last_missing_inputs_log)) {
      logging::Warn("insertion missing inputs color=", logging::Hex{color_srv.handle},
                    " velocity=", logging::Hex{velocity_srv.handle},
                    " depth=", logging::Hex{depth_srv.handle});
    }
    return false;
  }

  if (!EnsureComputePipeline(cmd_list)) return false;

  reshade::api::resource color_resource = {0};
  if (!EnsureHistory(cmd_list, color_srv, color_resource)) return false;

  resources.velocity_srv = velocity_srv;
  resources.depth_srv = depth_srv;

  const uint32_t current = resources.accum_index;
  const uint32_t previous = 1u - current;
  if (!DispatchCompute(cmd_list, color_srv, velocity_srv, depth_srv, color_resource, current, previous)) {
    return false;
  }

  resources.accum_index = previous;
  // The jitter offset applied this frame is what the geometry passes saw via
  // jitter::ApplyMappedBuffer. Record it as the current sample so the next
  // frame's compute shader can subtract it from per-object velocity.
  constant_buffers::MarkTaaDispatched(constant_buffers::CurrentFrameJitter(resources.width, resources.height));

  if (LogEvery(last_dispatch_log)) {
    logging::Info("dispatched TAA frame=", constant_buffers::frame_state.frame_index,
                  " size=", resources.width, "x", resources.height,
                  " current=", current, " previous=", previous);
  }
  return true;
}

inline void OnDestroyResourceView(renodx::utils::resource::ResourceViewInfo* info) {
  if (info == nullptr) return;
  const uint64_t h = info->view.handle;
  if (h == resources.color_srv.handle) {
    resources.color_srv = {0};
    resources.color_resource = {0};
  }
  if (h == resources.velocity_srv.handle) resources.velocity_srv = {0};
  if (h == resources.depth_srv.handle) resources.depth_srv = {0};
}

}  // namespace taa::resolve
