// SPDX-License-Identifier: MIT
#ifndef MGSV_LIGHTING_MEMORY_PROBE_API_HPP_
#define MGSV_LIGHTING_MEMORY_PROBE_API_HPP_

#include <array>
#include <cstdint>
#include <shared_mutex>

#include "../../../../utils/cross_addon.hpp"

// Development interface between the game addon and the local lighting probe
// addon: the probe turns scene memory upkeep on or off and reads its state.
namespace mgsv::lighting::memory::probe {

// The state the GPU kept for the latest frame read back, and what the host
// knew when it recorded that frame.
struct Status {
  bool enabled = false;
  // Updates recorded, and how many of those the host has read back.
  uint64_t submissions = 0u;
  uint64_t readbacks = 0u;
  // Submissions between recording a frame and reading it back.
  uint64_t readback_lag = 0u;
  // The engine's pause word: whether it verified, and in how many recorded
  // updates game time was stopped.
  bool pause_known = false;
  bool paused = false;
  uint64_t paused_submissions = 0u;
  // Sample counts of the frame read back.
  uint32_t valid_samples = 0u;
  uint32_t moving_samples = 0u;
  bool sun_atlas = false;
  uint32_t sun_covered_samples = 0u;
  uint32_t sun_facing_samples = 0u;
  uint32_t sun_epoch = 0u;
  std::array<float, 3> sun_direction = {};
  std::array<float, 3> eye = {};
  // Distance from the GPU eye to the camera position the host computed for
  // the same frame, and to the translation of the GPU camera matrix. Negative
  // when the host had no valid camera.
  float eye_error = -1.f;
  float eye_mismatch = 0.f;
  uint32_t gpu_frames = 0u;
  // Grids: each level's origin (world cell index of its first cell), its
  // cells near a surface, the samples marked in the frame read back and
  // whether that frame marked, the cells it emptied, each level's known
  // cells (counted only while the probe enables upkeep; 1048576 per level),
  // the samples that wrote surface color and sun visibility, the full clears
  // the host requested, whether the game's terrain textures were in use and
  // the cells the terrain pass touched.
  std::array<std::array<int32_t, 3>, 4> origins = {};
  std::array<uint32_t, 4> near_cells = {};
  uint32_t marked_samples = 0u;
  bool marking = false;
  uint32_t carved_cells = 0u;
  std::array<uint32_t, 4> known_cells = {};
  uint32_t color_samples = 0u;
  uint32_t sun_samples = 0u;
  uint64_t clears = 0u;
  bool terrain = false;
  uint32_t terrain_cells = 0u;
  // For reading captures: the lighting debug view of the latest update, the
  // camera's forward direction and the scene's exposure scale (small by day,
  // large at night).
  int32_t debug_view = 0;
  std::array<float, 3> forward = {};
  float exposure = 0.f;
};

// The callbacks only copy status and flags. Callers hold this lock while
// calling them, so unloading the game addon cannot leave the probe running
// code from its DLL. Change the UUID when the layout changes.
struct __declspec(uuid("3B9D5E72-6C14-4A8F-9E03-D7A21F4C8B56")) Interface {
  std::shared_mutex mutex;
  void (*enable)(bool) = nullptr;
  void (*read)(Status*) = nullptr;
};
static renodx::utils::cross_addon::Shared<Interface> shared;

}  // namespace mgsv::lighting::memory::probe
#endif
