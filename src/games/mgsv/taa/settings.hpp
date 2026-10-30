#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <include/reshade.hpp>

#include "../../../utils/settings.hpp"
#include "../shared.h"
#include "./dlss/runtime.hpp"
#include "./runtime.hpp"
#include "./runtime/state.hpp"

namespace mgsv::reconstruction::settings {

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
      temporal_mode_value = (method_value == legacy_analytical_method
                                 ? static_cast<int>(state::TemporalMode::ANALYTICAL_TAA)
                                 : static_cast<int>(state::TemporalMode::AMD_FSR3));
    }
    reshade::set_config_value(
        nullptr,
        section,
        "FxTemporalReconstructionMode",
        temporal_mode_value);
  }
}

inline void AppendSettings(
    renodx::utils::settings::Settings* registry,
    ShaderInjectData* shader_injection,
    State* settings_state) {
  if (registry == nullptr || shader_injection == nullptr || settings_state == nullptr) return;
  // Existing descriptors capture this state and its bindings. Never rebind it.
  if (settings_state->temporal_mode_setting != nullptr || settings_state->dlss_model_setting != nullptr) return;
  if (std::ranges::any_of(*registry, [](auto* setting) {
        return setting->key == "FxTemporalReconstructionMode";
      })) return;

  settings_state->temporal_mode_setting = new renodx::utils::settings::Setting{
      .key = "FxTemporalReconstructionMode",
      .binding = &shader_injection->custom_taa,
      .value_type = renodx::utils::settings::SettingValueType::INTEGER,
      .default_value = static_cast<float>(state::DEFAULT_TEMPORAL_MODE),
      .label = "Temporal Reconstruction Mode",
      .section = "Temporal Anti-Aliasing",
      .max = static_cast<float>(transitions::TEMPORAL_MODE_OPTIONS.size()) - 1.f,
      // Setting::Write must not expose requested DLSS to shaders before the
      // coordinator has activated it. TransitionTemporalMode publishes the
      // effective binding after readiness and history are synchronized.
      .parse = [](float) { return static_cast<float>(state::GetTemporalMode()); },
      .is_visible = [] { return false; },
  };
  settings_state->dlss_model_setting = new renodx::utils::settings::Setting{
      .key = "FxDlssModel",
      .binding = &settings_state->dlss_model,
      .value_type = renodx::utils::settings::SettingValueType::INTEGER,
      .default_value = static_cast<float>(dlss::DEFAULT_MODEL),
      .label = "DLSS Model",
      .section = "Temporal Anti-Aliasing",
      .max = static_cast<float>(dlss::Model::M),
      .is_visible = [] { return false; },
  };

  renodx::utils::settings::Settings reconstruction_settings = {
      settings_state->temporal_mode_setting,
      new renodx::utils::settings::Setting{
          .value_type = renodx::utils::settings::SettingValueType::CUSTOM,
          .can_reset = false,
          .label = "Temporal Reconstruction",
          .section = "Temporal Anti-Aliasing",
          .on_draw = [settings_state] {
            if (settings_state->temporal_mode_setting == nullptr) return false;

            const auto current_mode = state::NormalizeTemporalMode(settings_state->temporal_mode_setting->GetValue());
            const auto* current_option = transitions::FindTemporalModeOption(current_mode);
            const char* preview = (current_option == nullptr ? "Unknown" : current_option->label);
            if (current_mode == state::TemporalMode::NVIDIA_DLSS && dlss::ActivationPending()) {
              preview = "NVIDIA DLSS (pending; rendering Off)";
            }
            const bool combo_open = ImGui::BeginCombo("Temporal Reconstruction", preview);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
              ImGui::SetTooltip(
                  "Selects vanilla FXAA, analytical TAA, AMD FSR 3.1.5, or NVIDIA DLSS. "
                  "Changing modes resets temporal state. The first DLSS selection checks the DLL and driver "
                  "and installs its runtime hooks; NVIDIA vendor eligibility alone does not guarantee DLSS support.");
            }
            if (!combo_open) return false;

            bool changed = false;
            for (const auto& option : transitions::TEMPORAL_MODE_OPTIONS) {
              const char* unavailable_reason = (option.unavailable_reason == nullptr
                                                    ? nullptr
                                                    : option.unavailable_reason());
              const bool available = unavailable_reason == nullptr;
              const bool selected = option.mode == current_mode;
              if (!available) {
                ImGui::BeginDisabled();
              }
              const bool chosen = ImGui::Selectable(option.label, selected);
              if (!available) {
                ImGui::EndDisabled();
              }

              if (!available
                  && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.f);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.2f, 0.2f, 1.f));
                ImGui::TextUnformatted(unavailable_reason);
                ImGui::PopStyleColor();
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
              }
              if (chosen && available && !selected) {
                changed = UpdateSetting(
                    settings_state->temporal_mode_setting,
                    static_cast<float>(option.mode));
                if (changed) {
                  transitions::TransitionTemporalMode(*settings_state, static_cast<float>(option.mode), "temporal mode changed");
                }
              }
              if (selected) {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
            return changed;
          },
      },
      settings_state->dlss_model_setting,
      new renodx::utils::settings::Setting{
          .value_type = renodx::utils::settings::SettingValueType::CUSTOM,
          .can_reset = false,
          .label = "DLSS Model",
          .section = "Temporal Anti-Aliasing",
          .on_draw = [settings_state] {
            if (settings_state->dlss_model_setting == nullptr) return false;

            const dlss::Model current_model = dlss::NormalizeModel(settings_state->dlss_model_setting->GetValue());
            const auto* current_option = dlss::FindModelOption(current_model);
            const char* preview = (current_option == nullptr ? "Unknown" : current_option->label);
            if (current_model == dlss::Model::DEFAULT) {
              preview = "DLL Default (selected model unavailable)";
            }
            const bool combo_open = ImGui::BeginCombo("DLSS Model", preview);
            if (!combo_open) return false;

            bool changed = false;
            for (const auto& option : dlss::MODEL_OPTIONS) {
              if (option.model == dlss::Model::DEFAULT) continue;
              if (!dlss::IsModelAvailable(option.model)) continue;
              const bool selected = option.model == current_model;
              if (ImGui::Selectable(option.label, selected) && !selected) {
                changed = UpdateSetting(
                    settings_state->dlss_model_setting,
                    static_cast<float>(option.model));
                if (changed) {
                  transitions::TransitionDlssModel(static_cast<float>(option.model), "DLSS model changed");
                }
              }
              if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
                ImGui::SetTooltip("%s", option.description);
              }
              if (selected) {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
            return changed; },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::NVIDIA_DLSS; },
      },
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
      new renodx::utils::settings::Setting{
          .key = "FxTaaDiagnosticView",
          .binding = &settings_state->diagnostic_view,
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
            transitions::InvalidateHistoryForSetting("diagnostic view changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
#endif
      new renodx::utils::settings::Setting{
          .key = "FxTaaJitterPattern",
          .binding = &settings_state->jitter_pattern,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = static_cast<float>(state::DEFAULT_JITTER_PATTERN),
          .label = "TAA Jitter Pattern",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Diagnostic projection sampling pattern. Off keeps analytical TAA active with zero projection jitter. "
                     "FSR3 and DLSS enforce the eight-phase Halton sequence. Changing modes resets temporal history "
                     "and the sample sequence.",
          .labels = {"Off", "Halton (2,3) - 8 Phase"},
          .on_change_value = [](float previous, float current) {
            if (static_cast<uint32_t>(previous) == static_cast<uint32_t>(current)) return;
            transitions::TransitionJitterPattern(current); },
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
            transitions::InvalidateHistoryForSetting("motion-vector clamp changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
      new renodx::utils::settings::Setting{
          .key = "FxTaaVelocityVisualizationRange",
          .binding = &settings_state->velocity_visualization_range,
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
            return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA
                   && ((view >= 3.f && view < 5.f) || view >= 7.f); },
          .is_logarithmic = true,
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaObjectMotionMode",
          .binding = &settings_state->object_motion_mode,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = static_cast<float>(state::DEFAULT_OBJECT_MOTION_MODE),
          .label = "TAA Object Motion Source",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Auto Center Pixel changes only Auto's spatial selection: center velocity, mask and camera depth instead of nearest-depth dilation. Compare Auto vs Auto Center with Jitter Pattern Off. Direct Object instead bypasses the composite at the same dilated pixel. Matrix Camera Everywhere removes animation motion. Changing modes resets history.",
          .labels = {
              "Auto-Corrected Native Object Velocity",
              "Matrix Camera Everywhere",
              "Native - Current Jitter",
              "Native + Current Jitter",
              "Native - Jitter Delta",
              "Native + Jitter Delta",
              "Direct Object Velocity (Bypass Composite)",
              "Auto Center Pixel (No Dilation)",
          },
          .on_change_value = [](float previous, float current) {
            (void)previous;
            state::SetObjectMotionMode(current);
            transitions::InvalidateHistoryForSetting("object motion mode changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
#endif
      new renodx::utils::settings::Setting{
          .key = "FxTaaClipTightness",
          .binding = &settings_state->clip_tightness,
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
            transitions::InvalidateHistoryForSetting("clip tightness changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaHistoryClipStrength",
          .binding = &settings_state->history_clip_strength,
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
            transitions::InvalidateHistoryForSetting("history clip strength changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaCurrentFrameBlend",
          .binding = &settings_state->current_frame_blend,
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
            transitions::InvalidateHistoryForSetting("current frame blend changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
#if ENABLE_TAA_PROJECTION_JITTER_DIAGNOSTICS
      new renodx::utils::settings::Setting{
          .key = "FxTaaVelocityProjectionJitterScale",
          .binding = &settings_state->projection_jitter_scales[static_cast<std::size_t>(state::ProjectionJitterPath::VELOCITY)],
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
            transitions::SetProjectionJitterScale(
              state::ProjectionJitterPath::VELOCITY,
              current,
              "velocity projection jitter changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaForwardProjectionJitterScale",
          .binding = &settings_state->projection_jitter_scales[static_cast<std::size_t>(state::ProjectionJitterPath::FORWARD)],
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
            transitions::SetProjectionJitterScale(
              state::ProjectionJitterPath::FORWARD,
              current,
              "forward projection jitter changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaModelProjectionJitterScale",
          .binding = &settings_state->projection_jitter_scales[static_cast<std::size_t>(state::ProjectionJitterPath::MODEL)],
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
            transitions::SetProjectionJitterScale(
              state::ProjectionJitterPath::MODEL,
              current,
              "model projection jitter changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaAlphaProjectionJitterScale",
          .binding = &settings_state->projection_jitter_scales[static_cast<std::size_t>(state::ProjectionJitterPath::ALPHA_MODEL)],
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
            transitions::SetProjectionJitterScale(
              state::ProjectionJitterPath::ALPHA_MODEL,
              current,
              "alpha-model projection jitter changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaOverlayProjectionJitterScale",
          .binding = &settings_state->projection_jitter_scales[static_cast<std::size_t>(state::ProjectionJitterPath::OVERLAY_MODEL)],
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
            transitions::SetProjectionJitterScale(
              state::ProjectionJitterPath::OVERLAY_MODEL,
              current,
              "overlay-model projection jitter changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaLocalLightProjectionJitterScale",
          .binding = &settings_state->projection_jitter_scales[static_cast<std::size_t>(state::ProjectionJitterPath::LOCAL_LIGHT)],
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
            transitions::SetProjectionJitterScale(
              state::ProjectionJitterPath::LOCAL_LIGHT,
              current,
              "local-light projection jitter changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaSunVolumeJitterScale",
          .binding = &settings_state->projection_jitter_scales[static_cast<std::size_t>(state::ProjectionJitterPath::SUN_VOLUME)],
          .value_type = renodx::utils::settings::SettingValueType::FLOAT,
          .default_value = state::DEFAULT_PROJECTION_JITTER_SCALE,
          .label = "Sun Volume Projection Jitter",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Experimental: translates a private VP copy for CPU-projected sunlight volumes. Default 1; test independently.",
          .min = -2.f,
          .max = 2.f,
          .format = "%.2fx",
          .on_change_value = [](float, float current) { transitions::SetProjectionJitterScale(state::ProjectionJitterPath::SUN_VOLUME, current, "sun volume jitter changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaShFallbackJitterScale",
          .binding = &settings_state->projection_jitter_scales[static_cast<std::size_t>(state::ProjectionJitterPath::SH_FALLBACK)],
          .value_type = renodx::utils::settings::SettingValueType::FLOAT,
          .default_value = state::DEFAULT_PROJECTION_JITTER_SCALE,
          .label = "SH Fallback Projection Jitter",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Experimental: translates successful SH fallback clip vertices, preserving visibility, Z/W and depth bounds. Not the deferred SH packet route.",
          .min = -2.f,
          .max = 2.f,
          .format = "%.2fx",
          .on_change_value = [](float, float current) { transitions::SetProjectionJitterScale(state::ProjectionJitterPath::SH_FALLBACK, current, "SH fallback jitter changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaTerrainDecalJitterScale",
          .binding = &settings_state->projection_jitter_scales[static_cast<std::size_t>(state::ProjectionJitterPath::TERRAIN_DECAL)],
          .value_type = renodx::utils::settings::SettingValueType::FLOAT,
          .default_value = state::DEFAULT_PROJECTION_JITTER_SCALE,
          .label = "Terrain Decal Projection Jitter",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Experimental: corrects the owned decal packet's copied P before publication. Affects inverse-plane reconstruction, not the separately captured raster P. Default 1.",
          .min = -2.f,
          .max = 2.f,
          .format = "%.2fx",
          .on_change_value = [](float, float current) { transitions::SetProjectionJitterScale(state::ProjectionJitterPath::TERRAIN_DECAL, current, "terrain decal jitter changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
      new renodx::utils::settings::Setting{
          .key = "FxTaaLightPacketJitterScale",
          .binding = &settings_state->projection_jitter_scales[static_cast<std::size_t>(state::ProjectionJitterPath::LIGHT_PACKET)],
          .value_type = renodx::utils::settings::SettingValueType::FLOAT,
          .default_value = state::DEFAULT_PROJECTION_JITTER_SCALE,
          .label = "General Light Packet Jitter",
          .section = "Temporal Anti-Aliasing",
          .tooltip = "Experimental: translates owned light-packet VP before publication, leaving clip-W and selection data unchanged. Final consumer coverage is not yet visually verified.",
          .min = -2.f,
          .max = 2.f,
          .format = "%.2fx",
          .on_change_value = [](float, float current) { transitions::SetProjectionJitterScale(state::ProjectionJitterPath::LIGHT_PACKET, current, "light packet jitter changed"); },
          .is_visible = [] { return state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA; },
      },
#endif
      new renodx::utils::settings::Setting{
          .value_type = renodx::utils::settings::SettingValueType::CUSTOM,
          .can_reset = false,
          .label = "Presets",
          .section = "Presets",
          .on_draw = [] { return false; },
      },
  };

  for (auto* setting : reconstruction_settings) {
    setting->is_global = true;
    setting->is_sticky = true;
  }
  registry->insert(registry->begin(), reconstruction_settings.begin(), reconstruction_settings.end());
}

}  // namespace mgsv::reconstruction::settings
