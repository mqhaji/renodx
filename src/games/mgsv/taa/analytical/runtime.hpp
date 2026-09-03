#pragma once

/* Analytical TAA history, pipeline, and dispatch implementation. */

#include <array>
#include <cstdint>
#include <limits>

#include <embed/shaders.h>
#include <include/reshade.hpp>

#include "../runtime/frame_inputs.hpp"
#include "../runtime/logging.hpp"
#include "../runtime/state.hpp"

namespace taa::analytical {

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
  reshade::api::resource_view color_srv = {0};
  reshade::api::resource color_resource = {0};
  reshade::api::pipeline_layout compute_layout = {0};
  reshade::api::pipeline compute_pipeline = {0};
  std::array<reshade::api::sampler, 1> samplers = {};
};

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

inline Resources resources;
inline uint64_t last_compute_fail_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_history_format_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_history_create_fail_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_history_invalidate_log = std::numeric_limits<uint64_t>::max();

inline bool LogEvery(uint64_t& last_frame, uint64_t interval = 120u) {
  return logging::ShouldLogFrame(state::CurrentFrameToken(), last_frame, interval);
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

inline void Release(reshade::api::device* device) {
  if (device == nullptr
      || (resources.history[0].resource.handle == 0u
          && resources.history[1].resource.handle == 0u)) {
    return;
  }
  const uint32_t width = resources.width;
  const uint32_t height = resources.height;
  DestroyHistory(device);
  logging::Info("released inactive analytical TAA history size=", width, "x", height);
}

inline void InvalidateHistory(const char* reason) {
  const bool was_initialized = resources.initialized;
  resources.initialized = false;
  resources.accum_index = 0u;
  if (was_initialized && LogEvery(last_history_invalidate_log, 1u)) {
    logging::Info("invalidated analytical TAA history reason=", reason,
                  " frame=", state::CurrentFrameToken());
  }
}

inline bool EnsureComputePipeline(reshade::api::command_list* cmd_list) {
  auto* device = cmd_list != nullptr ? cmd_list->get_device() : nullptr;
  if (device == nullptr) return false;
  if (resources.compute_layout.handle != 0u
      && resources.compute_pipeline.handle != 0u
      && resources.samplers[0].handle != 0u) {
    return true;
  }

  DestroyCompute(device);
  std::array<reshade::api::pipeline_layout_param, 4> params = {};
  params[0].type = reshade::api::pipeline_layout_param_type::push_descriptors;
  params[0].push_descriptors.count = static_cast<uint32_t>(resources.samplers.size());
  params[0].push_descriptors.type = reshade::api::descriptor_type::sampler;
  params[0].push_descriptors.dx_register_index = 0;
  params[0].push_descriptors.dx_register_space = 0;
  params[1].type = reshade::api::pipeline_layout_param_type::push_descriptors;
  params[1].push_descriptors.count = 5;
  params[1].push_descriptors.type = reshade::api::descriptor_type::texture_shader_resource_view;
  params[1].push_descriptors.dx_register_index = 0;
  params[1].push_descriptors.dx_register_space = 0;
  params[2].type = reshade::api::pipeline_layout_param_type::push_descriptors;
  params[2].push_descriptors.count = 1;
  params[2].push_descriptors.type = reshade::api::descriptor_type::texture_unordered_access_view;
  params[2].push_descriptors.dx_register_index = 0;
  params[2].push_descriptors.dx_register_space = 0;
  params[3].type = reshade::api::pipeline_layout_param_type::push_constants;
  params[3].push_constants.count = sizeof(ResolveConstants) / sizeof(uint32_t);
  params[3].push_constants.dx_register_index = 0;
  params[3].push_constants.dx_register_space = 0;
  params[3].push_constants.visibility = reshade::api::shader_stage::compute;

  if (!device->create_pipeline_layout(
          static_cast<uint32_t>(params.size()),
          params.data(),
          &resources.compute_layout)) {
    if (LogEvery(last_compute_fail_log)) logging::Warn("failed to create TAA compute pipeline layout");
    DestroyCompute(device);
    return false;
  }

  const reshade::api::sampler_desc sampler_description = {
      .filter = reshade::api::filter_mode::min_mag_mip_point,
  };
  if (!device->create_sampler(sampler_description, resources.samplers.data())) {
    if (LogEvery(last_compute_fail_log)) logging::Warn("failed to create TAA sampler");
    DestroyCompute(device);
    return false;
  }

  reshade::api::shader_desc shader_description = {
      .code = __mgsv_taa.data(),
      .code_size = __mgsv_taa.size(),
  };
  const reshade::api::pipeline_subobject subobject = {
      .type = reshade::api::pipeline_subobject_type::compute_shader,
      .count = 1,
      .data = &shader_description,
  };
  if (!device->create_pipeline(resources.compute_layout, 1, &subobject, &resources.compute_pipeline)) {
    if (LogEvery(last_compute_fail_log)) logging::Warn("failed to create TAA compute pipeline");
    DestroyCompute(device);
    return false;
  }

  logging::Info("created TAA compute pipeline layout=", logging::Hex{resources.compute_layout.handle},
                " pipeline=", logging::Hex{resources.compute_pipeline.handle});
  return true;
}

inline bool GetSupportedHistoryFormat(
    const reshade::api::resource_desc& color_description,
    const reshade::api::resource_view_desc& color_view_description,
    reshade::api::format& resource_format,
    reshade::api::format& view_format) {
  resource_format = color_description.texture.format;
  view_format = color_view_description.format != reshade::api::format::unknown
                    ? color_view_description.format
                    : reshade::api::format_to_default_typed(resource_format);
  if (resource_format == reshade::api::format::r16g16b16a16_typeless
      || resource_format == reshade::api::format::r16g16b16a16_float) {
    view_format = reshade::api::format::r16g16b16a16_float;
    return true;
  }
  if (view_format == reshade::api::format::r16g16b16a16_float) {
    resource_format = reshade::api::format::r16g16b16a16_typeless;
    return true;
  }
  return false;
}

inline bool SeedHistory(
    reshade::api::command_list* cmd_list,
    reshade::api::resource color_resource,
    reshade::api::resource_usage color_usage) {
  if (cmd_list == nullptr || color_resource.handle == 0u) return false;
  for (const auto& item : resources.history) {
    if (item.resource.handle == 0u) return false;
  }

  cmd_list->barrier(color_resource, color_usage, reshade::api::resource_usage::copy_source);
  for (auto& item : resources.history) {
    cmd_list->barrier(
        item.resource,
        reshade::api::resource_usage::shader_resource,
        reshade::api::resource_usage::copy_dest);
    cmd_list->copy_resource(color_resource, item.resource);
    cmd_list->barrier(
        item.resource,
        reshade::api::resource_usage::copy_dest,
        reshade::api::resource_usage::shader_resource);
  }
  cmd_list->barrier(color_resource, reshade::api::resource_usage::copy_source, color_usage);
  resources.accum_index = 0u;
  resources.initialized = true;
  return true;
}

inline bool EnsureHistory(const ValidatedFrameInputs& inputs, bool& history_seeded) {
  auto* device = inputs.device;
  history_seeded = false;
  if (device == nullptr) return false;

  if (resources.history[0].resource.handle != 0u
      && resources.color_srv.handle == inputs.color_srv.handle
      && resources.color_resource.handle == inputs.color_resource.handle) {
    if (!resources.initialized) {
      history_seeded = SeedHistory(
          inputs.cmd_list,
          inputs.color_resource,
          reshade::api::resource_usage::shader_resource);
      return history_seeded;
    }
    return true;
  }

  const auto color_view_description = device->get_resource_view_desc(inputs.color_srv);
  reshade::api::format resource_format = reshade::api::format::unknown;
  reshade::api::format view_format = reshade::api::format::unknown;
  if (!GetSupportedHistoryFormat(
          inputs.color_description,
          color_view_description,
          resource_format,
          view_format)) {
    if (LogEvery(last_history_format_log)) {
      logging::Warn("unsupported TAA color format resource_format=",
                    static_cast<uint32_t>(inputs.color_description.texture.format),
                    " view_format=", static_cast<uint32_t>(color_view_description.format),
                    " frame=", inputs.frame_token);
    }
    return false;
  }

  const bool matches = resources.history[0].resource.handle != 0u
                       && resources.width == inputs.width
                       && resources.height == inputs.height
                       && resources.resource_format == resource_format
                       && resources.view_format == view_format;
  if (matches) {
    resources.color_srv = inputs.color_srv;
    resources.color_resource = inputs.color_resource;
    if (!resources.initialized) {
      history_seeded = SeedHistory(
          inputs.cmd_list,
          inputs.color_resource,
          reshade::api::resource_usage::shader_resource);
      return history_seeded;
    }
    return true;
  }

  DestroyHistory(device);
  reshade::api::resource_desc description = {};
  description.type = reshade::api::resource_type::texture_2d;
  description.texture = {
      .width = inputs.width,
      .height = inputs.height,
      .depth_or_layers = 1,
      .levels = 1,
      .format = resource_format,
      .samples = 1,
  };
  description.heap = reshade::api::memory_heap::gpu_only;
  description.usage = reshade::api::resource_usage::shader_resource
                      | reshade::api::resource_usage::unordered_access
                      | reshade::api::resource_usage::copy_source
                      | reshade::api::resource_usage::copy_dest;
  description.flags = reshade::api::resource_flags::none;
  const auto view_description = reshade::api::resource_view_desc(
      reshade::api::resource_view_type::texture_2d,
      view_format,
      0,
      1,
      0,
      1);

  for (auto& item : resources.history) {
    if (!device->create_resource(
            description,
            nullptr,
            reshade::api::resource_usage::shader_resource,
            &item.resource)
        || !device->create_resource_view(
            item.resource,
            reshade::api::resource_usage::shader_resource,
            view_description,
            &item.srv)
        || !device->create_resource_view(
            item.resource,
            reshade::api::resource_usage::unordered_access,
            view_description,
            &item.uav)) {
      if (LogEvery(last_history_create_fail_log)) logging::Warn("failed to create TAA history resources");
      DestroyHistory(device);
      return false;
    }
  }

  resources.width = inputs.width;
  resources.height = inputs.height;
  resources.resource_format = resource_format;
  resources.view_format = view_format;
  resources.color_srv = inputs.color_srv;
  resources.color_resource = inputs.color_resource;
  resources.initialized = false;
  history_seeded = SeedHistory(
      inputs.cmd_list,
      inputs.color_resource,
      reshade::api::resource_usage::shader_resource);
  if (!history_seeded) {
    DestroyHistory(device);
    return false;
  }

  logging::Info("created TAA history ", resources.width, "x", resources.height,
                " resource_format=", static_cast<uint32_t>(resources.resource_format),
                " view_format=", static_cast<uint32_t>(resources.view_format));
  return true;
}

inline void DispatchCompute(
    const ValidatedFrameInputs& inputs,
    uint32_t current,
    uint32_t previous,
    const ResolveConstants& constants) {
  const std::array<reshade::api::resource_view, 5> srvs = {
      inputs.color_srv,
      resources.history[previous].srv,
      inputs.velocity_srv,
      inputs.depth_srv,
      inputs.object_velocity_srv,
  };
  const std::array<reshade::api::resource_view, 1> uavs = {resources.history[current].uav};
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

  inputs.cmd_list->barrier(
      resources.history[current].resource,
      reshade::api::resource_usage::shader_resource,
      reshade::api::resource_usage::unordered_access);
  for (uint32_t index = 0u; index < updates.size(); ++index) {
    inputs.cmd_list->push_descriptors(
        reshade::api::shader_stage::all_compute,
        resources.compute_layout,
        index,
        updates[index]);
  }
  inputs.cmd_list->push_constants(
      reshade::api::shader_stage::all_compute,
      resources.compute_layout,
      3,
      0,
      sizeof(ResolveConstants) / sizeof(uint32_t),
      &constants);
  inputs.cmd_list->bind_pipeline(reshade::api::pipeline_stage::all_compute, resources.compute_pipeline);
  inputs.cmd_list->dispatch((resources.width + 7u) / 8u, (resources.height + 7u) / 8u, 1u);
}

inline bool Dispatch(const ValidatedFrameInputs& inputs, MethodOutput& output) {
  if (resources.initialized && !inputs.camera.camera_reprojection_valid) {
    InvalidateHistory("native camera matrix history unavailable");
  }
  if (!EnsureComputePipeline(inputs.cmd_list)) return false;

  bool history_seeded = false;
  if (!EnsureHistory(inputs, history_seeded)) return false;
  const uint32_t current = resources.accum_index;
  const uint32_t previous = 1u - current;
  const ResolveConstants constants = {
      .diagnostic_view = state::GetDiagnosticView(),
      .velocity_visualization_range = state::GetVelocityVisualizationRange(),
      .camera_reprojection_valid = !history_seeded && inputs.camera.camera_reprojection_valid ? 1.f : 0.f,
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
  DispatchCompute(inputs, current, previous, constants);

  if (history_seeded) {
    logging::Info("TAA accumulation started insertion=", inputs.insertion_name,
                  " frame=", inputs.frame_token,
                  " native_frame=", inputs.camera.frame_token,
                  " sample=", inputs.sample_index,
                  " size=", resources.width, "x", resources.height);
  }
  resources.accum_index = previous;
  output = {
      .resource = resources.history[current].resource,
      .final_usage = reshade::api::resource_usage::shader_resource,
  };
  return true;
}

}  // namespace taa::analytical
