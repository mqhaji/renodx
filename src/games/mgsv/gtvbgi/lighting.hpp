/*
 * Copyright (C) 2024 Musa Haji
 * Copyright (C) 2026 Musa Haji
 * Copyright (C) 2026 ZioPao
 * SPDX-License-Identifier: MIT
 */

#ifndef SRC_GAMES_MGSV_LIGHTING_HPP_
#define SRC_GAMES_MGSV_LIGHTING_HPP_

#include <d3d11.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <shared_mutex>
#include <span>
#include <sstream>
#include <string>
#include <utility>

#include <wrl/client.h>

#include <embed/shaders.h>
#include <include/reshade.hpp>

#include "../../../utils/mutex.hpp"
#include "../../../utils/settings.hpp"
#include "../assets/noise/fast_noise_gr8.h"
#include "../d3d11_compute_state.hpp"
#include "../inputs.hpp"
#include "../reconstruction/coordinator.hpp"
#include "../shared.h"
#include "temporal_inputs.hpp"

// Shared lighting resources and dispatch support.
namespace mgsv::lighting {

template <typename... Args>
inline void LogLight(reshade::log::level level, Args&&... args) {
  std::ostringstream text;
  text << "MgsvLighting: ";
  (text << ... << std::forward<Args>(args));
  reshade::log::message(level, text.str().c_str());
}

// Report each diagnostic once per addon load, even when multiple contexts record it.
template <typename... Args>
inline void LogLightOnce(std::atomic_flag* reported, reshade::log::level level, Args&&... args) {
  if (reported->test(std::memory_order_relaxed)
      || reported->test_and_set(std::memory_order_relaxed)) return;
  LogLight(level, std::forward<Args>(args)...);
}

// Borrowed bindings for one lighting callback, not a retained or validated frame snapshot.
struct LightFrame {
  reshade::api::resource_view depth = {0};
  reshade::api::resource_view normal = {0};
  reshade::api::resource_view light = {0};
  reshade::api::resource_view light_source = {0};
  reshade::api::resource_view albedo = {0};
  reshade::api::resource_view specular = {0};
  uint32_t width = 0;
  uint32_t height = 0;
};

struct LightPass {
  reshade::api::pipeline pipeline = {0};
};

struct LightTarget {
  reshade::api::resource resource = {0};
  reshade::api::resource_view view = {0};
  reshade::api::resource_view uav = {0};
  uint32_t width = 0;
  uint32_t height = 0;
  reshade::api::format format = reshade::api::format::unknown;
};

inline reshade::api::pipeline_layout light_pass_layout = {0};
inline reshade::api::resource noise_resource = {0};
inline reshade::api::resource_view noise_view = {0};
inline std::atomic<uint32_t> noise_frame = 0u;
inline bool noise_creation_failed = false;

inline void DestroyLightTarget(reshade::api::device* device, LightTarget* target) {
  if (target->uav.handle != 0) {
    device->destroy_resource_view(target->uav);
    target->uav = {0};
  }
  if (target->view.handle != 0) {
    device->destroy_resource_view(target->view);
    target->view = {0};
  }
  if (target->resource.handle != 0) {
    device->destroy_resource(target->resource);
    target->resource = {0};
  }
  target->width = 0;
  target->height = 0;
}

inline bool EnsureLightTarget(reshade::api::device* device, LightTarget* target, uint32_t width,
                              uint32_t height,
                              reshade::api::format format = reshade::api::format::r16g16b16a16_float) {
  if (target->uav.handle != 0 && target->width == width && target->height == height
      && target->format == format) {
    return true;
  }

  const uint32_t previous_width = target->width;
  const uint32_t previous_height = target->height;
  DestroyLightTarget(device, target);
  target->format = format;

  const reshade::api::resource_desc desc(
      width, height, 1u, 1u, format,
      1u,
      reshade::api::memory_heap::gpu_only,
      reshade::api::resource_usage::shader_resource
          | reshade::api::resource_usage::unordered_access);
  if (!device->create_resource(desc, nullptr, desc.usage, &target->resource)
      || !device->create_resource_view(
          target->resource,
          reshade::api::resource_usage::shader_resource,
          reshade::api::resource_view_desc(format),
          &target->view)
      || !device->create_resource_view(
          target->resource,
          reshade::api::resource_usage::unordered_access,
          reshade::api::resource_view_desc(format),
          &target->uav)) {
    static std::atomic_flag reported = {};
    LogLightOnce(&reported, reshade::log::level::error,
                 "failed to create lighting texture; size=", width, "x", height,
                 " format=", static_cast<uint32_t>(format));
    return false;
  }

  target->width = width;
  target->height = height;
  LogLight(reshade::log::level::info, "created lighting texture; previous_size=",
           previous_width, "x", previous_height, " size=", width, "x", height,
           " format=", static_cast<uint32_t>(format));
  return true;
}

inline bool EnsureLightPassLayout(reshade::api::device* device) {
  if (light_pass_layout.handle != 0) return true;

  std::array<reshade::api::pipeline_layout_param, 3> params = {};
  params[0].type = reshade::api::pipeline_layout_param_type::push_descriptors;
  params[0].push_descriptors.count = 9u;  // t0-t8; input roles vary by pass
  params[0].push_descriptors.type = reshade::api::descriptor_type::texture_shader_resource_view;
  params[0].push_descriptors.dx_register_index = 0u;
  params[0].push_descriptors.dx_register_space = 0u;
  params[1].type = reshade::api::pipeline_layout_param_type::push_descriptors;
  params[1].push_descriptors.count = 4u;  // u0-u3; only accumulation uses all four outputs
  params[1].push_descriptors.type = reshade::api::descriptor_type::texture_unordered_access_view;
  params[1].push_descriptors.dx_register_index = 0u;
  params[1].push_descriptors.dx_register_space = 0u;
  params[2].type = reshade::api::pipeline_layout_param_type::push_constants;
  params[2].push_constants.count = static_cast<uint32_t>(sizeof(ShaderInjectData) / sizeof(uint32_t));
  params[2].push_constants.dx_register_index = 13u;
  params[2].push_constants.dx_register_space = 0u;
  params[2].push_constants.visibility = reshade::api::shader_stage::compute;
  if (!device->create_pipeline_layout(
          static_cast<uint32_t>(params.size()), params.data(), &light_pass_layout)) {
    static std::atomic_flag reported = {};
    LogLightOnce(&reported, reshade::log::level::error, "failed to create lighting pipeline layout");
    return false;
  }
  return true;
}

inline bool EnsureLightPass(reshade::api::device* device, LightPass* pass, const char* name,
                            std::span<const uint8_t> code) {
  // Each pass uses fixed bytecode and layout on its owning device until teardown.
  if (pass->pipeline.handle != 0) return true;
  if (!EnsureLightPassLayout(device)) return false;

  reshade::api::shader_desc shader_description = {
      .code = code.data(),
      .code_size = code.size(),
  };
  const reshade::api::pipeline_subobject subobject = {
      .type = reshade::api::pipeline_subobject_type::compute_shader,
      .count = 1,
      .data = &shader_description,
  };
  reshade::api::pipeline pipeline = {0};
  if (!device->create_pipeline(light_pass_layout, 1, &subobject, &pipeline)) {
    static std::atomic_flag reported = {};
    LogLightOnce(&reported, reshade::log::level::error, "failed to create lighting pipeline; pass=", name);
    return false;
  }

  pass->pipeline = pipeline;
  LogLight(reshade::log::level::info, name);
  return true;
}

inline void DestroyLightPass(reshade::api::device* device, LightPass* pass) {
  if (pass->pipeline.handle != 0) {
    device->destroy_pipeline(pass->pipeline);
    pass->pipeline = {0};
  }
}

inline void DestroyLightPassLayout(reshade::api::device* device) {
  if (noise_view.handle != 0) {
    device->destroy_resource_view(noise_view);
    noise_view = {0};
  }
  if (noise_resource.handle != 0) {
    device->destroy_resource(noise_resource);
    noise_resource = {0};
  }
  noise_frame.store(0u, std::memory_order_relaxed);
  noise_creation_failed = false;
  if (light_pass_layout.handle != 0) {
    device->destroy_pipeline_layout(light_pass_layout);
    light_pass_layout = {0};
  }
}

// 1x1 placeholders for unused input and output slots in the shared layout.
// Keep the input texture separate from the output textures, with one texture per
// output slot, so binding a placeholder cannot unbind another input or output.
inline reshade::api::resource fallback_srv_resource = {0};
inline reshade::api::resource_view fallback_srv = {0};
inline std::array<reshade::api::resource, 3> fallback_uav_resources = {};
inline std::array<reshade::api::resource_view, 3> fallback_uavs = {};

inline void DestroyFallbackViews(reshade::api::device* device) {
  for (auto& view : fallback_uavs) {
    if (view.handle != 0) {
      device->destroy_resource_view(view);
      view = {0};
    }
  }
  for (auto& resource : fallback_uav_resources) {
    if (resource.handle != 0) {
      device->destroy_resource(resource);
      resource = {0};
    }
  }
  if (fallback_srv.handle != 0) {
    device->destroy_resource_view(fallback_srv);
    fallback_srv = {0};
  }
  if (fallback_srv_resource.handle != 0) {
    device->destroy_resource(fallback_srv_resource);
    fallback_srv_resource = {0};
  }
}

inline bool EnsureFallbackViews(reshade::api::device* device) {
  if (fallback_srv.handle != 0
      && std::ranges::all_of(fallback_uavs, [](const auto& view) { return view.handle != 0; })) {
    return true;
  }
  DestroyFallbackViews(device);

  const reshade::api::resource_desc srv_desc(
      1u, 1u, 1u, 1u,
      reshade::api::format::r16g16b16a16_float,
      1u,
      reshade::api::memory_heap::gpu_only,
      reshade::api::resource_usage::shader_resource);
  if (!device->create_resource(srv_desc, nullptr, srv_desc.usage, &fallback_srv_resource)
      || !device->create_resource_view(
          fallback_srv_resource,
          reshade::api::resource_usage::shader_resource,
          reshade::api::resource_view_desc(reshade::api::format::r16g16b16a16_float),
          &fallback_srv)) {
    DestroyFallbackViews(device);
    static std::atomic_flag reported = {};
    LogLightOnce(&reported, reshade::log::level::error, "failed to create placeholder input texture");
    return false;
  }

  const reshade::api::resource_desc uav_desc(
      1u, 1u, 1u, 1u,
      reshade::api::format::r16g16b16a16_float,
      1u,
      reshade::api::memory_heap::gpu_only,
      reshade::api::resource_usage::unordered_access);
  for (std::size_t index = 0; index < fallback_uavs.size(); ++index) {
    if (!device->create_resource(uav_desc, nullptr, uav_desc.usage,
                                 &fallback_uav_resources[index])
        || !device->create_resource_view(
            fallback_uav_resources[index],
            reshade::api::resource_usage::unordered_access,
            reshade::api::resource_view_desc(reshade::api::format::r16g16b16a16_float),
            &fallback_uavs[index])) {
      DestroyFallbackViews(device);
      static std::atomic_flag reported = {};
      LogLightOnce(&reported, reshade::log::level::error, "failed to create placeholder output texture; index=", index);
      return false;
    }
  }
  return true;
}

inline reshade::api::resource_view GetFallbackView(reshade::api::command_list* cmd_list) {
  if (!EnsureFallbackViews(cmd_list->get_device())) return {0};
  return fallback_srv;
}

inline reshade::api::resource_view GetFallbackUav(reshade::api::command_list* cmd_list,
                                                  std::size_t index) {
  if (!EnsureFallbackViews(cmd_list->get_device())) return {0};
  return fallback_uavs[index % fallback_uavs.size()];
}

// Bind every input and output for this pass. ApplyLightPasses restores game state
// after the complete lighting chain, not between individual dispatches.
inline void DispatchLightPass(reshade::api::command_list* cmd_list, LightPass* pass,
                              const ShaderInjectData& constants,
                              const std::array<reshade::api::resource_view, 9>& sources,
                              reshade::api::resource_view target,
                              reshade::api::resource_view second_target,
                              reshade::api::resource_view third_target,
                              reshade::api::resource_view fourth_target,
                              uint32_t width, uint32_t height) {
  cmd_list->bind_pipeline(reshade::api::pipeline_stage::all_compute, pass->pipeline);
  const std::array<reshade::api::resource_view, 4> targets = {
      target,  // u0
      second_target,
      third_target,
      fourth_target,
  };
  cmd_list->push_descriptors(
      reshade::api::shader_stage::all_compute, light_pass_layout, 1u,
      {
          .table = {0u},
          .binding = 0u,
          .array_offset = 0u,
          .count = 4u,
          .type = reshade::api::descriptor_type::texture_unordered_access_view,
          .descriptors = targets.data(),
      });
  cmd_list->push_descriptors(
      reshade::api::shader_stage::all_compute, light_pass_layout, 0u,
      {
          .table = {0u},
          .binding = 0u,
          .array_offset = 0u,
          .count = static_cast<uint32_t>(sources.size()),
          .type = reshade::api::descriptor_type::texture_shader_resource_view,
          .descriptors = sources.data(),
      });
  cmd_list->push_constants(
      reshade::api::shader_stage::all_compute, light_pass_layout, 2u, 0u,
      static_cast<uint32_t>(sizeof(constants) / sizeof(uint32_t)), &constants);
  cmd_list->dispatch((width + 7u) / 8u, (height + 7u) / 8u, 1u);
}

}  // namespace mgsv::lighting

// Ambient occlusion resources, passes, and settings.
namespace mgsv::ao {

using lighting::DestroyLightPass;
using lighting::DestroyLightTarget;
using lighting::DispatchLightPass;
using lighting::EnsureFallbackViews;
using lighting::EnsureLightPass;
using lighting::EnsureLightTarget;
using lighting::GetFallbackUav;
using lighting::GetFallbackView;
using lighting::LightPass;
using lighting::LightTarget;
using lighting::LogLight;
using lighting::LogLightOnce;

inline float depth_mips_enabled = 0.f;
inline LightTarget visibility_target;
inline std::array<LightTarget, 2> temporal_targets;
inline std::array<LightTarget, 2> filter_targets;
inline uint32_t temporal_status = UINT32_MAX;
inline uint32_t temporal_status_reports = 0u;

// The AO march's depth mip chain. Every level holds the nearest of the four texels
// below it, so a distant step can fetch one texel that covers the footprint its
// spacing would otherwise skip over.
constexpr uint32_t DEPTH_MIP_LEVELS = 4;

struct DepthMipChain {
  reshade::api::resource resource = {0};
  reshade::api::resource_view view = {0};
  std::array<reshade::api::resource_view, DEPTH_MIP_LEVELS> level_views = {};
  std::array<reshade::api::resource_view, DEPTH_MIP_LEVELS> level_source_views = {};
  reshade::api::pipeline_layout layout = {0};
  reshade::api::pipeline pipeline = {0};
  uint32_t width = 0;
  uint32_t height = 0;
  bool ready = false;
};

inline DepthMipChain depth_mip_chain;

inline void DestroyDepthMipChain(reshade::api::device* device) {
  depth_mip_chain.ready = false;
  for (auto& view : depth_mip_chain.level_views) {
    if (view.handle != 0) {
      device->destroy_resource_view(view);
      view = {0};
    }
  }
  for (auto& view : depth_mip_chain.level_source_views) {
    if (view.handle != 0) {
      device->destroy_resource_view(view);
      view = {0};
    }
  }
  if (depth_mip_chain.view.handle != 0) {
    device->destroy_resource_view(depth_mip_chain.view);
    depth_mip_chain.view = {0};
  }
  if (depth_mip_chain.resource.handle != 0) {
    device->destroy_resource(depth_mip_chain.resource);
    depth_mip_chain.resource = {0};
  }
  if (depth_mip_chain.pipeline.handle != 0) {
    device->destroy_pipeline(depth_mip_chain.pipeline);
    depth_mip_chain.pipeline = {0};
  }
  if (depth_mip_chain.layout.handle != 0) {
    device->destroy_pipeline_layout(depth_mip_chain.layout);
    depth_mip_chain.layout = {0};
  }
  depth_mip_chain.width = 0;
  depth_mip_chain.height = 0;
}

inline bool EnsureDepthMipChain(reshade::api::device* device, uint32_t width, uint32_t height) {
  if (depth_mip_chain.ready
      && depth_mip_chain.width == width
      && depth_mip_chain.height == height) {
    return true;
  }
  DestroyDepthMipChain(device);

  const reshade::api::resource_desc desc(
      std::max(width / 2u, 1u), std::max(height / 2u, 1u), 1u, DEPTH_MIP_LEVELS,
      reshade::api::format::r32_float,
      1u,
      reshade::api::memory_heap::gpu_only,
      reshade::api::resource_usage::shader_resource
          | reshade::api::resource_usage::unordered_access
          | reshade::api::resource_usage::copy_source
          | reshade::api::resource_usage::copy_dest);
  if (!device->create_resource(desc, nullptr, desc.usage, &depth_mip_chain.resource)) {
    static std::atomic_flag reported = {};
    LogLightOnce(&reported, reshade::log::level::error, "AO depth mip texture allocation failed; source_size=", width, "x", height);
    return false;
  }
  if (!device->create_resource_view(
          depth_mip_chain.resource,
          reshade::api::resource_usage::shader_resource,
          reshade::api::resource_view_desc(reshade::api::format::r32_float),
          &depth_mip_chain.view)) {
    static std::atomic_flag reported = {};
    LogLightOnce(&reported, reshade::log::level::error, "AO depth mip input view creation failed");
    return false;
  }
  for (uint32_t level = 0; level < DEPTH_MIP_LEVELS; ++level) {
    const reshade::api::resource_view_desc level_desc(
        reshade::api::format::r32_float, level, 1u, 0u, 1u);
    if (!device->create_resource_view(
            depth_mip_chain.resource,
            reshade::api::resource_usage::unordered_access,
            level_desc,
            &depth_mip_chain.level_views[level])) {
      static std::atomic_flag reported = {};
      LogLightOnce(&reported, reshade::log::level::error, "AO depth mip output view creation failed; level=", level);
      return false;
    }
    if (!device->create_resource_view(
            depth_mip_chain.resource,
            reshade::api::resource_usage::shader_resource,
            level_desc,
            &depth_mip_chain.level_source_views[level])) {
      static std::atomic_flag reported = {};
      LogLightOnce(&reported, reshade::log::level::error, "AO depth mip level input view creation failed; level=", level);
      return false;
    }
  }

  std::array<reshade::api::pipeline_layout_param, 2> params = {};
  params[0].type = reshade::api::pipeline_layout_param_type::push_descriptors;
  params[0].push_descriptors.count = 1u;
  params[0].push_descriptors.type = reshade::api::descriptor_type::shader_resource_view;
  params[0].push_descriptors.dx_register_index = 0u;
  params[0].push_descriptors.dx_register_space = 0u;
  params[1].type = reshade::api::pipeline_layout_param_type::push_descriptors;
  params[1].push_descriptors.count = 1u;
  params[1].push_descriptors.type = reshade::api::descriptor_type::unordered_access_view;
  params[1].push_descriptors.dx_register_index = 0u;
  params[1].push_descriptors.dx_register_space = 0u;
  if (!device->create_pipeline_layout(
          static_cast<uint32_t>(params.size()), params.data(), &depth_mip_chain.layout)) {
    static std::atomic_flag reported = {};
    LogLightOnce(&reported, reshade::log::level::error, "AO depth mip pipeline layout creation failed");
    return false;
  }

  reshade::api::shader_desc shader_description = {
      .code = __gtvbgi_depth_mips.data(),
      .code_size = __gtvbgi_depth_mips.size(),
  };
  const reshade::api::pipeline_subobject subobject = {
      .type = reshade::api::pipeline_subobject_type::compute_shader,
      .count = 1,
      .data = &shader_description,
  };
  if (!device->create_pipeline(depth_mip_chain.layout, 1, &subobject, &depth_mip_chain.pipeline)) {
    static std::atomic_flag reported = {};
    LogLightOnce(&reported, reshade::log::level::error, "AO depth mip pipeline creation failed");
    return false;
  }

  depth_mip_chain.width = width;
  depth_mip_chain.height = height;
  depth_mip_chain.ready = true;
  LogLight(reshade::log::level::info, "created AO depth mip chain; source_size=", width, "x", height,
           " levels=", DEPTH_MIP_LEVELS);
  return true;
}

// Builds the chain before the shared trace when its depth-mip control is enabled.
inline bool BuildDepthMips(reshade::api::command_list* cmd_list, reshade::api::resource_view depth_view, bool enabled) {
  if (!enabled) return false;

  auto* device = cmd_list->get_device();
  if (depth_view.handle == 0) return false;

  const auto depth_desc = device->get_resource_desc(device->get_resource_from_view(depth_view));
  if (depth_desc.type != reshade::api::resource_type::texture_2d) return false;
  const uint32_t width = depth_desc.texture.width;
  const uint32_t height = depth_desc.texture.height;
  if (width < 2u || height < 2u) return false;
  // Four levels require a base mip with a largest dimension of at least eight.
  if (std::max(width, height) < (1u << DEPTH_MIP_LEVELS)) return false;

  if (!EnsureDepthMipChain(device, width, height)) return false;

  cmd_list->bind_pipeline(reshade::api::pipeline_stage::all_compute, depth_mip_chain.pipeline);
  for (uint32_t level = 0; level < DEPTH_MIP_LEVELS; ++level) {
    const reshade::api::resource_view source =
        level == 0 ? depth_view : depth_mip_chain.level_source_views[level - 1];
    const reshade::api::resource_view target = depth_mip_chain.level_views[level];
    const std::array<reshade::api::descriptor_table_update, 2> updates = {{
        {
            .table = {},
            .binding = 0u,
            .array_offset = 0u,
            .count = 1u,
            .type = reshade::api::descriptor_type::shader_resource_view,
            .descriptors = &source,
        },
        {
            .table = {},
            .binding = 0u,
            .array_offset = 0u,
            .count = 1u,
            .type = reshade::api::descriptor_type::unordered_access_view,
            .descriptors = &target,
        },
    }};
    cmd_list->push_descriptors(
        reshade::api::shader_stage::all_compute, depth_mip_chain.layout, 1u, updates[1]);
    cmd_list->push_descriptors(
        reshade::api::shader_stage::all_compute, depth_mip_chain.layout, 0u, updates[0]);

    const uint32_t level_width = std::max(width >> (level + 1u), 1u);
    const uint32_t level_height = std::max(height >> (level + 1u), 1u);
    cmd_list->dispatch((level_width + 7u) / 8u, (level_height + 7u) / 8u, 1u);
  }

  return true;
}

inline void Destroy(reshade::api::device* device) {
  DestroyDepthMipChain(device);
  DestroyLightTarget(device, &visibility_target);
  for (auto& target : temporal_targets) {
    DestroyLightTarget(device, &target);
  }
  for (auto& target : filter_targets) {
    DestroyLightTarget(device, &target);
  }
  temporal_status = UINT32_MAX;
  temporal_status_reports = 0u;
}

}  // namespace mgsv::ao

namespace mgsv::il {

using lighting::DestroyLightPass;
using lighting::DestroyLightTarget;
using lighting::DispatchLightPass;
using lighting::EnsureFallbackViews;
using lighting::EnsureLightPass;
using lighting::EnsureLightTarget;
using lighting::GetFallbackUav;
using lighting::GetFallbackView;
using lighting::LightPass;
using lighting::LightTarget;
using lighting::LogLight;
using lighting::LogLightOnce;

inline LightPass gather_pass;
inline LightPass add_pass;
inline LightPass temporal_pass;
inline LightPass filter_pass;

inline LightTarget bounce_target;
inline LightTarget diagnostic_target;
inline std::array<LightTarget, 2> temporal_targets;
inline std::array<LightTarget, 2> geometry_targets;
inline std::array<LightTarget, 2> filter_targets;
inline std::array<lighting::temporal_inputs::Scene, 2> history_scenes;

// Updated only by the serialized immediate submission callback.
struct History {
  std::array<float, 12> parameters = {};
  std::array<float, 11> ao_parameters = {};
  std::array<float, 2> jitter_uv = {};
  uint64_t generation = 0u;
  uint64_t submission = 0u;
  uintptr_t depth_identity = 0u;
  uint32_t frame = 0u;
  uint32_t width = 0u;
  uint32_t height = 0u;
  uint32_t slot = 0u;
  bool valid = false;
  bool ao_valid = false;
};
inline History history;
inline uint32_t temporal_status = UINT32_MAX;
inline uint32_t temporal_status_reports = 0u;

// The trace and add passes receive the debug view through their effect-local injection copy.
inline float raw_view = 0.f;

inline void Destroy(reshade::api::device* device) {
  DestroyLightPass(device, &gather_pass);
  DestroyLightPass(device, &add_pass);
  DestroyLightPass(device, &temporal_pass);
  DestroyLightPass(device, &filter_pass);
  DestroyLightTarget(device, &bounce_target);
  DestroyLightTarget(device, &diagnostic_target);
  for (auto& target : temporal_targets) {
    DestroyLightTarget(device, &target);
  }
  for (auto& target : geometry_targets) {
    DestroyLightTarget(device, &target);
  }
  for (auto& target : filter_targets) {
    DestroyLightTarget(device, &target);
  }
  history_scenes = {};
  history = {};
  temporal_status = UINT32_MAX;
  temporal_status_reports = 0u;
}

}  // namespace mgsv::il

namespace mgsv::lighting {

// Passed shader storage must outlive the descriptors; assemble once per registry/storage pair.
inline void AppendSettings(renodx::utils::settings::Settings* registry, ShaderInjectData* shader_injection) {
  if (std::ranges::any_of(*registry, [](const auto* setting) {
        return setting->key == "FxIndirectLightingEnabled";
      })) return;
  const auto is_temporal_visible = [shader_injection] {
    return shader_injection->lighting_mode >= 0.5f && shader_injection->gt_temporal_enabled >= 0.5f;
  };
  const auto is_spatial_visible = [shader_injection] {
    return shader_injection->lighting_mode >= 0.5f && shader_injection->gt_spatial_enabled >= 0.5f;
  };
  renodx::utils::settings::Settings additions = {
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingEnabled",
          .binding = &shader_injection->lighting_mode,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 1.f,
          .label = "Screen-Space Lighting",
          .section = "Indirect Lighting",
          .tooltip = "Adds ambient occlusion and Global Illumination: contact shading and approximate bounced light from visible surfaces. Off restores native lighting.",
          .labels = {"Off", "GT-VBGI"},
      },
      new renodx::utils::settings::Setting{
          .key = "FxAOStrength",
          .binding = &shader_injection->ao_strength,
          .default_value = 75.f,
          .label = "Ambient Occlusion Strength",
          .section = "Indirect Lighting",
          .tooltip = "Controls how strongly the added shading darkens the scene. Values above 100 increase the darkening.",
          .max = 200.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxGIStrength",
          .binding = &shader_injection->gi_strength,
          .default_value = 75.f,
          .label = "Global Illumination Strength",
          .section = "Indirect Lighting",
          .tooltip = "Controls how much bounced light is added to the scene.",
          .max = 200.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingResolution",
          .binding = &shader_injection->gt_resolution,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 2.f,
          .label = "Lighting Resolution",
          .section = "Indirect Lighting",
          .tooltip = "Scales both width and height of the AO/GI trace. Quarter uses 1/16 as many pixels; Eighth uses 1/64. Lower resolutions can lose thin geometry and contact detail. Search radii stay in full-resolution pixels.",
          .labels = {"Full", "Half", "Quarter", "Eighth"},
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingSearchRadiusPixels",
          .binding = &shader_injection->gt_search_radius_pixels,
          .default_value = 2000.f,
          .label = "Search Radius",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Search distance in full-resolution pixels for both AO and GI. Larger values reach farther visible sources and blockers but spread samples farther apart. Default 2000 pairs with 40 samples per side.",
          .min = 1.f,
          .max = 4096.f,
          .format = "%.0f px",
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingSurfaceThickness",
          .binding = &shader_injection->gt_surface_thickness,
          .default_value = 0.15f,
          .label = "Surface Thickness",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Assumed surface thickness in native view-depth units for both AO and GI. Higher values fill more directions but can overstate thin surfaces.",
          .min = 0.f,
          .max = 0.5f,
          .format = "%.4f",
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingSurfaceOffset",
          .binding = &shader_injection->gt_surface_offset,
          .default_value = 0.0009765625f,
          .label = "Surface Offset",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Offsets the AO and GI receiver along its shading normal in native view-space units to reduce self-occlusion. Higher values can lose contact detail.",
          .min = 0.f,
          .max = 0.01f,
          .format = "%.6f",
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingSliceCount",
          .binding = &shader_injection->gt_slice_count,
          .default_value = 3.f,
          .label = "Sampling Directions",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Search lines through each pixel, sampled on both sides and shared by AO and Global Illumination. More lines improve coverage but cost more GPU time.",
          .min = 1.f,
          .max = 8.f,
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingSamplesPerSide",
          .binding = &shader_injection->gt_samples_per_side,
          .default_value = 40.f,
          .label = "Samples per Side",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Samples on each side of every sampling direction, shared by AO and Global Illumination. Higher values find smaller surfaces but cost more GPU time.",
          .min = 1.f,
          .max = 64.f,
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingDepthMipsEnabled",
          .binding = &ao::depth_mips_enabled,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 0.f,
          .label = "Coarse Depth Sampling",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Uses reduced depth for distant AO and GI samples to catch objects between steps. GI locates the corresponding source pixel before reading its light. Off uses full-resolution depth as in the reference.",
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxGISourceBackfaceRejectionEnabled",
          .binding = &shader_injection->gi_reject_backfaces,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 0.f,
          .label = "Reject Backfacing GI Sources",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Rejects bounced light from source surfaces facing away from the receiver. These surfaces still block farther sources. Off matches the reference default.",
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxGISourceSpecularWeight",
          .binding = &shader_injection->gi_source_specular_weight,
          .default_value = 0.f,
          .label = "Reflections in GI Source",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Scales captured specular light in the bounce source. The default zero uses diffuse outgoing light as in the reference. Specular light depends on the camera view and can produce bright artifacts.",
          .max = 100.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingTemporalAccumulationEnabled",
          .binding = &shader_injection->gt_temporal_enabled,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 1.f,
          .label = "Temporal Accumulation",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Combines matching samples from earlier frames before spatial filtering. Missing camera, motion, or submission data uses the current sample.",
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxAOHistoryWeight",
          .binding = &shader_injection->ao_history_weight,
          .default_value = 90.f,
          .label = "AO History Weight",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Maximum percentage of accepted history. Higher values reduce noise but can retain changes longer. Zero uses only the current sample.",
          .max = 99.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = is_temporal_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxAODepthRejection",
          .binding = &shader_injection->ao_depth_rejection,
          .default_value = 20.f,
          .label = "AO Depth Rejection",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Allowed history depth difference as a percentage of the expected previous depth. Lower values reject mismatched surfaces more strongly.",
          .max = 20.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = is_temporal_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxAONormalRejection",
          .binding = &shader_injection->ao_normal_rejection,
          .default_value = 0.f,
          .label = "AO Normal Rejection",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Surface-direction agreement required for full history weight. The weight fades to zero over a 0.4-wide range below this value.",
          .max = 1.f,
          .format = "%.2f",
          .is_visible = is_temporal_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxAOHistoryClamp",
          .binding = &shader_injection->ao_history_clamp,
          .default_value = 0.1f,
          .label = "AO History Clamp",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Allowed history range in standard deviations of nearby current samples. Lower values react faster; zero disables clamping.",
          .max = 4.f,
          .format = "%.2f",
          .is_visible = is_temporal_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxGIHistoryWeight",
          .binding = &shader_injection->gi_history_weight,
          .default_value = 90.f,
          .label = "GI History Weight",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Maximum percentage of accepted history. Higher values reduce noise but can retain changes longer. Zero uses only the current sample.",
          .max = 99.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = is_temporal_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxGIDepthRejection",
          .binding = &shader_injection->gi_depth_rejection,
          .default_value = 5.f,
          .label = "GI Depth Rejection",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Allowed history depth difference as a percentage of the expected previous depth. Lower values reject mismatched surfaces more strongly.",
          .max = 20.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = is_temporal_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxGINormalRejection",
          .binding = &shader_injection->gi_normal_rejection,
          .default_value = 0.9f,
          .label = "GI Normal Rejection",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Surface-direction agreement required for full history weight. The weight fades to zero over a 0.4-wide range below this value.",
          .max = 1.f,
          .format = "%.2f",
          .is_visible = is_temporal_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxGIHistoryClamp",
          .binding = &shader_injection->gi_history_clamp,
          .default_value = 1.f,
          .label = "GI History Clamp",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Allowed history range in standard deviations of nearby current samples. Lower values react faster; zero disables clamping.",
          .max = 4.f,
          .format = "%.2f",
          .is_visible = is_temporal_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingSpatialFilteringEnabled",
          .binding = &shader_injection->gt_spatial_enabled,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 1.f,
          .label = "Spatial Filtering",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Smooths the result after temporal accumulation while preserving depth and surface-direction boundaries. Off passes the temporal result through the same shader.",
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingFilterType",
          .binding = &shader_injection->gt_filter_type,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 1.f,
          .label = "Filter Type",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Separable filters horizontally then vertically. A-Trous uses a two-dimensional kernel whose spacing can grow between passes.",
          .labels = {"Separable", "A-Trous"},
          .is_visible = is_spatial_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingFilterRadius",
          .binding = &shader_injection->gt_filter_radius,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 8.f,
          .label = "Filter Radius",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Controls Gaussian width in Separable. In A-Trous it limits spacing after the first pass; with one pass, Taps controls the footprint and Radius has no effect. Units are lighting pixels.",
          .min = 1.f,
          .max = 8.f,
          .format = "%d",
          .is_enabled = [shader_injection] { return shader_injection->gt_filter_type < 0.5f || shader_injection->gt_filter_passes > 1.f; },
          .is_visible = is_spatial_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingFilterPasses",
          .binding = &shader_injection->gt_filter_passes,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 3.f,
          .label = "Filter Passes",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Filtering iterations. Each A-Trous iteration uses one dispatch; each Separable iteration uses horizontal and vertical dispatches.",
          .min = 1.f,
          .max = 4.f,
          .format = "%d",
          .is_visible = is_spatial_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingFilterTapsSeparable",
          .binding = &shader_injection->gt_filter_taps_separable,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 5.f,
          .label = "Separable Taps",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Samples per horizontal or vertical pass. Even values round up to the next odd count. More taps cost more texture reads.",
          .min = 3.f,
          .max = 17.f,
          .format = "%d",
          .is_enabled = [shader_injection] { return shader_injection->gt_filter_type < 0.5f; },
          .is_visible = is_spatial_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingFilterTapsAtrous",
          .binding = &shader_injection->gt_filter_taps_atrous,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 5.f,
          .label = "A-Trous Taps",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Samples per axis of the two-dimensional kernel. Five means 25 taps per iteration. Even values round up to the next odd count.",
          .min = 3.f,
          .max = 9.f,
          .format = "%d",
          .is_enabled = [shader_injection] { return shader_injection->gt_filter_type >= 0.5f; },
          .is_visible = is_spatial_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingDebugView",
          .binding = &il::raw_view,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 0.f,
          .label = "Lighting Debug View",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Replaces diffuse lighting to inspect bounced light, sector coverage, accepted source directions, valid receivers, or source radiance. Surface color, specular, fog, and later processing still apply.",
          .labels = {"Off", "Bounce Lighting", "Geometry Coverage", "Accepted Source Coverage", "Valid / Invalid Results", "Source Lighting", "Temporal GI", "Filtered GI", "Accepted GI History", "Raw AO", "Temporal AO", "Filtered AO", "Accepted AO History"},
          .is_visible = [shader_injection] {
            return shader_injection->lighting_mode >= 0.5f;
          },
      },
  };
  for (auto* setting : additions) {
    if (setting->value_type != renodx::utils::settings::SettingValueType::BOOLEAN) continue;
    setting->on_change_value = [setting](float, float current) {
      const char* state = "Off";
      if (current >= 0.5f) {
        state = (setting->key == "FxIndirectLightingEnabled" ? "GT-VBGI" : "On");
      }
      LogLight(reshade::log::level::info, setting->label, " = ", state, " (", setting->key, ")");
    };
  }
  registry->insert(std::ranges::find_if(*registry, [](const auto* setting) {
                     return setting->section == "Options";
                   }),
                   additions.begin(), additions.end());
}

}  // namespace mgsv::lighting

namespace mgsv::ao {

inline void Apply(const lighting::LightFrame& frame, reshade::api::command_list* cmd_list,
                  ShaderInjectData constants, const lighting::temporal_inputs::Snapshot& temporal,
                  bool history_allowed) {
  auto* device = cmd_list->get_device();
  uint32_t scale = 1u;
  if (constants.gt_resolution > 2.5f) {
    scale = 8u;
  } else if (constants.gt_resolution > 1.5f) {
    scale = 4u;
  } else if (constants.gt_resolution > 0.5f) {
    scale = 2u;
  }
  constants.gt_resolution_divisor = static_cast<float>(scale);

  constants.gt_noise_frame = static_cast<float>(lighting::noise_frame.load(std::memory_order_relaxed) & 31u);
  const uint32_t present = lighting::noise_frame.load(std::memory_order_relaxed);
  const bool new_submission = history_allowed && temporal.valid
                              && (temporal.submission != il::history.submission
                                  || temporal.generation != il::history.generation);
  constants.gi_temporal_active = (new_submission && constants.gt_temporal_enabled >= 0.5f
                                          && constants.gi_history_weight > 0.f
                                      ? 1.f
                                      : 0.f);
  constants.gi_history_valid = 0.f;
  constants.ao_temporal_active = (new_submission && constants.gt_temporal_enabled >= 0.5f
                                          && constants.ao_history_weight > 0.f
                                      ? 1.f
                                      : 0.f);
  constants.ao_history_valid = 0.f;
  constants.gt_current_jitter_x = temporal.current_jitter_uv[0];
  constants.gt_current_jitter_y = temporal.current_jitter_uv[1];
  constants.gt_previous_jitter_x = il::history.jitter_uv[0];
  constants.gt_previous_jitter_y = il::history.jitter_uv[1];
  constants.gt_velocity_jitter_scale = temporal.velocity_projection_jitter_scale;
  if (lighting::noise_view.handle == 0 && !lighting::noise_creation_failed) {
    static_assert(FAST_NOISE_RG8.size() == 128u * 128u * 32u * 2u);
    const reshade::api::resource_desc noise_desc(
        128u, 128u, 32u, 1u, reshade::api::format::r8g8_unorm, 1u,
        reshade::api::memory_heap::gpu_only, reshade::api::resource_usage::shader_resource);
    std::array<reshade::api::subresource_data, 32> initial_data = {};
    for (uint32_t slice = 0; slice < initial_data.size(); ++slice) {
      // ReShade's upload API takes a mutable pointer but does not modify input data.
      initial_data[slice] = {.data = const_cast<uint8_t*>(FAST_NOISE_RG8.data() + (slice * 128u * 128u * 2u)),
                             .row_pitch = 256u,
                             .slice_pitch = 128u * 128u * 2u};
    }
    if (device->create_resource(noise_desc, initial_data.data(), noise_desc.usage, &lighting::noise_resource)
        && device->create_resource_view(lighting::noise_resource, reshade::api::resource_usage::shader_resource,
                                        reshade::api::resource_view_desc(reshade::api::resource_view_type::texture_2d_array,
                                                                         reshade::api::format::r8g8_unorm, 0u, 1u, 0u, 32u),
                                        &lighting::noise_view)) {
      LogLight(reshade::log::level::info, "IS-FAST noise created; 128x128x32 RG8_UNORM");
    } else {
      if (lighting::noise_resource.handle != 0) {
        device->destroy_resource(lighting::noise_resource);
        lighting::noise_resource = {0};
      }
      static std::atomic_flag failed = {};
      lighting::noise_creation_failed = true;
      LogLightOnce(&failed, reshade::log::level::warning, "IS-FAST noise unavailable; skipping custom AO/GI until device recreation");
    }
  }
  if (lighting::noise_view.handle == 0) return;
  const uint32_t width = (frame.width + scale - 1u) / scale;
  const uint32_t height = (frame.height + scale - 1u) / scale;

  if (!EnsureLightTarget(device, &visibility_target, width, height, reshade::api::format::r16g16_float)
      || !EnsureLightTarget(device, &il::bounce_target, width, height)
      || !EnsureLightPass(device, &il::gather_pass, "mgsv: GT-VBGI trace created", __gtvbgi)
      || !EnsureLightPass(device, &il::add_pass, "mgsv: GT-VBGI application created", __gtvbgi_add)
      || !EnsureFallbackViews(device)) {
    static std::atomic_flag failed = {};
    LogLightOnce(&failed, reshade::log::level::warning,
                 "GT-VBGI setup failed; skipping custom lighting; effect=", width, "x", height);
    return;
  }

  if (constants.gi_debug_view >= 2.f
      && !EnsureLightTarget(device, &il::diagnostic_target, width, height)) {
    constants.gi_debug_view = 0.f;
    static std::atomic_flag failed = {};
    LogLightOnce(&failed, reshade::log::level::warning,
                 "GI diagnostic allocation failed; normal AO and GI remain enabled");
  }

  const bool depth_mips_requested = constants.gt_depth_mips_enabled != 0.f;
  const bool depth_mips_ready = BuildDepthMips(cmd_list, frame.depth, depth_mips_requested);
  constants.gt_depth_mips_enabled = (depth_mips_ready ? 1.f : 0.f);
  const auto scratch0 = GetFallbackUav(cmd_list, 0);
  const auto scratch1 = GetFallbackUav(cmd_list, 1);
  const auto scratch2 = GetFallbackUav(cmd_list, 2);
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  const bool immediate = context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;
  const std::array<float, 12> parameters = {
      constants.gt_search_radius_pixels,
      constants.gt_surface_thickness,
      constants.gt_surface_offset,
      constants.gt_slice_count,
      constants.gt_samples_per_side,
      constants.gt_depth_mips_enabled,
      constants.gi_reject_backfaces,
      constants.gi_source_specular_weight,
      constants.gt_resolution,
      constants.gt_temporal_enabled,
      constants.custom_taa,
      constants.unclamp_motion_vectors,
  };
  const std::array<float, 11> ao_parameters = {
      constants.gt_search_radius_pixels,
      constants.gt_surface_thickness,
      constants.gt_surface_offset,
      constants.gt_slice_count,
      constants.gt_samples_per_side,
      constants.gt_depth_mips_enabled,
      constants.gt_resolution,
      constants.ao_strength,
      constants.gt_temporal_enabled,
      constants.custom_taa,
      constants.unclamp_motion_vectors,
  };
  const bool same_camera = il::history_scenes[il::history.slot].camera_generation == temporal.scene.camera_generation
                           && il::history_scenes[il::history.slot].camera_identity == temporal.scene.camera_identity
                           && il::history_scenes[il::history.slot].viewport_identity == temporal.scene.viewport_identity;
  const bool geometry_changed = il::history.width != width || il::history.height != height
                                || il::history.generation != temporal.generation
                                || il::history.depth_identity != temporal.scene.depth_identity || !same_camera;
  if (immediate && (geometry_changed || il::history.parameters != parameters)) {
    il::history.valid = false;
  }
  if (immediate && (geometry_changed || il::history.ao_parameters != ao_parameters)) {
    il::history.ao_valid = false;
  }
  const uint32_t write_slot = 1u - il::history.slot;
  // An unsplit deferred fallback must never write submitted history textures.
  bool temporal_ready = immediate && EnsureLightPass(device, &il::temporal_pass, "mgsv: combined AO/GI temporal pass created", __gtvbgi_temporal);
  for (uint32_t index = 0u; index < 2u && temporal_ready; ++index) {
    temporal_ready = EnsureLightTarget(device, &il::temporal_targets[index], width, height)
                     && EnsureLightTarget(device, &ao::temporal_targets[index], width, height, reshade::api::format::r16g16_float)
                     && EnsureLightTarget(device, &il::geometry_targets[index], width, height,
                                          reshade::api::format::r32g32_uint);
  }
  if (!temporal_ready) {
    constants.gi_temporal_active = 0.f;
    constants.ao_temporal_active = 0.f;
  }
  if (constants.gi_temporal_active >= 0.5f || constants.ao_temporal_active >= 0.5f) {
    D3D11_BUFFER_DESC description = {};
    temporal.scene.buffer->GetDesc(&description);
    D3D11_BUFFER_DESC previous = {};
    auto& scene = il::history_scenes[write_slot];
    if (scene.buffer != nullptr) scene.buffer->GetDesc(&previous);
    if (scene.buffer == nullptr || previous.ByteWidth != description.ByteWidth) {
      scene.buffer.Reset();
      description.Usage = D3D11_USAGE_DEFAULT;
      description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
      description.CPUAccessFlags = 0u;
      description.MiscFlags = 0u;
      description.StructureByteStride = 0u;
      auto* native_device = reinterpret_cast<ID3D11Device*>(device->get_native());  // NOLINT(performance-no-int-to-ptr)
      if (FAILED(native_device->CreateBuffer(&description, nullptr, &scene.buffer))) {
        constants.gi_temporal_active = 0.f;
        constants.ao_temporal_active = 0.f;
        static std::atomic_flag failed = {};
        LogLightOnce(&failed, reshade::log::level::warning, "Lighting camera history allocation failed; using current AO and GI");
      }
    }
  }
  if (!temporal_ready) {
    static std::atomic_flag failed = {};
    LogLightOnce(&failed, reshade::log::level::warning,
                 "Lighting temporal unavailable; immediate=", immediate,
                 " ready=", temporal_ready, "; using current samples");
  }
  constants.gi_history_valid = (constants.gi_temporal_active >= 0.5f && il::history.valid
                                        && present - il::history.frame == 1u
                                        && il::history_scenes[il::history.slot].buffer != nullptr
                                    ? 1.f
                                    : 0.f);
  constants.ao_history_valid = (constants.ao_temporal_active >= 0.5f && il::history.ao_valid
                                        && present - il::history.frame == 1u
                                        && il::history_scenes[il::history.slot].buffer != nullptr
                                    ? 1.f
                                    : 0.f);
  for (const bool ambient_occlusion : {true, false}) {
    uint32_t status = 0u;
    const bool requested = (ambient_occlusion
                                ? constants.gt_temporal_enabled >= 0.5f && constants.ao_history_weight > 0.f
                                : constants.gt_temporal_enabled >= 0.5f && constants.gi_history_weight > 0.f);
    const bool active = (ambient_occlusion ? constants.ao_temporal_active : constants.gi_temporal_active) >= 0.5f;
    const bool valid = (ambient_occlusion ? constants.ao_history_valid : constants.gi_history_valid) >= 0.5f;
    if (requested) {
      if (!immediate || !history_allowed) {
        status = 1u;
      } else if (!temporal.valid) {
        status = 2u;
      } else if (!active) {
        status = 3u;
      } else {
        status = (valid ? 5u : 4u);
      }
    }
    auto& previous_status = (ambient_occlusion ? ao::temporal_status : il::temporal_status);
    auto& reports = (ambient_occlusion ? ao::temporal_status_reports : il::temporal_status_reports);
    if (previous_status != status) {
      previous_status = status;
      if (reports++ < 32u) {
        static constexpr std::array<const char*, 6> STATUS_NAMES = {
            "disabled",
            "submission unavailable",
            "native inputs unavailable",
            "setup unavailable or duplicate producer",
            "seeding",
            "history submitted",
        };
        LogLight(reshade::log::level::info, (ambient_occlusion ? "AO temporal: " : "GI temporal: "), STATUS_NAMES[status],
                 " scene=", temporal.scene.buffer != nullptr,
                 " motion=", temporal.motion.velocity != nullptr,
                 " generation=", temporal.generation, " submission=", temporal.submission,
                 "; per-pixel acceptance is available in the corresponding Accepted History view");
      }
    }
  }
  bool filter_ready = EnsureLightPass(device, &il::filter_pass, "mgsv: combined AO/GI spatial filter created", __gtvbgi_filter);
  for (uint32_t index = 0u; index < 2u && filter_ready; ++index) {
    filter_ready = EnsureLightTarget(device, &il::filter_targets[index], width, height)
                   && EnsureLightTarget(device, &ao::filter_targets[index], width, height, reshade::api::format::r16g16_float);
  }
  if (!filter_ready) {
    static std::atomic_flag failed = {};
    LogLightOnce(&failed, reshade::log::level::warning, "Lighting spatial setup failed; using unfiltered AO and GI");
  }
  const std::array<reshade::api::resource_view, 9> sources = {
      frame.depth,
      frame.normal,
      (depth_mip_chain.ready ? depth_mip_chain.view : GetFallbackView(cmd_list)),
      frame.light_source,
      frame.albedo,
      frame.specular,
      lighting::noise_view,
      GetFallbackView(cmd_list),
      GetFallbackView(cmd_list),
  };
  DispatchLightPass(cmd_list, &il::gather_pass, constants, sources,
                    visibility_target.uav, il::bounce_target.uav,
                    constants.gi_debug_view >= 2.f ? il::diagnostic_target.uav : scratch1,
                    scratch2, width, height);

  auto ao_source = visibility_target.view;
  auto gi_source = il::bounce_target.view;
  if (temporal_ready) {
    const std::array<ID3D11Buffer*, 2> scenes = {
        temporal.scene.buffer.Get(),
        il::history_scenes[il::history.slot].buffer.Get(),
    };
    Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context1;
    if (SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&context1)))) {
      const std::array<UINT, 2> first = {
          temporal.scene.first_constant,
          il::history_scenes[il::history.slot].first_constant,
      };
      const std::array<UINT, 2> count = {
          temporal.scene.constant_count,
          il::history_scenes[il::history.slot].constant_count,
      };
      context1->CSSetConstantBuffers1(1u, 2u, scenes.data(), first.data(), count.data());
    } else {
      context->CSSetConstantBuffers(1u, 2u, scenes.data());
    }
    const std::array<reshade::api::resource_view, 9> temporal_sources = {
        gi_source,
        frame.depth,
        frame.normal,
        temporal.motion.velocity != nullptr
            ? reshade::api::resource_view{reinterpret_cast<uintptr_t>(temporal.motion.velocity.Get())}
            : GetFallbackView(cmd_list),
        temporal.motion.object_velocity != nullptr
            ? reshade::api::resource_view{reinterpret_cast<uintptr_t>(temporal.motion.object_velocity.Get())}
            : GetFallbackView(cmd_list),
        il::temporal_targets[il::history.slot].view,
        il::geometry_targets[il::history.slot].view,
        ao_source,
        ao::temporal_targets[il::history.slot].view,
    };
    DispatchLightPass(cmd_list, &il::temporal_pass, constants, temporal_sources,
                      il::temporal_targets[write_slot].uav, il::geometry_targets[write_slot].uav,
                      constants.gi_debug_view >= 2.f ? il::diagnostic_target.uav : scratch1,
                      ao::temporal_targets[write_slot].uav, width, height);
    gi_source = il::temporal_targets[write_slot].view;
    ao_source = ao::temporal_targets[write_slot].view;
    if (new_submission) {
      if (constants.gi_temporal_active >= 0.5f || constants.ao_temporal_active >= 0.5f) {
        // Retain the camera paired with both newly written histories after
        // their previous slot has been consumed on this GPU command stream.
        auto buffer = il::history_scenes[write_slot].buffer;
        il::history_scenes[write_slot] = temporal.scene;
        il::history_scenes[write_slot].buffer = std::move(buffer);
        context->CopyResource(il::history_scenes[write_slot].buffer.Get(), temporal.scene.buffer.Get());
      }
      il::history = {
          .parameters = parameters,
          .ao_parameters = ao_parameters,
          .jitter_uv = temporal.current_jitter_uv,
          .generation = temporal.generation,
          .submission = temporal.submission,
          .depth_identity = temporal.scene.depth_identity,
          .frame = present,
          .width = width,
          .height = height,
          .slot = write_slot,
          .valid = constants.gi_temporal_active >= 0.5f,
          .ao_valid = constants.ao_temporal_active >= 0.5f,
      };
    }
  }
  if (filter_ready) {
    const bool separable = constants.gt_filter_type < 0.5f;
    const uint32_t iterations = (std::isfinite(constants.gt_filter_passes)
                                     ? static_cast<uint32_t>(std::clamp(constants.gt_filter_passes, 1.f, 4.f))
                                     : 1u);
    const uint32_t radius = (std::isfinite(constants.gt_filter_radius)
                                 ? static_cast<uint32_t>(std::clamp(constants.gt_filter_radius, 1.f, 8.f))
                                 : 2u);
    const uint32_t pass_count = iterations * (separable ? 2u : 1u);
    for (uint32_t index = 0u; index < pass_count; ++index) {
      constants.gt_filter_step = (separable ? 1.f : static_cast<float>(std::min(1u << index, std::max(1u, radius / 2u))));
      constants.gt_filter_direction = (separable ? static_cast<float>(index & 1u) : 2.f);
      const std::array<reshade::api::resource_view, 9> filter_sources = {
          gi_source,
          frame.depth,
          frame.normal,
          ao_source,
          GetFallbackView(cmd_list),
          GetFallbackView(cmd_list),
          GetFallbackView(cmd_list),
          GetFallbackView(cmd_list),
          GetFallbackView(cmd_list),
      };
      DispatchLightPass(cmd_list, &il::filter_pass, constants, filter_sources,
                        il::filter_targets[index & 1u].uav, ao::filter_targets[index & 1u].uav,
                        constants.gi_debug_view == 11.f ? il::diagnostic_target.uav : scratch1,
                        scratch2, width, height);
      gi_source = il::filter_targets[index & 1u].view;
      ao_source = ao::filter_targets[index & 1u].view;
    }
  }
  const std::array<reshade::api::resource_view, 9> apply_sources = {
      gi_source,
      frame.depth,
      constants.gi_debug_view >= 2.f ? il::diagnostic_target.view : GetFallbackView(cmd_list),
      ao_source,
      GetFallbackView(cmd_list),
      GetFallbackView(cmd_list),
      GetFallbackView(cmd_list),
      GetFallbackView(cmd_list),
      GetFallbackView(cmd_list),
  };
  // Resolve both signals together: AO attenuates only existing diffuse, and
  // the visibility-integrated bounce is added afterward in the same store.
  DispatchLightPass(cmd_list, &il::add_pass, constants, apply_sources, frame.light,
                    scratch0, scratch1, scratch2, frame.width, frame.height);
  static std::atomic_flag recorded = {};
  LogLightOnce(&recorded, reshade::log::level::info,
               "GT-VBGI commands recorded; effect=", width, "x", height,
               " noise=IS-FAST",
               " full_resolution=", frame.width, "x", frame.height,
               " scale=", constants.gt_resolution_divisor,
               " slices=", constants.gt_slice_count,
               " normal_bias=", constants.gt_surface_offset,
               " radius_px=", constants.gt_search_radius_pixels,
               " steps_per_side=", constants.gt_samples_per_side,
               " thickness=", constants.gt_surface_thickness,
               " ao_strength=", constants.ao_strength,
               " gi_backface=", constants.gi_reject_backfaces,
               " gi_specular=", constants.gi_source_specular_weight,
               " gi_strength=", constants.gi_strength,
               " gt_depth_mips_enabled=", constants.gt_depth_mips_enabled,
               " gi_debug=", constants.gi_debug_view,
               " temporal=", constants.gt_temporal_enabled,
               " spatial=", constants.gt_spatial_enabled,
               " filter_type=", constants.gt_filter_type,
               " filter_passes=", constants.gt_filter_passes,
               "; command submission is not GPU completion");

  if (depth_mips_requested && !depth_mips_ready) {
    static std::atomic_flag mip_failed = {};
    LogLightOnce(&mip_failed, reshade::log::level::warning,
                 "shared depth mip generation unavailable; AO/GI use full-resolution depth samples");
  }
}

}  // namespace mgsv::ao

// Native composite inputs and post-scatter lighting orchestration.
namespace mgsv::lighting {

// Called immediately after the game's scatter dispatch is recorded, not after GPU completion.
// Dispatch leaves its bindings unchanged, so read them here before recording our own passes.
inline void ApplyLightPasses(reshade::api::command_list* cmd_list, const ShaderInjectData& shader_injection) {
  if (cmd_list == nullptr || cmd_list->get_device()->get_api() != reshade::api::device_api::d3d11) return;

  ShaderInjectData constants;
  {
    const std::shared_lock lock(renodx::utils::mutex::global_mutex);
    constants = shader_injection;
    constants.gi_debug_view = std::clamp(il::raw_view, 0.f, 12.f);
    constants.gt_depth_mips_enabled = ao::depth_mips_enabled;
  }
  if (constants.lighting_mode < 0.5f) {
    static std::atomic_flag reported = {};
    mgsv::lighting::LogLightOnce(&reported, reshade::log::level::info, "scatter hook reached; Screen-Space Lighting is Off");
    return;
  }

  // Allocation and teardown share the first-present device contract. No lighting work is admitted during resize.
  const auto shared_inputs = inputs::Read(cmd_list->get_device());
  if (shared_inputs.device == nullptr) return;

  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (context == nullptr) return;

  // Keep these views alive until this callback returns, even if later bindings replace them.
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normal;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> light_source;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> light;
  context->CSGetShaderResources(1u, 1u, depth.GetAddressOf());
  context->CSGetShaderResources(2u, 1u, normal.GetAddressOf());
  context->CSGetUnorderedAccessViews(0u, 1u, light.GetAddressOf());
  if (depth == nullptr || normal == nullptr || light == nullptr) {
    static std::atomic_flag reported = {};
    mgsv::lighting::LogLightOnce(&reported, reshade::log::level::warning, "skipping lighting: missing scatter bindings depth=",
                                 depth != nullptr, " normal=", normal != nullptr, " light=", light != nullptr);
    return;
  }

  D3D11_UNORDERED_ACCESS_VIEW_DESC light_view_desc = {};
  light->GetDesc(&light_view_desc);
  if (light_view_desc.ViewDimension != D3D11_UAV_DIMENSION_TEXTURE2D
      || light_view_desc.Texture2D.MipSlice != 0u) {
    static std::atomic_flag reported = {};
    mgsv::lighting::LogLightOnce(&reported, reshade::log::level::warning,
                                 "skipping lighting: light output must be a 2D view of mip 0; dimension=",
                                 static_cast<uint32_t>(light_view_desc.ViewDimension));
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11Resource> light_resource;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> light_texture;
  light->GetResource(light_resource.GetAddressOf());
  if (FAILED(light_resource.As(&light_texture))) {
    static std::atomic_flag reported = {};
    mgsv::lighting::LogLightOnce(&reported, reshade::log::level::warning,
                                 "skipping lighting: light output is not a 2D texture");
    return;
  }

  D3D11_TEXTURE2D_DESC light_desc = {};
  light_texture->GetDesc(&light_desc);
  if (light_desc.Width == 0u || light_desc.Height == 0u
      || light_desc.ArraySize != 1u || light_desc.SampleDesc.Count != 1u) {
    static std::atomic_flag reported = {};
    mgsv::lighting::LogLightOnce(&reported, reshade::log::level::warning,
                                 "skipping lighting: unsupported light output size=", light_desc.Width, "x", light_desc.Height,
                                 " array_size=", light_desc.ArraySize, " samples=", light_desc.SampleDesc.Count);
    return;
  }

  const auto input_error = [&](ID3D11ShaderResourceView* view, DXGI_FORMAT expected_format = DXGI_FORMAT_UNKNOWN) -> const char* {
    if (view == nullptr) return "missing view";
    D3D11_SHADER_RESOURCE_VIEW_DESC view_desc = {};
    view->GetDesc(&view_desc);
    if (view_desc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D) return "view dimension";
    if (view_desc.Texture2D.MostDetailedMip != 0u) return "first mip";
    if (expected_format != DXGI_FORMAT_UNKNOWN && view_desc.Format != expected_format) return "view format";

    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    view->GetResource(resource.GetAddressOf());
    if (FAILED(resource.As(&texture))) return "resource type";
    if (expected_format != DXGI_FORMAT_UNKNOWN) {
      if (resource.Get() == light_resource.Get()) return "output alias";
      if (view_desc.Texture2D.MipLevels != 1u) return "mip count";
      Microsoft::WRL::ComPtr<ID3D11Device> input_device;
      // ReShade redirects texture GetDevice to its proxy. Views retain the
      // native owner, matching inputs::ReadTexture and device::get_native.
      view->GetDevice(input_device.GetAddressOf());
      if (input_device.Get() != reinterpret_cast<ID3D11Device*>(cmd_list->get_device()->get_native())) return "device";
    }

    D3D11_TEXTURE2D_DESC desc = {};
    texture->GetDesc(&desc);
    if (desc.Width != light_desc.Width || desc.Height != light_desc.Height) return "dimensions";
    if (desc.ArraySize != 1u) return "array size";
    if (desc.SampleDesc.Count != 1u) return "sample count";
    return nullptr;
  };
  if (input_error(depth.Get()) != nullptr || input_error(normal.Get()) != nullptr) {
    static std::atomic_flag reported = {};
    mgsv::lighting::LogLightOnce(&reported, reshade::log::level::warning,
                                 "skipping lighting: depth or normal view has an unsupported layout or size; output=",
                                 light_desc.Width, "x", light_desc.Height);
    return;
  }

  // t4 is the native copied pre-SSS diffuse, distinct from the output UAV.
  context->CSGetShaderResources(4u, 1u, light_source.GetAddressOf());
  const bool generation_matches = shared_inputs.materials.generation == shared_inputs.generation;
  const char* source_error = input_error(light_source.Get(), DXGI_FORMAT_R16G16B16A16_FLOAT);
  const char* specular_error = input_error(shared_inputs.materials.specular.Get(), DXGI_FORMAT_R16G16B16A16_FLOAT);
  const char* albedo_error = "missing view";
  if (shared_inputs.materials.albedo != nullptr) {
    D3D11_SHADER_RESOURCE_VIEW_DESC albedo_desc = {};
    shared_inputs.materials.albedo->GetDesc(&albedo_desc);
    if (albedo_desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
        || albedo_desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
      albedo_error = input_error(shared_inputs.materials.albedo.Get(), albedo_desc.Format);
    } else {
      albedo_error = "requires sRGB view";
    }
  }
  if (!generation_matches || source_error != nullptr || specular_error != nullptr || albedo_error != nullptr) {
    // Report startup absence separately so it cannot hide a later rejection
    // after both material views have been captured. At most two reports/load.
    static std::atomic_flag missing_materials = {};
    static std::atomic_flag rejected_inputs = {};
    mgsv::lighting::LogLightOnce(
        shared_inputs.materials.albedo != nullptr && shared_inputs.materials.specular != nullptr
            ? &rejected_inputs
            : &missing_materials,
        reshade::log::level::warning,
        "raw GT GI inputs rejected; generation_matches=", generation_matches,
        " source=", (source_error != nullptr ? source_error : "ok"),
        " albedo=", (albedo_error != nullptr ? albedo_error : "ok"),
        " specular=", (specular_error != nullptr ? specular_error : "ok"),
        "; skipping custom lighting");
    return;
  }
  // These checks qualify layouts and ownership, not fresh pixels or GPU order.
  Microsoft::WRL::ComPtr<ID3D11Buffer> native_geometry;
  Microsoft::WRL::ComPtr<ID3D11Buffer> geometry;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context1;
  UINT geometry_first = 0u;
  UINT geometry_count = 0u;
  if (SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&context1)))) {
    context1->CSGetConstantBuffers1(0u, 1u, &native_geometry, &geometry_first, &geometry_count);
  } else {
    context->CSGetConstantBuffers(0u, 1u, &native_geometry);
  }
  if (native_geometry != nullptr) {
    D3D11_BUFFER_DESC description = {};
    native_geometry->GetDesc(&description);
    if (context1 == nullptr) geometry_count = description.ByteWidth / 16u;
    if (geometry_count >= 4u && (static_cast<uint64_t>(geometry_first) * 16u) + 64u <= description.ByteWidth) {
      description.Usage = D3D11_USAGE_DEFAULT;
      description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
      description.CPUAccessFlags = 0u;
      description.MiscFlags = 0u;
      description.StructureByteStride = 0u;
      auto* native_device = reinterpret_cast<ID3D11Device*>(cmd_list->get_device()->get_native());  // NOLINT(performance-no-int-to-ptr)
      if (SUCCEEDED(native_device->CreateBuffer(&description, nullptr, &geometry))) {
        context->CopyResource(geometry.Get(), native_geometry.Get());
      }
    }
  }
  // Every deferred reference outlives its recorded work. The native geometry
  // constants are copied on the GPU at this insertion, not read back by the CPU.
  const auto apply = [width = light_desc.Width, height = light_desc.Height, constants, geometry, geometry_first, geometry_count,
                      depth, normal, light, light_source,
                      albedo = shared_inputs.materials.albedo,
                      specular = shared_inputs.materials.specular,
                      generation = shared_inputs.generation](reshade::api::command_list* submitted, bool history_allowed) {
    const auto selected = inputs::Read(submitted->get_device());
    if (selected.device == nullptr || selected.generation != generation) return;
    auto* submitted_context = reinterpret_cast<ID3D11DeviceContext*>(submitted->get_native());  // NOLINT(performance-no-int-to-ptr)
    const auto previous_state = mgsv::rendering::d3d11_compute_state::Capture(submitted);
    if (geometry != nullptr) {
      Microsoft::WRL::ComPtr<ID3D11DeviceContext1> submitted_context1;
      auto* buffer = geometry.Get();
      if (SUCCEEDED(submitted_context->QueryInterface(IID_PPV_ARGS(&submitted_context1)))) {
        submitted_context1->CSSetConstantBuffers1(0u, 1u, &buffer, &geometry_first, &geometry_count);
      } else {
        submitted_context->CSSetConstantBuffers(0u, 1u, &buffer);
      }
    }
    const LightFrame submitted_frame = {
        .depth = {reinterpret_cast<uintptr_t>(depth.Get())},
        .normal = {reinterpret_cast<uintptr_t>(normal.Get())},
        .light = {reinterpret_cast<uintptr_t>(light.Get())},
        .light_source = {reinterpret_cast<uintptr_t>(light_source.Get())},
        .albedo = {reinterpret_cast<uintptr_t>(albedo.Get())},
        .specular = {reinterpret_cast<uintptr_t>(specular.Get())},
        .width = width,
        .height = height,
    };
    mgsv::ao::Apply(submitted_frame, submitted, constants,
                    temporal_inputs::Read(submitted->get_device(), submitted_frame.depth), history_allowed);
    mgsv::rendering::d3d11_compute_state::Restore(submitted, previous_state);
  };
  if (geometry != nullptr && mgsv::reconstruction::coordinator::ScheduleLighting(cmd_list, apply)) return;

  // Missing safe submission support keeps the existing same-context chain.
  // It may spatially filter current GI, but cannot advance temporal history.
  const mgsv::reconstruction::coordinator::ExecutionGuard execution_guard;
  apply(cmd_list, false);
}

}  // namespace mgsv::lighting

#endif  // SRC_GAMES_MGSV_LIGHTING_HPP_
