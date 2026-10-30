/*
 * Copyright (C) 2024 Musa Haji
 * Copyright (C) 2026 Musa Haji
 * Copyright (C) 2026 ZioPao
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <d3d11.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <sstream>
#include <utility>

#include <wrl/client.h>

#include <embed/shaders.h>
#include <include/reshade.hpp>

#include "../../../utils/settings.hpp"
#include "../d3d11_compute_state.hpp"
#include "../inputs.hpp"
#include "../shared.h"

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
  params[0].push_descriptors.count = 7u;  // t0-t6; input roles vary by pass
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
                              const std::array<reshade::api::resource_view, 7>& sources,
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
using lighting::EnsureLightPass;
using lighting::EnsureLightTarget;
using lighting::GetFallbackUav;
using lighting::GetFallbackView;
using lighting::LightPass;
using lighting::LightTarget;
using lighting::LogLight;
using lighting::LogLightOnce;

inline float depth_mips_enabled = 0.f;
inline LightPass visibility_pass;
inline LightPass resolve_pass;
inline LightTarget visibility_target;

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
      .code = __mgsv_ao_depth_mips.data(),
      .code_size = __mgsv_ao_depth_mips.size(),
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

// Builds the chain immediately before the AO dispatch, from the depth the AO is about to read.
inline bool BuildDepthMips(reshade::api::command_list* cmd_list, reshade::api::resource_view depth_view) {
  if (depth_mips_enabled == 0.f) return false;

  auto* device = cmd_list->get_device();
  if (depth_view.handle == 0) return false;

  const auto depth_desc = device->get_resource_desc(device->get_resource_from_view(depth_view));
  if (depth_desc.type != reshade::api::resource_type::texture_2d) return false;
  const uint32_t width = depth_desc.texture.width;
  const uint32_t height = depth_desc.texture.height;
  if (width < 2u || height < 2u) return false;

  if (!EnsureDepthMipChain(device, width, height)) return false;

  cmd_list->bind_pipeline(reshade::api::pipeline_stage::all_compute, depth_mip_chain.pipeline);
  for (uint32_t level = 0; level < DEPTH_MIP_LEVELS; ++level) {
    const reshade::api::resource_view source =
        level == 0 ? depth_view : depth_mip_chain.level_source_views[level - 1];
    const reshade::api::resource_view target = depth_mip_chain.level_views[level];
    const reshade::api::descriptor_table_update updates[] = {
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
    };
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

inline reshade::api::resource_view GetDepthMipView(reshade::api::command_list* cmd_list) {
  if (depth_mip_chain.ready) return depth_mip_chain.view;
  return GetFallbackView(cmd_list);
}

inline void Apply(const lighting::LightFrame& frame, reshade::api::command_list* cmd_list,
                  ShaderInjectData constants) {
  auto* device = cmd_list->get_device();

  // 0 = full, 1 = half, 2 = quarter: the AO factor and its resolve follow this.
  uint32_t ao_scale = 1u;
  if (constants.ao_resolution > 1.5f) {
    ao_scale = 4u;
  } else if (constants.ao_resolution > 0.5f) {
    ao_scale = 2u;
  }
  constants.ao_scale = static_cast<float>(ao_scale);
  const uint32_t ao_width = (frame.width + ao_scale - 1u) / ao_scale;
  const uint32_t ao_height = (frame.height + ao_scale - 1u) / ao_scale;

  if (EnsureLightTarget(device, &visibility_target, ao_width, ao_height)
      && EnsureLightPass(device, &visibility_pass, "mgsv: AO pass created", __mgsv_ao)
      && EnsureLightPass(device, &resolve_pass, "mgsv: AO resolve created",
                         __mgsv_ao_resolve)) {
    constants.ao_depth_mips = BuildDepthMips(cmd_list, frame.depth) ? 1.f : 0.f;
    const auto scratch0 = GetFallbackUav(cmd_list, 0);
    const auto scratch1 = GetFallbackUav(cmd_list, 1);
    const auto scratch2 = GetFallbackUav(cmd_list, 2);
    const std::array<reshade::api::resource_view, 7> sources = {
        frame.depth,                // t0
        frame.normal,               // t1
        GetDepthMipView(cmd_list),  // t2, the depth chain or placeholder
        GetFallbackView(cmd_list),  // t3, unused by the AO
        GetFallbackView(cmd_list),  // t4, unused by the AO
        GetFallbackView(cmd_list),  // t5, unused by the AO
        GetFallbackView(cmd_list),  // t6, unused by the AO
    };
    DispatchLightPass(cmd_list, &visibility_pass, constants, sources, visibility_target.uav, scratch0, scratch1, scratch2,
                      ao_width, ao_height);

    const std::array<reshade::api::resource_view, 7> resolve_sources = {
        visibility_target.view,     // t0, calculated visibility
        frame.depth,                // t1, joint bilateral upsample
        GetFallbackView(cmd_list),  // t2, unused
        GetFallbackView(cmd_list),  // t3, unused
        GetFallbackView(cmd_list),  // t4, unused
        GetFallbackView(cmd_list),  // t5, unused
        GetFallbackView(cmd_list),  // t6, unused
    };
    DispatchLightPass(cmd_list, &resolve_pass, constants, resolve_sources, frame.light, scratch0, scratch1,
                      scratch2, frame.width, frame.height);
    static std::atomic_flag recorded = {};
    LogLightOnce(&recorded, reshade::log::level::info,
                 "AO commands recorded; output=", frame.width, "x", frame.height,
                 " effect=", ao_width, "x", ao_height, " depth_mips=", constants.ao_depth_mips,
                 "; this is not GPU completion");
    if (depth_mips_enabled != 0.f && constants.ao_depth_mips == 0.f) {
      static std::atomic_flag mip_failed = {};
      LogLightOnce(&mip_failed, reshade::log::level::warning,
                   "AO depth mip generation unavailable; AO is using full-resolution depth samples");
    }
  } else {
    static std::atomic_flag failed = {};
    LogLightOnce(&failed, reshade::log::level::warning,
                 "AO setup failed; no AO commands recorded; effect=", ao_width, "x", ao_height);
  }
}

inline void Destroy(reshade::api::device* device) {
  DestroyDepthMipChain(device);
  DestroyLightPass(device, &visibility_pass);
  DestroyLightPass(device, &resolve_pass);
  DestroyLightTarget(device, &visibility_target);
}

// Passed shader storage must outlive the descriptors; assemble once per registry/storage pair.
inline void AppendSettings(renodx::utils::settings::Settings* registry, ShaderInjectData* shader_injection) {
  if (std::ranges::any_of(*registry, [](const auto* setting) {
        return setting->key == "FxAmbientOcclusion";
      })) return;
  renodx::utils::settings::Settings additions = {
      new renodx::utils::settings::Setting{
          .key = "FxAmbientOcclusion",
          .binding = &shader_injection->ao_enabled,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 1.f,
          .label = "Visibility-Bitmask Ambient Occlusion",
          .section = "Ambient Occlusion",
          .tooltip = "Adds shading where nearby surfaces block light. This also darkens directly lit surfaces.",
      },
      new renodx::utils::settings::Setting{
          .key = "FxAmbientOcclusionStrength",
          .binding = &shader_injection->ao_strength,
          .default_value = 100.f,
          .label = "AO Strength",
          .section = "Ambient Occlusion",
          .tooltip = "Controls how strongly the added shading darkens the scene. Values above 100 increase the darkening.",
          .max = 400.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [shader_injection] { return shader_injection->ao_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxAmbientOcclusionSamplingRadius",
          .binding = &shader_injection->ao_radius,
          .default_value = 70.f,
          .label = "AO Sampling Radius",
          .section = "Ambient Occlusion",
          .tooltip = "Controls how far around each pixel the effect searches for nearby surfaces. Higher values create broader shading.",
          .max = 100.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [shader_injection] { return shader_injection->ao_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxAmbientOcclusionThicknessScale",
          .binding = &shader_injection->ao_thickness,
          .default_value = 800.f,
          .label = "AO Thickness Scale",
          .section = "Ambient Occlusion",
          .tooltip = "Controls how thick objects are assumed to be. Higher values block more light but can over-darken thin objects.",
          .max = 2000.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [shader_injection] { return shader_injection->ao_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxAmbientOcclusionDepthFadeScale",
          .binding = &shader_injection->ao_fade,
          .default_value = 100.f,
          .label = "AO Depth Fade Scale",
          .section = "Ambient Occlusion",
          .tooltip = "Controls how quickly the shading fades with depth. Higher values keep it visible farther from the camera.",
          .max = 100.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [shader_injection] { return shader_injection->ao_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxAmbientOcclusionSliceCount",
          .binding = &shader_injection->ao_slices,
          .default_value = 2.f,
          .label = "AO Slice Count",
          .section = "Ambient Occlusion (Advanced)",
          .tooltip = "Controls how many directions around each pixel are checked. Higher values improve coverage but cost more GPU time.",
          .max = 8.f,
          .is_visible = [shader_injection] { return shader_injection->ao_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxAmbientOcclusionSamplesPerSide",
          .binding = &shader_injection->ao_steps,
          .default_value = 6.f,
          .label = "AO Samples per Side",
          .section = "Ambient Occlusion (Advanced)",
          .tooltip = "Controls how many depth samples are taken on each side of every slice. Higher values help find thin objects but cost more GPU time.",
          .max = 20.f,
          .is_visible = [shader_injection] { return shader_injection->ao_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxAmbientOcclusionResolution",
          .binding = &shader_injection->ao_resolution,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 1.f,
          .label = "AO Resolution",
          .section = "Ambient Occlusion (Advanced)",
          .tooltip = "Sets the resolution used to calculate the shading. Lower resolutions are faster but can lose detail.",
          .labels = {"Full", "Half", "Quarter"},
          .is_visible = [shader_injection] { return shader_injection->ao_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxAmbientOcclusionDepthMipSampling",
          .binding = &depth_mips_enabled,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 1.f,
          .label = "AO Depth Mip Sampling",
          .section = "Ambient Occlusion (Advanced)",
          .tooltip = "Uses lower-resolution depth for distant samples to help catch objects between sampling steps.",
          .is_visible = [shader_injection] { return shader_injection->ao_enabled != 0.f; },
      },
  };
  registry->insert(std::ranges::find_if(*registry, [](const auto* setting) {
                     return setting->section == "Options";
                   }),
                   additions.begin(), additions.end());
}

}  // namespace mgsv::ao

// Indirect lighting resources, history, passes, and settings.
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
inline LightPass denoise_pass;
inline LightPass add_pass;
inline LightPass accumulate_pass;

inline LightTarget bounce_target;
inline LightTarget filtered_target;
inline LightTarget history_targets[2];
inline LightTarget history_aux_targets[2];
inline LightTarget moments_targets[2];
inline LightTarget accumulated_target;

inline uint32_t history_index = 0;
inline uint32_t history_width = 0;
inline uint32_t history_height = 0;
inline bool history_valid = false;

inline void Apply(const lighting::LightFrame& frame, reshade::api::command_list* cmd_list,
                  ShaderInjectData constants, reshade::api::resource_view velocity_view) {
  auto* device = cmd_list->get_device();

  const auto light_source = frame.light_source;

  // 0 = full, 1 = half, 2 = quarter: the gather, history, moments and a-trous all follow.
  uint32_t scale = 1u;
  if (constants.il_resolution > 1.5f) {
    scale = 4u;
  } else if (constants.il_resolution > 0.5f) {
    scale = 2u;
  }
  constants.il_scale = static_cast<float>(scale);
  const uint32_t width = (frame.width + scale - 1u) / scale;
  const uint32_t height = (frame.height + scale - 1u) / scale;

  if (history_width != width || history_height != height) {
    LogLight(reshade::log::level::info, "indirect lighting dimensions changed; previous_size=",
             history_width, "x", history_height, " size=", width, "x", height,
             " output=", frame.width, "x", frame.height, "; history invalidated, preparing resources");
    history_width = width;
    history_height = height;
    history_valid = false;
  }

  if (EnsureLightTarget(device, &bounce_target, width, height)
      && EnsureLightTarget(device, &filtered_target, width, height)
      && EnsureLightTarget(device, &history_targets[0], width, height)
      && EnsureLightTarget(device, &history_targets[1], width, height)
      && EnsureLightTarget(device, &history_aux_targets[0], width, height)
      && EnsureLightTarget(device, &history_aux_targets[1], width, height)
      && EnsureLightTarget(device, &moments_targets[0], width, height,
                           reshade::api::format::r16g16_float)
      && EnsureLightTarget(device, &moments_targets[1], width, height,
                           reshade::api::format::r16g16_float)
      && EnsureLightTarget(device, &accumulated_target, width, height)
      && EnsureLightPass(device, &gather_pass, "mgsv: IL pass created", __mgsv_il)
      && EnsureLightPass(device, &denoise_pass, "mgsv: IL composite created",
                         __mgsv_il_composite)
      && EnsureLightPass(device, &add_pass, "mgsv: IL add created",
                         __mgsv_il_add)
      && EnsureLightPass(device, &accumulate_pass, "mgsv: IL accumulate created",
                         __mgsv_il_accumulate)
      && EnsureFallbackViews(device)) {
    // Use separate textures for unused outputs so they cannot unbind a pass input.
    const auto fallback_uav0 = GetFallbackUav(cmd_list, 0u);
    const auto fallback_uav1 = GetFallbackUav(cmd_list, 1u);
    const auto fallback_uav2 = GetFallbackUav(cmd_list, 2u);
    const std::array<reshade::api::resource_view, 7> gather_sources = {
        frame.depth,                // t0
        frame.normal,               // t1
        light_source,               // t2
        frame.albedo,               // t3
        frame.specular,             // t4
        GetFallbackView(cmd_list),  // t5, unused
        GetFallbackView(cmd_list),  // t6, unused
    };
    DispatchLightPass(cmd_list, &gather_pass, constants, gather_sources, bounce_target.uav,
                      fallback_uav0, fallback_uav1, fallback_uav2, width, height);

    // A missing velocity view sets the history blend weight to zero. This does not verify
    // which frame the view belongs to or prevent all history reads in the shader.
    if (velocity_view.handle == 0u) {
      static std::atomic_flag reported = {};
      LogLightOnce(&reported, reshade::log::level::warning,
                   "indirect lighting has no captured velocity view; history blend weight is zero");
    }
    constants.il_history_valid = history_valid && velocity_view.handle != 0 ? 1.f : 0.f;
    const std::array<reshade::api::resource_view, 7> accumulate_sources = {
        bounce_target.view,                       // t0
        history_targets[history_index].view,      // t1
        velocity_view,                            // t2
        frame.depth,                              // t3
        frame.normal,                             // t4
        history_aux_targets[history_index].view,  // t5
        moments_targets[history_index].view,      // t6
    };
    const uint32_t write_index = history_index ^ 1u;
    DispatchLightPass(cmd_list, &accumulate_pass, constants, accumulate_sources,
                      history_targets[write_index].uav, accumulated_target.uav,
                      history_aux_targets[write_index].uav,
                      moments_targets[write_index].uav, width, height);
    history_index = write_index;
    history_valid = true;

    // Denoise with sample spacing of 1, 2, then 4 pixels. Alternate input and output
    // textures so each pass reads a different texture from the one it writes.
    const uint32_t filter_passes =
        static_cast<uint32_t>(std::clamp(constants.il_filter_radius, 1.f, 3.f));
    reshade::api::resource_view filter_input = accumulated_target.view;
    for (uint32_t step = 0; step < filter_passes; ++step) {
      const auto filter_output = (step % 2) == 0 ? filtered_target.uav : accumulated_target.uav;
      const std::array<reshade::api::resource_view, 7> filter_sources = {
          filter_input,                         // t0
          frame.depth,                          // t1
          frame.normal,                         // t2
          moments_targets[history_index].view,  // t3, mean and mean-square luminance
          GetFallbackView(cmd_list),            // t4, unused
          GetFallbackView(cmd_list),            // t5, unused
          GetFallbackView(cmd_list),            // t6, unused
      };
      constants.il_filter_step = static_cast<float>(step);
      DispatchLightPass(cmd_list, &denoise_pass, constants, filter_sources, filter_output,
                        fallback_uav0, fallback_uav1, fallback_uav2, width, height);
      filter_input = (step % 2) == 0 ? filtered_target.view : accumulated_target.view;
    }
    constants.il_filter_step = 0.f;

    // Use the last denoise result: filtered_target after 1 or 3 passes,
    // accumulated_target after 2 passes.
    const bool filtered_in_filtered = (filter_passes % 2) != 0;
    const auto filtered_view = filtered_in_filtered ? filtered_target.view
                                                    : accumulated_target.view;

    const std::array<reshade::api::resource_view, 7> add_sources = {
        filtered_view,              // t0
        frame.depth,                // t1, joint bilateral upsample
        GetFallbackView(cmd_list),  // t2, unused
        GetFallbackView(cmd_list),  // t3, unused
        GetFallbackView(cmd_list),  // t4, unused
        GetFallbackView(cmd_list),  // t5, unused
        GetFallbackView(cmd_list),  // t6, unused
    };
    DispatchLightPass(cmd_list, &add_pass, constants, add_sources, frame.light,
                      fallback_uav0, fallback_uav1, fallback_uav2, frame.width, frame.height);
    static std::atomic_flag recorded = {};
    LogLightOnce(&recorded, reshade::log::level::info,
                 "indirect lighting commands recorded; output=", frame.width, "x", frame.height,
                 " effect=", width, "x", height, " denoise_iterations=", filter_passes,
                 " velocity_present=", velocity_view.handle != 0u, "; this is not GPU completion");
  } else {
    static std::atomic_flag failed = {};
    LogLightOnce(&failed, reshade::log::level::warning,
                 "indirect lighting setup failed; no indirect lighting commands recorded; effect=",
                 width, "x", height);
  }
}

inline void Destroy(reshade::api::device* device) {
  DestroyLightPass(device, &gather_pass);
  DestroyLightPass(device, &denoise_pass);
  DestroyLightPass(device, &add_pass);
  DestroyLightPass(device, &accumulate_pass);
  DestroyLightTarget(device, &bounce_target);
  DestroyLightTarget(device, &filtered_target);
  DestroyLightTarget(device, &history_targets[0]);
  DestroyLightTarget(device, &history_targets[1]);
  DestroyLightTarget(device, &history_aux_targets[0]);
  DestroyLightTarget(device, &history_aux_targets[1]);
  DestroyLightTarget(device, &moments_targets[0]);
  DestroyLightTarget(device, &moments_targets[1]);
  DestroyLightTarget(device, &accumulated_target);
  history_valid = false;
}

// Passed shader storage must outlive the descriptors; assemble once per registry/storage pair.
inline void AppendSettings(renodx::utils::settings::Settings* registry, ShaderInjectData* shader_injection) {
  if (std::ranges::any_of(*registry, [](const auto* setting) {
        return setting->key == "FxIndirectLighting";
      })) return;
  renodx::utils::settings::Settings additions = {
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLighting",
          .binding = &shader_injection->il_enabled,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 1.f,
          .label = "Screen-Space Indirect Lighting",
          .section = "Indirect Lighting",
          .tooltip = "Adds approximate bounced light from surfaces visible on screen. Objects outside the screen cannot contribute.",
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingStrength",
          .binding = &shader_injection->il_amount,
          .default_value = 80.f,
          .label = "Indirect Lighting Strength",
          .section = "Indirect Lighting",
          .tooltip = "Controls how much bounced light is added to the scene.",
          .max = 200.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [shader_injection] { return shader_injection->il_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingSamplingRadius",
          .binding = &shader_injection->il_radius,
          .default_value = 80.f,
          .label = "Indirect Lighting Sampling Radius",
          .section = "Indirect Lighting",
          .tooltip = "Controls how far the effect searches for surfaces contributing light, as a percentage of screen height.",
          .max = 100.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [shader_injection] { return shader_injection->il_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingSampleCount",
          .binding = &shader_injection->il_samples,
          .default_value = 8.f,
          .label = "Indirect Lighting Sample Count",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Controls how many nearby samples are checked per pixel. Higher values can reduce noise but cost more GPU time.",
          .min = 2.f,
          .max = 16.f,
          .is_visible = [shader_injection] { return shader_injection->il_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingResolution",
          .binding = &shader_injection->il_resolution,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 2.f,
          .label = "Indirect Lighting Resolution",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Sets the resolution used to calculate and smooth bounced light. Lower resolutions are faster but can lose detail.",
          .labels = {"Full", "Half", "Quarter"},
          .is_visible = [shader_injection] { return shader_injection->il_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingDenoiseIterations",
          .binding = &shader_injection->il_filter_radius,
          .default_value = 3.f,
          .label = "Indirect Lighting Denoise Iterations",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Controls how many smoothing passes are applied. More passes reduce noise but can soften detail and cost more GPU time.",
          .min = 1.f,
          .max = 3.f,
          .is_visible = [shader_injection] { return shader_injection->il_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingHistoryWeight",
          .binding = &shader_injection->il_history_blend,
          .default_value = 90.f,
          .label = "Indirect Lighting History Weight",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Limits how much previous frames contribute when history is usable. Higher values can reduce noise but leave longer trails.",
          .max = 95.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [shader_injection] { return shader_injection->il_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingDebugView",
          .binding = &shader_injection->il_debug,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 0.f,
          .label = "Indirect Lighting Debug View",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Replaces the normal lighting output with a view used to inspect the effect.",
          .labels = {"Off", "Indirect Lighting Only", "History Difference", "History Length", "Reprojection Diagnostics", "Relative Variance"},
          .is_visible = [shader_injection] { return shader_injection->il_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingMinimumValidSamples",
          .binding = &shader_injection->il_min_samples,
          .default_value = 1.f,
          .label = "Indirect Lighting Minimum Valid Samples",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Sets how many samples must pass the checks before a pixel contributes bounced light.",
          .min = 1.f,
          .max = 16.f,
          // Hidden from the UI: grazing surfaces need one sample to survive for the ground to
          // contribute at all, and the shader clamps the value, so there is nothing to tune.
          .is_visible = [] { return false; },
      },
  };
  registry->insert(std::ranges::find_if(*registry, [](const auto* setting) {
                     return setting->section == "Options";
                   }),
                   additions.begin(), additions.end());
}

}  // namespace mgsv::il

// Native composite inputs and post-scatter lighting orchestration.
namespace mgsv::lighting {

// Add indirect lighting and AO after the game's subsurface-scattering passes.
// Indirect lighting runs first, so AO darkens both the direct and added light.

// Called immediately after the game's scatter dispatch is recorded, not after GPU completion.
// Dispatch leaves its bindings unchanged, so read them here before recording our own passes.
inline void ApplyLightPasses(reshade::api::command_list* cmd_list, const ShaderInjectData& shader_injection) {
  if (cmd_list == nullptr || cmd_list->get_device()->get_api() != reshade::api::device_api::d3d11) return;

  const bool ao_enabled = shader_injection.ao_enabled != 0.f;
  const bool il_enabled = shader_injection.il_enabled != 0.f;
  if (!ao_enabled && !il_enabled) {
    static std::atomic_flag reported = {};
    mgsv::lighting::LogLightOnce(&reported, reshade::log::level::info, "scatter hook reached; AO and indirect lighting are disabled");
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

  const auto is_supported_input = [&](ID3D11ShaderResourceView* view, ID3D11Resource* excluded_resource = nullptr) {
    if (view == nullptr) return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC view_desc = {};
    view->GetDesc(&view_desc);
    if (view_desc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D
        || view_desc.Texture2D.MostDetailedMip != 0u) return false;

    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    view->GetResource(resource.GetAddressOf());
    if (resource.Get() == excluded_resource || FAILED(resource.As(&texture))) return false;

    D3D11_TEXTURE2D_DESC desc = {};
    texture->GetDesc(&desc);
    return desc.Width == light_desc.Width && desc.Height == light_desc.Height
           && desc.ArraySize == 1u && desc.SampleDesc.Count == 1u;
  };
  if (!is_supported_input(depth.Get()) || !is_supported_input(normal.Get())) {
    static std::atomic_flag reported = {};
    mgsv::lighting::LogLightOnce(&reported, reshade::log::level::warning,
                                 "skipping lighting: depth or normal view has an unsupported layout or size; output=",
                                 light_desc.Width, "x", light_desc.Height);
    return;
  }

  // Missing inputs or a light source that overlaps the output skip indirect lighting, not AO.
  if (il_enabled) {
    context->CSGetShaderResources(4u, 1u, light_source.GetAddressOf());
  }
  // Pin native observations for this callback. The compatibility selection is not a freshness guarantee.
  const reshade::api::resource_view albedo_view = {reinterpret_cast<uintptr_t>(shared_inputs.materials.albedo.Get())};
  const reshade::api::resource_view specular_view = {reinterpret_cast<uintptr_t>(shared_inputs.materials.specular.Get())};
  const bool run_il = il_enabled && is_supported_input(light_source.Get(), light_resource.Get())
                      && albedo_view.handle != 0u && specular_view.handle != 0u;
  if (il_enabled && !run_il) {
    static std::atomic_flag reported = {};
    mgsv::lighting::LogLightOnce(&reported, reshade::log::level::warning,
                                 "skipping indirect lighting: check light source layout, size, and output overlap; source_present=",
                                 light_source != nullptr, " albedo_present=", albedo_view.handle != 0u,
                                 " specular_present=", specular_view.handle != 0u, "; AO enabled=", ao_enabled);
  }
  if (!run_il && !ao_enabled) return;

  const mgsv::lighting::LightFrame frame = {
      .depth = {reinterpret_cast<uintptr_t>(depth.Get())},
      .normal = {reinterpret_cast<uintptr_t>(normal.Get())},
      .light = {reinterpret_cast<uintptr_t>(light.Get())},
      .light_source = {reinterpret_cast<uintptr_t>(light_source.Get())},
      .albedo = albedo_view,
      .specular = specular_view,
      .width = light_desc.Width,
      .height = light_desc.Height,
  };

  // Lighting commands can also unbind graphics resources. Save the affected state once
  // and restore it after both effects, including slots that were originally empty.
  const auto previous_state = mgsv::rendering::d3d11_compute_state::Capture(cmd_list);
  if (run_il) {
    // Only the previous AA capture path publishes this selection; new Off-mode observations are not admitted.
    mgsv::il::Apply(frame, cmd_list, shader_injection,
                    {reinterpret_cast<uintptr_t>(shared_inputs.lighting_motion.velocity.Get())});
  }
  if (ao_enabled) {
    mgsv::ao::Apply(frame, cmd_list, shader_injection);
  }
  mgsv::rendering::d3d11_compute_state::Restore(cmd_list, previous_state);
}

}  // namespace mgsv::lighting