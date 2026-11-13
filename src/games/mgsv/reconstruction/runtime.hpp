#pragma once

/* MGSV draw routing, settings transitions, and ReShade lifecycle for temporal reconstruction. */

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

#include <intrin.h>

#include <include/reshade.hpp>

#include "../../../utils/mutex.hpp"
#include "../../../utils/settings.hpp"
#include "../../../utils/shader.hpp"
#include "../../../utils/state.hpp"
#include "../../../utils/swapchain.hpp"
#include "../rendering/inputs.hpp"
#include "../rendering/resource_upgrades.hpp"
#include "../shared.h"
#include "camera_state.hpp"
#include "coordinator.hpp"
#include "dlss/model.hpp"
#include "dlss/runtime.hpp"
#include "input_capture.hpp"
#include "logging.hpp"
#include "mip_bias.hpp"
#include "projection_jitter.hpp"
#include "resolution.hpp"
#include "state.hpp"

namespace mgsv::game::reconstruction {

namespace runtime = ::mgsv::reconstruction;

inline bool logged_tonemap = false;
inline bool logged_tonemap_lut = false;
inline bool logged_bloom_reduction = false;
inline bool logged_scatter_bake = false;
inline bool logged_dof_gate = false;
inline bool logged_mb_gate = false;
inline bool logged_camera_velocity = false;
inline bool logged_gbuffer_velocity = false;
inline bool logged_gbuffer_masked_velocity = false;

namespace shader_hashes {

// The first reduction of the completed scene prepares bloom. The source must
// have the scene-color role; this shader also reduces unrelated images.
inline constexpr uint32_t COPY_REDUCTION_X4_PS = 0x72C1D157u;

// First fallback. MGSV can issue this shader with both scene color and smaller
// DoF inputs, so validation still checks t0 against captured depth and motion.
inline constexpr uint32_t DOF_SCATTER_BAKE_FIRST_PS = 0xFE1DC3F8u;

// CopyRenderBuffer is a fallback after DoF composition or motion-blur tile
// preparation. Sequence markers distinguish these uses from unrelated copies.
inline constexpr uint32_t COPY_RENDER_BUFFER_PS = 0x83272BCBu;

// Late grading is the last fallback when no earlier scene insertion ran.
inline constexpr uint32_t TONEMAP_PS = 0xE04D1471u;
inline constexpr uint32_t TONEMAP_1DLUT_PS = 0xC0C26E46u;

// Other post-effects observed but not used as insertion points. They
// operate on half-res downsamples that are already produced by the time
// they run; inserting at them would be too late.
inline constexpr uint32_t DOF_NEAR_PS = 0xE2D609B1u;
inline constexpr uint32_t DOF_FAR_PS = 0x7C017264u;
inline constexpr uint32_t DOF_FINAL_PS = 0xFC5542BBu;

inline constexpr uint32_t MB_TILE_MAX_PS = 0xF05DCBFDu;
inline constexpr uint32_t MB_TILE_REFINE_PS = 0x512E2B48u;

}  // namespace shader_hashes

inline bool HandleDraw(reshade::api::command_list* cmd_list, [[maybe_unused]] const ShaderInjectData* shader_injection = nullptr) {
  if (cmd_list == nullptr) return false;
  auto* device = cmd_list->get_device();
  if (device == nullptr || device->get_api() != reshade::api::device_api::d3d11) return false;

  namespace upgrades = ::mgsv::rendering::resource_upgrades;
  using upgrades::VelocityProducer;

  // MGSV issues thousands of draws per frame. Classify first so unrelated
  // draws never touch TAA command-list state or contend on the resolve lock.
  auto* shader_state = renodx::utils::shader::GetCurrentState(cmd_list);
  if (shader_state == nullptr) return false;

  const uint32_t vertex_hash = renodx::utils::shader::GetCurrentVertexShaderHash(shader_state);
  const uint32_t pixel_hash = renodx::utils::shader::GetCurrentPixelShaderHash(shader_state);
  const auto velocity_producer = upgrades::IdentifyVelocityProducer(vertex_hash, pixel_hash);
  const bool is_dof_marker = pixel_hash == shader_hashes::DOF_NEAR_PS
                             || pixel_hash == shader_hashes::DOF_FAR_PS
                             || pixel_hash == shader_hashes::DOF_FINAL_PS;
  const bool is_mb_marker = pixel_hash == shader_hashes::MB_TILE_MAX_PS
                            || pixel_hash == shader_hashes::MB_TILE_REFINE_PS;
  const bool is_insertion = pixel_hash == shader_hashes::COPY_REDUCTION_X4_PS
                            || pixel_hash == shader_hashes::DOF_SCATTER_BAKE_FIRST_PS
                            || pixel_hash == shader_hashes::COPY_RENDER_BUFFER_PS
                            || pixel_hash == shader_hashes::TONEMAP_PS
                            || pixel_hash == shader_hashes::TONEMAP_1DLUT_PS;
  if (velocity_producer == VelocityProducer::NONE
      && !is_dof_marker
      && !is_mb_marker
      && !is_insertion) {
    return false;
  }

  if (velocity_producer == VelocityProducer::NONE && !runtime::state::IsEnabled()) return false;

  const runtime::coordinator::ExecutionGuard execution_guard;

  const auto log_observed_shader = [](const char* stage, const char* name, uint32_t hash, bool* logged) {
    if (*logged) return;
    *logged = true;
    runtime::logging::Info("observed ", stage, " shader ", name, " hash=", runtime::logging::Hex{hash});
  };
  if (velocity_producer == VelocityProducer::CAMERA && runtime::state::IsEnabled()) {
    log_observed_shader("pixel", "MotionBlurCameraVelocity", pixel_hash, &logged_camera_velocity);
  }

  reshade::api::resource_view velocity_rtv = {0};
  if (velocity_producer != VelocityProducer::NONE) {
    const auto* render_state = renodx::utils::state::GetCurrentState(cmd_list);
    if (render_state != nullptr && !render_state->render_targets.empty()) {
      velocity_rtv = render_state->render_targets[0];
    }
    velocity_rtv = upgrades::PrepareVelocityTarget(cmd_list, velocity_rtv, vertex_hash, pixel_hash);
  }

  const bool lighting_motion_needed = velocity_producer == VelocityProducer::CAMERA
                                      && lighting::temporal_inputs::enabled.load(std::memory_order_acquire);
  const auto record_lighting_motion = [&](inputs::Motion motion, const runtime::camera_state::CameraFrame& camera) {
    // This observation has its own submission ledger. It never promotes the
    // old lighting_motion compatibility cache or enables projection jitter.
    const bool reconstruction_enabled = runtime::state::IsEnabled();
    const uint64_t generation = motion.generation;
    lighting::temporal_inputs::Record(cmd_list, {
                                                    .device = device,
                                                    .generation = generation,
                                                    .motion = std::move(motion),
                                                    .motion_jitter_uv = (reconstruction_enabled
                                                                             ? std::array<float, 2>{camera.jitter_uv_x, camera.jitter_uv_y}
                                                                             : std::array<float, 2>{}),
                                                    .velocity_projection_jitter_scale = (reconstruction_enabled
                                                                                             ? runtime::state::GetProjectionJitterScale(runtime::state::ProjectionJitterPath::VELOCITY)
                                                                                             : 0.f),
                                                    .camera_publication = (reconstruction_enabled ? camera.publication_sequence : 0u),
                                                    .camera_generation = (reconstruction_enabled ? camera.reset_generation : 0u),
                                                    .camera_sample = (reconstruction_enabled ? camera.sample_index : 0u),
                                                    .jitter_valid = !reconstruction_enabled || (camera.valid && camera.camera_matrix_valid),
                                                    .kind = lighting::temporal_inputs::ProducerKind::MOTION,
                                                });
  };
  if (lighting_motion_needed && !runtime::state::IsEnabled()) {
    record_lighting_motion(inputs::CaptureMotion(cmd_list, velocity_rtv, false), {});
  }

  // Velocity precision and GI producer observation remain active with scene reconstruction Off.
  if (!runtime::state::IsEnabled()) return false;
  auto* data = ::mgsv::inputs::descriptor_tracker::Get(cmd_list);
  if (data == nullptr) return false;

  if (velocity_producer == VelocityProducer::GBUFFER) {
    log_observed_shader("vertex", "GBufferVelocity", vertex_hash, &logged_gbuffer_velocity);
  }
  if (velocity_producer == VelocityProducer::GBUFFER_MASKED) {
    log_observed_shader("vertex", "GBufferMaskedVelocity", vertex_hash, &logged_gbuffer_masked_velocity);
  }

  // MotionBlurCameraVelocity exposes pixel b2 and writes RTV0; capture stays at this pre-draw boundary.
  if (velocity_producer == VelocityProducer::CAMERA) {
    auto captured = runtime::input_capture::CaptureCameraMotionLocked(cmd_list, *data, velocity_rtv);
    if (lighting_motion_needed) {
      record_lighting_motion(std::move(captured.motion), captured.camera);
    }
  }

  // Sequence markers arm the later CopyRenderBuffer fallbacks. The DoF
  // composite passes follow the ScatterBakeFirst fallback;
  // motion-blur tile preparation follows the DoF sequence. The next eligible
  // copy after either marker becomes that path's fallback. BeginFrame clears
  // both flags.
  if (is_dof_marker) {
    runtime::state::frame_state.dof_fired = true;
  }
  if (is_mb_marker) {
    runtime::state::frame_state.mb_tile_prep_fired = true;
  }

  if (runtime::state::frame_state.reconstruction_completed) return false;

  // Reconstruct the source before bloom downsamples it. Role and frame-input
  // checks reject unrelated reductions and the subsequent smaller bloom images.
  if (pixel_hash == shader_hashes::COPY_REDUCTION_X4_PS) {
    if (upgrades::ResourceViewMatchesCloneTarget(data->pixel_srv_t0, &upgrades::scene_tonemap_upgrade_info)) {
      log_observed_shader("pixel", "scene reduction before bloom", pixel_hash, &logged_bloom_reduction);
      runtime::coordinator::MaybeRunLocked(cmd_list, *data, "BeforeBloom");
    }
    return false;
  }

  // The first DoF preparation remains a fallback for scenes without the bloom
  // reduction. Validation leaves smaller inputs to the later candidates.
  if (pixel_hash == shader_hashes::DOF_SCATTER_BAKE_FIRST_PS) {
    log_observed_shader("pixel", "DOF_ScatterBakeFirst", pixel_hash, &logged_scatter_bake);
    runtime::coordinator::MaybeRunLocked(cmd_list, *data, "DOF_ScatterBakeFirst");
    return false;
  }

  // Later CRB fallbacks if neither early insertion ran.
  // After a DoF pass fires or after MB tile prep fires,
  // the next eligible CopyRenderBuffer is a valid insertion point.
  // Both gates run TAA on the same pixel SRV t0.
  if (pixel_hash == shader_hashes::COPY_RENDER_BUFFER_PS) {
    if (runtime::state::frame_state.dof_fired) {
      log_observed_shader("pixel", "CRB after DoF", pixel_hash, &logged_dof_gate);
      runtime::coordinator::MaybeRunLocked(cmd_list, *data, "CopyRenderBufferAfterDoF");
      return false;
    }
    if (runtime::state::frame_state.mb_tile_prep_fired) {
      log_observed_shader("pixel", "CRB after MB tile prep", pixel_hash, &logged_mb_gate);
      runtime::coordinator::MaybeRunLocked(cmd_list, *data, "CopyRenderBufferAfterMBTilePrep");
      return false;
    }
  }

  // Last fallback: tonemap. Used when none of the above
  // fired — e.g. menu screens, cutscene transitions, or graphics settings
  // with both DoF and motion blur disabled.
  const bool is_tonemap = pixel_hash == shader_hashes::TONEMAP_PS;
  const bool is_tonemap_lut = pixel_hash == shader_hashes::TONEMAP_1DLUT_PS;
  if (is_tonemap) {
    log_observed_shader("pixel", "Tonemap", pixel_hash, &logged_tonemap);
  }
  if (is_tonemap_lut) {
    log_observed_shader("pixel", "Tonemap_1DLUT", pixel_hash, &logged_tonemap_lut);
  }

  if (is_tonemap || is_tonemap_lut) {
    runtime::coordinator::MaybeRunLocked(cmd_list, *data, is_tonemap ? "Tonemap" : "Tonemap_1DLUT");
  }

  // Always return false: we never replace the original game draw, we just
  // schedule the compute resolve in front of it.
  return false;
}

}  // namespace mgsv::game::reconstruction

namespace mgsv::reconstruction::settings {

// Addon-owned storage must stay at a fixed address while its one registry borrows bindings.
// Descriptor pointers borrow from that same registry and are valid only during its lifetime; do not rebind them.
struct State {
  renodx::utils::settings::Setting* temporal_mode_setting = nullptr;
  renodx::utils::settings::Setting* dlss_model_setting = nullptr;
  renodx::utils::settings::Setting* upscaling_quality_setting = nullptr;
  // Float-backed CPU preferences adapt Setting::binding's float*; runtime normalizes enum choices.
  float dlss_model = static_cast<float>(dlss::DEFAULT_MODEL);
  float jitter_pattern = static_cast<float>(state::DEFAULT_JITTER_PATTERN);
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
  float diagnostic_view = state::DEFAULT_DIAGNOSTIC_VIEW;
  float velocity_visualization_range = state::DEFAULT_VELOCITY_VISUALIZATION_RANGE;
  float object_motion_mode = static_cast<float>(state::DEFAULT_OBJECT_MOTION_MODE);
#endif
#if ENABLE_TAA_PROJECTION_JITTER_DIAGNOSTICS
  std::array<float, state::PROJECTION_JITTER_SLIDER_COUNT> projection_jitter_scales = state::DEFAULT_PROJECTION_JITTER_SCALES;
#endif
  float clip_tightness = state::DEFAULT_CLIP_TIGHTNESS;
  float history_clip_strength = state::DEFAULT_HISTORY_CLIP_STRENGTH;
  float current_frame_blend = state::DEFAULT_CURRENT_FRAME_BLEND;
  float upscaling_quality = static_cast<float>(resolution::Quality::AUTO);
};

inline bool UpdateSetting(renodx::utils::settings::Setting* setting, float value) {
  if (setting == nullptr) return false;
  if (!setting->HasNumericValue()) return false;
  const std::unique_lock lock(renodx::utils::mutex::global_mutex);
  setting->Set(value)->Write();
  return true;
}

}  // namespace mgsv::reconstruction::settings

namespace mgsv::reconstruction::transitions {

inline std::atomic_flag runtime_transition_lock = ATOMIC_FLAG_INIT;

using UnavailableReason = const char* (*)();

struct TemporalModeOption {
  state::TemporalMode mode;
  const char* label;
  // Return nullptr when available, or the reason this method cannot run.
  UnavailableReason unavailable_reason = nullptr;
};

inline constexpr std::array TEMPORAL_MODE_OPTIONS = {
    TemporalModeOption{
        .mode = state::TemporalMode::OFF,
        .label = "Off (Vanilla FXAA)",
    },
    TemporalModeOption{
        .mode = state::TemporalMode::ANALYTICAL_TAA,
        .label = "Analytical TAA",
    },
    TemporalModeOption{
        .mode = state::TemporalMode::AMD_FSR3,
        .label = "AMD FSR 3.1.5",
    },
    TemporalModeOption{
        .mode = state::TemporalMode::NVIDIA_DLSS,
        .label = "NVIDIA DLSS",
        .unavailable_reason = dlss::UnavailableReason,
    },
};

inline const TemporalModeOption* FindTemporalModeOption(state::TemporalMode mode) {
  const auto found = std::ranges::find_if(
      TEMPORAL_MODE_OPTIONS,
      [mode](const TemporalModeOption& option) { return option.mode == mode; });
  return found == TEMPORAL_MODE_OPTIONS.end() ? nullptr : &*found;
}

// Settings callbacks and preset changes can arrive on different threads.
struct RuntimeTransitionGuard {
  RuntimeTransitionGuard() {
    while (runtime_transition_lock.test_and_set(std::memory_order_acquire)) {
      _mm_pause();
    }
  }
  RuntimeTransitionGuard(const RuntimeTransitionGuard&) = delete;
  RuntimeTransitionGuard& operator=(const RuntimeTransitionGuard&) = delete;
  RuntimeTransitionGuard(RuntimeTransitionGuard&&) = delete;
  RuntimeTransitionGuard& operator=(RuntimeTransitionGuard&&) = delete;

  ~RuntimeTransitionGuard() {
    runtime_transition_lock.clear(std::memory_order_release);
  }
};

inline void TransitionTemporalMode(
    const settings::State& settings_state,
    float value,
    const char* reason,
    bool force_reset = false,
    bool verify_restoration = true) {
  const std::unique_lock settings_lock(renodx::utils::mutex::global_mutex);
  // A snapshot taken before a newer user choice must not revive its request.
  if (!force_reset && settings_state.temporal_mode_setting != nullptr && settings_state.temporal_mode_setting->GetValue() != value) return;
  auto mode = state::NormalizeTemporalMode(value);
  const bool requested_dlss = mode == state::TemporalMode::NVIDIA_DLSS;
  const bool activation_pending = requested_dlss && dlss::ActivationPending();
  const auto* option = FindTemporalModeOption(mode);
  const char* unavailable_reason = "Unknown temporal reconstruction method.";
  if (option != nullptr) {
    unavailable_reason = (option->unavailable_reason == nullptr
                              ? nullptr
                              : option->unavailable_reason());
  }
  if (unavailable_reason != nullptr || activation_pending) {
    if (!activation_pending) {
      logging::Warn("temporal mode unavailable mode=", static_cast<uint32_t>(mode), " reason=", unavailable_reason);
    }
    mode = state::TemporalMode::OFF;
    option = FindTemporalModeOption(mode);
    if (activation_pending) {
      unavailable_reason = nullptr;
    }
  }

  if (unavailable_reason != nullptr && settings_state.temporal_mode_setting != nullptr) {
    settings_state.temporal_mode_setting->Set(static_cast<float>(state::TemporalMode::OFF))->Write();
  }
  const float jitter_pattern = settings_state.jitter_pattern;

  const RuntimeTransitionGuard transition_guard;
  // Cancellation is visible even while waiting for in-flight addon work.
  // Pending DLSS keeps its stored value, but renders as Off until ready.
  dlss::activation_requested.store(requested_dlss && unavailable_reason == nullptr, std::memory_order_release);
  const bool enabled = mode != state::TemporalMode::OFF;
  const bool sdk_reconstruction = mode == state::TemporalMode::AMD_FSR3
                                  || mode == state::TemporalMode::NVIDIA_DLSS;
  const uint32_t effective_jitter_pattern = (sdk_reconstruction
                                                 ? 1u
                                                 : static_cast<uint32_t>(std::clamp(jitter_pattern, 0.f, 1.f)));
  const auto previous_mode = state::GetTemporalMode();
  const bool was_enabled = previous_mode != state::TemporalMode::OFF;
  if (previous_mode == mode
      && (!enabled || effective_jitter_pattern == state::GetJitterPattern())
      && !force_reset) {
    return;
  }
  const uint64_t transition_frame = state::CurrentFrameToken();
  const uint32_t completed_samples = state::CurrentSampleIndex();

  // Disable projection writes before waiting for an in-flight resolve. Enable
  // only after history/sample state is reset, so the native hook cannot publish
  // a sample from the previous temporal sequence.
  if (!enabled && was_enabled) {
    const camera_state::PublicationWriterGuard publication_guard;
    state::SetTemporalMode(state::TemporalMode::OFF);
    camera_state::InvalidateLocked();
    if (verify_restoration && projection_jitter::IsInstalled()) {
      projection_jitter::BeginProductionRestorationCheck();
    }
  }
  if (!verify_restoration) {
    projection_jitter::CancelProductionRestorationCheck();
  }

  const coordinator::ExecutionGuard execution_guard;
  if (mode == state::TemporalMode::NVIDIA_DLSS && !dlss::runtime_ready.load(std::memory_order_acquire)) return;
  const camera_state::PublicationWriterGuard publication_guard;
  if (enabled) {
    state::SetJitterPattern(effective_jitter_pattern);
  }
  coordinator::ResetTemporalStateWithPublicationLocked(reason);
  if (enabled) {
    projection_jitter::CancelProductionRestorationCheck();
    state::SetTemporalMode(mode);
  }
  if (settings_state.temporal_mode_setting != nullptr && settings_state.temporal_mode_setting->binding != nullptr) {
    *settings_state.temporal_mode_setting->binding = static_cast<float>(mode);
  }

  if (enabled) {
    logging::Info("temporal reconstruction selected mode=", option->label,
                  " reason=", reason,
                  " frame=", transition_frame,
                  " jitter_pattern=", effective_jitter_pattern == 0u ? "off" : "halton_8");
  } else {
    logging::Info("temporal reconstruction disabled reason=", reason,
                  " frame=", transition_frame,
                  " completed_samples=", completed_samples);
  }
}

inline void TransitionJitterPattern(float value) {
  const RuntimeTransitionGuard transition_guard;

  const uint32_t preference = static_cast<uint32_t>(std::clamp(value, 0.f, 1.f));
  const auto mode = state::GetTemporalMode();
  const bool sdk_reconstruction = mode == state::TemporalMode::AMD_FSR3
                                  || mode == state::TemporalMode::NVIDIA_DLSS;
  const uint32_t effective_pattern = (sdk_reconstruction
                                          ? 1u
                                          : preference);
  if (effective_pattern == state::GetJitterPattern()) return;

  // Resolve already uses execution -> publication ordering when committing
  // matrix history. Preserve that order so the hook cannot publish a sample
  // from the previous pattern while history/sample state is being reset.
  const coordinator::ExecutionGuard execution_guard;
  const camera_state::PublicationWriterGuard publication_guard;
  state::SetJitterPattern(effective_pattern);
  coordinator::ResetTemporalStateWithPublicationLocked("jitter pattern changed");

  logging::Info("TAA jitter pattern changed pattern=", effective_pattern);
}

inline void TransitionDlssModel(float value, const char* reason) {
  const dlss::Model model = dlss::NormalizeModel(value);
  const RuntimeTransitionGuard transition_guard;
  if (dlss::GetModel() == model) return;
  dlss::SetModel(model);
  if (state::GetTemporalMode() == state::TemporalMode::NVIDIA_DLSS) {
    const coordinator::ExecutionGuard execution_guard;
    const camera_state::PublicationWriterGuard publication_guard;
    coordinator::ResetTemporalStateWithPublicationLocked(reason);
  }
  const auto* option = dlss::FindModelOption(model);
  logging::Info("NVIDIA DLSS model changed model=", option == nullptr ? "unknown" : option->label);
}

inline void InvalidateHistoryForSetting(const char* reason) {
  const coordinator::ExecutionGuard execution_guard;
  coordinator::ResetTemporalState(reason);
}

#if ENABLE_TAA_PROJECTION_JITTER_DIAGNOSTICS
inline void SetProjectionJitterScale(
    state::ProjectionJitterPath path,
    float value,
    const char* reason) {
  if (static_cast<std::size_t>(path) >= state::PROJECTION_JITTER_SLIDER_COUNT) return;
  const RuntimeTransitionGuard transition_guard;
  const coordinator::ExecutionGuard execution_guard;
  const camera_state::PublicationWriterGuard publication_guard;
  if (state::runtime_projection_jitter_scales[static_cast<std::size_t>(path)].load(std::memory_order_acquire)
      == std::clamp(value, -2.f, 2.f)) return;
  state::SetProjectionJitterScale(path, value);
  coordinator::ResetTemporalStateWithPublicationLocked(reason);
}
#endif

inline void ApplySettingsSnapshot(const settings::State& settings_state) {
  struct BindingSnapshot {
    float temporal_mode = 0.f;
    float dlss_model = 0.f;
    float jitter_pattern = 0.f;
    float clip_tightness = 0.f;
    float history_clip_strength = 0.f;
    float current_frame_blend = 0.f;
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
    float diagnostic_view = 0.f;
    float velocity_visualization_range = 0.f;
    float object_motion_mode = 0.f;
#endif
#if ENABLE_TAA_PROJECTION_JITTER_DIAGNOSTICS
    std::array<float, state::PROJECTION_JITTER_SLIDER_COUNT> projection_jitter_scales = {};
#endif
  };

  BindingSnapshot snapshot = {};
  {
    const std::unique_lock settings_lock(renodx::utils::mutex::global_mutex);
    snapshot.temporal_mode = (settings_state.temporal_mode_setting == nullptr
                                  ? static_cast<float>(state::TemporalMode::OFF)
                                  : settings_state.temporal_mode_setting->GetValue());
    snapshot.dlss_model = settings_state.dlss_model;
    snapshot.jitter_pattern = settings_state.jitter_pattern;
    snapshot.clip_tightness = settings_state.clip_tightness;
    snapshot.history_clip_strength = settings_state.history_clip_strength;
    snapshot.current_frame_blend = settings_state.current_frame_blend;
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
    snapshot.diagnostic_view = settings_state.diagnostic_view;
    snapshot.velocity_visualization_range = settings_state.velocity_visualization_range;
    snapshot.object_motion_mode = settings_state.object_motion_mode;
#endif
  }

  TransitionDlssModel(snapshot.dlss_model, "DLSS model synchronized");
  TransitionTemporalMode(settings_state, snapshot.temporal_mode, "temporal mode synchronized");
  TransitionJitterPattern(snapshot.jitter_pattern);
  // Re-read projection preferences while holding the settings lock: a callback
  // may have superseded the earlier snapshot during the mode transitions.
  const std::unique_lock settings_lock(renodx::utils::mutex::global_mutex);
#if ENABLE_TAA_PROJECTION_JITTER_DIAGNOSTICS
  snapshot.projection_jitter_scales = settings_state.projection_jitter_scales;
#endif
  const RuntimeTransitionGuard transition_guard;
  const coordinator::ExecutionGuard execution_guard;
  const camera_state::PublicationWriterGuard publication_guard;
  bool history_settings_changed = state::GetClipTightness() != std::clamp(snapshot.clip_tightness, 0.f, 1.f)
                                  || state::GetHistoryClipStrength()
                                         != std::clamp(snapshot.history_clip_strength, 0.f, 1.f)
                                  || state::GetCurrentFrameBlend()
                                         != std::clamp(snapshot.current_frame_blend, 0.f, 1.f);
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
  history_settings_changed = history_settings_changed
                             || state::GetDiagnosticView()
                                    != static_cast<float>(static_cast<uint32_t>(
                                        std::clamp(snapshot.diagnostic_view, 0.f, 10.f)))
                             || state::GetObjectMotionMode()
                                    != static_cast<uint32_t>(
                                        std::clamp(snapshot.object_motion_mode, 0.f, 7.f));
#endif
#if ENABLE_TAA_PROJECTION_JITTER_DIAGNOSTICS
  if (state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA) {
    for (std::size_t index = 0u; index < snapshot.projection_jitter_scales.size(); ++index) {
      history_settings_changed = history_settings_changed
                                 || state::GetProjectionJitterScale(
                                        static_cast<state::ProjectionJitterPath>(index))
                                        != std::clamp(snapshot.projection_jitter_scales[index], -2.f, 2.f);
    }
  }
#endif
  state::SetClipTightness(snapshot.clip_tightness);
  state::SetHistoryClipStrength(snapshot.history_clip_strength);
  state::SetCurrentFrameBlend(snapshot.current_frame_blend);
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
  state::SetDiagnosticView(snapshot.diagnostic_view);
  state::SetVelocityVisualizationRange(snapshot.velocity_visualization_range);
  state::SetObjectMotionMode(snapshot.object_motion_mode);
#endif
#if ENABLE_TAA_PROJECTION_JITTER_DIAGNOSTICS
  for (std::size_t index = 0u; index < snapshot.projection_jitter_scales.size(); ++index) {
    state::SetProjectionJitterScale(
        static_cast<state::ProjectionJitterPath>(index),
        snapshot.projection_jitter_scales[index]);
  }
#endif
  if (history_settings_changed) {
    coordinator::ResetTemporalStateWithPublicationLocked("settings synchronized");
  }
}

}  // namespace mgsv::reconstruction::transitions

namespace mgsv::reconstruction {

inline bool attached = false;
// ReShade callbacks cannot capture context. This borrows the addon-owned state
// supplied once at attach; its address remains valid for the module's lifetime.
inline const settings::State* active_settings = nullptr;

inline void OnDestroyDevice(reshade::api::device* device) {
  {
    const coordinator::ExecutionGuard execution_guard;
    if (coordinator::IsForeignDevice(device)) return;
  }
  transitions::TransitionTemporalMode(
      *active_settings,
      static_cast<float>(state::TemporalMode::OFF),
      "device destroyed",
      true,
      false);
  coordinator::UninstallTemporalSubmissionBridge();
  logging::Info("destroy device");
  // Native hooks and pinned addon code survive device recreation. No unsafe
  // trampoline reclamation or in-flight wait occurs at device destruction.
  projection_jitter::StopAdmission();
  {
    const std::unique_lock lock(resolution::sizing_state.mutex);
    resolution::sizing_state.renderer = nullptr;
    resolution::sizing_state.requested = {};
    resolution::sizing_state.actual = {};
    ++resolution::sizing_state.generation;
  }
  {
    const coordinator::ExecutionGuard execution_guard;
    coordinator::Destroy(device);
  }
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
  auto* device = (queue != nullptr ? queue->get_device() : nullptr);
  {
    const coordinator::ExecutionGuard execution_guard;
    // Concurrent first Presents must not bind reconstruction to a different input device.
    if (!inputs::IsSelectedDevice(device)) return;
    if (!coordinator::BindRuntimeDevice(device)) return;
    dlss::CheckAdapter(device);
  }
  if (dlss::ConsumeFatalFailure()
      && state::GetTemporalMode() == state::TemporalMode::NVIDIA_DLSS) {
    settings::UpdateSetting(
        active_settings->temporal_mode_setting,
        static_cast<float>(state::TemporalMode::OFF));
    transitions::TransitionTemporalMode(
        *active_settings,
        static_cast<float>(state::TemporalMode::OFF),
        "DLSS runtime failure",
        true);
  }
  transitions::ApplySettingsSnapshot(*active_settings);
  if (coordinator::submission_failure_pending.exchange(false, std::memory_order_acq_rel)
      && state::IsEnabled()) {
    settings::UpdateSetting(
        active_settings->temporal_mode_setting,
        static_cast<float>(state::TemporalMode::OFF));
    transitions::TransitionTemporalMode(
        *active_settings,
        static_cast<float>(state::TemporalMode::OFF),
        "temporal submission failure",
        true);
  }
  bool activation_serviced = false;
  {
    const coordinator::ExecutionGuard execution_guard;
    const bool bridge_required = state::IsEnabled()
                                 || lighting::temporal_inputs::enabled.load(std::memory_order_acquire)
                                 || (dlss::activation_requested.load(std::memory_order_acquire)
                                     && dlss::ActivationPending());
    const bool bridge_ready = !bridge_required || coordinator::EnsureTemporalSubmissionBridge(device);
    if (!bridge_ready) {
      logging::Warn("failed to install temporal submission bridge");
      coordinator::submission_failure_pending.store(true, std::memory_order_release);
    }
    if (dlss::activation_requested.load(std::memory_order_acquire) && dlss::ActivationPending()) {
      activation_serviced = true;
      logging::Info("servicing requested NVIDIA DLSS activation");
      if (dlss::ProbeDevice(device)) {
        // NGX activation remains selection-driven; submission tracking is shared by all methods.
        if (dlss::activation_requested.load(std::memory_order_acquire)) {
          if (bridge_ready) {
            dlss::runtime_ready.store(true, std::memory_order_release);
            logging::Info("NVIDIA DLSS runtime ready; waiting for tracked deferred recording boundaries");
          } else {
            dlss::QueueFatalFailure();
          }
        }
      } else if (const char* reason = dlss::UnavailableReason(); reason != nullptr) {
        logging::Warn("requested NVIDIA DLSS activation unavailable: ", reason);
      }
    }
  }
  // Read the latest persisted choice, not the request from before the probe.
  if (activation_serviced) {
    transitions::ApplySettingsSnapshot(*active_settings);
  }
  const coordinator::ExecutionGuard execution_guard;
  const auto mode = state::GetTemporalMode();
  const auto display = renodx::utils::swapchain::GetBackBufferDesc(device);
  const auto previous_plan = resolution::Read();
  auto plan = resolution::MakePlan({.width = display.texture.width, .height = display.texture.height}, mode,
                                   resolution::quality.load(std::memory_order_acquire));
  if (!resolution::installed.load(std::memory_order_acquire) || resolution::failed.load(std::memory_order_acquire)) {
    plan = resolution::MakePlan(plan.output, mode, resolution::Quality::NATIVE_AA);
  }
  if (plan.IsScaled() && mode == state::TemporalMode::NVIDIA_DLSS) {
    if (previous_plan.output == plan.output && previous_plan.quality == plan.quality && previous_plan.mode == mode) {
      plan.render = previous_plan.render;
    } else if (dlss::runtime_ready.load(std::memory_order_acquire)) {
      NVSDK_NGX_Parameter* capabilities = nullptr;
      uint32_t minimum_width = 0u;
      uint32_t minimum_height = 0u;
      uint32_t maximum_width = 0u;
      uint32_t maximum_height = 0u;
      float sharpness = 0.f;
      const bool queried = NVSDK_NGX_SUCCEED(NVSDK_NGX_D3D11_GetCapabilityParameters(&capabilities))
                           && capabilities != nullptr
                           && NVSDK_NGX_SUCCEED(NGX_DLSS_GET_OPTIMAL_SETTINGS(
                               capabilities, plan.output.width, plan.output.height, dlss::GetPerformanceQuality(plan.quality),
                               &plan.render.width, &plan.render.height, &maximum_width, &maximum_height,
                               &minimum_width, &minimum_height, &sharpness));
      if (capabilities != nullptr) {
        NVSDK_NGX_D3D11_DestroyParameters(capabilities);
      }
      plan.render.width = std::min((plan.render.width + 1u) & ~1u, maximum_width & ~1u);
      plan.render.height = std::min((plan.render.height + 1u) & ~1u, maximum_height & ~1u);
      if (!queried || plan.render.width < minimum_width || plan.render.height < minimum_height
          || plan.render.width == 0u || plan.render.height == 0u
          || plan.render.width > plan.output.width || plan.render.height > plan.output.height) {
        resolution::Fail("DLSS did not provide a supported render size");
        plan = resolution::MakePlan(plan.output, mode, resolution::Quality::NATIVE_AA);
      }
    } else {
      plan = resolution::MakePlan(plan.output, mode, resolution::Quality::NATIVE_AA);
    }
  }
  uint64_t size_generation = 0u;
  bool size_changed = false;
  {
    const std::unique_lock lock(resolution::sizing_state.mutex);
    size_changed = resolution::sizing_state.requested != plan;
    resolution::sizing_state.requested = plan;
    size_generation = resolution::sizing_state.generation;
  }
  static uint64_t observed_size_generation = 0u;
  if (size_changed || observed_size_generation != size_generation) {
    observed_size_generation = size_generation;
    coordinator::ResetTemporalState("render/output size changed");
    if (previous_plan.IsScaled() && !plan.IsScaled()) {
      post_process::Reset();
    }
    if (size_changed) {
      logging::Info("upscaling plan render=", plan.render.width, "x", plan.render.height,
                    " output=", plan.output.width, "x", plan.output.height,
                    " quality=", resolution::QUALITY_LABELS[static_cast<size_t>(plan.quality)]);
    }
  }
  const auto actual = resolution::ReadActual();
  const uint32_t jitter_phases = plan.JitterPhaseCount(actual);
  state::runtime_jitter_phases.store(jitter_phases, std::memory_order_release);
  const float mip_bias = (resolution::failed.load(std::memory_order_acquire) ? 0.f : plan.MipBias(actual));
  resolution::mip_bias.store(mip_bias, std::memory_order_release);
  static float logged_mip_bias = 0.f;
  if (logged_mip_bias != mip_bias || size_changed) {
    logged_mip_bias = mip_bias;
    logging::Info("texture mip bias adjustment=", mip_bias, " jitter_phases=", jitter_phases);
  }
  coordinator::ReleaseInactiveResources(device, mode);
  static uint32_t missed_scaled_dispatches = 0u;
  if (!plan.IsScaled() || !state::frame_state.scaled_inputs_valid
      || state::frame_state.reconstruction_scheduled || state::frame_state.reconstruction_completed) {
    missed_scaled_dispatches = 0u;
  }
  if (mode != state::TemporalMode::OFF
      && state::frame_state.full_resolution_candidate_seen
      && !state::frame_state.reconstruction_scheduled
      && !state::frame_state.reconstruction_completed) {
    // Missing camera/motion during startup is not a permanent upscale failure.
    // Only count frames where checked scene inputs were actually available.
    if (plan.IsScaled() && state::frame_state.scaled_inputs_valid) {
      if (++missed_scaled_dispatches >= 8u) {
        resolution::Fail("qualified early reconstruction repeatedly could not execute");
      }
    } else {
      missed_scaled_dispatches = 0u;
    }
    // Candidate insertion draws may be lower-resolution DoF work. Preserve
    // history when no new full-resolution scene was submitted, and reset only
    // after a matching candidate was seen but no resolve completed.
    coordinator::ResetTemporalState("enabled frame ended without TAA resolve");
  }
  state::BeginFrame();
}

inline void Use(DWORD fdw_reason, const settings::State& settings_state) {
  switch (fdw_reason) {
    case DLL_PROCESS_ATTACH: {
      if (attached) return;
      active_settings = &settings_state;
      attached = true;
      transitions::ApplySettingsSnapshot(*active_settings);
      logging::Info("attach");
      const auto* option = transitions::FindTemporalModeOption(state::GetTemporalMode());
      logging::Info("initial temporal state enabled=", logging::Bool{state::IsEnabled()},
                    " mode=", option == nullptr ? "unknown" : option->label,
                    " jitter_pattern=", state::GetJitterPattern() == 0u ? "off" : "halton_2_3");
      logging::Info("DLSS startup=vendor-only; DLL inspection, NGX and hooks are selection-driven");
      projection_jitter::Use(fdw_reason);
      resolution::Use(fdw_reason);
      mip_bias::Use(fdw_reason);

      // These return before QI/locks until a temporal method installs the shared bridge.
      reshade::register_event<reshade::addon_event::init_command_list>(coordinator::OnInitCommandList);
      reshade::register_event<reshade::addon_event::destroy_command_list>(coordinator::OnDestroyCommandList);
      reshade::register_event<reshade::addon_event::execute_secondary_command_list>(
          coordinator::OnExecuteSecondaryCommandList);
      reshade::register_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
      reshade::register_event<reshade::addon_event::present>(OnPresent);
      break;
    }

    case DLL_PROCESS_DETACH:
      if (!attached) return;
      attached = false;
      logging::Info("detach");
      // DllMain holds the loader lock. Runtime transitions and resource
      // quiescence happen in destroy_device; process detach only unregisters.
      projection_jitter::Use(fdw_reason);
      resolution::Use(fdw_reason);
      mip_bias::Use(fdw_reason);

      reshade::unregister_event<reshade::addon_event::init_command_list>(coordinator::OnInitCommandList);
      reshade::unregister_event<reshade::addon_event::destroy_command_list>(coordinator::OnDestroyCommandList);
      reshade::unregister_event<reshade::addon_event::execute_secondary_command_list>(
          coordinator::OnExecuteSecondaryCommandList);
      reshade::unregister_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
      reshade::unregister_event<reshade::addon_event::present>(OnPresent);
      break;
  }
}

}  // namespace mgsv::reconstruction
