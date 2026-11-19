// SPDX-License-Identifier: MIT
#ifndef RENODX_GAMES_MGSV_RECONSTRUCTION_PROBE_API_HPP_
#define RENODX_GAMES_MGSV_RECONSTRUCTION_PROBE_API_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <shared_mutex>

#include "../../../utils/cross_addon.hpp"

// Cross-addon interface for the MGSV lighting probe addon. The probe can swap
// the reconstruction preparation shader while the game runs and read back
// status. The mod registers the callbacks; the probe calls them.
namespace mgsv::reconstruction::probe {

struct Request {
  // Must equal the current Status::revision, so a stale request is rejected.
  uint64_t expected_revision = 0u;
  const uint8_t* code = nullptr;
  size_t size = 0u;
  // Return to the built-in shader; code and size are ignored.
  bool restore = false;
};

// Counts from the hook on the engine's post-processing start, for the
// presented main view. Normally starts equals cuts and the other three stay
// zero.
struct PostFilterStatus {
  uint64_t starts = 0u;
  uint64_t cuts = 0u;
  uint64_t lists_open_at_start = 0u;
  uint64_t unused_starts = 0u;
  uint64_t repeated_starts = 0u;
};

// revision counts accepted requests, active_revision is the one the shader was
// last built from, and submitted_revision is the one a reconstruction has run
// with since. live is true while probe bytecode replaces the built-in shader.
struct Status {
  uint64_t revision = 0u;
  uint64_t active_revision = 0u;
  uint64_t submitted_revision = 0u;
  bool pending = false;
  bool live = false;
  std::array<char, 256> error = {};
  PostFilterStatus post_filter;
};

// request copies the bytecode before it returns; the caller keeps ownership.
// Only plain data crosses this boundary, never containers or graphics objects.
// Hold `mutex` while calling a callback so the mod cannot unregister it
// during the call.
struct __declspec(uuid("847E48E7-C140-4A70-9389-34246E069325")) Interface {
  std::shared_mutex mutex;
  bool (*request)(const Request*, Status*) = nullptr;
  void (*read)(Status*) = nullptr;
};
static renodx::utils::cross_addon::Shared<Interface> shared;

}  // namespace mgsv::reconstruction::probe
#endif
