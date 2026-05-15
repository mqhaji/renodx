#pragma once

/*
 * MGSV temporal resolve implementation.
 *
 * Reads HDR color at the insertion draw, captures camera velocity/depth from
 * MotionBlurCameraVelocity earlier in the same frame, dispatches the embedded
 * compute shader, ping-pongs history textures, and copies the resolved result
 * back into the HDR color resource before the insertion draw runs.
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
 * MGSV differs from Alien Isolation only in the resource slots/formats:
 * MotionBlurCameraVelocity writes BGRA8 velocity to RTV0 and reads depth at
 * pixel t2. The compute shader decodes that BGRA8 velocity directly.
 */

#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <embed/shaders.h>
#include <include/reshade.hpp>

#include "../../../../utils/resource_upgrade.hpp"
#include "../../../../utils/resource.hpp"
#include "../../../../utils/state.hpp"
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

  // SRV created over the active scene-color RTV clone for the deferred
  // LocalReflectionAddBuffer insertion point. The game has only bound this
  // resource as an RTV at that point, so there may not be a game SRV to reuse.
  reshade::api::resource_view rtv_color_srv = {0};
  reshade::api::resource rtv_color_resource = {0};
  reshade::api::format rtv_color_view_format = reshade::api::format::unknown;

  reshade::api::resource velocity_resource = {0};
  reshade::api::resource_view velocity_rtv = {0};
  reshade::api::resource_view velocity_srv = {0};
  reshade::api::format velocity_view_format = reshade::api::format::unknown;
  reshade::api::resource_view depth_srv = {0};
  uint64_t capture_frame = std::numeric_limits<uint64_t>::max();

  reshade::api::pipeline_layout compute_layout = {0};
  reshade::api::pipeline compute_pipeline = {0};
  std::array<reshade::api::sampler, 2> samplers = {};
};

inline Resources resources;
inline uint64_t last_dispatch_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_missing_inputs_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_capture_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_capture_missing_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_stale_capture_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_compute_fail_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_history_format_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_history_create_fail_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_rtv_srv_create_fail_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_color_size_reject_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_dispatch_setup_log = std::numeric_limits<uint64_t>::max();

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

inline void DestroyRenderTargetColorSrv(reshade::api::device* device) {
  if (device == nullptr) return;
  if (resources.rtv_color_srv.handle != 0u) device->destroy_resource_view(resources.rtv_color_srv);
  resources.rtv_color_srv = {0};
  resources.rtv_color_resource = {0};
  resources.rtv_color_view_format = reshade::api::format::unknown;
}

inline void DestroyVelocitySrv(reshade::api::device* device) {
  if (device == nullptr) return;
  if (resources.velocity_srv.handle != 0u) device->destroy_resource_view(resources.velocity_srv);
  resources.velocity_resource = {0};
  resources.velocity_rtv = {0};
  resources.velocity_srv = {0};
  resources.velocity_view_format = reshade::api::format::unknown;
  resources.capture_frame = std::numeric_limits<uint64_t>::max();
}

inline void Destroy(reshade::api::device* device) {
  DestroyCompute(device);
  DestroyHistory(device);
  DestroyRenderTargetColorSrv(device);
  DestroyVelocitySrv(device);
  resources = {};
}

// Layout mirrors the compute shader registers:
//   s0-s1 samplers, t0-t3 SRVs, u0 UAV.
// Jitter state is CPU-owned, matching Alien Isolation, so the compute shader
// receives no jitter constants.
inline bool EnsureComputePipeline(reshade::api::command_list* cmd_list) {
  auto* device = cmd_list != nullptr ? cmd_list->get_device() : nullptr;
  if (device == nullptr) return false;

  if (resources.compute_layout.handle != 0u && resources.compute_pipeline.handle != 0u
      && resources.samplers[0].handle != 0u && resources.samplers[1].handle != 0u) {
    return true;
  }

  DestroyCompute(device);

  std::array<reshade::api::pipeline_layout_param, 3> params = {};
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

inline reshade::api::resource_view NormalizeColorSrv(reshade::api::resource_view color_srv) {
  if (color_srv.handle == 0u) return {0u};

  const auto clone = renodx::utils::resource::upgrade::GetResourceViewClone(color_srv, {
      .require_enabled = true,
      .allow_create = true,
      .activate = false,
  });
  return clone.handle != 0u ? clone : color_srv;
}

inline bool EnsureHistory(
    reshade::api::command_list* cmd_list,
    reshade::api::resource_view color_srv,
  reshade::api::resource& color_resource,
  reshade::api::resource_usage color_current_usage = reshade::api::resource_usage::shader_resource) {
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
  cmd_list->barrier(color_resource, color_current_usage, reshade::api::resource_usage::copy_source);
  for (auto& item : resources.history) {
    cmd_list->barrier(item.resource, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::copy_dest);
    cmd_list->copy_resource(color_resource, item.resource);
    cmd_list->barrier(item.resource, reshade::api::resource_usage::copy_dest, reshade::api::resource_usage::shader_resource);
  }
  cmd_list->barrier(color_resource, reshade::api::resource_usage::copy_source, color_current_usage);

  logging::Info("created TAA history ", resources.width, "x", resources.height,
                " resource_format=", static_cast<uint32_t>(resources.resource_format),
                " view_format=", static_cast<uint32_t>(resources.view_format));
  return true;
}

inline bool EnsureColorSrvFromRenderTarget(
    reshade::api::command_list* cmd_list,
    reshade::api::resource_view color_rtv,
    reshade::api::resource_view& color_srv) {
  auto* device = cmd_list != nullptr ? cmd_list->get_device() : nullptr;
  color_srv = {0};
  if (device == nullptr || color_rtv.handle == 0u) return false;

  const auto color_rtv_clone = renodx::utils::resource::upgrade::GetResourceViewClone(color_rtv, {
      .require_enabled = true,
      .allow_create = true,
      .activate = false,
  });
  const auto color_rtv_for_srv = color_rtv_clone.handle != 0u ? color_rtv_clone : color_rtv;

  const auto color_resource = device->get_resource_from_view(color_rtv_for_srv);
  if (color_resource.handle == 0u) return false;

  const auto color_desc = device->get_resource_desc(color_resource);
  const auto color_view_desc = device->get_resource_view_desc(color_rtv_for_srv);
  reshade::api::format resource_format = reshade::api::format::unknown;
  reshade::api::format color_view_format = reshade::api::format::unknown;
  if (!GetSupportedHistoryFormat(color_desc, color_view_desc, resource_format, color_view_format)) return false;
  (void)resource_format;

  if (resources.rtv_color_srv.handle != 0u
      && resources.rtv_color_resource.handle == color_resource.handle
      && resources.rtv_color_view_format == color_view_format) {
    color_srv = resources.rtv_color_srv;
    return true;
  }

  DestroyRenderTargetColorSrv(device);

  const auto srv_desc = reshade::api::resource_view_desc(
      reshade::api::resource_view_type::texture_2d,
      color_view_format,
      0,
      1,
      0,
      1);
  if (!device->create_resource_view(color_resource, reshade::api::resource_usage::shader_resource, srv_desc, &resources.rtv_color_srv)) {
    if (LogEvery(last_rtv_srv_create_fail_log)) {
      logging::Warn("failed to create TAA SRV for scene RTV clone resource=", logging::Hex{color_resource.handle},
                    " view_format=", static_cast<uint32_t>(color_view_format));
    }
    resources.rtv_color_resource = {0};
    resources.rtv_color_view_format = reshade::api::format::unknown;
    return false;
  }

  resources.rtv_color_resource = color_resource;
  resources.rtv_color_view_format = color_view_format;
  color_srv = resources.rtv_color_srv;
  logging::Info("created TAA scene RTV SRV resource=", logging::Hex{color_resource.handle},
                " srv=", logging::Hex{color_srv.handle},
                " view_format=", static_cast<uint32_t>(color_view_format));
  return true;
}

inline reshade::api::format GetTypedViewFormat(reshade::api::device* device, reshade::api::resource_view view) {
  if (device == nullptr || view.handle == 0u) return reshade::api::format::unknown;
  const auto view_desc = device->get_resource_view_desc(view);
  if (view_desc.format != reshade::api::format::unknown) return view_desc.format;
  const auto resource = device->get_resource_from_view(view);
  if (resource.handle == 0u) return reshade::api::format::unknown;
  const auto desc = device->get_resource_desc(resource);
  return reshade::api::format_to_default_typed(desc.texture.format);
}

inline bool EnsureVelocitySrv(reshade::api::command_list* cmd_list, reshade::api::resource_view velocity_rtv) {
  auto* device = cmd_list != nullptr ? cmd_list->get_device() : nullptr;
  if (device == nullptr || velocity_rtv.handle == 0u) return false;

  if (resources.velocity_srv.handle != 0u
      && resources.velocity_rtv.handle == velocity_rtv.handle
      && resources.velocity_resource.handle != 0u) {
    return true;
  }

  const auto velocity_resource = device->get_resource_from_view(velocity_rtv);
  if (velocity_resource.handle == 0u) return false;

  const auto view_format = GetTypedViewFormat(device, velocity_rtv);
  if (view_format == reshade::api::format::unknown) return false;
  if (resources.velocity_srv.handle != 0u
      && resources.velocity_resource.handle == velocity_resource.handle
      && resources.velocity_view_format == view_format) {
    resources.velocity_rtv = velocity_rtv;
    return true;
  }

  DestroyVelocitySrv(device);

  const auto view_desc = reshade::api::resource_view_desc(
      reshade::api::resource_view_type::texture_2d,
      view_format,
      0,
      1,
      0,
      1);
  if (!device->create_resource_view(velocity_resource, reshade::api::resource_usage::shader_resource, view_desc, &resources.velocity_srv)) {
    resources.velocity_resource = {0};
    resources.velocity_view_format = reshade::api::format::unknown;
    logging::Warn("failed to create velocity SRV view_format=", static_cast<uint32_t>(view_format));
    return false;
  }

  resources.velocity_resource = velocity_resource;
  resources.velocity_rtv = velocity_rtv;
  resources.velocity_view_format = view_format;
  logging::Info("created velocity SRV resource=", logging::Hex{velocity_resource.handle},
                " view_format=", static_cast<uint32_t>(view_format));
  return true;
}

inline void CaptureCameraMotion(
    reshade::api::command_list* cmd_list,
    const descriptor_tracker::CommandListData& command_data,
    reshade::api::resource_view velocity_rtv) {
  if (velocity_rtv.handle == 0u) {
    if (LogEvery(last_capture_missing_log)) logging::Warn("camera velocity pass has no RTV0");
    return;
  }

  // Snapshot-verified: MotionBlurCameraVelocity_ps reads depth from pixel t2
  // and writes the final camera/object velocity texture to RTV0.
  const auto depth_srv = command_data.pixel_srv_t2;
  if (depth_srv.handle == 0u) {
    if (LogEvery(last_capture_missing_log)) {
      logging::Warn("camera velocity capture missing depth_srv_t2 velocity_rtv=", logging::Hex{velocity_rtv.handle});
    }
    return;
  }
  if (!EnsureVelocitySrv(cmd_list, velocity_rtv)) return;

  resources.depth_srv = depth_srv;
  resources.capture_frame = constant_buffers::frame_state.frame_index;

  if (LogEvery(last_capture_log, 30u)) {
    auto* device = cmd_list != nullptr ? cmd_list->get_device() : nullptr;
    uint32_t velocity_width = 0u;
    uint32_t velocity_height = 0u;
    uint32_t depth_width = 0u;
    uint32_t depth_height = 0u;
    uint32_t depth_format = 0u;
    if (device != nullptr) {
      if (resources.velocity_resource.handle != 0u) {
        const auto velocity_desc = device->get_resource_desc(resources.velocity_resource);
        velocity_width = velocity_desc.texture.width;
        velocity_height = velocity_desc.texture.height;
      }
      const auto depth_resource = device->get_resource_from_view(resources.depth_srv);
      if (depth_resource.handle != 0u) {
        const auto depth_desc = device->get_resource_desc(depth_resource);
        const auto depth_view_desc = device->get_resource_view_desc(resources.depth_srv);
        depth_width = depth_desc.texture.width;
        depth_height = depth_desc.texture.height;
        depth_format = static_cast<uint32_t>(depth_view_desc.format);
      }
    }
    logging::Info("captured camera velocity inputs frame=", resources.capture_frame,
                  " velocity_rtv=", logging::Hex{velocity_rtv.handle},
                  " velocity_srv=", logging::Hex{resources.velocity_srv.handle},
                  " velocity_resource=", logging::Hex{resources.velocity_resource.handle},
                  " velocity_size=", velocity_width, "x", velocity_height,
                  " velocity_format=", static_cast<uint32_t>(resources.velocity_view_format),
                  " depth_srv=", logging::Hex{resources.depth_srv.handle},
                  " depth_size=", depth_width, "x", depth_height,
                  " depth_view_format=", depth_format);
  }
}

inline bool DispatchCompute(
    reshade::api::command_list* cmd_list,
    reshade::api::resource_view color_srv,
    reshade::api::resource color_resource,
    reshade::api::resource_usage color_initial_usage,
    reshade::api::resource_usage color_final_usage,
    uint32_t current,
  uint32_t previous) {
  const PreviousComputeState previous_compute_state = CaptureComputeState(cmd_list);

  const std::array<reshade::api::resource_view, 4> srvs = {
      color_srv,
      resources.history[previous].srv,
      resources.velocity_srv,
      resources.depth_srv,
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

  cmd_list->barrier(resources.velocity_resource, reshade::api::resource_usage::render_target, reshade::api::resource_usage::shader_resource);
  if (color_initial_usage != reshade::api::resource_usage::shader_resource) {
    cmd_list->barrier(color_resource, color_initial_usage, reshade::api::resource_usage::shader_resource);
  }
  cmd_list->barrier(resources.history[current].resource, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::unordered_access);

  cmd_list->push_descriptors(reshade::api::shader_stage::all_compute, resources.compute_layout, 0, updates[0]);
  cmd_list->push_descriptors(reshade::api::shader_stage::all_compute, resources.compute_layout, 1, updates[1]);
  cmd_list->push_descriptors(reshade::api::shader_stage::all_compute, resources.compute_layout, 2, updates[2]);
  cmd_list->bind_pipeline(reshade::api::pipeline_stage::all_compute, resources.compute_pipeline);
  cmd_list->dispatch((resources.width + 7u) / 8u, (resources.height + 7u) / 8u, 1u);

  cmd_list->barrier(resources.history[current].resource, reshade::api::resource_usage::unordered_access, reshade::api::resource_usage::copy_source);
  cmd_list->barrier(color_resource, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::copy_dest);
  cmd_list->copy_resource(resources.history[current].resource, color_resource);
  cmd_list->barrier(color_resource, reshade::api::resource_usage::copy_dest, color_final_usage);
  cmd_list->barrier(resources.history[current].resource, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::shader_resource);
  cmd_list->barrier(resources.velocity_resource, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::render_target);

  RestoreComputeState(cmd_list, previous_compute_state);
  return true;
}

inline bool ColorMatchesCapturedDepth(
    reshade::api::command_list* cmd_list,
    reshade::api::resource_view color_srv) {
  auto* device = cmd_list != nullptr ? cmd_list->get_device() : nullptr;
  if (device == nullptr || color_srv.handle == 0u || resources.depth_srv.handle == 0u) return false;

  const auto color_resource = device->get_resource_from_view(color_srv);
  const auto depth_resource = device->get_resource_from_view(resources.depth_srv);
  if (color_resource.handle == 0u || depth_resource.handle == 0u) return false;

  const auto color_desc = device->get_resource_desc(color_resource);
  const auto depth_desc = device->get_resource_desc(depth_resource);
  const bool matches = color_desc.texture.width == depth_desc.texture.width
                       && color_desc.texture.height == depth_desc.texture.height;
  if (!matches && LogEvery(last_color_size_reject_log, 30u)) {
    logging::Warn("rejecting non-full-res TAA color size=", color_desc.texture.width, "x", color_desc.texture.height,
                  " expected=", depth_desc.texture.width, "x", depth_desc.texture.height,
                  " color_srv=", logging::Hex{color_srv.handle},
                  " depth_srv=", logging::Hex{resources.depth_srv.handle});
  }
  return matches;
}

inline bool MaybeRunFromColorSrv(
    reshade::api::command_list* cmd_list,
    reshade::api::resource_view color_srv,
    reshade::api::resource_usage color_initial_usage = reshade::api::resource_usage::shader_resource,
    reshade::api::resource_usage color_final_usage = reshade::api::resource_usage::shader_resource,
    const char* insertion_name = "unknown") {
  if (!constant_buffers::IsEnabled()) return false;
  if (constant_buffers::frame_state.taa_ran_this_frame) return false;

  if (color_srv.handle == 0u || resources.velocity_srv.handle == 0u || resources.depth_srv.handle == 0u) {
    if (LogEvery(last_missing_inputs_log)) {
      logging::Warn("insertion missing inputs insertion=", insertion_name,
                    " color=", logging::Hex{color_srv.handle},
                    " velocity=", logging::Hex{resources.velocity_srv.handle},
                    " depth=", logging::Hex{resources.depth_srv.handle});
    }
    return false;
  }
  if (resources.capture_frame != constant_buffers::frame_state.frame_index) {
    if (LogEvery(last_stale_capture_log)) {
      logging::Warn("insertion reached with stale camera velocity capture insertion=", insertion_name,
                    " capture_frame=", resources.capture_frame,
                    " frame=", constant_buffers::frame_state.frame_index);
    }
    return false;
  }
  if (!ColorMatchesCapturedDepth(cmd_list, color_srv)) return false;

  if (!EnsureComputePipeline(cmd_list)) return false;

  reshade::api::resource color_resource = {0};
  if (!EnsureHistory(cmd_list, color_srv, color_resource, color_initial_usage)) return false;

  const uint32_t current = resources.accum_index;
  const uint32_t previous = 1u - current;
  const auto applied_jitter = constant_buffers::CurrentFrameJitter(resources.width, resources.height);
  const auto previous_history_jitter = constant_buffers::frame_state.current_jitter;
  if (LogEvery(last_dispatch_setup_log, 30u)) {
    auto* device = cmd_list != nullptr ? cmd_list->get_device() : nullptr;
    uint32_t color_width = 0u;
    uint32_t color_height = 0u;
    uint32_t color_format = 0u;
    uint64_t color_handle = color_resource.handle;
    if (device != nullptr && color_resource.handle != 0u) {
      const auto color_desc = device->get_resource_desc(color_resource);
      color_width = color_desc.texture.width;
      color_height = color_desc.texture.height;
      color_format = static_cast<uint32_t>(color_desc.texture.format);
    }
    logging::Info("TAA dispatch setup insertion=", insertion_name,
                  " frame=", constant_buffers::frame_state.frame_index,
                  " color_srv=", logging::Hex{color_srv.handle},
                  " color_resource=", logging::Hex{color_handle},
                  " color_size=", color_width, "x", color_height,
                  " color_format=", color_format,
                  " velocity_srv=", logging::Hex{resources.velocity_srv.handle},
                  " depth_srv=", logging::Hex{resources.depth_srv.handle},
                  " history_current=", current,
                  " history_previous=", previous,
                  " jitter_uv=", applied_jitter[0], ",", applied_jitter[1],
                  " previous_jitter_uv=", previous_history_jitter[0], ",", previous_history_jitter[1]);
  }
  if (!DispatchCompute(cmd_list, color_srv, color_resource, color_initial_usage, color_final_usage, current, previous)) {
    return false;
  }

  resources.accum_index = previous;
  // The jitter offset applied this frame is what the geometry passes saw via
  // jitter::ApplyMappedBuffer. Record it for history/debug bookkeeping only;
  // the compute shader receives no jitter constants.
  constant_buffers::MarkTaaDispatched(applied_jitter);

  if (LogEvery(last_dispatch_log)) {
    logging::Info("dispatched TAA insertion=", insertion_name,
                  " frame=", constant_buffers::frame_state.frame_index,
                  " size=", resources.width, "x", resources.height,
                  " current=", current, " previous=", previous,
                  " jitter=", applied_jitter[0], ",", applied_jitter[1],
                  " previous_jitter=", previous_history_jitter[0], ",", previous_history_jitter[1]);
  }
  return true;
}

inline bool MaybeRun(
    reshade::api::command_list* cmd_list,
    const descriptor_tracker::CommandListData& command_data,
    const char* insertion_name = "SRV") {
  const auto color_srv = NormalizeColorSrv(command_data.pixel_srv_t0);
  return MaybeRunFromColorSrv(cmd_list, color_srv,
                              reshade::api::resource_usage::shader_resource,
                              reshade::api::resource_usage::shader_resource,
                              insertion_name);
}

inline bool MaybeRunOnRenderTarget(
    reshade::api::command_list* cmd_list,
    reshade::api::resource_view color_rtv,
    const char* insertion_name = "RTV") {
  reshade::api::resource_view color_srv = {0};
  if (!EnsureColorSrvFromRenderTarget(cmd_list, color_rtv, color_srv)) {
    if (LogEvery(last_missing_inputs_log)) {
      logging::Warn("insertion missing scene RTV clone insertion=", insertion_name,
                    " color_rtv=", logging::Hex{color_rtv.handle});
    }
    return false;
  }
  return MaybeRunFromColorSrv(
      cmd_list,
      color_srv,
      reshade::api::resource_usage::render_target,
      reshade::api::resource_usage::render_target,
      insertion_name);
}

inline void OnDestroyResourceView(renodx::utils::resource::ResourceViewInfo* info) {
  if (info == nullptr) return;
  const uint64_t h = info->view.handle;
  if (h == resources.color_srv.handle) {
    resources.color_srv = {0};
    resources.color_resource = {0};
  }
  if (h == resources.rtv_color_srv.handle) {
    resources.rtv_color_srv = {0};
    resources.rtv_color_resource = {0};
    resources.rtv_color_view_format = reshade::api::format::unknown;
  }
  if (h == resources.velocity_rtv.handle) resources.velocity_rtv = {0};
  if (h == resources.velocity_srv.handle) resources.velocity_srv = {0};
  if (h == resources.depth_srv.handle) resources.depth_srv = {0};
}

}  // namespace taa::resolve
