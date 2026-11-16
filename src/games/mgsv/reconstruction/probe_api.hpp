// SPDX-License-Identifier: MIT
#ifndef RENODX_GAMES_MGSV_RECONSTRUCTION_PROBE_API_HPP_
#define RENODX_GAMES_MGSV_RECONSTRUCTION_PROBE_API_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <shared_mutex>

#include "../../../utils/cross_addon.hpp"

namespace mgsv::reconstruction::probe {

struct Request {
  uint64_t expected_revision = 0u;
  const uint8_t* code = nullptr;
  size_t size = 0u;
  bool restore = false;
};

struct Status {
  uint64_t revision = 0u;
  uint64_t active_revision = 0u;
  uint64_t submitted_revision = 0u;
  bool pending = false;
  bool live = false;
  std::array<char, 256> error = {};
};

// Requests copy borrowed bytecode before returning. No owning containers or
// native graphics objects cross this boundary. Hold the interface lock while
// calling a callback so the provider cannot unload during the call.
struct __declspec(uuid("847E48E7-C140-4A70-9389-34246E069325")) Interface {
  std::shared_mutex mutex;
  bool (*request)(const Request*, Status*) = nullptr;
  void (*read)(Status*) = nullptr;
};
static renodx::utils::cross_addon::Shared<Interface> shared;

}  // namespace mgsv::reconstruction::probe
#endif
