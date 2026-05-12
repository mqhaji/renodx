#pragma once

/*
 * Shared state for the MGSV TAA runtime.
 *
 * Owns:
 *  - The master enable binding (driven by a settings checkbox).
 *  - Per-frame counters used to gate dispatch and advance the jitter sample.
 *  - The Hammersley-permuted 16-sample jitter sequence, in UV space.
 *
 * The compute shader receives jitter offsets via ShaderInjectData push
 * constants (mgsv/shared.h). This module is the authoritative source for
 * both the cbuffer-patching code (taa::jitter) and the compute dispatch
 * (taa::resolve).
 */

#include <array>
#include <cstdint>

namespace taa::constant_buffers {

struct FrameState {
  uint32_t taa_sample_index = 0u;
  uint64_t frame_index = 0u;
  bool taa_ran_this_frame = false;

  // Both offsets are in UV space. previous_jitter holds the offset that was used
  // to render the current "previous history" texture; the compute shader needs
  // it to compensate per-object velocity (which only includes current jitter).
  std::array<float, 2> current_jitter = {0.f, 0.f};
  std::array<float, 2> previous_jitter = {0.f, 0.f};

  // Pipeline markers used to identify which CopyRenderBuffer invocation
  // is the right TAA insertion point. DoF runs at all times in observed
  // captures and runs before motion blur, so it is the primary gate.
  // Motion-blur tile prep is a fallback gate for scenes where DoF is
  // somehow absent but MB is active. Both flags clear every frame in
  // BeginFrame.
  bool dof_fired = false;
  bool mb_tile_prep_fired = false;
};

inline float enabled = 0.f;
inline float* enabled_binding = &enabled;
inline FrameState frame_state = {};

inline bool IsEnabled() {
  return enabled_binding != nullptr && *enabled_binding > 0.f;
}

// Radical-inverse Hammersley term. Same constant as Alien Isolation.
inline float HammersleySample(uint32_t bits, uint32_t seed) {
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x00ff00ffu) << 8u) | ((bits & 0xff00ff00u) >> 8u);
  bits = ((bits & 0x0f0f0f0fu) << 4u) | ((bits & 0xf0f0f0f0u) >> 4u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xccccccccu) >> 2u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xaaaaaaaau) >> 1u);
  bits ^= seed;
  return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

// Computes the per-frame jitter offset in UV space for the current sample index.
// Returns (0, 0) when TAA already dispatched this frame so accidental re-reads
// produce no further offset.
inline std::array<float, 2> CurrentFrameJitter(uint32_t width, uint32_t height) {
  if (frame_state.taa_ran_this_frame || width == 0u || height == 0u) return {0.f, 0.f};

  const uint32_t sample = (frame_state.taa_sample_index * 7u) % 16u;
  std::array<float, 2> result = {
      (static_cast<float>(sample) + 0.5f) / 16.f,
      HammersleySample(sample, 238308531u),
  };
  result[0] = (result[0] - 0.5f) * 2.f / static_cast<float>(width);
  result[1] = (result[1] - 0.5f) * 2.f / static_cast<float>(height);
  return result;
}

inline void BeginFrame() {
  ++frame_state.frame_index;
  frame_state.taa_ran_this_frame = false;
  frame_state.dof_fired = false;
  frame_state.mb_tile_prep_fired = false;
}

// Called only after a successful compute dispatch + copy-back. Promotes the
// jitter that was actually applied this frame to "previous_jitter" so the next
// frame's compute shader can subtract it from the velocity sample.
inline void MarkTaaDispatched(std::array<float, 2> applied_jitter) {
  frame_state.taa_ran_this_frame = true;
  frame_state.previous_jitter = frame_state.current_jitter;
  frame_state.current_jitter = applied_jitter;
  ++frame_state.taa_sample_index;
}

}  // namespace taa::constant_buffers
