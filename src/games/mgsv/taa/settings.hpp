#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

#include <intrin.h>

#include "../../../utils/settings.hpp"
#include "../shared.h"
#include "./runtime/camera_state.hpp"
#include "./runtime/coordinator.hpp"
#include "./runtime/logging.hpp"
#include "./runtime/projection_jitter.hpp"
#include "./runtime/state.hpp"

namespace taa::settings {

inline std::atomic_flag runtime_transition_lock = ATOMIC_FLAG_INIT;
inline renodx::utils::settings::Setting* temporal_mode_setting = nullptr;

using UnavailableReason = const char* (*)();

struct TemporalModeOption {
  state::TemporalMode mode;
  const char* label;
  // Return nullptr when available. Future DLSS/XeSS probes can return a
  // DLL-missing, unsupported-adapter, or combined diagnostic here.
  UnavailableReason unavailable_reason = nullptr;
};

inline constexpr std::array TEMPORAL_MODE_OPTIONS = {
    TemporalModeOption{state::TemporalMode::OFF, "Off (Vanilla FXAA)"},
    TemporalModeOption{state::TemporalMode::ANALYTICAL_TAA, "Analytical TAA"},
    TemporalModeOption{state::TemporalMode::AMD_FSR3, "AMD FSR 3.1.5"},
};

inline const TemporalModeOption* FindTemporalModeOption(state::TemporalMode mode) {
  const auto found = std::find_if(
      TEMPORAL_MODE_OPTIONS.begin(),
      TEMPORAL_MODE_OPTIONS.end(),
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
    float value,
    const char* reason,
    bool force_reset = false,
    bool verify_restoration = true) {
  auto mode = state::NormalizeTemporalMode(value);
  const auto* option = FindTemporalModeOption(mode);
  const char* unavailable_reason = "Unknown temporal reconstruction method.";
  if (option != nullptr) {
    unavailable_reason = option->unavailable_reason == nullptr
                             ? nullptr
                             : option->unavailable_reason();
  }
  if (unavailable_reason != nullptr) {
    logging::Warn("temporal mode unavailable mode=", static_cast<uint32_t>(mode), " reason=", unavailable_reason);
    mode = state::TemporalMode::OFF;
    option = FindTemporalModeOption(mode);
  }

  const std::unique_lock settings_lock(renodx::utils::mutex::global_mutex);
  if (unavailable_reason != nullptr && temporal_mode_setting != nullptr) {
    temporal_mode_setting->Set(static_cast<float>(state::TemporalMode::OFF))->Write();
  }
  if (temporal_mode_setting != nullptr && temporal_mode_setting->binding != nullptr) {
    *temporal_mode_setting->binding = static_cast<float>(mode);
  }
  const float jitter_pattern = state::jitter_pattern;

  RuntimeTransitionGuard transition_guard;
  const bool enabled = mode != state::TemporalMode::OFF;
  const uint32_t effective_jitter_pattern = mode == state::TemporalMode::AMD_FSR3
                                                ? 1u
                                                : static_cast<uint32_t>(std::clamp(jitter_pattern, 0.f, 1.f));
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
    camera_state::PublicationWriterGuard publication_guard;
    state::SetTemporalMode(state::TemporalMode::OFF);
    camera_state::InvalidateLocked();
    if (verify_restoration && projection_jitter::IsInstalled()) {
      projection_jitter::BeginProductionRestorationCheck();
    }
  }
  if (!verify_restoration) {
    projection_jitter::CancelProductionRestorationCheck();
  }

  coordinator::ExecutionGuard execution_guard;
  camera_state::PublicationWriterGuard publication_guard;
  if (enabled) {
    state::SetJitterPattern(effective_jitter_pattern);
  }
  coordinator::ResetTemporalStateWithPublicationLocked(reason);
  if (enabled) {
    projection_jitter::CancelProductionRestorationCheck();
    state::SetTemporalMode(mode);
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
  RuntimeTransitionGuard transition_guard;

  const uint32_t preference = static_cast<uint32_t>(std::clamp(value, 0.f, 1.f));
  const uint32_t effective_pattern = state::GetTemporalMode() == state::TemporalMode::AMD_FSR3
                                         ? 1u
                                         : preference;
  if (effective_pattern == state::GetJitterPattern()) return;

  // Resolve already uses execution -> publication ordering when committing
  // matrix history. Preserve that order so the hook cannot publish a sample
  // from the previous pattern while history/sample state is being reset.
  coordinator::ExecutionGuard execution_guard;
  camera_state::PublicationWriterGuard publication_guard;
  state::SetJitterPattern(effective_pattern);
  coordinator::ResetTemporalStateWithPublicationLocked("jitter pattern changed");

  logging::Info("TAA jitter pattern changed pattern=", effective_pattern);
}

inline void InvalidateHistoryForSetting(const char* reason) {
  coordinator::ExecutionGuard execution_guard;
  coordinator::ResetTemporalState(reason);
}

#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
inline void SetProjectionJitterScale(
    state::ProjectionJitterPath path,
    float value,
    const char* reason) {
  state::SetProjectionJitterScale(path, value);
  InvalidateHistoryForSetting(reason);
}
#endif

struct BindingSnapshot {
  float temporal_mode = static_cast<float>(state::DEFAULT_TEMPORAL_MODE);
  float jitter_pattern = static_cast<float>(state::DEFAULT_JITTER_PATTERN);
  float clip_tightness = state::DEFAULT_CLIP_TIGHTNESS;
  float history_clip_strength = state::DEFAULT_HISTORY_CLIP_STRENGTH;
  float current_frame_blend = state::DEFAULT_CURRENT_FRAME_BLEND;
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
  float diagnostic_view = state::DEFAULT_DIAGNOSTIC_VIEW;
  float velocity_visualization_range = state::DEFAULT_VELOCITY_VISUALIZATION_RANGE;
  float object_motion_mode = static_cast<float>(state::DEFAULT_OBJECT_MOTION_MODE);
  std::array<float, state::PROJECTION_JITTER_PATH_COUNT> projection_jitter_scales = {};
#endif
};

inline void ApplySettingsSnapshot() {
  BindingSnapshot snapshot = {};
  {
    const std::unique_lock settings_lock(renodx::utils::mutex::global_mutex);
    snapshot.temporal_mode = temporal_mode_setting == nullptr
                                 ? static_cast<float>(state::TemporalMode::OFF)
                                 : temporal_mode_setting->GetValue();
    snapshot.jitter_pattern = state::jitter_pattern;
    snapshot.clip_tightness = state::clip_tightness;
    snapshot.history_clip_strength = state::history_clip_strength;
    snapshot.current_frame_blend = state::current_frame_blend;
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
    snapshot.diagnostic_view = state::diagnostic_view;
    snapshot.velocity_visualization_range = state::velocity_visualization_range;
    snapshot.object_motion_mode = state::object_motion_mode;
    snapshot.projection_jitter_scales = state::projection_jitter_scales;
#endif
  }

  TransitionTemporalMode(snapshot.temporal_mode, "temporal mode synchronized");
  TransitionJitterPattern(snapshot.jitter_pattern);
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
                                        std::clamp(snapshot.object_motion_mode, 0.f, 5.f));
  for (std::size_t index = 0u; index < snapshot.projection_jitter_scales.size(); ++index) {
    history_settings_changed = history_settings_changed
                               || state::GetProjectionJitterScale(
                                      static_cast<state::ProjectionJitterPath>(index))
                                      != std::clamp(snapshot.projection_jitter_scales[index], -2.f, 2.f);
  }
#endif
  state::SetClipTightness(snapshot.clip_tightness);
  state::SetHistoryClipStrength(snapshot.history_clip_strength);
  state::SetCurrentFrameBlend(snapshot.current_frame_blend);
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
  state::SetDiagnosticView(snapshot.diagnostic_view);
  state::SetVelocityVisualizationRange(snapshot.velocity_visualization_range);
  state::SetObjectMotionMode(snapshot.object_motion_mode);
  for (std::size_t index = 0u; index < snapshot.projection_jitter_scales.size(); ++index) {
    state::SetProjectionJitterScale(
        static_cast<state::ProjectionJitterPath>(index),
        snapshot.projection_jitter_scales[index]);
  }
#endif
  if (history_settings_changed) {
    InvalidateHistoryForSetting("settings synchronized");
  }
}

inline void MigrateTemporalModeSetting() {
  constexpr std::array<const char*, 3> preset_sections = {
      "renodx-preset1",
      "renodx-preset2",
      "renodx-preset3",
  };
  for (const char* section : preset_sections) {
    int temporal_mode_value = 0;
    if (reshade::get_config_value(
            nullptr,
            section,
            "FxTemporalReconstructionMode",
            temporal_mode_value)) {
      continue;
    }

    int enabled_value = 0;
    // FxTaa predates the method selectors and originally enabled Analytical TAA.
    constexpr int legacy_analytical_method = 0;
    int method_value = legacy_analytical_method;
    const bool has_enabled = reshade::get_config_value(nullptr, section, "FxTaa", enabled_value);
    const bool has_current_method = reshade::get_config_value(
        nullptr,
        section,
        "FxTemporalReconstructionMethod",
        method_value);
    int legacy_method_value = 0;
    const bool has_legacy_method = reshade::get_config_value(
        nullptr,
        section,
        "FxTaaReconstructionMethod",
        legacy_method_value);
    if (!has_enabled && !has_current_method && !has_legacy_method) {
      continue;
    }

    temporal_mode_value = static_cast<int>(state::TemporalMode::OFF);
    if (has_enabled && enabled_value > 0) {
      if (!has_current_method && has_legacy_method) {
        method_value = legacy_method_value;
      }
      temporal_mode_value = method_value == legacy_analytical_method
                                ? static_cast<int>(state::TemporalMode::ANALYTICAL_TAA)
                                : static_cast<int>(state::TemporalMode::AMD_FSR3);
    }
    reshade::set_config_value(
        nullptr,
        section,
        "FxTemporalReconstructionMode",
        temporal_mode_value);
  }
}

inline bool DrawTemporalModeSelector() {
  if (temporal_mode_setting == nullptr) return false;

  const auto current_mode = state::NormalizeTemporalMode(temporal_mode_setting->GetValue());
  const auto* current_option = FindTemporalModeOption(current_mode);
  const char* preview = current_option == nullptr ? "Unknown" : current_option->label;
  const bool combo_open = ImGui::BeginCombo("Temporal Reconstruction", preview);
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
    ImGui::SetTooltip(
        "Selects vanilla FXAA, analytical TAA, or AMD FSR 3.1.5. Changing modes resets temporal state.");
  }
  if (!combo_open) return false;

  bool changed = false;
  for (const auto& option : TEMPORAL_MODE_OPTIONS) {
    const char* unavailable_reason = option.unavailable_reason == nullptr
                                         ? nullptr
                                         : option.unavailable_reason();
    const bool available = unavailable_reason == nullptr;
    const bool selected = option.mode == current_mode;
    if (!available) ImGui::BeginDisabled();
    const bool chosen = ImGui::Selectable(option.label, selected);
    if (!available) ImGui::EndDisabled();

    if (!available
        && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::BeginTooltip();
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.2f, 0.2f, 1.f));
      ImGui::TextWrapped("%s", unavailable_reason);
      ImGui::PopStyleColor();
      ImGui::EndTooltip();
    }
    if (chosen && available && !selected) {
      changed = renodx::utils::settings::UpdateSetting(
          "FxTemporalReconstructionMode",
          static_cast<float>(option.mode));
      if (changed) {
        TransitionTemporalMode(static_cast<float>(option.mode), "temporal mode changed");
      }
    }
    if (selected) ImGui::SetItemDefaultFocus();
  }
  ImGui::EndCombo();
  return changed;
}

inline void AppendSettings(
    renodx::utils::settings::Settings& settings,
    ShaderInjectData* shader_injection) {
  if (temporal_mode_setting != nullptr || shader_injection == nullptr) return;
  MigrateTemporalModeSetting();

  temporal_mode_setting = new renodx::utils::settings::Setting{
      .key = "FxTemporalReconstructionMode",
      .binding = &shader_injection->custom_taa,
      .value_type = renodx::utils::settings::SettingValueType::INTEGER,
      .default_value = static_cast<float>(state::DEFAULT_TEMPORAL_MODE),
      .label = "Temporal Reconstruction Mode",
      .section = "Temporal Anti-Aliasing",
      .max = static_cast<float>(TEMPORAL_MODE_OPTIONS.size()) - 1.f,
      .is_visible = [] { return false; },
  };

  std::vector<renodx::utils::settings::Setting*> taa_settings = {
      temporal_mode_setting,
      new renodx::utils::settings::Setting{
          .value_type = renodx::utils::settings::SettingValueType::CUSTOM,
          .can_reset = false,
          .label = "Temporal Reconstruction",
          .section = "Temporal Anti-Aliasing",
          .on_draw = DrawTemporalModeSelector,
      },
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
      new renodx::utils::settings::Setting{
          .key = "FxTaaDiagnosticView",
          .binding = &state::diagnostic_view,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = state::DEFAULT_DIAGNOSTIC_VIEW,
          .label = "TAA Diagnostic View",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Compares current color, raw/final motion, object-mask coverage, selected motion, and native-object motion relative to matrix camera motion. Changing modes resets temporal history.",
          .labels = {
              "Temporal Resolve",
              "Raw Current",
              "Filtered Current",
              "Raw Velocity Direction",
              "Raw Velocity Magnitude",
              "Raw Object Mask",
              "Selected Object Mask",
              "Selected Velocity Direction",
              "Selected Velocity Magnitude",
              "Object-Camera Residual Direction",
              "Object-Camera Residual Magnitude",
          },
          .on_change_value = [](float previous, float current) {
            (void)previous;
            state::SetDiagnosticView(current);
            InvalidateHistoryForSetting("diagnostic view changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
#endif
      new renodx::utils::settings::Setting{
          .key = "FxTaaJitterPattern",
          .binding = &state::jitter_pattern,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = static_cast<float>(state::DEFAULT_JITTER_PATTERN),
          .label = "TAA Jitter Pattern",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Diagnostic projection sampling pattern. Off keeps analytical TAA active with zero projection jitter. "
                     "FSR3 enforces the eight-phase Halton sequence. Changing modes resets temporal history and the "
                     "sample sequence.",
          .labels = {"Off", "Halton (2,3) - 8 Phase"},
          .on_change_value = [](float previous, float current) {
            if (static_cast<uint32_t>(previous) == static_cast<uint32_t>(current)) return;
            TransitionJitterPattern(current); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaUnclampMotionVectors",
          .binding = &shader_injection->unclamp_motion_vectors,
          .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
          .default_value = 0.f,
          .label = "Unclamp Motion Vectors",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Removes only MGSV's unit-length saturate from object and camera velocity encoding while retaining the original 64-pixel scale. This can improve motion above 64 pixels but also changes native motion blur.",
          .on_change_value = [](float previous, float current) {
            if ((previous > 0.f) == (current > 0.f)) return;
            InvalidateHistoryForSetting("motion-vector clamp changed"); },
          .is_visible = [] { return state::IsEnabled(); },
      },
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
      new renodx::utils::settings::Setting{
          .key = "FxTaaVelocityVisualizationRange",
          .binding = &state::velocity_visualization_range,
          .value_type = renodx::utils::settings::SettingValueType::FLOAT,
          .default_value = state::DEFAULT_VELOCITY_VISUALIZATION_RANGE,
          .label = "TAA Velocity View Range",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Pixel velocity represented by full intensity in the direction and magnitude diagnostic views.",
          .min = 0.25f,
          .max = 64.f,
          .format = "%.2f px",
          .on_change_value = [](float previous, float current) {
            (void)previous;
            state::SetVelocityVisualizationRange(current); },
          .is_visible = [] {
            const float view = state::GetDiagnosticView();
            return state::IsEnabled()
                   && ((view >= 3.f && view < 5.f) || view >= 7.f); },
          .is_logarithmic = true,
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaObjectMotionMode",
          .binding = &state::object_motion_mode,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = static_cast<float>(state::DEFAULT_OBJECT_MOTION_MODE),
          .label = "TAA Object Motion Source",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Tests whether native skinned motion contains projection jitter. Matrix Camera Everywhere intentionally removes animation motion. Add modes are sign checks. Changing modes resets history.",
          .labels = {
              "Auto-Corrected Native Object Velocity",
              "Matrix Camera Everywhere",
              "Native - Current Jitter",
              "Native + Current Jitter",
              "Native - Jitter Delta",
              "Native + Jitter Delta",
          },
          .on_change_value = [](float previous, float current) {
            (void)previous;
            state::SetObjectMotionMode(current);
            InvalidateHistoryForSetting("object motion mode changed"); },
          .is_visible = [] { return state::IsEnabled(); },
      },
#endif
      new renodx::utils::settings::Setting{
          .key = "FxTaaClipTightness",
          .binding = &state::clip_tightness,
          .value_type = renodx::utils::settings::SettingValueType::FLOAT,
          .default_value = state::DEFAULT_CLIP_TIGHTNESS,
          .label = "TAA Clip Tightness",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Blends broad 3x3 history bounds toward the tighter cross-shaped bounds. 0 uses broad bounds; 1 uses tight bounds. Lower values may preserve unstable thin detail but increase ghosting.",
          .min = 0.f,
          .max = 1.f,
          .format = "%.2f",
          .on_change_value = [](float previous, float current) {
            (void)previous;
            state::SetClipTightness(current);
            InvalidateHistoryForSetting("clip tightness changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaHistoryClipStrength",
          .binding = &state::history_clip_strength,
          .value_type = renodx::utils::settings::SettingValueType::FLOAT,
          .default_value = state::DEFAULT_HISTORY_CLIP_STRENGTH,
          .label = "TAA History Clip Strength",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Controls how strongly reprojected history is clipped to current-frame color bounds. 0 disables clipping; 1 is the current full clip. Lower values may stabilize fine patterns but can cause ghosting.",
          .min = 0.f,
          .max = 1.f,
          .format = "%.2f",
          .on_change_value = [](float previous, float current) {
            (void)previous;
            state::SetHistoryClipStrength(current);
            InvalidateHistoryForSetting("history clip strength changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaCurrentFrameBlend",
          .binding = &state::current_frame_blend,
          .value_type = renodx::utils::settings::SettingValueType::FLOAT,
          .default_value = state::DEFAULT_CURRENT_FRAME_BLEND,
          .label = "TAA Current Frame Blend",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Maximum adaptive contribution from filtered current color after history clipping. 0 retains clipped history completely; higher values respond faster but can expose jitter-phase flicker.",
          .min = 0.f,
          .max = 1.f,
          .format = "%.2f",
          .on_change_value = [](float previous, float current) {
            (void)previous;
            state::SetCurrentFrameBlend(current);
            InvalidateHistoryForSetting("current frame blend changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
      new renodx::utils::settings::Setting{
          .key = "FxTaaVelocityProjectionJitterScale",
          .binding = &state::projection_jitter_scales[static_cast<std::size_t>(state::ProjectionJitterPath::VELOCITY)],
          .value_type = renodx::utils::settings::SettingValueType::FLOAT,
          .default_value = state::DEFAULT_PROJECTION_JITTER_SCALE,
          .label = "Velocity Projection Jitter",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Scales same-frame jitter applied after MakeVelocityBuffer resets the active projection. Default native object motion automatically removes this current-jitter term.",
          .min = -2.f,
          .max = 2.f,
          .format = "%.2fx",
          .on_change_value = [](float previous, float current) {
            (void)previous;
            SetProjectionJitterScale(
              state::ProjectionJitterPath::VELOCITY,
              current,
              "velocity projection jitter changed"); },
          .is_visible = [] { return state::IsEnabled(); },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaForwardProjectionJitterScale",
          .binding = &state::projection_jitter_scales[static_cast<std::size_t>(state::ProjectionJitterPath::FORWARD)],
          .value_type = renodx::utils::settings::SettingValueType::FLOAT,
          .default_value = state::DEFAULT_PROJECTION_JITTER_SCALE,
          .label = "Forward Projection Jitter",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Scales same-frame jitter applied by the GrPluginForwardRendering setup callback.",
          .min = -2.f,
          .max = 2.f,
          .format = "%.2fx",
          .on_change_value = [](float previous, float current) {
            (void)previous;
            SetProjectionJitterScale(
              state::ProjectionJitterPath::FORWARD,
              current,
              "forward projection jitter changed"); },
          .is_visible = [] { return state::IsEnabled(); },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaModelProjectionJitterScale",
          .binding = &state::projection_jitter_scales[static_cast<std::size_t>(state::ProjectionJitterPath::MODEL)],
          .value_type = renodx::utils::settings::SettingValueType::FLOAT,
          .default_value = state::DEFAULT_PROJECTION_JITTER_SCALE,
          .label = "Model Projection Jitter",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Scales same-frame jitter applied after GrPluginModel installs viewport projection state.",
          .min = -2.f,
          .max = 2.f,
          .format = "%.2fx",
          .on_change_value = [](float previous, float current) {
            (void)previous;
            SetProjectionJitterScale(
              state::ProjectionJitterPath::MODEL,
              current,
              "model projection jitter changed"); },
          .is_visible = [] { return state::IsEnabled(); },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaAlphaProjectionJitterScale",
          .binding = &state::projection_jitter_scales[static_cast<std::size_t>(state::ProjectionJitterPath::ALPHA_MODEL)],
          .value_type = renodx::utils::settings::SettingValueType::FLOAT,
          .default_value = state::DEFAULT_PROJECTION_JITTER_SCALE,
          .label = "Alpha Model Projection Jitter",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Scales same-frame jitter applied after GrPluginAlphaModel installs viewport projection state.",
          .min = -2.f,
          .max = 2.f,
          .format = "%.2fx",
          .on_change_value = [](float previous, float current) {
            (void)previous;
            SetProjectionJitterScale(
              state::ProjectionJitterPath::ALPHA_MODEL,
              current,
              "alpha-model projection jitter changed"); },
          .is_visible = [] { return state::IsEnabled(); },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaOverlayProjectionJitterScale",
          .binding = &state::projection_jitter_scales[static_cast<std::size_t>(state::ProjectionJitterPath::OVERLAY_MODEL)],
          .value_type = renodx::utils::settings::SettingValueType::FLOAT,
          .default_value = state::DEFAULT_PROJECTION_JITTER_SCALE,
          .label = "Overlay Model Projection Jitter",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Scales same-frame jitter applied after GrPluginOverlayModel installs viewport projection state.",
          .min = -2.f,
          .max = 2.f,
          .format = "%.2fx",
          .on_change_value = [](float previous, float current) {
            (void)previous;
            SetProjectionJitterScale(
              state::ProjectionJitterPath::OVERLAY_MODEL,
              current,
              "overlay-model projection jitter changed"); },
          .is_visible = [] { return state::IsEnabled(); },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaLocalLightProjectionJitterScale",
          .binding = &state::projection_jitter_scales[static_cast<std::size_t>(state::ProjectionJitterPath::LOCAL_LIGHT)],
          .value_type = renodx::utils::settings::SettingValueType::FLOAT,
          .default_value = state::DEFAULT_PROJECTION_JITTER_SCALE,
          .label = "Local Light Projection Jitter",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Temporarily scales jitter on GrViewport projection while GrPluginLocalLight builds its private scene packet, then restores the exact original matrix.",
          .min = -2.f,
          .max = 2.f,
          .format = "%.2fx",
          .on_change_value = [](float previous, float current) {
            (void)previous;
            SetProjectionJitterScale(
              state::ProjectionJitterPath::LOCAL_LIGHT,
              current,
              "local-light projection jitter changed"); },
          .is_visible = [] { return state::IsEnabled(); },
      },
#endif
  };

  settings.insert(settings.begin(), taa_settings.begin(), taa_settings.end());
}

inline void OnPresetOff() {
  // Programmatic setting updates do not invoke on_change_value.
  renodx::utils::settings::UpdateSetting(
      "FxTemporalReconstructionMode",
      static_cast<float>(state::TemporalMode::OFF));
  TransitionTemporalMode(static_cast<float>(state::TemporalMode::OFF), "preset off", true);
}

}  // namespace taa::settings
