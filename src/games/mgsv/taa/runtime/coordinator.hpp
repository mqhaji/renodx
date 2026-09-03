#pragma once

/* Method-neutral temporal candidate validation, dispatch, and completion. */

#include <intrin.h>

#include <atomic>

#include "../analytical/runtime.hpp"
#include "../fsr3/runtime.hpp"
#include "./camera_state.hpp"
#include "./d3d11_compute_state.hpp"
#include "./descriptor_tracker.hpp"
#include "./input_capture.hpp"
#include "./logging.hpp"
#include "./state.hpp"

namespace taa::coordinator {

inline std::atomic_flag execution_lock = ATOMIC_FLAG_INIT;
inline reshade::api::device* runtime_device = nullptr;

struct ExecutionGuard {
  ExecutionGuard() {
    while (execution_lock.test_and_set(std::memory_order_acquire)) {
      _mm_pause();
    }
  }
  ExecutionGuard(const ExecutionGuard&) = delete;
  ExecutionGuard& operator=(const ExecutionGuard&) = delete;
  ExecutionGuard(ExecutionGuard&&) = delete;
  ExecutionGuard& operator=(ExecutionGuard&&) = delete;
  ~ExecutionGuard() { execution_lock.clear(std::memory_order_release); }
};

inline void ResetTemporalStateWithPublicationLocked(const char* reason) {
  input_capture::InvalidateCapturedFrame();
  analytical::InvalidateHistory(reason);
  fsr3::InvalidateHistory();
  camera_state::InvalidateLocked();
  camera_state::ResetMatrixHistoryLocked();
  state::ResetTemporalState();
}

inline void ResetTemporalState(const char* reason) {
  camera_state::PublicationWriterGuard publication_guard;
  ResetTemporalStateWithPublicationLocked(reason);
}

inline void Destroy(reshade::api::device* device) {
  if (runtime_device != nullptr && device != runtime_device) return;
  input_capture::Destroy(device);
  analytical::Destroy(device);
  fsr3::Destroy();
  runtime_device = nullptr;
}

inline void ReleaseInactiveResources(
    reshade::api::device* device,
    state::TemporalMode mode) {
  if (mode != state::TemporalMode::ANALYTICAL_TAA) {
    analytical::Release(device);
  }
  if (mode != state::TemporalMode::AMD_FSR3) {
    fsr3::ReleaseTemporalResources(device);
  }
}

inline void MaybeRunLocked(
    reshade::api::command_list* cmd_list,
    const descriptor_tracker::CommandListData& command_data,
    const char* insertion_name) {
  if (!state::IsEnabled() || state::frame_state.reconstruction_completed) return;

  ValidatedFrameInputs inputs = {};
  if (!input_capture::BuildValidatedFrameInputsLocked(
          cmd_list,
          command_data.pixel_srv_t0,
          insertion_name,
          inputs)) {
    return;
  }
  if (runtime_device != nullptr && runtime_device != inputs.device) {
    logging::Warn("rejecting temporal dispatch from a second D3D11 device");
    return;
  }
  runtime_device = inputs.device;

  const auto previous_compute_state = d3d11_compute_state::Capture(cmd_list);
  cmd_list->barrier(
      inputs.velocity_resource,
      reshade::api::resource_usage::render_target,
      reshade::api::resource_usage::shader_resource);

  MethodOutput output = {};
  bool succeeded = false;
  switch (state::GetTemporalMode()) {
    case state::TemporalMode::ANALYTICAL_TAA:
      succeeded = analytical::Dispatch(inputs, output);
      break;
    case state::TemporalMode::AMD_FSR3:
      succeeded = fsr3::Dispatch(inputs, output);
      break;
    case state::TemporalMode::OFF:
      break;
  }
  if (succeeded && output.resource.handle != 0u) {
    cmd_list->barrier(
        output.resource,
        reshade::api::resource_usage::unordered_access,
        reshade::api::resource_usage::copy_source);
    cmd_list->barrier(
        inputs.color_resource,
        reshade::api::resource_usage::shader_resource,
        reshade::api::resource_usage::copy_dest);
    cmd_list->copy_resource(output.resource, inputs.color_resource);
    cmd_list->barrier(
        inputs.color_resource,
        reshade::api::resource_usage::copy_dest,
        reshade::api::resource_usage::shader_resource);
    cmd_list->barrier(output.resource, reshade::api::resource_usage::copy_source, output.final_usage);
  } else {
    succeeded = false;
  }
  cmd_list->barrier(
      inputs.velocity_resource,
      reshade::api::resource_usage::shader_resource,
      reshade::api::resource_usage::render_target);
  d3d11_compute_state::Restore(cmd_list, previous_compute_state);
  if (!succeeded) return;

  if (!camera_state::Commit(inputs.camera.frame_token, inputs.sample_index)) {
    ResetTemporalState("native camera matrix commit failed");
    logging::Warn("temporal native camera matrix commit failed");
    state::frame_state.reconstruction_completed = true;
    return;
  }
  state::CommitTemporalFrame();
}

}  // namespace taa::coordinator
