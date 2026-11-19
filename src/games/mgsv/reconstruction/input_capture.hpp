#pragma once

/* The scene record reconstruction reads where post-processing starts, and the
 * capture at the camera-velocity draw that feeds the lighting history. */

#include <d3d11.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

#include <include/reshade.hpp>

#include "../rendering/engine_targets.hpp"
#include "../rendering/inputs.hpp"
#include "../rendering/native_context.hpp"
#include "../rendering/native_velocity.hpp"
#include "camera_state.hpp"
#include "frame_inputs.hpp"
#include "logging.hpp"
#include "resolution.hpp"
#include "state.hpp"

namespace mgsv::reconstruction::input_capture {

// The camera-velocity draw's bindings and the camera its scene was recorded
// with, for the lighting history.
struct Resources {
  inputs::Motion motion;
  camera_state::CameraFrame camera = {};
};

inline uint64_t last_capture_missing_log = std::numeric_limits<uint64_t>::max();

inline bool LogEvery(uint64_t& last_frame, uint64_t interval = 120u) {
  return logging::ShouldLogFrame(state::CurrentFrameToken(), last_frame, interval);
}

inline reshade::api::format GetTypedViewFormat(
    reshade::api::device* device,
    reshade::api::resource_view view) {
  if (device == nullptr || view.handle == 0u) return reshade::api::format::unknown;
  const auto view_description = device->get_resource_view_desc(view);
  if (view_description.format != reshade::api::format::unknown) return view_description.format;
  const auto resource = device->get_resource_from_view(view);
  if (resource.handle == 0u) return reshade::api::format::unknown;
  return reshade::api::format_to_default_typed(device->get_resource_desc(resource).texture.format);
}

inline Resources CaptureCameraMotionLocked(
    reshade::api::command_list* cmd_list,
    const inputs::descriptor_tracker::CommandListData& command_data,
    reshade::api::resource_view velocity_rtv) {
  if (velocity_rtv.handle == 0u) {
    if (LogEvery(last_capture_missing_log)) logging::Warn("camera velocity pass has no RTV0");
    return {};
  }
  if (command_data.pixel_srv_t2.handle == 0u || command_data.pixel_srv_t3.handle == 0u) {
    if (LogEvery(last_capture_missing_log)) {
      logging::Warn("camera velocity capture missing inputs velocity_rtv=", logging::Hex{velocity_rtv.handle},
                    " depth=", logging::Hex{command_data.pixel_srv_t2.handle},
                    " object_velocity=", logging::Hex{command_data.pixel_srv_t3.handle});
    }
    return {};
  }

  // Retain the live native bindings, including the velocity target this draw writes.
  auto motion = inputs::CaptureMotion(cmd_list, velocity_rtv, true);
  if (motion.velocity == nullptr) return {};
  return {.motion = std::move(motion), .camera = camera_state::GetForCapture()};
}

// How much of a scene's reconstruction inputs the cut found.
enum class Record : uint8_t {
  // No scene image reconstruction can read.
  UNUSABLE,
  // The scene image is usable, but its motion vectors or camera are missing.
  SCENE_ONLY,
  COMPLETE,
};

// Why the last record was not complete; null while records are complete.
// Accessed under the coordinator execution lock.
inline const char* reported_cause = nullptr;
// The probe can have complete records treated as scene-only, to exercise the
// fill-only outcome at will.
inline std::atomic<bool> probe_fill_only = false;

// Builds the inputs of one scene's reconstruction where its post-processing
// starts, from the engine's own targets and the camera recorded for the view.
// The caller holds the execution lock and keeps `images` alive while it uses
// the result. Every check a record must pass is in this one list.
inline Record BuildFrameInputsLocked(
    reshade::api::command_list* cmd_list,
    const std::array<rendering::engine_targets::FrameImage, 4>& images,
    ValidatedFrameInputs* output) {
  using rendering::engine_targets::FrameTarget;
  auto* device = cmd_list->get_device();
  const auto owner = rendering::native_context::GetScope().owner;
  const auto& color = images[static_cast<size_t>(FrameTarget::SCENE_COLOR)];
  const auto& depth = images[static_cast<size_t>(FrameTarget::DEPTH)];
  const auto& velocity = images[static_cast<size_t>(FrameTarget::VELOCITY)];
  const auto& object_velocity = images[static_cast<size_t>(FrameTarget::OBJECT_VELOCITY)];
  auto record = Record::UNUSABLE;
  const char* cause = nullptr;
  const auto plan = resolution::Read();
  if (std::ranges::any_of(images, [](const rendering::engine_targets::FrameImage& image) {
        return image.texture == nullptr || image.shader_view == nullptr;
      })) {
    cause = "the engine created its scene, depth or velocity targets before the mod's hooks were installed";
  } else {
    const reshade::api::resource color_resource = {reinterpret_cast<uintptr_t>(color.texture.Get())};
    const reshade::api::resource_view color_srv = {reinterpret_cast<uintptr_t>(color.shader_view.Get())};
    const reshade::api::resource_view depth_srv = {reinterpret_cast<uintptr_t>(depth.shader_view.Get())};
    const auto color_description = device->get_resource_desc(color_resource);
    const uint32_t width = color_description.texture.width;
    const uint32_t height = color_description.texture.height;
    const auto color_format = GetTypedViewFormat(device, color_srv);
    const auto matches_scene = [&](const rendering::engine_targets::FrameImage& image) {
      D3D11_TEXTURE2D_DESC description;
      image.texture->GetDesc(&description);
      return description.Width == width && description.Height == height && description.ArraySize == 1u
             && description.MipLevels == 1u && description.SampleDesc.Count == 1u;
    };
    if (!std::ranges::all_of(images, matches_scene)) {
      cause = "the scene, depth and velocity targets are not single-sample images of one size";
    } else if (plan.mode != state::GetTemporalMode()) {
      // Native resize and SDK activation can overlap recording.
      cause = "the size plan still belongs to another method";
    } else if (!IsReconstructionColorTexture(color_description) || color_format != color_description.texture.format
               || (plan.IsScaled() && color_format != reshade::api::format::r16g16b16a16_float)) {
      cause = "the scene image is not a float image";
    } else if (plan.IsScaled() && plan.render != resolution::Size{.width = width, .height = height}) {
      cause = "the scene is not at the planned render size";
    } else {
      const auto camera = camera_state::GetForCapture();
      record = Record::SCENE_ONLY;
      if (!rendering::native_velocity::WasRecorded(owner)) {
        cause = "the engine drew no motion vectors for the view";
      } else if (camera.scene != owner.scene || camera.owner_generation != owner.generation || !camera.valid
                 || camera.reset_generation != camera_state::reset_generation.load(std::memory_order_acquire)) {
        cause = "no camera was recorded for the view";
      } else if (!camera.camera_matrix_valid || camera.width != width || camera.height != height) {
        cause = "the recorded camera does not match the scene size";
      } else {
        record = Record::COMPLETE;
      }
      *output = {
          .cmd_list = cmd_list,
          .device = device,
          .color_srv = color_srv,
          .color_resource = color_resource,
          .color_description = color_description,
          .color_format = color_format,
          .velocity_srv = {reinterpret_cast<uintptr_t>(velocity.shader_view.Get())},
          .velocity_resource = {reinterpret_cast<uintptr_t>(velocity.texture.Get())},
          .depth_srv = depth_srv,
          .depth_resource = {reinterpret_cast<uintptr_t>(depth.texture.Get())},
          .depth_format = GetTypedViewFormat(device, depth_srv),
          .object_velocity_srv = {reinterpret_cast<uintptr_t>(object_velocity.shader_view.Get())},
          .camera = camera,
          .frame_token = (record == Record::COMPLETE ? camera.frame_token : state::CurrentFrameToken()),
          .sample_index = camera.sample_index,
          .render_width = width,
          .render_height = height,
          // A scaled request has no destination until the coordinator allocates
          // the output route. Never describe the smaller source as that target.
          .output = {
              .resource = (plan.IsScaled() ? reshade::api::resource{0u} : color_resource),
              .width = (plan.IsScaled() ? plan.output.width : width),
              .height = (plan.IsScaled() ? plan.output.height : height),
              .format = color_format,
          },
      };
    }
  }
  if (record == Record::COMPLETE && probe_fill_only.load(std::memory_order_acquire)) {
    record = Record::SCENE_ONLY;
    cause = "the probe asked for fill only";
  }
  if (cause != reported_cause) {
    reported_cause = cause;
    if (cause == nullptr) {
      logging::Info("reconstruction inputs are complete scene=", owner.scene);
    } else {
      logging::Warn("reconstruction inputs are incomplete: ", cause, " scene=", owner.scene);
    }
  }
  if (record == Record::COMPLETE) {
    state::frame_state.complete_record_seen = true;
  }
  return record;
}

}  // namespace mgsv::reconstruction::input_capture
