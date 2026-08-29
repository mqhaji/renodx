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

#include "./constant_buffers.hpp"
#include "./logging.hpp"

namespace taa::projection_jitter {

using SetViewMatrixState = void(__fastcall*)(const float* view_matrix);

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
  std::array<float, 16> current_to_previous_clip = {};
};

struct Matrix4d {
  double m[4][4] = {};
};

inline SetViewMatrixState set_view_matrix_state = nullptr;
inline uint8_t* projection_commit_address = nullptr;
inline uint8_t* helper_entry_address = nullptr;
inline uint8_t* expected_return_address = nullptr;
inline void** shader_manager_global = nullptr;
inline bool installed = false;

inline std::atomic<uint64_t> accepted_calls = 0u;
inline std::atomic<uint64_t> rejected_return_address = 0u;
inline std::atomic<uint64_t> rejected_structure = 0u;
inline std::atomic<uint64_t> projection_mismatches = 0u;
inline std::atomic<uint64_t> production_jitter_writes = 0u;
inline std::atomic<uint64_t> production_jitter_rejects = 0u;
inline std::atomic<uint32_t> production_restoration_hits = 0u;
inline std::atomic<uint32_t> production_restoration_mismatches = 0u;
inline std::atomic<bool> production_awaiting_restoration = false;
inline std::atomic<bool> logged_first_accept = false;
inline std::atomic<bool> logged_first_production_jitter = false;
inline std::atomic<uint32_t> hook_calls_in_flight = 0u;

inline std::atomic<uint64_t> published_sequence = 0u;
inline std::atomic<bool> published_valid = false;
inline std::atomic<uint64_t> published_frame_token = 0u;
inline std::atomic<uint32_t> published_sample_index = 0u;
inline std::atomic<uint32_t> published_width = 0u;
inline std::atomic<uint32_t> published_height = 0u;
inline std::atomic<float> published_jitter_uv_x = 0.f;
inline std::atomic<float> published_jitter_uv_y = 0.f;
inline std::atomic<bool> published_camera_matrix_valid = false;
inline std::atomic<bool> published_camera_reprojection_valid = false;
inline std::array<std::atomic<float>, 16> published_current_to_previous_clip = {};
inline std::atomic_flag publication_write_lock = ATOMIC_FLAG_INIT;

// Protected by publication_write_lock. The hook stages the current no-jitter
// matrix here, while only a successful TAA dispatch promotes it to previous.
inline Matrix4d staged_current_view_projection = {};
inline Matrix4d committed_previous_view_projection = {};
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
    for (double& value : augmented[col]) value /= pivot_value;

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

inline void InvalidateAppliedJitterLocked() {
  published_sequence.fetch_add(1u, std::memory_order_acq_rel);
  published_valid.store(false, std::memory_order_relaxed);
  published_camera_matrix_valid.store(false, std::memory_order_relaxed);
  published_camera_reprojection_valid.store(false, std::memory_order_relaxed);
  published_sequence.fetch_add(1u, std::memory_order_release);
}

inline void InvalidateAppliedJitter() {
  LockPublicationWriter();
  InvalidateAppliedJitterLocked();
  UnlockPublicationWriter();
}

inline void PublishAppliedJitterLocked(const AppliedJitter& jitter) {
  published_sequence.fetch_add(1u, std::memory_order_acq_rel);
  published_frame_token.store(jitter.frame_token, std::memory_order_relaxed);
  published_sample_index.store(jitter.sample_index, std::memory_order_relaxed);
  published_width.store(jitter.width, std::memory_order_relaxed);
  published_height.store(jitter.height, std::memory_order_relaxed);
  published_jitter_uv_x.store(jitter.jitter_uv_x, std::memory_order_relaxed);
  published_jitter_uv_y.store(jitter.jitter_uv_y, std::memory_order_relaxed);
  published_camera_matrix_valid.store(jitter.camera_matrix_valid, std::memory_order_relaxed);
  published_camera_reprojection_valid.store(jitter.camera_reprojection_valid, std::memory_order_relaxed);
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
  for (;;) {
    const uint64_t before = published_sequence.load(std::memory_order_acquire);
    if ((before & 1u) != 0u) continue;

    result.frame_token = published_frame_token.load(std::memory_order_relaxed);
    result.sample_index = published_sample_index.load(std::memory_order_relaxed);
    result.width = published_width.load(std::memory_order_relaxed);
    result.height = published_height.load(std::memory_order_relaxed);
    result.jitter_uv_x = published_jitter_uv_x.load(std::memory_order_relaxed);
    result.jitter_uv_y = published_jitter_uv_y.load(std::memory_order_relaxed);
    result.camera_matrix_valid = published_camera_matrix_valid.load(std::memory_order_relaxed);
    result.camera_reprojection_valid = published_camera_reprojection_valid.load(std::memory_order_relaxed);
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
  LockPublicationWriter();
  ResetMatrixHistoryLocked();
  UnlockPublicationWriter();
}

inline bool CommitCameraMatrix(uint64_t frame_token, uint32_t sample_index) {
  LockPublicationWriter();
  const bool matches = published_valid.load(std::memory_order_relaxed)
                       && published_camera_matrix_valid.load(std::memory_order_relaxed)
                       && published_frame_token.load(std::memory_order_relaxed) == frame_token
                       && published_sample_index.load(std::memory_order_relaxed) == sample_index
                       && staged_current_view_projection_valid;
  if (matches) {
    committed_previous_view_projection = staged_current_view_projection;
    committed_previous_view_projection_valid = true;
  }
  UnlockPublicationWriter();
  return matches;
}

inline bool IsInstalled() {
  return installed;
}

inline void BeginProductionRestorationCheck() {
  production_restoration_hits.store(0u, std::memory_order_relaxed);
  production_restoration_mismatches.store(0u, std::memory_order_relaxed);
  production_awaiting_restoration.store(true, std::memory_order_release);
  logging::Info("awaiting exact production projection restoration");
}

inline bool CancelProductionRestorationCheck() {
  return production_awaiting_restoration.exchange(false, std::memory_order_acq_rel);
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

inline void __fastcall HookSetViewMatrixState(const float* view_matrix) {
  struct HookCallGuard {
    HookCallGuard() { hook_calls_in_flight.fetch_add(1u, std::memory_order_acq_rel); }
    ~HookCallGuard() { hook_calls_in_flight.fetch_sub(1u, std::memory_order_acq_rel); }
  } hook_call_guard;

  void* return_address = _ReturnAddress();
  const auto original = set_view_matrix_state;
  if (original == nullptr) return;
  original(view_matrix);

  if (return_address != expected_return_address) {
    rejected_return_address.fetch_add(1u, std::memory_order_relaxed);
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
    rejected_structure.fetch_add(1u, std::memory_order_relaxed);
    return;
  }

  auto* active_projection = reinterpret_cast<float*>(
      static_cast<uint8_t*>(shader_manager) + 0x680u);
  const bool projection_matches = std::memcmp(projection, active_projection, 16u * sizeof(float)) == 0;
  if (!projection_matches) {
    projection_mismatches.fetch_add(1u, std::memory_order_relaxed);
  }

  accepted_calls.fetch_add(1u, std::memory_order_relaxed);

  const bool taa_enabled = constant_buffers::IsEnabled();
  if (!taa_enabled
      && production_awaiting_restoration.load(std::memory_order_acquire)) {
    if (projection_matches) {
      const uint32_t hit = production_restoration_hits.fetch_add(1u, std::memory_order_relaxed) + 1u;
      if (hit >= REQUIRED_RESTORATION_HITS
          && production_awaiting_restoration.exchange(false, std::memory_order_acq_rel)) {
        logging::Info("production projection restored exactly restoration_hits=", hit,
                      " restoration_mismatches=",
                      production_restoration_mismatches.load(std::memory_order_relaxed));
      }
    } else {
      production_restoration_hits.store(0u, std::memory_order_relaxed);
      production_restoration_mismatches.fetch_add(1u, std::memory_order_relaxed);
    }
  }

  bool production_write_committed = false;
  bool production_write_rejected = false;
  uint64_t production_frame_token = 0u;
  uint32_t production_sample_index = 0u;
  std::array<float, 2> production_jitter_uv = {0.f, 0.f};
  const Matrix4d current_view_projection = Multiply(
      LoadColumnMajorMatrix(projection),
      LoadColumnMajorMatrix(view_matrix));
  Matrix4d current_inverse_view_projection = {};
  const bool current_camera_matrix_valid = Invert(
      current_view_projection,
      current_inverse_view_projection);
  if (taa_enabled) {
    LockPublicationWriter();
    if (constant_buffers::IsEnabled()) {
      if (!projection_matches) {
        InvalidateAppliedJitterLocked();
        production_write_rejected = true;
      } else {
        production_frame_token = constant_buffers::CurrentFrameToken();
        production_sample_index = constant_buffers::CurrentSampleIndex();
        production_jitter_uv = constant_buffers::JitterForSample(production_sample_index, width, height);
        ApplyProjectionJitter(
            active_projection,
            shader_manager,
            production_jitter_uv[0],
            production_jitter_uv[1]);
        staged_current_view_projection = current_view_projection;
        staged_current_view_projection_valid = current_camera_matrix_valid;
        const bool camera_reprojection_valid = current_camera_matrix_valid
                                               && committed_previous_view_projection_valid;
        const auto current_to_previous_clip = camera_reprojection_valid
                                                  ? ToRowMajorFloatArray(Multiply(
                                                        committed_previous_view_projection,
                                                        current_inverse_view_projection))
                                                  : std::array<float, 16>{};
        PublishAppliedJitterLocked(AppliedJitter{
            .valid = true,
            .camera_matrix_valid = current_camera_matrix_valid,
            .camera_reprojection_valid = camera_reprojection_valid,
            .frame_token = production_frame_token,
            .sample_index = production_sample_index,
            .width = width,
            .height = height,
            .jitter_uv_x = production_jitter_uv[0],
            .jitter_uv_y = production_jitter_uv[1],
            .current_to_previous_clip = current_to_previous_clip,
        });
        production_write_committed = true;
      }
    }
    UnlockPublicationWriter();
  }

  if (production_write_rejected) {
    production_jitter_rejects.fetch_add(1u, std::memory_order_relaxed);
  }
  if (production_write_committed) {
    production_jitter_writes.fetch_add(1u, std::memory_order_relaxed);

    if (!logged_first_production_jitter.exchange(true, std::memory_order_relaxed)) {
      logging::Info("native projection jitter active frame=", production_frame_token,
                    " sample=", production_sample_index,
                    " pattern=", constant_buffers::GetJitterPattern(),
                    " dimensions=", width, "x", height,
                    " jitter_uv=", production_jitter_uv[0], ",", production_jitter_uv[1]);
    }
  }

  if (!logged_first_accept.exchange(true, std::memory_order_relaxed)) {
    logging::Info("native projection hook accepted viewport=",
                  logging::Hex{reinterpret_cast<uintptr_t>(viewport)},
                  " shader_manager=", logging::Hex{reinterpret_cast<uintptr_t>(shader_manager)},
                  " camera=", logging::Hex{reinterpret_cast<uintptr_t>(camera)},
                  " dimensions=", width, "x", height,
                  " flags=", logging::Hex{flags},
                  " projection_scale=", projection[0], ",", projection[5]);
  }
}

inline bool Attach() {
  if (installed) return true;

  InvalidateAppliedJitter();

  HMODULE module = GetModuleHandleW(nullptr);
  projection_commit_address = FindProjectionCommit(module);
  if (projection_commit_address == nullptr) return false;

  auto* call = projection_commit_address + RELATIVE_CALL_OFFSET;
  expected_return_address = call + 5u;
  helper_entry_address = DecodeRelativeCall(call);
  set_view_matrix_state = reinterpret_cast<SetViewMatrixState>(helper_entry_address);
  shader_manager_global = DecodeShaderManagerGlobal(projection_commit_address);
  if (set_view_matrix_state == nullptr || shader_manager_global == nullptr) {
    logging::Warn("failed to decode native projection hook addresses");
    set_view_matrix_state = nullptr;
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
  if (DetourTransactionCommit() != NO_ERROR) {
    DetourTransactionAbort();
    set_view_matrix_state = nullptr;
    return false;
  }

  installed = true;
  logging::Info("installed native projection hook aob=",
                logging::Hex{reinterpret_cast<uintptr_t>(projection_commit_address)},
                " helper=", logging::Hex{reinterpret_cast<uintptr_t>(helper_entry_address)},
                " return=", logging::Hex{reinterpret_cast<uintptr_t>(expected_return_address)},
                " shader_manager_global=", logging::Hex{reinterpret_cast<uintptr_t>(shader_manager_global)});
  return true;
}

inline void Detach(bool wait_for_hook_calls = true, bool transition_runtime = true) {
  if (!installed || set_view_matrix_state == nullptr) return;

  if (transition_runtime) {
    LockPublicationWriter();
    constant_buffers::SetEnabled(false);
    InvalidateAppliedJitterLocked();
    UnlockPublicationWriter();
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
  logging::Info("detached native projection hook accepted=",
                accepted_calls.load(std::memory_order_relaxed),
                " rejected_return=", rejected_return_address.load(std::memory_order_relaxed),
                " rejected_structure=", rejected_structure.load(std::memory_order_relaxed),
                " projection_mismatches=", projection_mismatches.load(std::memory_order_relaxed),
                " production_jitter_writes=", production_jitter_writes.load(std::memory_order_relaxed),
                " production_jitter_rejects=", production_jitter_rejects.load(std::memory_order_relaxed),
                " production_restoration_hits=", production_restoration_hits.load(std::memory_order_relaxed),
                " production_restoration_mismatches=",
                production_restoration_mismatches.load(std::memory_order_relaxed));
  if (hook_calls_in_flight.load(std::memory_order_acquire) == 0u) {
    set_view_matrix_state = nullptr;
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