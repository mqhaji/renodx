// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

#ifndef SRC_GAMES_MGSV_LIGHTING_HPP_
#define SRC_GAMES_MGSV_LIGHTING_HPP_

#include <d3d11.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
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
#include "../reconstruction/resolution.hpp"
#include "../shared.h"
#include "ambient_diffuse.hpp"
#include "environment_specular.hpp"
#include "temporal_inputs.hpp"

// The scatter hook at the end of this file validates native inputs and schedules
// one submitted lighting chain. ao::Apply records its trace, temporal, spatial,
// and application stages. The shared types here own the resources they reuse.
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

// Views borrowed from this callback. Keeping a view alive does not keep its pixels unchanged.
struct LightFrame {
  reshade::api::resource_view depth = {0};
  reshade::api::resource_view normal = {0};
  reshade::api::resource_view light = {0};
  reshade::api::resource_view light_source = {0};
  reshade::api::resource_view albedo = {0};
  reshade::api::resource_view specular = {0};
  reshade::api::resource_view scattering_copy = {0};
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
inline std::shared_mutex noise_mutex;
inline reshade::api::device* noise_device = nullptr;

inline constexpr size_t NATIVE_LIGHT_VIEW_CACHE_SIZE = 4u;

struct NativeLightView {
  // Identity only; the UAV below owns a reference to this texture.
  ID3D11Resource* resource = nullptr;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> view;
};

// Two native images are used per invocation. A small bounded cache also covers
// alternating scene targets without retaining every texture seen by the addon.
struct NativeLightViews {
  std::shared_mutex mutex;
  std::array<NativeLightView, NATIVE_LIGHT_VIEW_CACHE_SIZE> entries;
  reshade::api::device* device = nullptr;
  uint64_t generation = 0u;
  size_t next_entry = 0u;
};

inline NativeLightViews native_light_views;

inline void ResetNativeLightViews(reshade::api::device* device) {
  std::array<NativeLightView, NATIVE_LIGHT_VIEW_CACHE_SIZE> retired;
  {
    const std::unique_lock lock(native_light_views.mutex);
    if (native_light_views.device != device) return;
    retired.swap(native_light_views.entries);
    native_light_views.device = nullptr;
    native_light_views.generation = 0u;
    native_light_views.next_entry = 0u;
  }
  // A view retains its native texture. Release outside the lock so resource
  // destruction callbacks cannot reenter a locked cache.
}

// Soft shadows and GT-VBGI share the same immutable texture and frame phase.
// Return an owned view so device retirement cannot invalidate a borrowed handle.
inline Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetNoiseView(reshade::api::device* device) {
  if (inputs::ReadSelection(device).device == nullptr) return {};
  {
    const std::shared_lock lock(noise_mutex);
    if (noise_device == device && noise_view.handle != 0) {
      return Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>(reinterpret_cast<ID3D11ShaderResourceView*>(noise_view.handle));  // NOLINT(performance-no-int-to-ptr)
    }
    if (noise_creation_failed || (noise_device != nullptr && noise_device != device)) return {};
  }
  const std::unique_lock lock(noise_mutex);
  if (noise_device != nullptr && noise_device != device) return {};
  noise_device = device;
  if (noise_view.handle == 0 && !noise_creation_failed) {
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
    if (device->create_resource(noise_desc, initial_data.data(), noise_desc.usage, &noise_resource)
        && device->create_resource_view(noise_resource, reshade::api::resource_usage::shader_resource,
                                        reshade::api::resource_view_desc(reshade::api::resource_view_type::texture_2d_array,
                                                                         reshade::api::format::r8g8_unorm, 0u, 1u, 0u, 32u),
                                        &noise_view)) {
      LogLight(reshade::log::level::info, "IS-FAST noise created; 128x128x32 RG8_UNORM");
    } else {
      if (noise_resource.handle != 0) {
        device->destroy_resource(noise_resource);
        noise_resource = {0};
      }
      static std::atomic_flag failed = {};
      noise_creation_failed = true;
      LogLightOnce(&failed, reshade::log::level::warning, "IS-FAST noise unavailable until device recreation; PCSS uses a fixed disk and GT-VBGI is skipped");
    }
  }
  if (noise_view.handle == 0) return {};
  return Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>(reinterpret_cast<ID3D11ShaderResourceView*>(noise_view.handle));  // NOLINT(performance-no-int-to-ptr)
}

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
  return true;
}

inline bool EnsureLightPassLayout(reshade::api::device* device) {
  if (light_pass_layout.handle != 0) return true;

  std::array<reshade::api::pipeline_layout_param, 3> params = {};
  params[0].type = reshade::api::pipeline_layout_param_type::push_descriptors;
  params[0].push_descriptors.count = 12u;  // t0-t11; input roles vary by pass
  params[0].push_descriptors.type = reshade::api::descriptor_type::texture_shader_resource_view;
  params[0].push_descriptors.dx_register_index = 0u;
  params[0].push_descriptors.dx_register_space = 0u;
  params[1].type = reshade::api::pipeline_layout_param_type::push_descriptors;
  params[1].push_descriptors.count = 5u;  // u0-u4; independent signal histories share one dispatch
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
  ResetNativeLightViews(device);
  const std::unique_lock lock(noise_mutex);
  if (noise_device != nullptr && noise_device != device) return;
  noise_device = nullptr;
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
inline std::array<reshade::api::resource, 4> fallback_uav_resources = {};
inline std::array<reshade::api::resource_view, 4> fallback_uavs = {};
inline Microsoft::WRL::ComPtr<ID3D11SamplerState> fallback_point_wrap;

inline void DestroyFallbackViews(reshade::api::device* device) {
  fallback_point_wrap.Reset();
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
  if (fallback_srv.handle != 0 && fallback_point_wrap != nullptr
      && std::ranges::all_of(fallback_uavs, [](const auto& view) { return view.handle != 0; })) {
    return true;
  }
  DestroyFallbackViews(device);

  const D3D11_SAMPLER_DESC sampler = {
      .Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
      .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
      .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
      .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
      .MaxAnisotropy = 1u,
      .ComparisonFunc = D3D11_COMPARISON_ALWAYS,
      .MaxLOD = D3D11_FLOAT32_MAX,
  };
  if (FAILED(reinterpret_cast<ID3D11Device*>(device->get_native())->CreateSamplerState(&sampler, &fallback_point_wrap))) return false;  // NOLINT(performance-no-int-to-ptr)

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

// Bind every input and output for this pass. ApplyLightPasses restores game state
// after the complete lighting chain, not between individual dispatches.
inline void DispatchLightPass(reshade::api::command_list* cmd_list, LightPass* pass,
                              const ShaderInjectData& constants,
                              const std::array<reshade::api::resource_view, 12>& sources,
                              reshade::api::resource_view target,
                              reshade::api::resource_view second_target,
                              reshade::api::resource_view third_target,
                              reshade::api::resource_view fourth_target,
                              reshade::api::resource_view fifth_target,
                              uint32_t width, uint32_t height) {
  cmd_list->bind_pipeline(reshade::api::pipeline_stage::all_compute, pass->pipeline);
  const auto bind_sources = [cmd_list](const std::array<reshade::api::resource_view, 12>& inputs) {
    cmd_list->push_descriptors(
        reshade::api::shader_stage::all_compute, light_pass_layout, 0u,
        {
            .table = {0u},
            .binding = 0u,
            .array_offset = 0u,
            .count = static_cast<uint32_t>(inputs.size()),
            .type = reshade::api::descriptor_type::texture_shader_resource_view,
            .descriptors = inputs.data(),
        });
  };
  // Retire prior reads before reusing ping-pong textures as writable outputs.
  // The surrounding game state is restored once after the complete chain.
  bind_sources({});
  const std::array<reshade::api::resource_view, 5> targets = {
      target,  // u0
      second_target,
      third_target,
      fourth_target,
      fifth_target,
  };
  cmd_list->push_descriptors(
      reshade::api::shader_stage::all_compute, light_pass_layout, 1u,
      {
          .table = {0u},
          .binding = 0u,
          .array_offset = 0u,
          .count = static_cast<uint32_t>(targets.size()),
          .type = reshade::api::descriptor_type::texture_unordered_access_view,
          .descriptors = targets.data(),
      });
  bind_sources(sources);
  cmd_list->push_constants(
      reshade::api::shader_stage::all_compute, light_pass_layout, 2u, 0u,
      static_cast<uint32_t>(sizeof(constants) / sizeof(uint32_t)), &constants);
  cmd_list->dispatch((width + 7u) / 8u, (height + 7u) / 8u, 1u);
}

}  // namespace mgsv::lighting

// Optional shared depth reductions and the specular visibility targets.
// AO itself travels in the alpha channel of il's diffuse-lighting targets.
namespace mgsv::ao {

using lighting::DestroyLightPass;
using lighting::DestroyLightTarget;
using lighting::DispatchLightPass;
using lighting::EnsureFallbackViews;
using lighting::EnsureLightPass;
using lighting::EnsureLightTarget;
using lighting::LightPass;
using lighting::LightTarget;
using lighting::LogLight;
using lighting::LogLightOnce;

inline LightTarget specular_visibility_target;
inline std::array<LightTarget, 2> specular_temporal_targets;
inline std::array<LightTarget, 2> specular_filter_targets;
inline uint32_t temporal_status = UINT32_MAX;
inline uint32_t temporal_status_reports = 0u;

// Reverse-Z reductions keep the nearest depth in each footprint. Distant trace
// steps can then detect thin geometry between their samples. Odd edge pixels
// join the last footprint; the trace finds its full-resolution color owner.
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

inline void DestroyDepthMipTargets(reshade::api::device* device) {
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
  depth_mip_chain.width = 0;
  depth_mip_chain.height = 0;
}

inline bool EnsureDepthMipChain(reshade::api::device* device, uint32_t width, uint32_t height) {
  if (depth_mip_chain.ready
      && depth_mip_chain.width == width
      && depth_mip_chain.height == height) {
    return true;
  }
  // Texture dimensions do not affect the shader or layout. Keep both alive
  // across scene resizes and rebuild only the images and views here.
  DestroyDepthMipTargets(device);

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
  if (depth_mip_chain.layout.handle == 0 && !device->create_pipeline_layout(static_cast<uint32_t>(params.size()), params.data(), &depth_mip_chain.layout)) {
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
  if (depth_mip_chain.pipeline.handle == 0 && !device->create_pipeline(depth_mip_chain.layout, 1, &subobject, &depth_mip_chain.pipeline)) {
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
  // Each level reads only the preceding level. Bind the new output first to
  // release the previous UAV before exposing that same mip as an input.
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
  DestroyDepthMipTargets(device);
  if (depth_mip_chain.pipeline.handle != 0) {
    device->destroy_pipeline(depth_mip_chain.pipeline);
    depth_mip_chain.pipeline = {0};
  }
  if (depth_mip_chain.layout.handle != 0) {
    device->destroy_pipeline_layout(depth_mip_chain.layout);
    depth_mip_chain.layout = {0};
  }
  DestroyLightTarget(device, &specular_visibility_target);
  for (auto& target : specular_temporal_targets) {
    DestroyLightTarget(device, &target);
  }
  for (auto& target : specular_filter_targets) {
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
using lighting::LightPass;
using lighting::LightTarget;
using lighting::LogLight;
using lighting::LogLightOnce;

inline LightPass gather_pass;
inline LightPass add_pass;
inline LightPass temporal_pass;
inline LightPass filter_pass;

// RGB is diffuse GI; alpha is ambient visibility. Each keeps its own validity.
inline LightTarget indirect_diffuse_target;
inline LightTarget diagnostic_target;
inline std::array<LightTarget, 2> temporal_targets;
inline std::array<LightTarget, 2> geometry_targets;
inline std::array<LightTarget, 2> filter_targets;
inline std::array<lighting::temporal_inputs::Scene, 2> history_scenes;

// Updated only by the serialized immediate submission callback. These signatures
// invalidate each signal independently when its trace settings change. The slot
// identifies temporal output before spatial blur, never a filtered image.
struct History {
  std::array<float, 11> parameters = {};
  std::array<float, 11> ao_parameters = {};
  std::array<float, 10> specular_parameters = {};
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
  bool specular_valid = false;
};
inline History history;
inline uint32_t temporal_status = UINT32_MAX;
inline uint32_t temporal_status_reports = 0u;

inline void Destroy(reshade::api::device* device) {
  DestroyLightPass(device, &gather_pass);
  DestroyLightPass(device, &add_pass);
  DestroyLightPass(device, &temporal_pass);
  DestroyLightPass(device, &filter_pass);
  DestroyLightTarget(device, &indirect_diffuse_target);
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

// Settings keep pointers into this storage, so it must outlive the registry.
inline void AppendSettings(renodx::utils::settings::Settings* registry, ShaderInjectData* shader_injection) {
  if (std::ranges::any_of(*registry, [](const auto* setting) {
        return setting->key == "FxIndirectLightingEnabled";
      })) return;
  const auto is_temporal_visible = [shader_injection] {
    return shader_injection->gt_enabled >= 0.5f && shader_injection->gt_temporal_enabled >= 0.5f;
  };
  const auto is_spatial_visible = [shader_injection] {
    return shader_injection->gt_enabled >= 0.5f && shader_injection->gt_spatial_enabled >= 0.5f;
  };
  renodx::utils::settings::Settings additions = {
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingEnabled",
          .binding = &shader_injection->gt_enabled,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 1.f,
          .label = "Screen-Space Lighting",
          .section = "Indirect Lighting",
          .tooltip = "Adds ambient occlusion, bounced light, and occlusion of environment reflections. Off restores native lighting.",
          .labels = {"Off", "GT-VBGI"},
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingAOStrength",
          .binding = &shader_injection->ao_strength,
          .default_value = 100.f,
          .label = "Ambient Occlusion Strength",
          .section = "Indirect Lighting",
          .tooltip = "Controls how strongly nearby geometry blocks ambient diffuse light. Direct light keeps its own shadows, and added GI is not darkened again.",
          .max = 100.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [shader_injection] { return shader_injection->gt_enabled >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingGIStrength",
          .binding = &shader_injection->gi_strength,
          .default_value = 100.f,
          .label = "Global Illumination Strength",
          .section = "Indirect Lighting",
          .tooltip = "Controls how much bounced light is added to the scene.",
          .max = 100.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [shader_injection] { return shader_injection->gt_enabled >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingSpecularOcclusionStrength",
          .binding = &shader_injection->specular_occlusion_strength,
          .default_value = 100.f,
          .label = "Specular Occlusion Strength",
          .section = "Indirect Lighting",
          .tooltip = "Controls occlusion of environment reflections. 0% keeps native environment occlusion; 100% uses visibility-bitmask occlusion where supported. Qualified direct highlights use their own shadows. Separate rain reflections retain native occlusion.",
          .max = 100.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = [shader_injection] { return shader_injection->gt_enabled >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingResolution",
          .binding = &shader_injection->gt_resolution,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 1.f,
          .label = "Lighting Resolution",
          .section = "Indirect Lighting",
          .tooltip = "Fraction of the full-resolution pixel count used by the shared lighting trace. 1/4 halves each axis; 1/16 divides each axis by 4; 1/64 divides each axis by 8. Lower resolutions can lose thin geometry and contact detail. Search radii stay in full-resolution pixels.",
          .labels = {"Full", "1/4", "1/16", "1/64"},
          .is_visible = [shader_injection] { return shader_injection->gt_enabled >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingSearchRadiusPixels",
          .binding = &shader_injection->gt_search_radius_pixels,
          .default_value = 2000.f,
          .label = "Search Radius",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Search distance in output pixels for AO, GI, and specular occlusion. Converted to the current render size when upscaling. Larger values reach farther surfaces but spread samples farther apart.",
          .min = 1.f,
          .max = 4096.f,
          .format = "%.0f px",
          .is_visible = [shader_injection] { return shader_injection->gt_enabled >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingSurfaceThickness",
          .binding = &shader_injection->gt_surface_thickness,
          .default_value = 0.15f,
          .label = "Surface Thickness",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Depth assigned behind each visible surface for AO, GI, and specular occlusion. Higher values block more directions and can make thin surfaces look too solid. Uses the game's depth units.",
          .min = 0.f,
          .max = 0.5f,
          .format = "%.4f",
          .is_visible = [shader_injection] { return shader_injection->gt_enabled >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingSurfaceOffset",
          .binding = &shader_injection->gt_surface_offset,
          .default_value = 0.0009765625f,
          .label = "Surface Offset",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Moves the search start slightly away from the surface to reduce self-occlusion. Higher values can lose contact detail. Uses the game's position units.",
          .min = 0.f,
          .max = 0.01f,
          .format = "%.6f",
          .is_visible = [shader_injection] { return shader_injection->gt_enabled >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingSamplingDirections",
          .binding = &shader_injection->gt_slice_count,
          .default_value = 3.f,
          .label = "Sampling Directions",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Search lines through each pixel, sampled on both sides for AO, GI, and specular occlusion. More directions improve coverage but cost more GPU time.",
          .min = 1.f,
          .max = 8.f,
          .is_visible = [shader_injection] { return shader_injection->gt_enabled >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingSamplesPerSide",
          .binding = &shader_injection->gt_samples_per_side,
          .default_value = 40.f,
          .label = "Samples per Side",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Depth samples on each side of every search line. All three lighting effects share them. More samples find smaller surfaces but cost more GPU time.",
          .min = 1.f,
          .max = 64.f,
          .is_visible = [shader_injection] { return shader_injection->gt_enabled >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingDepthMipsEnabled",
          .binding = &shader_injection->gt_depth_mips_enabled,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 0.f,
          .label = "Coarse Depth Sampling",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Uses smaller depth images for distant samples to catch surfaces between steps. GI finds the matching full-resolution pixel before reading its light. Off reads full-resolution depth throughout.",
          .is_visible = [shader_injection] { return shader_injection->gt_enabled >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingGIBackfaceRejectionEnabled",
          .binding = &shader_injection->gi_reject_backfaces,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 0.f,
          .label = "Reject Backfacing GI Sources",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Rejects bounced light from surfaces facing away from the receiver. These surfaces still block light and reflections.",
          .is_visible = [shader_injection] { return shader_injection->gt_enabled >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingTemporalAccumulationEnabled",
          .binding = &shader_injection->gt_temporal_enabled,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 1.f,
          .label = "Temporal Accumulation",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Combines matching samples from earlier frames before spatial filtering. Missing camera, motion, or submission data uses the current sample.",
          .is_visible = [shader_injection] { return shader_injection->gt_enabled >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingOcclusionHistoryWeight",
          .binding = &shader_injection->occlusion_history_weight,
          .default_value = 90.f,
          .label = "Occlusion History Weight",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Applies to AO and specular visibility. Maximum percentage of accepted history. Higher values reduce noise but can retain changes longer. Zero uses only the current sample.",
          .max = 99.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = is_temporal_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingOcclusionDepthRejection",
          .binding = &shader_injection->occlusion_depth_rejection,
          .default_value = 20.f,
          .label = "Occlusion Depth Rejection",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Applies to AO and specular visibility. Allowed history depth difference as a percentage of the expected previous depth. Lower values reject mismatched surfaces more strongly.",
          .max = 20.f,
          .parse = [](float value) { return value * 0.01f; },
          .is_visible = is_temporal_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingOcclusionNormalRejection",
          .binding = &shader_injection->occlusion_normal_rejection,
          .default_value = 0.f,
          .label = "Occlusion Normal Rejection",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Applies to AO and specular visibility. Surface-direction agreement required for full history weight. The weight fades to zero over a 0.4-wide range below this value. Specular history also checks roughness and reflected direction.",
          .max = 1.f,
          .format = "%.2f",
          .is_visible = is_temporal_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingAOHistoryClamp",
          .binding = &shader_injection->ao_history_clamp,
          .default_value = 0.1f,
          .label = "AO History Clamp",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Applies to AO. Allowed history range in standard deviations of nearby current samples. Lower values react faster; zero disables clamping.",
          .max = 4.f,
          .format = "%.2f",
          .is_visible = is_temporal_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingSpecularHistoryClamp",
          .binding = &shader_injection->specular_history_clamp,
          .default_value = 2.f,
          .label = "Specular History Clamp",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Limits specular history using nearby current visibility, measured in standard deviations. Lower values react faster; zero disables clamping. Surface and motion checks still apply.",
          .max = 4.f,
          .format = "%.2f",
          .is_visible = is_temporal_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingGIHistoryWeight",
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
          .key = "FxIndirectLightingGIDepthRejection",
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
          .key = "FxIndirectLightingGINormalRejection",
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
          .key = "FxIndirectLightingGIHistoryClamp",
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
          .is_visible = [shader_injection] { return shader_injection->gt_enabled >= 0.5f; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingFilterRadius",
          .binding = &shader_injection->gt_filter_radius,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 8.f,
          .label = "Filter Radius",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Limits sample spacing after the first pass, in lighting pixels. With one pass, Filter Width determines the footprint and this setting has no effect.",
          .min = 1.f,
          .max = 8.f,
          .format = "%d",
          .is_enabled = [shader_injection] { return shader_injection->gt_filter_passes > 1.f; },
          .is_visible = is_spatial_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingFilterPasses",
          .binding = &shader_injection->gt_filter_passes,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 3.f,
          .label = "Filter Passes",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "How many times to filter the lighting. More passes remove more noise but can soften detail and cost more GPU time.",
          .min = 1.f,
          .max = 4.f,
          .format = "%d",
          .is_visible = is_spatial_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingFilterTaps",
          .binding = &shader_injection->gt_filter_taps,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 5.f,
          .label = "Filter Width",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Width of the two-dimensional filter before sparse sampling. Each pass keeps the center and one quarter of neighboring taps. Even values round up to the next odd width.",
          .min = 3.f,
          .max = 9.f,
          .format = "%d",
          .is_visible = is_spatial_visible,
      },
      new renodx::utils::settings::Setting{
          .key = "FxIndirectLightingDebugView",
          .binding = &shader_injection->gt_debug_view,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 0.f,
          .label = "Lighting Debug View",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Replaces diffuse lighting for inspection; surface color, specular, fog, and later processing still apply. In the specular visibility views, cyan means no sampled slice crossed the reflection cone; magenta means missing inputs.",
          .labels = {"Off", "GI Lighting", "Geometry Coverage", "Accepted Source Coverage", "Valid / Invalid Results", "Source Lighting", "Temporal GI", "Accepted GI History", "Raw AO", "Temporal AO", "Filtered AO", "Accepted AO History", "Native Roughness", "Environment Specular", "Raw Specular Visibility", "Temporal Specular Visibility", "Filtered Specular Visibility", "Accepted Specular History", "Specular Confidence", "Ambient Diffuse", "Direct Diffuse", "Corrected Diffuse", "Ambient Input Validity"},
          .is_visible = [shader_injection] {
            return shader_injection->gt_enabled >= 0.5f;
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
  // 1. Translate UI units to this scene. Search radius is in output pixels;
  // trace/filter dimensions divide each render axis and round up at odd sizes.
  // All overrides stay in this invocation's settings copy.
  auto* device = cmd_list->get_device();
  const auto plan = reconstruction::resolution::Read();
  if (plan.IsScaled() && plan.render == reconstruction::resolution::Size{.width = frame.width, .height = frame.height}) {
    constants.gt_search_radius_pixels *= static_cast<float>(frame.width) / static_cast<float>(plan.output.width);
  }
  uint32_t scale = 1u;
  if (constants.gt_resolution > 2.5f) {
    scale = 8u;
  } else if (constants.gt_resolution > 1.5f) {
    scale = 4u;
  } else if (constants.gt_resolution > 0.5f) {
    scale = 2u;
  }
  constants.gt_resolution_divisor = static_cast<float>(scale);

  const uint32_t present = lighting::noise_frame.load(std::memory_order_relaxed);
  constants.gt_noise_frame = static_cast<float>(present & 31u);
  const bool new_submission = history_allowed && temporal.valid
                              && (temporal.submission != il::history.submission
                                  || temporal.generation != il::history.generation);
  constants.gi_temporal_active = (new_submission && constants.gt_temporal_enabled >= 0.5f
                                          && constants.gi_history_weight > 0.f
                                      ? 1.f
                                      : 0.f);
  constants.gi_history_valid = 0.f;
  constants.ao_temporal_active = (new_submission && constants.gt_temporal_enabled >= 0.5f
                                          && constants.occlusion_history_weight > 0.f
                                      ? 1.f
                                      : 0.f);
  constants.ao_history_valid = 0.f;
  constants.gt_current_jitter_x = temporal.current_jitter_uv[0];
  constants.gt_current_jitter_y = temporal.current_jitter_uv[1];
  constants.gt_previous_jitter_x = il::history.jitter_uv[0];
  constants.gt_previous_jitter_y = il::history.jitter_uv[1];
  constants.gt_velocity_jitter_scale = temporal.velocity_projection_jitter_scale;
  const auto noise = lighting::GetNoiseView(device);
  if (noise == nullptr) return;
  const uint32_t width = (frame.width + scale - 1u) / scale;
  const uint32_t height = (frame.height + scale - 1u) / scale;
  const bool size_changed = il::indirect_diffuse_target.width != width || il::indirect_diffuse_target.height != height;

  if (!EnsureLightTarget(device, &il::indirect_diffuse_target, width, height)
      || !EnsureLightPass(device, &il::gather_pass, "mgsv: GT-VBGI trace created", __gtvbgi)
      || !EnsureLightPass(device, &il::add_pass, "mgsv: GT-VBGI application created", __gtvbgi_add)
      || !EnsureFallbackViews(device)) {
    static std::atomic_flag failed = {};
    LogLightOnce(&failed, reshade::log::level::warning,
                 "GT-VBGI setup failed; skipping custom lighting; effect=", width, "x", height);
    return;
  }

  if (size_changed) {
    // One message distinguishes a scene resize from a change to Lighting Resolution.
    LogLight(reshade::log::level::info, "GT-VBGI resolution: scene=", frame.width, "x", frame.height,
             " trace=", width, "x", height, " axis_divisor=", scale);
  }

  const bool diagnostic_requested = (constants.gt_debug_view >= 2.f && constants.gt_debug_view <= 11.f)
                                    || constants.gt_debug_view == 17.f;
  const bool diagnostic_ready = diagnostic_requested && EnsureLightTarget(device, &il::diagnostic_target, width, height);
  if (diagnostic_requested && !diagnostic_ready) {
    constants.gt_debug_view = 0.f;
    static std::atomic_flag failed = {};
    LogLightOnce(&failed, reshade::log::level::warning,
                 "Lighting debug texture unavailable; normal lighting remains enabled");
  }

  // 2. Match the environment capture to this specular/depth pair. Its material
  // input supplies roughness. Missing specular data disables only that signal;
  // the diffuse trace can still run with the already qualified native inputs.
  Microsoft::WRL::ComPtr<ID3D11Resource> specular_resource;
  Microsoft::WRL::ComPtr<ID3D11Resource> depth_resource;
  reinterpret_cast<ID3D11ShaderResourceView*>(frame.specular.handle)->GetResource(&specular_resource);  // NOLINT(performance-no-int-to-ptr)
  reinterpret_cast<ID3D11ShaderResourceView*>(frame.depth.handle)->GetResource(&depth_resource);        // NOLINT(performance-no-int-to-ptr)
  auto environment = lighting::environment_specular::Read(device, specular_resource.Get());
  if (environment != nullptr && environment->depth.Get() != depth_resource.Get()) {
    environment.reset();
  }
  constants.gt_specular_active = 0.f;
  if (environment != nullptr
      && environment->material != nullptr && environment->srv != nullptr) {
    D3D11_SHADER_RESOURCE_VIEW_DESC view = {};
    environment->material->GetDesc(&view);
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    environment->material->GetResource(&resource);
    if (view.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D && view.Texture2D.MostDetailedMip == 0u
        && SUCCEEDED(resource.As(&texture))) {
      D3D11_TEXTURE2D_DESC description = {};
      texture->GetDesc(&description);
      if (description.Width == frame.width && description.Height == frame.height
          && description.ArraySize == 1u && description.SampleDesc.Count == 1u
          && (view.Format == DXGI_FORMAT_R8G8B8A8_UNORM || view.Format == DXGI_FORMAT_B8G8R8A8_UNORM
              || view.Format == DXGI_FORMAT_R16G16B16A16_FLOAT || view.Format == DXGI_FORMAT_R32G32B32A32_FLOAT)
          && EnsureLightTarget(device, &specular_visibility_target, width, height)) {
        constants.gt_specular_active = 1.f;
      }
    }
  }
  if (constants.gt_specular_active < 0.5f) {
    static std::atomic_flag failed = {};
    LogLightOnce(&failed, reshade::log::level::warning, "Bitmask specular inputs or output unavailable; keeping native specular occlusion");
  }
  const bool depth_mips_requested = constants.gt_depth_mips_enabled != 0.f;
  const bool depth_mips_ready = BuildDepthMips(cmd_list, frame.depth, depth_mips_requested);
  constants.gt_depth_mips_enabled = (depth_mips_ready ? 1.f : 0.f);
  // Setup above qualifies these handles once for the complete chain.
  const auto fallback_input = lighting::fallback_srv;
  const auto fallback_outputs = lighting::fallback_uavs;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  const bool immediate = context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;
  // 3. Qualify history before reading it. A matching size alone is insufficient:
  // require the same camera, viewport, depth allocation, and device generation.
  // AO strength belongs in its signature because the trace applies it; GI and
  // specular strengths are applied later and do not change their stored samples.
  const std::array<float, 11> parameters = {
      constants.gt_search_radius_pixels,
      constants.gt_surface_thickness,
      constants.gt_surface_offset,
      constants.gt_slice_count,
      constants.gt_samples_per_side,
      constants.gt_depth_mips_enabled,
      constants.gi_reject_backfaces,
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
  const std::array<float, 10> specular_parameters = {
      constants.gt_search_radius_pixels,
      constants.gt_surface_thickness,
      constants.gt_surface_offset,
      constants.gt_slice_count,
      constants.gt_samples_per_side,
      constants.gt_depth_mips_enabled,
      constants.gt_resolution,
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
  if (immediate && (geometry_changed || il::history.specular_parameters != specular_parameters || constants.gt_specular_active < 0.5f)) {
    il::history.specular_valid = false;
  }
  // Alternate two history slots so no dispatch reads its own output. Camera
  // copies follow those slots and preserve the native constant-buffer range.
  const uint32_t write_slot = 1u - il::history.slot;
  // An unsplit deferred fallback must never write submitted history textures.
  bool temporal_ready = immediate && EnsureLightPass(device, &il::temporal_pass, "mgsv: combined AO/GI temporal pass created", __gtvbgi_temporal);
  for (uint32_t index = 0u; index < 2u && temporal_ready; ++index) {
    temporal_ready = EnsureLightTarget(device, &il::temporal_targets[index], width, height)
                     && EnsureLightTarget(device, &il::geometry_targets[index], width, height,
                                          reshade::api::format::r32g32_uint);
  }
  bool specular_temporal_ready = temporal_ready && constants.gt_specular_active >= 0.5f;
  for (uint32_t index = 0u; index < 2u && specular_temporal_ready; ++index) {
    specular_temporal_ready = EnsureLightTarget(device, &specular_temporal_targets[index], width, height);
  }
  if (temporal_ready && constants.gt_specular_active >= 0.5f && !specular_temporal_ready) {
    static std::atomic_flag failed = {};
    LogLightOnce(&failed, reshade::log::level::warning, "Bitmask specular history allocation failed; using current masses");
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
  // Present supplies only the phase-gap check. A new qualified producer
  // submission is also required, so repeated hooks cannot advance history.
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
  constants.gt_specular_history_valid = (specular_temporal_ready && constants.ao_temporal_active >= 0.5f
                                                 && il::history.specular_valid && present - il::history.frame == 1u
                                                 && il::history_scenes[il::history.slot].buffer != nullptr
                                             ? 1.f
                                             : 0.f);
  for (const bool ambient_occlusion : {true, false}) {
    uint32_t status = 0u;
    const bool requested = (ambient_occlusion
                                ? constants.gt_temporal_enabled >= 0.5f && constants.occlusion_history_weight > 0.f
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
    filter_ready = EnsureLightTarget(device, &il::filter_targets[index], width, height);
  }
  bool specular_filter_ready = filter_ready && constants.gt_specular_active >= 0.5f;
  for (uint32_t index = 0u; index < 2u && specular_filter_ready; ++index) {
    specular_filter_ready = EnsureLightTarget(device, &specular_filter_targets[index], width, height);
  }
  if (filter_ready && constants.gt_specular_active >= 0.5f && !specular_filter_ready) {
    static std::atomic_flag failed = {};
    LogLightOnce(&failed, reshade::log::level::warning, "Bitmask specular spatial allocation failed; using unfiltered masses");
  }
  if (!filter_ready) {
    static std::atomic_flag failed = {};
    LogLightOnce(&failed, reshade::log::level::warning, "Lighting spatial setup failed; using unfiltered AO and GI");
  }
  // 4. Trace geometry once for AO, GI, and specular visibility. Diffuse RGB and
  // AO alpha share u1. Unused slots keep distinct placeholders in the common
  // layout; they must never alias the real inputs or writable destinations.
  const std::array<reshade::api::resource_view, 12> sources = {
      frame.depth,
      frame.normal,
      (depth_mip_chain.ready ? depth_mip_chain.view : fallback_input),
      frame.light_source,
      frame.albedo,
      fallback_input,
      reshade::api::resource_view{reinterpret_cast<uintptr_t>(noise.Get())},
      constants.gt_specular_active >= 0.5f ? reshade::api::resource_view{reinterpret_cast<uintptr_t>(environment->material.Get())} : fallback_input,
      constants.gt_specular_active >= 0.5f ? reshade::api::resource_view{reinterpret_cast<uintptr_t>(environment->srv.Get())} : fallback_input,
      fallback_input,
      fallback_input,
  };
  DispatchLightPass(cmd_list, &il::gather_pass, constants, sources,
                    fallback_outputs[0], il::indirect_diffuse_target.uav,
                    diagnostic_ready ? il::diagnostic_target.uav : fallback_outputs[1],
                    constants.gt_specular_active >= 0.5f ? specular_visibility_target.uav : fallback_outputs[2],
                    fallback_outputs[3], width, height);
  if (constants.gt_specular_active >= 0.5f) {
    static std::atomic_flag recorded = {};
    LogLightOnce(&recorded, reshade::log::level::info, "Bitmask specular commands recorded; effect=", width, "x", height,
                 "; shared temporal and spatial passes, native fallback; inspect Accepted Specular History for per-pixel acceptance");
  }

  auto lighting_source = il::indirect_diffuse_target.view;
  auto specular_source = (constants.gt_specular_active >= 0.5f ? specular_visibility_target.view : fallback_input);
  auto specular_temporal_source = specular_source;
  // 5. Accumulate all signals in one dispatch. The shader copies current values
  // when history is cold or disabled. Save this result before spatial filtering
  // so blur cannot grow by feeding the filtered image back every frame.
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
    const std::array<reshade::api::resource_view, 12> temporal_sources = {
        lighting_source,
        frame.depth,
        frame.normal,
        temporal.motion.velocity != nullptr
            ? reshade::api::resource_view{reinterpret_cast<uintptr_t>(temporal.motion.velocity.Get())}
            : fallback_input,
        temporal.motion.object_velocity != nullptr
            ? reshade::api::resource_view{reinterpret_cast<uintptr_t>(temporal.motion.object_velocity.Get())}
            : fallback_input,
        il::temporal_targets[il::history.slot].view,
        il::geometry_targets[il::history.slot].view,
        fallback_input,
        fallback_input,
        specular_source,
        specular_temporal_ready ? specular_temporal_targets[il::history.slot].view : fallback_input,
    };
    auto temporal_constants = constants;
    if (!specular_temporal_ready) {
      temporal_constants.gt_specular_active = 0.f;
    }
    DispatchLightPass(cmd_list, &il::temporal_pass, temporal_constants, temporal_sources,
                      il::temporal_targets[write_slot].uav, il::geometry_targets[write_slot].uav,
                      diagnostic_ready ? il::diagnostic_target.uav : fallback_outputs[1],
                      fallback_outputs[2],
                      specular_temporal_ready ? specular_temporal_targets[write_slot].uav : fallback_outputs[3],
                      width, height);
    lighting_source = il::temporal_targets[write_slot].view;
    if (specular_temporal_ready) {
      specular_source = specular_temporal_targets[write_slot].view;
      specular_temporal_source = specular_source;
    }
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
          .specular_parameters = specular_parameters,
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
          .specular_valid = specular_temporal_ready && constants.ao_temporal_active >= 0.5f,
      };
    }
  }
  // 6. Ping-pong shared spatial outputs. Spacing grows 1, 2, 4, ... lighting
  // pixels up to the radius limit. Off follows the same path with shader copies;
  // allocation failure leaves the most recent available result as the source.
  if (filter_ready) {
    const uint32_t iterations = (std::isfinite(constants.gt_filter_passes)
                                     ? static_cast<uint32_t>(std::clamp(constants.gt_filter_passes, 1.f, 4.f))
                                     : 1u);
    const uint32_t radius = (std::isfinite(constants.gt_filter_radius)
                                 ? static_cast<uint32_t>(std::clamp(constants.gt_filter_radius, 1.f, 8.f))
                                 : 2u);
    for (uint32_t index = 0u; index < iterations; ++index) {
      constants.gt_filter_step = static_cast<float>(std::min(1u << index, std::max(1u, radius / 2u)));
      const std::array<reshade::api::resource_view, 12> filter_sources = {
          lighting_source,
          frame.depth,
          frame.normal,
          fallback_input,
          specular_source,
          reshade::api::resource_view{reinterpret_cast<uintptr_t>(noise.Get())},
          fallback_input,
          fallback_input,
          fallback_input,
          fallback_input,
          fallback_input,
      };
      auto filter_constants = constants;
      if (!specular_filter_ready) {
        filter_constants.gt_specular_active = 0.f;
      }
      DispatchLightPass(cmd_list, &il::filter_pass, filter_constants, filter_sources,
                        il::filter_targets[index & 1u].uav, fallback_outputs[0],
                        constants.gt_debug_view == 10.f ? il::diagnostic_target.uav : fallback_outputs[1],
                        specular_filter_ready ? specular_filter_targets[index & 1u].uav : fallback_outputs[2],
                        fallback_outputs[3], width, height);
      lighting_source = il::filter_targets[index & 1u].view;
      if (specular_filter_ready) {
        specular_source = specular_filter_targets[index & 1u].view;
      }
    }
  }
  // 7. Reconstruct the captured native ambient separately from our lighting.
  // Its exact sampler, phase weights, and b5 range are needed to subtract only
  // ambient from total diffuse. Missing capture skips diffuse AO, not GI.
  Microsoft::WRL::ComPtr<ID3D11Resource> diffuse_resource;
  reinterpret_cast<ID3D11UnorderedAccessView*>(frame.light.handle)->GetResource(&diffuse_resource);  // NOLINT(performance-no-int-to-ptr)
  auto ambient = lighting::ambient_diffuse::Read(device, diffuse_resource.Get());
  if (ambient != nullptr && (ambient->depth.Get() != depth_resource.Get() || ambient->width != frame.width || ambient->height != frame.height)) {
    ambient.reset();
  }
  if (ambient == nullptr) {
    static std::atomic_flag reported = {};
    LogLightOnce(&reported, reshade::log::level::warning, "ambient diffuse unavailable; diffuse AO is skipped while GI and specular visibility continue");
  }
  auto* const ambient_constants = (ambient != nullptr ? ambient->constants.Get() : nullptr);
  auto* const ambient_sampler = (ambient != nullptr ? ambient->sampler.Get() : lighting::fallback_point_wrap.Get());
  Microsoft::WRL::ComPtr<ID3D11DeviceContext1> ambient_context;
  if (ambient != nullptr && SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&ambient_context)))) {
    ambient_context->CSSetConstantBuffers1(3u, 1u, &ambient_constants, &ambient->first_constant, &ambient->constant_count);
  } else {
    context->CSSetConstantBuffers(3u, 1u, &ambient_constants);
  }
  context->CSSetSamplers(0u, 1u, &ambient_sampler);
  const std::array<reshade::api::resource_view, 12> apply_sources = {
      lighting_source,
      frame.depth,
      diagnostic_ready ? il::diagnostic_target.view : fallback_input,
      fallback_input,
      frame.normal,
      environment != nullptr ? reshade::api::resource_view{reinterpret_cast<uintptr_t>(environment->srv.Get())} : fallback_input,
      environment != nullptr ? reshade::api::resource_view{reinterpret_cast<uintptr_t>(environment->material.Get())} : fallback_input,
      constants.gt_specular_active >= 0.5f ? specular_visibility_target.view : fallback_input,
      specular_temporal_source,
      specular_source,
      ambient != nullptr ? reshade::api::resource_view{reinterpret_cast<uintptr_t>(ambient->view.Get())} : fallback_input,
      ambient != nullptr ? reshade::api::resource_view{reinterpret_cast<uintptr_t>(ambient->weight_view.Get())} : fallback_input,
  };
  // Full-resolution application writes native diffuse and eligible skin-copy
  // RGB, preserving each alpha. It also resolves specular visibility/confidence
  // for deferred composition; it does not darken total specular here.
  DispatchLightPass(cmd_list, &il::add_pass, constants, apply_sources, frame.light,
                    environment != nullptr ? reshade::api::resource_view{reinterpret_cast<uintptr_t>(environment->visibility_uav.Get())} : fallback_outputs[0],
                    frame.scattering_copy.handle != 0u ? frame.scattering_copy : fallback_outputs[1],
                    fallback_outputs[2], fallback_outputs[3], frame.width, frame.height);
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
               " gi_strength=", constants.gi_strength,
               " gt_depth_mips_enabled=", constants.gt_depth_mips_enabled,
               " debug_view=", constants.gt_debug_view,
               " temporal=", constants.gt_temporal_enabled,
               " spatial=", constants.gt_spatial_enabled,
               " filter_passes=", constants.gt_filter_passes,
               "; command submission is not GPU completion");

  if (depth_mips_requested && !depth_mips_ready) {
    static std::atomic_flag mip_failed = {};
    LogLightOnce(&mip_failed, reshade::log::level::warning,
                 "shared depth mip generation unavailable; AO/GI use full-resolution depth samples");
  }
}

}  // namespace mgsv::ao

// Native composite inputs and pre-scatter lighting orchestration.
namespace mgsv::lighting {

// Called before the first scatter, with its native inputs still bound. Recording
// collects owned references and GPU constant copies. The scheduled callback runs
// after the native prefix is submitted and before either skin scatter consumes it.
inline void ApplyLightPasses(reshade::api::command_list* cmd_list, const ShaderInjectData& shader_injection) {
  if (cmd_list == nullptr || cmd_list->get_device()->get_api() != reshade::api::device_api::d3d11) return;

  ShaderInjectData constants;
  {
    const std::shared_lock lock(renodx::utils::mutex::global_mutex);
    constants = shader_injection;
    constants.gt_debug_view = std::clamp(constants.gt_debug_view, 0.f, 22.f);
  }
  if (constants.gt_enabled < 0.5f) {
    static std::atomic_flag reported = {};
    mgsv::lighting::LogLightOnce(&reported, reshade::log::level::info, "scatter hook reached; Screen-Space Lighting is Off");
    return;
  }

  // Allocation and teardown share the first-present device contract. No lighting work is admitted during resize.
  const auto shared_inputs = inputs::Read(cmd_list->get_device());
  if (shared_inputs.device == nullptr) return;

  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (context == nullptr) return;

  // Reuse UAVs for the two native destinations instead of creating them at each
  // hook. Four entries cover alternating pairs; round-robin eviction is bounded
  // and cheaper to maintain than an unbounded map of rarely reused textures.
  const auto get_light_view = [](reshade::api::device* device, uint64_t generation, ID3D11Resource* resource) {
    {
      const std::shared_lock lock(native_light_views.mutex);
      if (native_light_views.device == device && native_light_views.generation == generation) {
        for (const auto& entry : native_light_views.entries) {
          if (entry.resource == resource && entry.view != nullptr) return entry.view;
        }
      }
    }
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> created;
    const D3D11_UNORDERED_ACCESS_VIEW_DESC description = {
        .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
        .ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D,
    };
    if (FAILED(reinterpret_cast<ID3D11Device*>(device->get_native())->CreateUnorderedAccessView(resource, &description, &created))) return created;  // NOLINT(performance-no-int-to-ptr)

    // Evicted views and their textures may trigger destruction callbacks. Move
    // them out of the cache so their final Release happens after unlocking.
    std::array<NativeLightView, NATIVE_LIGHT_VIEW_CACHE_SIZE> retired;
    NativeLightView evicted;
    {
      const std::unique_lock lock(native_light_views.mutex);
      if (native_light_views.device != device || native_light_views.generation != generation) {
        retired.swap(native_light_views.entries);
        native_light_views.device = device;
        native_light_views.generation = generation;
        native_light_views.next_entry = 0u;
      }
      for (const auto& entry : native_light_views.entries) {
        if (entry.resource == resource && entry.view != nullptr) return entry.view;
      }
      const size_t index = native_light_views.next_entry;
      std::swap(evicted, native_light_views.entries[index]);
      native_light_views.entries[index] = {.resource = resource, .view = created};
      native_light_views.next_entry = (index + 1u) % native_light_views.entries.size();
    }
    return created;
  };

  // 1. Retain this scatter's inputs, rather than selecting a global latest frame.
  // The submitted callback captures these references if recording is deferred.
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normal;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> light_source;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> scattering_copy;
  Microsoft::WRL::ComPtr<ID3D11Resource> copied_resource;
  context->CSGetShaderResources(1u, 1u, depth.GetAddressOf());
  context->CSGetShaderResources(2u, 1u, normal.GetAddressOf());
  context->CSGetShaderResources(0u, 1u, light_source.GetAddressOf());
  if (light_source != nullptr) {
    light_source->GetResource(&copied_resource);
    scattering_copy = get_light_view(cmd_list->get_device(), shared_inputs.generation, copied_resource.Get());
  }
  if (depth == nullptr || normal == nullptr || scattering_copy == nullptr) {
    static std::atomic_flag reported = {};
    mgsv::lighting::LogLightOnce(&reported, reshade::log::level::warning, "skipping lighting: missing scatter bindings depth=",
                                 depth != nullptr, " normal=", normal != nullptr, " copied_light=", scattering_copy != nullptr);
    return;
  }

  D3D11_UNORDERED_ACCESS_VIEW_DESC light_view_desc = {};
  scattering_copy->GetDesc(&light_view_desc);
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
  scattering_copy->GetResource(light_resource.GetAddressOf());
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

  // 2. Reject incompatible size, device, encoding, or read/write aliases before
  // touching native lighting. A float format by itself does not prove linear RGB.
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
      // The native copy is read by the trace, then written by application.
      // Materials must remain separate from that writable image.
      if (resource.Get() == light_resource.Get() && view != light_source.Get()) return "output alias";
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

  // The first scatter reads the copied, unmodified diffuse at t0.
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
  // 3. Snapshot the scatter's projection constants in GPU order. Retaining the
  // native buffer alone is insufficient: another recorded draw can overwrite it.
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
  // 4. Resolve the original diffuse destination only after the copy's prefix is
  // submitted. Its association is consumed once, preventing repeated application.
  // Every deferred reference outlives its recorded work. The native geometry
  // constants are copied on the GPU at this insertion, not read back by the CPU.
  const auto apply = [width = light_desc.Width, height = light_desc.Height, constants, geometry, geometry_first, geometry_count,
                      depth, normal, light_source, scattering_copy,
                      albedo = shared_inputs.materials.albedo,
                      specular = shared_inputs.materials.specular,
                      generation = shared_inputs.generation, get_light_view](reshade::api::command_list* submitted, bool first_submission) {
    if (!first_submission) {
      // Cached native work may replay, but custom AO/GI must not compound on a
      // lighting image that was already corrected by this recorded insertion.
      Microsoft::WRL::ComPtr<ID3D11Resource> copied;
      light_source->GetResource(&copied);
      copied->SetPrivateDataInterface(temporal_inputs::DIFFUSE_SOURCE_GUID, nullptr);
      return;
    }
    const auto selected = inputs::Read(submitted->get_device());
    if (selected.device == nullptr || selected.generation != generation) return;
    auto* submitted_context = reinterpret_cast<ID3D11DeviceContext*>(submitted->get_native());  // NOLINT(performance-no-int-to-ptr)
    Microsoft::WRL::ComPtr<ID3D11Resource> copied;
    Microsoft::WRL::ComPtr<ID3D11Resource> diffuse;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> diffuse_texture;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> light;
    light_source->GetResource(&copied);
    UINT bytes = sizeof(ID3D11Resource*);
    if (FAILED(copied->GetPrivateData(temporal_inputs::DIFFUSE_SOURCE_GUID, &bytes, diffuse.GetAddressOf())) || diffuse == nullptr
        || FAILED(diffuse.As(&diffuse_texture))) {
      static std::atomic_flag reported = {};
      LogLightOnce(&reported, reshade::log::level::warning, "skipping lighting: diffuse copy has no qualified submitted source");
      return;
    }
    // A submitted native copy admits one lighting application, not one per
    // Present or every repeated interception of the same scattering input.
    copied->SetPrivateDataInterface(temporal_inputs::DIFFUSE_SOURCE_GUID, nullptr);
    D3D11_TEXTURE2D_DESC description = {};
    diffuse_texture->GetDesc(&description);
    if (diffuse == copied || description.Width != width || description.Height != height
        || description.ArraySize != 1u || description.MipLevels != 1u || description.SampleDesc.Count != 1u) return;
    light = get_light_view(submitted->get_device(), generation, diffuse.Get());
    if (light == nullptr) return;
    // 5. Preserve state once around the complete chain. Clear displaced graphics
    // reads as well as compute inputs before making the native images writable.
    const auto previous_state = mgsv::rendering::d3d11_compute_state::Capture(submitted);
    // The native copy can leave these lighting images bound in graphics slots.
    const std::array<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> empty = {};
    submitted_context->VSSetShaderResources(0u, static_cast<UINT>(empty.size()), empty.data());
    submitted_context->HSSetShaderResources(0u, static_cast<UINT>(empty.size()), empty.data());
    submitted_context->DSSetShaderResources(0u, static_cast<UINT>(empty.size()), empty.data());
    submitted_context->GSSetShaderResources(0u, static_cast<UINT>(empty.size()), empty.data());
    submitted_context->PSSetShaderResources(0u, static_cast<UINT>(empty.size()), empty.data());
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
        .scattering_copy = {reinterpret_cast<uintptr_t>(scattering_copy.Get())},
        .width = width,
        .height = height,
    };
    mgsv::ao::Apply(submitted_frame, submitted, constants,
                    temporal_inputs::Read(submitted->get_device(), submitted_frame.depth),
                    submitted_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);
    mgsv::rendering::d3d11_compute_state::Restore(submitted, previous_state);
  };
  if (geometry != nullptr && mgsv::reconstruction::coordinator::ScheduleLighting(cmd_list, apply)) return;

  // Missing safe submission support keeps the existing same-context chain.
  // It may spatially filter current GI, but cannot advance temporal history.
  const mgsv::reconstruction::coordinator::ExecutionGuard execution_guard;
  apply(cmd_list, true);
}

}  // namespace mgsv::lighting

#endif  // SRC_GAMES_MGSV_LIGHTING_HPP_
