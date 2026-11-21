// SPDX-License-Identifier: MIT
#ifndef RENODX_GAMES_MGSV_RENDERING_PROBE_API_HPP_
#define RENODX_GAMES_MGSV_RENDERING_PROBE_API_HPP_

#include <cstddef>
#include <cstdint>
#include <shared_mutex>

#include "../../../utils/cross_addon.hpp"
#include "d3d12_timing.hpp"

// What the local development probe add-on may ask of the mod beyond each
// feature's interface: its settings, timing of its D3D12 work, and shaders
// to try in the running game.
namespace mgsv::rendering::probe {

// Callbacks only copy values. Hold this lock while calling them so unloading
// the provider cannot leave a caller executing code from its DLL.
struct __declspec(uuid("5E2B9C41-7A3D-4F86-B10E-93C4D8A26F57")) Interface {
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
  // Replaces a shader by the name it is embedded under; see live_shaders.hpp.
  void (*replace_shader)(const char* name, const uint8_t* data, size_t size) = nullptr;
};
static renodx::utils::cross_addon::Shared<Interface> shared;

}  // namespace mgsv::rendering::probe

#endif  // RENODX_GAMES_MGSV_RENDERING_PROBE_API_HPP_
