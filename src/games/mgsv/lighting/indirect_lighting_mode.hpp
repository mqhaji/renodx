#ifndef MGSV_LIGHTING_INDIRECT_LIGHTING_MODE_HPP_
#define MGSV_LIGHTING_INDIRECT_LIGHTING_MODE_HPP_

#include <atomic>

#include <include/reshade.hpp>

#include "../rendering/native_context.hpp"

namespace mgsv::lighting {

// Present copies the user's mode here, so every pass recorded for one frame
// reads the same answer without taking the settings lock on a worker thread.
inline std::atomic<bool> indirect_lighting_enhanced = false;

// Enhanced indirect lighting belongs to the presented main view. The material
// capture, the diffuse gather, the environment export, and the reflections all
// ask this one question: a view that exports its environment light for
// replacement must also be the view whose reflections replace it. Other views
// keep the game's own lighting.
inline bool UsesEnhancedIndirectLighting() {
  if (!indirect_lighting_enhanced.load(std::memory_order_acquire)) return false;
  const auto owner = rendering::native_context::GetScope().owner;
  if (owner.scene == 0u) {
    static std::atomic_flag reported = {};
    if (!reported.test_and_set(std::memory_order_relaxed)) {
      reshade::log::message(reshade::log::level::warning, "MgsvLighting: a lighting draw has no native view owner; it keeps native indirect lighting");
    }
    return false;
  }
  return owner.main_view && owner.active_member && owner.presentation_matches;
}

}  // namespace mgsv::lighting

#endif  // MGSV_LIGHTING_INDIRECT_LIGHTING_MODE_HPP_
