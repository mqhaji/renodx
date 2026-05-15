#pragma once

/*
 * CPU-side projection jitter injection for MGSV's scene cbuffers.
 *
 * MGSV's `cVSScene`/`cPSScene` struct holds m_projectionView (offset 0) and
 * m_projection (offset 64) along with several other matrices we leave alone.
 * The struct is 480 bytes and is bound to vertex b2 and pixel b2 across many
 * passes via separate D3D11 cbuffer resources.
 *
 * Strategy mirrors Alias Isolation's CPU side more closely:
 *  - Known velocity passes still tag their b2 resources for logging/validation.
 *  - Map/unmap handling no longer depends exclusively on those tags, because
 *    MGSV often maps dynamic/ring cVSScene buffers before the draw that first
 *    identifies them. Instead, any 480-byte buffer with a plausible CbScene
 *    layout can be patched immediately on unmap.
 *  - Jittered matrix variants are cached from no-jitter inputs so repeated maps
 *    of the same resource/sample cannot accumulate the offset.
 *
 * Jitter is stored in UV space by constant_buffers and doubled only at matrix
 * patch time. The compute resolve samples MGSV's velocity unchanged; no jitter
 * cbuffer is bound to the TAA shader.
 */

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include <include/reshade.hpp>

#include "./constant_buffers.hpp"
#include "./descriptor_tracker.hpp"
#include "./logging.hpp"

namespace taa::jitter {

struct Matrix4 {
  float m[4][4] = {};
};

struct Float4 {
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
  float w = 0.f;
};

// Layout mirrors the `cVSScene` struct disassembly. Matrices are column-major
// per MGSV's shader compilation (cb2[4..7] => m_projection columns).
struct CbScene {
  Matrix4 m_projection_view;        // offset    0
  Matrix4 m_projection;             // offset   64
  Matrix4 m_view;                   // offset  128
  Matrix4 m_shadow_projection;      // offset  192
  Matrix4 m_shadow_projection2;     // offset  256
  Float4 m_eyepos;                  // offset  320
  Float4 m_projection_param;        // offset  336
  Float4 m_viewport_size;           // offset  352
  Float4 m_exposure;                // offset  368
  Float4 m_fog_param[3];            // offset  384
  Float4 m_fog_color;               // offset  432
  Float4 m_camera_center_offset;    // offset  448
  Float4 m_shadow_map_resolutions;  // offset 464
};

static_assert(sizeof(CbScene) == 480, "CbScene layout must be 480 bytes");

inline constexpr uint64_t CBSCENE_BYTES = 480u;

enum class BufferSource : uint8_t {
  UNKNOWN,
  TRACKED_VELOCITY,
  SIZE_HEURISTIC,
};

struct MappedBuffer {
  uint8_t* data = nullptr;
  uint64_t size = 0u;
  uint64_t resource_handle = 0u;
  BufferSource source = BufferSource::UNKNOWN;
  bool tracked = false;
};

struct MatrixState {
  // Kept without jitter, like Alias Isolation, so future velocity/shadow fixes
  // can compose precise matrices instead of reading already-jittered cbuffers.
  Matrix4 current_projection_view_no_jitter = {};
  Matrix4 previous_projection_view_no_jitter = {};
  bool has_current_projection_view = false;
  bool has_previous_projection_view = false;
};

struct SceneCache {
  Matrix4 projection_no_jitter = {};
  Matrix4 projection_view_no_jitter = {};
  Matrix4 projection_jittered = {};
  Matrix4 projection_view_jittered = {};
  uint32_t sample_index = std::numeric_limits<uint32_t>::max();
  float projection_jitter_x = 0.f;
  float projection_jitter_y = 0.f;
  bool valid = false;
};

struct RenderState {
  uint32_t screen_width = 0u;
  uint32_t screen_height = 0u;
  uint32_t rt_width = 0u;
  uint32_t rt_height = 0u;
  float viewport_width = 0.f;
  float viewport_height = 0.f;
  bool has_render_target = false;
  bool has_viewport = false;
};

struct DebugState {
  uint64_t frame_index = 0u;
  uint32_t applied_count = 0u;
  uint32_t skipped_count = 0u;
  uint64_t last_resource_handle = 0u;
  uint32_t last_rt_width = 0u;
  uint32_t last_rt_height = 0u;
  float last_viewport_width = 0.f;
  float last_viewport_height = 0.f;
  float last_jitter_uv_x = 0.f;
  float last_jitter_uv_y = 0.f;
  float last_jitter_pixels_x = 0.f;
  float last_jitter_pixels_y = 0.f;
  float last_projection_jitter_x = 0.f;
  float last_projection_jitter_y = 0.f;
  uint64_t last_mapped_size = 0u;
  uint32_t tracked_count = 0u;
  uint32_t mapped_count = 0u;
  uint32_t heuristic_mapped_count = 0u;
  bool last_tracked = false;
  bool last_camera_like = false;
  bool last_fullscreen = false;
};

inline std::unordered_map<uint64_t, MappedBuffer> mapped_buffers;
inline std::unordered_map<uint64_t, SceneCache> scene_caches;
inline std::unordered_set<uint64_t> tracked_scene_cbuffers;
inline RenderState render_state = {};
inline MatrixState matrix_state = {};
inline DebugState debug_state = {};
inline std::mutex state_mutex;
inline uint64_t last_apply_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_skip_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_swapchain_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_track_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_capture_cb_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_map_log = std::numeric_limits<uint64_t>::max();

inline bool LogEvery(uint64_t& last_frame, uint64_t interval = 120u) {
  return logging::ShouldLogFrame(constant_buffers::frame_state.frame_index, last_frame, interval);
}

inline void ResetDebugForFrameLocked() {
  if (debug_state.frame_index == constant_buffers::frame_state.frame_index) return;
  debug_state.frame_index = constant_buffers::frame_state.frame_index;
  debug_state.applied_count = 0u;
  debug_state.skipped_count = 0u;
  debug_state.heuristic_mapped_count = 0u;
  debug_state.last_fullscreen = false;
}

inline DebugState GetDebugState() {
  std::lock_guard<std::mutex> lock(state_mutex);
  ResetDebugForFrameLocked();
  return debug_state;
}

inline void TrackSceneBuffer(reshade::api::buffer_range range, const char* source) {
  if (range.buffer.handle == 0u) return;

  bool should_log = false;
  {
    std::lock_guard<std::mutex> lock(state_mutex);
    const bool inserted = tracked_scene_cbuffers.insert(range.buffer.handle).second;
    debug_state.tracked_count = static_cast<uint32_t>(tracked_scene_cbuffers.size());
    should_log = inserted && logging::ShouldLogFrame(constant_buffers::frame_state.frame_index, last_track_log, 120u);
  }

  if (should_log) {
    logging::Info("tracking CbScene ", source, " resource=", logging::Hex{range.buffer.handle});
  }
}

inline void CaptureConstantBuffers(
    const descriptor_tracker::CommandListData& data,
    bool is_camera_velocity_ps,
    bool is_gbuffer_velocity_vs) {
  if (!constant_buffers::IsEnabled()) return;

  if ((is_camera_velocity_ps || is_gbuffer_velocity_vs)) {
    bool should_log = false;
    {
      std::lock_guard<std::mutex> lock(state_mutex);
      should_log = logging::ShouldLogFrame(constant_buffers::frame_state.frame_index, last_capture_cb_log, 30u);
    }
    if (should_log) {
      logging::Info("CbScene capture candidates frame=", constant_buffers::frame_state.frame_index,
                    " camera_velocity=", logging::Bool{is_camera_velocity_ps},
                    " gbuffer_velocity=", logging::Bool{is_gbuffer_velocity_vs},
                    " vertex_b2=", logging::Hex{data.vertex_cb_b2.buffer.handle},
                    " vertex_b2_size=", data.vertex_cb_b2.size,
                    " pixel_b2=", logging::Hex{data.pixel_cb_b2.buffer.handle},
                    " pixel_b2_size=", data.pixel_cb_b2.size);
    }
  }

  // Snapshot-verified: MotionBlurCameraVelocity_ps uses cPSScene at pixel b2.
  // Its VS is fullscreen and does not use cVSScene, but track vertex b2 too for
  // safety in case a variant binds it.
  if (is_camera_velocity_ps) {
    TrackSceneBuffer(data.pixel_cb_b2, "camera_ps_b2");
    TrackSceneBuffer(data.vertex_cb_b2, "camera_vs_b2");
  }

  // Snapshot-verified: GBufferVelocity_vs and GBufferMaskedVelocity_vs use
  // cVSScene at vertex b2 for the current projection. Without jittering this
  // buffer the velocity pass is unjittered, history reprojection rejects/clips
  // heavily, and the resolve behaves like a soft current-frame filter.
  if (is_gbuffer_velocity_vs) {
    TrackSceneBuffer(data.vertex_cb_b2, "gbuffer_velocity_vs_b2");
  }
}

inline bool IsTrackedSceneCbuffer(reshade::api::resource resource) {
  if (resource.handle == 0u) return false;
  std::lock_guard<std::mutex> lock(state_mutex);
  return tracked_scene_cbuffers.contains(resource.handle);
}

inline const char* BufferSourceName(BufferSource source) {
  switch (source) {
    case BufferSource::TRACKED_VELOCITY:
      return "tracked_velocity";
    case BufferSource::SIZE_HEURISTIC:
      return "size_heuristic";
    default:
      return "unknown";
  }
}

inline Matrix4 Identity() {
  Matrix4 result = {};
  for (uint32_t i = 0u; i < 4u; ++i) result.m[i][i] = 1.f;
  return result;
}

inline Matrix4 JitterAdd(float x, float y) {
  Matrix4 result = Identity();
  result.m[0][3] = x;
  result.m[1][3] = y;
  return result;
}

inline Matrix4 Multiply(const Matrix4& a, const Matrix4& b) {
  Matrix4 result = {};
  for (uint32_t col = 0u; col < 4u; ++col) {
    for (uint32_t row = 0u; row < 4u; ++row) {
      float value = 0.f;
      for (uint32_t k = 0u; k < 4u; ++k) {
        value += a.m[k][row] * b.m[col][k];
      }
      result.m[col][row] = value;
    }
  }
  return result;
}

inline bool MatrixEqual(const Matrix4& a, const Matrix4& b) {
  return std::memcmp(&a, &b, sizeof(Matrix4)) == 0;
}

inline bool IsFiniteFloat4(const Float4& value) {
  return std::isfinite(value.x) && std::isfinite(value.y)
         && std::isfinite(value.z) && std::isfinite(value.w);
}

inline bool IsFiniteMatrix(const Matrix4& matrix) {
  for (uint32_t col = 0u; col < 4u; ++col) {
    for (uint32_t row = 0u; row < 4u; ++row) {
      if (!std::isfinite(matrix.m[col][row])) return false;
    }
  }
  return true;
}

inline float Vec3Distance(float ax, float ay, float az, float bx, float by, float bz) {
  const float dx = ax - bx;
  const float dy = ay - by;
  const float dz = az - bz;
  return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
}

inline bool LooksLikeSceneBuffer(const CbScene& scene) {
  return IsFiniteMatrix(scene.m_projection_view)
         && IsFiniteMatrix(scene.m_projection)
         && IsFiniteMatrix(scene.m_view)
         && IsFiniteMatrix(scene.m_shadow_projection)
         && IsFiniteMatrix(scene.m_shadow_projection2)
         && IsFiniteFloat4(scene.m_eyepos)
         && IsFiniteFloat4(scene.m_projection_param)
         && IsFiniteFloat4(scene.m_viewport_size)
         && IsFiniteFloat4(scene.m_camera_center_offset)
         && !(MatrixEqual(scene.m_projection, Matrix4{}) && MatrixEqual(scene.m_projection_view, Matrix4{}));
}

inline bool LooksLikeCameraScene(const CbScene& scene) {
  // Same guard used by Alias Isolation: reconstruct camera position from the
  // view matrix and compare it with the cbuffer camera position. Shadow/reflection
  // scene buffers often share the same layout but do not pass this test.
  const float tx = -scene.m_view.m[0][3];
  const float ty = -scene.m_view.m[1][3];
  const float tz = -scene.m_view.m[2][3];
  const float view_camera_x = (scene.m_view.m[0][0] * tx) + (scene.m_view.m[1][0] * ty) + (scene.m_view.m[2][0] * tz);
  const float view_camera_y = (scene.m_view.m[0][1] * tx) + (scene.m_view.m[1][1] * ty) + (scene.m_view.m[2][1] * tz);
  const float view_camera_z = (scene.m_view.m[0][2] * tx) + (scene.m_view.m[1][2] * ty) + (scene.m_view.m[2][2] * tz);
  return Vec3Distance(view_camera_x, view_camera_y, view_camera_z,
                      scene.m_eyepos.x, scene.m_eyepos.y, scene.m_eyepos.z)
         < 0.05f;
}

// P * J, matching the Alien Isolation jitter path. The cbuffer stores matrices
// as columns and the shader reconstructs clip coordinates via dp4 with those
// columns. To add clip-space jitter before the perspective divide, add the w
// column into the x/y columns instead of writing translation slots directly:
//   x' = x + jx * w
//   y' = y + jy * w
inline void ApplyJitterInPlace(Matrix4& matrix, float jx, float jy) {
  matrix = Multiply(matrix, JitterAdd(jx, jy));
}

inline bool IsFullscreenPass() {
  if (render_state.screen_width == 0u || render_state.screen_height == 0u) return false;
  if (!render_state.has_render_target || !render_state.has_viewport) return false;
  if (render_state.rt_width != render_state.screen_width
      || render_state.rt_height != render_state.screen_height) return false;

  const float w = static_cast<float>(render_state.screen_width);
  const float h = static_cast<float>(render_state.screen_height);
  return std::abs(render_state.viewport_width - w) < 0.5f
         && std::abs(render_state.viewport_height - h) < 0.5f;
}

inline std::array<float, 2> GetFrameJitter() {
  return constant_buffers::CurrentFrameJitter(render_state.screen_width, render_state.screen_height);
}

inline void ApplySceneJitterCached(CbScene& scene, uint64_t resource_handle, float projection_jitter_x, float projection_jitter_y) {
  auto& cache = scene_caches[resource_handle];
  const uint32_t sample_index = constant_buffers::frame_state.taa_sample_index;

  if (cache.valid
      && cache.sample_index == sample_index
      && cache.projection_jitter_x == projection_jitter_x
      && cache.projection_jitter_y == projection_jitter_y
      && MatrixEqual(scene.m_projection, cache.projection_jittered)
      && MatrixEqual(scene.m_projection_view, cache.projection_view_jittered)) {
    // Already patched from this exact no-jitter source this frame/sample.
    scene.m_projection = cache.projection_jittered;
    scene.m_projection_view = cache.projection_view_jittered;
    return;
  }

  const bool needs_rebuild = !cache.valid
                             || cache.sample_index != sample_index
                             || cache.projection_jitter_x != projection_jitter_x
                             || cache.projection_jitter_y != projection_jitter_y
                             || !MatrixEqual(cache.projection_no_jitter, scene.m_projection)
                             || !MatrixEqual(cache.projection_view_no_jitter, scene.m_projection_view);

  if (needs_rebuild) {
    cache.valid = true;
    cache.sample_index = sample_index;
    cache.projection_jitter_x = projection_jitter_x;
    cache.projection_jitter_y = projection_jitter_y;
    cache.projection_no_jitter = scene.m_projection;
    cache.projection_view_no_jitter = scene.m_projection_view;
    cache.projection_jittered = Multiply(scene.m_projection, JitterAdd(projection_jitter_x, projection_jitter_y));
    cache.projection_view_jittered = Multiply(scene.m_projection_view, JitterAdd(projection_jitter_x, projection_jitter_y));

    matrix_state.current_projection_view_no_jitter = cache.projection_view_no_jitter;
    matrix_state.has_current_projection_view = true;
  }

  scene.m_projection = cache.projection_jittered;
  scene.m_projection_view = cache.projection_view_jittered;
}

inline bool ApplyMappedBuffer(const MappedBuffer& mapped) {
  if (mapped.data == nullptr || mapped.size < CBSCENE_BYTES) return false;
  if (!constant_buffers::IsEnabled()) return false;

  auto* scene = reinterpret_cast<CbScene*>(mapped.data);
  if (!LooksLikeSceneBuffer(*scene)) return false;

  const bool fullscreen = IsFullscreenPass();
  const bool camera_like = LooksLikeCameraScene(*scene);

  ResetDebugForFrameLocked();
  debug_state.last_resource_handle = mapped.resource_handle;
  debug_state.last_rt_width = render_state.rt_width;
  debug_state.last_rt_height = render_state.rt_height;
  debug_state.last_viewport_width = render_state.viewport_width;
  debug_state.last_viewport_height = render_state.viewport_height;
  debug_state.last_tracked = mapped.tracked;
  debug_state.last_camera_like = camera_like;

  // Prefer the same fullscreen gate as Alien Isolation, but also allow camera-
  // looking CbScene maps when MGSV maps the ring-buffer before the relevant RT
  // binding event reaches us.
  if (!fullscreen && !camera_like) {
    debug_state.skipped_count += 1u;
    debug_state.last_fullscreen = false;
    if (LogEvery(last_skip_log)) {
      logging::Info("skipping CbScene jitter outside fullscreen pass rt=",
                    render_state.rt_width, "x", render_state.rt_height,
                    " viewport=", render_state.viewport_width, "x", render_state.viewport_height,
                    " screen=", render_state.screen_width, "x", render_state.screen_height);
    }
    return false;
  }

  const auto offset = GetFrameJitter();
  const float projection_jitter_x = offset[0] * 2.f;
  const float projection_jitter_y = offset[1] * 2.f;
  debug_state.applied_count += 1u;
  debug_state.last_fullscreen = fullscreen;
  debug_state.last_jitter_uv_x = offset[0];
  debug_state.last_jitter_uv_y = offset[1];
  debug_state.last_jitter_pixels_x = offset[0] * static_cast<float>(render_state.screen_width);
  debug_state.last_jitter_pixels_y = offset[1] * static_cast<float>(render_state.screen_height);
  debug_state.last_projection_jitter_x = projection_jitter_x;
  debug_state.last_projection_jitter_y = projection_jitter_y;
  ApplySceneJitterCached(*scene, mapped.resource_handle, projection_jitter_x, projection_jitter_y);
  return true;
}

inline void Reset() {
  std::lock_guard<std::mutex> lock(state_mutex);
  mapped_buffers.clear();
  scene_caches.clear();
  tracked_scene_cbuffers.clear();
  render_state = {};
  matrix_state = {};
  debug_state = {};
}

inline void FinishFrame() {
  std::lock_guard<std::mutex> lock(state_mutex);
  if (matrix_state.has_current_projection_view) {
    matrix_state.previous_projection_view_no_jitter = matrix_state.current_projection_view_no_jitter;
    matrix_state.has_previous_projection_view = true;
  }
}

inline void OnInitSwapchain(reshade::api::swapchain* swapchain, bool resize) {
  (void)resize;
  if (swapchain == nullptr) return;
  auto* device = swapchain->get_device();
  if (device == nullptr) return;

  const auto back_buffer = swapchain->get_back_buffer(0);
  if (back_buffer.handle == 0u) return;

  const auto desc = device->get_resource_desc(back_buffer);
  const uint32_t screen_width = desc.texture.width;
  const uint32_t screen_height = desc.texture.height;
  bool should_log = false;
  {
    std::lock_guard<std::mutex> lock(state_mutex);
    render_state.screen_width = screen_width;
    render_state.screen_height = screen_height;
    should_log = logging::ShouldLogFrame(constant_buffers::frame_state.frame_index, last_swapchain_log, 1u);
  }

  if (should_log) {
    logging::Info("jitter screen size ", screen_width, "x", screen_height);
  }
}

inline void OnDestroySwapchain(reshade::api::swapchain* swapchain, bool resize) {
  (void)swapchain;
  (void)resize;
  std::lock_guard<std::mutex> lock(state_mutex);
  render_state.screen_width = 0u;
  render_state.screen_height = 0u;
  render_state.has_render_target = false;
  render_state.has_viewport = false;
}

inline void OnBindRenderTargetsAndDepthStencil(
    reshade::api::command_list* cmd_list,
    uint32_t count,
    const reshade::api::resource_view* rtvs,
    reshade::api::resource_view dsv) {
  (void)dsv;
  if (cmd_list == nullptr || count == 0u || rtvs == nullptr || rtvs[0].handle == 0u) {
    std::lock_guard<std::mutex> lock(state_mutex);
    render_state.has_render_target = false;
    return;
  }
  auto* device = cmd_list->get_device();
  if (device == nullptr) return;

  const auto resource = device->get_resource_from_view(rtvs[0]);
  if (resource.handle == 0u) {
    std::lock_guard<std::mutex> lock(state_mutex);
    render_state.has_render_target = false;
    return;
  }
  const auto desc = device->get_resource_desc(resource);
  std::lock_guard<std::mutex> lock(state_mutex);
  render_state.rt_width = desc.texture.width;
  render_state.rt_height = desc.texture.height;
  render_state.has_render_target = true;
}

inline void OnBindViewports(
    reshade::api::command_list* cmd_list,
    uint32_t first,
    uint32_t count,
    const reshade::api::viewport* viewports) {
  (void)cmd_list;
  if (viewports == nullptr || count == 0u || first != 0u) {
    std::lock_guard<std::mutex> lock(state_mutex);
    render_state.has_viewport = false;
    return;
  }
  std::lock_guard<std::mutex> lock(state_mutex);
  render_state.viewport_width = viewports[0].width;
  render_state.viewport_height = viewports[0].height;
  render_state.has_viewport = true;
}

inline void OnMapBufferRegion(
    reshade::api::device* device,
    reshade::api::resource resource,
    uint64_t offset,
    uint64_t size,
    reshade::api::map_access access,
    void** mapped_data) {
  (void)access;
  if (device == nullptr || mapped_data == nullptr || *mapped_data == nullptr) return;

  const auto desc = device->get_resource_desc(resource);
  uint64_t resolved_size = size;
  if (resolved_size == UINT64_MAX) {
    resolved_size = desc.buffer.size > offset ? desc.buffer.size - offset : 0u;
  }

  // MGSV's cVSScene/cPSScene layout is exactly 480 bytes. Accept exact maps and
  // ReShade's "rest of buffer" maps over a 480-byte resource; reject larger
  // aggregate buffers so we do not write into unrelated cbuffer arrays.
  const bool is_scene_sized = resolved_size == CBSCENE_BYTES || desc.buffer.size == CBSCENE_BYTES;
  if (!is_scene_sized || resolved_size < CBSCENE_BYTES) return;

  std::lock_guard<std::mutex> lock(state_mutex);
  const bool tracked = tracked_scene_cbuffers.contains(resource.handle);
  const BufferSource source = tracked ? BufferSource::TRACKED_VELOCITY : BufferSource::SIZE_HEURISTIC;
  ResetDebugForFrameLocked();
  mapped_buffers[resource.handle] = MappedBuffer{
      .data = static_cast<uint8_t*>(*mapped_data) + offset,
      .size = resolved_size,
      .resource_handle = resource.handle,
      .source = source,
      .tracked = tracked,
  };
  debug_state.last_resource_handle = resource.handle;
  debug_state.last_mapped_size = resolved_size;
  debug_state.mapped_count = static_cast<uint32_t>(mapped_buffers.size());
  debug_state.tracked_count = static_cast<uint32_t>(tracked_scene_cbuffers.size());
  if (!tracked) debug_state.heuristic_mapped_count += 1u;
  if (logging::ShouldLogFrame(constant_buffers::frame_state.frame_index, last_map_log, 120u)) {
    logging::Info("mapped CbScene resource=", logging::Hex{resource.handle},
                  " offset=", offset,
                  " size=", resolved_size,
                  " source=", BufferSourceName(source),
                  " tracked=", debug_state.tracked_count,
                  " heuristic_maps=", debug_state.heuristic_mapped_count,
                  " pending_maps=", debug_state.mapped_count,
                  " rt=", render_state.rt_width, "x", render_state.rt_height,
                  " viewport=", render_state.viewport_width, "x", render_state.viewport_height,
                  " screen=", render_state.screen_width, "x", render_state.screen_height);
  }
}

inline void OnUnmapBufferRegion(reshade::api::device* device, reshade::api::resource resource) {
  (void)device;
  MappedBuffer mapped = {};
  {
    std::lock_guard<std::mutex> lock(state_mutex);
    const auto it = mapped_buffers.find(resource.handle);
    if (it == mapped_buffers.end()) return;
    mapped = it->second;
    mapped_buffers.erase(it);
    debug_state.mapped_count = static_cast<uint32_t>(mapped_buffers.size());
  }

  bool should_log = false;
  std::array<float, 2> offset = {};
  DebugState log_debug = {};
  {
    std::lock_guard<std::mutex> lock(state_mutex);
    const bool applied = ApplyMappedBuffer(mapped);
    if (applied) {
      should_log = logging::ShouldLogFrame(constant_buffers::frame_state.frame_index, last_apply_log, 120u);
      if (should_log) {
        offset = GetFrameJitter();
        log_debug = debug_state;
      }
    }
  }

  if (should_log) {
    logging::Info("applied jitter to CbScene resource=", logging::Hex{resource.handle},
                  " frame=", constant_buffers::frame_state.frame_index,
                  " sample=", constant_buffers::frame_state.taa_sample_index,
                  " source=", BufferSourceName(mapped.source),
                  " tracked=", logging::Bool{log_debug.last_tracked},
                  " camera_like=", logging::Bool{log_debug.last_camera_like},
                  " jitter_uv=", offset[0], ",", offset[1],
                  " jitter_px=", offset[0] * static_cast<float>(log_debug.last_rt_width), ",",
                  offset[1] * static_cast<float>(log_debug.last_rt_height),
                  " projection=", offset[0] * 2.f, ",", offset[1] * 2.f,
                  " rt=", log_debug.last_rt_width, "x", log_debug.last_rt_height,
                  " viewport=", log_debug.last_viewport_width, "x", log_debug.last_viewport_height,
                  " size=", log_debug.last_mapped_size,
                  " taa_ran=", logging::Bool{constant_buffers::frame_state.taa_ran_this_frame});
  }
}

}  // namespace taa::jitter
