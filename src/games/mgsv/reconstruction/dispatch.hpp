#ifndef RENODX_GAMES_MGSV_RECONSTRUCTION_DISPATCH_HPP_
#define RENODX_GAMES_MGSV_RECONSTRUCTION_DISPATCH_HPP_

/* Runs a scene's temporal reconstruction. ScheduleSceneLocked cuts the scene's
 * job into the list the engine has just begun for its post-processing
 * (rendering/submission.hpp); the job runs the selected method on the
 * immediate context when the game executes that list. This file also owns the
 * reconstruction device and the resets of method history. */

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <utility>

#include <d3d11_1.h>
#include <wrl/client.h>

#include "../engine/render_targets.hpp"
#include "../engine/views.hpp"
#include "../rendering/d3d11_state.hpp"
#include "../rendering/d3d12_bridge.hpp"
#include "../rendering/d3d12_timing.hpp"
#include "../rendering/producer_records.hpp"
#include "../rendering/submission.hpp"
#include "analytical/runtime.hpp"
#include "camera_state.hpp"
#include "d3d12.hpp"
#include "dlss/runtime.hpp"
#include "frame_inputs.hpp"
#include "fsr/runtime.hpp"
#include "input_capture.hpp"
#include "logging.hpp"
#include "output_size/resources.hpp"
#include "resolution.hpp"
#include "state.hpp"

namespace mgsv::reconstruction::dispatch {

inline reshade::api::device* runtime_device = nullptr;
// Protected by the submission ExecutionGuard. After a failed teardown, no new
// work may use the old device's resources, even if a new device reuses its
// address.
inline bool device_teardown_pending = false;
inline std::atomic<bool> logged_immediate_dispatch = false;
// Set when scheduled work could not run as planned. The next Present turns
// temporal reconstruction off.
inline std::atomic<bool> submission_failure_pending = false;

// The first device seen becomes the reconstruction device; later calls succeed
// only for that device.
inline bool BindRuntimeDevice(reshade::api::device* device) {
  if (device == nullptr || device_teardown_pending) return false;
  if (runtime_device == nullptr) {
    runtime_device = device;
  }
  return runtime_device == device;
}

inline bool IsForeignDevice(reshade::api::device* device) {
  return runtime_device != nullptr && runtime_device != device;
}

inline void ResetTemporalStateWithPublicationLocked(const char* reason) {
  analytical::InvalidateHistory(reason);
  dlss::InvalidateHistory();
  fsr::InvalidateHistory();
  camera_state::InvalidateLocked();
  camera_state::ResetMatrixHistoryLocked();
  state::ResetTemporalState();
}

inline void ResetTemporalState(const char* reason) {
  const camera_state::PublicationWriterGuard publication_guard;
  ResetTemporalStateWithPublicationLocked(reason);
}

inline void Destroy(reshade::api::device* device) {
  if (runtime_device != nullptr && device != runtime_device) return;
  dx12::Drain();
  analytical::Destroy(device);
  dlss::Destroy(device);
  fsr::Destroy();
  dx12::Destroy();
  rendering::d3d12_bridge::Destroy();
  output_size::Reset(true);
  submission_failure_pending.store(false, std::memory_order_release);
  runtime_device = nullptr;
  lighting::temporal_inputs::Reset();
  device_teardown_pending = false;
}

// Called under ExecutionGuard at Present and before each evaluation; either can
// come first after a method change. Frees the other methods' GPU allocations
// but keeps their SDKs loaded, so switching back neither passes through Off nor
// resizes the game while a runtime loads again.
inline void ReleaseInactiveResources(reshade::api::device* device, state::TemporalMode mode) {
  if (mode != state::TemporalMode::ANALYTICAL_TAA) {
    analytical::Destroy(device);
  }
  if (mode != state::TemporalMode::AMD_FSR) {
    fsr::ReleaseContext();
  }
  if (mode != state::TemporalMode::NVIDIA_DLSS) {
    if (!dlss::ReleaseFeatureResources()) {
      throw std::runtime_error("NVIDIA DLSS feature release failed");
    }
  }
}

// Runs one scene's reconstruction on the immediate context, between the native
// prefix and remainder. Returns true when the method wrote the output.
inline bool DispatchValidatedLocked(ValidatedFrameInputs& inputs) {
  // A recorded scene can reach submission after device teardown has started.
  if (inputs.device == nullptr || device_teardown_pending) return false;
  const auto destination = (inputs.output.resource.handle != 0u ? inputs.device->get_resource_desc(inputs.output.resource) : reshade::api::resource_desc{});
  if ((state::GetTemporalMode() == state::TemporalMode::ANALYTICAL_TAA && !inputs.HasNativeOutputSize())
      || !IsReconstructionColorTexture(destination)
      || destination.texture.format != inputs.output.format
      || inputs.output.width < inputs.render_width || inputs.output.height < inputs.render_height
      || destination.texture.width != inputs.output.width || destination.texture.height != inputs.output.height
      || inputs.output.resource.handle == 0u
      || inputs.output.format != inputs.color_format) {
    static uint64_t last_output_failure_log = std::numeric_limits<uint64_t>::max();
    if (logging::ShouldLogFrame(state::CurrentFrameToken(), last_output_failure_log)) {
      logging::Warn("rejecting incompatible reconstruction output scene=", inputs.camera.scene,
                    " format=", static_cast<uint32_t>(destination.texture.format),
                    " render=", inputs.render_width, "x", inputs.render_height,
                    " output=", inputs.output.width, "x", inputs.output.height);
    }
    return false;
  }
  {
    const camera_state::PublicationWriterGuard guard;
    auto& camera = inputs.camera;
    const auto& previous = camera_state::committed_camera;
    if (!camera.valid || !camera.camera_matrix_valid || camera.scene == 0u || camera.reservation == 0u
        || camera.reset_generation != camera_state::reset_generation.load(std::memory_order_relaxed)
        || (previous.valid && camera.reservation <= previous.reservation)) return false;

    // The engine's motion vectors lead back to the scene just before this one.
    // If a scene was skipped, start fresh history: the motion cannot span it.
    camera.camera_reprojection_valid = previous.valid
                                       && previous.owner_generation == camera.owner_generation
                                       && previous.reservation + 1u == camera.reservation
                                       && previous.viewport_identity == camera.viewport_identity
                                       && previous.camera_identity == camera.camera_identity
                                       && previous.width == camera.width && previous.height == camera.height;
    camera.previous_jitter_uv_x = (camera.camera_reprojection_valid ? previous.jitter_uv_x : 0.f);
    camera.previous_jitter_uv_y = (camera.camera_reprojection_valid ? previous.jitter_uv_y : 0.f);
    camera.current_to_previous_clip = (camera.camera_reprojection_valid
                                           ? camera_state::ToRowMajorFloatArray(camera_state::Multiply(
                                                 previous.current_view_projection, camera.inverse_view_projection))
                                           : std::array<float, 16>{});
  }
  // Apply a preparation shader the lighting probe has queued.
  if (dx12::probe_pending.load(std::memory_order_acquire)) {
    dx12::ProbeUpdate update;
    {
      const std::unique_lock lock(dx12::probe_state.mutex);
      update = std::move(dx12::probe_state.pending);
      dx12::probe_state.pending = {};
      dx12::probe_pending.store(false, std::memory_order_release);
    }
    try {
      const std::span<const uint8_t> code = (update.restore ? std::span<const uint8_t>(__prepare_reconstruction) : std::span<const uint8_t>(update.code));
      Microsoft::WRL::ComPtr<ID3D11ComputeShader> replacement;
      auto* native = reinterpret_cast<ID3D11Device*>(inputs.device->get_native());  // NOLINT(performance-no-int-to-ptr)
      dx12::Check(native->CreateComputeShader(code.data(), code.size(), nullptr, &replacement), "Create live reconstruction preparation");
      // Commands already recorded keep their own reference to the old shader.
      // Only the preparation shader changes; D3D12 resources stay as they are.
      if (dx12::runtime.prepare != nullptr) {
        dx12::runtime.prepare = std::move(replacement);
      }
      dx12::probe_state.active_code = std::move(update.code);
      // Different inputs start fresh method history. The scene, camera sample
      // and resources are kept.
      analytical::InvalidateHistory("probe reconstruction inputs changed");
      dlss::InvalidateHistory();
      fsr::InvalidateHistory();
      {
        const std::unique_lock lock(dx12::probe_state.mutex);
        auto& status = dx12::probe_state.status;
        status.active_revision = update.revision;
        status.live = !dx12::probe_state.active_code.empty();
        status.pending = dx12::probe_pending.load(std::memory_order_acquire);
        if (status.revision == update.revision) {
          status.error = {};
        }
      }
      dx12::probe_state.report_submission = true;
      logging::Info("probe reconstruction inputs changed; SDK history restarted");
    } catch (const std::exception& error) {
      {
        const std::unique_lock lock(dx12::probe_state.mutex);
        auto& status = dx12::probe_state.status;
        if (status.revision == update.revision) {
          const size_t message_size = std::min(std::strlen(error.what()), status.error.size() - 1u);
          std::copy_n(error.what(), message_size, status.error.data());
          status.error[message_size] = '\0';
        }
        status.pending = dx12::probe_pending.load(std::memory_order_acquire);
      }
      logging::Warn("probe reconstruction update rejected: ", error.what());
    }
  }
  if (!inputs.camera.camera_reprojection_valid) {
    analytical::InvalidateHistory("scene history is not consecutive");
    dlss::InvalidateHistory();
    fsr::InvalidateHistory();
  }
  const auto previous_compute_state = mgsv::rendering::d3d11_compute_state::Capture(inputs.cmd_list);
  inputs.cmd_list->barrier(
      inputs.velocity_resource,
      reshade::api::resource_usage::render_target,
      reshade::api::resource_usage::shader_resource);

  bool succeeded = false;
  try {
    ReleaseInactiveResources(inputs.device, state::GetTemporalMode());
    // Each method writes inputs.output through the shared D3D12 transport
    // (d3d12.hpp). It queues a D3D11 wait, so native post-processing reads
    // that image only after D3D12 has written it.
    switch (state::GetTemporalMode()) {
      case state::TemporalMode::ANALYTICAL_TAA: succeeded = analytical::Dispatch(inputs); break;
      case state::TemporalMode::AMD_FSR:        succeeded = fsr::Dispatch(inputs); break;
      case state::TemporalMode::NVIDIA_DLSS:    succeeded = dlss::Dispatch(inputs); break;
      case state::TemporalMode::OFF:            break;
    }
  } catch (const std::exception& error) {
    static uint64_t last_backend_failure = std::numeric_limits<uint64_t>::max();
    if (logging::ShouldLogFrame(state::CurrentFrameToken(), last_backend_failure)) {
      logging::Warn("D3D12 reconstruction failed: ", error.what());
    }
    // An SDK may have changed its internal state while recording the discarded
    // work, so its history cannot be trusted. DLSS and FSR are turned off at
    // the next Present instead of reusing it.
    if (state::GetTemporalMode() == state::TemporalMode::NVIDIA_DLSS) {
      dlss::QueueFatalFailure();
    } else if (state::GetTemporalMode() == state::TemporalMode::AMD_FSR) {
      // FSR then reports itself unavailable, and the next settings
      // transition selects Off and shows the reason.
      fsr::runtime_failed.store(true, std::memory_order_release);
    }
    succeeded = false;
  }
  inputs.cmd_list->barrier(
      inputs.velocity_resource,
      reshade::api::resource_usage::shader_resource,
      reshade::api::resource_usage::render_target);
  mgsv::rendering::d3d11_compute_state::Restore(inputs.cmd_list, previous_compute_state);
  if (!succeeded) {
    // An SDK evaluation may have changed its history before output validation
    // failed. Its next evaluation must start fresh history.
    analytical::InvalidateHistory("reconstruction output was not installed");
    dlss::InvalidateHistory();
    fsr::InvalidateHistory();
    return false;
  }

  if (dx12::probe_state.report_submission) {
    const std::unique_lock lock(dx12::probe_state.mutex);
    dx12::probe_state.status.submitted_revision = dx12::probe_state.status.active_revision;
    dx12::probe_state.report_submission = false;
  }
  if (!camera_state::Commit(inputs.camera)) {
    ResetTemporalState("native camera matrix commit failed");
    logging::Warn("temporal native camera matrix commit failed");
    state::frame_state.reconstruction_completed = true;
  }
  return true;
}

// One scene's reconstruction, from the cut where its post-processing starts
// until the list holding it executes. The COM references keep the scene's
// inputs and output alive until then.
struct SceneJob {
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> color_srv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> velocity_srv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth_srv;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> object_velocity_srv;
  Microsoft::WRL::ComPtr<ID3D11Resource> output_resource;
  std::shared_ptr<output_size::Output> post_output;
  ValidatedFrameInputs inputs;
  uint64_t temporal_generation = 0u;
  state::TemporalMode mode = state::TemporalMode::OFF;
  // The job only fills the output from the native image: the scene had no
  // motion vectors or camera, or the job was canceled.
  bool fill_only = false;
};

// Decides what the scene gets where its post-processing starts, and schedules
// it in the list the engine has just begun. Called on the build thread with
// the execution lock held.
//   Reconstruct: the record is complete.
//   Fill only: upscaling without motion vectors or a camera. The job scales the
//     native image into the output, so the output-size passes read this
//     frame's pixels, and the next reconstruction starts fresh history.
//   Nothing: there is no usable scene image, or the list cannot be cut. At
//     native size the engine's image stands. While upscaling the engine's
//     render-size path does not present a correct image, so these frames must
//     stay rare. If complete records keep going unscheduled, the check at
//     Present ends upscaling.
inline void ScheduleSceneLocked(reshade::api::command_list* cmd_list) {
  if (!state::IsEnabled()) return;
  const auto scope = engine::views::GetScope();
  // Retained here until the job has taken its own references.
  const auto images = engine::render_targets::ReadFrameImages();
  ValidatedFrameInputs inputs = {};
  const auto record = input_capture::BuildFrameInputsLocked(cmd_list, images, &inputs);
  const bool fill_only = record == input_capture::Record::SCENE_ONLY;
  // At native size the engine's image already stands in for a missing reconstruction.
  if (record == input_capture::Record::UNUSABLE || (fill_only && inputs.HasNativeOutputSize())) return;
  if (!BindRuntimeDevice(inputs.device)) {
    logging::Warn("rejecting temporal dispatch from an unavailable or different D3D11 device");
    return;
  }
  if (rendering::submission::Device() == nullptr
      || (state::GetTemporalMode() == state::TemporalMode::NVIDIA_DLSS && !dlss::runtime_ready.load(std::memory_order_acquire))) return;

  // The engine records with deferred contexts; the job runs when the list is executed.
  auto* native_context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  const auto fail_submission = [] {
    logging::Warn("failed to schedule temporal dispatch");
    submission_failure_pending.store(true, std::memory_order_release);
    state::frame_state.reconstruction_scheduled = true;
  };
  if (native_context == nullptr || native_context->GetType() != D3D11_DEVICE_CONTEXT_DEFERRED
      || rendering::submission::Device() != inputs.device) {
    fail_submission();
    return;
  }
  // A list with an open query cannot be split.
  if (!rendering::submission::CanCut(native_context, false)) return;

  std::shared_ptr<output_size::Output> post_output;
  std::shared_ptr<output_size::OutputProducer> output_producer;
  if (!inputs.HasNativeOutputSize()) {
    post_output = output_size::GetActive(cmd_list);
    if (post_output != nullptr && !post_output->MatchesInputs(inputs)) {
      resolution::Fail("scene reconstruction inputs changed after output recording began");
      return;
    }
    if (post_output == nullptr) {
      try {
        post_output = output_size::AcquireOutput(inputs);
      } catch (const std::exception& error) {
        logging::Warn(error.what());
        resolution::Fail("scaled post-processing resources unavailable");
      }
    }
    if (post_output == nullptr) {
      logging::Warn("scaled output route unavailable scene=", scope.owner.scene, " generation=", scope.owner.generation,
                    " render=", inputs.render_width, "x", inputs.render_height,
                    " output=", inputs.output.width, "x", inputs.output.height,
                    " color_format=", static_cast<uint32_t>(inputs.color_format));
      resolution::Fail("scaled reconstruction has no compatible output route");
      return;
    }
    inputs.output = post_output->target;
    output_size::Recording route;
    output_size::GetActive(cmd_list, &route, true);
    output_producer = route.producer;
  }
  auto scene = std::make_shared<SceneJob>(SceneJob{
      .color_srv = reinterpret_cast<ID3D11ShaderResourceView*>(inputs.color_srv.handle),                      // NOLINT(performance-no-int-to-ptr)
      .velocity_srv = reinterpret_cast<ID3D11ShaderResourceView*>(inputs.velocity_srv.handle),                // NOLINT(performance-no-int-to-ptr)
      .depth_srv = reinterpret_cast<ID3D11ShaderResourceView*>(inputs.depth_srv.handle),                      // NOLINT(performance-no-int-to-ptr)
      .object_velocity_srv = reinterpret_cast<ID3D11ShaderResourceView*>(inputs.object_velocity_srv.handle),  // NOLINT(performance-no-int-to-ptr)
      .post_output = post_output,
      .inputs = inputs,
      .temporal_generation = state::frame_state.temporal_generation,
      .mode = state::GetTemporalMode(),
      .fill_only = fill_only,
  });
  // At native size the output is the scene image itself, which color_srv
  // already keeps alive. A separate output needs its own reference until the
  // job runs.
  if (inputs.output.resource.handle != inputs.color_resource.handle) {
    scene->output_resource = reinterpret_cast<ID3D11Resource*>(inputs.output.resource.handle);  // NOLINT(performance-no-int-to-ptr)
  }
  const auto result = rendering::submission::ScheduleLocked(
      cmd_list,
      {
          // Runs on the immediate context after the native commands before the
          // cut. A replayed scene is not added to history a second time; while
          // upscaling, its output is refreshed from the native image instead.
          .run = [scene](reshade::api::command_list* command_list, const rendering::submission::Execution& execution) {
            auto* context = reinterpret_cast<ID3D11DeviceContext*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
            // Every path below reads output-size depth, and the fill uses constants
            // this preparation writes, so it runs first.
            const bool depth_ready = scene->post_output == nullptr
                                     || scene->post_output->PrepareDepth(context, scene->inputs.camera.jitter_uv_x, scene->inputs.camera.jitter_uv_y);
            rendering::d3d12_timing::CpuSubmission::EndPart("output depth");
            const bool reconstructs = !scene->fill_only && execution.first
                                      && scene->temporal_generation == state::frame_state.temporal_generation
                                      && state::GetTemporalMode() == scene->mode;
            if (reconstructs && depth_ready && execution.state_saved && scene->inputs.device == runtime_device) {
              if (!logged_immediate_dispatch.exchange(true, std::memory_order_relaxed)) {
                logging::Info("executing temporal dispatch on immediate context mode=", static_cast<uint32_t>(scene->mode),
                              " frame=", scene->inputs.frame_token, " submission_frame=", state::CurrentFrameToken());
              }
              auto inputs = scene->inputs;
              inputs.cmd_list = command_list;
              const bool reconstructed = DispatchValidatedLocked(inputs);
              rendering::d3d12_timing::CpuSubmission::EndPart("compute state restore, commit");
              if (reconstructed) return;
            } else if (reconstructs) {
              logging::Warn("rejected scene reconstruction submission because its device or saved state is unavailable");
              submission_failure_pending.store(true, std::memory_order_release);
            }
            if (reconstructs) {
              state::frame_state.reconstruction_scheduled = false;
            }
            if (scene->fill_only) {
              // The next reconstruction must not continue history across this scene.
              analytical::InvalidateHistory("a scene was filled without reconstruction");
              dlss::InvalidateHistory();
              fsr::InvalidateHistory();
            }
            if (scene->post_output != nullptr) {
              scene->post_output->CopyNative(context);
            } },
          // No D3D12 hand-off is possible inside another deferred list, so
          // the scene is reported as a submission failure.
          .run_merged = [scene](reshade::api::command_list* deferred, bool first) {
            if (scene->post_output != nullptr) {
              output_size::RetainOutput(deferred, scene->post_output);
            }
            if (!first || state::GetTemporalMode() != scene->mode
                || scene->temporal_generation != state::frame_state.temporal_generation) return;
            state::frame_state.reconstruction_scheduled = false;
            logging::Warn("temporal post list was merged into another deferred context");
            submission_failure_pending.store(true, std::memory_order_release); },
          .on_abandoned = [generation = scene->temporal_generation] {
            if (!state::IsEnabled()) return;
            const rendering::submission::ExecutionGuard execution_guard;
            if (generation == state::frame_state.temporal_generation && state::frame_state.reconstruction_scheduled) {
              state::frame_state.reconstruction_scheduled = false;
              logging::Info("the command list holding a scene's reconstruction was abandoned");
            } },
          // The swapchain the scene was recorded for is gone. The game's
          // commands after the cut still read the output, so the job stays and
          // fills it from the native image; its references to the scene's
          // images are released.
          .cancel = [scene] {
            scene->color_srv.Reset();
            scene->velocity_srv.Reset();
            scene->depth_srv.Reset();
            scene->object_velocity_srv.Reset();
            scene->output_resource.Reset();
            scene->inputs = {};
            scene->fill_only = true; },
          .timing = rendering::d3d12_timing::Stream::RECONSTRUCTION,
      });
  if (result == rendering::submission::Result::BUSY) return;
  if (result != rendering::submission::Result::SCHEDULED) {
    fail_submission();
    return;
  }
  // The output-size route redirects the rest of this list's post-processing
  // into this output, so make it the active output now.
  if (post_output != nullptr) {
    const std::unique_lock lock(output_size::state.mutex);
    output_size::Recording route = {.output = post_output, .producer = output_producer, .frame = inputs.frame_token, .generation = output_size::state.generation, .owner = scope.owner};
    output_producer->reconstruction_recorded = true;
    output_size::state.active = post_output;
    output_size::state.producer = output_producer;
    output_size::state.frame = inputs.frame_token;
    output_size::state.active_owner = scope.owner;
    if (const auto pooled = std::ranges::find(output_size::state.pool, post_output, &output_size::Recording::output); pooled != output_size::state.pool.end()) {
      *pooled = route;
    }
    const auto existing = std::ranges::find_if(output_size::state.sequences, [&](const output_size::Recording& candidate) {
      return candidate.owner.scene == scope.owner.scene && candidate.owner.generation == scope.owner.generation;
    });
    if (existing == output_size::state.sequences.end()) {
      output_size::state.sequences.push_back(route);
    } else {
      *existing = route;
    }
    if (auto* recording = cmd_list->get_private_data<output_size::Recording>(); recording != nullptr) {
      output_size::RetainOutput(cmd_list, recording->output);
      route.retained_outputs = std::move(recording->retained_outputs);
      *recording = std::move(route);
    }
  }
  if (fill_only) return;
  state::frame_state.reconstruction_scheduled = true;
  if (!logged_immediate_dispatch.load(std::memory_order_relaxed)) {
    logging::Info("scheduled temporal dispatch frame=", inputs.frame_token);
  }
}

}  // namespace mgsv::reconstruction::dispatch

#endif  // RENODX_GAMES_MGSV_RECONSTRUCTION_DISPATCH_HPP_
