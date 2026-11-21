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

#include "../../../../utils/mutex.hpp"
#include "../../../../utils/settings.hpp"
#include "../../assets/noise/fast_noise_gr8.h"
#include "../../materials/capture.hpp"
#include "../../reconstruction/resolution.hpp"
#include "../../rendering/d3d11_state.hpp"
#include "../../rendering/inputs.hpp"
#include "../../rendering/producer_records.hpp"
#include "../../rendering/submission.hpp"
#include "../../shared.h"
#include "../environment_specular.hpp"
#include "../indirect_lighting_mode.hpp"
#include "ambient_diffuse.hpp"
#include "source_preparation.hpp"

// Diffuse AO and GI with GT-VBGI, a visibility-bitmask screen-space method.
// ApplyLightPasses, at the end of this file, runs at the first native skin
// scatter: it checks the native inputs and schedules one lighting chain for
// submission. ao::Apply then records the trace and the application pass that
// updates native diffuse lighting before both scatters read it. The shared
// types here own the resources the chain reuses.
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

// Views borrowed for one chain invocation. Holding a view does not keep its pixels unchanged.
struct LightFrame {
  reshade::api::resource_view depth = {0};
  reshade::api::resource_view normal = {0};
  reshade::api::resource_view light = {0};
  reshade::api::resource_view light_source = {0};
  reshade::api::resource_view albedo = {0};
  reshade::api::resource_view specular = {0};
  reshade::api::resource_view scattering_copy = {0};
  reshade::api::resource_view prepared_source = {0};
  uint32_t width = 0;
  uint32_t height = 0;
};

struct LightPass {
  reshade::api::pipeline pipeline = {0};
};

// Every lighting stage binds this complete range so an unused slot cannot
// retain an SRV from a preceding pass. Individual shaders assign their roles.
inline constexpr uint32_t LIGHT_INPUT_COUNT = 14u;

struct LightTarget {
  reshade::api::resource resource = {0};
  reshade::api::resource_view view = {0};
  reshade::api::resource_view uav = {0};
  uint32_t width = 0;
  uint32_t height = 0;
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

// UAVs for the native diffuse images the chain writes, two per invocation. A
// small fixed cache with round-robin eviction also covers scene targets that
// alternate between frames, without holding every texture the addon has seen.
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

// PCSS soft shadows and GT-VBGI share this IS-FAST noise texture and its frame
// phase. Return an owned reference, so destroying the device's lighting
// resources cannot invalidate a handle a caller still holds.
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
      initial_data[slice] = {
          .data = const_cast<uint8_t*>(FAST_NOISE_RG8.data() + (slice * 128u * 128u * 2u)),
          .row_pitch = 256u,
          .slice_pitch = 128u * 128u * 2u,
      };
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

// RGBA16F, the format of every lighting image in this chain.
inline bool EnsureLightTarget(reshade::api::device* device, LightTarget* target, uint32_t width, uint32_t height) {
  if (target->uav.handle != 0 && target->width == width && target->height == height) return true;

  DestroyLightTarget(device, target);
  constexpr auto format = reshade::api::format::r16g16b16a16_float;

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
  params[0].push_descriptors.count = LIGHT_INPUT_COUNT;  // t0-t13
  params[0].push_descriptors.type = reshade::api::descriptor_type::texture_shader_resource_view;
  params[0].push_descriptors.dx_register_index = 0u;
  params[0].push_descriptors.dx_register_space = 0u;
  params[1].type = reshade::api::pipeline_layout_param_type::push_descriptors;
  params[1].push_descriptors.count = 3u;  // lighting, scattering copy, and diagnostics
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
  // Created once per device; the bytecode and layout do not change before teardown.
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
inline std::array<reshade::api::resource, 2> fallback_uav_resources = {};
inline std::array<reshade::api::resource_view, 2> fallback_uavs = {};
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
                              const std::array<reshade::api::resource_view, LIGHT_INPUT_COUNT>& sources,
                              reshade::api::resource_view target,
                              reshade::api::resource_view second_target,
                              reshade::api::resource_view third_target,
                              uint32_t width, uint32_t height) {
  cmd_list->bind_pipeline(reshade::api::pipeline_stage::all_compute, pass->pipeline);
  const auto bind_sources = [cmd_list](const std::array<reshade::api::resource_view, LIGHT_INPUT_COUNT>& inputs) {
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
  // Clear the previous pass's inputs before binding this pass's outputs, so no
  // texture is bound for reading and writing at the same time.
  bind_sources({});
  const std::array<reshade::api::resource_view, 3> targets = {
      target,  // u0
      second_target,
      third_target,
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

// Optional depth mips for the diffuse geometry search. AO itself is stored in
// the alpha channel of il::indirect_diffuse_target.
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

// With reverse-Z, each mip keeps the nearest depth in its footprint, so distant
// trace steps can detect thin geometry between their samples. Odd edge pixels
// join the last footprint; the trace then finds the matching full-resolution
// pixel to read color from.
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
// RGB is diffuse GI; alpha is ambient visibility. Each keeps its own validity.
inline LightTarget indirect_diffuse_target;
inline LightTarget diagnostic_target;

inline void Destroy(reshade::api::device* device) {
  DestroyLightPass(device, &gather_pass);
  DestroyLightPass(device, &add_pass);
  DestroyLightTarget(device, &indirect_diffuse_target);
  DestroyLightTarget(device, &diagnostic_target);
}

}  // namespace mgsv::il

namespace mgsv::lighting {

// Settings keep pointers into this storage, so it must outlive the registry.
inline void AppendSettings(renodx::utils::settings::Settings* registry, ShaderInjectData* shader_injection) {
  if (std::ranges::any_of(*registry, [](const auto* setting) {
        return setting->key == "IndirectLightingMode";
      })) return;
  renodx::utils::settings::Settings additions = {
      new renodx::utils::settings::Setting{
          .key = "IndirectLightingMode",
          .binding = &shader_injection->indirect_lighting_mode,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 1.f,
          .label = "Global Illumination",
          .section = "Indirect Lighting",
          .tooltip = "Enhanced adds ambient occlusion and diffuse light bounced from visible surfaces, and replaces native screen-space reflections with ray-marched reflections traced after sky, fog, and forward lighting. Rays that leave the screen keep environment lighting. It runs in the main view only and needs the RenoDX tone mapper. With Reconstruction Off the reflections are not denoised.",
          .labels = {"Vanilla", "Enhanced"},
      },
      new renodx::utils::settings::Setting{
          .key = "IndirectLightingDiffuseResolution",
          .binding = &shader_injection->lighting_resolution,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 1.f,
          .label = "Diffuse Lighting Resolution",
          .section = "Indirect Lighting",
          .tooltip = "Fraction of the full-resolution pixel count used by the shared lighting trace. 1/4 halves each axis; 1/16 divides each axis by 4; 1/64 divides each axis by 8. Lower resolutions can lose thin geometry and contact detail. Search radii stay in full-resolution pixels.",
          .labels = {"Full", "1/4", "1/16", "1/64"},
      },
      new renodx::utils::settings::Setting{
          .key = "IndirectLightingReflectionRaysPerPixel",
          .binding = &shader_injection->reflection_rays_per_pixel,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 1.f,
          .label = "Rays per Pixel",
          .section = "Indirect Lighting",
          .tooltip = "Reflection samples per input pixel. Lower values reduce tracing cost but add noise and can reduce stability in motion. Higher values do more tracing; reflection strength and material roughness stay the same.",
          .min = 1.f,
          .max = static_cast<float>(MGSV_REFLECTION_MAX_RAYS_PER_PIXEL),
          .format = "%d",
      },
      new renodx::utils::settings::Setting{
          .key = "IndirectLightingDiffuseOcclusionStrength",
          .binding = &shader_injection->ao_strength,
          .default_value = 100.f,
          .label = "Diffuse Occlusion Strength",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Controls how strongly nearby geometry blocks ambient diffuse light. Direct light keeps its own shadows, and added GI is not darkened again.",
          .max = 100.f,
          .parse = [](float value) { return value * 0.01f; },
      },
      new renodx::utils::settings::Setting{
          .key = "IndirectLightingDiffuseGIStrength",
          .binding = &shader_injection->diffuse_gi_strength,
          .default_value = 100.f,
          .label = "Diffuse GI Strength",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Controls diffuse light bounced from visible surfaces. Ambient occlusion remains active at 0%.",
          .max = 100.f,
          .parse = [](float value) { return value * 0.01f; },
      },
      new renodx::utils::settings::Setting{
          .key = "IndirectLightingReflectionStrength",
          .binding = &shader_injection->reflection_strength,
          .default_value = 100.f,
          .label = "Reflection Strength",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Blends screen-space reflections with environment lighting. Added reflected light and removed environment change together. Zero retains environment lighting without running the original SSR chain.",
          .max = 100.f,
          .parse = [](float value) { return value * 0.01f; },
      },
      new renodx::utils::settings::Setting{
          .key = "IndirectLightingSpecularOcclusionStrength",
          .binding = &shader_injection->specular_occlusion_strength,
          .default_value = 100.f,
          .label = "Specular Occlusion Strength",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Blocks environment light when a reflection ray hits nearby geometry, including hits whose screen color is unavailable. Does not darken accepted screen reflections again. Off-screen misses retain the environment. Changes image strength, not tracing cost.",
          .max = 100.f,
          .parse = [](float value) { return value * 0.01f; },
      },
      new renodx::utils::settings::Setting{
          .key = "IndirectLightingDiffuseSearchRadiusPixels",
          .binding = &shader_injection->lighting_search_radius_pixels,
          .default_value = 2000.f,
          .label = "Search Radius",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Search distance in output pixels for AO and diffuse GI. Converted to the current render size when upscaling. Larger values reach farther surfaces but spread samples farther apart.",
          .min = 1.f,
          .max = 4096.f,
          .format = "%.0f px",
      },
      new renodx::utils::settings::Setting{
          .key = "IndirectLightingDiffuseSurfaceThickness",
          .binding = &shader_injection->lighting_surface_thickness,
          .default_value = 0.15f,
          .label = "Surface Thickness",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Depth assigned behind each visible surface for AO and diffuse GI. Higher values block more directions and can make thin surfaces look too solid. Uses the game's depth units.",
          .min = 0.f,
          .max = 0.5f,
          .format = "%.4f",
      },
      new renodx::utils::settings::Setting{
          .key = "IndirectLightingDiffuseSurfaceOffset",
          .binding = &shader_injection->lighting_surface_offset,
          .default_value = 0.0009765625f,
          .label = "Surface Offset",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Moves the search start slightly away from the surface to reduce self-occlusion. Higher values can lose contact detail. Uses the game's position units.",
          .min = 0.f,
          .max = 0.01f,
          .format = "%.6f",
      },
      new renodx::utils::settings::Setting{
          .key = "IndirectLightingDiffuseSamplingDirections",
          .binding = &shader_injection->lighting_slice_count,
          .default_value = 3.f,
          .label = "Sampling Directions",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Search lines through each pixel, sampled on both sides for AO and diffuse GI. More directions improve coverage but cost more GPU time.",
          .min = 1.f,
          .max = 8.f,
      },
      new renodx::utils::settings::Setting{
          .key = "IndirectLightingDiffuseSamplesPerSide",
          .binding = &shader_injection->lighting_samples_per_side,
          .default_value = 40.f,
          .label = "Samples per Side",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Depth samples on each side of every search line, shared by AO and diffuse GI. More samples find smaller surfaces but cost more GPU time.",
          .min = 1.f,
          .max = 64.f,
      },
      new renodx::utils::settings::Setting{
          .key = "IndirectLightingDiffuseDepthMipsEnabled",
          .binding = &shader_injection->lighting_depth_mips_enabled,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 0.f,
          .label = "Coarse Depth Sampling",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Uses smaller depth images for distant samples to catch surfaces between steps. GI finds the matching full-resolution pixel before reading its light. Off reads full-resolution depth throughout.",
      },
      new renodx::utils::settings::Setting{
          .key = "IndirectLightingDiffuseGIBackfaceRejectionEnabled",
          .binding = &shader_injection->gi_reject_backfaces,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 0.f,
          .label = "Reject Backfacing GI Sources",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Rejects bounced light from surfaces facing away from the receiver. These surfaces still block light and reflections.",
      },
      new renodx::utils::settings::Setting{
          .key = "IndirectLightingDebugView",
          .binding = &shader_injection->lighting_debug_view,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 0.f,
          .label = "Lighting Debug View",
          .section = "Indirect Lighting (Advanced)",
          .tooltip = "Diffuse views inspect the AO/GI stages. Reflection views compare denoised and raw light, environment weights, input coverage, saved environment, specular visibility, and NRD validation. Sampling weights can exceed one. Reflection Coverage shows qualified inputs, not ray hits. Scene Memory shows the surfaces stored around the camera in their stored color, Behind Camera the ones behind it, as if turned around; magenta there is space not seen yet. Scene Memory Agreement compares memory with what is on screen: green matches, red is a stored surface in front of what is visible, blue is a visible surface memory holds as empty, magenta is unseen space. Scene Memory Sun Visibility shows stored sunlight from black (shadowed) to white (lit), red where none is stored (surfaces facing away from the sun store none) and orange where the sun has since moved more than two degrees (more for coarser cells). Magenta marks unavailable inputs.",
          .labels = {"Off", "GI Lighting", "Geometry Coverage", "Accepted Source Coverage", "Valid / Invalid Results", "Source Lighting", "Ambient Occlusion", "Ambient Diffuse", "Direct Diffuse", "Corrected Diffuse", "Ambient Input Validity", "Reflected Light", "Environment Weight", "Reflection Coverage", "Environment Specular", "Specular Visibility", "Raw Reflections", "NRD Validation", "Scene Memory", "Scene Memory Behind Camera", "Scene Memory Agreement", "Scene Memory Sun Visibility"},
      },
  };
  for (auto* setting : additions) {
    setting->is_enabled = [shader_injection] { return shader_injection->tone_map_type != 0.f; };
    if (setting->key != "IndirectLightingMode") {
      setting->is_visible = [shader_injection] { return shader_injection->indirect_lighting_mode >= 0.5f; };
    }
    if (setting->value_type != renodx::utils::settings::SettingValueType::BOOLEAN) continue;
    setting->on_change_value = [setting](float, float current) {
      const bool enabled = current >= 0.5f;
      const char* state = (enabled ? "On" : "Off");
      if (setting->labels.size() == 2u) {
        state = setting->labels[enabled ? 1u : 0u].c_str();
      }
      LogLight(reshade::log::level::info, setting->label, " = ", state, " (", setting->key, ")");
    };
  }
  // The Vanilla tone mapper applies its curve before the reflection composite, so the mode is off
  // under it. Explain that above the disabled controls.
  additions.insert(additions.begin(), new renodx::utils::settings::Setting{
                                          .value_type = renodx::utils::settings::SettingValueType::TEXT,
                                          .label = "Global Illumination needs the RenoDX tone mapper. Set Tone Mapper to RenoDX to use these settings.",
                                          .section = "Indirect Lighting",
                                          .tint = 0xFF0000,
                                          .is_visible = [shader_injection] { return shader_injection->tone_map_type == 0.f; },
                                      });
  registry->insert(std::ranges::find_if(*registry, [](const auto* setting) {
                     return setting->section == "Options";
                   }),
                   additions.begin(), additions.end());
}

}  // namespace mgsv::lighting

namespace mgsv::ao {

inline void Apply(const lighting::LightFrame& frame, reshade::api::command_list* cmd_list,
                  ShaderInjectData constants, const lighting::temporal_inputs::Snapshot& temporal) {
  // 1. Translate UI units to this scene. Search radius is in output pixels;
  // trace dimensions divide each render axis and round up at odd sizes.
  // All overrides stay in this invocation's settings copy.
  auto* device = cmd_list->get_device();
  const auto plan = reconstruction::resolution::Read();
  if (plan.IsScaled() && plan.render == reconstruction::resolution::Size{.width = frame.width, .height = frame.height}) {
    constants.lighting_search_radius_pixels *= static_cast<float>(frame.width) / static_cast<float>(plan.output.width);
  }
  uint32_t scale = 1u;
  if (constants.lighting_resolution > 2.5f) {
    scale = 8u;
  } else if (constants.lighting_resolution > 1.5f) {
    scale = 4u;
  } else if (constants.lighting_resolution > 0.5f) {
    scale = 2u;
  }
  constants.lighting_resolution_divisor = static_cast<float>(scale);

  const bool scene_sample = temporal.camera_publication != 0u;
  const uint32_t sample = (scene_sample ? temporal.camera_sample : lighting::noise_frame.load(std::memory_order_relaxed));
  constants.lighting_noise_frame = static_cast<float>(sample & 31u);
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

  const bool diagnostic_requested = (constants.lighting_debug_view >= 2.f && constants.lighting_debug_view <= 6.f);
  const bool diagnostic_ready = diagnostic_requested && EnsureLightTarget(device, &il::diagnostic_target, width, height);
  if (diagnostic_requested && !diagnostic_ready) {
    constants.lighting_debug_view = 0.f;
    static std::atomic_flag failed = {};
    LogLightOnce(&failed, reshade::log::level::warning,
                 "Lighting debug texture unavailable; normal lighting remains enabled");
  }

  // 2. Find the environment capture for this specular and depth pair. Its
  // material view is the G-buffer material input the trace uses for clean
  // diffuse radiance.
  Microsoft::WRL::ComPtr<ID3D11Resource> specular_resource;
  Microsoft::WRL::ComPtr<ID3D11Resource> depth_resource;
  reinterpret_cast<ID3D11ShaderResourceView*>(frame.specular.handle)->GetResource(&specular_resource);  // NOLINT(performance-no-int-to-ptr)
  reinterpret_cast<ID3D11ShaderResourceView*>(frame.depth.handle)->GetResource(&depth_resource);        // NOLINT(performance-no-int-to-ptr)
  auto environment = lighting::environment_specular::Read(device, specular_resource.Get());
  if (environment != nullptr && environment->depth.Get() != depth_resource.Get()) {
    environment.reset();
  }
  const bool depth_mips_requested = constants.lighting_depth_mips_enabled != 0.f;
  const bool depth_mips_ready = BuildDepthMips(cmd_list, frame.depth, depth_mips_requested);
  constants.lighting_depth_mips_enabled = (depth_mips_ready ? 1.f : 0.f);
  // EnsureFallbackViews above created these placeholders for the whole chain.
  const auto fallback_input = lighting::fallback_srv;
  const auto fallback_outputs = lighting::fallback_uavs;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11Resource> albedo_resource;
  reinterpret_cast<ID3D11ShaderResourceView*>(frame.albedo.handle)->GetResource(&albedo_resource);  // NOLINT(performance-no-int-to-ptr)
  auto clean_material = lighting::material_inputs::Read(device, albedo_resource.Get());
  const auto clean_color = (clean_material != nullptr
                                ? reshade::api::resource_view{reinterpret_cast<uintptr_t>(clean_material->color_srv.Get())}
                                : fallback_input);
  const auto clean_owner = (clean_material != nullptr
                                ? reshade::api::resource_view{reinterpret_cast<uintptr_t>(clean_material->owner_srv.Get())}
                                : reshade::api::resource_view{0u});
  // 3. Trace AO and diffuse GI together. Reflections are traced separately,
  // later in the frame, and add no work to this chain.
  const std::array<reshade::api::resource_view, lighting::LIGHT_INPUT_COUNT> sources = {
      frame.depth,
      frame.normal,
      depth_mip_chain.ready ? depth_mip_chain.view : fallback_input,
      frame.light_source,
      frame.albedo,
      fallback_input,
      {reinterpret_cast<uintptr_t>(noise.Get())},
      environment != nullptr ? reshade::api::resource_view{reinterpret_cast<uintptr_t>(environment->material.Get())} : fallback_input,
      fallback_input,
      fallback_input,
      fallback_input,
      clean_color,
      clean_owner,
      frame.prepared_source,
  };
  DispatchLightPass(cmd_list, &il::gather_pass, constants, sources,
                    fallback_outputs[0], il::indirect_diffuse_target.uav,
                    diagnostic_ready ? il::diagnostic_target.uav : fallback_outputs[1],
                    width, height);

  // 4. Apply. The application shader rebuilds the captured native ambient term
  // with its exact sampler, phase weights, and native b5 constants (bound here
  // at s0 and b3), so AO darkens only the ambient part of total diffuse. A
  // missing capture skips diffuse AO but not GI.
  Microsoft::WRL::ComPtr<ID3D11Resource> diffuse_resource;
  reinterpret_cast<ID3D11UnorderedAccessView*>(frame.light.handle)->GetResource(&diffuse_resource);  // NOLINT(performance-no-int-to-ptr)
  auto ambient = lighting::ambient_diffuse::Read(device, diffuse_resource.Get());
  if (ambient != nullptr && (ambient->depth.Get() != depth_resource.Get() || ambient->width != frame.width || ambient->height != frame.height)) {
    ambient.reset();
  }
  if (ambient == nullptr) {
    static std::atomic_flag reported = {};
    LogLightOnce(&reported, reshade::log::level::warning, "ambient diffuse unavailable; diffuse AO is skipped while GI continues");
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
  const std::array<reshade::api::resource_view, lighting::LIGHT_INPUT_COUNT> apply_sources = {
      il::indirect_diffuse_target.view,
      frame.depth,
      diagnostic_ready ? il::diagnostic_target.view : fallback_input,
      fallback_input,
      fallback_input,
      fallback_input,
      fallback_input,
      fallback_input,
      fallback_input,
      fallback_input,
      ambient != nullptr ? reshade::api::resource_view{reinterpret_cast<uintptr_t>(ambient->view.Get())} : fallback_input,
      ambient != nullptr ? reshade::api::resource_view{reinterpret_cast<uintptr_t>(ambient->weight_view.Get())} : fallback_input,
  };
  // Update native diffuse and the eligible scattering copy, preserving their alpha.
  DispatchLightPass(cmd_list, &il::add_pass, constants, apply_sources, frame.light, fallback_outputs[0],
                    frame.scattering_copy.handle != 0u ? frame.scattering_copy : fallback_outputs[1], frame.width, frame.height);
  static std::atomic_flag recorded = {};
  LogLightOnce(&recorded, reshade::log::level::info,
               "GT-VBGI commands recorded; effect=", width, "x", height,
               " noise=IS-FAST",
               " full_resolution=", frame.width, "x", frame.height,
               " scale=", constants.lighting_resolution_divisor,
               " slices=", constants.lighting_slice_count,
               " normal_bias=", constants.lighting_surface_offset,
               " radius_px=", constants.lighting_search_radius_pixels,
               " steps_per_side=", constants.lighting_samples_per_side,
               " thickness=", constants.lighting_surface_thickness,
               " ao_strength=", constants.ao_strength,
               " gi_backface=", constants.gi_reject_backfaces,
               " diffuse_gi_strength=", constants.diffuse_gi_strength,
               " lighting_depth_mips_enabled=", constants.lighting_depth_mips_enabled,
               " debug_view=", constants.lighting_debug_view,
               "; command submission is not GPU completion");

  if (depth_mips_requested && !depth_mips_ready) {
    static std::atomic_flag mip_failed = {};
    LogLightOnce(&mip_failed, reshade::log::level::warning,
                 "shared depth mip generation unavailable; AO/GI use full-resolution depth samples");
  }
}

}  // namespace mgsv::ao

// Scheduling of the lighting chain at the first skin scatter.
namespace mgsv::lighting {

// Called from the first skin scatter's shader callback, with its native inputs
// still bound. Recording keeps owned references and copies constants on the
// GPU. The scheduled callback runs when the command list recorded up to this
// point is submitted, before either skin scatter reads diffuse lighting.
inline void ApplyLightPasses(reshade::api::command_list* cmd_list, const ShaderInjectData& shader_injection) {
  if (cmd_list == nullptr || cmd_list->get_device()->get_api() != reshade::api::device_api::d3d11) return;
  if (!UsesEnhancedIndirectLighting()) return;

  ShaderInjectData constants;
  {
    const std::shared_lock lock(renodx::utils::mutex::global_mutex);
    constants = shader_injection;
    constants.lighting_debug_view = std::clamp(constants.lighting_debug_view, 0.f, 21.f);
  }

  // Lighting runs only on the device selected at the first Present, and not during a resize.
  const auto shared_inputs = inputs::Read(cmd_list->get_device());
  if (shared_inputs.device == nullptr) return;

  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (context == nullptr) return;

  // Reuse cached UAVs for the native diffuse images instead of creating them on
  // every scatter (see NativeLightViews).
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
  std::array<ID3D11ShaderResourceView*, 3> native_inputs = {};
  context->CSGetShaderResources(0u, static_cast<UINT>(native_inputs.size()), native_inputs.data());
  light_source.Attach(native_inputs[0]);
  depth.Attach(native_inputs[1]);
  normal.Attach(native_inputs[2]);
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
  // These checks cover layouts and ownership, not whether the pixels are current.
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
    // Report missing views at startup separately, so that report cannot hide a
    // later rejection once both material views exist. At most two reports per
    // load.
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
  // 3. Copy the scatter's geometry constant buffer (b0, 64 bytes used) on the
  // GPU, here in command order. Holding the native buffer is not enough: a
  // later recorded draw can overwrite it.
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
    if (context1 == nullptr) {
      geometry_count = description.ByteWidth / 16u;
    }
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
  // 4. At submission, find the native diffuse image the copy was made from,
  // using the producer record published with the copy. The lambda holds
  // references to everything the deferred work uses.
  const auto apply = [width = light_desc.Width, height = light_desc.Height, constants, geometry, geometry_first, geometry_count,
                      depth, normal, light_source, scattering_copy,
                      albedo = shared_inputs.materials.albedo,
                      specular = shared_inputs.materials.specular,
                      generation = shared_inputs.generation, get_light_view](reshade::api::command_list* submitted, const mgsv::rendering::submission::Execution& execution) {
    if (!execution.first) {
      // A replayed command list must not apply AO/GI again to diffuse lighting
      // that this insertion already corrected.
      Microsoft::WRL::ComPtr<ID3D11Resource> copied;
      light_source->GetResource(&copied);
      copied->SetPrivateDataInterface(temporal_inputs::DIFFUSE_SOURCE_GUID, nullptr);
      return;
    }
    const auto selected = inputs::ReadSelection(submitted->get_device());
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
    // Consume the association: each submitted native copy allows exactly one
    // lighting application.
    copied->SetPrivateDataInterface(temporal_inputs::DIFFUSE_SOURCE_GUID, nullptr);
    D3D11_TEXTURE2D_DESC description = {};
    diffuse_texture->GetDesc(&description);
    if (diffuse == copied || description.Width != width || description.Height != height
        || description.ArraySize != 1u || description.MipLevels != 1u || description.SampleDesc.Count != 1u) return;
    light = get_light_view(submitted->get_device(), generation, diffuse.Get());
    if (light == nullptr) return;
    // Use the prepared source only if this submitted copy wrote it from the
    // same diffuse, albedo, normal, and depth inputs. Compare the underlying
    // textures and formats, since matching dimensions do not identify a view.
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> prepared;
    bytes = sizeof(ID3D11ShaderResourceView*);
    copied->GetPrivateData(temporal_inputs::PREPARED_SOURCE_GUID, &bytes, prepared.GetAddressOf());
    const auto prepared_inputs = source_preparation::Read(submitted->get_device(), copied.Get());
    if (prepared_inputs == nullptr || prepared_inputs->srv != prepared) {
      prepared.Reset();
    } else {
      const std::array<ID3D11ShaderResourceView*, 3> expected = {albedo.Get(), normal.Get(), depth.Get()};
      const std::array<size_t, 3> slots = {2u, 3u, 4u};
      for (size_t index = 0u; index < slots.size(); ++index) {
        if (prepared_inputs->inputs[slots[index]].Get() == expected[index]) continue;
        Microsoft::WRL::ComPtr<ID3D11Resource> source;
        Microsoft::WRL::ComPtr<ID3D11Resource> current;
        prepared_inputs->inputs[slots[index]]->GetResource(&source);
        expected[index]->GetResource(&current);
        D3D11_SHADER_RESOURCE_VIEW_DESC source_view = {};
        D3D11_SHADER_RESOURCE_VIEW_DESC current_view = {};
        prepared_inputs->inputs[slots[index]]->GetDesc(&source_view);
        expected[index]->GetDesc(&current_view);
        if (source != current || source_view.Format != current_view.Format) prepared.Reset();
      }
      Microsoft::WRL::ComPtr<ID3D11Resource> source;
      prepared_inputs->inputs[0]->GetResource(&source);
      if (source != diffuse) prepared.Reset();
    }
    if (prepared != nullptr) {
      static std::atomic_flag reported = {};
      LogLightOnce(&reported, reshade::log::level::info, "Shared source radiance: submitted native copy matched; FP32 source reused by diffuse GI");
    } else {
      static std::atomic_flag reported = {};
      LogLightOnce(&reported, reshade::log::level::info, "Shared source radiance unavailable for this copy; using trace source loads");
    }
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
        .prepared_source = {reinterpret_cast<uintptr_t>(prepared.Get())},
        .width = width,
        .height = height,
    };
    mgsv::ao::Apply(submitted_frame, submitted, constants, temporal_inputs::Read(submitted->get_device(), submitted_frame.depth));
    mgsv::rendering::d3d11_compute_state::Restore(submitted, previous_state);
  };
  if (geometry != nullptr && mgsv::rendering::submission::Schedule(cmd_list, {.run = apply})) return;

  // Without a constants copy or a free submission slot, record the chain
  // directly on this context.
  const mgsv::rendering::submission::ExecutionGuard execution_guard;
  apply(cmd_list, {.first = true});
}

}  // namespace mgsv::lighting

#endif  // SRC_GAMES_MGSV_LIGHTING_HPP_
