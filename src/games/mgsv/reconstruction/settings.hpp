#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <include/reshade.hpp>

#include "../../../utils/settings.hpp"
#include "../shared.h"
#include "dlss/runtime.hpp"
#include "logging.hpp"
#include "runtime.hpp"
#include "state.hpp"

// The Temporal Anti-Aliasing settings. The method, DLSS model and quality are
// stored in hidden settings and drawn with custom combo boxes, so unavailable
// choices can be explained and method and model changes go through the
// transitions in runtime.hpp.
namespace mgsv::reconstruction::settings {

inline void AppendSettings(
    renodx::utils::settings::Settings* registry,
    ShaderInjectData* shader_injection,
    State* settings_state) {
  if (registry == nullptr || shader_injection == nullptr || settings_state == nullptr) return;
  // Settings already created hold pointers into this state; never create them twice.
  if (settings_state->temporal_mode_setting != nullptr || settings_state->dlss_model_setting != nullptr) return;
  if (std::ranges::any_of(*registry, [](auto* setting) {
        return setting->key == "FxTemporalReconstructionMode";
      })) return;

  settings_state->temporal_mode_setting = new renodx::utils::settings::Setting{
      .key = "FxTemporalReconstructionMode",
      .binding = &shader_injection->reconstruction_mode,
      .value_type = renodx::utils::settings::SettingValueType::INTEGER,
      .default_value = static_cast<float>(state::DEFAULT_TEMPORAL_MODE),
      .label = "Temporal Reconstruction Mode",
      .section = "Temporal Anti-Aliasing",
      .max = static_cast<float>(transitions::TEMPORAL_MODE_OPTIONS.size()) - 1.f,
      // The shader binding holds the active method, not the requested one, so
      // shaders never see DLSS before it is ready. TransitionTemporalMode
      // writes the binding once the method has actually switched.
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
  settings_state->upscaling_quality_setting = new renodx::utils::settings::Setting{
      .key = "FxUpscalingQuality",
      .binding = &settings_state->upscaling_quality,
      .value_type = renodx::utils::settings::SettingValueType::INTEGER,
      .default_value = static_cast<float>(resolution::Quality::AUTO),
      .label = "Upscaling Quality",
      .section = "Temporal Anti-Aliasing",
      .labels = {resolution::QUALITY_LABELS.begin(), resolution::QUALITY_LABELS.end()},
      .max = static_cast<float>(resolution::QUALITY_LABELS.size() - 1u),
      .parse = [](float value) {
        const auto selected = static_cast<resolution::Quality>(static_cast<uint32_t>(std::clamp(value, 0.f, static_cast<float>(resolution::QUALITY_LABELS.size() - 1u))));
        // A different quality clears an earlier upscaling failure, so it retries.
        if (resolution::quality.exchange(selected, std::memory_order_acq_rel) != selected) {
          resolution::failed.store(false, std::memory_order_release);
          logging::Info("upscaling quality selected ", resolution::QUALITY_LABELS[static_cast<size_t>(selected)]);
        }
        return static_cast<float>(selected); },
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
              preview = "NVIDIA DLSS (initializing)";
            }
            const bool combo_open = ImGui::BeginCombo("Temporal Reconstruction", preview);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
              ImGui::SetTooltip(
                  "Selects vanilla FXAA, analytical TAA, FSR, or NVIDIA DLSS. FSR uses AMD's DLLs and selects the newest compatible implementation. "
                  "Changing modes resets temporal state. The first DLSS selection checks the DLL and driver "
                  "and installs its runtime hooks. The current method stays active until DLSS is ready. "
                  "NVIDIA vendor eligibility alone does not guarantee DLSS support.");
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
      settings_state->upscaling_quality_setting,
      new renodx::utils::settings::Setting{
          .value_type = renodx::utils::settings::SettingValueType::CUSTOM,
          .can_reset = false,
          .label = "Upscaling Quality",
          .section = "Temporal Anti-Aliasing",
          .on_draw = [settings_state] {
            if (settings_state->upscaling_quality_setting == nullptr) return false;
            const auto current = static_cast<size_t>(std::clamp(settings_state->upscaling_quality_setting->GetValue(), 0.f, static_cast<float>(resolution::QUALITY_LABELS.size() - 1u)));
            const bool open = ImGui::BeginCombo("Upscaling Quality", resolution::QUALITY_LABELS[current]);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
              ImGui::SetTooltip("Renders the scene at a lower resolution, then reconstructs it before post-processing and UI. Auto chooses by output pixel count: Native below 1080p, Quality through 1440p, Performance through 4K, and Ultra Performance above 4K. Native keeps the game's resolution. Changing quality rebuilds size-dependent game resources and restarts history.");
            }
            if (!open) return false;
            bool changed = false;
            for (size_t index = 0u; index < resolution::QUALITY_LABELS.size(); ++index) {
              const bool selected = index == current;
              if (ImGui::Selectable(resolution::QUALITY_LABELS[index], selected) && !selected) {
                changed = UpdateSetting(settings_state->upscaling_quality_setting, static_cast<float>(index));
              }
              if (selected) {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
            return changed; },
          .is_visible = [] {
            const auto mode = state::GetTemporalMode();
            return resolution::installed.load(std::memory_order_acquire)
                   && (mode == state::TemporalMode::AMD_FSR || mode == state::TemporalMode::NVIDIA_DLSS); },
      },
      new renodx::utils::settings::Setting{
          .value_type = renodx::utils::settings::SettingValueType::CUSTOM,
          .can_reset = false,
          .label = "Upscaling Status",
          .section = "Temporal Anti-Aliasing",
          .on_draw = [] {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.2f, 0.2f, 1.f));
            ImGui::TextWrapped("Upscaling disabled. Select Native, then choose a quality mode to retry.");
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) { ImGui::SetTooltip("The ReShade log contains the reason the upscale path could not run."); }
            return false; },
          .is_visible = [] {
            const auto mode = state::GetTemporalMode();
            return resolution::failed.load(std::memory_order_acquire)
                   && (mode == state::TemporalMode::AMD_FSR || mode == state::TemporalMode::NVIDIA_DLSS); },
      },
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
      new renodx::utils::settings::Setting{
          .key = "FxTaaDiagnosticView",
          .binding = &settings_state->diagnostic_view,
          .value_type = renodx::utils::settings::SettingValueType::INTEGER,
          .default_value = state::DEFAULT_DIAGNOSTIC_VIEW,
          .label = "TAA Diagnostic View",
          .section = "Temporal Anti-Aliasing (Advanced)",
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

#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
      new renodx::utils::settings::Setting{
          .key = "FxTaaVelocityVisualizationRange",
          .binding = &settings_state->velocity_visualization_range,
          .value_type = renodx::utils::settings::SettingValueType::FLOAT,
          .default_value = state::DEFAULT_VELOCITY_VISUALIZATION_RANGE,
          .label = "TAA Velocity View Range",
          .section = "Temporal Anti-Aliasing (Advanced)",
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

#endif
      new renodx::utils::settings::Setting{
          .key = "FxTaaClipTightness",
          .binding = &settings_state->clip_tightness,
          .value_type = renodx::utils::settings::SettingValueType::FLOAT,
          .default_value = state::DEFAULT_CLIP_TIGHTNESS,
          .label = "TAA Clip Tightness",
          .section = "Temporal Anti-Aliasing (Advanced)",
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
          .section = "Temporal Anti-Aliasing (Advanced)",
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
          .section = "Temporal Anti-Aliasing (Advanced)",
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
  };

  reconstruction_settings.insert(
      reconstruction_settings.end(),
      {
          new renodx::utils::settings::Setting{
              .value_type = renodx::utils::settings::SettingValueType::CUSTOM,
              .can_reset = false,
              .label = "Presets",
              .section = "Presets",
              .on_draw = [] { return false; },
          },
      });

  for (auto* setting : reconstruction_settings) {
    setting->is_global = true;
    setting->is_sticky = true;
  }
  registry->insert(registry->begin(), reconstruction_settings.begin(), reconstruction_settings.end());
}

}  // namespace mgsv::reconstruction::settings
