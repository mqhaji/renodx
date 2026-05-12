#pragma once

/*
 * CPU-side projection jitter injection for MGSV's camera cbuffer.
 *
 * MGSV's `cVSScene`/`cPSScene` struct holds m_projectionView (offset 0) and
 * m_projection (offset 64) along with several other matrices we leave alone.
 * The struct is 480 bytes and is bound to vertex b2 and pixel b2 across many
 * passes via separate D3D11 cbuffer resources.
 *
 * Strategy:
 *  - Track every D3D11 buffer bound at vertex/pixel b2 as a candidate. (See
 *    descriptor_tracker::OnPushDescriptors.)
 *  - On map/unmap, if the resource is tracked and its size matches the camera
 *    cbuffer layout, patch m_projection and m_projectionView with the current
 *    frame's jitter offset before the GPU consumes the data.
 *
 * Per-object velocity ends up as `pure_motion + current_jitter` because the
 * previous-frame matrices (m_shadowProjection, m_shadowProjection2) are not
 * patched. The compute shader compensates by subtracting previous_jitter from
 * the decoded velocity sample.
 */

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <unordered_map>

#include <include/reshade.hpp>

#include "./constant_buffers.hpp"
#include "./descriptor_tracker.hpp"
#include "./logging.hpp"

namespace taa::jitter {

struct Matrix4 {
  float m[4][4] = {};
};

// Layout mirrors the `cVSScene` struct disassembly. Matrices are column-major
// per MGSV's shader compilation (cb2[4..7] => m_projection columns).
struct CbScene {
  Matrix4 m_projectionView;     // offset    0
  Matrix4 m_projection;         // offset   64
  Matrix4 m_view;               // offset  128
  Matrix4 m_shadowProjection;   // offset  192
  Matrix4 m_shadowProjection2;  // offset  256
  // Remaining 160 bytes are scalar params we don't touch:
  //   m_eyepos, m_projectionParam, m_viewportSize, m_exposure,
  //   m_fogParam[3], m_fogColor, m_cameraCenterOffset, m_shadowMapResolutions.
};

static_assert(sizeof(CbScene) == 320, "CbScene matrix block must be 320 bytes (5 float4x4)");

inline constexpr uint64_t CBSCENE_BYTES = 480u;

struct MappedBuffer {
  uint8_t* data = nullptr;
  uint64_t size = 0u;
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

inline std::unordered_map<uint64_t, MappedBuffer> mapped_buffers;
inline RenderState render_state = {};
inline uint64_t last_apply_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_skip_log = std::numeric_limits<uint64_t>::max();
inline uint64_t last_swapchain_log = std::numeric_limits<uint64_t>::max();

inline bool LogEvery(uint64_t& last_frame, uint64_t interval = 120u) {
  return logging::ShouldLogFrame(constant_buffers::frame_state.frame_index, last_frame, interval);
}

// J * P, where J is the jitter-insert column-vector form used by the shader
// disassembly: dp4 with cb2[4..7]. For column-major storage in the cbuffer,
// the jitter offsets land in the translation slots of the projection matrix.
inline void ApplyJitterInPlace(Matrix4& matrix, float jx, float jy) {
  // m[col][row] in this storage; for column-major HLSL with cb2[col].row index.
  // dp4(r, cb2[col]) computes sum over k of cb2[col][k] * r[k] -> output[col].
  // To add jitter as a clip-space translation we increment the row that
  // contributes to x and y after the w divide. In MGSV's matrix that is row 3:
  //   x_clip = ... + cb2[col][3] * 1   for the position output column.
  // Concretely: add jitter to cb2[3].x and cb2[3].y (column index 3, row 0/1).
  matrix.m[3][0] += jx;
  matrix.m[3][1] += jy;
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

inline bool ApplyMappedBuffer(const MappedBuffer& mapped) {
  if (mapped.data == nullptr || mapped.size < CBSCENE_BYTES) return false;
  if (!constant_buffers::IsEnabled()) return false;

  // Apply jitter only to fullscreen-resolution passes — keeps planar
  // reflections, mirrors, low-res helpers, and shadow-map renders out of the
  // jittered set.
  if (!IsFullscreenPass()) {
    if (LogEvery(last_skip_log)) {
      logging::Info("skipping CbScene jitter outside fullscreen pass rt=",
                    render_state.rt_width, "x", render_state.rt_height,
                    " viewport=", render_state.viewport_width, "x", render_state.viewport_height,
                    " screen=", render_state.screen_width, "x", render_state.screen_height);
    }
    return false;
  }

  auto* scene = reinterpret_cast<CbScene*>(mapped.data);
  const auto offset = GetFrameJitter();
  ApplyJitterInPlace(scene->m_projection, offset[0], offset[1]);
  ApplyJitterInPlace(scene->m_projectionView, offset[0], offset[1]);
  return true;
}

inline void Reset() {
  mapped_buffers.clear();
  render_state = {};
  descriptor_tracker::tracked_camera_cbuffers.clear();
}

inline void OnInitSwapchain(reshade::api::swapchain* swapchain, bool) {
  if (swapchain == nullptr) return;
  auto* device = swapchain->get_device();
  if (device == nullptr) return;

  const auto back_buffer = swapchain->get_back_buffer(0);
  if (back_buffer.handle == 0u) return;

  const auto desc = device->get_resource_desc(back_buffer);
  render_state.screen_width = desc.texture.width;
  render_state.screen_height = desc.texture.height;

  if (LogEvery(last_swapchain_log, 1u)) {
    logging::Info("jitter screen size ", render_state.screen_width, "x", render_state.screen_height);
  }
}

inline void OnDestroySwapchain(reshade::api::swapchain*, bool) {
  render_state.screen_width = 0u;
  render_state.screen_height = 0u;
  render_state.has_render_target = false;
  render_state.has_viewport = false;
}

inline void OnBindRenderTargetsAndDepthStencil(
    reshade::api::command_list* cmd_list,
    uint32_t count,
    const reshade::api::resource_view* rtvs,
    reshade::api::resource_view) {
  if (cmd_list == nullptr || count == 0u || rtvs == nullptr || rtvs[0].handle == 0u) {
    render_state.has_render_target = false;
    return;
  }
  auto* device = cmd_list->get_device();
  if (device == nullptr) return;

  const auto resource = device->get_resource_from_view(rtvs[0]);
  if (resource.handle == 0u) {
    render_state.has_render_target = false;
    return;
  }
  const auto desc = device->get_resource_desc(resource);
  render_state.rt_width = desc.texture.width;
  render_state.rt_height = desc.texture.height;
  render_state.has_render_target = true;
}

inline void OnBindViewports(
    reshade::api::command_list*,
    uint32_t first,
    uint32_t count,
    const reshade::api::viewport* viewports) {
  if (viewports == nullptr || count == 0u || first != 0u) {
    render_state.has_viewport = false;
    return;
  }
  render_state.viewport_width = viewports[0].width;
  render_state.viewport_height = viewports[0].height;
  render_state.has_viewport = true;
}

inline void OnMapBufferRegion(
    reshade::api::device* device,
    reshade::api::resource resource,
    uint64_t offset,
    uint64_t size,
    reshade::api::map_access,
    void** mapped_data) {
  if (device == nullptr || mapped_data == nullptr || *mapped_data == nullptr) return;
  if (!descriptor_tracker::IsTrackedCameraCbuffer(resource)) return;

  uint64_t resolved_size = size;
  if (resolved_size == UINT64_MAX) {
    const auto desc = device->get_resource_desc(resource);
    resolved_size = desc.buffer.size > offset ? desc.buffer.size - offset : 0u;
  }
  if (resolved_size < CBSCENE_BYTES) return;

  mapped_buffers[resource.handle] = MappedBuffer{
      .data = static_cast<uint8_t*>(*mapped_data) + offset,
      .size = resolved_size,
  };
}

inline void OnUnmapBufferRegion(reshade::api::device*, reshade::api::resource resource) {
  const auto it = mapped_buffers.find(resource.handle);
  if (it == mapped_buffers.end()) return;

  const bool applied = ApplyMappedBuffer(it->second);
  mapped_buffers.erase(it);

  if (applied && LogEvery(last_apply_log)) {
    const auto offset = GetFrameJitter();
    logging::Info("applied jitter to CbScene resource=", logging::Hex{resource.handle},
                  " frame=", constant_buffers::frame_state.frame_index,
                  " sample=", constant_buffers::frame_state.taa_sample_index,
                  " offset=", offset[0], ",", offset[1]);
  }
}

}  // namespace taa::jitter
