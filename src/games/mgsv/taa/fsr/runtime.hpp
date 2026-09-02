#pragma once

/*
 * D3D11 scheduler and temporal-resource owner for the MGSV FSR2 SM5 port.
 * Shader sources remain separate; this file only creates resources, binds the
 * six-pass graph, handles resets, and copies encoded output back to MGSV.
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include <embed/shaders.h>
#include <include/reshade.hpp>

#include "../runtime/logging.hpp"
#include "../runtime/projection_jitter.hpp"
#include "../runtime/resolve.hpp"
#include "../runtime/state.hpp"

namespace taa::fsr {

// Header-only because MGSV's addon is built as one translation unit. All
// functions run under resolve::ExecutionGuard, including destruction.
inline constexpr uint32_t THREAD_GROUP_SIZE = 8u;
inline constexpr uint32_t SPD_TILE_SIZE = 64u;
inline constexpr uint32_t SCENE_LUMINANCE_MIP_COUNT = 6u;

struct OwnedTexture {
  reshade::api::resource resource = {0};
  reshade::api::resource_view srv = {0};
  reshade::api::resource_view uav = {0};
  reshade::api::resource_usage usage = reshade::api::resource_usage::undefined;
};

struct Pipelines {
  reshade::api::pipeline_layout common_layout = {0};
  reshade::api::pipeline_layout prepare_layout = {0};
  reshade::api::pipeline prepare = {0};
  reshade::api::pipeline luminance_pyramid = {0};
  reshade::api::pipeline reconstruct_previous_depth = {0};
  reshade::api::pipeline depth_clip = {0};
  reshade::api::pipeline lock = {0};
  reshade::api::pipeline accumulate = {0};
  reshade::api::sampler linear_sampler = {0};
};

// Pipelines live for the device lifetime. Temporal textures are released when
// FSR2 is inactive so analytical TAA and FSR2 histories are never both resident.
struct Resources {
  Pipelines pipelines = {};

  OwnedTexture linear_input_color = {};
  OwnedTexture motion_input = {};
  OwnedTexture prepared_input_color = {};
  OwnedTexture reconstructed_previous_depth = {};
  std::array<OwnedTexture, 2> dilated_motion = {};
  OwnedTexture dilated_depth = {};
  std::array<OwnedTexture, 2> lock_status = {};
  OwnedTexture lock_input_luma = {};
  OwnedTexture new_locks = {};
  std::array<OwnedTexture, 2> internal_upscaled = {};
  OwnedTexture encoded_output = {};
  OwnedTexture scene_luminance = {};
  reshade::api::resource_view scene_luminance_mip4_uav = {0};
  reshade::api::resource_view scene_luminance_mip5_uav = {0};
  std::array<OwnedTexture, 2> luma_history = {};
  OwnedTexture spd_atomic = {};
  OwnedTexture dilated_reactive_masks = {};
  OwnedTexture auto_exposure = {};

  uint32_t width = 0u;
  uint32_t height = 0u;
  uint32_t resource_frame_index = 0u;
  uint32_t frame_index = 0u;
  uint64_t settings_generation = std::numeric_limits<uint64_t>::max();
  bool initialized = false;
  std::chrono::steady_clock::time_point previous_dispatch_time;
};

// These layouts mirror the HLSL cbuffers exactly. Keep the static assertions
// when changing either side of the ABI.
struct Fsr2Constants {
  std::array<int32_t, 2> render_size = {};
  std::array<int32_t, 2> max_render_size = {};
  std::array<int32_t, 2> display_size = {};
  std::array<int32_t, 2> input_color_resource_dimensions = {};
  std::array<int32_t, 2> luma_mip_dimensions = {};
  int32_t luma_mip_level_to_use = 4;
  int32_t frame_index = 0;

  std::array<float, 4> device_to_view_depth = {};
  std::array<float, 2> jitter = {};
  std::array<float, 2> motion_vector_scale = {1.f, 1.f};
  std::array<float, 2> downscale_factor = {1.f, 1.f};
  std::array<float, 2> motion_vector_jitter_cancellation = {};
  float pre_exposure = 1.f;
  float previous_frame_pre_exposure = 1.f;
  float tan_half_fov = 1.f;
  float jitter_sequence_length = 8.f;
  float delta_time = 1.f / 60.f;
  float dynamic_resolution_change_factor = 0.f;
  float view_space_to_meters_factor = 1.f;
  float padding = 0.f;
};

struct SpdConstants {
  uint32_t mips = 0u;
  uint32_t num_work_groups = 0u;
  std::array<uint32_t, 2> work_group_offset = {};
  std::array<uint32_t, 2> render_size = {};
};

struct PrepareConstants {
  std::array<float, 2> current_jitter_uv = {};
  float velocity_projection_jitter_scale = 1.f;
  float camera_reprojection_valid = 0.f;
  std::array<uint32_t, 2> render_size = {};
  std::array<float, 2> reciprocal_render_size = {};
  std::array<float, 16> current_to_previous_clip = {};
};

struct DepthClipConstants {
  float depth_separation_scale = 0.f;
  float depth_clip_power = 1.f;
  float depth_clip_power_is_three = 0.f;
  float padding = 0.f;
};

static_assert(sizeof(Fsr2Constants) == 128u, "FSR2 constants must occupy 32 dwords");
static_assert(sizeof(SpdConstants) == 24u, "FSR2 SPD constants must occupy 6 dwords");
static_assert(sizeof(PrepareConstants) == 96u, "MGSV FSR2 input constants must occupy 24 dwords");
static_assert(sizeof(DepthClipConstants) == 16u, "MGSV FSR2 depth constants must occupy 4 dwords");

inline Resources resources = {};
inline uint64_t last_failure_log = std::numeric_limits<uint64_t>::max();

inline bool LogEvery(uint64_t interval = 120u) {
  return logging::ShouldLogFrame(state::frame_state.frame_index, last_failure_log, interval);
}

inline uint32_t DivideRoundUp(uint32_t value, uint32_t divisor) {
  return (value + divisor - 1u) / divisor;
}

inline void Transition(
    reshade::api::command_list* cmd_list,
    OwnedTexture& owned_texture,
    reshade::api::resource_usage usage) {
  if (cmd_list == nullptr || owned_texture.resource.handle == 0u || owned_texture.usage == usage) return;
  cmd_list->barrier(owned_texture.resource, owned_texture.usage, usage);
  owned_texture.usage = usage;
}

inline void DestroyTexture(reshade::api::device* device, OwnedTexture& owned_texture) {
  if (device == nullptr) return;
  if (owned_texture.uav.handle != 0u) device->destroy_resource_view(owned_texture.uav);
  if (owned_texture.srv.handle != 0u) device->destroy_resource_view(owned_texture.srv);
  if (owned_texture.resource.handle != 0u) device->destroy_resource(owned_texture.resource);
  owned_texture = {};
}

inline bool CreateTexture(
    reshade::api::device* device,
    OwnedTexture& owned_texture,
    uint32_t width,
    uint32_t height,
    uint32_t mip_levels,
    reshade::api::format format,
    bool sampled) {
  if (device == nullptr || width == 0u || height == 0u || mip_levels == 0u) return false;

  reshade::api::resource_desc desc = {};
  desc.type = reshade::api::resource_type::texture_2d;
  desc.texture = {width, height, 1u, static_cast<uint16_t>(mip_levels), format, 1u};
  desc.heap = reshade::api::memory_heap::gpu_only;
  desc.usage = reshade::api::resource_usage::unordered_access
               | (sampled ? reshade::api::resource_usage::shader_resource
                          : reshade::api::resource_usage::undefined);
  desc.flags = reshade::api::resource_flags::none;

  const auto initial_usage = sampled ? reshade::api::resource_usage::shader_resource
                                     : reshade::api::resource_usage::unordered_access;
  if (!device->create_resource(desc, nullptr, initial_usage, &owned_texture.resource)) return false;
  owned_texture.usage = initial_usage;

  if (sampled) {
    const reshade::api::resource_view_desc srv_desc(
        reshade::api::resource_view_type::texture_2d,
        format,
        0u,
        mip_levels,
        0u,
        1u);
    if (!device->create_resource_view(
            owned_texture.resource,
            reshade::api::resource_usage::shader_resource,
            srv_desc,
            &owned_texture.srv)) {
      DestroyTexture(device, owned_texture);
      return false;
    }
  }

  const reshade::api::resource_view_desc uav_desc(
      reshade::api::resource_view_type::texture_2d,
      format,
      0u,
      1u,
      0u,
      1u);
  if (!device->create_resource_view(
          owned_texture.resource,
          reshade::api::resource_usage::unordered_access,
          uav_desc,
          &owned_texture.uav)) {
    DestroyTexture(device, owned_texture);
    return false;
  }
  return true;
}

inline bool CreateMipUav(
    reshade::api::device* device,
    const OwnedTexture& owned_texture,
    uint32_t mip,
    reshade::api::resource_view& view) {
  const reshade::api::resource_view_desc desc(
      reshade::api::resource_view_type::texture_2d,
      reshade::api::format::r16_float,
      mip,
      1u,
      0u,
      1u);
  return device->create_resource_view(
      owned_texture.resource,
      reshade::api::resource_usage::unordered_access,
      desc,
      &view);
}

inline void DestroyPipelines(reshade::api::device* device) {
  if (device == nullptr) return;
  auto& pipelines = resources.pipelines;
  const std::array<reshade::api::pipeline*, 6> pipeline_handles = {
      &pipelines.prepare,
      &pipelines.luminance_pyramid,
      &pipelines.reconstruct_previous_depth,
      &pipelines.depth_clip,
      &pipelines.lock,
      &pipelines.accumulate,
  };
  for (auto* pipeline : pipeline_handles) {
    if (pipeline->handle != 0u) device->destroy_pipeline(*pipeline);
    *pipeline = {0};
  }
  if (pipelines.common_layout.handle != 0u) device->destroy_pipeline_layout(pipelines.common_layout);
  if (pipelines.prepare_layout.handle != 0u) device->destroy_pipeline_layout(pipelines.prepare_layout);
  pipelines.common_layout = {0};
  pipelines.prepare_layout = {0};
  if (pipelines.linear_sampler.handle != 0u) device->destroy_sampler(pipelines.linear_sampler);
  pipelines.linear_sampler = {0};
}

inline void DestroyTemporalResources(reshade::api::device* device) {
  if (device == nullptr) return;
  if (resources.scene_luminance_mip4_uav.handle != 0u) {
    device->destroy_resource_view(resources.scene_luminance_mip4_uav);
  }
  if (resources.scene_luminance_mip5_uav.handle != 0u) {
    device->destroy_resource_view(resources.scene_luminance_mip5_uav);
  }
  resources.scene_luminance_mip4_uav = {0};
  resources.scene_luminance_mip5_uav = {0};

  const std::array<OwnedTexture*, 20> textures = {
      &resources.linear_input_color,
      &resources.motion_input,
      &resources.prepared_input_color,
      &resources.reconstructed_previous_depth,
      resources.dilated_motion.data(),
      resources.dilated_motion.data() + 1u,
      &resources.dilated_depth,
      resources.lock_status.data(),
      resources.lock_status.data() + 1u,
      &resources.lock_input_luma,
      &resources.new_locks,
      resources.internal_upscaled.data(),
      resources.internal_upscaled.data() + 1u,
      &resources.encoded_output,
      &resources.scene_luminance,
      resources.luma_history.data(),
      resources.luma_history.data() + 1u,
      &resources.spd_atomic,
      &resources.dilated_reactive_masks,
      &resources.auto_exposure,
  };
  for (auto* texture : textures) {
    DestroyTexture(device, *texture);
  }

  resources.width = 0u;
  resources.height = 0u;
  resources.resource_frame_index = 0u;
  resources.frame_index = 0u;
  resources.settings_generation = std::numeric_limits<uint64_t>::max();
  resources.initialized = false;
  resources.previous_dispatch_time = {};
}

inline void ReleaseTemporalResources(reshade::api::device* device) {
  if (device == nullptr || resources.linear_input_color.resource.handle == 0u) return;
  const uint32_t released_width = resources.width;
  const uint32_t released_height = resources.height;
  DestroyTemporalResources(device);
  logging::Info("released inactive AMD FSR2 resources size=", released_width, "x", released_height);
}

inline void Destroy(reshade::api::device* device) {
  DestroyTemporalResources(device);
  DestroyPipelines(device);
  resources = {};
}

inline bool CreatePipeline(
    reshade::api::device* device,
    reshade::api::pipeline_layout layout,
    std::span<const uint8_t> code,
    reshade::api::pipeline& pipeline) {
  reshade::api::shader_desc shader = {
      .code = code.data(),
      .code_size = code.size(),
  };
  const reshade::api::pipeline_subobject subobject = {
      .type = reshade::api::pipeline_subobject_type::compute_shader,
      .count = 1u,
      .data = &shader,
  };
  return device->create_pipeline(layout, 1u, &subobject, &pipeline);
}

inline bool EnsurePipelines(reshade::api::device* device) {
  if (device == nullptr || device->get_api() != reshade::api::device_api::d3d11) return false;
  auto& pipelines = resources.pipelines;
  if (pipelines.prepare.handle != 0u
      && pipelines.luminance_pyramid.handle != 0u
      && pipelines.reconstruct_previous_depth.handle != 0u
      && pipelines.depth_clip.handle != 0u
      && pipelines.lock.handle != 0u
      && pipelines.accumulate.handle != 0u) {
    return true;
  }

  DestroyPipelines(device);

  const std::array<reshade::api::pipeline_layout_param, 6> common_params = {
      reshade::api::pipeline_layout_param{reshade::api::descriptor_range{
          .binding = 0u,
          .dx_register_index = 1u,
          .count = 1u,
          .visibility = reshade::api::shader_stage::compute,
          .type = reshade::api::descriptor_type::sampler,
      }},
      reshade::api::pipeline_layout_param{reshade::api::descriptor_range{
          .binding = 0u,
          .dx_register_index = 0u,
          .count = 12u,
          .visibility = reshade::api::shader_stage::compute,
          .type = reshade::api::descriptor_type::texture_shader_resource_view,
      }},
      reshade::api::pipeline_layout_param{reshade::api::descriptor_range{
          .binding = 0u,
          .dx_register_index = 0u,
          .count = 5u,
          .visibility = reshade::api::shader_stage::compute,
          .type = reshade::api::descriptor_type::texture_unordered_access_view,
      }},
      reshade::api::pipeline_layout_param{reshade::api::constant_range{
          .binding = 0u,
          .dx_register_index = 0u,
          .count = 32u,
          .visibility = reshade::api::shader_stage::compute,
      }},
      reshade::api::pipeline_layout_param{reshade::api::constant_range{
          .binding = 0u,
          .dx_register_index = 1u,
          .count = 6u,
          .visibility = reshade::api::shader_stage::compute,
      }},
      reshade::api::pipeline_layout_param{reshade::api::constant_range{
          .binding = 0u,
          .dx_register_index = 2u,
          .count = 4u,
          .visibility = reshade::api::shader_stage::compute,
      }},
  };
  if (!device->create_pipeline_layout(
          static_cast<uint32_t>(common_params.size()),
          common_params.data(),
          &pipelines.common_layout)) {
    return false;
  }

  const std::array<reshade::api::pipeline_layout_param, 3> prepare_params = {
      reshade::api::pipeline_layout_param{reshade::api::descriptor_range{
          .binding = 0u,
          .dx_register_index = 0u,
          .count = 4u,
          .visibility = reshade::api::shader_stage::compute,
          .type = reshade::api::descriptor_type::texture_shader_resource_view,
      }},
      reshade::api::pipeline_layout_param{reshade::api::descriptor_range{
          .binding = 0u,
          .dx_register_index = 0u,
          .count = 2u,
          .visibility = reshade::api::shader_stage::compute,
          .type = reshade::api::descriptor_type::texture_unordered_access_view,
      }},
      reshade::api::pipeline_layout_param{reshade::api::constant_range{
          .binding = 0u,
          .dx_register_index = 0u,
          .count = 24u,
          .visibility = reshade::api::shader_stage::compute,
      }},
  };
  if (!device->create_pipeline_layout(
          static_cast<uint32_t>(prepare_params.size()),
          prepare_params.data(),
          &pipelines.prepare_layout)) {
    DestroyPipelines(device);
    return false;
  }

  if (!device->create_sampler(
          reshade::api::sampler_desc{.filter = reshade::api::filter_mode::min_mag_mip_linear},
          &pipelines.linear_sampler)) {
    DestroyPipelines(device);
    return false;
  }

  const bool created =
      CreatePipeline(device, pipelines.prepare_layout, __fsr2_prepare_inputs, pipelines.prepare)
      && CreatePipeline(device, pipelines.common_layout, __fsr2_luminance_pyramid, pipelines.luminance_pyramid)
      && CreatePipeline(device, pipelines.common_layout, __fsr2_reconstruct_previous_depth, pipelines.reconstruct_previous_depth)
      && CreatePipeline(
          device,
          pipelines.common_layout,
          __fsr2_depth_clip_zero_masks,
          pipelines.depth_clip)
      && CreatePipeline(device, pipelines.common_layout, __fsr2_lock, pipelines.lock)
      && CreatePipeline(device, pipelines.common_layout, __fsr2_accumulate, pipelines.accumulate);
  if (!created) {
    DestroyPipelines(device);
    return false;
  }

  logging::Info("created AMD FSR2 2.3.4 D3D11 SM5 pipelines history_kernel=approximate",
                " input_masks=zero output_encode=fused");
  return true;
}

inline bool EnsureTemporalResources(reshade::api::device* device, uint32_t width, uint32_t height) {
  if (device == nullptr || width == 0u || height == 0u) return false;
  if (resources.linear_input_color.resource.handle != 0u
      && resources.width == width
      && resources.height == height) {
    return true;
  }

  DestroyTemporalResources(device);
  resources.width = width;
  resources.height = height;
  const auto make_full = [&](OwnedTexture& texture, reshade::api::format format) {
    return CreateTexture(device, texture, width, height, 1u, format, true);
  };

  const bool created = make_full(resources.linear_input_color, reshade::api::format::r16g16b16a16_float)
                       && make_full(resources.motion_input, reshade::api::format::r16g16_float)
                       && make_full(resources.prepared_input_color, reshade::api::format::r16g16b16a16_float)
                       && make_full(resources.reconstructed_previous_depth, reshade::api::format::r32_uint)
                       && make_full(resources.dilated_motion[0], reshade::api::format::r16g16_float)
                       && make_full(resources.dilated_motion[1], reshade::api::format::r16g16_float)
                       && make_full(resources.dilated_depth, reshade::api::format::r32_float)
                       && make_full(resources.lock_status[0], reshade::api::format::r16g16_float)
                       && make_full(resources.lock_status[1], reshade::api::format::r16g16_float)
                       && make_full(resources.lock_input_luma, reshade::api::format::r16_float)
                       && CreateTexture(
                           device,
                           resources.new_locks,
                           width,
                           height,
                           1u,
                           reshade::api::format::r8_unorm,
                           false)
                       && make_full(resources.internal_upscaled[0], reshade::api::format::r16g16b16a16_float)
                       && make_full(resources.internal_upscaled[1], reshade::api::format::r16g16b16a16_float)
                       && CreateTexture(
                           device,
                           resources.encoded_output,
                           width,
                           height,
                           1u,
                           reshade::api::format::r16g16b16a16_float,
                           false)
                       && make_full(resources.luma_history[0], reshade::api::format::r8g8b8a8_unorm)
                       && make_full(resources.luma_history[1], reshade::api::format::r8g8b8a8_unorm)
                       && CreateTexture(
                           device,
                           resources.scene_luminance,
                           std::max(1u, width / 2u),
                           std::max(1u, height / 2u),
                           SCENE_LUMINANCE_MIP_COUNT,
                           reshade::api::format::r16_float,
                           true)
                       && CreateMipUav(device, resources.scene_luminance, 4u, resources.scene_luminance_mip4_uav)
                       && CreateMipUav(device, resources.scene_luminance, 5u, resources.scene_luminance_mip5_uav)
                       && CreateTexture(device, resources.spd_atomic, 1u, 1u, 1u, reshade::api::format::r32_uint, false)
                       && make_full(resources.dilated_reactive_masks, reshade::api::format::r8g8_unorm)
                       && CreateTexture(device, resources.auto_exposure, 1u, 1u, 1u, reshade::api::format::r32g32_float, true);
  if (!created) {
    if (LogEvery()) logging::Warn("failed to create AMD FSR2 D3D11 resources");
    DestroyTemporalResources(device);
    return false;
  }
  if (resources.scene_luminance.uav.handle != 0u) {
    device->destroy_resource_view(resources.scene_luminance.uav);
    resources.scene_luminance.uav = {0};
  }

  resources.resource_frame_index = 0u;
  resources.frame_index = 0u;
  resources.initialized = false;
  resources.previous_dispatch_time = {};
  logging::Info("created AMD FSR2 2.3.4 resources size=", width, "x", height,
                " luma_mips=", SCENE_LUMINANCE_MIP_COUNT);
  return true;
}

inline void ClearFloat(
    reshade::api::command_list* cmd_list,
    OwnedTexture& owned_texture,
    const float (&values)[4]) {
  Transition(cmd_list, owned_texture, reshade::api::resource_usage::unordered_access);
  cmd_list->clear_unordered_access_view_float(owned_texture.uav, values, 0u, nullptr);
}

inline void ClearUint(
    reshade::api::command_list* cmd_list,
    OwnedTexture& owned_texture,
    const uint32_t (&values)[4]) {
  Transition(cmd_list, owned_texture, reshade::api::resource_usage::unordered_access);
  cmd_list->clear_unordered_access_view_uint(owned_texture.uav, values, 0u, nullptr);
}

inline void ResetHistory(reshade::api::command_list* cmd_list) {
  constexpr float zero_float[4] = {0.f, 0.f, 0.f, 0.f};
  constexpr uint32_t zero_uint[4] = {0u, 0u, 0u, 0u};
  constexpr float reset_exposure[4] = {-1.f, 1e8f, 0.f, 0.f};

  // The reset accumulation pass ignores previous color/lock/luma history and
  // fully overwrites the current ping-pong targets and prepared input.
  ClearFloat(cmd_list, resources.new_locks, zero_float);
  ClearUint(cmd_list, resources.reconstructed_previous_depth, zero_uint);
  ClearUint(cmd_list, resources.spd_atomic, zero_uint);
  ClearFloat(cmd_list, resources.auto_exposure, reset_exposure);

  Transition(cmd_list, resources.scene_luminance, reshade::api::resource_usage::unordered_access);
  cmd_list->clear_unordered_access_view_float(resources.scene_luminance_mip4_uav, zero_float, 0u, nullptr);
  cmd_list->clear_unordered_access_view_float(resources.scene_luminance_mip5_uav, zero_float, 0u, nullptr);

  resources.resource_frame_index = 0u;
  resources.frame_index = 0u;
  resources.initialized = false;
}

template <size_t SrvCount, size_t UavCount>
inline void PushTextureDescriptors(
    reshade::api::command_list* cmd_list,
    reshade::api::pipeline_layout layout,
    uint32_t srv_param,
    uint32_t uav_param,
    const std::array<reshade::api::resource_view, SrvCount>& srvs,
    const std::array<reshade::api::resource_view, UavCount>& uavs) {
  const reshade::api::descriptor_table_update srv_update = {
      .table = {},
      .binding = 0u,
      .array_offset = 0u,
      .count = static_cast<uint32_t>(srvs.size()),
      .type = reshade::api::descriptor_type::texture_shader_resource_view,
      .descriptors = srvs.data(),
  };
  const reshade::api::descriptor_table_update uav_update = {
      .table = {},
      .binding = 0u,
      .array_offset = 0u,
      .count = static_cast<uint32_t>(uavs.size()),
      .type = reshade::api::descriptor_type::texture_unordered_access_view,
      .descriptors = uavs.data(),
  };
  cmd_list->push_descriptors(reshade::api::shader_stage::compute, layout, srv_param, srv_update);
  cmd_list->push_descriptors(reshade::api::shader_stage::compute, layout, uav_param, uav_update);
}

inline void BindFsrConstants(
    reshade::api::command_list* cmd_list,
    const Fsr2Constants& constants,
    const SpdConstants& secondary,
    const DepthClipConstants& depth_clip) {
  const reshade::api::descriptor_table_update sampler_update = {
      .table = {},
      .binding = 0u,
      .array_offset = 0u,
      .count = 1u,
      .type = reshade::api::descriptor_type::sampler,
      .descriptors = &resources.pipelines.linear_sampler,
  };
  cmd_list->push_descriptors(
      reshade::api::shader_stage::compute,
      resources.pipelines.common_layout,
      0u,
      sampler_update);
  cmd_list->push_constants(
      reshade::api::shader_stage::compute,
      resources.pipelines.common_layout,
      3u,
      0u,
      sizeof(constants) / sizeof(uint32_t),
      &constants);
  cmd_list->push_constants(
      reshade::api::shader_stage::compute,
      resources.pipelines.common_layout,
      4u,
      0u,
      sizeof(secondary) / sizeof(uint32_t),
      &secondary);
  cmd_list->push_constants(
      reshade::api::shader_stage::compute,
      resources.pipelines.common_layout,
      5u,
      0u,
      sizeof(depth_clip) / sizeof(uint32_t),
      &depth_clip);
}

inline void DispatchFsrPass(
    reshade::api::command_list* cmd_list,
    reshade::api::pipeline pipeline,
    const std::array<reshade::api::resource_view, 12>& srvs,
    const std::array<reshade::api::resource_view, 5>& uavs,
    uint32_t dispatch_x,
    uint32_t dispatch_y) {
  PushTextureDescriptors(cmd_list, resources.pipelines.common_layout, 1u, 2u, srvs, uavs);
  cmd_list->bind_pipeline(reshade::api::pipeline_stage::compute_shader, pipeline);
  cmd_list->dispatch(dispatch_x, dispatch_y, 1u);

  // D3D11 cannot bind the same resource as an SRV and UAV in the next pass.
  const std::array<reshade::api::resource_view, 12> null_srvs = {};
  const std::array<reshade::api::resource_view, 5> null_uavs = {};
  PushTextureDescriptors(cmd_list, resources.pipelines.common_layout, 1u, 2u, null_srvs, null_uavs);
}

inline float FrameDeltaSeconds(bool reset) {
  const auto now = std::chrono::steady_clock::now();
  float delta = 1.f / 60.f;
  if (!reset && resources.previous_dispatch_time.time_since_epoch().count() != 0) {
    delta = std::chrono::duration<float>(now - resources.previous_dispatch_time).count();
  }
  resources.previous_dispatch_time = now;
  return std::clamp(delta, 0.f, 1.f);
}

inline Fsr2Constants BuildConstants(
    const projection_jitter::AppliedJitter& native_jitter,
    bool reset) {
  if (reset) {
    resources.frame_index = 0u;
  } else {
    ++resources.frame_index;
  }

  Fsr2Constants constants = {};
  constants.render_size = {static_cast<int32_t>(resources.width), static_cast<int32_t>(resources.height)};
  constants.max_render_size = constants.render_size;
  constants.display_size = constants.render_size;
  constants.input_color_resource_dimensions = constants.render_size;
  constants.luma_mip_dimensions = {
      static_cast<int32_t>(resources.width / 32u),
      static_cast<int32_t>(resources.height / 32u),
  };
  constants.luma_mip_level_to_use = 4;
  constants.frame_index = static_cast<int32_t>(resources.frame_index);
  constants.device_to_view_depth = native_jitter.device_to_view_depth;
  constants.jitter = {
      native_jitter.jitter_uv_x * static_cast<float>(resources.width),
      native_jitter.jitter_uv_y * static_cast<float>(resources.height),
  };
  constants.motion_vector_scale = {1.f, 1.f};
  constants.downscale_factor = {1.f, 1.f};
  constants.pre_exposure = 1.f;
  constants.previous_frame_pre_exposure = 1.f;
  constants.tan_half_fov = std::abs(native_jitter.device_to_view_depth[2]);
  constants.jitter_sequence_length = 8.f;
  constants.delta_time = FrameDeltaSeconds(reset);
  constants.dynamic_resolution_change_factor = 0.f;
  constants.view_space_to_meters_factor = 1.f;
  return constants;
}

inline SpdConstants BuildSpdConstants() {
  const uint32_t groups_x = DivideRoundUp(resources.width, SPD_TILE_SIZE);
  const uint32_t groups_y = DivideRoundUp(resources.height, SPD_TILE_SIZE);
  const uint32_t resolution = std::max(resources.width, resources.height);
  return SpdConstants{
      .mips = std::min(12u, static_cast<uint32_t>(std::floor(std::log2(static_cast<float>(resolution))))),
      .num_work_groups = groups_x * groups_y,
      .work_group_offset = {0u, 0u},
      .render_size = {resources.width, resources.height},
  };
}

inline DepthClipConstants BuildDepthClipConstants(
    const projection_jitter::AppliedJitter& native_jitter) {
  const float width = static_cast<float>(resources.width);
  const float height = static_cast<float>(resources.height);
  const float diagonal = std::hypot(width, height);
  const float reference_diagonal = std::hypot(1920.f, 1080.f);
  const float center_ndc_x = (2.f * std::floor(width * 0.5f) / width) - 1.f;
  const float center_ndc_y = 1.f - (2.f * std::floor(height * 0.5f) / height);
  const float inverse_projection_x = native_jitter.device_to_view_depth[2];
  const float inverse_projection_y = native_jitter.device_to_view_depth[3];
  const float center_length = std::sqrt(
      ((inverse_projection_x * center_ndc_x) * (inverse_projection_x * center_ndc_x))
      + ((inverse_projection_y * center_ndc_y) * (inverse_projection_y * center_ndc_y))
      + 1.f);
  const float corner_length = std::sqrt(
      (inverse_projection_x * inverse_projection_x)
      + (inverse_projection_y * inverse_projection_y)
      + 1.f);
  const float resolution_factor = std::clamp(diagonal / reference_diagonal, 0.f, 1.f);

  return DepthClipConstants{
      .depth_separation_scale = 1.37e-05f * (corner_length / center_length) * diagonal,
      .depth_clip_power = 1.f + (2.f * resolution_factor),
      .depth_clip_power_is_three = resolution_factor >= 1.f ? 1.f : 0.f,
  };
}

inline bool IsCameraDiscontinuity(const projection_jitter::AppliedJitter& native_jitter) {
  if (!native_jitter.camera_reprojection_valid) return true;

  // A valid matrix pair can still span a cut. Mid-depth center/corners catch
  // large lateral, FOV, and roll discontinuities without treating ordinary
  // nonlinear near/far depth motion as a cut.
  constexpr std::array<std::array<float, 3>, 5> clip_samples = {{
      {0.f, 0.f, 0.5f},
      {-1.f, -1.f, 0.5f},
      {1.f, -1.f, 0.5f},
      {-1.f, 1.f, 0.5f},
      {1.f, 1.f, 0.5f},
  }};
  const auto is_discontinuous = [&](const std::array<float, 3>& sample) {
    const auto& matrix = native_jitter.current_to_previous_clip;
    const float previous_x = (matrix[0] * sample[0]) + (matrix[1] * sample[1])
                             + (matrix[2] * sample[2]) + matrix[3];
    const float previous_y = (matrix[4] * sample[0]) + (matrix[5] * sample[1])
                             + (matrix[6] * sample[2]) + matrix[7];
    const float previous_w = (matrix[12] * sample[0]) + (matrix[13] * sample[1])
                             + (matrix[14] * sample[2]) + matrix[15];
    if (!std::isfinite(previous_w) || previous_w <= 1e-6f) return true;
    const float previous_x_ndc = previous_x / previous_w;
    const float previous_y_ndc = previous_y / previous_w;
    return !std::isfinite(previous_x_ndc)
           || !std::isfinite(previous_y_ndc)
           || std::abs(previous_w - 1.f) > 0.5f
           || std::abs(previous_x_ndc - sample[0]) > 0.75f
           || std::abs(previous_y_ndc - sample[1]) > 0.75f;
  };
  return std::any_of(clip_samples.begin(), clip_samples.end(), is_discontinuous);
}

inline void Dispatch(
    reshade::api::command_list* cmd_list,
    reshade::api::resource_view color_srv,
    reshade::api::resource color_resource,
    reshade::api::resource_usage color_initial_usage,
    reshade::api::resource_usage color_final_usage,
    const projection_jitter::AppliedJitter& native_jitter,
    bool reset) {
  const auto previous_compute_state = resolve::CaptureComputeState(cmd_list);
  auto& captured = resolve::resources;

  if (color_initial_usage != reshade::api::resource_usage::shader_resource) {
    cmd_list->barrier(color_resource, color_initial_usage, reshade::api::resource_usage::shader_resource);
  }
  cmd_list->barrier(
      captured.velocity_resource,
      reshade::api::resource_usage::render_target,
      reshade::api::resource_usage::shader_resource);

  // 1. Decode MGSV scene color and normalize camera/object motion for FSR2.
  Transition(cmd_list, resources.linear_input_color, reshade::api::resource_usage::unordered_access);
  Transition(cmd_list, resources.motion_input, reshade::api::resource_usage::unordered_access);
  const std::array<reshade::api::resource_view, 4> prepare_srvs = {
      color_srv,
      captured.velocity_srv,
      captured.depth_srv,
      captured.object_velocity_srv,
  };
  const std::array<reshade::api::resource_view, 2> prepare_uavs = {
      resources.linear_input_color.uav,
      resources.motion_input.uav,
  };
  const PrepareConstants prepare_constants = {
      .current_jitter_uv = {native_jitter.jitter_uv_x, native_jitter.jitter_uv_y},
      .velocity_projection_jitter_scale = state::GetProjectionJitterScale(state::ProjectionJitterPath::VELOCITY),
      .camera_reprojection_valid = !reset && native_jitter.camera_reprojection_valid ? 1.f : 0.f,
      .render_size = {resources.width, resources.height},
      .reciprocal_render_size = {
          1.f / static_cast<float>(resources.width),
          1.f / static_cast<float>(resources.height),
      },
      .current_to_previous_clip = native_jitter.current_to_previous_clip,
  };
  PushTextureDescriptors(
      cmd_list,
      resources.pipelines.prepare_layout,
      0u,
      1u,
      prepare_srvs,
      prepare_uavs);
  cmd_list->push_constants(
      reshade::api::shader_stage::compute,
      resources.pipelines.prepare_layout,
      2u,
      0u,
      sizeof(prepare_constants) / sizeof(uint32_t),
      &prepare_constants);
  cmd_list->bind_pipeline(reshade::api::pipeline_stage::compute_shader, resources.pipelines.prepare);
  cmd_list->dispatch(
      DivideRoundUp(resources.width, THREAD_GROUP_SIZE),
      DivideRoundUp(resources.height, THREAD_GROUP_SIZE),
      1u);
  const std::array<reshade::api::resource_view, 4> null_prepare_srvs = {};
  const std::array<reshade::api::resource_view, 2> null_prepare_uavs = {};
  PushTextureDescriptors(
      cmd_list,
      resources.pipelines.prepare_layout,
      0u,
      1u,
      null_prepare_srvs,
      null_prepare_uavs);
  Transition(cmd_list, resources.linear_input_color, reshade::api::resource_usage::shader_resource);
  Transition(cmd_list, resources.motion_input, reshade::api::resource_usage::shader_resource);

  if (reset) ResetHistory(cmd_list);

  const bool odd_frame = (resources.resource_frame_index & 1u) != 0u;
  const uint32_t previous = odd_frame ? 1u : 0u;
  const uint32_t current = 1u - previous;
  const uint32_t current_motion = odd_frame ? 1u : 0u;
  const uint32_t previous_motion = 1u - current_motion;
  const Fsr2Constants constants = BuildConstants(native_jitter, reset);
  const SpdConstants spd_constants = BuildSpdConstants();
  const DepthClipConstants depth_clip_constants = BuildDepthClipConstants(native_jitter);
  BindFsrConstants(cmd_list, constants, spd_constants, depth_clip_constants);

  // 2. Build luminance mips and update FSR2's exposure/shading-change state.
  Transition(cmd_list, resources.scene_luminance, reshade::api::resource_usage::unordered_access);
  Transition(cmd_list, resources.spd_atomic, reshade::api::resource_usage::unordered_access);
  Transition(cmd_list, resources.auto_exposure, reshade::api::resource_usage::unordered_access);
  std::array<reshade::api::resource_view, 12> srvs = {};
  std::array<reshade::api::resource_view, 5> uavs = {};
  srvs[0] = resources.linear_input_color.srv;
  uavs[0] = resources.spd_atomic.uav;
  uavs[1] = resources.scene_luminance_mip4_uav;
  uavs[2] = resources.scene_luminance_mip5_uav;
  uavs[3] = resources.auto_exposure.uav;
  DispatchFsrPass(
      cmd_list,
      resources.pipelines.luminance_pyramid,
      srvs,
      uavs,
      DivideRoundUp(resources.width, SPD_TILE_SIZE),
      DivideRoundUp(resources.height, SPD_TILE_SIZE));
  Transition(cmd_list, resources.scene_luminance, reshade::api::resource_usage::shader_resource);
  Transition(cmd_list, resources.auto_exposure, reshade::api::resource_usage::shader_resource);

  // 3. Reconstruct previous depth and dilate current depth and motion.
  Transition(cmd_list, resources.reconstructed_previous_depth, reshade::api::resource_usage::unordered_access);
  Transition(cmd_list, resources.dilated_motion[current_motion], reshade::api::resource_usage::unordered_access);
  Transition(cmd_list, resources.dilated_depth, reshade::api::resource_usage::unordered_access);
  Transition(cmd_list, resources.lock_input_luma, reshade::api::resource_usage::unordered_access);
  srvs = {};
  uavs = {};
  srvs[0] = resources.motion_input.srv;
  srvs[1] = captured.depth_srv;
  srvs[2] = resources.linear_input_color.srv;
  uavs[0] = resources.reconstructed_previous_depth.uav;
  uavs[1] = resources.dilated_motion[current_motion].uav;
  uavs[2] = resources.dilated_depth.uav;
  uavs[3] = resources.lock_input_luma.uav;
  DispatchFsrPass(
      cmd_list,
      resources.pipelines.reconstruct_previous_depth,
      srvs,
      uavs,
      DivideRoundUp(resources.width, THREAD_GROUP_SIZE),
      DivideRoundUp(resources.height, THREAD_GROUP_SIZE));
  Transition(cmd_list, resources.reconstructed_previous_depth, reshade::api::resource_usage::shader_resource);
  Transition(cmd_list, resources.dilated_motion[current_motion], reshade::api::resource_usage::shader_resource);
  Transition(cmd_list, resources.dilated_depth, reshade::api::resource_usage::shader_resource);
  Transition(cmd_list, resources.lock_input_luma, reshade::api::resource_usage::shader_resource);

  // 4. Compute depth rejection and prepared color; input masks are currently zero.
  Transition(cmd_list, resources.dilated_reactive_masks, reshade::api::resource_usage::unordered_access);
  Transition(cmd_list, resources.prepared_input_color, reshade::api::resource_usage::unordered_access);
  srvs = {};
  uavs = {};
  srvs[0] = resources.reconstructed_previous_depth.srv;
  srvs[1] = resources.dilated_motion[current_motion].srv;
  srvs[2] = resources.dilated_depth.srv;
  srvs[5] = resources.dilated_motion[previous_motion].srv;
  srvs[6] = resources.motion_input.srv;
  srvs[7] = resources.linear_input_color.srv;
  srvs[8] = captured.depth_srv;
  uavs[0] = resources.dilated_reactive_masks.uav;
  uavs[1] = resources.prepared_input_color.uav;
  DispatchFsrPass(
      cmd_list,
      resources.pipelines.depth_clip,
      srvs,
      uavs,
      DivideRoundUp(resources.width, THREAD_GROUP_SIZE),
      DivideRoundUp(resources.height, THREAD_GROUP_SIZE));
  Transition(cmd_list, resources.dilated_reactive_masks, reshade::api::resource_usage::shader_resource);
  Transition(cmd_list, resources.prepared_input_color, reshade::api::resource_usage::shader_resource);

  // 5. Create thin-feature locks and clear reconstructed depth for the next frame.
  Transition(cmd_list, resources.new_locks, reshade::api::resource_usage::unordered_access);
  Transition(cmd_list, resources.reconstructed_previous_depth, reshade::api::resource_usage::unordered_access);
  srvs = {};
  uavs = {};
  srvs[0] = resources.lock_input_luma.srv;
  uavs[0] = resources.new_locks.uav;
  uavs[1] = resources.reconstructed_previous_depth.uav;
  DispatchFsrPass(
      cmd_list,
      resources.pipelines.lock,
      srvs,
      uavs,
      DivideRoundUp(resources.width, THREAD_GROUP_SIZE),
      DivideRoundUp(resources.height, THREAD_GROUP_SIZE));

  // 6. Accumulate linear history and write MGSV's encoded scene output.
  Transition(cmd_list, resources.internal_upscaled[previous], reshade::api::resource_usage::shader_resource);
  Transition(cmd_list, resources.internal_upscaled[current], reshade::api::resource_usage::unordered_access);
  Transition(cmd_list, resources.lock_status[previous], reshade::api::resource_usage::shader_resource);
  Transition(cmd_list, resources.lock_status[current], reshade::api::resource_usage::unordered_access);
  Transition(cmd_list, resources.luma_history[previous], reshade::api::resource_usage::shader_resource);
  Transition(cmd_list, resources.luma_history[current], reshade::api::resource_usage::unordered_access);
  srvs = {};
  uavs = {};
  srvs[1] = resources.dilated_reactive_masks.srv;
  srvs[2] = resources.dilated_motion[current_motion].srv;
  srvs[3] = resources.internal_upscaled[previous].srv;
  srvs[4] = resources.lock_status[previous].srv;
  srvs[5] = resources.prepared_input_color.srv;
  srvs[8] = resources.scene_luminance.srv;
  srvs[9] = resources.auto_exposure.srv;
  srvs[10] = resources.luma_history[previous].srv;
  srvs[11] = resources.linear_input_color.srv;
  uavs[0] = resources.internal_upscaled[current].uav;
  uavs[1] = resources.lock_status[current].uav;
  uavs[2] = resources.encoded_output.uav;
  uavs[3] = resources.new_locks.uav;
  uavs[4] = resources.luma_history[current].uav;
  DispatchFsrPass(
      cmd_list,
      resources.pipelines.accumulate,
      srvs,
      uavs,
      DivideRoundUp(resources.width, THREAD_GROUP_SIZE),
      DivideRoundUp(resources.height, THREAD_GROUP_SIZE));
  Transition(cmd_list, resources.internal_upscaled[current], reshade::api::resource_usage::shader_resource);
  Transition(cmd_list, resources.lock_status[current], reshade::api::resource_usage::shader_resource);
  Transition(cmd_list, resources.luma_history[current], reshade::api::resource_usage::shader_resource);

  Transition(cmd_list, resources.encoded_output, reshade::api::resource_usage::copy_source);
  cmd_list->barrier(color_resource, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::copy_dest);
  cmd_list->copy_resource(resources.encoded_output.resource, color_resource);
  cmd_list->barrier(color_resource, reshade::api::resource_usage::copy_dest, color_final_usage);
  Transition(cmd_list, resources.encoded_output, reshade::api::resource_usage::unordered_access);
  cmd_list->barrier(
      captured.velocity_resource,
      reshade::api::resource_usage::shader_resource,
      reshade::api::resource_usage::render_target);

  resolve::RestoreComputeState(cmd_list, previous_compute_state);
  resources.resource_frame_index = (resources.resource_frame_index + 1u) & 15u;
}

inline bool TryResolve(
    reshade::api::command_list* cmd_list,
    reshade::api::resource_view color_srv,
    reshade::api::resource_usage color_initial_usage,
    reshade::api::resource_usage color_final_usage,
    const projection_jitter::AppliedJitter& native_jitter,
    const char* insertion_name) {
  // Shared resolve validation already proved input freshness and dimensions.
  // This boundary checks only FSR2's D3D11 format and resource requirements.
  auto* device = cmd_list != nullptr ? cmd_list->get_device() : nullptr;
  if (device == nullptr || device->get_api() != reshade::api::device_api::d3d11) return false;
  if (!EnsurePipelines(device)) {
    if (LogEvery()) logging::Warn("failed to create AMD FSR2 D3D11 pipelines");
    return false;
  }

  const auto color_resource = device->get_resource_from_view(color_srv);
  if (color_resource.handle == 0u) return false;
  resolve::ReleaseHistory(device);
  const auto color_desc = device->get_resource_desc(color_resource);
  const auto color_view_desc = device->get_resource_view_desc(color_srv);
  const auto color_view_format = resolve::GetTypedViewFormat(device, color_srv);
  const bool copy_compatible = color_desc.type == reshade::api::resource_type::texture_2d
                               && color_desc.texture.depth_or_layers == 1u
                               && color_desc.texture.levels == 1u
                               && color_desc.texture.samples == 1u
                               && color_view_desc.type == reshade::api::resource_view_type::texture_2d
                               && color_view_desc.texture.first_level == 0u
                               && color_view_desc.texture.first_layer == 0u
                               && color_view_format == reshade::api::format::r16g16b16a16_float
                               && (color_desc.texture.format == reshade::api::format::r16g16b16a16_float
                                   || color_desc.texture.format == reshade::api::format::r16g16b16a16_typeless);
  if (!copy_compatible) {
    if (LogEvery(30u)) {
      logging::Warn("rejecting AMD FSR2 dispatch with incompatible scene format insertion=", insertion_name,
                    " resource_format=", static_cast<uint32_t>(color_desc.texture.format),
                    " view_format=", static_cast<uint32_t>(color_view_format));
    }
    return false;
  }
  if (native_jitter.width != color_desc.texture.width
      || native_jitter.height != color_desc.texture.height) {
    if (LogEvery(30u)) {
      logging::Warn("rejecting AMD FSR2 dispatch with mismatched native dimensions insertion=", insertion_name,
                    " jitter=", native_jitter.width, "x", native_jitter.height,
                    " color=", color_desc.texture.width, "x", color_desc.texture.height);
    }
    return false;
  }
  if (!EnsureTemporalResources(device, color_desc.texture.width, color_desc.texture.height)) return false;

  const uint64_t settings_generation = state::RuntimeSettingsGeneration();
  const bool reset = !resources.initialized
                     || resources.settings_generation != settings_generation
                     || IsCameraDiscontinuity(native_jitter);
  Dispatch(
      cmd_list,
      color_srv,
      color_resource,
      color_initial_usage,
      color_final_usage,
      native_jitter,
      reset);

  if (reset) {
    logging::Info("AMD FSR2 accumulation started insertion=", insertion_name,
                  " frame=", state::CurrentFrameToken(),
                  " native_frame=", native_jitter.frame_token,
                  " sample=", state::CurrentSampleIndex(),
                  " size=", resources.width, "x", resources.height);
  }
  resources.initialized = true;
  resources.settings_generation = settings_generation;

  if (!projection_jitter::CommitCameraMatrix(
          native_jitter.frame_token,
          state::CurrentSampleIndex())) {
    resources.initialized = false;
    resolve::InvalidateHistory("AMD FSR2 native camera matrix commit failed");
    logging::Warn("AMD FSR2 native camera matrix commit failed");
  }
  state::MarkTaaDispatched();
  return true;
}

}  // namespace taa::fsr
