#ifndef RENODX_GAMES_MGSV_RECONSTRUCTION_RUNTIME_HPP_
#define RENODX_GAMES_MGSV_RECONSTRUCTION_RUNTIME_HPP_

/* ReShade event handlers and method changes for temporal reconstruction.
 * Present applies settings, plans the render size and checks the frame that
 * just ended. The post-processing cut schedules each scene. Method and setting
 * changes reset history. OnCameraVelocityRecorded only serves the lighting
 * history. */

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <mutex>

#include <intrin.h>

#include <include/reshade.hpp>

#include "../../../utils/mutex.hpp"
#include "../../../utils/settings.hpp"
#include "../../../utils/swapchain.hpp"
#include "../engine/render_targets.hpp"
#include "../engine/views.hpp"
#include "../rendering/inputs.hpp"
#include "../rendering/producer_records.hpp"
#include "../rendering/submission.hpp"
#include "../shared.h"
#include "camera_state.hpp"
#include "dispatch.hpp"
#include "dlss/model.hpp"
#include "dlss/runtime.hpp"
#include "logging.hpp"
#include "mip_bias.hpp"
#include "projection_jitter.hpp"
#include "resolution.hpp"
#include "state.hpp"

namespace mgsv::reconstruction::settings {

// Owned by the addon at a fixed address, because the settings registry keeps
// pointers to these fields as bindings. The Setting pointers belong to that
// registry and stay valid while it exists; they are set once, never replaced.
struct State {
  renodx::utils::settings::Setting* temporal_mode_setting = nullptr;
  renodx::utils::settings::Setting* dlss_model_setting = nullptr;
  renodx::utils::settings::Setting* upscaling_quality_setting = nullptr;
  // Setting::binding points to a float, so choices are stored as floats and
  // converted to enums where they are read.
  float dlss_model = static_cast<float>(dlss::DEFAULT_MODEL);

#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
  float diagnostic_view = state::DEFAULT_DIAGNOSTIC_VIEW;
  float velocity_visualization_range = state::DEFAULT_VELOCITY_VISUALIZATION_RANGE;

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
        .mode = state::TemporalMode::AMD_FSR,
        .label = "FSR",
        .unavailable_reason = fsr::UnavailableReason,
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
// Lock order: settings mutex, this lock, the execution lock, then the camera
// publication lock.
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

// Makes `value` the active method and resets history. force_reset resets even
// when the method is unchanged and skips the stale-request check; failures and
// teardown use it. verify_restoration confirms that turning jitter off gave
// the game back its unmodified projection.
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
  const RuntimeTransitionGuard transition_guard;
  // First-time DLSS initialization runs at Present. Keep the active method,
  // its history, and its shader binding until that initialization finishes.
  // Going through Off would make the native backend resize to the display.
  dlss::activation_requested.store(requested_dlss && (activation_pending || unavailable_reason == nullptr), std::memory_order_release);
  if (activation_pending) return;

  if (unavailable_reason != nullptr) {
    logging::Warn("temporal mode unavailable mode=", static_cast<uint32_t>(mode), " reason=", unavailable_reason);
    mode = state::TemporalMode::OFF;
    option = FindTemporalModeOption(mode);
  }

  if (unavailable_reason != nullptr && settings_state.temporal_mode_setting != nullptr) {
    settings_state.temporal_mode_setting->Set(static_cast<float>(state::TemporalMode::OFF))->Write();
  }

  const bool enabled = mode != state::TemporalMode::OFF;
  const auto previous_mode = state::GetTemporalMode();
  const bool was_enabled = previous_mode != state::TemporalMode::OFF;
  if (previous_mode == mode
      && !force_reset) {
    return;
  }
  const uint64_t transition_frame = state::CurrentFrameToken();
  const uint32_t reserved_samples = state::CurrentSampleIndex();

  // Turn jitter off before waiting for the execution lock, which a running
  // reconstruction may hold. Turn a method on only after history and samples
  // are reset, so the projection hook cannot publish a sample from the old
  // sequence.
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

  const rendering::submission::ExecutionGuard execution_guard;
  // Present calls this again once the DLSS runtime is ready.
  if (mode == state::TemporalMode::NVIDIA_DLSS && !dlss::runtime_ready.load(std::memory_order_acquire)) return;
  const camera_state::PublicationWriterGuard publication_guard;
  dispatch::ResetTemporalStateWithPublicationLocked(reason);
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
                  " jitter_pattern=halton_2_3");
  } else {
    logging::Info("temporal reconstruction disabled reason=", reason,
                  " frame=", transition_frame,
                  " reserved_samples=", reserved_samples);
  }
}

inline void TransitionDlssModel(float value, const char* reason) {
  const dlss::Model model = dlss::NormalizeModel(value);
  const RuntimeTransitionGuard transition_guard;
  if (dlss::GetModel() == model) return;
  dlss::SetModel(model);
  if (state::GetTemporalMode() == state::TemporalMode::NVIDIA_DLSS) {
    const rendering::submission::ExecutionGuard execution_guard;
    const camera_state::PublicationWriterGuard publication_guard;
    dispatch::ResetTemporalStateWithPublicationLocked(reason);
  }
  const auto* option = dlss::FindModelOption(model);
  logging::Info("NVIDIA DLSS model changed model=", option == nullptr ? "unknown" : option->label);
}

inline void InvalidateHistoryForSetting(const char* reason) {
  const rendering::submission::ExecutionGuard execution_guard;
  dispatch::ResetTemporalState(reason);
}

// Copies the saved settings under the settings mutex, then applies them. Runs
// at attach and at every Present, so preset loads also reach the runtime.
inline void ApplySettingsSnapshot(const settings::State& settings_state) {
  struct BindingSnapshot {
    float temporal_mode = 0.f;
    float dlss_model = 0.f;

    float clip_tightness = 0.f;
    float history_clip_strength = 0.f;
    float current_frame_blend = 0.f;
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
    float diagnostic_view = 0.f;
    float velocity_visualization_range = 0.f;

#endif
  };

  BindingSnapshot snapshot = {};
  {
    const std::unique_lock settings_lock(renodx::utils::mutex::global_mutex);
    snapshot.temporal_mode = (settings_state.temporal_mode_setting == nullptr
                                  ? static_cast<float>(state::TemporalMode::OFF)
                                  : settings_state.temporal_mode_setting->GetValue());
    snapshot.dlss_model = settings_state.dlss_model;

    snapshot.clip_tightness = settings_state.clip_tightness;
    snapshot.history_clip_strength = settings_state.history_clip_strength;
    snapshot.current_frame_blend = settings_state.current_frame_blend;
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
    snapshot.diagnostic_view = settings_state.diagnostic_view;
    snapshot.velocity_visualization_range = settings_state.velocity_visualization_range;

#endif
  }

  TransitionDlssModel(snapshot.dlss_model, "DLSS model synchronized");
  TransitionTemporalMode(settings_state, snapshot.temporal_mode, "temporal mode synchronized");

  // Take the locks in the order documented on RuntimeTransitionGuard.
  const std::unique_lock settings_lock(renodx::utils::mutex::global_mutex);
  const RuntimeTransitionGuard transition_guard;
  const rendering::submission::ExecutionGuard execution_guard;
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
                                        std::clamp(snapshot.diagnostic_view, 0.f, 10.f)));
#endif
  state::SetClipTightness(snapshot.clip_tightness);
  state::SetHistoryClipStrength(snapshot.history_clip_strength);
  state::SetCurrentFrameBlend(snapshot.current_frame_blend);
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
  state::SetDiagnosticView(snapshot.diagnostic_view);
  state::SetVelocityVisualizationRange(snapshot.velocity_visualization_range);

#endif
  if (history_settings_changed) {
    dispatch::ResetTemporalStateWithPublicationLocked("settings synchronized");
  }
}

}  // namespace mgsv::reconstruction::transitions

namespace mgsv::reconstruction {

inline bool attached = false;
// ReShade callbacks cannot capture context. This borrows the addon-owned state
// supplied once at attach; its address remains valid for the module's lifetime.
inline const settings::State* active_settings = nullptr;

inline void OnDestroyDevice(reshade::api::device* device) noexcept {
  try {
    if (device->get_api() != reshade::api::device_api::d3d11) return;
    {
      const rendering::submission::ExecutionGuard execution_guard;
      if (dispatch::IsForeignDevice(device)) return;
      dispatch::device_teardown_pending = true;
    }
    // Projection hooks remain installed across device recreation. Stop new
    // jitter and reconstruction work before releasing their resources.
    projection_jitter::StopAdmission();
    transitions::TransitionTemporalMode(
        *active_settings,
        static_cast<float>(state::TemporalMode::OFF),
        "device destroyed",
        true,
        false);
    rendering::submission::Uninstall();
    logging::Info("destroy device");
    {
      const std::unique_lock lock(resolution::sizing_state.mutex);
      resolution::sizing_state.backend = 0u;
      resolution::sizing_state.presentation_generation = 0u;
      resolution::sizing_state.requested = {};
      resolution::sizing_state.actual = {};
      resolution::sizing_state.engine_wanted = {};
      resolution::sizing_state.last_written = {};
      ++resolution::sizing_state.generation;
    }
    const rendering::submission::ExecutionGuard execution_guard;
    dispatch::Destroy(device);
  } catch (const std::exception& error) {
    // A failed GPU wait or SDK release stops cleanup with the remaining state
    // intact. Do not free potentially live resources or throw into ReShade.
    // Log directly so handling an allocation failure does not allocate again.
    reshade::log::message(reshade::log::level::error, "MgsvTaa: device teardown failed; remaining resources retained and reconstruction blocked. Restart the game.");
    reshade::log::message(reshade::log::level::error, error.what());
  } catch (...) {
    reshade::log::message(reshade::log::level::error, "MgsvTaa: unexpected device teardown failure; remaining resources retained and reconstruction blocked. Restart the game.");
  }
}

// Runs between frames: applies settings and reported failures, finishes DLSS
// activation, releases inactive methods, plans the next render size, and
// checks whether the frame that just ended missed a reconstruction.
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
    const rendering::submission::ExecutionGuard execution_guard;
    // Concurrent first Presents must not bind reconstruction to a different input device.
    if (!inputs::IsSelectedDevice(device)) return;
    if (!dispatch::BindRuntimeDevice(device)) return;
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
  if (dispatch::submission_failure_pending.exchange(false, std::memory_order_acq_rel)
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
    const rendering::submission::ExecutionGuard execution_guard;
    const bool bridge_required = state::IsEnabled()
                                 || lighting::temporal_inputs::enabled.load(std::memory_order_acquire)
                                 || rendering::submission::install_requested.load(std::memory_order_acquire)
                                 || (dlss::activation_requested.load(std::memory_order_acquire)
                                     && dlss::ActivationPending());
    const bool bridge_ready = !bridge_required || rendering::submission::Install(device);
    if (!bridge_ready) {
      logging::Warn("failed to install temporal submission bridge");
      dispatch::submission_failure_pending.store(true, std::memory_order_release);
    }
    if (dlss::activation_requested.load(std::memory_order_acquire) && dlss::ActivationPending()) {
      activation_serviced = true;
      logging::Info("servicing requested NVIDIA DLSS activation");
      if (dlss::ProbeDevice(device)) {
        // DLSS loads only when selected. It also needs the submission bridge,
        // which every method shares.
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
  const rendering::submission::ExecutionGuard execution_guard;
  const auto mode = state::GetTemporalMode();
  // Initialization can finish after the user cancels or selects another method.
  // Release resources based on the active method, never a pending UI request.
  // The new method allocates its GPU history at its first dispatch.
  try {
    dispatch::ReleaseInactiveResources(device, mode);
  } catch (const std::exception& error) {
    static uint64_t last_release_failure_log = std::numeric_limits<uint64_t>::max();
    if (logging::ShouldLogFrame(state::CurrentFrameToken(), last_release_failure_log)) {
      logging::Warn("reconstruction method teardown failed: ", error.what());
    }
    dispatch::submission_failure_pending.store(true, std::memory_order_release);
    return;
  }
  // The output is the render size the game requests. Until the Start hook has
  // selected the backend, the back buffer's size stands in.
  resolution::Size output;
  resolution::Plan previous_plan;
  {
    const std::shared_lock lock(resolution::sizing_state.mutex);
    output = resolution::sizing_state.engine_wanted;
    previous_plan = resolution::sizing_state.requested;
  }
  if (output.width == 0u || output.height == 0u) {
    const auto back_buffer = renodx::utils::swapchain::GetBackBufferDesc(device);
    output = {.width = back_buffer.texture.width, .height = back_buffer.texture.height};
  }
  auto plan = resolution::MakePlan(output, mode, resolution::quality.load(std::memory_order_acquire));
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
      const bool queried = NVSDK_NGX_SUCCEED(NVSDK_NGX_D3D12_GetCapabilityParameters(&capabilities))
                           && capabilities != nullptr
                           && NVSDK_NGX_SUCCEED(NGX_DLSS_GET_OPTIMAL_SETTINGS(
                               capabilities, plan.output.width, plan.output.height, dlss::GetPerformanceQuality(plan.quality),
                               &plan.render.width, &plan.render.height, &maximum_width, &maximum_height,
                               &minimum_width, &minimum_height, &sharpness));
      if (capabilities != nullptr) {
        NVSDK_NGX_D3D12_DestroyParameters(capabilities);
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
    dispatch::ResetTemporalState("render/output size changed");
    if (previous_plan.IsScaled() && !plan.IsScaled()) {
      output_size::Reset();
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
  // Eight upscaled frames in a row that had a complete scene record but were
  // not reconstructed end upscaling. A frame with an incomplete record never
  // counts, so startup screens and size changes cannot trigger it.
  static uint32_t missed_reconstructions = 0u;
  const bool missed = mode != state::TemporalMode::OFF && plan.IsScaled() && state::frame_state.complete_record_seen
                      && !state::frame_state.reconstruction_scheduled && !state::frame_state.reconstruction_completed;
  missed_reconstructions = (missed ? missed_reconstructions + 1u : 0u);
  if (missed_reconstructions >= 8u) {
    resolution::Fail("reconstruction repeatedly could not run on a complete scene record");
  }
  // History continuity is checked at submission, not here: a scene recorded
  // before this Present keeps its jitter and inputs.
  state::BeginFrame();
}

// Called by engine::motion_vectors on the thread building a view, after the
// engine has recorded the view's camera-velocity pass and before it ends that
// pass, so the pass's target and inputs are still bound. Records the motion
// and camera the lighting history uses; reconstruction takes its own inputs
// where post-processing starts.
inline void OnCameraVelocityRecorded() {
  if (!lighting::temporal_inputs::enabled.load(std::memory_order_acquire)) return;
  try {
    const auto context = engine::views::Unwrap(engine::views::GetRecording().d3d_context);
    // The engine records the pass on a deferred context.
    if (context == nullptr || context->GetType() != D3D11_DEVICE_CONTEXT_DEFERRED) return;
    auto* cmd_list = rendering::submission::FindRecordingList(context.Get());
    if (cmd_list == nullptr) return;
    const rendering::submission::ExecutionGuard execution_guard;
    const auto images = engine::render_targets::ReadFrameImages();
    auto motion = inputs::CaptureMotion(cmd_list, images[static_cast<size_t>(engine::render_targets::FrameTarget::VELOCITY)].texture.Get());
    // The lighting history tracks this capture through its own submissions.
    // Without reconstruction the scene has no jitter, so no camera is attached
    // and the jitter counts as known. With it, a failed capture carries no
    // camera and no valid jitter.
    const bool reconstructing = state::IsEnabled();
    lighting::temporal_inputs::Producer producer = {
        .device = cmd_list->get_device(),
        .generation = motion.generation,
        .motion = std::move(motion),
        .velocity_projection_jitter_scale = (reconstructing ? 1.f : 0.f),
        .jitter_valid = !reconstructing,
        .kind = lighting::temporal_inputs::ProducerKind::MOTION,
    };
    if (reconstructing && producer.motion.velocity != nullptr) {
      const auto camera = camera_state::GetForCapture();
      producer.motion_jitter_uv = {camera.jitter_uv_x, camera.jitter_uv_y};
      producer.camera_publication = camera.publication_sequence;
      producer.camera_generation = camera.reset_generation;
      producer.camera_sample = camera.sample_index;
      producer.jitter_valid = camera.valid && camera.camera_matrix_valid;
    }
    lighting::temporal_inputs::Record(cmd_list, std::move(producer));
  } catch (const std::exception& error) {
    // The engine continues its pass; the lighting history gets no motion for this view.
    static std::atomic_flag reported;
    if (!reported.test_and_set(std::memory_order_relaxed)) {
      logging::Warn("recording the camera-velocity pass failed: ", error.what());
    }
  }
}

// Called by engine::post_filter at each cut: schedules the presented main
// view's reconstruction in the list the engine has just begun for its
// post-processing.
inline void OnPostFilterStart(ID3D11DeviceContext* context) {
  if (!state::IsEnabled()) return;
  try {
    auto* cmd_list = rendering::submission::FindRecordingList(context);
    if (cmd_list == nullptr) return;
    const rendering::submission::ExecutionGuard execution_guard;
    dispatch::ScheduleSceneLocked(cmd_list);
  } catch (const std::exception& error) {
    // The engine continues its post-processing; this scene is not reconstructed.
    static std::atomic_flag reported;
    if (!reported.test_and_set(std::memory_order_relaxed)) {
      logging::Warn("scheduling reconstruction failed: ", error.what());
    }
  }
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
                    " jitter_pattern=halton_2_3");
      logging::Info("DLSS startup=vendor-only; DLL inspection, NGX and hooks are selection-driven");
      projection_jitter::Use(fdw_reason);
      resolution::Use(fdw_reason);
      mip_bias::Use(fdw_reason);

      // Until the submission bridge is installed, its handlers return before
      // any QueryInterface call or lock.
      reshade::register_event<reshade::addon_event::init_command_list>(rendering::submission::OnInitCommandList);
      reshade::register_event<reshade::addon_event::destroy_command_list>(rendering::submission::OnDestroyCommandList);
      reshade::register_event<reshade::addon_event::execute_secondary_command_list>(
          rendering::submission::OnExecuteSecondaryCommandList);
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

      reshade::unregister_event<reshade::addon_event::init_command_list>(rendering::submission::OnInitCommandList);
      reshade::unregister_event<reshade::addon_event::destroy_command_list>(rendering::submission::OnDestroyCommandList);
      reshade::unregister_event<reshade::addon_event::execute_secondary_command_list>(
          rendering::submission::OnExecuteSecondaryCommandList);
      reshade::unregister_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
      reshade::unregister_event<reshade::addon_event::present>(OnPresent);
      break;
  }
}

}  // namespace mgsv::reconstruction

#endif  // RENODX_GAMES_MGSV_RECONSTRUCTION_RUNTIME_HPP_
