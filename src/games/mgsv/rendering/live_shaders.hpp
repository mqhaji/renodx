// SPDX-License-Identifier: MIT
#ifndef MGSV_RENDERING_LIVE_SHADERS_HPP_
#define MGSV_RENDERING_LIVE_SHADERS_HPP_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <shared_mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

// Development: shaders the local probe replaced in the running game, by the
// name they are embedded under (gtvbgi for __gtvbgi). The diffuse lighting
// chain's passes and the fog volume's passes recreate their shaders from
// these bytes when the revision changes, so a shader edit is tried without
// restarting the game. Nothing is replaced unless the probe sends it.
namespace mgsv::rendering::live_shaders {

struct State {
  std::shared_mutex mutex;
  std::unordered_map<std::string, std::vector<uint8_t>> shaders;
  uint32_t revision = 0u;
};

static State state;

// The bytes to create name's pipeline from: the probe's replacement if it
// sent one, else the embedded shader.
inline std::vector<uint8_t> Bytes(const std::string& name, std::span<const uint8_t> embedded) {
  const std::shared_lock lock(state.mutex);
  if (const auto replaced = state.shaders.find(name); replaced != state.shaders.end()) return replaced->second;
  return {embedded.begin(), embedded.end()};
}

inline uint32_t Revision() {
  const std::shared_lock lock(state.mutex);
  return state.revision;
}

// Copies the bytes before returning. A shader whose bytes did not change
// leaves the revision alone.
inline void Replace(const char* name, const uint8_t* data, size_t size) {
  const std::unique_lock lock(state.mutex);
  auto& stored = state.shaders[name];
  if (stored.size() == size && std::equal(stored.begin(), stored.end(), data)) return;
  stored.assign(data, data + size);
  ++state.revision;
}

}  // namespace mgsv::rendering::live_shaders

#endif  // MGSV_RENDERING_LIVE_SHADERS_HPP_
