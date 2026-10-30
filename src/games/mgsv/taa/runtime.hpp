#pragma once

/* Settings state, transitions, and ReShade lifecycle for MGSV temporal reconstruction. */

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
#include "../../../utils/pipeline_layout.hpp"
#include "../../../utils/resource.hpp"
#include "../../../utils/settings.hpp"
#include "../../../utils/shader.hpp"
#include "../../../utils/state.hpp"
#include "../game/reconstruction.hpp"
#include "./dlss/model.hpp"
#include "./dlss/runtime.hpp"
#include "./runtime/camera_state.hpp"
#include "./runtime/coordinator.hpp"
#include "./runtime/input_capture.hpp"
#include "./runtime/logging.hpp"
#include "./runtime/projection_jitter.hpp"
#include "./runtime/state.hpp"

namespace mgsv::reconstruction::settings {

// Addon-owned storage must stay at a fixed address while its one registry borrows bindings.
// Descriptor pointers borrow from that same registry and are valid only during its lifetime; do not rebind them.
struct State {
  renodx::utils::settings::Setting* temporal_mode_setting = nullptr;
  renodx::utils::settings::Setting* dlss_model_setting = nullptr;
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
  // Return nullptr when available. Future DLSS/XeSS probes can return a
  // DLL-missing, unsupported-adapter, or combined diagnostic here.
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
#if ENABLE_TAA_PROJECTION_JITTER_DIAGNOSTICS
    snapshot.projection_jitter_scales = settings_state.projection_jitter_scales;
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
  coordinator::ReleaseInactiveResources(device, mode);
  if (mode != state::TemporalMode::OFF
      && state::frame_state.full_resolution_candidate_seen
      && !state::frame_state.reconstruction_scheduled
      && !state::frame_state.reconstruction_completed) {
    // Candidate insertion draws may be lower-resolution DoF work. Preserve
    // history when no new full-resolution scene was submitted, and reset only
    // after a matching candidate was seen but no resolve completed.
    coordinator::ResetTemporalState("enabled frame ended without TAA resolve");
  }
  state::BeginFrame();
}

inline void Use(DWORD fdw_reason, const settings::State& settings_state) {
  if (fdw_reason == DLL_PROCESS_ATTACH) {
    // RTV capture and compute-state preservation use the snapshot-backed compatibility accessor.
    renodx::utils::state::use_snapshot = true;
  }
  renodx::utils::resource::Use(fdw_reason);
  renodx::utils::pipeline_layout::Use(fdw_reason);
  renodx::utils::shader::Use(fdw_reason);
  renodx::utils::state::Use(fdw_reason);
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
                    " jitter_pattern=", state::GetJitterPattern() == 0u ? "off" : "halton_8");
      logging::Info("DLSS startup=vendor-only; DLL inspection, NGX and hooks are selection-driven");
      projection_jitter::Use(fdw_reason);

      reshade::register_event<reshade::addon_event::init_command_list>(descriptor_tracker::OnInitCommandList);
      reshade::register_event<reshade::addon_event::destroy_command_list>(descriptor_tracker::OnDestroyCommandList);
      reshade::register_event<reshade::addon_event::reset_command_list>(descriptor_tracker::OnResetCommandList);
      // These return before QI/locks until a temporal method installs the shared bridge.
      reshade::register_event<reshade::addon_event::init_command_list>(coordinator::OnInitCommandList);
      reshade::register_event<reshade::addon_event::destroy_command_list>(coordinator::OnDestroyCommandList);
      reshade::register_event<reshade::addon_event::execute_secondary_command_list>(
          coordinator::OnExecuteSecondaryCommandList);
      reshade::register_event<reshade::addon_event::push_descriptors>(descriptor_tracker::OnPushDescriptors);
      reshade::register_event<reshade::addon_event::draw>(mgsv::game::reconstruction::OnDraw);
      reshade::register_event<reshade::addon_event::draw_indexed>(mgsv::game::reconstruction::OnDrawIndexed);
      reshade::register_event<reshade::addon_event::draw_or_dispatch_indirect>(mgsv::game::reconstruction::OnDrawOrDispatchIndirect);
      reshade::register_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
      reshade::register_event<reshade::addon_event::present>(OnPresent);
      renodx::utils::resource::RegisterOnDestroyResourceViewInfoCallback(input_capture::OnDestroyResourceView);
      break;
    }

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
      reshade::unregister_event<reshade::addon_event::init_command_list>(coordinator::OnInitCommandList);
      reshade::unregister_event<reshade::addon_event::destroy_command_list>(coordinator::OnDestroyCommandList);
      reshade::unregister_event<reshade::addon_event::execute_secondary_command_list>(
          coordinator::OnExecuteSecondaryCommandList);
      reshade::unregister_event<reshade::addon_event::push_descriptors>(descriptor_tracker::OnPushDescriptors);
      reshade::unregister_event<reshade::addon_event::draw>(mgsv::game::reconstruction::OnDraw);
      reshade::unregister_event<reshade::addon_event::draw_indexed>(mgsv::game::reconstruction::OnDrawIndexed);
      reshade::unregister_event<reshade::addon_event::draw_or_dispatch_indirect>(mgsv::game::reconstruction::OnDrawOrDispatchIndirect);
      reshade::unregister_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
      reshade::unregister_event<reshade::addon_event::present>(OnPresent);
      renodx::utils::resource::UnregisterOnDestroyResourceViewInfoCallback(input_capture::OnDestroyResourceView);
      break;
  }
}

}  // namespace mgsv::reconstruction