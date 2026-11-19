#pragma once

/*
 * Camera jitter for temporal reconstruction. While a method is on, the engine
 * paths below that turn the scene's projection into GPU constants all get the
 * same per-scene sub-pixel offset.
 *
 * HookSetViewMatrixState is the main hook. When Render::buildRendering commits
 * a gameplay projection (the call returning to RENDER_SET_VIEW_RETURN), it
 * reserves the scene's jitter sample, offsets the shader manager's active
 * projection, and publishes the unjittered camera to camera_state. Calls from
 * the velocity, model and thermography paths reapply the saved sample. The
 * other hooks carry the same sample into paths with their own copies of the
 * camera: forward rendering, local-light packets, the scene constant bank,
 * skin-scattering constants, the sun volume, spherical-harmonics lighting,
 * the sky and terrain decals.
 *
 * With reconstruction off, the hooks change nothing and the game's next
 * projection copy is unmodified. HookFlushConstants also improves the
 * precision of the character-motion matrices, which stays on regardless.
 */

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include <intrin.h>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <shared_mutex>

#include <detours.h>
#include <include/reshade.hpp>

#include "../../../utils/hash.hpp"
#include "../engine/build.hpp"
#include "../engine/image.hpp"
#include "camera_state.hpp"
#include "logging.hpp"
#include "resolution.hpp"
#include "state.hpp"

namespace mgsv::reconstruction::projection_jitter {

// The native path a NativeProjectionScope belongs to. Nested hooks check it
// before they read the scope's camera.
enum class ProjectionPath : uint8_t {
  SUN_VOLUME,
  SH_LIGHTING,
  TERRAIN_DECAL,
  SKY,
};

using SetViewMatrixState = void(__fastcall*)(const float* view_matrix);
using FlushConstants = void(__fastcall*)(void* registers, void* backend);
using UploadConstants = void(__fastcall*)(void* manager, void* buffer, const void* values, uint32_t bytes);
using RenderPluginCallback = void(__fastcall*)(void* plugin, void* render, void* viewport);
using RequestLocalLightDraws = void(__fastcall*)(
    void* renderer, void* packet, void* visible_lights, void* shadow_ready,
    void* input_context, void* output_context, const void* viewport, void* jobs);

// Native scene constants occupy thirty float4 registers in bank 2. Register 28
// carries the projection center used to reconstruct camera rays from depth.
inline constexpr int SCENE_CAMERA_CENTER_REGISTER = 28;
inline constexpr uint32_t SCENE_CONSTANT_REGISTERS = 30u;
// The velocity vertex programs give c12 and c16 different meanings from the
// lighting programs. Identify their exact bytecode before changing those fields.
inline constexpr GUID VELOCITY_SHADER_GUID = {
    .Data1 = 0x153488D9u,
    .Data2 = 0x1D71u,
    .Data3 = 0x494Du,
    .Data4 = {0x90u, 0x68u, 0xA9u, 0xFCu, 0x18u, 0x62u, 0x3Bu, 0xD7u},
};
enum class VelocityShaderKind : uint8_t {
  NONE,
  OPAQUE_MOTION,
  MASKED_MOTION,
};

// Every hook and return address below, the calls between them, and the
// engine's spherical-harmonic volume table these hooks index.
inline constexpr std::array REQUIRED_ENTRIES = {
    engine::Entry::FORWARD_RENDERING_PRE_EXEC,
    engine::Entry::REQUEST_DRAW_ALL_LOCAL_LIGHT_ON_JOB,
    engine::Entry::LOCAL_LIGHT_REQUEST_SLOT,
    engine::Entry::LOCAL_LIGHT_REQUEST_RETURN,
    engine::Entry::LOCAL_LIGHT_VIEWPORT_COPY,
    engine::Entry::SHADOW_CONSTANT_REGISTER_FLUSH,
    engine::Entry::CONSTANT_BUFFER_MAP_DATA,
    engine::Entry::COMMAND_BUFFER_FLUSH_RENDER_RESOURCE,
    engine::Entry::COMMAND_BUFFER_FLUSH_CONSTANT_REGISTER,
    engine::Entry::MODEL_FLUSH_RENDER_RESOURCE_CALL,
    engine::Entry::MODEL_CONSTANT_UPLOAD_RETURN,
    engine::Entry::SCATTER_UPLOAD_RETURN,
    engine::Entry::PROJECTION_COMMIT,
    engine::Entry::RENDER_SET_VIEW_RETURN,
    engine::Entry::SHADER_MANAGER_SET_VIEW_MATRIX,
    engine::Entry::SHADER_MANAGER,
    engine::Entry::DEFERRED_GEOMETRY_SET_VIEW_RETURN,
    engine::Entry::MODEL_SET_VIEW_RETURN,
    engine::Entry::ALPHA_MODEL_SET_VIEW_RETURN,
    engine::Entry::OVERLAY_MODEL_SET_VIEW_RETURN,
    engine::Entry::NEW_THERMOGRAPHY_SET_VIEW_RETURN,
    engine::Entry::THERMOGRAPHY_SET_VIEW_RETURN,
    engine::Entry::SUN_LIGHT_DRAW,
    engine::Entry::DRAW_SHADOW_BOX,
    engine::Entry::SUN_SHADOW_BOX_RETURN,
    engine::Entry::SPHERICAL_HARMONICS_CALCULATE_LIGHT_VOLUME,
    engine::Entry::SPHERICAL_HARMONICS_LIGHT_VOLUME_RETURN,
    engine::Entry::SPHERICAL_HARMONICS_REGISTER_DRAW_ON_JOB,
    engine::Entry::SPHERICAL_HARMONICS_SET_OUTPUT,
    engine::Entry::SPHERICAL_HARMONICS_SET_OUTPUT_RETURN,
    engine::Entry::SPHERICAL_HARMONICS_VOLUME_VERTEX_COUNTS,
    engine::Entry::SKY_CALC_SHADER_PARAMETERS,
    engine::Entry::SKY_PARAMETERS_RETURN,
    engine::Entry::TERRAIN_DECAL_DRAW_ON_JOB,
    engine::Entry::TERRAIN_DECAL_SET_INPUT,
    engine::Entry::TERRAIN_DECAL_SET_INPUT_RETURN,
};
inline constexpr engine::Requirement REQUIREMENT = {.feature = "Camera jitter for temporal reconstruction", .entries = REQUIRED_ENTRIES};

inline constexpr uint32_t REQUIRED_RESTORATION_HITS = 3u;

inline SetViewMatrixState set_view_matrix_state = nullptr;
inline FlushConstants flush_constants = nullptr;
inline const void* motion_upload_return_address = nullptr;
inline UploadConstants upload_constants = nullptr;
inline const void* scatter_upload_return_address = nullptr;
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
// Only install and shutdown take this mutex; the native hooks never do, so
// they never wait on it. Hold it before changing any hook target or trampoline.
inline std::shared_mutex lifecycle_mutex;

// After jitter is turned off, the next REQUIRED_RESTORATION_HITS gameplay
// projection copies must equal the shader manager's active projection. That
// confirms the game's projection is back.
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

// A finite perspective projection from a viewport with a camera and flag bit 0
// set. Viewports smaller than 640x360 pass only at the current upscaling
// render size.
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

// Offsets the projection center and sets the same shader-manager dirty flags
// the engine sets when it commits a projection, so the change is uploaded.
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

// Called under the publication lock. Returns the camera saved for the current
// scene when it matches this viewport's live camera, matrices and size; never
// a newer scene that another worker may already be recording.
inline bool GetPublishedCameraForViewportLocked(const uint8_t* viewport, camera_state::CameraFrame* frame) {
  if (!state::IsEnabled() || viewport == nullptr) return false;

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

  const auto owner = engine::views::GetScope().owner;
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

  *frame = published;
  return true;
}

// Hold the publication lock through validation and the resulting matrix write.
inline bool GetPublishedJitterForViewportLocked(
    const uint8_t* viewport,
    std::array<float, 2>* jitter_uv) {
  camera_state::CameraFrame published;
  if (!GetPublishedCameraForViewportLocked(viewport, &published)) return false;
  *jitter_uv = {
      published.jitter_uv_x,
      published.jitter_uv_y,
  };
  return true;
}

// Corrects the scene constant bank as it is uploaded: adds the scene's jitter
// to the camera center (register 28), and improves the precision of the
// character-motion matrices. Engine commands can write the bank directly,
// without the setter, so the correction happens here, after every write.
inline void __fastcall HookFlushConstants(void* registers, void* backend) {
  if (registers == nullptr) {
    flush_constants(registers, backend);
    return;
  }
  auto* bytes = static_cast<uint8_t*>(registers);
  // Eight 32-byte bank records precede the native dirty and active masks.
  // Bank 2 is the 480-byte scene block. Leave the game's upload decisions alone.
  const uint32_t dirty = *reinterpret_cast<const uint32_t*>(bytes + 0x110u);
  const uint32_t active = *reinterpret_cast<const uint32_t*>(bytes + 0x114u);
  auto** source = reinterpret_cast<const float**>(bytes + 0x48u);
  if ((dirty & active & 4u) == 0u || *source == nullptr
      || *reinterpret_cast<const uint32_t*>(bytes + 0x58u) != SCENE_CONSTANT_REGISTERS) {
    flush_constants(registers, backend);
    return;
  }
  alignas(16) std::array<float, SCENE_CONSTANT_REGISTERS * 4u> corrected;
  bool replace_source = false;
  if (state::IsEnabled()) {
    const camera_state::PublicationWriterGuard guard;
    camera_state::CameraFrame frame;
    const auto owner = engine::views::GetScope().owner;
    if (GetPublishedCameraForViewportLocked(std::bit_cast<const uint8_t*>(owner.viewport), &frame)) {
      const auto* base = reinterpret_cast<const float*>(std::bit_cast<const uint8_t*>(frame.camera_identity) + 0x118u);
      const size_t index = SCENE_CAMERA_CENTER_REGISTER * 4u;
      if ((*source)[index] == base[0] && (*source)[index + 1u] == base[1]) {
        std::memcpy(corrected.data(), *source, sizeof(corrected));
        corrected[index] += 2.f * frame.jitter_uv_x;
        corrected[index + 1u] -= 2.f * frame.jitter_uv_y;
        replace_source = true;
      }
    }
  }
  if (backend != nullptr && _ReturnAddress() == motion_upload_return_address) {
    // Only this verified model call has already flushed its shader bindings.
    // Other upload callers may still have a shader change pending. Read this
    // recording context's bound VS, never another worker's state.
    auto* context = *reinterpret_cast<ID3D11DeviceContext**>(static_cast<uint8_t*>(backend) + 8u);
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex;
    if (context != nullptr) {
      context->VSGetShader(&vertex, nullptr, nullptr);
    }
    VelocityShaderKind vertex_kind = VelocityShaderKind::NONE;
    UINT size = sizeof(vertex_kind);
    bool motion_pair = false;
    if (vertex != nullptr && SUCCEEDED(vertex->GetPrivateData(VELOCITY_SHADER_GUID, &size, &vertex_kind))
        && size == sizeof(vertex_kind) && vertex_kind != VelocityShaderKind::NONE) {
      // The same scene bank is visible to both stages. Require the matching
      // native motion PS as well; an unrelated PS may read these fields itself.
      Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel;
      context->PSGetShader(&pixel, nullptr, nullptr);
      VelocityShaderKind pixel_kind = VelocityShaderKind::NONE;
      size = sizeof(pixel_kind);
      motion_pair = pixel != nullptr && SUCCEEDED(pixel->GetPrivateData(VELOCITY_SHADER_GUID, &size, &pixel_kind))
                    && size == sizeof(pixel_kind) && pixel_kind == vertex_kind;
    }
    if (motion_pair) {
      // This precision correction uses only the two native matrices from this
      // upload. It needs no reconstruction history or published camera and
      // stays on with reconstruction Off.
      if (!replace_source) {
        std::memcpy(corrected.data(), *source, sizeof(corrected));
      }
      // Native motion transforms the previous skinned position into world space,
      // then into clip space. Large world coordinates lose subpixel precision in
      // those float operations. Combine the matrices once per upload in double,
      // before rounding back to float. The unmodified vertex shader then goes
      // straight from previous model space to clip space, then multiplies by
      // identity.
      constexpr size_t previous_world = 12u * 4u;
      constexpr size_t previous_clip = 16u * 4u;
      for (size_t row = 0u; row < 4u; ++row) {
        for (size_t column = 0u; column < 4u; ++column) {
          double value = 0.0;
          for (size_t k = 0u; k < 4u; ++k) {
            value += static_cast<double>((*source)[previous_clip + (row * 4u) + k])
                     * (*source)[previous_world + (k * 4u) + column];
          }
          corrected[previous_world + (row * 4u) + column] = static_cast<float>(value);
          corrected[previous_clip + (row * 4u) + column] = (row == column ? 1.f : 0.f);
        }
      }
      replace_source = true;
    }
  }
  if (!replace_source) {
    flush_constants(registers, backend);
    return;
  }
  // Only one recording thread uses this command context, and Flush copies the
  // selected bank into its mapped GPU buffer before returning. So point the
  // source at our corrected copy for this call only, keep all of the engine's
  // dirty and binding updates, and restore the pointer afterward. The engine's
  // CPU data and the camera are never changed.
  struct RestoreSource {
    const float** destination;
    const float* original;
    ~RestoreSource() { *destination = original; }
  };
  const RestoreSource restore{.destination = source, .original = *source};
  *source = corrected.data();
  flush_constants(registers, backend);
}

// Applies the scene's jitter to the camera center in the 64-byte
// skin-scattering constants uploaded from SCATTER_UPLOAD_RETURN.
inline void __fastcall HookUploadConstants(void* manager, void* buffer, const void* values, uint32_t bytes) {
  struct alignas(16) ScatterConstants {
    uint32_t width;
    uint32_t height;
    uint32_t tiles_x;
    uint32_t tiles_y;
    std::array<float, 2> half_fov_recip;
    std::array<float, 2> camera_center_offset;
    std::array<float, 4> projection;
    std::array<float, 2> tweak;
    float fade;
    float padding;
  };
  static_assert(sizeof(ScatterConstants) == 64u);
  if (_ReturnAddress() != scatter_upload_return_address || bytes != sizeof(ScatterConstants)
      || values == nullptr || !state::IsEnabled()) {
    upload_constants(manager, buffer, values, bytes);
    return;
  }
  ScatterConstants corrected;
  const void* selected = values;
  {
    const camera_state::PublicationWriterGuard guard;
    camera_state::CameraFrame frame;
    const auto owner = engine::views::GetScope().owner;
    if (GetPublishedCameraForViewportLocked(std::bit_cast<const uint8_t*>(owner.viewport), &frame)) {
      std::memcpy(&corrected, values, sizeof(corrected));
      const auto* base = reinterpret_cast<const float*>(std::bit_cast<const uint8_t*>(frame.camera_identity) + 0x118u);
      if (corrected.width == frame.width && corrected.height == frame.height
          && corrected.camera_center_offset[0] == base[0] && corrected.camera_center_offset[1] == base[1]) {
        corrected.camera_center_offset[0] += 2.f * frame.jitter_uv_x;
        corrected.camera_center_offset[1] -= 2.f * frame.jitter_uv_y;
        selected = &corrected;
      }
    }
  }
  // Both native scatter stages and custom diffuse lighting read this one block.
  upload_constants(manager, buffer, selected, bytes);
}

// Reapplies the scene's saved jitter to the shader manager's active projection
// when it still holds this viewport's unjittered projection.
inline void ApplyPublishedJitterToActiveProjection(
    const uint8_t* viewport) {
  // The caller may already hold the execution lock, so do not take it here.
  // Keep camera validation and the write under one publication lock so a
  // reset, a camera change or a new sample cannot happen between them.
  const camera_state::PublicationWriterGuard publication_guard;
  std::array<float, 2> jitter_uv = {};
  void* shader_manager = shader_manager_global != nullptr ? *shader_manager_global : nullptr;
  if (shader_manager == nullptr) {
    return;
  }
  auto* active_projection = reinterpret_cast<float*>(static_cast<uint8_t*>(shader_manager) + 0x680u);
  if (!GetPublishedJitterForViewportLocked(viewport, &jitter_uv)) return;

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
    if (return_address != velocity_return_address && return_address != model_return_address
        && return_address != alpha_model_return_address && return_address != overlay_model_return_address
        && return_address != new_thermography_return_address && return_address != thermography_return_address) return;
    const auto* viewport = reinterpret_cast<const uint8_t*>(view_matrix) - 0x2C0u;
    ApplyPublishedJitterToActiveProjection(viewport);
    return;
  }

  const bool taa_enabled = state::IsEnabled();
  // The hook stays installed for the whole process. With reconstruction off it
  // does nothing more, unless the restoration check is still running.
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

  // Three matching unmodified copies in a row confirm that turning
  // reconstruction off restored the game's active projection.
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

  const auto owner = engine::views::GetScope().owner;
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
  // First setup of this scene: reserve its sample and number, jitter the
  // active projection, and publish the unjittered camera.
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
  camera_state::CameraFrame camera_frame = {
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
  };
  // Read native history while the originating viewport is alive. A later
  // draw may run after another worker changes it, so retain values, not pointers.
  std::memcpy(camera_frame.native_previous_projection.data(), viewport + 0x400u, sizeof(camera_frame.native_previous_projection));
  std::memcpy(camera_frame.native_previous_view.data(), viewport + 0x3C0u, sizeof(camera_frame.native_previous_view));
  camera_frame.native_history_available = true;
  camera_state::PublishLocked(camera_frame);
}

// The forward-rendering plugin can rewrite the active projection; reapply the
// scene's jitter after it.
inline void __fastcall HookForwardRendering(void* plugin, void* render, void* viewport_pointer) {
  const auto original = forward_rendering;
  if (original == nullptr) return;
  original(plugin, render, viewport_pointer);

  ApplyPublishedJitterToActiveProjection(static_cast<const uint8_t*>(viewport_pointer));
}

inline void __fastcall HookRequestLocalLightDraws(
    void* renderer, void* packet_pointer, void* visible_lights, void* shadow_ready,
    void* input_context, void* output_context, const void* viewport_pointer, void* jobs) {
  const auto original = request_local_light_draws;
  if (original == nullptr) return;

  // PluginLocalLight::MainExec has finished this packet but not yet handed it
  // to worker jobs. Only this verified caller passes the viewport as argument
  // seven.
  if (_ReturnAddress() == local_light_request_return_address
      && packet_pointer != nullptr && viewport_pointer != nullptr && state::IsEnabled()) {
    const camera_state::PublicationWriterGuard publication_guard;
    const auto* viewport = static_cast<const uint8_t*>(viewport_pointer);
    auto* packet = static_cast<uint8_t*>(packet_pointer);
    std::array<float, 2> jitter_uv = {};
    // Match the packet's copied projection, view and viewport rectangle as well
    // as the scene's camera. A packet that already has jitter fails this
    // check, so it never gets a second offset.
    if (GetPublishedJitterForViewportLocked(viewport, &jitter_uv)
        && std::memcmp(packet + 0x1A0u, viewport + 0x280u, 2u * 16u * sizeof(float)) == 0
        && std::memcmp(packet + 0x370u, viewport + 0x5D0u, 4u * sizeof(uint32_t)) == 0) {
      // Workers keep or copy this render projection. The separate culling
      // matrices and frustum planes are left alone, and the shared viewport is
      // only read.
      ApplyProjectionJitter(reinterpret_cast<float*>(packet + 0x1A0u), jitter_uv[0], jitter_uv[1]);
    }
  }

  // Release the publication lock before the game copies packets and queues jobs.
  original(renderer, packet_pointer, visible_lights, shadow_ready,
           input_context, output_context, viewport_pointer, jobs);
}

// Paths that build their own matrices from the scene's camera. A producer hook
// opens a NativeProjectionScope around the engine call. The matrices that call
// produces are corrected by nested hooks or after the call returns.
using SunPass = void(__fastcall*)(void*, void*, uint32_t, uint32_t, uint8_t, uint8_t);
using SunVolume = void(__fastcall*)(void*, const float*, const void*, const void*, void*, void*);
using ShFallback = uint32_t(__fastcall*)(void*, uint32_t, float*, float*, float*, void*, void*);
using ShProducer = void(__fastcall*)(void*, void*, void*, void*);
using SkyParameters = void*(__fastcall*)(void*, uint32_t*, void*, void*);
using TerrainProducer = void(__fastcall*)(void*, void*, void*, void*, uint64_t);
using AttachPacket = void(__fastcall*)(void*, void*, uint64_t, uint64_t);
inline SunPass sun_pass = nullptr;
inline SunVolume sun_volume = nullptr;
inline ShFallback sh_fallback = nullptr;
inline ShProducer sh_producer = nullptr;
inline AttachPacket sh_attach = nullptr;
inline SkyParameters sky_parameters = nullptr;
inline TerrainProducer terrain_producer = nullptr;
inline AttachPacket terrain_attach = nullptr;
inline const void* sun_shadow_box_return_address = nullptr;
inline const void* sh_light_volume_return_address = nullptr;
inline const void* sh_set_output_return_address = nullptr;
inline const void* sky_parameters_return_address = nullptr;
inline const void* terrain_set_input_return_address = nullptr;
inline constexpr std::array<uint32_t, 4> SH_VERTEX_COUNTS = {8u, 6u, 18u, 8u};

// The innermost NativeProjectionScope on this thread.
struct NativeProjectionScope;
inline thread_local const NativeProjectionScope* native_scope = nullptr;

// Lives for the duration of one engine call so nested hooks on the same thread
// can find the scene's camera. It copies every matrix and sample it needs;
// only the link to the enclosing scope is a pointer. A scope that failed its
// checks still hides the enclosing one, so a nested call never uses another
// call's camera.
struct NativeProjectionScope {
  const NativeProjectionScope* previous = native_scope;
  ProjectionPath path;
  camera_state::CameraFrame frame = {};
  alignas(16) std::array<float, 16> projection = {};
  alignas(16) std::array<float, 16> view = {};
  std::array<float, 2> jitter = {};
  bool valid = false;

  NativeProjectionScope(ProjectionPath selected, const void* source)
      : path(selected) {
    native_scope = this;
    if (source == nullptr) return;
    // Only the publication lock is taken, not the execution lock, so a
    // reconstruction running elsewhere never makes these workers skip jitter.
    const camera_state::PublicationWriterGuard publication_guard;
    frame = camera_state::GetForSceneLocked();
    if (!frame.valid || !frame.camera_matrix_valid
        || !state::IsEnabled()
        || frame.scene == 0u
        || frame.reset_generation != camera_state::reset_generation.load(std::memory_order_relaxed)) return;
    const auto* bytes = static_cast<const uint8_t*>(source);
    const auto* p = reinterpret_cast<const float*>(bytes + 0x280u);
    const auto* v = reinterpret_cast<const float*>(bytes + 0x2C0u);
    const void* camera = *reinterpret_cast<void* const*>(bytes + 0x570u);
    if (camera == nullptr || reinterpret_cast<uintptr_t>(camera) != frame.camera_identity) return;
    if (reinterpret_cast<uintptr_t>(source) != frame.viewport_identity || (bytes[0x6E2u] & 1u) == 0u) return;
    const uint32_t width = *reinterpret_cast<const uint32_t*>(bytes + 0x5D8u);
    const uint32_t height = *reinterpret_cast<const uint32_t*>(bytes + 0x5DCu);
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
    jitter = {frame.jitter_uv_x, frame.jitter_uv_y};
    valid = std::isfinite(jitter[0]) && std::isfinite(jitter[1]) && (jitter[0] != 0.f || jitter[1] != 0.f);
  }
  NativeProjectionScope(const NativeProjectionScope&) = delete;
  NativeProjectionScope& operator=(const NativeProjectionScope&) = delete;
  ~NativeProjectionScope() { native_scope = previous; }

  // True while the scope's camera still belongs to the current scene and no
  // history reset has happened. Called under the publication lock.
  [[nodiscard]] bool CurrentLocked() const {
    const auto owner = engine::views::GetScope().owner;
    return valid && state::IsEnabled()
           && frame.scene == owner.scene && frame.owner_generation == owner.generation
           && frame.reset_generation == camera_state::reset_generation.load(std::memory_order_relaxed);
  }
};

// Adds twice the UV jitter times W to each float4's x and y, which shifts clip
// space without changing Z or W. Matrices are four float4s; spherical-harmonics
// outputs are up to eighteen. Every result is checked before anything is written.
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

// Jitters a matrix the engine built from the scope's projection (times its
// view, unless projection_only). The matrix must equal the expected unjittered
// value; a matrix that is already jittered or unrelated is left alone.
inline bool CorrectNativeMatrix(float* matrix, const NativeProjectionScope& scope, bool projection_only = false) {
  if (!scope.valid || matrix == nullptr) return false;
  const camera_state::PublicationWriterGuard publication_guard;
  if (!scope.CurrentLocked()) return false;
  alignas(16) std::array<float, 16> expected = scope.projection;
  if (!projection_only) {
    // Sum in the same pairwise order as the engine's SSE code, so a tight
    // tolerance works. A tolerance scaled to world coordinates could hide a
    // whole sub-pixel offset.
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

// Identifies the sun pass recording on this thread, or 0 outside one. A pass
// shades one directional light against one depth image, with up to four draws.
// Screen-space shadows use it to group the draws of one pass.
inline thread_local uint64_t sun_pass_count = 0u;
inline thread_local uint64_t active_sun_pass = 0u;

inline void __fastcall HookSunPass(void* renderer, void* viewport, uint32_t a, uint32_t b, uint8_t c, uint8_t d) {
  const NativeProjectionScope scope(ProjectionPath::SUN_VOLUME, viewport);
  active_sun_pass = ++sun_pass_count;
  sun_pass(renderer, viewport, a, b, c, d);
  active_sun_pass = 0u;
}

inline void __fastcall HookSunVolume(void* context, const float* vp, const void* volume, const void* color,
                                     void* vertices, void* indices) {
  alignas(16) std::array<float, 16> copy = {};
  if (_ReturnAddress() == sun_shadow_box_return_address && native_scope != nullptr
      && native_scope->path == ProjectionPath::SUN_VOLUME && vp != nullptr) {
    std::memcpy(copy.data(), vp, 64u);
    if (CorrectNativeMatrix(copy.data(), *native_scope)) vp = copy.data();
  }
  // The native helper finishes the upload and draw before this copy dies.
  sun_volume(context, vp, volume, color, vertices, indices);
}

inline uint32_t __fastcall HookShFallback(void* renderer, uint32_t kind, float* output, float* min_w,
                                          float* max_w, void* shape, void* viewport) {
  const bool qualified = _ReturnAddress() == sh_light_volume_return_address && kind < SH_VERTEX_COUNTS.size();
  const NativeProjectionScope scope(ProjectionPath::SH_LIGHTING, qualified ? viewport : nullptr);
  const uint32_t success = sh_fallback(renderer, kind, output, min_w, max_w, shape, viewport);
  if (success != 0u && scope.valid) {
    const camera_state::PublicationWriterGuard publication_guard;
    if (scope.CurrentLocked()) {
      TranslateClip(output, SH_VERTEX_COUNTS[kind], scope.jitter);
    }
  }
  return success;  // A success flag, not a vertex count. Bounds, Z and W are unchanged.
}

inline void __fastcall HookShProducer(void* renderer, void* scene, void* viewport, void* inputs) {
  const NativeProjectionScope scope(ProjectionPath::SH_LIGHTING, viewport);
  sh_producer(renderer, scene, viewport, inputs);
}

inline void __fastcall HookShAttach(void* job, void* packet, uint64_t c, uint64_t d) {
  if (_ReturnAddress() == sh_set_output_return_address && packet != nullptr && native_scope != nullptr
      && native_scope->path == ProjectionPath::SH_LIGHTING
      && std::memcmp(static_cast<const uint8_t*>(packet) + 0x120u, native_scope->view.data(), 64u) == 0) {
    // Both SH workers retain this packet. ExecuteDrawLight derives its clip
    // vertices and inverse light-volume coordinates from the same render P/V.
    // Correct it before publication so the two calculations use one sample.
    CorrectNativeMatrix(reinterpret_cast<float*>(static_cast<uint8_t*>(packet) + 0x160u), *native_scope, true);
  }
  sh_attach(job, packet, c, d);
}

inline void* __fastcall HookSkyParameters(void* plugin, uint32_t* result, void* viewport, void* parameters) {
  const bool qualified = _ReturnAddress() == sky_parameters_return_address && result != nullptr && parameters != nullptr;
  const NativeProjectionScope scope(ProjectionPath::SKY, qualified ? viewport : nullptr);
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
  const NativeProjectionScope scope(ProjectionPath::TERRAIN_DECAL, viewport);
  terrain_producer(a, b, scene, viewport, e);
}

inline void __fastcall HookTerrainAttach(void* command, void* packet, uint64_t c, uint64_t d) {
  if (_ReturnAddress() == terrain_set_input_return_address && packet != nullptr && native_scope != nullptr
      && native_scope->path == ProjectionPath::TERRAIN_DECAL) {
    CorrectNativeMatrix(reinterpret_cast<float*>(static_cast<uint8_t*>(packet) + 0x160u), *native_scope, true);
  }
  // The engine's packet is corrected in place and passed on; no
  // temporary copy reaches the engine.
  terrain_attach(command, packet, c, d);
}

inline bool Attach() {
  const std::scoped_lock lifecycle_guard(lifecycle_mutex);
  if (installed.load(std::memory_order_acquire)) return true;

  camera_state::Invalidate();

  if (!engine::Verified(REQUIRED_ENTRIES)) {
    logging::Warn("the native projection entries did not verify; camera jitter is off");
    return false;
  }
  using engine::Address;
  using engine::Entry;
  set_view_matrix_state = reinterpret_cast<SetViewMatrixState>(Address(Entry::SHADER_MANAGER_SET_VIEW_MATRIX));
  shader_manager_global = reinterpret_cast<void**>(Address(Entry::SHADER_MANAGER));
  expected_return_address = Address(Entry::RENDER_SET_VIEW_RETURN);
  velocity_return_address = Address(Entry::DEFERRED_GEOMETRY_SET_VIEW_RETURN);
  model_return_address = Address(Entry::MODEL_SET_VIEW_RETURN);
  alpha_model_return_address = Address(Entry::ALPHA_MODEL_SET_VIEW_RETURN);
  overlay_model_return_address = Address(Entry::OVERLAY_MODEL_SET_VIEW_RETURN);
  new_thermography_return_address = Address(Entry::NEW_THERMOGRAPHY_SET_VIEW_RETURN);
  thermography_return_address = Address(Entry::THERMOGRAPHY_SET_VIEW_RETURN);
  forward_rendering = reinterpret_cast<RenderPluginCallback>(Address(Entry::FORWARD_RENDERING_PRE_EXEC));
  request_local_light_draws = reinterpret_cast<RequestLocalLightDraws>(Address(Entry::REQUEST_DRAW_ALL_LOCAL_LIGHT_ON_JOB));
  local_light_request_return_address = Address(Entry::LOCAL_LIGHT_REQUEST_RETURN);
  flush_constants = reinterpret_cast<FlushConstants>(Address(Entry::SHADOW_CONSTANT_REGISTER_FLUSH));
  motion_upload_return_address = Address(Entry::MODEL_CONSTANT_UPLOAD_RETURN);
  upload_constants = reinterpret_cast<UploadConstants>(Address(Entry::CONSTANT_BUFFER_MAP_DATA));
  scatter_upload_return_address = Address(Entry::SCATTER_UPLOAD_RETURN);
  // These points are safe to edit at: the SH packet still belongs to its
  // producer when it is attached, and the sky helper finishes its inverse
  // matrices before MainExec uploads them for the draw.
  sun_pass = reinterpret_cast<SunPass>(Address(Entry::SUN_LIGHT_DRAW));
  sun_volume = reinterpret_cast<SunVolume>(Address(Entry::DRAW_SHADOW_BOX));
  sun_shadow_box_return_address = Address(Entry::SUN_SHADOW_BOX_RETURN);
  sh_fallback = reinterpret_cast<ShFallback>(Address(Entry::SPHERICAL_HARMONICS_CALCULATE_LIGHT_VOLUME));
  sh_light_volume_return_address = Address(Entry::SPHERICAL_HARMONICS_LIGHT_VOLUME_RETURN);
  sh_producer = reinterpret_cast<ShProducer>(Address(Entry::SPHERICAL_HARMONICS_REGISTER_DRAW_ON_JOB));
  sh_attach = reinterpret_cast<AttachPacket>(Address(Entry::SPHERICAL_HARMONICS_SET_OUTPUT));
  sh_set_output_return_address = Address(Entry::SPHERICAL_HARMONICS_SET_OUTPUT_RETURN);
  sky_parameters = reinterpret_cast<SkyParameters>(Address(Entry::SKY_CALC_SHADER_PARAMETERS));
  sky_parameters_return_address = Address(Entry::SKY_PARAMETERS_RETURN);
  terrain_producer = reinterpret_cast<TerrainProducer>(Address(Entry::TERRAIN_DECAL_DRAW_ON_JOB));
  terrain_attach = reinterpret_cast<AttachPacket>(Address(Entry::TERRAIN_DECAL_SET_INPUT));
  terrain_set_input_return_address = Address(Entry::TERRAIN_DECAL_SET_INPUT_RETURN);
  // The hooks are never removed, because a game thread could still be entering
  // one. Pin the addon so unloading it cannot free their code either. Install
  // at the first device creation, before scene rendering starts.
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
  if (DetourAttach(reinterpret_cast<void**>(&flush_constants), reinterpret_cast<void*>(HookFlushConstants)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&upload_constants), reinterpret_cast<void*>(HookUploadConstants)) != NO_ERROR) {
    DetourTransactionAbort();
    return false;
  }
  if (DetourAttach(reinterpret_cast<void**>(&sun_pass), reinterpret_cast<void*>(HookSunPass)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&sun_volume), reinterpret_cast<void*>(HookSunVolume)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&sh_fallback), reinterpret_cast<void*>(HookShFallback)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&sh_producer), reinterpret_cast<void*>(HookShProducer)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&sh_attach), reinterpret_cast<void*>(HookShAttach)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&sky_parameters), reinterpret_cast<void*>(HookSkyParameters)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&terrain_producer), reinterpret_cast<void*>(HookTerrainProducer)) != NO_ERROR
      || DetourAttach(reinterpret_cast<void**>(&terrain_attach), reinterpret_cast<void*>(HookTerrainAttach)) != NO_ERROR) {
    DetourTransactionAbort();
    return false;
  }
  if (DetourTransactionCommit() != NO_ERROR) {
    DetourTransactionAbort();
    set_view_matrix_state = nullptr;
    forward_rendering = nullptr;
    request_local_light_draws = nullptr;
    return false;
  }

  installed.store(true, std::memory_order_release);
  logging::Info("installed native projection hook commit=",
                logging::Hex{reinterpret_cast<uintptr_t>(engine::Address(engine::Entry::PROJECTION_COMMIT))},
                " helper=", logging::Hex{reinterpret_cast<uintptr_t>(engine::Address(engine::Entry::SHADER_MANAGER_SET_VIEW_MATRIX))},
                " return=", logging::Hex{reinterpret_cast<uintptr_t>(expected_return_address)},
                " forward=", logging::Hex{reinterpret_cast<uintptr_t>(forward_rendering)},
                " local_light_packets=", logging::Hex{reinterpret_cast<uintptr_t>(request_local_light_draws)},
                " shader_manager_global=", logging::Hex{reinterpret_cast<uintptr_t>(shader_manager_global)});
  return true;
}

// Stops new jitter at device teardown and DLL detach. With transition_runtime it turns
// temporal reconstruction off and invalidates the published camera. The hooks
// themselves stay installed.
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

  // The hooks and trampolines stay until the process exits. A zero count of
  // calls in progress would not stop a later native call from entering, so
  // nothing is waited for, unpatched, cleared or freed here.
}

inline void OnInitDevice(reshade::api::device* device) {
  (void)device;
  Attach();
}

// Tags the character-motion vertex and pixel shaders by bytecode hash, for the
// precision fix in HookFlushConstants.
inline void OnInitPipeline(reshade::api::device* device, reshade::api::pipeline_layout /*layout*/,
                           uint32_t count, const reshade::api::pipeline_subobject* objects,
                           reshade::api::pipeline pipeline) {
  if (device->get_api() != reshade::api::device_api::d3d11 || pipeline.handle == 0u || count != 1u
      || objects[0].data == nullptr) return;
  const bool vertex = objects[0].type == reshade::api::pipeline_subobject_type::vertex_shader;
  if (!vertex && objects[0].type != reshade::api::pipeline_subobject_type::pixel_shader) return;
  const auto& shader = *static_cast<const reshade::api::shader_desc*>(objects[0].data);
  if (shader.code == nullptr || shader.code_size == 0u) return;
  const uint32_t hash = renodx::utils::hash::ComputeCRC32(static_cast<const uint8_t*>(shader.code), shader.code_size);
  VelocityShaderKind kind = VelocityShaderKind::NONE;
  if (vertex) {
    switch (hash) {
      case 0x1D2531B7u: kind = VelocityShaderKind::OPAQUE_MOTION; break;
      case 0x7B809E72u: kind = VelocityShaderKind::MASKED_MOTION; break;
      default:          return;
    }
  } else {
    switch (hash) {
      case 0x9815404Fu: kind = VelocityShaderKind::OPAQUE_MOTION; break;
      case 0x58C10658u: kind = VelocityShaderKind::MASKED_MOTION; break;
      default:          return;
    }
  }
  auto* shader_object = reinterpret_cast<ID3D11DeviceChild*>(pipeline.handle);  // NOLINT(performance-no-int-to-ptr)
  shader_object->SetPrivateData(VELOCITY_SHADER_GUID, sizeof(kind), &kind);
}

inline void Use(DWORD reason) {
  if (reason == DLL_PROCESS_ATTACH) {
    reshade::register_event<reshade::addon_event::init_device>(OnInitDevice);
    reshade::register_event<reshade::addon_event::init_pipeline>(OnInitPipeline);
  } else if (reason == DLL_PROCESS_DETACH) {
    reshade::unregister_event<reshade::addon_event::init_device>(OnInitDevice);
    reshade::unregister_event<reshade::addon_event::init_pipeline>(OnInitPipeline);
    // DllMain holds the loader lock. Process termination reclaims pinned code;
    // never run a Detours transaction or wait for native work here.
    StopAdmission(false, false);
  }
}

}  // namespace mgsv::reconstruction::projection_jitter
