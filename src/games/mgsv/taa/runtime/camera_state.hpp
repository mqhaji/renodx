#pragma once

/* Coherent camera publication and temporal matrix history shared by all methods. */

#include <intrin.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace taa::camera_state {

struct CameraFrame {
  bool valid = false;
  bool camera_matrix_valid = false;
  bool camera_reprojection_valid = false;
  uint64_t frame_token = 0u;
  uint32_t sample_index = 0u;
  uint32_t width = 0u;
  uint32_t height = 0u;
  float jitter_uv_x = 0.f;
  float jitter_uv_y = 0.f;
  float previous_jitter_uv_x = 0.f;
  float previous_jitter_uv_y = 0.f;
  std::array<float, 4> device_to_view_depth = {};
  std::array<float, 16> current_to_previous_clip = {};
};

struct Matrix4d {
  std::array<std::array<double, 4>, 4> m = {};
};

inline std::atomic<uint64_t> published_sequence = 0u;
inline std::atomic<bool> published_valid = false;
inline std::atomic<uint64_t> published_frame_token = 0u;
inline std::atomic<uint32_t> published_sample_index = 0u;
inline std::atomic<uint32_t> published_width = 0u;
inline std::atomic<uint32_t> published_height = 0u;
inline std::atomic<float> published_jitter_uv_x = 0.f;
inline std::atomic<float> published_jitter_uv_y = 0.f;
inline std::atomic<float> published_previous_jitter_uv_x = 0.f;
inline std::atomic<float> published_previous_jitter_uv_y = 0.f;
inline std::atomic<bool> published_camera_matrix_valid = false;
inline std::atomic<bool> published_camera_reprojection_valid = false;
inline std::array<std::atomic<float>, 4> published_device_to_view_depth = {};
inline std::array<std::atomic<float>, 16> published_current_to_previous_clip = {};
inline std::atomic_flag publication_write_lock = ATOMIC_FLAG_INIT;

// Protected by publication_write_lock. The native hook stages the current
// no-jitter matrix; only a successful reconstruction promotes it to previous.
inline Matrix4d staged_current_view_projection = {};
inline Matrix4d committed_previous_view_projection = {};
inline std::array<float, 2> committed_previous_jitter_uv = {0.f, 0.f};
inline bool staged_current_view_projection_valid = false;
inline bool committed_previous_view_projection_valid = false;

inline Matrix4d LoadColumnMajorMatrix(const float* values) {
  Matrix4d result = {};
  if (values == nullptr) return result;
  for (uint32_t column = 0u; column < 4u; ++column) {
    for (uint32_t row = 0u; row < 4u; ++row) {
      result.m[row][column] = static_cast<double>(values[(column * 4u) + row]);
    }
  }
  return result;
}

inline Matrix4d Multiply(const Matrix4d& left, const Matrix4d& right) {
  Matrix4d result = {};
  for (uint32_t row = 0u; row < 4u; ++row) {
    for (uint32_t column = 0u; column < 4u; ++column) {
      for (uint32_t index = 0u; index < 4u; ++index) {
        result.m[row][column] += left.m[row][index] * right.m[index][column];
      }
    }
  }
  return result;
}

inline bool Invert(const Matrix4d& input, Matrix4d& output) {
  std::array<std::array<double, 8>, 4> augmented = {};
  for (uint32_t row = 0u; row < 4u; ++row) {
    for (uint32_t column = 0u; column < 4u; ++column) {
      augmented[row][column] = input.m[row][column];
    }
    augmented[row][4u + row] = 1.0;
  }

  for (uint32_t column = 0u; column < 4u; ++column) {
    uint32_t pivot = column;
    double best = std::abs(augmented[column][column]);
    for (uint32_t row = column + 1u; row < 4u; ++row) {
      const double candidate = std::abs(augmented[row][column]);
      if (candidate > best) {
        best = candidate;
        pivot = row;
      }
    }
    if (best <= 1e-12) return false;
    if (pivot != column) {
      for (uint32_t index = 0u; index < 8u; ++index) {
        const double value = augmented[pivot][index];
        augmented[pivot][index] = augmented[column][index];
        augmented[column][index] = value;
      }
    }

    const double pivot_value = augmented[column][column];
    for (double& value : augmented[column]) {
      value /= pivot_value;
    }

    for (uint32_t row = 0u; row < 4u; ++row) {
      if (row == column) continue;
      const double scale = augmented[row][column];
      for (uint32_t index = 0u; index < 8u; ++index) {
        augmented[row][index] -= scale * augmented[column][index];
      }
    }
  }

  for (uint32_t row = 0u; row < 4u; ++row) {
    for (uint32_t column = 0u; column < 4u; ++column) {
      output.m[row][column] = augmented[row][4u + column];
    }
  }
  return true;
}

inline std::array<float, 16> ToRowMajorFloatArray(const Matrix4d& matrix) {
  std::array<float, 16> result = {};
  for (uint32_t row = 0u; row < 4u; ++row) {
    for (uint32_t column = 0u; column < 4u; ++column) {
      result[(row * 4u) + column] = static_cast<float>(matrix.m[row][column]);
    }
  }
  return result;
}

inline void ResetMatrixHistoryLocked() {
  committed_previous_view_projection = {};
  committed_previous_jitter_uv = {0.f, 0.f};
  committed_previous_view_projection_valid = false;
}

struct PublicationWriterGuard {
  PublicationWriterGuard() {
    while (publication_write_lock.test_and_set(std::memory_order_acquire)) {
      _mm_pause();
    }
  }
  PublicationWriterGuard(const PublicationWriterGuard&) = delete;
  PublicationWriterGuard& operator=(const PublicationWriterGuard&) = delete;
  PublicationWriterGuard(PublicationWriterGuard&&) = delete;
  PublicationWriterGuard& operator=(PublicationWriterGuard&&) = delete;
  ~PublicationWriterGuard() { publication_write_lock.clear(std::memory_order_release); }
};

inline void InvalidateLocked() {
  published_sequence.fetch_add(1u, std::memory_order_acq_rel);
  published_valid.store(false, std::memory_order_relaxed);
  published_camera_matrix_valid.store(false, std::memory_order_relaxed);
  published_camera_reprojection_valid.store(false, std::memory_order_relaxed);
  published_sequence.fetch_add(1u, std::memory_order_release);
}

inline void Invalidate() {
  PublicationWriterGuard guard;
  InvalidateLocked();
}

inline void PublishLocked(const CameraFrame& frame) {
  published_sequence.fetch_add(1u, std::memory_order_acq_rel);
  published_frame_token.store(frame.frame_token, std::memory_order_relaxed);
  published_sample_index.store(frame.sample_index, std::memory_order_relaxed);
  published_width.store(frame.width, std::memory_order_relaxed);
  published_height.store(frame.height, std::memory_order_relaxed);
  published_jitter_uv_x.store(frame.jitter_uv_x, std::memory_order_relaxed);
  published_jitter_uv_y.store(frame.jitter_uv_y, std::memory_order_relaxed);
  published_previous_jitter_uv_x.store(frame.previous_jitter_uv_x, std::memory_order_relaxed);
  published_previous_jitter_uv_y.store(frame.previous_jitter_uv_y, std::memory_order_relaxed);
  published_camera_matrix_valid.store(frame.camera_matrix_valid, std::memory_order_relaxed);
  published_camera_reprojection_valid.store(frame.camera_reprojection_valid, std::memory_order_relaxed);
  for (uint32_t index = 0u; index < frame.device_to_view_depth.size(); ++index) {
    published_device_to_view_depth[index].store(frame.device_to_view_depth[index], std::memory_order_relaxed);
  }
  for (uint32_t index = 0u; index < frame.current_to_previous_clip.size(); ++index) {
    published_current_to_previous_clip[index].store(frame.current_to_previous_clip[index], std::memory_order_relaxed);
  }
  published_valid.store(frame.valid, std::memory_order_relaxed);
  published_sequence.fetch_add(1u, std::memory_order_release);
}

inline CameraFrame Get() {
  CameraFrame result = {};
  for (;;) {
    const uint64_t before = published_sequence.load(std::memory_order_acquire);
    if ((before & 1u) != 0u) continue;

    result.frame_token = published_frame_token.load(std::memory_order_relaxed);
    result.sample_index = published_sample_index.load(std::memory_order_relaxed);
    result.width = published_width.load(std::memory_order_relaxed);
    result.height = published_height.load(std::memory_order_relaxed);
    result.jitter_uv_x = published_jitter_uv_x.load(std::memory_order_relaxed);
    result.jitter_uv_y = published_jitter_uv_y.load(std::memory_order_relaxed);
    result.previous_jitter_uv_x = published_previous_jitter_uv_x.load(std::memory_order_relaxed);
    result.previous_jitter_uv_y = published_previous_jitter_uv_y.load(std::memory_order_relaxed);
    result.camera_matrix_valid = published_camera_matrix_valid.load(std::memory_order_relaxed);
    result.camera_reprojection_valid = published_camera_reprojection_valid.load(std::memory_order_relaxed);
    for (uint32_t index = 0u; index < result.device_to_view_depth.size(); ++index) {
      result.device_to_view_depth[index] = published_device_to_view_depth[index].load(std::memory_order_relaxed);
    }
    for (uint32_t index = 0u; index < result.current_to_previous_clip.size(); ++index) {
      result.current_to_previous_clip[index] = published_current_to_previous_clip[index].load(std::memory_order_relaxed);
    }
    result.valid = published_valid.load(std::memory_order_relaxed);

    const uint64_t after = published_sequence.load(std::memory_order_acquire);
    if (before == after) break;
  }
  return result;
}

inline bool Commit(uint64_t frame_token, uint32_t sample_index) {
  PublicationWriterGuard guard;
  const bool matches = published_valid.load(std::memory_order_relaxed)
                       && published_camera_matrix_valid.load(std::memory_order_relaxed)
                       && published_frame_token.load(std::memory_order_relaxed) == frame_token
                       && published_sample_index.load(std::memory_order_relaxed) == sample_index
                       && staged_current_view_projection_valid;
  if (matches) {
    committed_previous_view_projection = staged_current_view_projection;
    committed_previous_jitter_uv = {
        published_jitter_uv_x.load(std::memory_order_relaxed),
        published_jitter_uv_y.load(std::memory_order_relaxed),
    };
    committed_previous_view_projection_valid = true;
  }
  return matches;
}

}  // namespace taa::camera_state
