#pragma once

/* Capture and validate MGSV's game-native temporal resources once per frame. */

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>

#include <include/reshade.hpp>

#include "../../../../utils/resource.hpp"
#include "../../../../utils/resource_upgrade.hpp"
#include "./camera_state.hpp"
#include "./descriptor_tracker.hpp"
#include "./frame_inputs.hpp"
#include "./logging.hpp"
#include "./state.hpp"

namespace taa::input_capture {

struct Resources {
  reshade::api::resource velocity_resource = {0};
  reshade::api::resource_view velocity_rtv = {0};
  reshade::api::resource_view velocity_srv = {0};
  reshade::api::format velocity_view_format = reshade::api::format::unknown;
  reshade::api::resource_view depth_srv = {0};
  reshade::api::resource_view object_velocity_srv = {0};
  camera_state::CameraFrame camera = {};
  uint64_t capture_frame = std::numeric_limits<uint64_t>::max();
  uint32_t capture_sample_index = std::numeric_limits<uint32_t>::max();
};

inline Resources resources;
inline constexpr size_t DESTROYED_VIEW_MAILBOX_SIZE = 64u;
inline std::array<std::atomic<uint64_t>, DESTROYED_VIEW_MAILBOX_SIZE> destroyed_view_mailbox = {};
inline std::atomic<bool> destroyed_view_mailbox_overflow = false;
inline uint64_t last_missing_inputs_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_capture_missing_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_stale_capture_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_missing_camera_log = std::numeric_limits<uint64_t>::max();
inline bool logged_non_full_resolution_candidate = false;
inline bool logged_waiting_for_camera = false;

inline bool LogEvery(uint64_t& last_frame, uint64_t interval = 120u) {
  return logging::ShouldLogFrame(state::CurrentFrameToken(), last_frame, interval);
}

inline void InvalidateCapturedFrame() {
  resources.depth_srv = {0};
  resources.object_velocity_srv = {0};
  resources.camera = {};
  resources.capture_frame = std::numeric_limits<uint64_t>::max();
  resources.capture_sample_index = std::numeric_limits<uint32_t>::max();
}

inline void DestroyVelocitySrv(reshade::api::device* device) {
  if (device != nullptr && resources.velocity_srv.handle != 0u) {
    device->destroy_resource_view(resources.velocity_srv);
  }
  resources.velocity_resource = {0};
  resources.velocity_rtv = {0};
  resources.velocity_srv = {0};
  resources.velocity_view_format = reshade::api::format::unknown;
  InvalidateCapturedFrame();
}

inline void Destroy(reshade::api::device* device) {
  DestroyVelocitySrv(device);
  for (auto& slot : destroyed_view_mailbox) slot.store(0u, std::memory_order_relaxed);
  destroyed_view_mailbox_overflow.store(false, std::memory_order_relaxed);
  logged_non_full_resolution_candidate = false;
  logged_waiting_for_camera = false;
}

inline void ApplyDestroyedResourceViewLocked(reshade::api::device* device, uint64_t handle) {
  if (handle == resources.velocity_rtv.handle || handle == resources.velocity_srv.handle) {
    DestroyVelocitySrv(handle == resources.velocity_srv.handle ? nullptr : device);
    return;
  }
  if (handle == resources.depth_srv.handle || handle == resources.object_velocity_srv.handle) {
    InvalidateCapturedFrame();
  }
}

inline void ProcessDestroyedResourceViewsLocked(reshade::api::device* device) {
  if (destroyed_view_mailbox_overflow.exchange(false, std::memory_order_acq_rel)) {
    DestroyVelocitySrv(device);
  }
  for (auto& slot : destroyed_view_mailbox) {
    const uint64_t handle = slot.exchange(0u, std::memory_order_acq_rel);
    if (handle != 0u) ApplyDestroyedResourceViewLocked(device, handle);
  }
}

inline void QueueDestroyedResourceView(uint64_t handle) {
  if (handle == 0u) return;
  for (auto& slot : destroyed_view_mailbox) {
    uint64_t empty = 0u;
    if (slot.compare_exchange_strong(
            empty,
            handle,
            std::memory_order_release,
            std::memory_order_relaxed)) {
      return;
    }
  }
  destroyed_view_mailbox_overflow.store(true, std::memory_order_release);
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

inline reshade::api::resource_view NormalizeColorSrv(reshade::api::resource_view color_srv) {
  if (color_srv.handle == 0u) return {0u};
  const auto clone = renodx::utils::resource::upgrade::GetResourceViewClone(
      color_srv,
      {
          .require_enabled = true,
          .allow_create = true,
          .activate = false,
      });
  return clone.handle != 0u ? clone : color_srv;
}

inline bool EnsureVelocitySrv(
    reshade::api::command_list* cmd_list,
    reshade::api::resource_view velocity_rtv) {
  auto* device = cmd_list != nullptr ? cmd_list->get_device() : nullptr;
  if (device == nullptr || velocity_rtv.handle == 0u) return false;
  if (resources.velocity_srv.handle != 0u
      && resources.velocity_rtv.handle == velocity_rtv.handle
      && resources.velocity_resource.handle != 0u) {
    return true;
  }

  const auto velocity_resource = device->get_resource_from_view(velocity_rtv);
  if (velocity_resource.handle == 0u) return false;
  const auto view_format = GetTypedViewFormat(device, velocity_rtv);
  if (view_format == reshade::api::format::unknown) return false;
  if (resources.velocity_srv.handle != 0u
      && resources.velocity_resource.handle == velocity_resource.handle
      && resources.velocity_view_format == view_format) {
    resources.velocity_rtv = velocity_rtv;
    return true;
  }

  DestroyVelocitySrv(device);
  const auto view_description = reshade::api::resource_view_desc(
      reshade::api::resource_view_type::texture_2d,
      view_format,
      0,
      1,
      0,
      1);
  if (!device->create_resource_view(
          velocity_resource,
          reshade::api::resource_usage::shader_resource,
          view_description,
          &resources.velocity_srv)) {
    logging::Warn("failed to create velocity SRV view_format=", static_cast<uint32_t>(view_format));
    return false;
  }

  resources.velocity_resource = velocity_resource;
  resources.velocity_rtv = velocity_rtv;
  resources.velocity_view_format = view_format;
  logging::Info("created velocity SRV resource=", logging::Hex{velocity_resource.handle},
                " view_format=", static_cast<uint32_t>(view_format));
  return true;
}

inline void CaptureCameraMotionLocked(
    reshade::api::command_list* cmd_list,
    const descriptor_tracker::CommandListData& command_data,
    reshade::api::resource_view velocity_rtv) {
  auto* device = cmd_list != nullptr ? cmd_list->get_device() : nullptr;
  ProcessDestroyedResourceViewsLocked(device);
  if (velocity_rtv.handle == 0u) {
    if (LogEvery(last_capture_missing_log)) logging::Warn("camera velocity pass has no RTV0");
    return;
  }
  if (command_data.pixel_srv_t2.handle == 0u || command_data.pixel_srv_t3.handle == 0u) {
    if (LogEvery(last_capture_missing_log)) {
      logging::Warn("camera velocity capture missing inputs velocity_rtv=", logging::Hex{velocity_rtv.handle},
                    " depth=", logging::Hex{command_data.pixel_srv_t2.handle},
                    " object_velocity=", logging::Hex{command_data.pixel_srv_t3.handle});
    }
    return;
  }

  auto object_velocity_srv = command_data.pixel_srv_t3;
  const auto object_velocity_clone = renodx::utils::resource::upgrade::GetResourceViewClone(object_velocity_srv);
  if (object_velocity_clone.handle != 0u) object_velocity_srv = object_velocity_clone;
  if (!EnsureVelocitySrv(cmd_list, velocity_rtv)) return;

  resources.depth_srv = command_data.pixel_srv_t2;
  resources.object_velocity_srv = object_velocity_srv;
  resources.camera = camera_state::Get();
  resources.capture_frame = state::CurrentFrameToken();
  resources.capture_sample_index = state::CurrentSampleIndex();
}

inline bool IsSingleSampleTexture(const reshade::api::resource_desc& description) {
  return description.type == reshade::api::resource_type::texture_2d
         && description.texture.depth_or_layers == 1u
         && description.texture.levels == 1u
         && description.texture.samples == 1u;
}

inline bool BuildValidatedFrameInputsLocked(
    reshade::api::command_list* cmd_list,
    reshade::api::resource_view source_color_srv,
    const char* insertion_name,
    ValidatedFrameInputs& output) {
  auto* device = cmd_list != nullptr ? cmd_list->get_device() : nullptr;
  ProcessDestroyedResourceViewsLocked(device);
  const auto color_srv = NormalizeColorSrv(source_color_srv);
  if (device == nullptr
      || color_srv.handle == 0u
      || resources.velocity_srv.handle == 0u
      || resources.depth_srv.handle == 0u
      || resources.object_velocity_srv.handle == 0u) {
    if (LogEvery(last_missing_inputs_log)) {
      logging::Warn("insertion missing inputs insertion=", insertion_name,
                    " color=", logging::Hex{color_srv.handle},
                    " velocity=", logging::Hex{resources.velocity_srv.handle},
                    " depth=", logging::Hex{resources.depth_srv.handle},
                    " object_velocity=", logging::Hex{resources.object_velocity_srv.handle});
    }
    return false;
  }

  const auto color_resource = device->get_resource_from_view(color_srv);
  const auto depth_resource = device->get_resource_from_view(resources.depth_srv);
  const auto object_velocity_resource = device->get_resource_from_view(resources.object_velocity_srv);
  if (color_resource.handle == 0u
      || resources.velocity_resource.handle == 0u
      || depth_resource.handle == 0u
      || object_velocity_resource.handle == 0u) {
    return false;
  }

  const auto color_description = device->get_resource_desc(color_resource);
  const auto velocity_description = device->get_resource_desc(resources.velocity_resource);
  const auto depth_description = device->get_resource_desc(depth_resource);
  const auto object_velocity_description = device->get_resource_desc(object_velocity_resource);
  const uint32_t width = color_description.texture.width;
  const uint32_t height = color_description.texture.height;
  const bool matching_resources = IsSingleSampleTexture(color_description)
                                  && IsSingleSampleTexture(velocity_description)
                                  && IsSingleSampleTexture(depth_description)
                                  && IsSingleSampleTexture(object_velocity_description)
                                  && velocity_description.texture.width == width
                                  && velocity_description.texture.height == height
                                  && depth_description.texture.width == width
                                  && depth_description.texture.height == height
                                  && object_velocity_description.texture.width == width
                                  && object_velocity_description.texture.height == height;
  if (!matching_resources) {
    if (!logged_non_full_resolution_candidate) {
      logged_non_full_resolution_candidate = true;
      logging::Info("skipping non-full-resolution temporal insertion candidate insertion=", insertion_name,
                    " color=", width, "x", height,
                    " velocity=", velocity_description.texture.width, "x", velocity_description.texture.height,
                    " depth=", depth_description.texture.width, "x", depth_description.texture.height,
                    " object_velocity=", object_velocity_description.texture.width, "x",
                    object_velocity_description.texture.height);
    }
    return false;
  }
  state::frame_state.full_resolution_candidate_seen = true;

  const uint64_t frame_token = state::CurrentFrameToken();
  const uint32_t sample_index = state::CurrentSampleIndex();
  const bool capture_frame_matches = resources.capture_frame == frame_token
                                     || (frame_token != 0u && resources.capture_frame == frame_token - 1u);
  const bool camera_frame_matches = resources.camera.frame_token == frame_token
                                    || (frame_token != 0u && resources.camera.frame_token == frame_token - 1u);
  if (!capture_frame_matches
      || resources.capture_sample_index != sample_index
      || !resources.camera.valid
      || !camera_frame_matches
      || resources.camera.sample_index != sample_index) {
    if (!resources.camera.valid && sample_index == 0u) {
      if (!logged_waiting_for_camera) {
        logged_waiting_for_camera = true;
        logging::Info("waiting for first native camera publication insertion=", insertion_name,
                      " frame=", frame_token);
      }
    } else if (LogEvery(last_stale_capture_log)) {
      logging::Warn("insertion reached with stale temporal capture insertion=", insertion_name,
                    " capture_frame=", resources.capture_frame,
                    " camera_frame=", resources.camera.frame_token,
                    " frame=", frame_token,
                    " capture_sample=", resources.capture_sample_index,
                    " camera_sample=", resources.camera.sample_index,
                    " sample=", sample_index);
    }
    return false;
  }
  if (!resources.camera.camera_matrix_valid
      || resources.camera.width != width
      || resources.camera.height != height) {
    if (LogEvery(last_missing_camera_log, 30u)) {
      logging::Warn("rejecting temporal dispatch with mismatched native camera insertion=", insertion_name,
                    " camera_size=", resources.camera.width, "x", resources.camera.height,
                    " color_size=", width, "x", height,
                    " frame=", frame_token);
    }
    return false;
  }

  output = {
      .cmd_list = cmd_list,
      .device = device,
      .color_srv = color_srv,
      .color_resource = color_resource,
      .color_description = color_description,
      .color_format = GetTypedViewFormat(device, color_srv),
      .velocity_srv = resources.velocity_srv,
      .velocity_resource = resources.velocity_resource,
      .depth_srv = resources.depth_srv,
      .depth_resource = depth_resource,
      .depth_format = GetTypedViewFormat(device, resources.depth_srv),
      .object_velocity_srv = resources.object_velocity_srv,
      .camera = resources.camera,
      .frame_token = frame_token,
      .sample_index = sample_index,
      .width = width,
      .height = height,
      .insertion_name = insertion_name,
  };
  return true;
}

inline void OnDestroyResourceView(renodx::utils::resource::ResourceViewInfo* info) {
  if (info != nullptr) QueueDestroyedResourceView(info->view.handle);
}

}  // namespace taa::input_capture
