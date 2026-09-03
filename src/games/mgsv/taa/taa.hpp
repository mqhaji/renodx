#pragma once

/*
 * ReShade lifecycle and insertion routing for MGSV temporal reconstruction.
 * The primary insertion is pre-DoF; coordinator.hpp validates one immutable
 * game-native frame and dispatches the selected reconstruction method.
 */

#include <windows.h>

#include <cstdint>

#include <include/reshade.hpp>

#include "../../../utils/pipeline_layout.hpp"
#include "../../../utils/resource.hpp"
#include "../../../utils/shader.hpp"
#include "../../../utils/state.hpp"
#include "../shared.h"
#include "./runtime/coordinator.hpp"
#include "./runtime/descriptor_tracker.hpp"
#include "./runtime/input_capture.hpp"
#include "./runtime/logging.hpp"
#include "./runtime/projection_jitter.hpp"
#include "./runtime/state.hpp"
#include "./settings.hpp"

namespace taa {

inline bool attached = false;
inline bool logged_tonemap = false;
inline bool logged_tonemap_lut = false;
inline bool logged_scatter_bake = false;
inline bool logged_dof_gate = false;
inline bool logged_mb_gate = false;
inline bool logged_camera_velocity = false;
inline bool logged_gbuffer_velocity = false;
inline bool logged_gbuffer_masked_velocity = false;

using PrepareVelocityTarget = reshade::api::resource_view (*)(
    reshade::api::command_list*,
    reshade::api::resource_view);
inline PrepareVelocityTarget prepare_velocity_target = nullptr;

namespace shader_hashes {

// Primary insertion hash. MGSV can issue this shader with both full-resolution
// scene color and lower-resolution DoF inputs, so the resolve accepts only the
// draw whose SRV t0 dimensions match captured depth and motion.
inline constexpr uint32_t DOF_SCATTER_BAKE_FIRST_PS = 0xFE1DC3F8u;

// Later fallback insertion point: CopyRenderBuffer is invoked as the
// pre-DoF-Final snapshot and as the post-MB upsample in the same frame. Used
// only if both earlier full-res color gates did not fire. HandleDraw gates by
// frame-sequence markers (DoF or MB tile-prep having fired earlier in the
// frame).
inline constexpr uint32_t COPY_RENDER_BUFFER_PS = 0x83272BCBu;

// Last fallback: tonemap. Used when none of the above fired (e.g. both DoF
// and MB disabled in graphics options, menus, cutscenes).
inline constexpr uint32_t TONEMAP_PS = 0xE04D1471u;
inline constexpr uint32_t TONEMAP_1DLUT_PS = 0xC0C26E46u;

// Other post-effects observed but not used as insertion points. They
// operate on half-res downsamples that are already produced by the time
// they run; inserting at them would be too late.
inline constexpr uint32_t DOF_NEAR_PS = 0xE2D609B1u;
inline constexpr uint32_t DOF_FAR_PS = 0x7C017264u;
inline constexpr uint32_t DOF_FINAL_PS = 0xFC5542BBu;

// Velocity pipeline. These never become insertion points. MotionBlurCameraVelocity
// exposes the camera cbuffer at pixel b2 and writes the TAA velocity input to RTV0.
inline constexpr uint32_t CAMERA_VELOCITY_PS = 0xA13321B6u;
inline constexpr uint32_t GBUFFER_VELOCITY_VS = 0x1D2531B7u;
inline constexpr uint32_t GBUFFER_VELOCITY_PS = 0x9815404Fu;
inline constexpr uint32_t GBUFFER_MASKED_VELOCITY_VS = 0x7B809E72u;
inline constexpr uint32_t GBUFFER_MASKED_VELOCITY_PS = 0x58C10658u;
inline constexpr uint32_t MB_TILE_MAX_PS = 0xF05DCBFDu;
inline constexpr uint32_t MB_TILE_REFINE_PS = 0x512E2B48u;

}  // namespace shader_hashes

inline void LogObservedShader(const char* stage, const char* name, uint32_t hash, bool& logged) {
  if (logged) return;
  logged = true;
  logging::Info("observed ", stage, " shader ", name, " hash=", logging::Hex{hash});
}

inline reshade::api::resource_view CurrentRenderTarget0(reshade::api::command_list* cmd_list) {
  const auto* render_state = renodx::utils::state::GetCurrentState(cmd_list);
  if (render_state == nullptr || render_state->render_targets.empty()) return {0};
  return render_state->render_targets[0];
}

inline bool HandleDraw(reshade::api::command_list* cmd_list) {
  if (!state::IsEnabled()) return false;

  // MGSV issues thousands of draws per frame. Classify first so unrelated
  // draws never touch TAA command-list state or contend on the resolve lock.
  auto* shader_state = renodx::utils::shader::GetCurrentState(cmd_list);
  if (shader_state == nullptr) return false;

  const uint32_t vertex_hash = renodx::utils::shader::GetCurrentVertexShaderHash(shader_state);
  const uint32_t pixel_hash = renodx::utils::shader::GetCurrentPixelShaderHash(shader_state);
  const bool is_camera_velocity = pixel_hash == shader_hashes::CAMERA_VELOCITY_PS;
  const bool is_gbuffer_velocity = vertex_hash == shader_hashes::GBUFFER_VELOCITY_VS
                                   && pixel_hash == shader_hashes::GBUFFER_VELOCITY_PS;
  const bool is_gbuffer_masked_velocity = vertex_hash == shader_hashes::GBUFFER_MASKED_VELOCITY_VS
                                          && pixel_hash == shader_hashes::GBUFFER_MASKED_VELOCITY_PS;
  const bool is_dof_marker = pixel_hash == shader_hashes::DOF_NEAR_PS
                             || pixel_hash == shader_hashes::DOF_FAR_PS
                             || pixel_hash == shader_hashes::DOF_FINAL_PS;
  const bool is_mb_marker = pixel_hash == shader_hashes::MB_TILE_MAX_PS
                            || pixel_hash == shader_hashes::MB_TILE_REFINE_PS;
  const bool is_insertion = pixel_hash == shader_hashes::DOF_SCATTER_BAKE_FIRST_PS
                            || pixel_hash == shader_hashes::COPY_RENDER_BUFFER_PS
                            || pixel_hash == shader_hashes::TONEMAP_PS
                            || pixel_hash == shader_hashes::TONEMAP_1DLUT_PS;
  if (!is_camera_velocity
      && !is_gbuffer_velocity
      && !is_gbuffer_masked_velocity
      && !is_dof_marker
      && !is_mb_marker
      && !is_insertion) {
    return false;
  }

  auto* data = descriptor_tracker::Get(cmd_list);
  if (data == nullptr) return false;

  coordinator::ExecutionGuard execution_guard;
  if (!state::IsEnabled()) return false;

  if (is_gbuffer_velocity || is_gbuffer_masked_velocity) {
    if (prepare_velocity_target != nullptr) {
      prepare_velocity_target(cmd_list, CurrentRenderTarget0(cmd_list));
    }
    if (is_gbuffer_velocity) {
      LogObservedShader("vertex", "GBufferVelocity", vertex_hash, logged_gbuffer_velocity);
    } else {
      LogObservedShader("vertex", "GBufferMaskedVelocity", vertex_hash, logged_gbuffer_masked_velocity);
    }
  }

  if (is_camera_velocity) {
    LogObservedShader("pixel", "MotionBlurCameraVelocity", pixel_hash, logged_camera_velocity);
    auto velocity_rtv = CurrentRenderTarget0(cmd_list);
    if (prepare_velocity_target != nullptr) {
      velocity_rtv = prepare_velocity_target(cmd_list, velocity_rtv);
    }
    input_capture::CaptureCameraMotionLocked(cmd_list, *data, velocity_rtv);
  }

  // Sequence markers arm the later CopyRenderBuffer fallbacks. The DoF
  // composite passes follow the primary ScatterBakeFirst insertion point;
  // motion-blur tile preparation follows the DoF sequence. The next eligible
  // copy after either marker becomes that path's fallback. BeginFrame clears
  // both flags.
  if (is_dof_marker) {
    state::frame_state.dof_fired = true;
  }
  if (is_mb_marker) {
    state::frame_state.mb_tile_prep_fired = true;
  }

  if (state::frame_state.reconstruction_completed) return false;

  // Priority 1: DOF_ScatterBakeFirst. The shader hash is reused at multiple
  // resolutions. MaybeRun accepts the full-resolution scene-color draw and
  // leaves lower-resolution DoF candidates for the later fallback cascade.
  if (pixel_hash == shader_hashes::DOF_SCATTER_BAKE_FIRST_PS) {
    LogObservedShader("pixel", "DOF_ScatterBakeFirst", pixel_hash, logged_scatter_bake);
    coordinator::MaybeRunLocked(cmd_list, *data, "DOF_ScatterBakeFirst");
    return false;
  }

  // Priority 2 + 3: CRB fallbacks if ScatterBakeFirst was not observed.
  // After a DoF pass fires (priority 2) or after MB tile prep fires
  // (priority 3), the next CopyRenderBuffer is a valid insertion point.
  // Both gates run TAA on the same pixel SRV t0.
  if (pixel_hash == shader_hashes::COPY_RENDER_BUFFER_PS) {
    if (state::frame_state.dof_fired) {
      LogObservedShader("pixel", "CRB after DoF", pixel_hash, logged_dof_gate);
      coordinator::MaybeRunLocked(cmd_list, *data, "CopyRenderBufferAfterDoF");
      return false;
    }
    if (state::frame_state.mb_tile_prep_fired) {
      LogObservedShader("pixel", "CRB after MB tile prep", pixel_hash, logged_mb_gate);
      coordinator::MaybeRunLocked(cmd_list, *data, "CopyRenderBufferAfterMBTilePrep");
      return false;
    }
  }

  // Priority 4 (last fallback): tonemap. Used when none of the above
  // fired — e.g. menu screens, cutscene transitions, or graphics settings
  // with both DoF and motion blur disabled.
  const bool is_tonemap = pixel_hash == shader_hashes::TONEMAP_PS;
  const bool is_tonemap_lut = pixel_hash == shader_hashes::TONEMAP_1DLUT_PS;
  if (is_tonemap) LogObservedShader("pixel", "Tonemap", pixel_hash, logged_tonemap);
  if (is_tonemap_lut) LogObservedShader("pixel", "Tonemap_1DLUT", pixel_hash, logged_tonemap_lut);

  if (is_tonemap || is_tonemap_lut) {
    coordinator::MaybeRunLocked(cmd_list, *data, is_tonemap ? "Tonemap" : "Tonemap_1DLUT");
  }

  // Always return false: we never replace the original game draw, we just
  // schedule the compute resolve in front of it.
  return false;
}

inline bool OnDraw(
    reshade::api::command_list* cmd_list,
    uint32_t vertex_count,
    uint32_t instance_count,
    uint32_t first_vertex,
    uint32_t first_instance) {
  (void)vertex_count;
  (void)instance_count;
  (void)first_vertex;
  (void)first_instance;
  return HandleDraw(cmd_list);
}

inline bool OnDrawIndexed(
    reshade::api::command_list* cmd_list,
    uint32_t index_count,
    uint32_t instance_count,
    uint32_t first_index,
    int32_t vertex_offset,
    uint32_t first_instance) {
  (void)index_count;
  (void)instance_count;
  (void)first_index;
  (void)vertex_offset;
  (void)first_instance;
  return HandleDraw(cmd_list);
}

inline bool OnDrawOrDispatchIndirect(
    reshade::api::command_list* cmd_list,
    reshade::api::indirect_command type,
    reshade::api::resource buffer,
    uint64_t offset,
    uint32_t draw_count,
    uint32_t stride) {
  (void)type;
  (void)buffer;
  (void)offset;
  (void)draw_count;
  (void)stride;
  return HandleDraw(cmd_list);
}

inline void OnDestroyDevice(reshade::api::device* device) {
  settings::TransitionRuntimeEnabled(false, "device destroyed", true, false);
  coordinator::ExecutionGuard execution_guard;
  logging::Info("destroy device");
  projection_jitter::Detach();
  coordinator::Destroy(device);
}

inline void OnPresent(
    reshade::api::command_queue* queue,
    reshade::api::swapchain* swapchain,
    const reshade::api::rect* source_rect,
    const reshade::api::rect* dest_rect,
    uint32_t dirty_rect_count,
    const reshade::api::rect* dirty_rects) {
  (void)swapchain;
  (void)source_rect;
  (void)dest_rect;
  (void)dirty_rect_count;
  (void)dirty_rects;
  settings::ApplySettingsSnapshot();
  coordinator::ExecutionGuard execution_guard;
  auto* device = queue != nullptr ? queue->get_device() : nullptr;
  const bool enabled = state::IsEnabled();
  const auto method = state::GetReconstructionMethod();
  coordinator::ReleaseInactiveResources(device, enabled, method);
  if (enabled
      && state::frame_state.full_resolution_candidate_seen
      && !state::frame_state.reconstruction_completed) {
    // Candidate insertion draws may be lower-resolution DoF work. Preserve
    // history when no new full-resolution scene was submitted, and reset only
    // after a matching candidate was seen but no resolve completed.
    coordinator::ResetTemporalState("enabled frame ended without TAA resolve");
  }
  state::BeginFrame();
}

inline void Use(DWORD fdw_reason, ShaderInjectData* shader_injection) {
  renodx::utils::resource::Use(fdw_reason);
  renodx::utils::pipeline_layout::Use(fdw_reason);
  renodx::utils::shader::Use(fdw_reason);
  renodx::utils::state::Use(fdw_reason);
  switch (fdw_reason) {
    case DLL_PROCESS_ATTACH:
      if (attached) return;
      attached = true;
      state::enabled_binding =
          shader_injection != nullptr ? &shader_injection->custom_taa : &state::enabled;
      settings::ApplySettingsSnapshot();
      logging::Info("attach");
      logging::Info("initial runtime state enabled=", logging::Bool{state::IsEnabled()},
                    " jitter_pattern=", state::GetJitterPattern() == 0u ? "off" : "halton_8");
      projection_jitter::Use(fdw_reason);

      reshade::register_event<reshade::addon_event::init_command_list>(descriptor_tracker::OnInitCommandList);
      reshade::register_event<reshade::addon_event::destroy_command_list>(descriptor_tracker::OnDestroyCommandList);
      reshade::register_event<reshade::addon_event::reset_command_list>(descriptor_tracker::OnResetCommandList);
      reshade::register_event<reshade::addon_event::push_descriptors>(descriptor_tracker::OnPushDescriptors);
      reshade::register_event<reshade::addon_event::draw>(OnDraw);
      reshade::register_event<reshade::addon_event::draw_indexed>(OnDrawIndexed);
      reshade::register_event<reshade::addon_event::draw_or_dispatch_indirect>(OnDrawOrDispatchIndirect);
      reshade::register_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
      reshade::register_event<reshade::addon_event::present>(OnPresent);
      renodx::utils::resource::RegisterOnDestroyResourceViewInfoCallback(input_capture::OnDestroyResourceView);
      break;

    case DLL_PROCESS_DETACH:
      if (!attached) return;
      attached = false;
      logging::Info("detach");
      // DllMain holds the loader lock. Runtime transitions and resource
      // quiescence happen in destroy_device; process detach only unregisters.
      projection_jitter::Use(fdw_reason);

      reshade::unregister_event<reshade::addon_event::init_command_list>(descriptor_tracker::OnInitCommandList);
      reshade::unregister_event<reshade::addon_event::destroy_command_list>(descriptor_tracker::OnDestroyCommandList);
      reshade::unregister_event<reshade::addon_event::reset_command_list>(descriptor_tracker::OnResetCommandList);
      reshade::unregister_event<reshade::addon_event::push_descriptors>(descriptor_tracker::OnPushDescriptors);
      reshade::unregister_event<reshade::addon_event::draw>(OnDraw);
      reshade::unregister_event<reshade::addon_event::draw_indexed>(OnDrawIndexed);
      reshade::unregister_event<reshade::addon_event::draw_or_dispatch_indirect>(OnDrawOrDispatchIndirect);
      reshade::unregister_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
      reshade::unregister_event<reshade::addon_event::present>(OnPresent);
      renodx::utils::resource::UnregisterOnDestroyResourceViewInfoCallback(input_capture::OnDestroyResourceView);
      break;
  }
}

}  // namespace taa
