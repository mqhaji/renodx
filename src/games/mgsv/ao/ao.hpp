/*
 * Copyright (C) 2026 Musa Haji
 * Copyright (C) 2026 ZioPao
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "../light_pass.hpp"

namespace ao {

inline float depth_mips_enabled = 0.f;
inline LightPass pass;
inline LightPass resolve_pass;
inline LightTarget target;

// The AO march's depth mip chain. Every level holds the nearest of the four texels
// below it, so a distant step can fetch one texel that covers the footprint its
// spacing would otherwise skip over.
constexpr uint32_t kDepthMipLevels = 4;

struct DepthMipChain {
  reshade::api::resource resource = {0};
  reshade::api::resource_view view = {0};
  std::array<reshade::api::resource_view, kDepthMipLevels> level_views = {};
  std::array<reshade::api::resource_view, kDepthMipLevels> level_source_views = {};
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
      std::max(width / 2u, 1u), std::max(height / 2u, 1u), 1u, kDepthMipLevels,
      reshade::api::format::r32_float,
      1u,
      reshade::api::memory_heap::gpu_only,
      reshade::api::resource_usage::shader_resource
          | reshade::api::resource_usage::unordered_access
          | reshade::api::resource_usage::copy_source
          | reshade::api::resource_usage::copy_dest);
  if (!device->create_resource(desc, nullptr, desc.usage, &depth_mip_chain.resource)) {
    reshade::log::message(reshade::log::level::error, "mgsv: depth mip chain allocation failed");
    return false;
  }
  if (!device->create_resource_view(
          depth_mip_chain.resource,
          reshade::api::resource_usage::shader_resource,
          reshade::api::resource_view_desc(reshade::api::format::r32_float),
          &depth_mip_chain.view)) {
    reshade::log::message(reshade::log::level::error, "mgsv: depth mip chain read view failed");
    return false;
  }
  for (uint32_t level = 0; level < kDepthMipLevels; ++level) {
    const reshade::api::resource_view_desc level_desc(
        reshade::api::format::r32_float, level, 1u, 0u, 1u);
    if (!device->create_resource_view(
            depth_mip_chain.resource,
            reshade::api::resource_usage::unordered_access,
            level_desc,
            &depth_mip_chain.level_views[level])) {
      reshade::log::message(reshade::log::level::error, "mgsv: depth mip chain write view failed");
      return false;
    }
    if (!device->create_resource_view(
            depth_mip_chain.resource,
            reshade::api::resource_usage::shader_resource,
            level_desc,
            &depth_mip_chain.level_source_views[level])) {
      reshade::log::message(reshade::log::level::error, "mgsv: depth mip chain level view failed");
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
    reshade::log::message(reshade::log::level::error, "mgsv: depth mip chain layout failed");
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
    reshade::log::message(reshade::log::level::error, "mgsv: depth mip chain pipeline failed");
    return false;
  }

  depth_mip_chain.width = width;
  depth_mip_chain.height = height;
  depth_mip_chain.ready = true;
  reshade::log::message(reshade::log::level::info, "mgsv: depth mip chain created");
  return true;
}

// Builds the chain immediately before the AO dispatch, from the depth the AO is about to read.
inline bool BuildDepthMips(reshade::api::command_list* cmd_list, reshade::api::resource_view depth_view) {
  shader_injection.ao_depth_mips = 0.f;
  if (shader_injection.ao_enabled == 0.f || depth_mips_enabled == 0.f) return true;

  auto* device = cmd_list->get_device();
  if (depth_view.handle == 0) return true;

  const auto depth_desc = device->get_resource_desc(device->get_resource_from_view(depth_view));
  if (depth_desc.type != reshade::api::resource_type::texture_2d) return true;
  const uint32_t width = depth_desc.texture.width;
  const uint32_t height = depth_desc.texture.height;
  if (width < 2u || height < 2u) return true;

  if (!EnsureDepthMipChain(device, width, height)) return true;

  cmd_list->bind_pipeline(reshade::api::pipeline_stage::all_compute, depth_mip_chain.pipeline);
  for (uint32_t level = 0; level < kDepthMipLevels; ++level) {
    const reshade::api::resource_view source =
        level == 0 ? depth_view : depth_mip_chain.level_source_views[level - 1];
    const reshade::api::resource_view target = depth_mip_chain.level_views[level];
    const reshade::api::descriptor_table_update updates[] = {
        {.table = {},
         .binding = 0u,
         .array_offset = 0u,
         .count = 1u,
         .type = reshade::api::descriptor_type::shader_resource_view,
         .descriptors = &source},
        {.table = {},
         .binding = 0u,
         .array_offset = 0u,
         .count = 1u,
         .type = reshade::api::descriptor_type::unordered_access_view,
         .descriptors = &target},
    };
    cmd_list->push_descriptors(
        reshade::api::shader_stage::all_compute, depth_mip_chain.layout, 1u, updates[1]);
    cmd_list->push_descriptors(
      reshade::api::shader_stage::all_compute, depth_mip_chain.layout, 0u, updates[0]);

    const uint32_t level_width = std::max(width >> (level + 1u), 1u);
    const uint32_t level_height = std::max(height >> (level + 1u), 1u);
    cmd_list->dispatch((level_width + 7u) / 8u, (level_height + 7u) / 8u, 1u);
  }

  shader_injection.ao_depth_mips = 1.f;
  return true;
}

inline reshade::api::resource_view GetDepthMipView(reshade::api::command_list* cmd_list) {
  if (depth_mip_chain.ready) return depth_mip_chain.view;
  return GetFallbackView(cmd_list);
}

inline void Apply(const LightFrame& frame, reshade::api::command_list* cmd_list) {
  auto* device = cmd_list->get_device();

  // 0 = full, 1 = half, 2 = quarter: the AO factor and its resolve follow this.
  const uint32_t ao_scale = shader_injection.ao_resolution > 1.5f
                                ? 4u
                                : (shader_injection.ao_resolution > 0.5f ? 2u : 1u);
  shader_injection.ao_scale = static_cast<float>(ao_scale);
  const uint32_t ao_width = (frame.width + ao_scale - 1u) / ao_scale;
  const uint32_t ao_height = (frame.height + ao_scale - 1u) / ao_scale;

  if (EnsureLightTarget(device, &target, ao_width, ao_height)
      && EnsureLightPass(device, &pass, "mgsv: AO pass created", __mgsv_ao, ao_width,
                         ao_height)
      && EnsureLightPass(device, &resolve_pass, "mgsv: AO resolve created",
                         __mgsv_ao_resolve, frame.width, frame.height)) {
    BuildDepthMips(cmd_list, frame.depth);
    const auto scratch0 = GetFallbackUav(cmd_list, 0);
    const auto scratch1 = GetFallbackUav(cmd_list, 1);
    const auto scratch2 = GetFallbackUav(cmd_list, 2);
    const std::array<reshade::api::resource_view, 7> sources = {
        frame.depth,                 // t0
        frame.normal,                // t1
        GetDepthMipView(cmd_list),   // t2, the addon's chain or the stand-in
        GetFallbackView(cmd_list),   // t3, unused by the AO
        GetFallbackView(cmd_list),   // t4, unused by the AO
        GetFallbackView(cmd_list),   // t5, unused by the AO
        GetFallbackView(cmd_list),   // t6, unused by the AO
    };
    DispatchLightPass(cmd_list, &pass, sources, target.uav, scratch0, scratch1, scratch2,
                      ao_width, ao_height);

    const std::array<reshade::api::resource_view, 7> resolve_sources = {
        target.view,                 // t0
        frame.depth,                 // t1, joint bilateral upsample
        GetFallbackView(cmd_list),   // t2, unused
        GetFallbackView(cmd_list),   // t3, unused
        GetFallbackView(cmd_list),   // t4, unused
        GetFallbackView(cmd_list),   // t5, unused
        GetFallbackView(cmd_list),   // t6, unused
    };
    DispatchLightPass(cmd_list, &resolve_pass, resolve_sources, frame.light, scratch0, scratch1,
                      scratch2, frame.width, frame.height);
  }
}

inline void Destroy(reshade::api::device* device) {
  DestroyDepthMipChain(device);
  DestroyLightPass(device, &pass);
  DestroyLightPass(device, &resolve_pass);
  DestroyLightTarget(device, &target);
}

inline bool settings_appended = false;

inline void AppendSettings(renodx::utils::settings::Settings& settings) {
  if (settings_appended) return;
  settings_appended = true;
  renodx::utils::settings::Settings additions = {
      new renodx::utils::settings::Setting{
          .key = "FxAmbientOcclusion",
          .binding = &shader_injection.ao_enabled,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 1.f,
          .label = "Visibility-Bitmask Ambient Occlusion",
          .section = "Ambient Occlusion",
          .tooltip = "Adds shading where nearby surfaces block light. This also darkens directly lit surfaces.",
      },
      new renodx::utils::settings::Setting{
          .key = "FxAmbientOcclusionStrength",
          .binding = &shader_injection.ao_strength,
          .default_value = 100.f,
          .label = "AO Strength",
          .section = "Ambient Occlusion",
          .tooltip = "Controls how strongly the added shading darkens the scene. Values above 100 increase the darkening.",
          .max = 400.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [] { return shader_injection.ao_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxAmbientOcclusionSamplingRadius",
          .binding = &shader_injection.ao_radius,
          .default_value = 70.f,
          .label = "AO Sampling Radius",
          .section = "Ambient Occlusion",
          .tooltip = "Controls how far around each pixel the effect searches for nearby surfaces. Higher values create broader shading.",
          .max = 100.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [] { return shader_injection.ao_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxAmbientOcclusionThicknessScale",
          .binding = &shader_injection.ao_thickness,
          .default_value = 800.f,
          .label = "AO Thickness Scale",
          .section = "Ambient Occlusion",
          .tooltip = "Controls how thick objects are assumed to be. Higher values block more light but can over-darken thin objects.",
          .max = 2000.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [] { return shader_injection.ao_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxAmbientOcclusionDepthFadeScale",
          .binding = &shader_injection.ao_fade,
          .default_value = 100.f,
          .label = "AO Depth Fade Scale",
          .section = "Ambient Occlusion",
          .tooltip = "Controls how quickly the shading fades with depth. Higher values keep it visible farther from the camera.",
          .max = 100.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [] { return shader_injection.ao_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxAmbientOcclusionSliceCount",
          .binding = &shader_injection.ao_slices,
          .default_value = 2.f,
          .label = "AO Slice Count",
          .section = "Ambient Occlusion (Advanced)",
          .tooltip = "Controls how many directions around each pixel are checked. Higher values improve coverage but cost more GPU time.",
          .max = 8.f,
          .is_visible = [] { return shader_injection.ao_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxAmbientOcclusionSamplesPerSide",
          .binding = &shader_injection.ao_steps,
          .default_value = 6.f,
          .label = "AO Samples per Side",
          .section = "Ambient Occlusion (Advanced)",
          .tooltip = "Controls how many depth samples are taken on each side of every slice. Higher values help find thin objects but cost more GPU time.",
          .max = 20.f,
          .is_visible = [] { return shader_injection.ao_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxAmbientOcclusionResolution",
          .binding = &shader_injection.ao_resolution,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 1.f,
          .label = "AO Resolution",
          .section = "Ambient Occlusion (Advanced)",
          .tooltip = "Sets the resolution used to calculate the shading. Lower resolutions are faster but can lose detail.",
          .labels = {"Full", "Half", "Quarter"},
          .is_visible = [] { return shader_injection.ao_enabled != 0.f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxAmbientOcclusionDepthMipSampling",
          .binding = &depth_mips_enabled,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 1.f,
          .label = "AO Depth Mip Sampling",
          .section = "Ambient Occlusion (Advanced)",
          .tooltip = "Uses lower-resolution depth for distant samples to help catch objects between sampling steps.",
          .is_visible = [] { return shader_injection.ao_enabled != 0.f; },
      },
  };
  InsertSettingsBeforeSection(settings, "Options", additions);
}

}  // namespace ao
