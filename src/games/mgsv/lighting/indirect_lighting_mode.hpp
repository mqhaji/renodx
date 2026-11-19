#ifndef MGSV_LIGHTING_INDIRECT_LIGHTING_MODE_HPP_
#define MGSV_LIGHTING_INDIRECT_LIGHTING_MODE_HPP_

#include <atomic>

#include <include/reshade.hpp>

#include "../engine/views.hpp"

namespace mgsv::lighting {

// Present stores here whether Enhanced indirect lighting is in effect: the mode
// is Enhanced and the RenoDX tone mapper is selected. Every pass recorded for
// one frame reads the same answer without taking the settings lock on a worker
// thread.
inline std::atomic<bool> indirect_lighting_enhanced = false;

// Enhanced indirect lighting runs only in the presented main view. The material
// capture, the diffuse gather, the environment export, and the reflections all
// ask this one function, because a view that exports its environment light for
// replacement must also be the view whose reflections replace it. Other views
// keep the game's lighting.
inline bool UsesEnhancedIndirectLighting() {
  if (!indirect_lighting_enhanced.load(std::memory_order_acquire)) return false;
  const auto owner = engine::views::GetScope().owner;
  if (owner.scene == 0u) {
    static std::atomic_flag reported = {};
    if (!reported.test_and_set(std::memory_order_relaxed)) {
      reshade::log::message(reshade::log::level::warning, "MgsvLighting: a lighting draw has no native view owner; it keeps native indirect lighting");
    }
    return false;
  }
  return engine::views::IsPresentedMainView(owner);
}

}  // namespace mgsv::lighting

#endif  // MGSV_LIGHTING_INDIRECT_LIGHTING_MODE_HPP_
