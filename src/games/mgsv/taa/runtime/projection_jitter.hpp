#pragma once

/*
 * Native hook for MGSV's gameplay projection commit.
 *
 * The verified signature is a locator, not a function boundary. It identifies
 * the exact projection copy followed by the dirty flags and adjacent
 * SetViewMatrixState call. We decode that call and detour the helper entry,
 * then filter by the expected return address so the helper's other call sites
 * remain untouched.
 *
 * While TAA is enabled, the hook applies jitter only to the active copied
 * projection and publishes the exact camera state consumed by the resolve.
 * The next unmodified game copy restores vanilla state when TAA is disabled.
 */

#include <windows.h>

#include <intrin.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <detours.h>

#include "./logging.hpp"
#include "./state.hpp"

namespace taa::projection_jitter {

using SetViewMatrixState = void(__fastcall*)(const float* view_matrix);
using RenderPluginCallback = void(__fastcall*)(void* plugin, void* render, void* viewport);

inline constexpr uintptr_t VELOCITY_SET_VIEW_RETURN_RVA = 0x1C57C8u;
inline constexpr uintptr_t MODEL_SET_VIEW_RETURN_RVA = 0x1CD63Au;
inline constexpr uintptr_t ALPHA_MODEL_SET_VIEW_RETURN_RVA = 0x1CFA0Du;
inline constexpr uintptr_t OVERLAY_MODEL_SET_VIEW_RETURN_RVA = 0x208D9Cu;
inline constexpr uintptr_t FORWARD_RENDERING_RVA = 0x1CD460u;
inline constexpr uintptr_t LOCAL_LIGHT_MAIN_EXEC_RVA = 0x20AA00u;
inline constexpr uintptr_t LOCAL_LIGHT_VIEWPORT_280_COPY_RVA = 0x20B48Cu;

inline constexpr std::array<uint8_t, 16> FORWARD_RENDERING_PROLOGUE = {
    0x40,
    0x53,
    0x48,
    0x83,
    0xEC,
    0x20,
    0x8B,
    0x0D,
    0x4C,
    0x7D,
    0x81,
    0x02,
    0x49,
    0x8B,
    0xD8,
    0xE8,
};
inline constexpr std::array<uint8_t, 16> LOCAL_LIGHT_MAIN_EXEC_PROLOGUE = {
    0x48,
    0x8B,
    0xC4,
    0x55,
    0x56,
    0x57,
    0x41,
    0x54,
    0x41,
    0x55,
    0x41,
    0x56,
    0x41,
    0x57,
    0x48,
    0x8D,
};
inline constexpr std::array<uint8_t, 16> LOCAL_LIGHT_VIEWPORT_280_COPY_PATTERN = {
    0x41,
    0x0F,
    0x28,
    0x87,
    0x80,
    0x02,
    0x00,
    0x00,
    0x41,
    0x0F,
    0x29,
    0x86,
    0xA0,
    0x01,
    0x00,
    0x00,
};

inline constexpr std::array<uint8_t, 26> PROJECTION_COMMIT_PATTERN = {
    0x83,
    0x88,
    0x90,
    0x08,
    0x00,
    0x00,
    0x09,
    0x83,
    0x88,
    0x94,
    0x08,
    0x00,
    0x00,
    0x08,
    0x48,
    0x8D,
    0x8F,
    0xC0,
    0x02,
    0x00,
    0x00,
    0xE8,
    0x00,
    0x00,
    0x00,
    0x00,
};
inline constexpr size_t RELATIVE_CALL_OFFSET = 0x15u;
inline constexpr size_t SHADER_MANAGER_LOAD_BACK_OFFSET = 0x3Fu;
inline constexpr size_t PROJECTION_COPY_BACK_OFFSET = 0x38u;
inline constexpr std::array<uint8_t, 56> PROJECTION_COPY_PATTERN = {
    0x0F,
    0x28,
    0x87,
    0x80,
    0x02,
    0x00,
    0x00,
    0x0F,
    0x29,
    0x80,
    0x80,
    0x06,
    0x00,
    0x00,
    0x0F,
    0x28,
    0x8F,
    0x90,
    0x02,
    0x00,
    0x00,
    0x0F,
    0x29,
    0x88,
    0x90,
    0x06,
    0x00,
    0x00,
    0x0F,
    0x28,
    0x87,
    0xA0,
    0x02,
    0x00,
    0x00,
    0x0F,
    0x29,
    0x80,
    0xA0,
    0x06,
    0x00,
    0x00,
    0x0F,
    0x28,
    0x8F,
    0xB0,
    0x02,
    0x00,
    0x00,
    0x0F,
    0x29,
    0x88,
    0xB0,
    0x06,
    0x00,
    0x00,
};
inline constexpr uint32_t REQUIRED_RESTORATION_HITS = 3u;

struct AppliedJitter {
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
  double m[4][4] = {};
};

inline SetViewMatrixState set_view_matrix_state = nullptr;
inline RenderPluginCallback forward_rendering = nullptr;
inline RenderPluginCallback local_light_main_exec = nullptr;
inline uint8_t* expected_return_address = nullptr;
inline void* velocity_return_address = nullptr;
inline void* model_return_address = nullptr;
inline void* alpha_model_return_address = nullptr;
inline void* overlay_model_return_address = nullptr;
inline void** shader_manager_global = nullptr;
inline bool installed = false;

inline std::atomic<uint32_t> production_restoration_hits = 0u;
inline std::atomic<bool> production_awaiting_restoration = false;
inline std::atomic<uint32_t> hook_calls_in_flight = 0u;
inline std::atomic_flag local_light_projection_lock = ATOMIC_FLAG_INIT;
inline thread_local bool local_light_projection_active = false;

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

// Protected by publication_write_lock. The hook stages the current no-jitter
// matrix here, while only a successful TAA dispatch promotes it to previous.
inline Matrix4d staged_current_view_projection = {};
inline Matrix4d committed_previous_view_projection = {};
inline std::array<float, 2> committed_previous_jitter_uv = {0.f, 0.f};
inline bool staged_current_view_projection_valid = false;
inline bool committed_previous_view_projection_valid = false;

inline Matrix4d LoadColumnMajorMatrix(const float* values) {
  Matrix4d result = {};
  if (values == nullptr) return result;
  for (uint32_t col = 0u; col < 4u; ++col) {
    for (uint32_t row = 0u; row < 4u; ++row) {
      result.m[row][col] = static_cast<double>(values[(col * 4u) + row]);
    }
  }
  return result;
}

inline Matrix4d Multiply(const Matrix4d& a, const Matrix4d& b) {
  Matrix4d result = {};
  for (uint32_t row = 0u; row < 4u; ++row) {
    for (uint32_t col = 0u; col < 4u; ++col) {
      for (uint32_t k = 0u; k < 4u; ++k) {
        result.m[row][col] += a.m[row][k] * b.m[k][col];
      }
    }
  }
  return result;
}

inline bool Invert(const Matrix4d& input, Matrix4d& output) {
  double augmented[4][8] = {};
  for (uint32_t row = 0u; row < 4u; ++row) {
    for (uint32_t col = 0u; col < 4u; ++col) {
      augmented[row][col] = input.m[row][col];
    }
    augmented[row][4u + row] = 1.0;
  }

  for (uint32_t col = 0u; col < 4u; ++col) {
    uint32_t pivot = col;
    double best = std::abs(augmented[col][col]);
    for (uint32_t row = col + 1u; row < 4u; ++row) {
      const double candidate = std::abs(augmented[row][col]);
      if (candidate > best) {
        best = candidate;
        pivot = row;
      }
    }
    if (best <= 1e-12) return false;
    if (pivot != col) {
      for (uint32_t index = 0u; index < 8u; ++index) {
        const double value = augmented[pivot][index];
        augmented[pivot][index] = augmented[col][index];
        augmented[col][index] = value;
      }
    }

    const double pivot_value = augmented[col][col];
    for (double& value : augmented[col]) {
      value /= pivot_value;
    }

    for (uint32_t row = 0u; row < 4u; ++row) {
      if (row == col) continue;
      const double scale = augmented[row][col];
      for (uint32_t index = 0u; index < 8u; ++index) {
        augmented[row][index] -= scale * augmented[col][index];
      }
    }
  }

  for (uint32_t row = 0u; row < 4u; ++row) {
    for (uint32_t col = 0u; col < 4u; ++col) {
      output.m[row][col] = augmented[row][4u + col];
    }
  }
  return true;
}

inline std::array<float, 16> ToRowMajorFloatArray(const Matrix4d& matrix) {
  std::array<float, 16> result = {};
  for (uint32_t row = 0u; row < 4u; ++row) {
    for (uint32_t col = 0u; col < 4u; ++col) {
      result[(row * 4u) + col] = static_cast<float>(matrix.m[row][col]);
    }
  }
  return result;
}

inline void ResetMatrixHistoryLocked() {
  committed_previous_view_projection = {};
  committed_previous_jitter_uv = {0.f, 0.f};
  committed_previous_view_projection_valid = false;
}

inline void LockPublicationWriter() {
  while (publication_write_lock.test_and_set(std::memory_order_acquire)) {
    _mm_pause();
  }
}

inline void UnlockPublicationWriter() {
  publication_write_lock.clear(std::memory_order_release);
}

// Publication fields and matrix history form one logical snapshot. Writers
// must update them together; readers remain lock-free through published_sequence.
struct PublicationWriterGuard {
  PublicationWriterGuard() { LockPublicationWriter(); }
  PublicationWriterGuard(const PublicationWriterGuard&) = delete;
  PublicationWriterGuard& operator=(const PublicationWriterGuard&) = delete;
  PublicationWriterGuard(PublicationWriterGuard&&) = delete;
  PublicationWriterGuard& operator=(PublicationWriterGuard&&) = delete;
  ~PublicationWriterGuard() { UnlockPublicationWriter(); }
};

inline void InvalidateAppliedJitterLocked() {
  published_sequence.fetch_add(1u, std::memory_order_acq_rel);
  published_valid.store(false, std::memory_order_relaxed);
  published_camera_matrix_valid.store(false, std::memory_order_relaxed);
  published_camera_reprojection_valid.store(false, std::memory_order_relaxed);
  published_sequence.fetch_add(1u, std::memory_order_release);
}

inline void InvalidateAppliedJitter() {
  PublicationWriterGuard guard;
  InvalidateAppliedJitterLocked();
}

inline void PublishAppliedJitterLocked(const AppliedJitter& jitter) {
  published_sequence.fetch_add(1u, std::memory_order_acq_rel);
  published_frame_token.store(jitter.frame_token, std::memory_order_relaxed);
  published_sample_index.store(jitter.sample_index, std::memory_order_relaxed);
  published_width.store(jitter.width, std::memory_order_relaxed);
  published_height.store(jitter.height, std::memory_order_relaxed);
  published_jitter_uv_x.store(jitter.jitter_uv_x, std::memory_order_relaxed);
  published_jitter_uv_y.store(jitter.jitter_uv_y, std::memory_order_relaxed);
  published_previous_jitter_uv_x.store(jitter.previous_jitter_uv_x, std::memory_order_relaxed);
  published_previous_jitter_uv_y.store(jitter.previous_jitter_uv_y, std::memory_order_relaxed);
  published_camera_matrix_valid.store(jitter.camera_matrix_valid, std::memory_order_relaxed);
  published_camera_reprojection_valid.store(jitter.camera_reprojection_valid, std::memory_order_relaxed);
  for (uint32_t index = 0u; index < jitter.device_to_view_depth.size(); ++index) {
    published_device_to_view_depth[index].store(
        jitter.device_to_view_depth[index],
        std::memory_order_relaxed);
  }
  for (uint32_t index = 0u; index < jitter.current_to_previous_clip.size(); ++index) {
    published_current_to_previous_clip[index].store(
        jitter.current_to_previous_clip[index],
        std::memory_order_relaxed);
  }
  published_valid.store(jitter.valid, std::memory_order_relaxed);
  published_sequence.fetch_add(1u, std::memory_order_release);
}

inline AppliedJitter GetAppliedJitter() {
  AppliedJitter result = {};
  // Retry if a writer was active or changed the snapshot while it was read.
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
      result.device_to_view_depth[index] =
          published_device_to_view_depth[index].load(std::memory_order_relaxed);
    }
    for (uint32_t index = 0u; index < result.current_to_previous_clip.size(); ++index) {
      result.current_to_previous_clip[index] =
          published_current_to_previous_clip[index].load(std::memory_order_relaxed);
    }
    result.valid = published_valid.load(std::memory_order_relaxed);

    const uint64_t after = published_sequence.load(std::memory_order_acquire);
    if (before == after) break;
  }
  return result;
}

inline void ResetMatrixHistory() {
  PublicationWriterGuard guard;
  ResetMatrixHistoryLocked();
}

inline bool CommitCameraMatrix(uint64_t frame_token, uint32_t sample_index) {
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

inline bool IsInstalled() {
  return installed;
}

inline void BeginProductionRestorationCheck() {
  production_restoration_hits.store(0u, std::memory_order_relaxed);
  production_awaiting_restoration.store(true, std::memory_order_release);
  logging::Info("awaiting exact production projection restoration");
}

inline void CancelProductionRestorationCheck() {
  production_awaiting_restoration.store(false, std::memory_order_release);
}

inline bool MatchesProjectionCommit(const uint8_t* candidate) {
  const auto* shader_manager_load = candidate - SHADER_MANAGER_LOAD_BACK_OFFSET;
  if (shader_manager_load[0] != 0x48u
      || shader_manager_load[1] != 0x8Bu
      || shader_manager_load[2] != 0x05u) {
    return false;
  }
  if (std::memcmp(
          candidate - PROJECTION_COPY_BACK_OFFSET,
          PROJECTION_COPY_PATTERN.data(),
          PROJECTION_COPY_PATTERN.size())
      != 0) {
    return false;
  }
  for (size_t index = 0u; index < PROJECTION_COMMIT_PATTERN.size(); ++index) {
    if (index > RELATIVE_CALL_OFFSET && index < RELATIVE_CALL_OFFSET + 5u) continue;
    if (candidate[index] != PROJECTION_COMMIT_PATTERN[index]) return false;
  }
  return true;
}

inline uint8_t* FindProjectionCommit(HMODULE module) {
  if (module == nullptr) return nullptr;

  auto* base = reinterpret_cast<uint8_t*>(module);
  const auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;

  const auto* nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos_header->e_lfanew);
  if (nt_headers->Signature != IMAGE_NT_SIGNATURE) return nullptr;

  const auto* section = IMAGE_FIRST_SECTION(nt_headers);
  uint8_t* match = nullptr;
  uint32_t match_count = 0u;
  for (uint16_t index = 0u; index < nt_headers->FileHeader.NumberOfSections; ++index, ++section) {
    const bool executable_code = (section->Characteristics & IMAGE_SCN_CNT_CODE) != 0u
                                 && (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0u;
    if (!executable_code || section->Misc.VirtualSize < PROJECTION_COMMIT_PATTERN.size()) continue;

    auto* section_begin = base + section->VirtualAddress;
    const size_t section_size = section->Misc.VirtualSize;
    for (size_t offset = SHADER_MANAGER_LOAD_BACK_OFFSET;
         offset <= section_size - PROJECTION_COMMIT_PATTERN.size();
         ++offset) {
      if (!MatchesProjectionCommit(section_begin + offset)) continue;
      match = section_begin + offset;
      if (++match_count > 1u) break;
    }
    if (match_count > 1u) break;
  }

  if (match_count != 1u) {
    logging::Warn("projection commit AOB expected one match, found ", match_count);
    return nullptr;
  }
  return match;
}

inline uint8_t* DecodeRelativeCall(uint8_t* instruction) {
  if (instruction == nullptr || instruction[0] != 0xE8u) return nullptr;
  int32_t displacement = 0;
  std::memcpy(&displacement, instruction + 1u, sizeof(displacement));
  return instruction + 5u + displacement;
}

inline void** DecodeShaderManagerGlobal(uint8_t* commit) {
  if (commit == nullptr) return nullptr;
  auto* load = commit - SHADER_MANAGER_LOAD_BACK_OFFSET;
  if (load[0] != 0x48u || load[1] != 0x8Bu || load[2] != 0x05u) return nullptr;

  int32_t displacement = 0;
  std::memcpy(&displacement, load + 3u, sizeof(displacement));
  return reinterpret_cast<void**>(load + 7u + displacement);
}

template <size_t Size>
inline bool MatchesBytes(const uint8_t* address, const std::array<uint8_t, Size>& expected) {
  return address != nullptr && std::memcmp(address, expected.data(), expected.size()) == 0;
}

inline bool InitializeAdditionalHookAddresses(HMODULE module, const uint8_t* helper_entry_address) {
  if (module == nullptr || helper_entry_address == nullptr) return false;
  auto* base = reinterpret_cast<uint8_t*>(module);

  auto* velocity_return = base + VELOCITY_SET_VIEW_RETURN_RVA;
  auto* model_return = base + MODEL_SET_VIEW_RETURN_RVA;
  auto* alpha_return = base + ALPHA_MODEL_SET_VIEW_RETURN_RVA;
  auto* overlay_return = base + OVERLAY_MODEL_SET_VIEW_RETURN_RVA;
  if (DecodeRelativeCall(velocity_return - 5u) != helper_entry_address
      || DecodeRelativeCall(model_return - 5u) != helper_entry_address
      || DecodeRelativeCall(alpha_return - 5u) != helper_entry_address
      || DecodeRelativeCall(overlay_return - 5u) != helper_entry_address
      || !MatchesBytes(base + FORWARD_RENDERING_RVA, FORWARD_RENDERING_PROLOGUE)
      || !MatchesBytes(base + LOCAL_LIGHT_MAIN_EXEC_RVA, LOCAL_LIGHT_MAIN_EXEC_PROLOGUE)
      || !MatchesBytes(base + LOCAL_LIGHT_VIEWPORT_280_COPY_RVA, LOCAL_LIGHT_VIEWPORT_280_COPY_PATTERN)) {
    logging::Warn("known additional projection jitter paths failed executable validation");
    return false;
  }

  velocity_return_address = velocity_return;
  model_return_address = model_return;
  alpha_model_return_address = alpha_return;
  overlay_model_return_address = overlay_return;
  forward_rendering = reinterpret_cast<RenderPluginCallback>(base + FORWARD_RENDERING_RVA);
  local_light_main_exec = reinterpret_cast<RenderPluginCallback>(base + LOCAL_LIGHT_MAIN_EXEC_RVA);
  return true;
}

inline bool LooksLikeGameplayProjection(
    const float* projection,
    uint32_t width,
    uint32_t height,
    uint8_t flags,
    const void* camera) {
  if (projection == nullptr || width < 640u || height < 360u || (flags & 1u) == 0u || camera == nullptr) return false;
  return std::isfinite(projection[0])
         && std::isfinite(projection[5])
         && std::abs(projection[0]) > 1.f
         && std::abs(projection[5]) > 1.f
         && std::abs(projection[11] - 1.f) < 0.0001f
         && std::abs(projection[15]) < 0.0001f
         && projection[10] < 0.f
         && projection[14] > 0.f;
}

inline void ApplyProjectionJitter(
    float* active_projection,
    void* shader_manager,
    float jitter_uv_x,
    float jitter_uv_y) {
  active_projection[8] += 2.f * jitter_uv_x;
  active_projection[9] -= 2.f * jitter_uv_y;
  *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(shader_manager) + 0x890u) |= 0x09u;
  *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(shader_manager) + 0x894u) |= 0x08u;
}

inline void ApplyProjectionJitter(float* projection, float jitter_uv_x, float jitter_uv_y) {
  projection[8] += 2.f * jitter_uv_x;
  projection[9] -= 2.f * jitter_uv_y;
}

struct HookCallGuard {
  HookCallGuard() { hook_calls_in_flight.fetch_add(1u, std::memory_order_acq_rel); }
  HookCallGuard(const HookCallGuard&) = delete;
  HookCallGuard& operator=(const HookCallGuard&) = delete;
  HookCallGuard(HookCallGuard&&) = delete;
  HookCallGuard& operator=(HookCallGuard&&) = delete;
  ~HookCallGuard() { hook_calls_in_flight.fetch_sub(1u, std::memory_order_acq_rel); }
};

inline bool GetPublishedJitterForViewport(
    const uint8_t* viewport,
    state::ProjectionJitterPath path,
    std::array<float, 2>& jitter_uv) {
  if (!state::IsEnabled() || viewport == nullptr) return false;

  const float scale = state::GetProjectionJitterScale(path);
  if (scale == 0.f) return false;

  const auto* projection = reinterpret_cast<const float*>(viewport + 0x280u);
  const uint32_t width = *reinterpret_cast<const uint32_t*>(viewport + 0x5D8u);
  const uint32_t height = *reinterpret_cast<const uint32_t*>(viewport + 0x5DCu);
  const uint8_t flags = *(viewport + 0x6E2u);
  void* camera = *reinterpret_cast<void* const*>(viewport + 0x570u);
  const AppliedJitter published = GetAppliedJitter();
  if (!LooksLikeGameplayProjection(projection, width, height, flags, camera)
      || !published.valid
      || published.frame_token != state::CurrentFrameToken()
      || published.width != width
      || published.height != height) {
    return false;
  }

  jitter_uv = {
      published.jitter_uv_x * scale,
      published.jitter_uv_y * scale,
  };
  return true;
}

inline bool ApplyPublishedJitterToActiveProjection(
    const uint8_t* viewport,
    state::ProjectionJitterPath jitter_path) {
  std::array<float, 2> jitter_uv = {};
  void* shader_manager = shader_manager_global != nullptr ? *shader_manager_global : nullptr;
  if (shader_manager == nullptr || !GetPublishedJitterForViewport(viewport, jitter_path, jitter_uv)) return false;

  const auto* projection = reinterpret_cast<const float*>(viewport + 0x280u);
  auto* active_projection = reinterpret_cast<float*>(static_cast<uint8_t*>(shader_manager) + 0x680u);
  if (std::memcmp(projection, active_projection, 16u * sizeof(float)) != 0) return false;

  ApplyProjectionJitter(active_projection, shader_manager, jitter_uv[0], jitter_uv[1]);
  return true;
}

inline void __fastcall HookSetViewMatrixState(const float* view_matrix) {
  HookCallGuard hook_call_guard;

  void* return_address = _ReturnAddress();
  const auto original = set_view_matrix_state;
  if (original == nullptr) return;
  original(view_matrix);

  if (return_address != expected_return_address) {
    state::ProjectionJitterPath path;
    if (return_address == velocity_return_address) {
      path = state::ProjectionJitterPath::VELOCITY;
    } else if (return_address == model_return_address) {
      path = state::ProjectionJitterPath::MODEL;
    } else if (return_address == alpha_model_return_address) {
      path = state::ProjectionJitterPath::ALPHA_MODEL;
    } else if (return_address == overlay_model_return_address) {
      path = state::ProjectionJitterPath::OVERLAY_MODEL;
    } else {
      return;
    }
    const auto* viewport = reinterpret_cast<const uint8_t*>(view_matrix) - 0x2C0u;
    ApplyPublishedJitterToActiveProjection(viewport, path);
    return;
  }

  const bool taa_enabled = state::IsEnabled();
  // The hook stays installed for the process lifetime, but default-Off work
  // stops here unless a short exact-restoration check is still pending.
  if (!taa_enabled && !production_awaiting_restoration.load(std::memory_order_acquire)) {
    return;
  }

  const auto* viewport = reinterpret_cast<const uint8_t*>(view_matrix) - 0x2C0u;
  const auto* projection = reinterpret_cast<const float*>(viewport + 0x280u);
  const uint32_t width = *reinterpret_cast<const uint32_t*>(viewport + 0x5D8u);
  const uint32_t height = *reinterpret_cast<const uint32_t*>(viewport + 0x5DCu);
  const uint8_t flags = *(viewport + 0x6E2u);
  void* camera = *reinterpret_cast<void* const*>(viewport + 0x570u);
  void* shader_manager = shader_manager_global != nullptr ? *shader_manager_global : nullptr;

  if (!LooksLikeGameplayProjection(projection, width, height, flags, camera) || shader_manager == nullptr) {
    return;
  }

  auto* active_projection = reinterpret_cast<float*>(
      static_cast<uint8_t*>(shader_manager) + 0x680u);
  const bool projection_matches = std::memcmp(projection, active_projection, 16u * sizeof(float)) == 0;

  // Three consecutive vanilla copies prove that disabling TAA restored the
  // active projection before the verification flag is cleared.
  if (!taa_enabled
      && production_awaiting_restoration.load(std::memory_order_acquire)) {
    if (projection_matches) {
      const uint32_t hit = production_restoration_hits.fetch_add(1u, std::memory_order_relaxed) + 1u;
      if (hit >= REQUIRED_RESTORATION_HITS
          && production_awaiting_restoration.exchange(false, std::memory_order_acq_rel)) {
        logging::Info("production projection restored exactly restoration_hits=", hit);
      }
    } else {
      production_restoration_hits.store(0u, std::memory_order_relaxed);
    }
  }
  if (!taa_enabled) return;

  const Matrix4d current_view_projection = Multiply(
      LoadColumnMajorMatrix(projection),
      LoadColumnMajorMatrix(view_matrix));
  Matrix4d current_inverse_view_projection = {};
  const bool current_camera_matrix_valid = Invert(
      current_view_projection,
      current_inverse_view_projection);
  PublicationWriterGuard publication_guard;
  if (!state::IsEnabled()) return;
  if (!projection_matches) {
    InvalidateAppliedJitterLocked();
    return;
  }

  const uint64_t frame_token = state::CurrentFrameToken();
  const uint32_t sample_index = state::CurrentSampleIndex();
  const auto jitter_uv = state::JitterForSample(sample_index, width, height);
  ApplyProjectionJitter(active_projection, shader_manager, jitter_uv[0], jitter_uv[1]);
  staged_current_view_projection = current_view_projection;
  staged_current_view_projection_valid = current_camera_matrix_valid;
  const bool camera_reprojection_valid = current_camera_matrix_valid
                                         && committed_previous_view_projection_valid;
  const auto current_to_previous_clip = camera_reprojection_valid
                                            ? ToRowMajorFloatArray(Multiply(
                                                  committed_previous_view_projection,
                                                  current_inverse_view_projection))
                                            : std::array<float, 16>{};
  const float projection_w_scale = projection[11];
  const std::array<float, 4> device_to_view_depth = {
      projection[10] / projection_w_scale,
      projection[14] / projection_w_scale,
      projection_w_scale / projection[0],
      projection_w_scale / projection[5],
  };
  PublishAppliedJitterLocked(AppliedJitter{
      .valid = true,
      .camera_matrix_valid = current_camera_matrix_valid,
      .camera_reprojection_valid = camera_reprojection_valid,
      .frame_token = frame_token,
      .sample_index = sample_index,
      .width = width,
      .height = height,
      .jitter_uv_x = jitter_uv[0],
      .jitter_uv_y = jitter_uv[1],
      .previous_jitter_uv_x = committed_previous_jitter_uv[0],
      .previous_jitter_uv_y = committed_previous_jitter_uv[1],
      .device_to_view_depth = device_to_view_depth,
      .current_to_previous_clip = current_to_previous_clip,
  });
}

inline void __fastcall HookForwardRendering(void* plugin, void* render, void* viewport_pointer) {
  HookCallGuard hook_call_guard;
  const auto original = forward_rendering;
  if (original == nullptr) return;
  original(plugin, render, viewport_pointer);

  ApplyPublishedJitterToActiveProjection(
      static_cast<const uint8_t*>(viewport_pointer),
      state::ProjectionJitterPath::FORWARD);
}

inline void __fastcall HookLocalLightMainExec(void* plugin, void* render, void* viewport_pointer) {
  HookCallGuard hook_call_guard;
  const auto original = local_light_main_exec;
  if (original == nullptr) return;

  auto* viewport = static_cast<uint8_t*>(viewport_pointer);
  std::array<float, 2> jitter_uv = {};
  if (local_light_projection_active
      || !GetPublishedJitterForViewport(viewport, state::ProjectionJitterPath::LOCAL_LIGHT, jitter_uv)) {
    original(plugin, render, viewport_pointer);
    return;
  }

  while (local_light_projection_lock.test_and_set(std::memory_order_acquire)) {
    _mm_pause();
  }
  local_light_projection_active = true;
  auto* projection = reinterpret_cast<float*>(viewport + 0x280u);
  {
    struct ProjectionRestoreGuard {
      float* projection;
      std::array<float, 16> original;

      explicit ProjectionRestoreGuard(float* target) : projection(target) {
        std::memcpy(original.data(), projection, sizeof(original));
      }

      ProjectionRestoreGuard(const ProjectionRestoreGuard&) = delete;
      ProjectionRestoreGuard& operator=(const ProjectionRestoreGuard&) = delete;
      ProjectionRestoreGuard(ProjectionRestoreGuard&&) = delete;
      ProjectionRestoreGuard& operator=(ProjectionRestoreGuard&&) = delete;

      ~ProjectionRestoreGuard() {
        std::memcpy(projection, original.data(), sizeof(original));
        local_light_projection_active = false;
        local_light_projection_lock.clear(std::memory_order_release);
      }
    } restore_guard(projection);
    ApplyProjectionJitter(projection, jitter_uv[0], jitter_uv[1]);
    original(plugin, render, viewport_pointer);
  }
}

inline bool Attach() {
  if (installed) return true;

  InvalidateAppliedJitter();

  HMODULE module = GetModuleHandleW(nullptr);
  auto* projection_commit_address = FindProjectionCommit(module);
  if (projection_commit_address == nullptr) return false;

  auto* call = projection_commit_address + RELATIVE_CALL_OFFSET;
  expected_return_address = call + 5u;
  auto* helper_entry_address = DecodeRelativeCall(call);
  set_view_matrix_state = reinterpret_cast<SetViewMatrixState>(helper_entry_address);
  shader_manager_global = DecodeShaderManagerGlobal(projection_commit_address);
  if (set_view_matrix_state == nullptr
      || shader_manager_global == nullptr
      || !InitializeAdditionalHookAddresses(module, helper_entry_address)) {
    logging::Warn("failed to decode native projection hook addresses");
    set_view_matrix_state = nullptr;
    forward_rendering = nullptr;
    local_light_main_exec = nullptr;
    return false;
  }

  if (DetourTransactionBegin() != NO_ERROR) return false;
  if (DetourUpdateThread(GetCurrentThread()) != NO_ERROR) {
    DetourTransactionAbort();
    return false;
  }
  if (DetourAttach(
          reinterpret_cast<void**>(&set_view_matrix_state),
          reinterpret_cast<void*>(HookSetViewMatrixState))
      != NO_ERROR) {
    DetourTransactionAbort();
    return false;
  }
  if (DetourAttach(
          reinterpret_cast<void**>(&forward_rendering),
          reinterpret_cast<void*>(HookForwardRendering))
      != NO_ERROR) {
    DetourTransactionAbort();
    return false;
  }
  if (DetourAttach(
          reinterpret_cast<void**>(&local_light_main_exec),
          reinterpret_cast<void*>(HookLocalLightMainExec))
      != NO_ERROR) {
    DetourTransactionAbort();
    return false;
  }
  if (DetourTransactionCommit() != NO_ERROR) {
    DetourTransactionAbort();
    set_view_matrix_state = nullptr;
    forward_rendering = nullptr;
    local_light_main_exec = nullptr;
    return false;
  }

  installed = true;
  logging::Info("installed native projection hook aob=",
                logging::Hex{reinterpret_cast<uintptr_t>(projection_commit_address)},
                " helper=", logging::Hex{reinterpret_cast<uintptr_t>(helper_entry_address)},
                " return=", logging::Hex{reinterpret_cast<uintptr_t>(expected_return_address)},
                " forward=", logging::Hex{reinterpret_cast<uintptr_t>(forward_rendering)},
                " local_light=", logging::Hex{reinterpret_cast<uintptr_t>(local_light_main_exec)},
                " shader_manager_global=", logging::Hex{reinterpret_cast<uintptr_t>(shader_manager_global)});
  return true;
}

inline void Detach(bool wait_for_hook_calls = true, bool transition_runtime = true) {
  if (!installed || set_view_matrix_state == nullptr) return;

  if (transition_runtime) {
    PublicationWriterGuard publication_guard;
    state::SetEnabled(false);
    InvalidateAppliedJitterLocked();
  }

  if (DetourTransactionBegin() != NO_ERROR) return;
  if (DetourUpdateThread(GetCurrentThread()) != NO_ERROR) {
    DetourTransactionAbort();
    return;
  }
  if (DetourDetach(
          reinterpret_cast<void**>(&set_view_matrix_state),
          reinterpret_cast<void*>(HookSetViewMatrixState))
      != NO_ERROR) {
    DetourTransactionAbort();
    return;
  }
  if (DetourDetach(
          reinterpret_cast<void**>(&forward_rendering),
          reinterpret_cast<void*>(HookForwardRendering))
      != NO_ERROR) {
    DetourTransactionAbort();
    return;
  }
  if (DetourDetach(
          reinterpret_cast<void**>(&local_light_main_exec),
          reinterpret_cast<void*>(HookLocalLightMainExec))
      != NO_ERROR) {
    DetourTransactionAbort();
    return;
  }
  if (DetourTransactionCommit() != NO_ERROR) {
    DetourTransactionAbort();
    return;
  }

  installed = false;
  if (wait_for_hook_calls) {
    while (hook_calls_in_flight.load(std::memory_order_acquire) != 0u) {
      _mm_pause();
    }
  }
  logging::Info("detached native projection hook");
  if (hook_calls_in_flight.load(std::memory_order_acquire) == 0u) {
    set_view_matrix_state = nullptr;
    forward_rendering = nullptr;
    local_light_main_exec = nullptr;
  }
}

inline void OnInitDevice(reshade::api::device* device) {
  (void)device;
  Attach();
}

inline void OnDestroyDevice(reshade::api::device* device) {
  (void)device;
  Detach();
}

inline void Use(DWORD reason) {
  if (reason == DLL_PROCESS_ATTACH) {
    reshade::register_event<reshade::addon_event::init_device>(OnInitDevice);
    reshade::register_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
  } else if (reason == DLL_PROCESS_DETACH) {
    reshade::unregister_event<reshade::addon_event::init_device>(OnInitDevice);
    reshade::unregister_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
    // DllMain holds the loader lock. Do not wait for another game thread here;
    // normal destroy_device teardown already performs a quiescent detach.
    Detach(false, false);
  }
}

}  // namespace taa::projection_jitter