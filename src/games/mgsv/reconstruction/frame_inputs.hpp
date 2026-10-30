#pragma once

/* Immutable game-native inputs accepted for one temporal reconstruction. */

#include <array>
#include <cstdint>

#include <include/reshade.hpp>

#include "./camera_state.hpp"

namespace mgsv::reconstruction {

struct alignas(16) PrepareConstants {
  std::array<float, 2> current_jitter_uv = {};
  float velocity_projection_jitter_scale = 1.f;
  float camera_reprojection_valid = 0.f;
  std::array<uint32_t, 2> render_size = {};
  std::array<float, 2> reciprocal_render_size = {};
  std::array<float, 16> current_to_previous_clip = {};
};

static_assert(sizeof(PrepareConstants) == 96u, "MGSV SDK input constants must occupy 24 dwords");

struct ValidatedFrameInputs {
  reshade::api::command_list* cmd_list = nullptr;
  reshade::api::device* device = nullptr;

  reshade::api::resource_view color_srv = {0};
  reshade::api::resource color_resource = {0};
  reshade::api::resource_desc color_description;
  reshade::api::format color_format = reshade::api::format::unknown;

  reshade::api::resource_view velocity_srv = {0};
  reshade::api::resource velocity_resource = {0};

  reshade::api::resource_view depth_srv = {0};
  reshade::api::resource depth_resource = {0};
  reshade::api::format depth_format = reshade::api::format::unknown;

  reshade::api::resource_view object_velocity_srv = {0};

  camera_state::CameraFrame camera = {};
  uint64_t frame_token = 0u;
  uint32_t sample_index = 0u;
  uint32_t width = 0u;
  uint32_t height = 0u;
  const char* insertion_name = "unknown";
};

struct MethodOutput {
  reshade::api::resource resource = {0};
  reshade::api::resource_usage final_usage = reshade::api::resource_usage::unordered_access;
};

}  // namespace mgsv::reconstruction
