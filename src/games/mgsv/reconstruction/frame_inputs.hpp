#pragma once

/* Immutable game-native inputs accepted for one temporal reconstruction. */

#include <array>
#include <cstdint>
#include <memory>

#include <include/reshade.hpp>

#include "../rendering/shared_texture.hpp"
#include "camera_state.hpp"

namespace mgsv::reconstruction {

// These are the scene formats the DX12 transport can read and write without
// changing their encoding or precision. Other full-size images are not proof
// that we have reached the reconstruction input.
inline bool IsReconstructionColorTexture(const reshade::api::resource_desc& description) {
  return description.type == reshade::api::resource_type::texture_2d
         && description.texture.depth_or_layers == 1u
         && description.texture.levels == 1u
         && description.texture.samples == 1u
         && (description.texture.format == reshade::api::format::r16g16b16a16_float
             || description.texture.format == reshade::api::format::r32g32b32a32_float);
}

struct alignas(16) PrepareConstants {
  std::array<float, 2> current_jitter_uv = {};
  float velocity_projection_jitter_scale = 1.f;
  float camera_reprojection_valid = 0.f;
  std::array<uint32_t, 2> render_size = {};
  std::array<float, 2> reciprocal_render_size = {};
  std::array<float, 16> current_to_previous_clip = {};
  std::array<float, 2> previous_jitter_uv = {};
  float object_motion_mode = 0.f;
  float analytical_taa = 0.f;
};

static_assert(sizeof(PrepareConstants) == 112u, "Reconstruction input constants must occupy 28 dwords");

struct ColorTarget {
  reshade::api::resource resource = {0};
  uint32_t width = 0u;
  uint32_t height = 0u;
  reshade::api::format format = reshade::api::format::unknown;
  reshade::api::resource_usage usage = reshade::api::resource_usage::shader_resource;
  // Optional shared allocation and its cached D3D12 import.
  std::shared_ptr<rendering::SharedTexture> shared_texture;
};

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
  uint32_t render_width = 0u;
  uint32_t render_height = 0u;
  ColorTarget output = {};
  const char* insertion_name = "unknown";

  // Analytical TAA keeps native resolution. SDK backends can use a distinct,
  // larger destination when the output-sized post-processing route is active.
  [[nodiscard]] bool HasNativeOutputSize() const {
    return render_width != 0u && render_height != 0u
           && render_width == output.width && render_height == output.height;
  }
};

}  // namespace mgsv::reconstruction
