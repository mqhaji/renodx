#pragma once

/*
 * Lightweight logging helpers for the MGSV TAA runtime.
 *
 * Mirrors the structure used by the Alien Isolation port. Default-enabled while
 * the implementation is in development so that map/unmap and dispatch decisions
 * are visible in ReShade.log without a rebuild.
 */

#include <cstdint>
#include <sstream>
#include <string>

#include <include/reshade.hpp>

#ifndef MGSV_TAA_LOGGING
#define MGSV_TAA_LOGGING 1
#endif

namespace taa::logging {

inline constexpr const char* TAG = "MgsvTaa: ";

struct Hex {
  uint64_t value;
};
struct Crc32 {
  uint32_t value;
};
struct Bool {
  bool value;
};

inline std::ostream& operator<<(std::ostream& os, Hex hex) {
  std::ios_base::fmtflags flags = os.flags();
  os << "0x" << std::hex << std::uppercase << hex.value;
  os.flags(flags);
  return os;
}

inline std::ostream& operator<<(std::ostream& os, Crc32 crc) {
  return os << Hex{crc.value};
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

inline bool ShouldLogFrame(uint64_t frame, uint64_t& last_logged, uint64_t interval = 120) {
  if (last_logged == std::numeric_limits<uint64_t>::max() || frame - last_logged >= interval) {
    last_logged = frame;
    return true;
  }
  return false;
}

}  // namespace taa::logging
