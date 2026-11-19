// SPDX-License-Identifier: MIT
#ifndef RENODX_GAMES_MGSV_RENDERING_PROBE_API_HPP_
#define RENODX_GAMES_MGSV_RENDERING_PROBE_API_HPP_

#include <cstdint>
#include <shared_mutex>

#include "../../../utils/cross_addon.hpp"
#include "d3d12_timing.hpp"

// What the local development probe add-on may ask of the mod beyond each
// feature's interface: its settings, and timing of its D3D12 work.
namespace mgsv::rendering::probe {

// Callbacks only copy values. Hold this lock while calling them so unloading
// the provider cannot leave a caller executing code from its DLL.
struct __declspec(uuid("C0ACDAC9-F85A-403E-99AD-7FA953196FEC")) Interface {
  std::shared_mutex mutex;
  // Reads any numeric setting by its saved key.
  bool (*get_setting)(const char* key, float* value) = nullptr;
  // Changes a lighting setting (a DirectLighting or IndirectLighting key), or
  // the reconstruction method or its quality, the way the menu does. The
  // preset is not saved, so a restart restores it.
  bool (*set_setting)(const char* key, float value) = nullptr;
  // Times the next submissions of each D3D12 stream; see d3d12_timing.hpp.
  void (*request_d3d12_timing)(uint32_t submissions) = nullptr;
  void (*read_d3d12_timing)(d3d12_timing::Report* report) = nullptr;
};
static renodx::utils::cross_addon::Shared<Interface> shared;

}  // namespace mgsv::rendering::probe

#endif  // RENODX_GAMES_MGSV_RENDERING_PROBE_API_HPP_
