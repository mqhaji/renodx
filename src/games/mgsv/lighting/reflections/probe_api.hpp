// SPDX-License-Identifier: MIT
#ifndef MGSV_REFLECTIONS_PROBE_API_HPP_
#define MGSV_REFLECTIONS_PROBE_API_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <shared_mutex>

#include "../../../../utils/cross_addon.hpp"

namespace mgsv::lighting::reflections::probe {

// The probe compiles and validates a complete set outside the renderer. These
// borrowed bytes are copied by the provider before Request returns. No owning
// containers, COM objects, or allocator responsibilities cross the DLL boundary.
inline constexpr size_t PASS_COUNT = 5u;
inline constexpr uint32_t CONSTANT_BYTES = 128u;
struct Bytecode {
  const uint8_t* data = nullptr;
  size_t size = 0u;
};
struct Request {
  uint64_t expected_revision = 0u;
  std::array<Bytecode, PASS_COUNT> shaders;
  bool restore = false;
};
// Development-only comparisons. Each starts from the SDK defaults, changes
// one stage or history policy, and restarts history without replacing shaders.
enum class DenoiserMode : uint8_t {
  DEFAULT,
  NO_STABILIZATION,
  SPATIAL_ONLY,
  TRACKING_PREPASS_ONLY,
  SHORT_HISTORY,
  COUNT,
};
inline constexpr uint32_t MAX_CAMERA_SNAPSHOTS = 4u;
struct CameraSnapshot {
  uint64_t scene = 0u;
  uint64_t owner_generation = 0u;
  uint64_t reservation = 0u;
  uint64_t publication = 0u;
  uint32_t sample = 0u;
  std::array<uint32_t, 2> size = {};
  std::array<float, 2> jitter = {};
  std::array<float, 16> source_projection = {};
  std::array<float, 16> source_view = {};
  std::array<float, 16> source_previous_projection = {};
  std::array<float, 16> source_previous_view = {};
  std::array<float, 16> native_previous_projection = {};
  std::array<float, 16> native_previous_view = {};
  bool native_history_available = false;
  std::array<float, 16> nrd_projection = {};
  std::array<float, 16> nrd_view = {};
  std::array<float, 16> nrd_previous_projection = {};
  std::array<float, 16> nrd_previous_view = {};
  std::array<float, 16> current_to_previous_clip = {};
  // Preserve raw SDK words, including integer fields and NaN bit patterns.
  // Analysis interprets the prefix using the pinned SDK's constant layout.
  std::array<uint32_t, 256> sdk_constants = {};
  uint32_t sdk_constant_bytes = 0u;
};
struct Status {
  uint64_t revision = 0u;
  uint64_t active_revision = 0u;
  uint64_t submitted_revision = 0u;
  bool pending = false;
  bool live = false;
  uint64_t nrd_submissions = 0u;
  uint64_t nrd_restarts = 0u;
  std::array<char, 256> error = {};
  DenoiserMode denoiser_mode = DenoiserMode::DEFAULT;
  uint64_t denoiser_revision = 0u;
  uint64_t denoiser_submitted_revision = 0u;
  uint64_t camera_revision = 0u;
  uint32_t camera_requested = 0u;
  uint32_t camera_count = 0u;
  std::array<CameraSnapshot, MAX_CAMERA_SNAPSHOTS> cameras;
};

// Callbacks only copy requests/status. Hold this lock while calling them so
// unloading the provider cannot leave a caller executing code from its DLL.
// The provider never calls back into this interface while holding its own lock.
// This UUID identifies the NRD guide layout, its 128-byte controls and live
// denoiser configuration. Older probes must not use a different callback layout.
struct __declspec(uuid("B4ED6C86-0731-40AE-8DC9-6A3E70EA1B02")) Interface {
  std::shared_mutex mutex;
  bool (*request)(const Request*, Status*) = nullptr;
  void (*read)(Status*) = nullptr;
  bool (*configure_denoiser)(DenoiserMode, Status*) = nullptr;
  bool (*capture_camera)(uint32_t, Status*) = nullptr;
};
static renodx::utils::cross_addon::Shared<Interface> shared;

}  // namespace mgsv::lighting::reflections::probe
#endif
