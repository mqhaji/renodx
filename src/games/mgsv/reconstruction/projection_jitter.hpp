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
#include <mutex>
#include <shared_mutex>

#include <detours.h>

#include "camera_state.hpp"
#include "logging.hpp"
#include "resolution.hpp"
#include "state.hpp"

namespace mgsv::reconstruction::projection_jitter {

using SetViewMatrixState = void(__fastcall*)(const float* view_matrix);
using RenderPluginCallback = void(__fastcall*)(void* plugin, void* render, void* viewport);
using RequestLocalLightDraws = void(__fastcall*)(
    void* renderer, void* packet, void* visible_lights, void* shadow_ready,
    void* input_context, void* output_context, const void* viewport, void* jobs);

inline constexpr uintptr_t VELOCITY_SET_VIEW_RETURN_RVA = 0x1C57C8u;
inline constexpr uintptr_t MODEL_SET_VIEW_RETURN_RVA = 0x1CD63Au;
inline constexpr uintptr_t ALPHA_MODEL_SET_VIEW_RETURN_RVA = 0x1CFA0Du;
inline constexpr uintptr_t OVERLAY_MODEL_SET_VIEW_RETURN_RVA = 0x208D9Cu;
// "NewThermography" is the game's name for its mode-2 branch.
inline constexpr uintptr_t NEW_THERMOGRAPHY_SET_VIEW_RETURN_RVA = 0x214531u;
inline constexpr uintptr_t THERMOGRAPHY_SET_VIEW_RETURN_RVA = 0x214AC3u;
inline constexpr uintptr_t FORWARD_RENDERING_RVA = 0x1CD460u;
inline constexpr uintptr_t LOCAL_LIGHT_REQUEST_RVA = 0x2A43E0u;
inline constexpr uintptr_t LOCAL_LIGHT_REQUEST_RETURN_RVA = 0x20B8A3u;
inline constexpr uintptr_t LOCAL_LIGHT_REQUEST_VTABLE_RVA = 0x211E8D8u;
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
inline constexpr std::array<uint8_t, 16> LOCAL_LIGHT_REQUEST_PROLOGUE = {
    0x40,
    0x57,
    0x41,
    0x56,
    0x41,
    0x57,
    0x48,
    0x83,
    0xEC,
    0x40,
    0x48,
    0xC7,
    0x44,
    0x24,
    0x30,
    0xFE,
};
// MainExec passes its completed packet in RDX and viewport as argument seven.
inline constexpr std::array<uint8_t, 38> LOCAL_LIGHT_REQUEST_CALL_PATTERN = {
    0x48,
    0x89,
    0x5C,
    0x24,
    0x38,
    0x4C,
    0x89,
    0x7C,
    0x24,
    0x30,
    0x48,
    0x89,
    0x44,
    0x24,
    0x28,
    0x48,
    0x89,
    0x54,
    0x24,
    0x20,
    0x4C,
    0x8B,
    0x4C,
    0x24,
    0x78,
    0x4C,
    0x8B,
    0x45,
    0x88,
    0x48,
    0x8B,
    0x54,
    0x24,
    0x58,
    0x41,
    0xFF,
    0x52,
    0x48,
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

inline SetViewMatrixState set_view_matrix_state = nullptr;
inline RenderPluginCallback forward_rendering = nullptr;
inline RequestLocalLightDraws request_local_light_draws = nullptr;
inline const void* local_light_request_return_address = nullptr;
inline uint8_t* expected_return_address = nullptr;
inline void* velocity_return_address = nullptr;
inline void* model_return_address = nullptr;
inline void* alpha_model_return_address = nullptr;
inline void* overlay_model_return_address = nullptr;
inline void* new_thermography_return_address = nullptr;
inline void* thermography_return_address = nullptr;
inline void** shader_manager_global = nullptr;
inline std::atomic<bool> installed = false;
// Only lifecycle callbacks take this mutex; native hooks must never need it
// to drain. Own installation before changing any target/trampoline pointer.
inline std::shared_mutex lifecycle_mutex;

inline std::atomic<uint32_t> production_restoration_hits = 0u;
inline std::atomic<bool> production_awaiting_restoration = false;

inline bool IsInstalled() {
  return installed.load(std::memory_order_acquire);
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
  auto* new_thermography_return = base + NEW_THERMOGRAPHY_SET_VIEW_RETURN_RVA;
  auto* thermography_return = base + THERMOGRAPHY_SET_VIEW_RETURN_RVA;
  if (DecodeRelativeCall(velocity_return - 5u) != helper_entry_address
      || DecodeRelativeCall(model_return - 5u) != helper_entry_address
      || DecodeRelativeCall(alpha_return - 5u) != helper_entry_address
      || DecodeRelativeCall(overlay_return - 5u) != helper_entry_address
      || DecodeRelativeCall(new_thermography_return - 5u) != helper_entry_address
      || DecodeRelativeCall(thermography_return - 5u) != helper_entry_address
      || !MatchesBytes(base + FORWARD_RENDERING_RVA, FORWARD_RENDERING_PROLOGUE)
      || !MatchesBytes(base + LOCAL_LIGHT_REQUEST_RVA, LOCAL_LIGHT_REQUEST_PROLOGUE)
      || !MatchesBytes(base + LOCAL_LIGHT_REQUEST_RETURN_RVA - LOCAL_LIGHT_REQUEST_CALL_PATTERN.size(), LOCAL_LIGHT_REQUEST_CALL_PATTERN)
      || *reinterpret_cast<const uintptr_t*>(base + LOCAL_LIGHT_REQUEST_VTABLE_RVA) != reinterpret_cast<uintptr_t>(base + LOCAL_LIGHT_REQUEST_RVA)
      || !MatchesBytes(base + LOCAL_LIGHT_VIEWPORT_280_COPY_RVA, LOCAL_LIGHT_VIEWPORT_280_COPY_PATTERN)) {
    logging::Warn("known additional projection jitter paths failed executable validation");
    return false;
  }

  velocity_return_address = velocity_return;
  model_return_address = model_return;
  alpha_model_return_address = alpha_return;
  overlay_model_return_address = overlay_return;
  new_thermography_return_address = new_thermography_return;
  thermography_return_address = thermography_return;
  forward_rendering = reinterpret_cast<RenderPluginCallback>(base + FORWARD_RENDERING_RVA);
  request_local_light_draws = reinterpret_cast<RequestLocalLightDraws>(base + LOCAL_LIGHT_REQUEST_RVA);
  local_light_request_return_address = base + LOCAL_LIGHT_REQUEST_RETURN_RVA;
  return true;
}

inline bool LooksLikeGameplayProjection(
    const float* projection,
    uint32_t width,
    uint32_t height,
    uint8_t flags,
    const void* camera) {
  if (projection == nullptr || (flags & 1u) == 0u || camera == nullptr) return false;
  if (width < 640u || height < 360u) {
    const std::shared_lock lock(resolution::sizing_state.mutex);
    if (!resolution::sizing_state.requested.IsScaled() || resolution::sizing_state.actual != resolution::Size{.width = width, .height = height}) return false;
  }
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

// Hold the publication lock through validation and the resulting matrix write.
inline bool GetPublishedJitterForViewportLocked(
    const uint8_t* viewport,
    state::ProjectionJitterPath path,
    std::array<float, 2>* jitter_uv) {
  if (!state::IsEnabled() || viewport == nullptr) return false;

  const float scale = state::GetProjectionJitterScale(path);
  if (scale == 0.f) return false;

  const auto* projection = reinterpret_cast<const float*>(viewport + 0x280u);
  const uint32_t width = *reinterpret_cast<const uint32_t*>(viewport + 0x5D8u);
  const uint32_t height = *reinterpret_cast<const uint32_t*>(viewport + 0x5DCu);
  const uint8_t flags = *(viewport + 0x6E2u);
  const void* camera = *reinterpret_cast<void* const*>(viewport + 0x570u);
  const camera_state::CameraFrame published = camera_state::GetForSceneLocked();
  if (!LooksLikeGameplayProjection(projection, width, height, flags, camera)) {
    return false;
  }
  if (!published.valid) {
    return false;
  }
  if (published.width != width || published.height != height) {
    return false;
  }

  const auto owner = rendering::native_context::GetScope().owner;
  if (published.scene == 0u || published.scene != owner.scene
      || published.owner_generation != owner.generation
      || published.reset_generation != camera_state::reset_generation.load(std::memory_order_relaxed)) return false;

  if (!published.camera_matrix_valid
      || published.viewport_identity != reinterpret_cast<uintptr_t>(viewport)
      || published.camera_identity != reinterpret_cast<uintptr_t>(camera)
      || std::memcmp(projection, published.projection.data(), sizeof(published.projection)) != 0
      || std::memcmp(viewport + 0x2C0u, published.view.data(), sizeof(published.view)) != 0) {
    return false;
  }

  *jitter_uv = {
      published.jitter_uv_x * scale,
      published.jitter_uv_y * scale,
  };
  return true;
}

inline void ApplyPublishedJitterToActiveProjection(
    const uint8_t* viewport,
    state::ProjectionJitterPath jitter_path) {
  // Native callbacks may already hold the execution gate, so do not take it
  // here. Keep camera validation and the write under one publication lock so
  // reset, camera replacement, or sample advancement cannot split them.
  const camera_state::PublicationWriterGuard publication_guard;
  std::array<float, 2> jitter_uv = {};
  void* shader_manager = shader_manager_global != nullptr ? *shader_manager_global : nullptr;
  if (shader_manager == nullptr) {
    return;
  }
  auto* active_projection = reinterpret_cast<float*>(static_cast<uint8_t*>(shader_manager) + 0x680u);
  if (!GetPublishedJitterForViewportLocked(viewport, jitter_path, &jitter_uv)) return;

  const auto* projection = reinterpret_cast<const float*>(viewport + 0x280u);
  if (std::memcmp(projection, active_projection, 16u * sizeof(float)) != 0) {
    return;
  }

  ApplyProjectionJitter(active_projection, shader_manager, jitter_uv[0], jitter_uv[1]);
}

inline void __fastcall HookSetViewMatrixState(const float* view_matrix) {
  const void* return_address = _ReturnAddress();
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
    } else if (return_address == new_thermography_return_address
               || return_address == thermography_return_address) {
      path = state::ProjectionJitterPath::THERMOGRAPHY;
    } else {
      return;
    }
    const auto* viewport = reinterpret_cast<const uint8_t*>(view_matrix) - 0x2C0u;
    ApplyPublishedJitterToActiveProjection(viewport, path);
    return;
  }

  const bool taa_enabled = state::IsEnabled();
  // The hook stays installed for the process lifetime, but Off-mode work stops
  // here unless a short exact-restoration check is still pending.
  if (!taa_enabled && !production_awaiting_restoration.load(std::memory_order_acquire)) {
    return;
  }

  const auto* viewport = reinterpret_cast<const uint8_t*>(view_matrix) - 0x2C0u;
  const auto* projection = reinterpret_cast<const float*>(viewport + 0x280u);
  const uint32_t width = *reinterpret_cast<const uint32_t*>(viewport + 0x5D8u);
  const uint32_t height = *reinterpret_cast<const uint32_t*>(viewport + 0x5DCu);
  const uint8_t flags = *(viewport + 0x6E2u);
  const void* camera = *reinterpret_cast<void* const*>(viewport + 0x570u);
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

  const auto owner = rendering::native_context::GetScope().owner;
  if (owner.scene == 0u || !owner.main_view || !owner.active_member || !owner.presentation_matches
      || owner.viewport != reinterpret_cast<uintptr_t>(viewport)
      || owner.camera != reinterpret_cast<uintptr_t>(camera)) return;

  const camera_state::Matrix4d current_view_projection = camera_state::Multiply(
      camera_state::LoadColumnMajorMatrix(projection),
      camera_state::LoadColumnMajorMatrix(view_matrix));
  camera_state::Matrix4d current_inverse_view_projection = {};
  const bool current_camera_matrix_valid = camera_state::Invert(
      current_view_projection,
      current_inverse_view_projection);
  const camera_state::PublicationWriterGuard publication_guard;
  if (!state::IsEnabled()) return;
  if (!projection_matches) {
    camera_state::InvalidateLocked();
    return;
  }

  const uint64_t frame_token = state::CurrentFrameToken();
  const auto recorded = camera_state::GetForSceneLocked();
  if (recorded.valid) {
    // A second projection setup for this scene must use its original sample.
    // Another worker may already have reserved the next scene by this point.
    if (recorded.reset_generation == camera_state::reset_generation.load(std::memory_order_relaxed)
        && recorded.width == width && recorded.height == height
        && std::memcmp(projection, recorded.projection.data(), 64u) == 0
        && std::memcmp(view_matrix, recorded.view.data(), 64u) == 0) {
      ApplyProjectionJitter(active_projection, shader_manager, recorded.jitter_uv_x, recorded.jitter_uv_y);
    }
    return;
  }
  const uint32_t sample_index = state::current_sample_index.fetch_add(1u, std::memory_order_relaxed);
  const uint64_t reservation = ++camera_state::next_reservation;
  const auto jitter_uv = state::JitterForSample(sample_index, width, height);
  ApplyProjectionJitter(active_projection, shader_manager, jitter_uv[0], jitter_uv[1]);
  camera_state::staged_current_view_projection = current_view_projection;
  camera_state::staged_viewport = viewport;
  camera_state::staged_camera = camera;
  std::memcpy(camera_state::staged_projection.data(), projection, 64u);
  std::memcpy(camera_state::staged_view.data(), view_matrix, 64u);
  camera_state::staged_current_view_projection_valid = current_camera_matrix_valid;
  const bool camera_reprojection_valid = current_camera_matrix_valid
                                         && camera_state::committed_previous_view_projection_valid;
  const auto current_to_previous_clip = camera_reprojection_valid
                                            ? camera_state::ToRowMajorFloatArray(camera_state::Multiply(
                                                  camera_state::committed_previous_view_projection,
                                                  current_inverse_view_projection))
                                            : std::array<float, 16>{};
  const float projection_w_scale = projection[11];
  const std::array<float, 4> device_to_view_depth = {
      projection[10] / projection_w_scale,
      projection[14] / projection_w_scale,
      projection_w_scale / projection[0],
      projection_w_scale / projection[5],
  };
  camera_state::PublishLocked(camera_state::CameraFrame{
      .valid = true,
      .camera_matrix_valid = current_camera_matrix_valid,
      .camera_reprojection_valid = camera_reprojection_valid,
      .frame_token = frame_token,
      .sample_index = sample_index,
      .width = width,
      .height = height,
      .jitter_uv_x = jitter_uv[0],
      .jitter_uv_y = jitter_uv[1],
      .previous_jitter_uv_x = camera_state::committed_previous_jitter_uv[0],
      .previous_jitter_uv_y = camera_state::committed_previous_jitter_uv[1],
      .device_to_view_depth = device_to_view_depth,
      .current_to_previous_clip = current_to_previous_clip,
      .reservation = reservation,
      .inverse_view_projection = current_inverse_view_projection,
      .projection = camera_state::staged_projection,
      .view = camera_state::staged_view,
  });
}

inline void __fastcall HookForwardRendering(void* plugin, void* render, void* viewport_pointer) {
  const auto original = forward_rendering;
  if (original == nullptr) return;
  original(plugin, render, viewport_pointer);

  ApplyPublishedJitterToActiveProjection(
      static_cast<const uint8_t*>(viewport_pointer),
      state::ProjectionJitterPath::FORWARD);
}

inline void __fastcall HookRequestLocalLightDraws(
    void* renderer, void* packet_pointer, void* visible_lights, void* shadow_ready,
    void* input_context, void* output_context, const void* viewport_pointer, void* jobs) {
  const auto original = request_local_light_draws;
  if (original == nullptr) return;

  // MainExec has finished this packet but has not published it to workers.
  // Only this verified caller supplies the viewport as argument seven.
  if (_ReturnAddress() == local_light_request_return_address
      && packet_pointer != nullptr && viewport_pointer != nullptr && state::IsEnabled()) {
    const camera_state::PublicationWriterGuard publication_guard;
    const auto* viewport = static_cast<const uint8_t*>(viewport_pointer);
    auto* packet = static_cast<uint8_t*>(packet_pointer);
    std::array<float, 2> jitter_uv = {};
    // Match the copied P/V and viewport rectangle as well as the scene's camera.
    // A packet already carrying jitter fails this check, preventing a second offset.
    if (GetPublishedJitterForViewportLocked(viewport, state::ProjectionJitterPath::LOCAL_LIGHT, &jitter_uv)
        && std::memcmp(packet + 0x1A0u, viewport + 0x280u, 2u * 16u * sizeof(float)) == 0
        && std::memcmp(packet + 0x370u, viewport + 0x5D0u, 4u * sizeof(uint32_t)) == 0) {
      // Workers retain or copy this render P. The separate culling P/V and
      // frustum planes stay intact, and the shared viewport remains read-only.
      ApplyProjectionJitter(reinterpret_cast<float*>(packet + 0x1A0u), jitter_uv[0], jitter_uv[1]);
    }
  }

  // Release the publication lock before the game copies packets and queues jobs.
  original(renderer, packet_pointer, visible_lights, shadow_ready,
           input_context, output_context, viewport_pointer, jobs);
}

#if ENABLE_TAA_PROJECTION_JITTER_DIAGNOSTICS
using SunPass = void(__fastcall*)(void*, void*, uint32_t, uint32_t, uint8_t, uint8_t);
using SunVolume = void(__fastcall*)(void*, const float*, const void*, const void*, void*, void*);
using ShFallback = uint32_t(__fastcall*)(void*, uint32_t, float*, float*, float*, void*, void*);
using ShProducer = void(__fastcall*)(void*, void*, void*, void*);
using SkyParameters = void*(__fastcall*)(void*, uint32_t*, void*, void*);
using TerrainProducer = void(__fastcall*)(void*, void*, void*, void*, uint64_t);
using AttachPacket = void(__fastcall*)(void*, void*, uint64_t, uint64_t);
using AlternateProjection = void(__fastcall*)(void*);
using AlternateCommand = void*(__fastcall*)(void*, void*);
using StageMatrix = void(__fastcall*)(void*, uint32_t, const float*);
inline SunPass sun_pass = nullptr;
inline SunVolume sun_volume = nullptr;
inline ShFallback sh_fallback = nullptr;
inline ShProducer sh_producer = nullptr;
inline AttachPacket sh_attach = nullptr;
inline SkyParameters sky_parameters = nullptr;
inline TerrainProducer terrain_producer = nullptr;
inline AttachPacket terrain_attach = nullptr;
inline AlternateProjection alternate_projection = nullptr;
inline AlternateCommand alternate_command = nullptr;
inline StageMatrix stage_matrix = nullptr;
inline uint8_t* native_base = nullptr;
inline constexpr std::array<uint32_t, 4> SH_VERTEX_COUNTS = {8u, 6u, 18u, 8u};

// The scope constructor links to this thread-local head of its own type.
struct NativeProjectionScope;
inline thread_local const NativeProjectionScope* native_scope = nullptr;

// Own every matrix/sample value; only the TLS link is borrowed, and only by
// synchronous children. Even a rejected nested parent shadows its predecessor.
struct NativeProjectionScope {
  const NativeProjectionScope* previous = native_scope;
  state::ProjectionJitterPath path;
  camera_state::CameraFrame frame = {};
  alignas(16) std::array<float, 16> projection = {};
  alignas(16) std::array<float, 16> view = {};
  std::array<float, 2> jitter = {};
  bool valid = false;

  NativeProjectionScope(state::ProjectionJitterPath selected, const void* source, bool alternate = false)
      : path(selected) {
    native_scope = this;
    if (source == nullptr || state::GetProjectionJitterScale(path) == 0.f) return;
    // Scene camera data has its own lock. GPU reconstruction can run while
    // workers prepare these matrices without making them skip jitter.
    const camera_state::PublicationWriterGuard publication_guard;
    frame = camera_state::GetForSceneLocked();
    if (!frame.valid || !frame.camera_matrix_valid
        || !state::IsEnabled()
        || frame.scene == 0u
        || frame.reset_generation != camera_state::reset_generation.load(std::memory_order_relaxed)) return;
    const auto* bytes = static_cast<const uint8_t*>(source);
    const auto* p = reinterpret_cast<const float*>(bytes + (alternate ? 0x1C0u : 0x280u));
    const auto* v = reinterpret_cast<const float*>(bytes + (alternate ? 0x200u : 0x2C0u));
    const void* camera = *reinterpret_cast<void* const*>(bytes + (alternate ? 0x308u : 0x570u));
    if (camera == nullptr || reinterpret_cast<uintptr_t>(camera) != frame.camera_identity) return;
    uint32_t width = 0u;
    uint32_t height = 0u;
    if (alternate) {
      const auto* descriptor = *reinterpret_cast<const uint8_t* const*>(bytes + 0x2F0u);
      if (descriptor == nullptr) return;
      width = *reinterpret_cast<const uint32_t*>(descriptor + 0x48u);
      height = *reinterpret_cast<const uint32_t*>(descriptor + 0x4Cu);
    } else {
      if (reinterpret_cast<uintptr_t>(source) != frame.viewport_identity || (bytes[0x6E2u] & 1u) == 0u) return;
      width = *reinterpret_cast<const uint32_t*>(bytes + 0x5D8u);
      height = *reinterpret_cast<const uint32_t*>(bytes + 0x5DCu);
    }
    if (width != frame.width || height != frame.height
        || !LooksLikeGameplayProjection(p, width, height, 1u, camera)
        || std::memcmp(p, frame.projection.data(), 64u) != 0
        || std::memcmp(v, frame.view.data(), 64u) != 0) {
      return;
    }
    for (uint32_t i = 0u; i < 16u; ++i) {
      if (!std::isfinite(p[i]) || !std::isfinite(v[i])) return;
    }
    std::memcpy(projection.data(), p, 64u);
    std::memcpy(view.data(), v, 64u);
    const float scale = state::GetProjectionJitterScale(path);
    jitter = {frame.jitter_uv_x * scale, frame.jitter_uv_y * scale};
    valid = std::isfinite(jitter[0]) && std::isfinite(jitter[1]) && (jitter[0] != 0.f || jitter[1] != 0.f);
  }
  NativeProjectionScope(const NativeProjectionScope&) = delete;
  NativeProjectionScope& operator=(const NativeProjectionScope&) = delete;
  ~NativeProjectionScope() { native_scope = previous; }

  [[nodiscard]] bool CurrentLocked() const {
    const auto owner = rendering::native_context::GetScope().owner;
    return valid && state::IsEnabled()
           && state::GetProjectionJitterScale(path) != 0.f
           && frame.scene == owner.scene && frame.owner_generation == owner.generation
           && frame.reset_generation == camera_state::reset_generation.load(std::memory_order_relaxed);
  }
};

// Shared clip translation: native column-major matrices are four float4s;
// SH outputs are up to eighteen float4s. Validate ALL results before any write.
inline bool TranslateClip(float* values, uint32_t count, const std::array<float, 2>& jitter) {
  if (values == nullptr || count > 18u) return false;
  std::array<float, 72> corrected = {};
  for (uint32_t i = 0u; i < count * 4u; ++i) {
    if (!std::isfinite(values[i])) return false;
    corrected[i] = values[i];
  }
  for (uint32_t i = 0u; i < count * 4u; i += 4u) {
    corrected[i] += 2.f * jitter[0] * values[i + 3u];
    corrected[i + 1u] -= 2.f * jitter[1] * values[i + 3u];
    if (!std::isfinite(corrected[i]) || !std::isfinite(corrected[i + 1u])) return false;
  }
  for (uint32_t i = 0u; i < count * 4u; i += 4u) {
    values[i] = corrected[i];
    values[i + 1u] = corrected[i + 1u];
  }
  return true;
}

inline bool CorrectNativeMatrix(float* matrix, const NativeProjectionScope& scope, bool projection_only = false) {
  if (!scope.valid || matrix == nullptr) return false;
  const camera_state::PublicationWriterGuard publication_guard;
  if (!scope.CurrentLocked()) return false;
  alignas(16) std::array<float, 16> expected = scope.projection;
  if (!projection_only) {
    // Match native SSE's pairwise sum, without a world-coordinate-relative
    // epsilon that could swallow an entire subpixel offset.
    for (uint32_t c = 0u; c < 4u; ++c) {
      for (uint32_t r = 0u; r < 4u; ++r) {
        expected[(c * 4u) + r] =
            ((scope.projection[r] * scope.view[c * 4u])
             + (scope.projection[4u + r] * scope.view[(c * 4u) + 1u]))
            + ((scope.projection[8u + r] * scope.view[(c * 4u) + 2u])
               + (scope.projection[12u + r] * scope.view[(c * 4u) + 3u]));
      }
    }
  }
  auto jittered = expected;
  if (!TranslateClip(jittered.data(), 4u, {scope.frame.jitter_uv_x, scope.frame.jitter_uv_y})) return false;
  float canonical_error = 0.f;
  float jittered_error = 0.f;
  for (uint32_t i = 0u; i < 16u; ++i) {
    if (!std::isfinite(matrix[i]) || !std::isfinite(expected[i])) return false;
    canonical_error = std::max(canonical_error, std::abs(matrix[i] - expected[i]));
    jittered_error = std::max(jittered_error, std::abs(matrix[i] - jittered[i]));
  }
  if (canonical_error > 0.00001f || jittered_error <= canonical_error) {
    return false;
  }
  if (!TranslateClip(matrix, 4u, scope.jitter)) return false;
  return true;
}

inline void __fastcall HookSunPass(void* renderer, void* viewport, uint32_t a, uint32_t b, uint8_t c, uint8_t d) {
  const NativeProjectionScope scope(state::ProjectionJitterPath::SUN_VOLUME, viewport);
  sun_pass(renderer, viewport, a, b, c, d);
}

inline void __fastcall HookSunVolume(void* context, const float* vp, const void* volume, const void* color,
                                     void* vertices, void* indices) {
  alignas(16) std::array<float, 16> copy = {};
  if (_ReturnAddress() == native_base + 0x30B07Du && native_scope != nullptr
      && native_scope->path == state::ProjectionJitterPath::SUN_VOLUME && vp != nullptr) {
    std::memcpy(copy.data(), vp, 64u);
    if (CorrectNativeMatrix(copy.data(), *native_scope)) vp = copy.data();
  }
  // The native helper finishes the upload and draw before this copy dies.
  sun_volume(context, vp, volume, color, vertices, indices);
}

inline uint32_t __fastcall HookShFallback(void* renderer, uint32_t kind, float* output, float* min_w,
                                          float* max_w, void* shape, void* viewport) {
  const bool qualified = _ReturnAddress() == native_base + 0x1F2529u && kind < SH_VERTEX_COUNTS.size();
  const NativeProjectionScope scope(state::ProjectionJitterPath::SH_LIGHTING, qualified ? viewport : nullptr);
  const uint32_t success = sh_fallback(renderer, kind, output, min_w, max_w, shape, viewport);
  if (success != 0u && scope.valid) {
    const camera_state::PublicationWriterGuard publication_guard;
    if (scope.CurrentLocked()) {
      TranslateClip(output, SH_VERTEX_COUNTS[kind], scope.jitter);
    }
  }
  return success;  // EAX is success, NOT a vertex count. Bounds/Z/W untouched.
}

inline void __fastcall HookShProducer(void* renderer, void* scene, void* viewport, void* inputs) {
  const NativeProjectionScope scope(state::ProjectionJitterPath::SH_LIGHTING, viewport);
  sh_producer(renderer, scene, viewport, inputs);
}

inline void __fastcall HookShAttach(void* job, void* packet, uint64_t c, uint64_t d) {
  if (_ReturnAddress() == native_base + 0x282A8Bu && packet != nullptr && native_scope != nullptr
      && native_scope->path == state::ProjectionJitterPath::SH_LIGHTING
      && std::memcmp(static_cast<const uint8_t*>(packet) + 0x120u, native_scope->view.data(), 64u) == 0) {
    // Both SH workers retain this packet. ExecuteDrawLight derives its clip
    // vertices and inverse light-volume coordinates from the same render P/V.
    // Correct it before publication so the two calculations use one sample.
    CorrectNativeMatrix(reinterpret_cast<float*>(static_cast<uint8_t*>(packet) + 0x160u), *native_scope, true);
  }
  sh_attach(job, packet, c, d);
}

inline void* __fastcall HookSkyParameters(void* plugin, uint32_t* result, void* viewport, void* parameters) {
  const bool qualified = _ReturnAddress() == native_base + 0x1F5990u && result != nullptr && parameters != nullptr;
  const NativeProjectionScope scope(state::ProjectionJitterPath::SKY, qualified ? viewport : nullptr);
  void* returned = sky_parameters(plugin, result, viewport, parameters);
  if (!scope.valid || *result == UINT32_MAX) return returned;

  const camera_state::PublicationWriterGuard publication_guard;
  if (!scope.CurrentLocked()) return returned;
  auto* inverse = reinterpret_cast<float*>(static_cast<uint8_t*>(parameters) + 0x40u);
  // Sky_Draw reconstructs a ray from clip XY using this inverse projection.
  // Validate the native inverse before composing inverse(P) * inverse(jitter).
  // CalcShaderParameters stores inverse rows for direct shader dot products.
  for (uint32_t column = 0u; column < 4u; ++column) {
    for (uint32_t row = 0u; row < 4u; ++row) {
      float value = 0.f;
      for (uint32_t inner = 0u; inner < 4u; ++inner) {
        value += scope.projection[(inner * 4u) + row] * inverse[(inner * 4u) + column];
      }
      const float expected = (column == row ? 1.f : 0.f);
      if (!std::isfinite(value) || std::abs(value - expected) > 0.0001f) return returned;
    }
  }
  std::array<float, 4> translated = {};
  for (uint32_t row = 0u; row < 4u; ++row) {
    translated[row] = inverse[(row * 4u) + 3u] - (2.f * scope.jitter[0] * inverse[row * 4u])
                      + (2.f * scope.jitter[1] * inverse[(row * 4u) + 1u]);
    if (!std::isfinite(translated[row])) return returned;
  }
  for (uint32_t row = 0u; row < 4u; ++row) {
    inverse[(row * 4u) + 3u] = translated[row];
  }
  return returned;
}

inline void __fastcall HookTerrainProducer(void* a, void* b, void* scene, void* viewport, uint64_t e) {
  const NativeProjectionScope scope(state::ProjectionJitterPath::TERRAIN_DECAL, viewport);
  terrain_producer(a, b, scene, viewport, e);
}

inline void __fastcall HookTerrainAttach(void* command, void* packet, uint64_t c, uint64_t d) {
  if (_ReturnAddress() == native_base + 0x2755CAu && packet != nullptr && native_scope != nullptr
      && native_scope->path == state::ProjectionJitterPath::TERRAIN_DECAL) {
    CorrectNativeMatrix(reinterpret_cast<float*>(static_cast<uint8_t*>(packet) + 0x160u), *native_scope, true);
  }
  // Original native object/header is retained; no borrowed/stack packet escapes.
  terrain_attach(command, packet, c, d);
}

inline void __fastcall HookAlternateProjection(void* context) {
  const NativeProjectionScope scope(state::ProjectionJitterPath::ALTERNATE_CAMERA, context, true);
  alternate_projection(context);
}

inline void* __fastcall HookAlternateCommand(void* context, void* command) {
  const NativeProjectionScope scope(state::ProjectionJitterPath::ALTERNATE_CAMERA, context, true);
  return alternate_command(context, command);
}

inline void __fastcall HookStageMatrix(void* context, uint32_t id, const float* matrix) {
  const void* caller = _ReturnAddress();
  alignas(16) std::array<float, 16> copy = {};
  if ((caller == native_base + 0x2E7484u || caller == native_base + 0x2E4DD7u)
      && id == 0x47u && matrix != nullptr && native_scope != nullptr
      && native_scope->path == state::ProjectionJitterPath::ALTERNATE_CAMERA) {
    std::memcpy(copy.data(), matrix, 64u);
    if (CorrectNativeMatrix(copy.data(), *native_scope)) matrix = copy.data();
  }
  // Only the synchronous staging source changes. Context P/V never change,
  // including null-camera updates, nested streams and every early return.
  stage_matrix(context, id, matrix);
}

inline bool InitializeNativeDiagnosticAddresses(HMODULE module) {
  auto* base = reinterpret_cast<uint8_t*>(module);
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
  if (nt->OptionalHeader.SizeOfImage != 0xA01D000u) return false;
  // The SH output packet is still producer-owned at this call. The sky helper
  // finishes its inverse matrices before MainExec uploads them for the draw.
  if (!MatchesBytes(base + 0x281EF0u, std::array<uint8_t, 15>{0x48, 0x8B, 0xC4, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57})
      || !MatchesBytes(base + 0x283B90u, std::array<uint8_t, 16>{0x40, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF, 0x48})
      || !MatchesBytes(base + 0x1F50D0u, std::array<uint8_t, 16>{0x48, 0x8B, 0xC4, 0x53, 0x55, 0x56, 0x57, 0x41, 0x56, 0x48, 0x81, 0xEC, 0x30, 0x01, 0x00, 0x00})
      || DecodeRelativeCall(base + 0x282A86u) != base + 0x283B90u
      || DecodeRelativeCall(base + 0x1F598Bu) != base + 0x1F50D0u) return false;
  // All checked RVAs (including the four-entry table) are below SizeOfImage.
  // Exact prologues include RIP displacements: other EXE builds fail closed.
  if (!MatchesBytes(base + 0x30AC50u, std::array<uint8_t, 16>{
                                          0x48,
                                          0x89,
                                          0x5C,
                                          0x24,
                                          0x08,
                                          0x44,
                                          0x89,
                                          0x4C,
                                          0x24,
                                          0x20,
                                          0x44,
                                          0x89,
                                          0x44,
                                          0x24,
                                          0x18,
                                          0x48,
                                      })
      || !MatchesBytes(base + 0x30B680u, std::array<uint8_t, 16>{
                                             0x48,
                                             0x8B,
                                             0xC4,
                                             0x48,
                                             0x89,
                                             0x58,
                                             0x10,
                                             0x48,
                                             0x89,
                                             0x70,
                                             0x18,
                                             0x48,
                                             0x89,
                                             0x78,
                                             0x20,
                                             0x55,
                                         })
      || !MatchesBytes(base + 0x27EF80u, std::array<uint8_t, 16>{
                                             0x48,
                                             0x89,
                                             0x5C,
                                             0x24,
                                             0x08,
                                             0x55,
                                             0x56,
                                             0x57,
                                             0x41,
                                             0x56,
                                             0x41,
                                             0x57,
                                             0x48,
                                             0x81,
                                             0xEC,
                                             0x80,
                                         })
      || !MatchesBytes(base + 0x275280u, std::array<uint8_t, 16>{
                                             0x40,
                                             0x53,
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
                                             0x81,
                                             0xEC,
                                         })
      || !MatchesBytes(base + 0x2D18D0u, std::array<uint8_t, 16>{
                                             0x40,
                                             0x57,
                                             0x48,
                                             0x83,
                                             0xEC,
                                             0x30,
                                             0x48,
                                             0xC7,
                                             0x44,
                                             0x24,
                                             0x20,
                                             0xFE,
                                             0xFF,
                                             0xFF,
                                             0xFF,
                                             0x48,
                                         })
      || !MatchesBytes(base + 0x2E7330u, std::array<uint8_t, 16>{
                                             0x4C,
                                             0x8B,
                                             0xDC,
                                             0x48,
                                             0x81,
                                             0xEC,
                                             0xA8,
                                             0x00,
                                             0x00,
                                             0x00,
                                             0x48,
                                             0x8B,
                                             0x05,
                                             0xEF,
                                             0x1F,
                                             0x86,
                                         })
      || !MatchesBytes(base + 0x2E4C70u, std::array<uint8_t, 16>{
                                             0x48,
                                             0x8B,
                                             0xC4,
                                             0x48,
                                             0x89,
                                             0x58,
                                             0x10,
                                             0x57,
                                             0x48,
                                             0x81,
                                             0xEC,
                                             0xC0,
                                             0x00,
                                             0x00,
                                             0x00,
                                             0x0F,
                                         })
      || !MatchesBytes(base + 0x2D7AA0u, std::array<uint8_t, 16>{
                                             0x48,
                                             0x81,
                                             0xEC,
                                             0x88,
                                             0x00,
                                             0x00,
                                             0x00,
                                             0x48,
                                             0x8B,
                                             0x05,
                                             0x82,
                                             0x18,
                                             0x87,
                                             0x02,
                                             0x48,
                                             0x33,
                                         })
      || DecodeRelativeCall(base + 0x30B078u) != base + 0x30B680u || DecodeRelativeCall(base + 0x1F2524u) != base + 0x27EF80u || DecodeRelativeCall(base + 0x2755C5u) != base + 0x2D18D0u || DecodeRelativeCall(base + 0x2E747Fu) != base + 0x2D7AA0u || DecodeRelativeCall(base + 0x2E4DD2u) != base + 0x2D7AA0u || std::memcmp(base + 0x21102B8u, SH_VERTEX_COUNTS.data(), sizeof(SH_VERTEX_COUNTS)) != 0) {
    return false;
  }
  native_base = base;
  sun_pass = reinterpret_cast<SunPass>(base + 0x30AC50u);
  sun_volume = reinterpret_cast<SunVolume>(base + 0x30B680u);
  sh_fallback = reinterpret_cast<ShFallback>(base + 0x27EF80u);
  sh_producer = reinterpret_cast<ShProducer>(base + 0x281EF0u);
  sh_attach = reinterpret_cast<AttachPacket>(base + 0x283B90u);
  sky_parameters = reinterpret_cast<SkyParameters>(base + 0x1F50D0u);
  terrain_producer = reinterpret_cast<TerrainProducer>(base + 0x275280u);
  terrain_attach = reinterpret_cast<AttachPacket>(base + 0x2D18D0u);
  alternate_projection = reinterpret_cast<AlternateProjection>(base + 0x2E7330u);
  alternate_command = reinterpret_cast<AlternateCommand>(base + 0x2E4C70u);
  stage_matrix = reinterpret_cast<StageMatrix>(base + 0x2D7AA0u);
  return true;
}
#endif

inline bool Attach() {
  const std::scoped_lock lifecycle_guard(lifecycle_mutex);
  if (installed.load(std::memory_order_acquire)) return true;

  camera_state::Invalidate();

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
    request_local_light_draws = nullptr;
    return false;
  }

#if ENABLE_TAA_PROJECTION_JITTER_DIAGNOSTICS
  if (!InitializeNativeDiagnosticAddresses(module)) {
    logging::Warn("native projection diagnostic executable validation failed");
    return false;
  }
#endif
  // Never reclaim trampolines while native callers can still enter. Pin the
  // addon as well: device destruction/FreeLibrary is not native quiescence.
  // Installation remains at initial device creation, before scene rendering.
  HMODULE pinned_module = nullptr;
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                         reinterpret_cast<LPCWSTR>(&HookSetViewMatrixState), &pinned_module)
      == FALSE) return false;
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
          reinterpret_cast<void**>(&request_local_light_draws),
          reinterpret_cast<void*>(HookRequestLocalLightDraws))
      != NO_ERROR) {
    DetourTransactionAbort();
    return false;
  }
#if ENABLE_TAA_PROJECTION_JITTER_DIAGNOSTICS
  if (DetourAttach(reinterpret_cast<void**>(&sun_pass), reinterpret_cast<void*>(HookSunPass)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&sun_volume), reinterpret_cast<void*>(HookSunVolume)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&sh_fallback), reinterpret_cast<void*>(HookShFallback)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&sh_producer), reinterpret_cast<void*>(HookShProducer)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&sh_attach), reinterpret_cast<void*>(HookShAttach)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&sky_parameters), reinterpret_cast<void*>(HookSkyParameters)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&terrain_producer), reinterpret_cast<void*>(HookTerrainProducer)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&terrain_attach), reinterpret_cast<void*>(HookTerrainAttach)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&alternate_projection), reinterpret_cast<void*>(HookAlternateProjection)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&alternate_command), reinterpret_cast<void*>(HookAlternateCommand)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&stage_matrix), reinterpret_cast<void*>(HookStageMatrix)) != NO_ERROR) {
    DetourTransactionAbort();
    return false;
  }
#endif
  if (DetourTransactionCommit() != NO_ERROR) {
    DetourTransactionAbort();
    set_view_matrix_state = nullptr;
    forward_rendering = nullptr;
    request_local_light_draws = nullptr;
    return false;
  }

  installed.store(true, std::memory_order_release);
  logging::Info("installed native projection hook aob=",
                logging::Hex{reinterpret_cast<uintptr_t>(projection_commit_address)},
                " helper=", logging::Hex{reinterpret_cast<uintptr_t>(helper_entry_address)},
                " return=", logging::Hex{reinterpret_cast<uintptr_t>(expected_return_address)},
                " forward=", logging::Hex{reinterpret_cast<uintptr_t>(forward_rendering)},
                " local_light_packets=", logging::Hex{reinterpret_cast<uintptr_t>(request_local_light_draws)},
                " shader_manager_global=", logging::Hex{reinterpret_cast<uintptr_t>(shader_manager_global)});
  return true;
}

inline void StopAdmission(bool wait_for_lifecycle_owner = true, bool transition_runtime = true) {
  std::unique_lock lifecycle_guard(lifecycle_mutex, std::defer_lock);
  if (wait_for_lifecycle_owner) {
    lifecycle_guard.lock();
  } else if (!lifecycle_guard.try_lock()) {
    // Process teardown holds the loader lock; never wait for an installer.
    return;
  }
  if (!installed.load(std::memory_order_acquire) || set_view_matrix_state == nullptr) return;

  if (transition_runtime) {
    const camera_state::PublicationWriterGuard publication_guard;
    state::SetTemporalMode(state::TemporalMode::OFF);
    camera_state::InvalidateLocked();
  }

  // Intentionally retain installed code and trampolines until process exit.
  // A counter reaching zero is NOT a barrier against a subsequent native call.
  // No wait, unpatch, pointer clearing or trampoline free occurs here.
}

inline void OnInitDevice(reshade::api::device* device) {
  (void)device;
  Attach();
}

inline void Use(DWORD reason) {
  if (reason == DLL_PROCESS_ATTACH) {
    reshade::register_event<reshade::addon_event::init_device>(OnInitDevice);
  } else if (reason == DLL_PROCESS_DETACH) {
    reshade::unregister_event<reshade::addon_event::init_device>(OnInitDevice);
    // DllMain holds the loader lock. Process termination reclaims pinned code;
    // never run a Detours transaction or wait for native work here.
    StopAdmission(false, false);
  }
}

}  // namespace mgsv::reconstruction::projection_jitter
