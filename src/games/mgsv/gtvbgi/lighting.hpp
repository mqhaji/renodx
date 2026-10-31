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
#include <cstddef>
#include <cstdint>
#include <span>
#include <sstream>
#include <utility>

#include <wrl/client.h>

#include <embed/shaders.h>
#include <include/reshade.hpp>

#include "../../../utils/settings.hpp"
#include "../assets/noise/fast_noise_gr8.h"
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

inline reshade::api::resource_view GetDepthMipView(reshade::api::command_list* cmd_list) {
  if (depth_mip_chain.ready) return depth_mip_chain.view;
  return GetFallbackView(cmd_list);
}

inline void Destroy(reshade::api::device* device) {
  DestroyDepthMipChain(device);
  DestroyLightPass(device, &resolve_pass);
  DestroyLightTarget(device, &visibility_target);
}

// Passed shader storage must outlive the descriptors; assemble once per registry/storage pair.
inline void AppendSettings(renodx::utils::settings::Settings* registry, ShaderInjectData* shader_injection) {
  if (std::ranges::any_of(*registry, [](const auto* setting) {
        return setting->key == "FxLightingMode";
      })) return;
  renodx::utils::settings::Settings additions = {
      new renodx::utils::settings::Setting{
          .key = "FxLightingMode",
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
          .default_value = 100.f,
          .label = "Ambient Occlusion Strength",
          .section = "Ambient Occlusion",
          .tooltip = "Controls how strongly the added shading darkens the scene. Values above 100 increase the darkening.",
          .max = 200.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },

      new renodx::utils::settings::Setting{
          .key = "FxGTSearchRadiusPixels",
          .binding = &shader_injection->gt_search_radius_pixels,
          .default_value = 2000.f,
          .label = "Search Radius",
          .section = "Ambient Occlusion (Advanced)",
          .tooltip = "Search distance in full-resolution pixels for both AO and GI. Larger values reach farther visible sources and blockers but spread samples farther apart. Default 2000 pairs with 40 samples per side.",
          .min = 1.f,
          .max = 4096.f,
          .format = "%.0f px",
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxGTSurfaceThickness",
          .binding = &shader_injection->gt_surface_thickness,
          .default_value = 0.15f,
          .label = "Surface Thickness",
          .section = "Ambient Occlusion (Advanced)",
          .tooltip = "Assumed surface thickness in native view-depth units for both AO and GI. Higher values fill more directions but can overstate thin surfaces.",
          .min = 0.f,
          .max = 0.5f,
          .format = "%.4f",
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxGTSurfaceOffset",
          .binding = &shader_injection->gt_surface_offset,
          .default_value = 0.0009765625f,
          .label = "Surface Offset",
          .section = "Ambient Occlusion (Advanced)",
          .tooltip = "Offsets the AO and GI receiver along its shading normal in native view-space units to reduce self-occlusion. Higher values can lose contact detail.",
          .min = 0.f,
          .max = 0.01f,
          .format = "%.6f",
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxGTSliceCount",
          .binding = &shader_injection->gt_slice_count,
          .default_value = 1.f,
          .label = "Sampling Directions",
          .section = "Ambient Occlusion (Advanced)",
          .tooltip = "Search lines through each pixel, sampled on both sides and shared by AO and Global Illumination. More lines improve coverage but cost more GPU time.",
          .min = 1.f,
          .max = 8.f,
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxGTSamplesPerSide",
          .binding = &shader_injection->gt_samples_per_side,
          .default_value = 40.f,
          .label = "Samples per Side",
          .section = "Ambient Occlusion (Advanced)",
          .tooltip = "Samples on each side of every sampling direction, shared by AO and Global Illumination. Higher values find smaller surfaces but cost more GPU time.",
          .min = 1.f,
          .max = 64.f,
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxGTResolution",
          .binding = &shader_injection->gt_resolution,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 1.f,
          .label = "Lighting Resolution",
          .section = "Ambient Occlusion (Advanced)",
          .tooltip = "Scales both width and height of the AO/GI trace. Quarter uses 1/16 as many pixels; Eighth uses 1/64. Lower resolutions can lose thin geometry and contact detail. Search radii stay in full-resolution pixels.",
          .labels = {"Full", "Half", "Quarter", "Eighth"},
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxGTDepthMipsEnabled",
          .binding = &depth_mips_enabled,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 0.f,
          .label = "Coarse Depth Sampling",
          .section = "Ambient Occlusion (Advanced)",
          .tooltip = "Uses reduced depth for distant AO and GI samples to catch objects between steps. GI locates the corresponding source pixel before reading its light. Off uses full-resolution depth as in the reference.",
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
  };
  registry->insert(std::ranges::find_if(*registry, [](const auto* setting) {
                     return setting->section == "Options";
                   }),
                   additions.begin(), additions.end());
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

inline LightTarget bounce_target;
inline LightTarget diagnostic_target;

// The trace and add passes receive the debug view through their effect-local injection copy.
inline float raw_view = 0.f;

inline void Destroy(reshade::api::device* device) {
  DestroyLightPass(device, &gather_pass);
  DestroyLightPass(device, &add_pass);
  DestroyLightTarget(device, &bounce_target);
  DestroyLightTarget(device, &diagnostic_target);
}

// Passed shader storage must outlive the descriptors; assemble once per registry/storage pair.
inline void AppendSettings(renodx::utils::settings::Settings* registry, ShaderInjectData* shader_injection) {
  if (std::ranges::any_of(*registry, [](const auto* setting) {
        return setting->key == "FxGIStrength";
      })) return;
  renodx::utils::settings::Settings additions = {
      new renodx::utils::settings::Setting{
          .key = "FxGIStrength",
          .binding = &shader_injection->gi_strength,
          .default_value = 100.f,
          .label = "Global Illumination Strength",
          .section = "Indirect Lighting",
          .tooltip = "Controls how much bounced light is added to the scene.",
          .max = 200.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
      },
  };
  additions.insert(
      additions.end(),
      {
          new renodx::utils::settings::Setting{
              .key = "FxGIRejectBackfaces",
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
      });
  additions.push_back(new renodx::utils::settings::Setting{
      .key = "FxGIDebugView",
      .binding = &raw_view,
      .value_type = renodx::utils::settings::SettingValueType::INTEGER,
      .default_value = 0.f,
      .label = "Global Illumination Debug View",
      .section = "Indirect Lighting (Advanced)",
      .tooltip = "Replaces diffuse lighting to inspect bounced light, sector coverage, accepted source directions, valid receivers, or source radiance. Surface color, specular, fog, and later processing still apply.",
      .labels = {"Off", "Bounce Lighting", "Geometry Coverage", "Accepted Source Coverage", "Valid / Invalid Results", "Source Lighting"},
      .is_visible = [shader_injection] {
        return shader_injection->lighting_mode >= 0.5f;
      },
  });
  additions.push_back(new renodx::utils::settings::Setting{
      .value_type = renodx::utils::settings::SettingValueType::TEXT,
      .label = "Shared sampling controls for Ambient Occlusion and Global Illumination:",
      .section = "Indirect Lighting",
      .is_visible = [shader_injection] { return shader_injection->lighting_mode >= 0.5f; },
  });
  registry->insert(std::ranges::find_if(*registry, [](const auto* setting) {
                     return setting->section == "Options";
                   }),
                   additions.begin(), additions.end());
  for (auto* setting : *registry) {
    if (setting->section == "Ambient Occlusion" || setting->section == "Ambient Occlusion (Advanced)"
        || setting->section == "Indirect Lighting" || setting->section == "Indirect Lighting (Advanced)") {
      setting->section = "Indirect Lighting";
      setting->group.clear();
    }
    if (!setting->key.starts_with("FxGI")
        && !setting->key.starts_with("FxGT")
        && setting->key != "FxGTSurfaceOffset"
        && setting->key != "FxLightingMode"
        && setting->key != "FxGIStrength"
        && setting->key != "FxGIDebugView"
        && setting->key != "FxAOStrength"
        && setting->key != "FxGTSliceCount"
        && setting->key != "FxGTSamplesPerSide"
        && setting->key != "FxGTDepthMipsEnabled"
        && setting->key != "FxGTResolution") continue;
    setting->on_change_value = [setting](float previous, float current) {
      // Slider drags can report every UI frame. Bound diagnostic volume per load.
      static std::atomic_uint32_t reports = 0u;
      const uint32_t report = reports.fetch_add(1u, std::memory_order_relaxed);
      if (report < 128u) {
        LogLight(reshade::log::level::info, "lighting setting changed; key=", setting->key,
                 " previous=", previous, " current=", current, " shader_value=", *setting->binding);
      } else if (report == 128u) {
        LogLight(reshade::log::level::info, "lighting setting change log limit reached; further UI changes remain active");
      }
    };
  }
  const auto first = std::ranges::find_if(*registry, [](const auto* setting) { return setting->section == "Indirect Lighting"; });
  const auto last = std::find_if(first, registry->end(), [](const auto* setting) { return setting->section != "Indirect Lighting"; });
  const auto order = [](const auto* setting) {
    if (setting->key == "FxLightingMode") return 0;
    if (setting->key == "FxAOStrength") return 1;
    if (setting->key == "FxGIStrength") return 2;
    if (setting->key == "FxGTResolution") return 3;
    if (setting->key.empty()) return 4;
    return setting->key == "FxGIDebugView" ? 6 : 5;
  };
  std::stable_sort(first, last, [&order](const auto* left, const auto* right) { return order(left) < order(right); });
}

}  // namespace mgsv::il

namespace mgsv::ao {

inline void Apply(const lighting::LightFrame& frame, reshade::api::command_list* cmd_list,
                  ShaderInjectData constants) {
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
  constants.il_scale = constants.gt_resolution_divisor;
  constants.gi_debug_view = std::clamp(il::raw_view, 0.f, 5.f);
  constants.gt_noise_frame = static_cast<float>(lighting::noise_frame.load(std::memory_order_relaxed) & 31u);
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

  if (!EnsureLightTarget(device, &visibility_target, width, height)
      || !EnsureLightTarget(device, &il::bounce_target, width, height)
      || !EnsureLightPass(device, &il::gather_pass, "mgsv: GT-VBGI trace created", __gtvbgi)
      || !EnsureLightPass(device, &resolve_pass, "mgsv: GT-VBGI AO application created", __gtvbgi_apply_ao)
      || !EnsureLightPass(device, &il::add_pass, "mgsv: GT-VBGI addition created", __gtvbgi_add)
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

  const bool depth_mips_requested = depth_mips_enabled != 0.f;
  const bool depth_mips_ready = BuildDepthMips(cmd_list, frame.depth, depth_mips_requested);
  constants.gt_depth_mips_enabled = (depth_mips_ready ? 1.f : 0.f);
  const auto scratch0 = GetFallbackUav(cmd_list, 0);
  const auto scratch1 = GetFallbackUav(cmd_list, 1);
  const auto scratch2 = GetFallbackUav(cmd_list, 2);
  const std::array<reshade::api::resource_view, 7> sources = {
      frame.depth,
      frame.normal,
      GetDepthMipView(cmd_list),
      frame.light_source,
      frame.albedo,
      frame.specular,
      lighting::noise_view,
  };
  DispatchLightPass(cmd_list, &il::gather_pass, constants, sources,
                    visibility_target.uav, il::bounce_target.uav,
                    constants.gi_debug_view >= 2.f ? il::diagnostic_target.uav : scratch1,
                    scratch2, width, height);

  const std::array<reshade::api::resource_view, 7> resolve_sources = {
      visibility_target.view,
      frame.depth,
      GetFallbackView(cmd_list),
      GetFallbackView(cmd_list),
      GetFallbackView(cmd_list),
      GetFallbackView(cmd_list),
      GetFallbackView(cmd_list),
  };
  // AO still treats existing diffuse broadly. Apply it before the new bounce,
  // whose sector coverage already integrates visibility.
  DispatchLightPass(cmd_list, &resolve_pass, constants, resolve_sources, frame.light,
                    scratch0, scratch1, scratch2, frame.width, frame.height);

  const std::array<reshade::api::resource_view, 7> add_sources = {
      il::bounce_target.view,
      frame.depth,
      constants.gi_debug_view >= 2.f ? il::diagnostic_target.view : GetFallbackView(cmd_list),
      GetFallbackView(cmd_list),
      GetFallbackView(cmd_list),
      GetFallbackView(cmd_list),
      GetFallbackView(cmd_list),
  };
  DispatchLightPass(cmd_list, &il::add_pass, constants, add_sources, frame.light,
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
               "; no dedicated GI history or spatial denoise; this is not GPU completion");

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

  if (shader_injection.lighting_mode < 0.5f) {
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
  const mgsv::lighting::LightFrame frame = {
      .depth = {reinterpret_cast<uintptr_t>(depth.Get())},
      .normal = {reinterpret_cast<uintptr_t>(normal.Get())},
      .light = {reinterpret_cast<uintptr_t>(light.Get())},
      .light_source = {reinterpret_cast<uintptr_t>(light_source.Get())},
      .albedo = {reinterpret_cast<uintptr_t>(shared_inputs.materials.albedo.Get())},
      .specular = {reinterpret_cast<uintptr_t>(shared_inputs.materials.specular.Get())},
      .width = light_desc.Width,
      .height = light_desc.Height,
  };

  // Lighting commands can also unbind graphics resources. Save the affected state once
  // and restore it after the full lighting chain, including originally empty slots.
  const auto previous_state = mgsv::rendering::d3d11_compute_state::Capture(cmd_list);
  mgsv::ao::Apply(frame, cmd_list, shader_injection);
  mgsv::rendering::d3d11_compute_state::Restore(cmd_list, previous_state);
}

}  // namespace mgsv::lighting

#endif  // SRC_GAMES_MGSV_LIGHTING_HPP_
