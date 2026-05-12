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
 *   - descriptor_tracker: per-command-list pixel t0/t2/t3 and camera b2.
 *   - jitter: patches CbScene matrices on unmap for projection jitter.
 *   - resolve: compute resolve dispatch + ping-pong history.
 *
 * Insertion is gated by shader-hash and frame-sequence markers; see
 * TAA_IMPLEMENTATION.md "Insertion-Point Strategy". Primary hook is
 * DOF_ScatterBakeFirst, where TAA runs on the full-res scene SRV t0
 * before the bake downsamples it into both DoF and MB inputs. Velocity
 * and depth are bound at t2 / t3 at the same draw — no separate capture
 * pass needed.
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

namespace shader_hashes {

// PRIMARY insertion point: DOF_ScatterBakeFirst is an MRT pass that reads
// the full-res HDR scene at SRV t0 and writes BOTH the DoF half-res pyramid
// base AND the motion-blur half-res scene input in a single draw. Hooking
// here means TAA runs on the full-res scene, then the downsamples produce
// TAA-resolved DoF and MB inputs. One hook, both effects fixed.
inline constexpr uint32_t DOF_SCATTER_BAKE_FIRST_PS = 0xFE1DC3F8u;

// Fallback insertion point: CopyRenderBuffer is invoked as the pre-DoF-Final
// snapshot and as the post-MB upsample in the same frame. Used only if
// DOF_SCATTER_BAKE_FIRST_PS did not fire (e.g. a DoF variant with a
// different bake hash). HandleDraw gates by frame-sequence markers (DoF or
// MB tile-prep having fired earlier in the frame).
inline constexpr uint32_t COPY_RENDER_BUFFER_PS = 0x83272BCBu;

// Last fallback: tonemap. Used when none of the above fired (e.g. both DoF
// and MB disabled in graphics options, menus, cutscenes).
inline constexpr uint32_t TONEMAP_PS = 0xE04D1471u;
inline constexpr uint32_t TONEMAP_1DLUT_PS = 0xC0C26E46u;

// DR_VolFog_TppTonemap finalizes deferred lighting + volumetric fog into
// the main scene. The image is "largely formed" by this point, well before
// particles/decals composite on top. Currently observed but not used as an
// insertion point because DOF_SCATTER_BAKE_FIRST_PS covers the same cases
// with a better placement (after particles).
inline constexpr uint32_t DR_VOL_FOG_TPP_TONEMAP_PS = 0x2EA8F13Fu;

// Other post-effects observed but not used as insertion points. They
// operate on half-res downsamples that are already produced by the time
// they run; inserting at them would be too late.
inline constexpr uint32_t DOF_NEAR_PS = 0xE2D609B1u;
inline constexpr uint32_t DOF_FAR_PS = 0x7C017264u;
inline constexpr uint32_t DOF_FINAL_PS = 0xFC5542BBu;
inline constexpr uint32_t MOTION_BLUR_MCGUIRE_PS = 0xBFC7D3C2u;

// Velocity pipeline. These never become insertion points, but we identify
// the camera cbuffer via GBUFFER_VELOCITY_VS at vertex b2.
inline constexpr uint32_t CAMERA_VELOCITY_PS = 0xA13321B6u;
inline constexpr uint32_t GBUFFER_VELOCITY_VS = 0x1D2531B7u;
inline constexpr uint32_t GBUFFER_VELOCITY_PS = 0x9815404Fu;
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

inline void AppendSettings(renodx::utils::settings::Settings& settings, ShaderInjectData* shader_injection) {
  if (settings_appended || shader_injection == nullptr) return;
  settings_appended = true;

  settings.insert(
      std::find_if(settings.begin(), settings.end(), [](const renodx::utils::settings::Setting* setting) {
        return setting != nullptr && setting->section == "Options";
      }),
      new renodx::utils::settings::Setting{
          .key = "FxTaa",
          .binding = &shader_injection->custom_taa,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 1.f,
          .label = "Temporal Anti-Aliasing",
          .section = "Effects",
      });
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

  const uint32_t pixel_hash = renodx::utils::shader::GetCurrentPixelShaderHash(shader_state);

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

  if (constant_buffers::frame_state.taa_ran_this_frame) return false;

  // Priority 1: DOF_ScatterBakeFirst. This MRT pass reads the full-res HDR
  // scene and writes BOTH the DoF half-res pyramid base AND the motion-blur
  // half-res scene input. Hooking here means TAA runs on the full-res
  // scene once, then the downsamples produce TAA-resolved inputs for both
  // DoF and MB. The shader hash itself is the gate — no sequence flags
  // needed. SRV t0 is the full-res HDR scene, which MaybeRun reads.
  if (pixel_hash == shader_hashes::DOF_SCATTER_BAKE_FIRST_PS) {
    LogObservedShader("pixel", "DOF_ScatterBakeFirst",
                      pixel_hash, logged_scatter_bake);
    resolve::MaybeRun(cmd_list, *data);
    return false;
  }

  // Priority 2 + 3: CRB fallbacks if ScatterBakeFirst was not observed.
  // After a DoF pass fires (priority 2) or after MB tile prep fires
  // (priority 3), the next CopyRenderBuffer is a valid insertion point.
  // Both gates run TAA on the same pixel SRV t0.
  if (pixel_hash == shader_hashes::COPY_RENDER_BUFFER_PS) {
    if (constant_buffers::frame_state.dof_fired) {
      LogObservedShader("pixel", "CRB after DoF", pixel_hash, logged_dof_gate);
      resolve::MaybeRun(cmd_list, *data);
      return false;
    }
    if (constant_buffers::frame_state.mb_tile_prep_fired) {
      LogObservedShader("pixel", "CRB after MB tile prep", pixel_hash, logged_mb_gate);
      resolve::MaybeRun(cmd_list, *data);
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
    resolve::MaybeRun(cmd_list, *data);
  }

  // Always return false: we never replace the original game draw, we just
  // schedule the compute resolve in front of it.
  return false;
}

inline bool OnDraw(reshade::api::command_list* cmd_list, uint32_t, uint32_t, uint32_t, uint32_t) {
  return HandleDraw(cmd_list);
}

inline bool OnDrawIndexed(reshade::api::command_list* cmd_list, uint32_t, uint32_t, uint32_t, int32_t, uint32_t) {
  return HandleDraw(cmd_list);
}

inline bool OnDrawOrDispatchIndirect(
    reshade::api::command_list* cmd_list,
    reshade::api::indirect_command,
    reshade::api::resource,
    uint64_t,
    uint32_t,
    uint32_t) {
  return HandleDraw(cmd_list);
}

inline void OnDestroyDevice(reshade::api::device* device) {
  logging::Info("destroy device");
  resolve::Destroy(device);
  jitter::Reset();
}

inline void OnPresent(
    reshade::api::command_queue*,
    reshade::api::swapchain*,
    const reshade::api::rect*,
    const reshade::api::rect*,
    uint32_t,
    const reshade::api::rect*) {
  constant_buffers::BeginFrame();
}

inline void Use(DWORD fdw_reason, ShaderInjectData* shader_injection) {
  resolve::shader_injection = shader_injection;
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
