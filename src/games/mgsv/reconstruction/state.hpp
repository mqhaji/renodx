#pragma once

/*
 * Shared state for the MGSV TAA runtime.
 *
 * Owns:
 *  - The availability-validated effective temporal mode.
 *  - Presentation diagnostics and the next jitter sample to reserve.
 *  - Halton (2,3) jitter in render-space UV coordinates.
 *    The pattern starts at eight phases and grows with the upscaling ratio.
 *
 * The native projection hook and ReShade callbacks can execute on different
 * threads. The projection hook reserves a sample for each native scene.
 * Reconstruction uses that recorded sample when the scene reaches the GPU.
 */

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#ifndef ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
#define ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS 1
#endif

#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS != 0 && ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS != 1
#error ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS must be 0 or 1
#endif

namespace mgsv::reconstruction::state {

// Stable persisted values. Append future reconstruction methods instead of
// reordering these entries so presets remain forward-compatible.
enum class TemporalMode : std::uint8_t {
  OFF = 0u,
  ANALYTICAL_TAA = 1u,
  AMD_FSR = 2u,
  NVIDIA_DLSS = 3u,
};

inline constexpr TemporalMode DEFAULT_TEMPORAL_MODE = TemporalMode::AMD_FSR;

inline TemporalMode NormalizeTemporalMode(float value) {
  if (value == static_cast<float>(TemporalMode::OFF)) return TemporalMode::OFF;
  if (value == static_cast<float>(TemporalMode::ANALYTICAL_TAA)) return TemporalMode::ANALYTICAL_TAA;
  if (value == static_cast<float>(TemporalMode::NVIDIA_DLSS)) return TemporalMode::NVIDIA_DLSS;
  return TemporalMode::AMD_FSR;
}

inline constexpr float DEFAULT_DIAGNOSTIC_VIEW = 0.f;
inline constexpr float DEFAULT_VELOCITY_VISUALIZATION_RANGE = 8.f;

inline std::atomic_flag execution_lock = ATOMIC_FLAG_INIT;

inline constexpr float DEFAULT_CLIP_TIGHTNESS = 0.5f;
inline constexpr float DEFAULT_HISTORY_CLIP_STRENGTH = 1.f;
inline constexpr float DEFAULT_CURRENT_FRAME_BLEND = 0.15f;

struct FrameState {
  uint32_t sample_index = 0u;  // Successfully installed reconstruction outputs.
  uint64_t frame_index = 0u;
  uint64_t temporal_generation = 0u;
  bool reconstruction_scheduled = false;
  bool reconstruction_completed = false;
  // A scene since the last Present had every reconstruction input.
  bool complete_record_seen = false;
};

inline std::atomic<TemporalMode> runtime_temporal_mode = TemporalMode::OFF;

#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
inline std::atomic<float> runtime_diagnostic_view = DEFAULT_DIAGNOSTIC_VIEW;
inline std::atomic<float> runtime_velocity_visualization_range = DEFAULT_VELOCITY_VISUALIZATION_RANGE;

#endif
inline std::atomic<float> runtime_clip_tightness = DEFAULT_CLIP_TIGHTNESS;
inline std::atomic<float> runtime_history_clip_strength = DEFAULT_HISTORY_CLIP_STRENGTH;
inline std::atomic<float> runtime_current_frame_blend = DEFAULT_CURRENT_FRAME_BLEND;
inline std::atomic<uint64_t> current_frame_token = 0u;
// Advances during native scene recording. History commits do not change it.
inline std::atomic<uint32_t> current_sample_index = 0u;
inline FrameState frame_state = {};
inline constexpr uint32_t BASE_JITTER_PHASE_COUNT = 8u;
inline std::atomic<uint32_t> runtime_jitter_phases = BASE_JITTER_PHASE_COUNT;

inline void SetTemporalMode(TemporalMode mode) {
  runtime_temporal_mode.store(mode, std::memory_order_release);
}

inline TemporalMode GetTemporalMode() {
  return runtime_temporal_mode.load(std::memory_order_acquire);
}

inline bool IsEnabled() {
  return GetTemporalMode() != TemporalMode::OFF;
}

#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
inline void SetDiagnosticView(float value) {
  value = static_cast<float>(static_cast<uint32_t>(std::clamp(value, 0.f, 10.f)));
  runtime_diagnostic_view.store(value, std::memory_order_release);
}
#endif

inline float GetDiagnosticView() {
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
  return runtime_diagnostic_view.load(std::memory_order_acquire);
#else
  return DEFAULT_DIAGNOSTIC_VIEW;
#endif
}

#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
inline void SetVelocityVisualizationRange(float value) {
  value = value > 0.01f ? value : 0.01f;
  runtime_velocity_visualization_range.store(value, std::memory_order_release);
}
#endif

inline float GetVelocityVisualizationRange() {
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
  return runtime_velocity_visualization_range.load(std::memory_order_acquire);
#else
  return DEFAULT_VELOCITY_VISUALIZATION_RANGE;
#endif
}

inline void SetResolveTuningValue(
    std::atomic<float>* runtime_value,
    float value) {
  value = std::clamp(value, 0.f, 1.f);
  runtime_value->store(value, std::memory_order_release);
}

inline void SetClipTightness(float value) {
  SetResolveTuningValue(&runtime_clip_tightness, value);
}

inline void SetHistoryClipStrength(float value) {
  SetResolveTuningValue(&runtime_history_clip_strength, value);
}

inline void SetCurrentFrameBlend(float value) {
  SetResolveTuningValue(&runtime_current_frame_blend, value);
}

inline float GetClipTightness() {
  return runtime_clip_tightness.load(std::memory_order_acquire);
}

inline float GetHistoryClipStrength() {
  return runtime_history_clip_strength.load(std::memory_order_acquire);
}

inline float GetCurrentFrameBlend() {
  return runtime_current_frame_blend.load(std::memory_order_acquire);
}

inline uint64_t CurrentFrameToken() {
  return current_frame_token.load(std::memory_order_acquire);
}

inline uint32_t CurrentSampleIndex() {
  return current_sample_index.load(std::memory_order_acquire);
}

inline std::array<float, 2> JitterForSample(uint32_t sample_index, uint32_t width, uint32_t height) {
  if (width == 0u || height == 0u) return {0.f, 0.f};

  const auto halton_sample = [](uint32_t index, uint32_t base) {
    const float inverse_base = 1.f / static_cast<float>(base);
    float fraction = inverse_base;
    float result = 0.f;
    while (index != 0u) {
      result += static_cast<float>(index % base) * fraction;
      index /= base;
      fraction *= inverse_base;
    }
    return result;
  };
  // Native-resolution TAA uses ceil(8 * 1^2) = 8 phases. Skip Halton index
  // zero because it is the unit-square origin rather than a distributed tap.
  const uint32_t halton_index = (sample_index % runtime_jitter_phases.load(std::memory_order_acquire)) + 1u;
  std::array<float, 2> result = {
      halton_sample(halton_index, 2u) - 0.5f,
      halton_sample(halton_index, 3u) - 0.5f,
  };
  // Matrix patching converts UV jitter to projection space by multiplying by two.
  result[0] /= static_cast<float>(width);
  result[1] /= static_cast<float>(height);
  return result;
}

inline void ResetTemporalState() {
  ++frame_state.temporal_generation;
  frame_state.sample_index = 0u;
  frame_state.reconstruction_scheduled = false;
  frame_state.reconstruction_completed = false;
  frame_state.complete_record_seen = false;
  current_sample_index.store(0u, std::memory_order_release);
}

inline void BeginFrame() {
  ++frame_state.frame_index;
  frame_state.reconstruction_scheduled = false;
  frame_state.reconstruction_completed = false;
  frame_state.complete_record_seen = false;
  current_frame_token.store(frame_state.frame_index, std::memory_order_release);
}

// Called only after a successful compute dispatch and copy-back.
inline void CommitTemporalFrame() {
  frame_state.reconstruction_scheduled = false;
  frame_state.reconstruction_completed = true;
  ++frame_state.sample_index;
}

}  // namespace mgsv::reconstruction::state
