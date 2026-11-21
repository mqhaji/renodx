// SPDX-License-Identifier: MIT
#ifndef RENODX_GAMES_MGSV_ENGINE_PAUSE_HPP_
#define RENODX_GAMES_MGSV_ENGINE_PAUSE_HPP_

#include <array>
#include <cstdint>
#include <optional>

#include "build.hpp"
#include "image.hpp"

// The engine's pause system. Each pause source (the pause menu, the game in
// the background, a paused cutscene, game over, some iDroid confirmations)
// holds a set of flags; at the start of every game frame the engine combines
// the flags of the active sources into one word, and GameTime::Update stops
// game time while any of its flags are set. Characters, vehicles and gimmicks
// then do not update. First person, scopes and ordinary iDroid browsing do not
// pause.
namespace mgsv::engine::pause {

inline constexpr std::array REQUIRED_ENTRIES = {Entry::GAME_TIME_UPDATE, Entry::GET_PAUSE_SYSTEM, Entry::PAUSE_SYSTEM};
inline constexpr std::array REQUIRED_FIELDS = {Field::PAUSE_FLAGS, Field::PAUSE_FLAGS_GAME_TIME};
inline constexpr Requirement REQUIREMENT = {.feature = "Game pause", .entries = REQUIRED_ENTRIES, .fields = REQUIRED_FIELDS};

// Whether game time is stopped, as of the start of the current game frame;
// rendering can lag that by a frame. Any thread may read it: the engine writes
// the word with one aligned 32-bit store. Empty when the entries did not verify.
inline std::optional<bool> IsGameTimeStopped() {
  static volatile const uint32_t* const FLAGS =
      Verified(REQUIREMENT) ? reinterpret_cast<const volatile uint32_t*>(Address(Entry::PAUSE_SYSTEM) + Value(Field::PAUSE_FLAGS)) : nullptr;
  if (FLAGS == nullptr) return std::nullopt;
  return (*FLAGS & Value(Field::PAUSE_FLAGS_GAME_TIME)) != 0u;
}

}  // namespace mgsv::engine::pause

#endif  // RENODX_GAMES_MGSV_ENGINE_PAUSE_HPP_
