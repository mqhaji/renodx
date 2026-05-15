#pragma once

/*
 * Entry point for the self-contained MGSV TAA integration.
 *
 * addon.cpp includes this header only; the rest of the runtime lives under
 * mgsv/taa/runtime. This file wires the RenoDX setting, registers ReShade
 * callbacks, and routes draw-time events into the jitter and resolve modules.
 *
 * Modules:
 *   - constant_buffers: master enable, frame state, jitter sequence.
 *   - descriptor_tracker: per-command-list pixel SRVs and b2 state.
 *   - jitter: patches CbScene matrices on unmap for projection jitter.
 *   - resolve: compute resolve dispatch + ping-pong history.
 *
 * Insertion is gated by shader-hash and frame-sequence markers; see
 * TAA_IMPLEMENTATION.md "Insertion-Point Strategy". Primary hook is deferred
 * one draw after LocalReflectionAddBuffer: the draw writes the largely formed
 * full-res scene RTV, then TAA resolves that RTV clone before subsequent draws.
 * DOF_ScatterBakeFirst remains a later fallback. Velocity/depth are captured
 * from MotionBlurCameraVelocity earlier in the same frame.
 */

#include <windows.h>
#include <algorithm>
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
#include "./runtime/jitter.hpp"
#include "./runtime/logging.hpp"
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
inline bool logged_local_reflection_gate = false;
inline uint64_t last_local_reflection_queue_log = UINT64_MAX;
inline uint64_t last_local_reflection_consume_log = UINT64_MAX;

namespace shader_hashes {

// PRIMARY insertion marker: this draw writes the largely formed full-res scene
// color RTV after local reflections have been added. Draw callbacks run before
// the game draw, so HandleDraw records RTV0 here and resolves it at the next
// draw callback, after LocalReflectionAddBuffer has actually executed.
inline constexpr uint32_t LOCAL_REFLECTION_ADD_BUFFER_PS = 0xDC3A84C3u;

// Later fallback insertion point: DOF_ScatterBakeFirst is an MRT pass that
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

// DR_VolFog_TppTonemap finalizes deferred lighting + volumetric fog into
// the main scene. Observed but not currently used as an insertion point;
// LocalReflectionAddBuffer is a later full-res scene-color marker.
inline constexpr uint32_t DR_VOL_FOG_TPP_TONEMAP_PS = 0x2EA8F13Fu;

// Other post-effects observed but not used as insertion points. They
// operate on half-res downsamples that are already produced by the time
// they run; inserting at them would be too late.
inline constexpr uint32_t DOF_NEAR_PS = 0xE2D609B1u;
inline constexpr uint32_t DOF_FAR_PS = 0x7C017264u;
inline constexpr uint32_t DOF_FINAL_PS = 0xFC5542BBu;
inline constexpr uint32_t MOTION_BLUR_MCGUIRE_PS = 0xBFC7D3C2u;

// Velocity pipeline. These never become insertion points. MotionBlurCameraVelocity
// exposes the camera cbuffer at pixel b2 and writes the TAA velocity input to RTV0.
inline constexpr uint32_t CAMERA_VELOCITY_PS = 0xA13321B6u;
inline constexpr uint32_t GBUFFER_VELOCITY_VS = 0x1D2531B7u;
inline constexpr uint32_t GBUFFER_VELOCITY_PS = 0x9815404Fu;
inline constexpr uint32_t GBUFFER_MASKED_VELOCITY_VS = 0x7B809E72u;
inline constexpr uint32_t GBUFFER_MASKED_VELOCITY_PS = 0x58C10658u;
inline constexpr uint32_t MB_TILE_MAX_PS = 0xF05DCBFDu;
inline constexpr uint32_t MB_TILE_REFINE_PS = 0x512E2B48u;

// Replaced or disabled when TAA is active.
inline constexpr uint32_t FXAA_PS = 0x900968FFu;

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

inline void AppendSettings(renodx::utils::settings::Settings& settings, ShaderInjectData* shader_injection) {
  if (settings_appended || shader_injection == nullptr) return;
  settings_appended = true;

  std::vector<renodx::utils::settings::Setting*> taa_settings = {
    new renodx::utils::settings::Setting{
      .key = "FxTaa",
      .binding = &shader_injection->custom_taa,
      .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
      .default_value = 1.f,
      .label = "Temporal Anti-Aliasing",
      .section = "Effects",
    },
    new renodx::utils::settings::Setting{
      .key = "FxTaaJitterScale",
      .binding = &constant_buffers::jitter_scale,
      .value_type = renodx::utils::settings::SettingValueType::FLOAT,
      .default_value = 1.f,
      .label = "TAA Jitter Scale",
      .section = "Effects",
      .tooltip = "Debug isolation. Set to 0 to disable projection jitter and matching jitter compensation; if warbling stops, the issue is jitter/velocity alignment.",
      .min = 0.f,
      .max = 1.f,
      .format = "%.2f",
      .on_change = [] { constant_buffers::ResetJitterHistory(); },
      .is_visible = [] { return constant_buffers::IsEnabled(); },
    },
    new renodx::utils::settings::Setting{
      .key = "FxTaaDebugReadout",
      .value_type = renodx::utils::settings::SettingValueType::CUSTOM,
      .can_reset = false,
      .label = "TAA Debug Readout",
      .section = "Effects",
      .on_draw = [] {
      const auto jitter_debug = jitter::GetDebugState();
      const auto frame = constant_buffers::frame_state;
      const auto next_jitter = constant_buffers::CurrentFrameJitter(
        jitter_debug.last_rt_width,
        jitter_debug.last_rt_height);

      ImGui::Text("TAA frame: %llu  ran: %s  sample: %u  jitter scale: %.2f",
            static_cast<unsigned long long>(frame.frame_index),
            frame.taa_ran_this_frame ? "yes" : "no",
            frame.taa_sample_index,
            constant_buffers::jitter_scale);
      ImGui::Text("History jitter UV: current %.9f, %.9f  previous %.9f, %.9f",
            frame.current_jitter[0], frame.current_jitter[1],
            frame.previous_jitter[0], frame.previous_jitter[1]);
      ImGui::Text("Next jitter UV: %.9f, %.9f  px: %.4f, %.4f",
            next_jitter[0], next_jitter[1],
            next_jitter[0] * static_cast<float>(jitter_debug.last_rt_width),
            next_jitter[1] * static_cast<float>(jitter_debug.last_rt_height));
      ImGui::Text("CbScene jitter: applied %u  skipped %u  last fullscreen: %s",
            jitter_debug.applied_count,
            jitter_debug.skipped_count,
            jitter_debug.last_fullscreen ? "yes" : "no");
      ImGui::Text("CbScene tracking: tracked %u  mapped %u  heuristic maps %u  last map size %llu",
        jitter_debug.tracked_count,
        jitter_debug.mapped_count,
        jitter_debug.heuristic_mapped_count,
        static_cast<unsigned long long>(jitter_debug.last_mapped_size));
      ImGui::Text("CbScene last source: tracked %s  camera-like %s",
            jitter_debug.last_tracked ? "yes" : "no",
            jitter_debug.last_camera_like ? "yes" : "no");
      ImGui::Text("Last applied px: %.4f, %.4f  projection: %.9f, %.9f",
            jitter_debug.last_jitter_pixels_x, jitter_debug.last_jitter_pixels_y,
            jitter_debug.last_projection_jitter_x, jitter_debug.last_projection_jitter_y);
      ImGui::Text("Last RT/view: %ux%u / %.1fx%.1f  CbScene: 0x%llX",
            jitter_debug.last_rt_width, jitter_debug.last_rt_height,
            jitter_debug.last_viewport_width, jitter_debug.last_viewport_height,
            static_cast<unsigned long long>(jitter_debug.last_resource_handle));
      return false;
      },
      .is_visible = [] { return constant_buffers::IsEnabled(); },
    },
  };

  settings.insert(
      std::find_if(settings.begin(), settings.end(), [](const renodx::utils::settings::Setting* setting) {
        return setting != nullptr && setting->section == "Options";
      }),
    taa_settings.begin(),
    taa_settings.end());
}

inline void OnPresetOff() {
  renodx::utils::settings::UpdateSetting("FxTaa", 0.f);
}

inline bool HandleDraw(reshade::api::command_list* cmd_list) {
  auto* data = descriptor_tracker::Get(cmd_list);
  if (data == nullptr) return false;

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

  if (is_camera_velocity) {
    LogObservedShader("pixel", "MotionBlurCameraVelocity", pixel_hash, logged_camera_velocity);
    resolve::CaptureCameraMotion(cmd_list, *data, CurrentRenderTarget0(cmd_list));
  }

  if (is_gbuffer_velocity) {
    LogObservedShader("vertex", "GBufferVelocity", vertex_hash, logged_gbuffer_velocity);
  }

  if (is_gbuffer_masked_velocity) {
    LogObservedShader("vertex", "GBufferMaskedVelocity", vertex_hash, logged_gbuffer_masked_velocity);
  }

  if (is_camera_velocity || is_gbuffer_velocity || is_gbuffer_masked_velocity) {
    jitter::CaptureConstantBuffers(*data, is_camera_velocity, is_gbuffer_velocity || is_gbuffer_masked_velocity);
  }

  // Sequence markers: setting these enables the CRB gate later in the
  // frame. DoF passes run in observed captures at draws 1188 / 1283 / 1285
  // (Near / Far / Final). MB tile prep at 1292 / 1293 — unique to MB.
  // BeginFrame clears both flags.
  if (pixel_hash == shader_hashes::DOF_NEAR_PS
      || pixel_hash == shader_hashes::DOF_FAR_PS
      || pixel_hash == shader_hashes::DOF_FINAL_PS) {
    constant_buffers::frame_state.dof_fired = true;
  }
  if (pixel_hash == shader_hashes::MB_TILE_MAX_PS
      || pixel_hash == shader_hashes::MB_TILE_REFINE_PS) {
    constant_buffers::frame_state.mb_tile_prep_fired = true;
  }

  if (data->pending_local_reflection_rtv.handle != 0u
      && data->pending_local_reflection_frame != constant_buffers::frame_state.frame_index) {
    data->pending_local_reflection_rtv = {0};
    data->pending_local_reflection_frame = UINT64_MAX;
  }

  if (data->pending_local_reflection_rtv.handle != 0u && !constant_buffers::frame_state.taa_ran_this_frame) {
    const auto rtv = data->pending_local_reflection_rtv;
    data->pending_local_reflection_rtv = {0};
    data->pending_local_reflection_frame = UINT64_MAX;
    if (logging::ShouldLogFrame(constant_buffers::frame_state.frame_index, last_local_reflection_consume_log, 30u)) {
      logging::Info("consuming deferred LocalReflectionAddBuffer insertion frame=", constant_buffers::frame_state.frame_index,
                    " rtv=", logging::Hex{rtv.handle});
    }
    resolve::MaybeRunOnRenderTarget(cmd_list, rtv, "LocalReflectionAddBuffer");
  }

  if (constant_buffers::frame_state.taa_ran_this_frame) return false;

  // Priority 1: LocalReflectionAddBuffer RTV. The draw callback is invoked
  // before the original game draw, so this branch only records RTV0. The next
  // draw callback resolves the saved RTV, which means LocalReflection's output
  // exists and the scene is no longer bound as the active output target.
  if (pixel_hash == shader_hashes::LOCAL_REFLECTION_ADD_BUFFER_PS) {
    LogObservedShader("pixel", "LocalReflectionAddBuffer", pixel_hash, logged_local_reflection_gate);
    data->pending_local_reflection_rtv = CurrentRenderTarget0(cmd_list);
    data->pending_local_reflection_frame = constant_buffers::frame_state.frame_index;
    if (logging::ShouldLogFrame(constant_buffers::frame_state.frame_index, last_local_reflection_queue_log, 30u)) {
      logging::Info("queued deferred LocalReflectionAddBuffer insertion frame=", constant_buffers::frame_state.frame_index,
                    " rtv=", logging::Hex{data->pending_local_reflection_rtv.handle});
    }
    return false;
  }

  // Priority 2: DOF_ScatterBakeFirst fallback. This MRT pass reads the full-res
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
  logging::Info("destroy device");
  resolve::Destroy(device);
  jitter::Reset();
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
  jitter::FinishFrame();
  constant_buffers::BeginFrame();
}

inline void Use(DWORD fdw_reason, ShaderInjectData* shader_injection) {
  constant_buffers::enabled_binding =
      shader_injection != nullptr ? &shader_injection->custom_taa : &constant_buffers::enabled;

  renodx::utils::resource::Use(fdw_reason);
  renodx::utils::pipeline_layout::Use(fdw_reason);
  renodx::utils::shader::Use(fdw_reason);
  renodx::utils::state::Use(fdw_reason);

  switch (fdw_reason) {
    case DLL_PROCESS_ATTACH:
      if (attached) return;
      attached = true;
      logging::Info("attach");

      reshade::register_event<reshade::addon_event::init_swapchain>(jitter::OnInitSwapchain);
      reshade::register_event<reshade::addon_event::destroy_swapchain>(jitter::OnDestroySwapchain);
      reshade::register_event<reshade::addon_event::init_command_list>(descriptor_tracker::OnInitCommandList);
      reshade::register_event<reshade::addon_event::destroy_command_list>(descriptor_tracker::OnDestroyCommandList);
      reshade::register_event<reshade::addon_event::reset_command_list>(descriptor_tracker::OnResetCommandList);
      reshade::register_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(jitter::OnBindRenderTargetsAndDepthStencil);
      reshade::register_event<reshade::addon_event::bind_viewports>(jitter::OnBindViewports);
      reshade::register_event<reshade::addon_event::push_descriptors>(descriptor_tracker::OnPushDescriptors);
      reshade::register_event<reshade::addon_event::map_buffer_region>(jitter::OnMapBufferRegion);
      reshade::register_event<reshade::addon_event::unmap_buffer_region>(jitter::OnUnmapBufferRegion);
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

      reshade::unregister_event<reshade::addon_event::init_swapchain>(jitter::OnInitSwapchain);
      reshade::unregister_event<reshade::addon_event::destroy_swapchain>(jitter::OnDestroySwapchain);
      reshade::unregister_event<reshade::addon_event::init_command_list>(descriptor_tracker::OnInitCommandList);
      reshade::unregister_event<reshade::addon_event::destroy_command_list>(descriptor_tracker::OnDestroyCommandList);
      reshade::unregister_event<reshade::addon_event::reset_command_list>(descriptor_tracker::OnResetCommandList);
      reshade::unregister_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(jitter::OnBindRenderTargetsAndDepthStencil);
      reshade::unregister_event<reshade::addon_event::bind_viewports>(jitter::OnBindViewports);
      reshade::unregister_event<reshade::addon_event::push_descriptors>(descriptor_tracker::OnPushDescriptors);
      reshade::unregister_event<reshade::addon_event::map_buffer_region>(jitter::OnMapBufferRegion);
      reshade::unregister_event<reshade::addon_event::unmap_buffer_region>(jitter::OnUnmapBufferRegion);
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
