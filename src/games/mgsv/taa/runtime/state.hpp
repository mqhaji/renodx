#pragma once

/*
 * Shared state for the MGSV TAA runtime.
 *
 * Owns:
 *  - The master enable binding (driven by a settings checkbox).
 *  - Per-frame counters used to gate dispatch and advance the jitter sample.
 *  - Diagnostic Off and eight-sample base-(2,3) Halton jitter patterns,
 *    in UV space.
 *
 * The native projection hook and ReShade callbacks can execute on different
 * threads. Atomic frame/sample mirrors let the hook choose the same sample
 * that the resolve expects without reading mutable render-callback state.
 */

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#ifndef ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
#define ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS 0
#endif

#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS != 0 && ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS != 1
#error ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS must be 0 or 1
#endif

namespace taa::state {

enum class ReconstructionMethod : std::uint8_t {
  ANALYTICAL_TAA = 0u,
  AMD_FSR3 = 1u,
};

inline constexpr ReconstructionMethod DEFAULT_RECONSTRUCTION_METHOD = ReconstructionMethod::AMD_FSR3;

inline ReconstructionMethod NormalizeReconstructionMethod(float value) {
  // The previous selector used 1 for FSR2 and 2 for FSR3. Treat every
  // non-analytical persisted value as FSR3 so both configurations migrate.
  return value == static_cast<float>(ReconstructionMethod::ANALYTICAL_TAA)
             ? ReconstructionMethod::ANALYTICAL_TAA
             : ReconstructionMethod::AMD_FSR3;
}

enum class ProjectionJitterPath : std::uint8_t {
  VELOCITY,
  FORWARD,
  MODEL,
  ALPHA_MODEL,
  OVERLAY_MODEL,
  LOCAL_LIGHT,
  COUNT,
};

inline constexpr std::size_t PROJECTION_JITTER_PATH_COUNT = static_cast<std::size_t>(ProjectionJitterPath::COUNT);
inline constexpr float DEFAULT_DIAGNOSTIC_VIEW = 0.f;
inline constexpr float DEFAULT_VELOCITY_VISUALIZATION_RANGE = 8.f;
inline constexpr uint32_t DEFAULT_OBJECT_MOTION_MODE = 0u;
inline constexpr float DEFAULT_PROJECTION_JITTER_SCALE = 1.f;
inline constexpr uint32_t DEFAULT_JITTER_PATTERN = 1u;
inline constexpr float DEFAULT_CLIP_TIGHTNESS = 0.5f;
inline constexpr float DEFAULT_HISTORY_CLIP_STRENGTH = 1.f;
inline constexpr float DEFAULT_CURRENT_FRAME_BLEND = 0.15f;

struct FrameState {
  uint32_t sample_index = 0u;
  uint64_t frame_index = 0u;
  bool reconstruction_completed = false;
  bool full_resolution_candidate_seen = false;

  // Pipeline markers used to identify which CopyRenderBuffer invocation
  // is the right TAA insertion point. The primary DoF hash can also run on
  // lower-resolution inputs, so these later full-resolution gates remain
  // available when that candidate is skipped.
  // Motion-blur tile prep is a fallback gate for scenes where DoF is
  // somehow absent but MB is active. Both flags clear every frame in
  // BeginFrame.
  bool dof_fired = false;
  bool mb_tile_prep_fired = false;
};

inline float enabled = 0.f;
inline float* enabled_binding = &enabled;
inline float reconstruction_method = static_cast<float>(DEFAULT_RECONSTRUCTION_METHOD);
inline float jitter_pattern = static_cast<float>(DEFAULT_JITTER_PATTERN);
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
inline float diagnostic_view = DEFAULT_DIAGNOSTIC_VIEW;
inline float velocity_visualization_range = DEFAULT_VELOCITY_VISUALIZATION_RANGE;
inline float object_motion_mode = static_cast<float>(DEFAULT_OBJECT_MOTION_MODE);
inline std::array<float, PROJECTION_JITTER_PATH_COUNT> projection_jitter_scales = {
    DEFAULT_PROJECTION_JITTER_SCALE,
    DEFAULT_PROJECTION_JITTER_SCALE,
    DEFAULT_PROJECTION_JITTER_SCALE,
    DEFAULT_PROJECTION_JITTER_SCALE,
    DEFAULT_PROJECTION_JITTER_SCALE,
    DEFAULT_PROJECTION_JITTER_SCALE,
};
#endif
inline float clip_tightness = DEFAULT_CLIP_TIGHTNESS;
inline float history_clip_strength = DEFAULT_HISTORY_CLIP_STRENGTH;
inline float current_frame_blend = DEFAULT_CURRENT_FRAME_BLEND;
inline std::atomic<bool> runtime_enabled = false;
inline std::atomic<uint32_t> runtime_reconstruction_method = static_cast<std::uint32_t>(DEFAULT_RECONSTRUCTION_METHOD);
inline std::atomic<uint32_t> runtime_jitter_pattern = DEFAULT_JITTER_PATTERN;
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
inline std::atomic<float> runtime_diagnostic_view = DEFAULT_DIAGNOSTIC_VIEW;
inline std::atomic<float> runtime_velocity_visualization_range = DEFAULT_VELOCITY_VISUALIZATION_RANGE;
inline std::atomic<uint32_t> runtime_object_motion_mode = DEFAULT_OBJECT_MOTION_MODE;
inline std::array<std::atomic<float>, PROJECTION_JITTER_PATH_COUNT> runtime_projection_jitter_scales = {
    std::atomic<float>{DEFAULT_PROJECTION_JITTER_SCALE},
    std::atomic<float>{DEFAULT_PROJECTION_JITTER_SCALE},
    std::atomic<float>{DEFAULT_PROJECTION_JITTER_SCALE},
    std::atomic<float>{DEFAULT_PROJECTION_JITTER_SCALE},
    std::atomic<float>{DEFAULT_PROJECTION_JITTER_SCALE},
    std::atomic<float>{DEFAULT_PROJECTION_JITTER_SCALE},
};
#endif
inline std::atomic<float> runtime_clip_tightness = DEFAULT_CLIP_TIGHTNESS;
inline std::atomic<float> runtime_history_clip_strength = DEFAULT_HISTORY_CLIP_STRENGTH;
inline std::atomic<float> runtime_current_frame_blend = DEFAULT_CURRENT_FRAME_BLEND;
inline std::atomic<uint64_t> current_frame_token = 0u;
inline std::atomic<uint32_t> current_sample_index = 0u;
inline FrameState frame_state = {};
inline constexpr uint32_t HALTON_SEQUENCE_LENGTH = 8u;

inline void SetEnabled(bool value) {
  runtime_enabled.store(value, std::memory_order_release);
}

inline bool IsEnabled() {
  return runtime_enabled.load(std::memory_order_acquire);
}

inline void SetReconstructionMethod(float value) {
  const auto method = NormalizeReconstructionMethod(value);
  const uint32_t method_value = static_cast<uint32_t>(method);
  reconstruction_method = static_cast<float>(method_value);
  runtime_reconstruction_method.store(method_value, std::memory_order_release);
}

inline ReconstructionMethod GetReconstructionMethod() {
  return static_cast<ReconstructionMethod>(runtime_reconstruction_method.load(std::memory_order_acquire));
}

inline void SetJitterPattern(float value) {
  const uint32_t pattern = static_cast<uint32_t>(std::clamp(value, 0.f, 1.f));
  runtime_jitter_pattern.store(pattern, std::memory_order_release);
}

inline uint32_t GetJitterPattern() {
  return runtime_jitter_pattern.load(std::memory_order_acquire);
}

#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
inline void SetDiagnosticView(float value) {
  value = static_cast<float>(static_cast<uint32_t>(std::clamp(value, 0.f, 10.f)));
  diagnostic_view = value;
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
  velocity_visualization_range = value;
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

#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
inline void SetObjectMotionMode(float value) {
  const uint32_t mode = static_cast<uint32_t>(std::clamp(value, 0.f, 5.f));
  object_motion_mode = static_cast<float>(mode);
  runtime_object_motion_mode.store(mode, std::memory_order_release);
}
#endif

inline uint32_t GetObjectMotionMode() {
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
  return runtime_object_motion_mode.load(std::memory_order_acquire);
#else
  return DEFAULT_OBJECT_MOTION_MODE;
#endif
}

inline void SetResolveTuningValue(
    float& binding,
    std::atomic<float>& runtime_value,
    float value) {
  value = std::clamp(value, 0.f, 1.f);
  binding = value;
  runtime_value.store(value, std::memory_order_release);
}

inline void SetClipTightness(float value) {
  SetResolveTuningValue(clip_tightness, runtime_clip_tightness, value);
}

inline void SetHistoryClipStrength(float value) {
  SetResolveTuningValue(history_clip_strength, runtime_history_clip_strength, value);
}

inline void SetCurrentFrameBlend(float value) {
  SetResolveTuningValue(current_frame_blend, runtime_current_frame_blend, value);
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

#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
inline void SetProjectionJitterScale(ProjectionJitterPath path, float value) {
  value = std::clamp(value, -2.f, 2.f);
  const std::size_t index = static_cast<std::size_t>(path);
  projection_jitter_scales[index] = value;
  runtime_projection_jitter_scales[index].store(value, std::memory_order_release);
}
#endif

inline float GetProjectionJitterScale(ProjectionJitterPath path) {
#if ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS
  return runtime_projection_jitter_scales[static_cast<std::size_t>(path)].load(std::memory_order_acquire);
#else
  (void)path;
  return DEFAULT_PROJECTION_JITTER_SCALE;
#endif
}

inline uint64_t CurrentFrameToken() {
  return current_frame_token.load(std::memory_order_acquire);
}

inline uint32_t CurrentSampleIndex() {
  return current_sample_index.load(std::memory_order_acquire);
}

inline float HaltonSample(uint32_t index, uint32_t base) {
  const float inverse_base = 1.f / static_cast<float>(base);
  float fraction = inverse_base;
  float result = 0.f;
  while (index != 0u) {
    result += static_cast<float>(index % base) * fraction;
    index /= base;
    fraction *= inverse_base;
  }
  return result;
}

inline std::array<float, 2> JitterPixelsForSample(uint32_t sample_index) {
  switch (GetJitterPattern()) {
    case 0u:
      return {0.f, 0.f};
    default: {
      // Native-resolution TAA uses ceil(8 * 1^2) = 8 phases. Skip Halton index
      // zero because it is the unit-square origin rather than a distributed tap.
      const uint32_t halton_index = (sample_index % HALTON_SEQUENCE_LENGTH) + 1u;
      return {
          HaltonSample(halton_index, 2u) - 0.5f,
          HaltonSample(halton_index, 3u) - 0.5f,
      };
    }
  }
}

inline std::array<float, 2> JitterForSample(uint32_t sample_index, uint32_t width, uint32_t height) {
  if (width == 0u || height == 0u) return {0.f, 0.f};

  auto result = JitterPixelsForSample(sample_index);
  // Keep the shared value in UV/pixel space. Matrix patching converts this to
  // projection/NDC space by multiplying by two.
  result[0] /= static_cast<float>(width);
  result[1] /= static_cast<float>(height);
  return result;
}

inline void ResetTemporalState() {
  frame_state.sample_index = 0u;
  frame_state.reconstruction_completed = false;
  frame_state.full_resolution_candidate_seen = false;
  current_sample_index.store(0u, std::memory_order_release);
}

inline void BeginFrame() {
  ++frame_state.frame_index;
  frame_state.reconstruction_completed = false;
  frame_state.full_resolution_candidate_seen = false;
  frame_state.dof_fired = false;
  frame_state.mb_tile_prep_fired = false;
  current_frame_token.store(frame_state.frame_index, std::memory_order_release);
}

inline void MarkFullResolutionCandidate() {
  frame_state.full_resolution_candidate_seen = true;
}

inline void MarkReconstructionCompleted() {
  frame_state.reconstruction_completed = true;
}

// Called only after a successful compute dispatch and copy-back.
inline void CommitTemporalFrame() {
  MarkReconstructionCompleted();
  ++frame_state.sample_index;
  current_sample_index.store(frame_state.sample_index, std::memory_order_release);
}

}  // namespace taa::state
