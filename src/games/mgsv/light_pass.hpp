/*
 * Copyright (C) 2026 Musa Haji
 * Copyright (C) 2026 ZioPao
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

#include <include/reshade.hpp>

#include "../../utils/settings.hpp"
#include "./shared.h"

extern ShaderInjectData shader_injection;

struct LightFrame {
  reshade::api::resource_view depth = {0};
  reshade::api::resource_view normal = {0};
  reshade::api::resource_view light = {0};
  reshade::api::resource_view light_source = {0};
  uint32_t width = 0;
  uint32_t height = 0;
  bool valid = false;
};

struct LightPass {
  reshade::api::pipeline pipeline = {0};
  uint32_t width = 0;
  uint32_t height = 0;
  bool ready = false;
};

struct LightTarget {
  reshade::api::resource resource = {0};
  reshade::api::resource_view view = {0};
  reshade::api::resource_view uav = {0};
  uint32_t width = 0;
  uint32_t height = 0;
  reshade::api::format format = reshade::api::format::unknown;
};

inline LightFrame light_frame;
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
    reshade::log::message(reshade::log::level::error, "mgsv: light target failed");
    return false;
  }

  target->width = width;
  target->height = height;
  return true;
}

inline bool EnsureLightPassLayout(reshade::api::device* device) {
  if (light_pass_layout.handle != 0) return true;

  std::array<reshade::api::pipeline_layout_param, 3> params = {};
  params[0].type = reshade::api::pipeline_layout_param_type::push_descriptors;
  params[0].push_descriptors.count = 7u;  // t0 depth, t1 normal, t2 pass input, t3 albedo, t4 specular, t5 history aux, t6 moments
  params[0].push_descriptors.type = reshade::api::descriptor_type::texture_shader_resource_view;
  params[0].push_descriptors.dx_register_index = 0u;
  params[0].push_descriptors.dx_register_space = 0u;
  params[1].type = reshade::api::pipeline_layout_param_type::push_descriptors;
  params[1].push_descriptors.count = 4u;  // u0 result, u1 display, u2 history aux, u3 moments
  params[1].push_descriptors.type = reshade::api::descriptor_type::texture_unordered_access_view;
  params[1].push_descriptors.dx_register_index = 0u;
  params[1].push_descriptors.dx_register_space = 0u;
  params[2].type = reshade::api::pipeline_layout_param_type::push_constants;
  params[2].push_constants.count = static_cast<uint32_t>(sizeof(shader_injection) / sizeof(uint32_t));
  params[2].push_constants.dx_register_index = 13u;
  params[2].push_constants.dx_register_space = 0u;
  params[2].push_constants.visibility = reshade::api::shader_stage::compute;
  if (!device->create_pipeline_layout(
          static_cast<uint32_t>(params.size()), params.data(), &light_pass_layout)) {
    reshade::log::message(reshade::log::level::error, "mgsv: light pass layout failed");
    return false;
  }
  return true;
}

inline bool EnsureLightPass(reshade::api::device* device, LightPass* pass, const char* name,
                            std::span<const uint8_t> code, uint32_t width, uint32_t height) {
  if (pass->ready && pass->width == width && pass->height == height) return true;

  pass->ready = false;
  if (pass->pipeline.handle != 0) {
    device->destroy_pipeline(pass->pipeline);
    pass->pipeline = {0};
  }
  pass->width = 0;
  pass->height = 0;
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
  if (!device->create_pipeline(light_pass_layout, 1, &subobject, &pass->pipeline)) return false;

  pass->width = width;
  pass->height = height;
  pass->ready = true;
  reshade::log::message(reshade::log::level::info, name);
  return true;
}

inline void DestroyLightPass(reshade::api::device* device, LightPass* pass) {
  pass->ready = false;
  if (pass->pipeline.handle != 0) {
    device->destroy_pipeline(pass->pipeline);
    pass->pipeline = {0};
  }
  pass->width = 0;
  pass->height = 0;
}

inline void DestroyLightPassLayout(reshade::api::device* device) {
  if (light_pass_layout.handle != 0) {
    device->destroy_pipeline_layout(light_pass_layout);
    light_pass_layout = {0};
  }
}

// 1x1 stand-ins for descriptors a pass must bind but does not read or write. The SRV and
// UAV fallbacks are separate resources: a resource cannot be bound as both at once, and
// each unused UAV slot needs its own texture.
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
      && std::all_of(fallback_uavs.begin(), fallback_uavs.end(),
                     [](const auto& view) { return view.handle != 0; })) {
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
    reshade::log::message(reshade::log::level::error, "mgsv: fallback view failed");
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
      reshade::log::message(reshade::log::level::error, "mgsv: fallback UAV failed");
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
      target, second_target, third_target, fourth_target};
  cmd_list->push_descriptors(
      reshade::api::shader_stage::all_compute, light_pass_layout, 1u,
      {.table = {0u},
       .binding = 0u,
       .array_offset = 0u,
       .count = 4u,
       .type = reshade::api::descriptor_type::texture_unordered_access_view,
       .descriptors = targets.data()});
  cmd_list->push_descriptors(
      reshade::api::shader_stage::all_compute, light_pass_layout, 0u,
      {.table = {0u},
       .binding = 0u,
       .array_offset = 0u,
       .count = static_cast<uint32_t>(sources.size()),
       .type = reshade::api::descriptor_type::texture_shader_resource_view,
       .descriptors = sources.data()});
  cmd_list->push_constants(
      reshade::api::shader_stage::all_compute, light_pass_layout, 2u, 0u,
      static_cast<uint32_t>(sizeof(constants) / sizeof(uint32_t)), &constants);
  cmd_list->dispatch((width + 7u) / 8u, (height + 7u) / 8u, 1u);
}

inline void InsertSettingsBeforeSection(renodx::utils::settings::Settings& settings,
                                        const char* section,
                                        const renodx::utils::settings::Settings& additions) {
  const auto position = std::find_if(settings.begin(), settings.end(), [section](auto* setting) {
    return setting->section == section;
  });
  settings.insert(position, additions.begin(), additions.end());
}
