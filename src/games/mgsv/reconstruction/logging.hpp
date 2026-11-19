#pragma once

/*
 * Logging helpers for temporal reconstruction. Messages go to ReShade.log with
 * an "MgsvTaa: " prefix. Hook installation, rejected inputs and scheduling
 * decisions are logged in normal builds; MGSV_TAA_LOGGING=0 compiles them out.
 */

#include <cstdint>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

#include <include/reshade.hpp>

#ifndef MGSV_TAA_LOGGING
#define MGSV_TAA_LOGGING 1
#endif

namespace mgsv::reconstruction::logging {

inline constexpr const char* TAG = "MgsvTaa: ";

struct Hex {
  uint64_t value;
};

struct Bool {
  bool value;
};

inline std::ostream& operator<<(std::ostream& os, Hex hex) {
  const auto flags = os.flags();
  os << "0x" << std::hex << std::uppercase << hex.value;
  os.flags(flags);
  return os;
}

inline std::ostream& operator<<(std::ostream& os, Bool b) {
  return os << (b.value ? "true" : "false");
}

template <typename... Args>
inline std::string Format(Args&&... args) {
  std::ostringstream stream;
  stream << TAG;
  (stream << ... << std::forward<Args>(args));
  return stream.str();
}

template <typename... Args>
inline void Info(Args&&... args) {
#if MGSV_TAA_LOGGING
  reshade::log::message(reshade::log::level::info, Format(std::forward<Args>(args)...).c_str());
#endif
}

template <typename... Args>
inline void Warn(Args&&... args) {
#if MGSV_TAA_LOGGING
  reshade::log::message(reshade::log::level::warning, Format(std::forward<Args>(args)...).c_str());
#endif
}

// Rate limit for messages that could repeat every frame: true at most once per
// `interval` frames for each `last_logged` counter.
inline bool ShouldLogFrame(uint64_t frame, uint64_t& last_logged, uint64_t interval = 120u) {
  if (last_logged == std::numeric_limits<uint64_t>::max() || frame - last_logged >= interval) {
    last_logged = frame;
    return true;
  }
  return false;
}

}  // namespace mgsv::reconstruction::logging
