#pragma once

/*
 * Entry point for the self-contained MGSV TAA integration.
 *
 * addon.cpp includes this header only; the rest of the runtime lives under
 * mgsv/taa/runtime. This file wires the RenoDX setting, registers ReShade
 * callbacks, and routes draw-time events into the native jitter and resolve modules.
 *
 * Modules:
 *   - constant_buffers: master enable, frame/sample state, jitter sequence.
 *   - descriptor_tracker: per-command-list pixel SRV state.
 *   - projection_jitter: native projection injection and applied-jitter publication.
 *   - resolve: compute resolve dispatch + ping-pong history.
 *
 * Insertion is gated by shader-hash and frame-sequence markers; see README.md.
 * The primary hook is DOF_ScatterBakeFirst, immediately before the game creates
 * its DoF and motion-blur inputs. Velocity/depth are captured from
 * MotionBlurCameraVelocity earlier in the same frame.
 */

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstdint>

#include <include/reshade.hpp>

#include "../../../utils/pipeline_layout.hpp"
#include "../../../utils/resource.hpp"
#include "../../../utils/settings.hpp"
#include "../../../utils/shader.hpp"
#include "../../../utils/state.hpp"
#include "../shared.h"
#include "./runtime/constant_buffers.hpp"
#include "./runtime/descriptor_tracker.hpp"
#include "./runtime/logging.hpp"
#include "./runtime/projection_jitter.hpp"
#include "./runtime/resolve.hpp"

namespace taa {

inline bool attached = false;
inline bool settings_appended = false;
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

inline std::atomic_flag runtime_transition_lock = ATOMIC_FLAG_INIT;

inline void SetRuntimeEnabled(bool enabled) {
  projection_jitter::LockPublicationWriter();
  if (constant_buffers::enabled_binding != nullptr) {
    *constant_buffers::enabled_binding = enabled ? 1.f : 0.f;
  }
  constant_buffers::SetEnabled(enabled);
  projection_jitter::InvalidateAppliedJitterLocked();
  projection_jitter::UnlockPublicationWriter();
}

inline void TransitionRuntimeEnabled(
    bool enabled,
    const char* reason,
    bool force_reset = false,
    bool verify_restoration = true) {
  while (runtime_transition_lock.test_and_set(std::memory_order_acquire)) {
    _mm_pause();
  }

  const bool was_enabled = constant_buffers::IsEnabled();
  if (was_enabled == enabled && !force_reset) {
    runtime_transition_lock.clear(std::memory_order_release);
    return;
  }

  // Disable projection writes before waiting for an in-flight resolve. Enable
  // only after history/sample state is reset, so the native hook cannot publish
  // a sample from the previous temporal sequence.
  if (!enabled && was_enabled) {
    SetRuntimeEnabled(false);
    if (verify_restoration && projection_jitter::IsInstalled()) {
      projection_jitter::BeginProductionRestorationCheck();
    }
  }
  if (!verify_restoration) {
    projection_jitter::CancelProductionRestorationCheck();
  }

  resolve::LockExecution();
  projection_jitter::InvalidateAppliedJitter();
  resolve::InvalidateHistory(reason);
  constant_buffers::ResetTemporalState();
  if (enabled) {
    projection_jitter::CancelProductionRestorationCheck();
    SetRuntimeEnabled(true);
  }
  resolve::UnlockExecution();

  logging::Info("persistent TAA ", enabled ? "enabled" : "disabled", " reason=", reason);
  runtime_transition_lock.clear(std::memory_order_release);
}

inline void SyncRuntimeEnabledFromBinding() {
  const bool desired = constant_buffers::enabled_binding != nullptr
                       && *constant_buffers::enabled_binding > 0.f;
  if (desired != constant_buffers::IsEnabled()) {
    TransitionRuntimeEnabled(desired, "setting synchronized");
  }
}

inline void TransitionJitterPattern(float value) {
  const uint32_t pattern = static_cast<uint32_t>(std::clamp(value, 0.f, 1.f));
  constant_buffers::jitter_pattern = static_cast<float>(pattern);
  if (pattern == constant_buffers::GetJitterPattern()) return;

  while (runtime_transition_lock.test_and_set(std::memory_order_acquire)) {
    _mm_pause();
  }

  // Resolve already uses execution -> publication ordering when committing
  // matrix history. Preserve that order so the hook cannot publish a sample
  // from the previous pattern while history/sample state is being reset.
  resolve::LockExecution();
  projection_jitter::LockPublicationWriter();
  constant_buffers::SetJitterPattern(static_cast<float>(pattern));
  projection_jitter::InvalidateAppliedJitterLocked();
  projection_jitter::UnlockPublicationWriter();
  resolve::InvalidateHistory("jitter pattern changed");
  constant_buffers::ResetTemporalState();
  resolve::UnlockExecution();

  logging::Info("TAA jitter pattern changed pattern=", pattern);
  runtime_transition_lock.clear(std::memory_order_release);
}

inline void SyncJitterPatternFromBinding() {
  const uint32_t desired = static_cast<uint32_t>(std::clamp(constant_buffers::jitter_pattern, 0.f, 1.f));
  constant_buffers::jitter_pattern = static_cast<float>(desired);
  if (desired != constant_buffers::GetJitterPattern()) {
    TransitionJitterPattern(static_cast<float>(desired));
  }
}

namespace shader_hashes {

// Primary insertion point: DOF_ScatterBakeFirst is an MRT pass that
// reads the full-res HDR scene at SRV t0 and writes BOTH the DoF half-res
// pyramid base AND the motion-blur half-res scene input in a single draw.
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
  const auto* state = renodx::utils::state::GetCurrentState(cmd_list);
  if (state == nullptr || state->render_targets.empty()) return {0};
  return state->render_targets[0];
}

inline void UpdateShaderInjectionJitter(ShaderInjectData* shader_injection) {
  if (shader_injection == nullptr) return;

  float jitter_uv_x = 0.f;
  float jitter_uv_y = 0.f;
  const uint64_t frame_token = constant_buffers::CurrentFrameToken();
  const auto jitter = projection_jitter::GetAppliedJitter();
  if (constant_buffers::IsEnabled()) {
    // The temporal resolve advances CurrentSampleIndex after dispatch. Draws
    // recorded later in the same frame must still use the publication for the
    // current frame (sample n), not the already-promoted next sample (n + 1).
    if (jitter.valid
        && jitter.frame_token == frame_token) {
      jitter_uv_x = jitter.jitter_uv_x;
      jitter_uv_y = jitter.jitter_uv_y;
    }
  }

  shader_injection->taa_jitter_uv_x = jitter_uv_x;
  shader_injection->taa_jitter_uv_y = jitter_uv_y;
  shader_injection->taa_jitter_padding = 0.f;
  shader_injection->taa_jitter_padding_2 = 0.f;
}

inline void AppendSettings(renodx::utils::settings::Settings& settings, ShaderInjectData* shader_injection) {
  if (settings_appended || shader_injection == nullptr) return;
  settings_appended = true;

  std::vector<renodx::utils::settings::Setting*> taa_settings = {
      new renodx::utils::settings::Setting{
          .key = "FxTaa",
          .binding = &shader_injection->custom_taa,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 0.f,
          .label = "Temporal Anti-Aliasing",
          .section = "Effects",
          .on_change_value = [](float previous, float current) {
            (void)previous;
            TransitionRuntimeEnabled(current > 0.f, "setting changed");
          },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaDiagnosticView",
          .binding = &constant_buffers::diagnostic_view,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 0.f,
          .label = "TAA Diagnostic View",
          .section = "Effects",
          .tooltip = "Temporal Resolve is normal output. Raw Current bypasses temporal history and the current filter; Filtered Current isolates the 3x3 current-frame filter. Raw Velocity views inspect the center pixel of MGSV's captured velocity texture. Changing modes resets temporal history.",
          .labels = {"Temporal Resolve", "Raw Current", "Filtered Current", "Raw Velocity Direction", "Raw Velocity Magnitude"},
          .on_change_value = [](float previous, float current) {
            (void)previous;
            constant_buffers::SetDiagnosticView(current);
            resolve::LockExecution();
            resolve::InvalidateHistory("diagnostic view changed");
            resolve::UnlockExecution(); },
          .is_visible = [] { return constant_buffers::IsEnabled(); },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaJitterPattern",
          .binding = &constant_buffers::jitter_pattern,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = 1.f,
          .label = "TAA Jitter Pattern",
          .section = "Effects",
          .tooltip = "Diagnostic projection sampling pattern. Off keeps temporal resolve active with zero projection jitter. Halton is the eight-phase production sequence. Changing modes resets temporal history and the sample sequence.",
          .labels = {"Off", "Halton (2,3) — 8 Phase"},
          .on_change_value = [](float previous, float current) {
            if (static_cast<uint32_t>(previous) == static_cast<uint32_t>(current)) return;
            TransitionJitterPattern(current); },
          .is_visible = [] { return constant_buffers::IsEnabled(); },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaVelocityVisualizationRange",
          .binding = &constant_buffers::velocity_visualization_range,
          .value_type = renodx::utils::settings::SettingValueType::FLOAT,
          .default_value = 8.f,
          .label = "TAA Velocity View Range",
          .section = "Effects",
          .tooltip = "Pixel velocity represented by full intensity in the direction and magnitude diagnostic views.",
          .min = 0.25f,
          .max = 64.f,
          .format = "%.2f px",
          .on_change_value = [](float previous, float current) {
            (void)previous;
            constant_buffers::SetVelocityVisualizationRange(current); },
          .is_visible = [] {
            const float view = constant_buffers::GetDiagnosticView();
            return constant_buffers::IsEnabled() && view >= 3.f && view < 5.f; },
          .is_logarithmic = true,
      },
  };

  settings.insert(
      std::find_if(settings.begin(), settings.end(), [](const renodx::utils::settings::Setting* setting) {
        return setting != nullptr && setting->section == "Options";
      }),
      taa_settings.begin(), taa_settings.end());
}

inline void OnPresetOff() {
  renodx::utils::settings::UpdateSetting("FxTaa", 0.f);
  TransitionRuntimeEnabled(false, "preset off", true);
}

inline bool HandleDraw(reshade::api::command_list* cmd_list) {
  auto* data = descriptor_tracker::Get(cmd_list);
  if (data == nullptr) return false;

  if (!constant_buffers::IsEnabled()) return false;
  resolve::ExecutionGuard execution_guard;
  if (!constant_buffers::IsEnabled()) return false;

  auto* shader_state = renodx::utils::shader::GetCurrentState(cmd_list);
  if (shader_state == nullptr) return false;

  const uint32_t vertex_hash = renodx::utils::shader::GetCurrentVertexShaderHash(shader_state);
  const uint32_t pixel_hash = renodx::utils::shader::GetCurrentPixelShaderHash(shader_state);
  const bool is_camera_velocity = pixel_hash == shader_hashes::CAMERA_VELOCITY_PS;
  const bool is_gbuffer_velocity = vertex_hash == shader_hashes::GBUFFER_VELOCITY_VS
                                   && pixel_hash == shader_hashes::GBUFFER_VELOCITY_PS;
  const bool is_gbuffer_masked_velocity = vertex_hash == shader_hashes::GBUFFER_MASKED_VELOCITY_VS
                                          && pixel_hash == shader_hashes::GBUFFER_MASKED_VELOCITY_PS;

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
    resolve::CaptureCameraMotion(cmd_list, *data, velocity_rtv);
  }

  // Sequence markers arm the later CopyRenderBuffer fallbacks. The DoF
  // composite passes follow the primary ScatterBakeFirst insertion point;
  // motion-blur tile preparation follows the DoF sequence. The next eligible
  // copy after either marker becomes that path's fallback. BeginFrame clears
  // both flags.
  if (pixel_hash == shader_hashes::DOF_NEAR_PS
      || pixel_hash == shader_hashes::DOF_FAR_PS
      || pixel_hash == shader_hashes::DOF_FINAL_PS) {
    constant_buffers::frame_state.dof_fired = true;
  }
  if (pixel_hash == shader_hashes::MB_TILE_MAX_PS
      || pixel_hash == shader_hashes::MB_TILE_REFINE_PS) {
    constant_buffers::frame_state.mb_tile_prep_fired = true;
  }

  if (constant_buffers::frame_state.taa_ran_this_frame) return false;

  // Priority 1: DOF_ScatterBakeFirst. This MRT pass reads the full-res
  // HDR scene and writes BOTH the DoF half-res pyramid base AND the motion-blur
  // half-res scene input. The shader hash itself is the gate — no sequence
  // flags needed. SRV t0 is the full-res HDR scene, which MaybeRun reads.
  if (pixel_hash == shader_hashes::DOF_SCATTER_BAKE_FIRST_PS) {
    LogObservedShader("pixel", "DOF_ScatterBakeFirst",
                      pixel_hash, logged_scatter_bake);
    resolve::MaybeRun(cmd_list, *data, "DOF_ScatterBakeFirst");
    return false;
  }

  // Priority 2 + 3: CRB fallbacks if ScatterBakeFirst was not observed.
  // After a DoF pass fires (priority 2) or after MB tile prep fires
  // (priority 3), the next CopyRenderBuffer is a valid insertion point.
  // Both gates run TAA on the same pixel SRV t0.
  if (pixel_hash == shader_hashes::COPY_RENDER_BUFFER_PS) {
    if (constant_buffers::frame_state.dof_fired) {
      LogObservedShader("pixel", "CRB after DoF", pixel_hash, logged_dof_gate);
      resolve::MaybeRun(cmd_list, *data, "CopyRenderBufferAfterDoF");
      return false;
    }
    if (constant_buffers::frame_state.mb_tile_prep_fired) {
      LogObservedShader("pixel", "CRB after MB tile prep", pixel_hash, logged_mb_gate);
      resolve::MaybeRun(cmd_list, *data, "CopyRenderBufferAfterMBTilePrep");
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
    resolve::MaybeRun(cmd_list, *data, is_tonemap ? "Tonemap" : "Tonemap_1DLUT");
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
  TransitionRuntimeEnabled(false, "device destroyed", true, false);
  resolve::LockExecution();
  logging::Info("destroy device");
  resolve::Destroy(device);
  resolve::UnlockExecution();
}

inline void OnPresent(
    reshade::api::command_queue* queue,
    reshade::api::swapchain* swapchain,
    const reshade::api::rect* source_rect,
    const reshade::api::rect* dest_rect,
    uint32_t dirty_rect_count,
    const reshade::api::rect* dirty_rects) {
  (void)queue;
  (void)swapchain;
  (void)source_rect;
  (void)dest_rect;
  (void)dirty_rect_count;
  (void)dirty_rects;
  SyncRuntimeEnabledFromBinding();
  SyncJitterPatternFromBinding();
  constant_buffers::SyncDiagnosticView();
  constant_buffers::SyncVelocityVisualizationRange();
  resolve::LockExecution();
  const bool taa_ran_this_frame = constant_buffers::frame_state.taa_ran_this_frame;
  if (constant_buffers::IsEnabled() && !taa_ran_this_frame) {
    resolve::InvalidateHistory("enabled frame ended without TAA resolve");
  }
  constant_buffers::BeginFrame();
  resolve::UnlockExecution();
}

inline void Use(DWORD fdw_reason, ShaderInjectData* shader_injection) {
  constant_buffers::enabled_binding =
      shader_injection != nullptr ? &shader_injection->custom_taa : &constant_buffers::enabled;
  constant_buffers::SyncEnabled();
  constant_buffers::SyncJitterPattern();
  constant_buffers::SyncDiagnosticView();
  constant_buffers::SyncVelocityVisualizationRange();

  renodx::utils::resource::Use(fdw_reason);
  renodx::utils::pipeline_layout::Use(fdw_reason);
  renodx::utils::shader::Use(fdw_reason);
  renodx::utils::state::Use(fdw_reason);

  switch (fdw_reason) {
    case DLL_PROCESS_ATTACH:
      if (attached) return;
      attached = true;
      logging::Info("attach");
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
      renodx::utils::resource::RegisterOnDestroyResourceViewInfoCallback(resolve::OnDestroyResourceView);
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
      renodx::utils::resource::UnregisterOnDestroyResourceViewInfoCallback(resolve::OnDestroyResourceView);
      break;
  }
}

}  // namespace taa
