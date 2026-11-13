/*
 * Copyright (C) 2026 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#ifndef RENODX_GAMES_MGSV_RECONSTRUCTION_POSTPROCESS_RUNTIME_HPP_
#define RENODX_GAMES_MGSV_RECONSTRUCTION_POSTPROCESS_RUNTIME_HPP_

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <d3d11.h>
#include <d3d11_1.h>
#include <wrl/client.h>

#include <include/reshade.hpp>

#include "../../../../utils/command_action.hpp"
#include "../../../../utils/hash.hpp"
#include "../../../../utils/resource_upgrade.hpp"
#include "../logging.hpp"
#include "../resolution.hpp"
#include "../state.hpp"
#include "resources.hpp"

namespace mgsv::reconstruction::post_process {

// Native constant buffers that contain dimensions or sampling offsets we adjust.
// These are the game's draw parameters, separate from injected user settings.
struct ConstantBinding {
  UINT slot;           // Shader register: b0 system, b2 scene, or b5 object data.
  UINT minimum_bytes;  // Minimum supported native layout size within the bound range.
};
inline constexpr auto CONSTANT_BINDINGS = std::to_array<ConstantBinding>({
    {.slot = 0u, .minimum_bytes = 64u},
    {.slot = 2u, .minimum_bytes = 480u},
    {.slot = 5u, .minimum_bytes = 208u},
});
inline constexpr uint32_t CONSTANT_BINDINGS_MASK = (1u << CONSTANT_BINDINGS.size()) - 1u;
// This adapter saves t0..t15. Keep their full saved bindings so
// restoring a draw also restores unused slots displaced by target changes.
inline constexpr uint32_t PIXEL_INPUT_COUNT = 16u;
inline constexpr uint32_t PIXEL_INPUT_MASK = (1u << PIXEL_INPUT_COUNT) - 1u;
// This native shader draws depth-tested flare sprites into a separate layer.
inline constexpr uint32_t LENS_FLARE_SHADER_HASH = 0x93A0C8D8u;
// One metadata word records the shader's actual reads: bits 0..2 identify the
// constant bindings above, bit 3 identifies flare sprites, and bit 4 says the
// texture reads are known. Bits 16..31 identify sampled texture slots t0..t15.
inline constexpr uint32_t LENS_FLARE_SHADER_FLAG = 8u;
inline constexpr uint32_t PIXEL_INPUTS_KNOWN_FLAG = 16u;
inline constexpr uint32_t PIXEL_INPUTS_SHIFT = 16u;

// Byte offsets in MGSV's native b0, b2, and b5 layouts. The remaining fields
// are copied unchanged, including values outside a draw's selected range.
inline constexpr UINT SYSTEM_VIEW_SIZE_OFFSET = 16u;
inline constexpr UINT SYSTEM_BUFFER_SIZE_OFFSET = 32u;
inline constexpr UINT SCENE_VIEWPORT_SIZE_OFFSET = 352u;
inline constexpr UINT OBJECT_LOCAL_PARAM_OFFSET = 144u;
inline constexpr UINT OBJECT_LAST_PARAM_OFFSET = 192u;

// Routing rules for an existing native draw, not a request to add a new pass.
struct Pass {
  uint32_t shader_hash;
  uint32_t observation_bit;  // Groups related draws under one success log message.
  const char* name;
  bool resize_targets = false;               // Allow a qualified draw to create enlarged targets.
  bool require_scene_input = false;          // Require a redirected t0 before changing this draw.
  bool display_output = false;               // Read our scene, but keep the game's final output target.
  bool keep_scene_size = false;              // Keep native b2 dimensions for motion preparation.
  bool dof_pass = false;                     // Require the matching native DoF call when scaling.
  Surface::Role role = Surface::Role::DATA;  // Purpose assigned to intermediate writes.
};

// Native bindings saved for one replayed draw. End restores only the groups
// changed by Begin, including empty slots and constant-buffer subranges.
struct DrawBindings {
  std::shared_ptr<Output> output;
  std::array<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> rtvs;
  Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;
  std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, PIXEL_INPUT_COUNT> srvs;
  std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2> vertex_srvs;
  uint32_t rtv_count = 0u;
  bool targets_changed = false;
  bool inputs_changed = false;
  bool vertex_inputs_changed = false;
  std::array<D3D11_VIEWPORT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports;
  std::array<D3D11_RECT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> scissors;
  UINT viewport_count = 0u;
  UINT scissor_count = 0u;
  bool rectangles_changed = false;
  std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, CONSTANT_BINDINGS.size()> constants;
  // D3D11.1 expresses these ranges in four-float constants, not bytes.
  std::array<UINT, CONSTANT_BINDINGS.size()> first_constants = {};
  std::array<UINT, CONSTANT_BINDINGS.size()> constant_counts = {};
  Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_constants;
  UINT vertex_first_constant = 0u;
  UINT vertex_constant_count = 0u;
  bool vertex_constants_changed = false;
  uint32_t changed_constants = 0u;
};

// Per-context state follows native bindings and reuses temporary GPU buffers.
// It describes command recording; it does not establish GPU execution order.
struct __declspec(uuid("9b2bb264-b9a8-4c77-b7e3-d72a281b1460")) DrawState {
  bool draw_2d = false;
  bool ui_draw = false;
  bool flare_sprite = false;
  // Tie the DoF chain to one native call, even if several calls share a frame.
  uint64_t dof_token = 0u;
  uint64_t dof_frame = 0u;
  bool dof_active = false;
  DrawBindings bindings;
  // Copies preserve unrelated native fields while selected size fields change.
  std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, CONSTANT_BINDINGS.size()> output_constants;
  std::array<UINT, CONSTANT_BINDINGS.size()> output_constant_bytes = {};
  Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_output_constants;
  UINT vertex_output_constant_bytes = 0u;
  // Small compute shaders write adjusted fields here before GPU buffer copies.
  Microsoft::WRL::ComPtr<ID3D11Buffer> system_scratch;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> system_scratch_uav;
};

// Private-data keys attach classifications to shader objects. Their numbers are
// unique identifiers, not shader hashes, register numbers, or tuning values.
inline constexpr GUID DRAW_2D_SHADER_GUID = {
    .Data1 = 0xEF97CA65u,
    .Data2 = 0x4F71u,
    .Data3 = 0x49B5u,
    .Data4 = {0x9Cu, 0x95u, 0xBCu, 0xE6u, 0x3Fu, 0x31u, 0x44u, 0x6Du},
};

inline constexpr GUID PIXEL_CONSTANTS_GUID = {
    .Data1 = 0x64241BC9u,
    .Data2 = 0xC51Au,
    .Data3 = 0x4A33u,
    .Data4 = {0x84u, 0x7Du, 0x52u, 0xBFu, 0xC7u, 0x09u, 0xF2u, 0x1Au},
};

// Classify once when a shader is created. Reflection identifies what it reads;
// the main source filename distinguishes UI/color draws from packed-data work.
inline void OnInitPipeline(reshade::api::device* device, reshade::api::pipeline_layout /*layout*/,
                           uint32_t count, const reshade::api::pipeline_subobject* objects, reshade::api::pipeline pipeline) {
  if (device->get_api() != reshade::api::device_api::d3d11 || pipeline.handle == 0u || count != 1u
      || objects[0].data == nullptr) return;
  if (objects[0].type != reshade::api::pipeline_subobject_type::vertex_shader
      && objects[0].type != reshade::api::pipeline_subobject_type::pixel_shader) return;
  const auto& shader = *static_cast<const reshade::api::shader_desc*>(objects[0].data);
  if (shader.code == nullptr) return;
  const std::span<const uint8_t> code(static_cast<const uint8_t*>(shader.code), shader.code_size);
  // Byte lengths defined by the compiled DXBC format: container/chunk headers,
  // resource bindings (RDEF), and source-file debug tables (SDBG). These are
  // file-layout constants; bounds checks must precede every table read.
  static constexpr size_t CONTAINER_HEADER_BYTES = 32u;
  static constexpr size_t CHUNK_HEADER_BYTES = 8u;
  static constexpr size_t RESOURCE_BINDING_BYTES = 32u;
  static constexpr size_t DEBUG_HEADER_BYTES = 84u;
  static constexpr size_t SOURCE_FILE_BYTES = 16u;
  const auto read_word = [](std::span<const uint8_t> bytes, size_t offset) {
    uint32_t value = 0u;
    if (offset <= bytes.size() && bytes.size() - offset >= sizeof(value)) {
      std::memcpy(&value, bytes.data() + offset, sizeof(value));
    }
    return value;
  };
  const auto find_chunk = [&read_word](std::span<const uint8_t> bytes, std::string_view kind) -> std::span<const uint8_t> {
    if (bytes.size() < CONTAINER_HEADER_BYTES || std::memcmp(bytes.data(), "DXBC", 4u) != 0) return {};
    const size_t container_size = read_word(bytes, 24u);
    if (container_size < CONTAINER_HEADER_BYTES || container_size > bytes.size()) return {};
    bytes = bytes.first(container_size);
    const size_t chunks = read_word(bytes, 28u);
    if (chunks > (bytes.size() - CONTAINER_HEADER_BYTES) / sizeof(uint32_t)) return {};
    for (size_t i = 0u; i < chunks; ++i) {
      const size_t offset = read_word(bytes, CONTAINER_HEADER_BYTES + (i * sizeof(uint32_t)));
      if (offset > bytes.size() - CHUNK_HEADER_BYTES) return {};
      const size_t length = read_word(bytes, offset + 4u);
      if (length > bytes.size() - offset - CHUNK_HEADER_BYTES) return {};
      if (std::memcmp(bytes.data() + offset, kind.data(), 4u) == 0) return bytes.subspan(offset + CHUNK_HEADER_BYTES, length);
    }
    return {};
  };
  if (objects[0].type == reshade::api::pipeline_subobject_type::pixel_shader) {
    const auto definition = find_chunk(code, "RDEF");
    if (definition.size() < 20u || (read_word(definition, 16u) & 0xFFFFu) != 0x0500u) return;
    const size_t bindings = read_word(definition, 8u);
    const size_t offset = read_word(definition, 12u);
    if (offset > definition.size() || bindings > (definition.size() - offset) / RESOURCE_BINDING_BYTES) return;
    // Low bits identify native constant slots; high bits identify sampled SRVs.
    uint32_t flags = PIXEL_INPUTS_KNOWN_FLAG;
    for (size_t i = 0u; i < bindings; ++i) {
      const size_t binding = offset + (i * RESOURCE_BINDING_BYTES);
      const uint32_t type = read_word(definition, binding + 4u);
      const uint32_t slot = read_word(definition, binding + 20u);
      if (type == 2u || type == 5u || type == 7u) {
        const uint32_t array_size = read_word(definition, binding + 24u);
        if (slot < PIXEL_INPUT_COUNT) {
          for (uint32_t index = slot; index < slot + std::min(array_size, PIXEL_INPUT_COUNT - slot); ++index) {
            flags |= 1u << (PIXEL_INPUTS_SHIFT + index);
          }
        }
      }
      if (type != 0u) continue;
      for (size_t index = 0u; index < CONSTANT_BINDINGS.size(); ++index) {
        if (slot == CONSTANT_BINDINGS[index].slot) {
          flags |= 1u << index;
        }
      }
    }
    if (renodx::utils::hash::ComputeCRC32(code.data(), code.size()) == LENS_FLARE_SHADER_HASH) {
      flags |= LENS_FLARE_SHADER_FLAG;
    }
    auto* pixel = reinterpret_cast<ID3D11PixelShader*>(pipeline.handle);  // NOLINT(performance-no-int-to-ptr)
    pixel->SetPrivateData(PIXEL_CONSTANTS_GUID, sizeof(flags), &flags);
    return;
  }
  // Read the main source filename, excluding Draw2D mentions in shared headers.
  const auto debug = find_chunk(code, "SDBG");
  if (debug.size() < DEBUG_HEADER_BYTES || read_word(debug, 0u) != DEBUG_HEADER_BYTES || read_word(debug, 20u) == 0u) return;
  // SDBG table offsets start after its 84-byte header.
  const size_t files = DEBUG_HEADER_BYTES + static_cast<size_t>(read_word(debug, 24u));
  const size_t strings = DEBUG_HEADER_BYTES + static_cast<size_t>(read_word(debug, 80u));
  if (files > debug.size() || static_cast<size_t>(read_word(debug, 20u)) > (debug.size() - files) / SOURCE_FILE_BYTES || strings > debug.size()) return;
  const size_t name_offset = read_word(debug, files);
  const size_t name_size = read_word(debug, files + 4u);
  if (name_offset > debug.size() - strings || name_size > debug.size() - strings - name_offset) return;
  std::string_view name(reinterpret_cast<const char*>(debug.data() + strings + name_offset), name_size);
  if (const size_t slash = name.find_last_of("/\\"); slash != std::string_view::npos) {
    name.remove_prefix(slash + 1u);
  }
  if (!name.starts_with("Draw2D") || !name.ends_with("_vs.hlsl")) return;
  // These Draw2D programs prepare packed material data, not scene color or UI.
  // Their native textures can later be reused as scene-color storage.
  if (name == "Draw2D_BlendNormal_vs.hlsl" || name == "Draw2D_BlendNormal_ln_vs.hlsl"
      || name == "Draw2D_EncodeNormal_vs.hlsl" || name == "Draw2D_BlendRoughness_vs.hlsl") return;
  auto* vertex = reinterpret_cast<ID3D11VertexShader*>(pipeline.handle);  // NOLINT(performance-no-int-to-ptr)
  // Plain Draw2D and Ui variants draw the native interface. Other Draw2D
  // variants include scene effects and still need a qualified scene route.
  const uint32_t draw_2d = (name == "Draw2D_vs.hlsl" || name.starts_with("Draw2D_Ui") ? 2u : 1u);
  vertex->SetPrivateData(DRAW_2D_SHADER_GUID, sizeof(draw_2d), &draw_2d);
}

inline void OnBindPipeline(reshade::api::command_list* command_list, reshade::api::pipeline_stage stages,
                           reshade::api::pipeline pipeline) {
  auto* draw = command_list->get_private_data<DrawState>();
  if (draw == nullptr) return;
  if ((static_cast<uint32_t>(stages) & static_cast<uint32_t>(reshade::api::pipeline_stage::pixel_shader)) != 0u) {
    draw->flare_sprite = false;
    if (pipeline.handle != 0u && stages == reshade::api::pipeline_stage::pixel_shader) {
      auto* pixel = reinterpret_cast<ID3D11PixelShader*>(pipeline.handle);  // NOLINT(performance-no-int-to-ptr)
      uint32_t flags = 0u;
      UINT bytes = sizeof(flags);
      if (SUCCEEDED(pixel->GetPrivateData(PIXEL_CONSTANTS_GUID, &bytes, &flags))) {
        draw->flare_sprite = (flags & LENS_FLARE_SHADER_FLAG) != 0u;
      }
    }
  }
  if ((static_cast<uint32_t>(stages) & static_cast<uint32_t>(reshade::api::pipeline_stage::vertex_shader)) == 0u) return;
  draw->draw_2d = false;
  draw->ui_draw = false;
  if (pipeline.handle == 0u || stages != reshade::api::pipeline_stage::vertex_shader) return;
  auto* vertex = reinterpret_cast<ID3D11VertexShader*>(pipeline.handle);  // NOLINT(performance-no-int-to-ptr)
  uint32_t kind = 0u;
  UINT size = sizeof(kind);
  if (SUCCEEDED(vertex->GetPrivateData(DRAW_2D_SHADER_GUID, &size, &kind))) {
    draw->draw_2d = kind == 1u || kind == 2u;
    draw->ui_draw = kind == 2u;
  }
}

inline void OnResetCommandList(reshade::api::command_list* command_list) {
  // ReShade also emits this after failed FinishCommandList calls. Only the
  // successful completion callback may release an unfinished output route.
  if (auto* draw = command_list->get_private_data<DrawState>(); draw != nullptr) {
    draw->draw_2d = false;
    draw->ui_draw = false;
    draw->flare_sprite = false;
    draw->dof_active = false;
  }
}

inline void OnInitCommandList(reshade::api::command_list* command_list) {
  if (command_list->get_device()->get_api() == reshade::api::device_api::d3d11) {
    command_list->create_private_data<DrawState>();
    command_list->create_private_data<Recording>();
  }
}

inline void OnDestroyCommandList(reshade::api::command_list* command_list) {
  if (command_list->get_device()->get_api() == reshade::api::device_api::d3d11) {
    command_list->destroy_private_data<DrawState>();
    command_list->destroy_private_data<Recording>();
  }
}

// The binding event carries live native views, before addon clone activation
// can remove an otherwise unused DSV from a fullscreen composition draw.
inline void OnBindTargets(reshade::api::command_list* command_list, uint32_t count,
                          const reshade::api::resource_view* targets, reshade::api::resource_view depth) {
  if (count == 0u || targets == nullptr || targets[0].handle == 0u || depth.handle == 0u
      || command_list->get_device()->get_api() != reshade::api::device_api::d3d11) return;
  auto* view = reinterpret_cast<ID3D11RenderTargetView*>(targets[0].handle);  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11Resource> resource;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  view->GetResource(&resource);
  if (FAILED(resource.As(&texture))) return;
  D3D11_TEXTURE2D_DESC description;
  texture->GetDesc(&description);
  const auto render = resolution::ReadActual();
  if (description.Width != render.width || description.Height != render.height
      || (description.Format != DXGI_FORMAT_B8G8R8A8_TYPELESS && description.Format != DXGI_FORMAT_B8G8R8A8_UNORM)) return;
  auto* native_depth = reinterpret_cast<ID3D11DepthStencilView*>(depth.handle);  // NOLINT(performance-no-int-to-ptr)
  resource->SetPrivateDataInterface(SCENE_DEPTH_GUID, native_depth);
  const auto clone = renodx::utils::resource::upgrade::GetResourceClone({reinterpret_cast<uint64_t>(resource.Get())}, {.allow_create = false});
  if (clone.handle != 0u) {
    auto* cloned = reinterpret_cast<ID3D11Resource*>(clone.handle);  // NOLINT(performance-no-int-to-ptr)
    cloned->SetPrivateDataInterface(SCENE_DEPTH_GUID, native_depth);
  }
}

// Loading and independently recorded UI acquire storage for their native scene.
// Their clears and draws can run before any reconstruction insertion exists.
// Callers qualify bound scene/depth identities before redirecting them.
inline Recording GetUiRoute(reshade::api::command_list* command_list) {
  Recording route;
  if (GetActive(command_list, &route, true) != nullptr) return route;
  const auto* device = command_list->get_device();
  const auto owner = rendering::native_context::GetScope().owner;
  // A utility view can draw into the same pooled texture as the main scene.
  // Its clears and Draw2D work must keep the utility view's pixel grid.
  if (owner.scene != 0u && (!owner.main_view || !owner.active_member || !owner.presentation_matches)) return {};
  const auto plan = resolution::Read();
  if (!plan.IsScaled() || resolution::failed.load(std::memory_order_acquire)) return {};
  std::shared_ptr<Output> cached;
  {
    const std::shared_lock lock(state.mutex);
    cached = state.cached;
  }
  if (cached == nullptr || cached->device != device
      || cached->render_width != plan.render.width || cached->render_height != plan.render.height
      || cached->target.width != plan.output.width || cached->target.height != plan.output.height) return {};
  if (!owner.presentation_matches || (owner.scene == 0u && owner.presentation_scene == 0u)) return {};
  auto inputs = cached->allocation_inputs;
  inputs.frame_token = reconstruction::state::CurrentFrameToken();
  // Keep the source layout alive until its views have been retained by the
  // new output. Native UI clears and draws initialize the acquired storage.
  try {
    AcquireOutput(inputs);
  } catch (const std::exception& error) {
    logging::Warn(error.what());
    resolution::Fail("UI output allocation failed");
    return {};
  }
  GetActive(command_list, &route, true);
  return route;
}

inline bool Begin(reshade::api::command_list* command_list, const Pass* pass = nullptr) {
  // Resolve ownership before changing any bindings. An unfinished older scene
  // must keep its own route even if another worker has published the next one.
  Recording route;
  auto output = GetActive(command_list, &route);
  auto* draw = command_list->get_private_data<DrawState>();
  if (draw == nullptr || draw->bindings.output != nullptr) return false;
  if (output == nullptr && pass != nullptr && (pass->display_output || (pass->shader_hash == 0u && draw->ui_draw))) {
    route = GetUiRoute(command_list);
    output = route.output;
  }
  if (output == nullptr) {
    // A missed late route otherwise looks like a successful native draw.
    // Report only known scene inputs, at most once per 600 recording intervals.
    if (pass != nullptr && (pass->observation_bit & (8u | 16u | 32u)) != 0u) {
      const auto plan = resolution::Read();
      std::shared_ptr<Output> cached;
      uint64_t published_frame = 0u;
      if (plan.IsScaled()) {
        const std::shared_lock lock(state.mutex);
        cached = state.cached;
        published_frame = state.frame;
      }
      if (cached != nullptr && cached->device == command_list->get_device()
          && cached->render_width == plan.render.width && cached->render_height == plan.render.height
          && cached->target.width == plan.output.width && cached->target.height == plan.output.height) {
        auto* context = reinterpret_cast<ID3D11DeviceContext*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> input;
        Microsoft::WRL::ComPtr<ID3D11Resource> resource;
        context->PSGetShaderResources(0u, 1u, &input);
        if (input != nullptr) {
          input->GetResource(&resource);
        }
        if (cached->IsSceneTarget(resource.Get())) {
          const uint64_t frame = reconstruction::state::CurrentFrameToken();
          static std::atomic<uint64_t> last_reported = std::numeric_limits<uint64_t>::max();
          auto previous = last_reported.load(std::memory_order_relaxed);
          if ((previous == std::numeric_limits<uint64_t>::max() || frame - previous >= 600u)
              && last_reported.compare_exchange_strong(previous, frame, std::memory_order_relaxed)) {
            logging::Warn("scaled post-processing route unavailable during recording; pass=", pass->name,
                          " shader=", logging::Hex{pass->shader_hash}, " recording_interval=", frame,
                          " published_interval=", published_frame, " deferred=", context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED);
          }
        }
      }
    }
    return false;
  }
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  const bool scene_only = pass != nullptr && pass->shader_hash == 0u;
  if (scene_only) {
    // Draw2D also initializes fixed-size captures in pooled scene textures.
    // Resource identity alone cannot qualify those color and stencil writes
    // for the main view. UI clipping uses scissors, not a smaller viewport.
    std::array<D3D11_VIEWPORT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports = {};
    UINT viewport_count = static_cast<UINT>(viewports.size());
    context->RSGetViewports(&viewport_count, viewports.data());
    if (viewport_count != 1u) return false;
    const auto& viewport = viewports[0];
    // Some UI already uses output coordinates and must not be enlarged twice.
    const bool main_view = (viewport.Width == static_cast<float>(output->render_width) && viewport.Height == static_cast<float>(output->render_height))
                           || (viewport.Width == static_cast<float>(output->target.width) && viewport.Height == static_cast<float>(output->target.height));
    if (viewport.TopLeftX != 0.f || viewport.TopLeftY != 0.f || !main_view) return false;
  }
  auto* saved = &draw->bindings;
  std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> targets = {};
  // Inspect the shader actually bound after replacement. Unknown metadata uses
  // conservative saved bindings; known texture reads avoid remapping stale slots.
  uint32_t constant_mask = CONSTANT_BINDINGS_MASK;
  bool constant_reads_known = false;
  uint32_t pixel_inputs = PIXEL_INPUT_MASK;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel;
  context->PSGetShader(&pixel, nullptr, nullptr);
  if (pixel == nullptr) {
    constant_mask = 0u;
    pixel_inputs = 0u;
  } else {
    uint32_t flags = 0u;
    UINT bytes = sizeof(flags);
    if (SUCCEEDED(pixel->GetPrivateData(PIXEL_CONSTANTS_GUID, &bytes, &flags)) && bytes == sizeof(flags)) {
      constant_mask = flags & CONSTANT_BINDINGS_MASK;
      constant_reads_known = true;
      if ((flags & PIXEL_INPUTS_KNOWN_FLAG) != 0u) {
        pixel_inputs = flags >> PIXEL_INPUTS_SHIFT;
      }
    }
  }
  std::array<ID3D11ShaderResourceView*, PIXEL_INPUT_COUNT> inputs = {};
  std::array<ID3D11ShaderResourceView*, 2> vertex_inputs = {};
  context->OMGetRenderTargets(static_cast<UINT>(targets.size()), targets.data(), &saved->dsv);
  context->PSGetShaderResources(0u, static_cast<UINT>(inputs.size()), inputs.data());
  for (size_t index = 0u; index < targets.size(); ++index) {
    saved->rtvs[index].Attach(targets[index]);
  }
  for (size_t index = 0u; index < inputs.size(); ++index) {
    saved->srvs[index].Attach(inputs[index]);
  }
  bool qualified_flare = true;
  if (pass != nullptr && pass->shader_hash == LENS_FLARE_SHADER_HASH && output->IsScaled()) {
    Microsoft::WRL::ComPtr<ID3D11Resource> depth_resource;
    if (inputs[1] != nullptr) {
      inputs[1]->GetResource(&depth_resource);
    }
    qualified_flare = depth_resource != nullptr && depth_resource == output->geometry_depth && targets[0] != nullptr;
    if (qualified_flare) {
      Microsoft::WRL::ComPtr<ID3D11Resource> resource;
      Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
      targets[0]->GetResource(&resource);
      if (!output->IsSceneTarget(resource.Get())) {
        D3D11_TEXTURE2D_DESC description = {};
        if (SUCCEEDED(resource.As(&texture))) {
          texture->GetDesc(&description);
        }
        qualified_flare = description.Width == (output->render_width >> 1u) && description.Height == (output->render_height >> 1u);
        if (!qualified_flare) {
          resolution::Fail("unsupported depth-tested lens-flare target");
        }
      }
    }
  }
  // Follow the purpose of t0 through reductions and copies. Equal-sized native
  // textures can hold scene color, bloom, or unrelated data at different times.
  auto input_role = Surface::Role::DATA;
  if ((pixel_inputs & 1u) != 0u && inputs[0] != nullptr) {
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    inputs[0]->GetResource(&resource);
    if (output->IsSceneTarget(resource.Get())) {
      input_role = Surface::Role::SCENE_COLOR;
    } else {
      output->GetSurface(resource.Get(), false, route.frame, route.producer.get(), &input_role);
    }
  }
  if (!qualified_flare || (pass != nullptr && pass->role == Surface::Role::BLOOM && input_role != Surface::Role::SCENE_COLOR && input_role != Surface::Role::BLOOM && input_role != Surface::Role::BLOOM_FIXED_GRID)) {
    *saved = {};
    return false;
  }
  // One reduction writes only the fixed bloom rectangle. Keep that rectangle's
  // viewport and carry its sampling rules through flare generation and the copy.
  bool fixed_bloom_reduction = false;
  if (pass != nullptr && pass->shader_hash == 0x72C1D157u && input_role == Surface::Role::BLOOM && targets[0] != nullptr) {
    D3D11_VIEWPORT viewport = {};
    UINT count = 1u;
    context->RSGetViewports(&count, &viewport);
    if (count == 1u && viewport.TopLeftX == 0.f && viewport.TopLeftY == 0.f
        && viewport.Width == static_cast<float>(BLOOM_GRID_WIDTH) && viewport.Height == static_cast<float>(BLOOM_GRID_HEIGHT)) {
      Microsoft::WRL::ComPtr<ID3D11Resource> resource;
      Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
      targets[0]->GetResource(&resource);
      D3D11_TEXTURE2D_DESC description = {};
      if (SUCCEEDED(resource.As(&texture))) {
        texture->GetDesc(&description);
      }
      fixed_bloom_reduction = description.Width == (output->render_width >> 2u) && description.Height == (output->render_height >> 2u);
    }
  }
  const bool patch_fixed_bloom = pass != nullptr
                                 && (fixed_bloom_reduction || (input_role == Surface::Role::BLOOM_FIXED_GRID && (pass->shader_hash == 0x1AC1A5EFu || pass->shader_hash == 0x265053A6u || pass->shader_hash == 0x83272BCBu)));
  if (pass != nullptr && pass->shader_hash == 0xD05F5ABCu && output->IsScaled()) {
    // Native DoF sprites fetch their prepared color and mask in the VS.
    // Their output-sized grid must sample the same aliases the bake wrote.
    context->VSGetShaderResources(0u, static_cast<UINT>(vertex_inputs.size()), vertex_inputs.data());
    for (size_t index = 0u; index < vertex_inputs.size(); ++index) {
      saved->vertex_srvs[index].Attach(vertex_inputs[index]);
      const auto mapped = output->MapInput(vertex_inputs[index], false, route.frame, route.producer.get());
      if (mapped != nullptr) {
        vertex_inputs[index] = mapped.Get();
        saved->vertex_inputs_changed = true;
      }
    }
  }
  bool ui_view = false;
  if (scene_only && output->IsScaled() && saved->dsv != nullptr && output->native_depth != nullptr) {
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    saved->dsv->GetResource(&resource);
    ui_view = resource == output->native_depth;
  }
  for (size_t index = 0u; index < inputs.size(); ++index) {
    if ((pixel_inputs & (1u << index)) == 0u) continue;
    auto& input = inputs[index];
    const auto mapped = output->MapInput(input, scene_only, route.frame, route.producer.get());
    if (mapped != nullptr) {
      input = mapped.Get();
      saved->inputs_changed = true;
    }
  }
  // CopyRenderBuffer reads only t0. Unused slots can retain scene/depth views
  // from an earlier draw and must not claim an unrelated copy's destination.
  if (pass != nullptr && pass->require_scene_input && inputs[0] == saved->srvs[0].Get()) {
    *saved = {};
    return false;
  }
  const bool resize_targets = pass != nullptr && pass->resize_targets;
  const bool display_output = pass != nullptr && pass->display_output;
  auto target_role = (pass != nullptr ? pass->role : Surface::Role::DATA);
  if (target_role == Surface::Role::BLOOM && (fixed_bloom_reduction || input_role == Surface::Role::BLOOM_FIXED_GRID)) {
    target_role = Surface::Role::BLOOM_FIXED_GRID;
  }
  if (pass != nullptr && pass->require_scene_input && pass->role != Surface::Role::BLOOM) {
    target_role = input_role;
    if (pass->shader_hash == 0x83272BCBu && input_role == Surface::Role::BLOOM_FIXED_GRID) {
      // This copy expands the fixed working region into the final bloom image.
      target_role = Surface::Role::BLOOM;
    }
  }
  // Creating a target does not make its pixels valid. Delay clears and role
  // updates until the entire draw passes validation.
  std::array<std::shared_ptr<Surface>, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> pending_surfaces;
  std::array<std::optional<ClearColor>, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> pending_clears;
  const bool create_targets = (resize_targets || ui_view) && !display_output;
  // Qualify each destination separately. The first color write may also need
  // the native clear observed before its enlarged counterpart was allocated.
  for (size_t i = 0u; i < targets.size(); ++i) {
    if (targets[i] == nullptr) continue;
    saved->rtv_count = static_cast<uint32_t>(i) + 1u;
    Microsoft::WRL::ComPtr<ID3D11Resource> target_resource;
    targets[i]->GetResource(&target_resource);
    bool first_color_write = output->IsScaled() && target_role != Surface::Role::DATA && !output->IsSceneTarget(target_resource.Get());
    auto previous_role = Surface::Role::DATA;
    if (first_color_write && output->GetSurface(target_resource.Get(), false, route.frame, route.producer.get(), &previous_role) != nullptr) {
      first_color_write = previous_role != target_role;
    }
    const auto mapped = output->MapTarget(targets[i], create_targets, scene_only, target_role, route.frame, route.producer.get(), false);
    if (mapped != nullptr) {
      if (create_targets && !output->IsSceneTarget(target_resource.Get())) {
        pending_surfaces[i] = output->GetSurface(target_resource.Get(), true, route.frame, route.producer.get());
      }
      if (first_color_write) {
        // Cropped bloom and blended flare layers need their native initial
        // color and alpha, including clears recorded before alias activation.
        bool already_cleared = false;
        {
          const std::shared_lock lock(output->surface_mutex);
          already_cleared = route.producer != nullptr
                            && std::ranges::find(route.producer->cleared_surfaces, target_resource.Get()) != route.producer->cleared_surfaces.end();
        }
        if (!already_cleared) {
          pending_clears[i] = GetRecordedClear(target_resource.Get(), route.owner.scene, route.owner.generation);
        }
        if (!already_cleared && !pending_clears[i].has_value() && pass != nullptr && pass->shader_hash == LENS_FLARE_SHADER_HASH) {
          // A blended layer must not start with undefined color or alpha.
          std::ostringstream reason;
          reason << "native lens-flare layer clear was not observed; scene=" << route.owner.scene
                 << " generation=" << route.owner.generation << " target=" << target_resource.Get()
                 << " deferred=" << (context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED);
          resolution::Fail(reason.str().c_str());
          *saved = {};
          return false;
        }
      }
      targets[i] = mapped.Get();
      saved->targets_changed = true;
    } else if (output->IsScaled() && resize_targets && (!pass->require_scene_input || pass->role == Surface::Role::BLOOM)) {
      // Qualified DoF and motion-blur passes require matching output targets.
      // Generic copies and 2D draws may legitimately keep fixed-size targets.
      if (!resolution::failed.load(std::memory_order_acquire)) {
        Microsoft::WRL::ComPtr<ID3D11Resource> resource;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        targets[i]->GetResource(&resource);
        D3D11_TEXTURE2D_DESC description = {};
        if (SUCCEEDED(resource.As(&texture))) {
          texture->GetDesc(&description);
        }
        logging::Warn("required post-processing target unavailable pass=", pass->name, " shader=", logging::Hex{pass->shader_hash},
                      " slot=", i, " resource=", description.Width, "x", description.Height,
                      " format=", static_cast<uint32_t>(description.Format), " render=", output->render_width, "x", output->render_height,
                      " output=", output->target.width, "x", output->target.height);
      }
      resolution::Fail("required post-processing target unavailable");
      *saved = {};
      return false;
    }
  }
  const bool depth_only = ui_view && saved->rtv_count == 0u;
  if (!saved->targets_changed && !saved->inputs_changed && !saved->vertex_inputs_changed && !depth_only) {
    *saved = {};
    return false;
  }
  saved->output = std::move(output);
  saved->targets_changed = saved->targets_changed || depth_only;
  auto depth = saved->dsv;
  if (saved->targets_changed && depth != nullptr && saved->output->IsScaled()) {
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> tests;
    context->OMGetDepthStencilState(&tests, nullptr);
    // A null state uses D3D11's default depth test and writes.
    D3D11_DEPTH_STENCIL_DESC test_description = {.DepthEnable = TRUE, .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL, .DepthFunc = D3D11_COMPARISON_LESS};
    if (tests != nullptr) {
      tests->GetDesc(&test_description);
    }
    if (test_description.DepthEnable == FALSE && test_description.StencilEnable == FALSE) {
      // An unused native depth target must not force a resize or accompany
      // the enlarged color target. End restores the original binding.
      depth.Reset();
    } else {
      Microsoft::WRL::ComPtr<ID3D11Resource> resource;
      Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
      depth->GetResource(&resource);
      D3D11_TEXTURE2D_DESC extent = {};
      if (SUCCEEDED(resource.As(&texture))) {
        texture->GetDesc(&extent);
      }
      auto& routed_output = *saved->output;
      if (extent.Width != routed_output.target.width || extent.Height != routed_output.target.height) {
        if (resource != routed_output.native_depth || routed_output.output_depth == nullptr) {
          if (!resolution::failed.load(std::memory_order_acquire)) {
            logging::Warn("unqualified active depth/stencil pass=", (pass != nullptr ? pass->name : "scene"),
                          " shader=", logging::Hex{pass != nullptr ? pass->shader_hash : 0u},
                          " depth=", extent.Width, "x", extent.Height, " format=", static_cast<uint32_t>(extent.Format),
                          " depth_test=", test_description.DepthEnable, " stencil_test=", test_description.StencilEnable);
          }
          resolution::Fail("unqualified depth/stencil bound to output-sized rendering");
          *saved = {};
          return false;
        }
        D3D11_DEPTH_STENCIL_VIEW_DESC description;
        depth->GetDesc(&description);
        if (description.Flags >= routed_output.output_depth_views.size()) {
          resolution::Fail("unsupported output depth/stencil view flags");
          *saved = {};
          return false;
        }
        {
          const std::unique_lock lock(routed_output.surface_mutex);
          auto& mapped = routed_output.output_depth_views[description.Flags];
          if (mapped == nullptr) {
            auto* native_device = reinterpret_cast<ID3D11Device*>(command_list->get_device()->get_native());  // NOLINT(performance-no-int-to-ptr)
            if (FAILED(native_device->CreateDepthStencilView(routed_output.output_depth.Get(), &description, &mapped))) {
              resolution::Fail("output depth/stencil view creation failed");
            }
          }
          depth = mapped;
        }
        if (depth == nullptr) {
          *saved = {};
          return false;
        }
      }
    }
  }
  // Raster rectangles and shader dimensions must agree about the enlarged
  // target. Some native DoF helpers already use output dimensions; preserve
  // those views while updating the dimensions of the textures they sample.
  Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context1;
  if (saved->output->IsScaled() && (saved->targets_changed || display_output)) {
    if (FAILED(context->QueryInterface(IID_PPV_ARGS(&context1)))) {
      resolution::Fail("D3D11.1 post-processing context unavailable");
      *saved = {};
      return false;
    }
    saved->viewport_count = static_cast<UINT>(saved->viewports.size());
    saved->scissor_count = static_cast<UINT>(saved->scissors.size());
    context->RSGetViewports(&saved->viewport_count, saved->viewports.data());
    context->RSGetScissorRects(&saved->scissor_count, saved->scissors.data());
    if (saved->viewport_count != 1u) {
      resolution::Fail("unsupported output viewport layout");
      *saved = {};
      return false;
    }
    auto viewports = saved->viewports;
    auto scissors = saved->scissors;
    auto patch = saved->output->size_patch;
    D3D11_TEXTURE2D_DESC native_description = {};
    D3D11_TEXTURE2D_DESC output_description = {};
    Microsoft::WRL::ComPtr<ID3D11Resource> native_target;
    if (saved->rtv_count != 0u && saved->rtvs[0] == nullptr) {
      resolution::Fail("unsupported output target layout");
      *saved = {};
      return false;
    }
    if (saved->rtvs[0] != nullptr) {
      saved->rtvs[0]->GetResource(&native_target);
    }
    if (native_target != nullptr && targets[0] != nullptr) {
      Microsoft::WRL::ComPtr<ID3D11Resource> resource;
      Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
      if (SUCCEEDED(native_target.As(&texture))) {
        texture->GetDesc(&native_description);
      }
      targets[0]->GetResource(resource.ReleaseAndGetAddressOf());
      texture.Reset();
      if (SUCCEEDED(resource.As(&texture))) {
        texture->GetDesc(&output_description);
      }
      // Find the patch by native identity, not the newly allocated destination.
      const auto surface = (pending_surfaces[0] != nullptr ? pending_surfaces[0]
                                                           : saved->output->GetSurface(native_target.Get(), false, route.frame, route.producer.get()));
      if (surface != nullptr) {
        patch = surface->size_patch;
      }
    }
    float scale_x = 1.f;
    float scale_y = 1.f;
    if (depth_only && saved->rtv_count == 0u) {
      if (viewports[0].Width == static_cast<float>(saved->output->render_width)
          && viewports[0].Height == static_cast<float>(saved->output->render_height)) {
        scale_x = static_cast<float>(saved->output->target.width) / static_cast<float>(saved->output->render_width);
        scale_y = static_cast<float>(saved->output->target.height) / static_cast<float>(saved->output->render_height);
      }
      output_description.Width = saved->output->target.width;
      output_description.Height = saved->output->target.height;
    }
    const bool native_output_view = pass != nullptr && resolution::dof_plan.IsScaled()
                                    && !saved->output->IsSceneTarget(native_target.Get());
    if (!native_output_view && !fixed_bloom_reduction && native_description.Width != 0u && native_description.Height != 0u && saved->viewport_count != 0u
        && viewports[0].Width <= static_cast<float>(native_description.Width)
        && viewports[0].Height <= static_cast<float>(native_description.Height)) {
      scale_x = static_cast<float>(output_description.Width) / static_cast<float>(native_description.Width);
      scale_y = static_cast<float>(output_description.Height) / static_cast<float>(native_description.Height);
    }
    for (UINT i = 0u; i < saved->viewport_count; ++i) {
      viewports[i].TopLeftX *= scale_x;
      viewports[i].TopLeftY *= scale_y;
      viewports[i].Width *= scale_x;
      viewports[i].Height *= scale_y;
    }
    for (UINT i = 0u; i < saved->scissor_count; ++i) {
      scissors[i].left = static_cast<LONG>(std::floor(static_cast<float>(scissors[i].left) * scale_x));
      scissors[i].top = static_cast<LONG>(std::floor(static_cast<float>(scissors[i].top) * scale_y));
      scissors[i].right = static_cast<LONG>(std::ceil(static_cast<float>(scissors[i].right) * scale_x));
      scissors[i].bottom = static_cast<LONG>(std::ceil(static_cast<float>(scissors[i].bottom) * scale_y));
    }
    if (display_output && saved->viewport_count == 1u) {
      viewports[0].TopLeftX = 0.f;
      viewports[0].TopLeftY = 0.f;
      viewports[0].Width = static_cast<float>(saved->output->target.width);
      viewports[0].Height = static_cast<float>(saved->output->target.height);
      if (saved->scissor_count == 1u) {
        scissors[0] = {.left = 0, .top = 0, .right = static_cast<LONG>(saved->output->target.width), .bottom = static_cast<LONG>(saved->output->target.height)};
      }
    }
    struct ComputeBindings {
      ID3D11DeviceContext1* context;
      Microsoft::WRL::ComPtr<ID3D11ComputeShader> shader;
      std::array<ID3D11ClassInstance*, 256> classes = {};
      std::array<Microsoft::WRL::ComPtr<ID3D11ClassInstance>, 256> owned_classes;
      UINT class_count = static_cast<UINT>(classes.size());
      std::array<ID3D11Buffer*, 2> buffers = {};
      std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, 2> owned_buffers;
      std::array<UINT, 2> first = {};
      std::array<UINT, 2> counts = {};
      Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> writable;

      explicit ComputeBindings(ID3D11DeviceContext1* context) : context(context) {
        context->CSGetShader(&shader, classes.data(), &class_count);
        for (UINT i = 0u; i < class_count; ++i) {
          owned_classes[i].Attach(classes[i]);
        }
        context->CSGetConstantBuffers1(0u, 2u, buffers.data(), first.data(), counts.data());
        for (size_t i = 0u; i < buffers.size(); ++i) {
          owned_buffers[i].Attach(buffers[i]);
        }
        context->CSGetUnorderedAccessViews(0u, 1u, &writable);
      }
      ComputeBindings(const ComputeBindings&) = delete;
      ComputeBindings& operator=(const ComputeBindings&) = delete;
      ~ComputeBindings() {
        context->CSSetUnorderedAccessViews(0u, 1u, writable.GetAddressOf(), nullptr);
        context->CSSetConstantBuffers1(0u, 2u, buffers.data(), first.data(), counts.data());
        context->CSSetShader(shader.Get(), classes.data(), class_count);
      }
    };
    // Capture only when a patch dispatch is needed, once for this draw.
    std::optional<ComputeBindings> compute_bindings;
    const auto run_constants = [&](ID3D11ComputeShader* shader, ID3D11Buffer* constants, UINT first_constant, UINT constant_count) {
      auto* native_device = reinterpret_cast<ID3D11Device*>(command_list->get_device()->get_native());  // NOLINT(performance-no-int-to-ptr)
      if (draw->system_scratch == nullptr || draw->system_scratch_uav == nullptr) {
        const D3D11_BUFFER_DESC scratch = {.ByteWidth = 32u, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_UNORDERED_ACCESS, .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS};
        const D3D11_UNORDERED_ACCESS_VIEW_DESC scratch_view = {
            .Format = DXGI_FORMAT_R32_TYPELESS,
            .ViewDimension = D3D11_UAV_DIMENSION_BUFFER,
            .Buffer = {
                .FirstElement = 0u,
                .NumElements = 8u,
                .Flags = D3D11_BUFFER_UAV_FLAG_RAW,
            },
        };
        if (FAILED(native_device->CreateBuffer(&scratch, nullptr, draw->system_scratch.ReleaseAndGetAddressOf()))
            || FAILED(native_device->CreateUnorderedAccessView(draw->system_scratch.Get(), &scratch_view, draw->system_scratch_uav.ReleaseAndGetAddressOf()))) {
          resolution::Fail("post-processing constants scratch allocation failed");
          return false;
        }
      }
      if (!compute_bindings.has_value()) {
        compute_bindings.emplace(context1.Get());
        context->CSSetConstantBuffers(1u, 1u, patch.GetAddressOf());
      }
      context->CSSetShader(shader, nullptr, 0u);
      context1->CSSetConstantBuffers1(0u, 1u, &constants, &first_constant, &constant_count);
      context->CSSetUnorderedAccessViews(0u, 1u, draw->system_scratch_uav.GetAddressOf(), nullptr);
      context->Dispatch(1u, 1u, 1u);
      // The following copy reads scratch; it must no longer be bound for writes.
      context->CSSetUnorderedAccessViews(0u, 1u, std::array<ID3D11UnorderedAccessView*, 1>{nullptr}.data(), nullptr);
      return true;
    };
    // Buffer-copy boxes use byte offsets, unlike texture-copy rectangles.
    const auto copy_bytes = [&](ID3D11Buffer* destination, UINT destination_offset,
                                ID3D11Buffer* source, UINT source_offset, UINT bytes) {
      const D3D11_BOX region = {.left = source_offset, .top = 0u, .front = 0u, .right = source_offset + bytes, .bottom = 1u, .back = 1u};
      context1->CopySubresourceRegion1(destination, 0u, destination_offset, 0u, 0u, source, 0u, &region, 0u);
    };
    // Start from the native buffer, including its selected range, then replace
    // only the dimensions/offsets this shader reads. Most fields stay untouched.
    for (size_t i = 0u; i < saved->constants.size(); ++i) {
      if ((constant_mask & (1u << i)) == 0u) continue;
      if (i == 1u && pass != nullptr && pass->keep_scene_size) continue;
      if (i == 2u && (pass == nullptr || (pass->shader_hash != 0xFE1DC3F8u && pass->shader_hash != 0x9BE51873u && !patch_fixed_bloom))) continue;
      const UINT slot = CONSTANT_BINDINGS[i].slot;
      context1->PSGetConstantBuffers1(slot, 1u, &saved->constants[i], &saved->first_constants[i], &saved->constant_counts[i]);
      if (saved->constants[i] == nullptr) {
        if (!constant_reads_known) continue;
        resolution::Fail("required post-processing constants are not bound");
        *saved = {};
        return false;
      }
      D3D11_BUFFER_DESC description;
      saved->constants[i]->GetDesc(&description);
      const uint64_t offset_bytes = static_cast<uint64_t>(saved->first_constants[i]) * 16u;
      // The buffer and its bound range must both contain the native fields.
      // A large allocation does not make fields outside that range readable.
      if (offset_bytes > description.ByteWidth
          || CONSTANT_BINDINGS[i].minimum_bytes > description.ByteWidth - offset_bytes
          || static_cast<uint64_t>(saved->constant_counts[i]) * 16u < CONSTANT_BINDINGS[i].minimum_bytes) {
        if (!constant_reads_known) continue;
        resolution::Fail("required post-processing constant range is unavailable");
        *saved = {};
        return false;
      }
      const auto offset = static_cast<UINT>(offset_bytes);
      if (draw->output_constants[i] == nullptr || draw->output_constant_bytes[i] != description.ByteWidth) {
        description.Usage = D3D11_USAGE_DEFAULT;
        description.CPUAccessFlags = 0u;
        description.MiscFlags = 0u;
        description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        auto* native_device = reinterpret_cast<ID3D11Device*>(command_list->get_device()->get_native());  // NOLINT(performance-no-int-to-ptr)
        if (FAILED(native_device->CreateBuffer(&description, nullptr, draw->output_constants[i].ReleaseAndGetAddressOf()))) {
          resolution::Fail("post-processing constants allocation failed");
          *saved = {};
          return false;
        }
        draw->output_constant_bytes[i] = description.ByteWidth;
      }
      context->CopyResource(draw->output_constants[i].Get(), saved->constants[i].Get());
      if (i == 0u) {
        if (native_output_view) {
          // The immutable patch stores buffer size/inverses in its first row;
          // the compute scratch below stores them in its second row.
          copy_bytes(draw->output_constants[i].Get(), offset + SYSTEM_BUFFER_SIZE_OFFSET, patch.Get(), offsetof(SizePatch, buffer_size), 16u);
        } else {
          if (!run_constants(saved->output->pipelines->system_shader.Get(), saved->constants[i].Get(), saved->first_constants[i], saved->constant_counts[i])) {
            *saved = {};
            return false;
          }
          copy_bytes(draw->output_constants[i].Get(), offset + SYSTEM_VIEW_SIZE_OFFSET, draw->system_scratch.Get(), 0u, 8u);
          copy_bytes(draw->output_constants[i].Get(), offset + SYSTEM_BUFFER_SIZE_OFFSET, draw->system_scratch.Get(), 16u, 16u);
        }
      } else if (i == 1u) {
        // Scene b2 keeps the viewport dimensions in c22.xy.
        copy_bytes(draw->output_constants[i].Get(), offset + SCENE_VIEWPORT_SIZE_OFFSET, patch.Get(), offsetof(SizePatch, output_size), 8u);
      } else if (pass != nullptr && pass->shader_hash == 0x9BE51873u) {
        // Kawase stores four UV offsets in c9 and reciprocal size in c12.xy.
        // Preserve c12.zw and the rest of the native object buffer.
        if (!run_constants(saved->output->pipelines->bloom_shader.Get(), saved->constants[i].Get(), saved->first_constants[i], saved->constant_counts[i])) {
          *saved = {};
          return false;
        }
        copy_bytes(draw->output_constants[i].Get(), offset + OBJECT_LOCAL_PARAM_OFFSET, draw->system_scratch.Get(), 0u, 16u);
        copy_bytes(draw->output_constants[i].Get(), offset + OBJECT_LAST_PARAM_OFFSET, draw->system_scratch.Get(), 16u, 8u);
      } else if (patch_fixed_bloom) {
        // The native bloom region stays 246x138. Its offsets, flare center,
        // edge fade, and final-copy crop must describe the output-sized buffer.
        switch (pass->shader_hash) {
          case 0x72C1D157u: {
            copy_bytes(draw->output_constants[i].Get(), offset + OBJECT_LAST_PARAM_OFFSET, patch.Get(), offsetof(SizePatch, sample_step), 8u);
            break;
          }
          case 0x1AC1A5EFu: {
            copy_bytes(draw->output_constants[i].Get(), offset + OBJECT_LOCAL_PARAM_OFFSET, patch.Get(), offsetof(SizePatch, flare_center), 12u);
            copy_bytes(draw->output_constants[i].Get(), offset + OBJECT_LOCAL_PARAM_OFFSET + 16u, patch.Get(), offsetof(SizePatch, flare_edge), 8u);
            break;
          }
          case 0x265053A6u: {
            copy_bytes(draw->output_constants[i].Get(), offset + OBJECT_LOCAL_PARAM_OFFSET, patch.Get(), offsetof(SizePatch, flare_center), 8u);
            break;
          }
          case 0x83272BCBu: {
            copy_bytes(draw->output_constants[i].Get(), offset + OBJECT_LAST_PARAM_OFFSET, patch.Get(), offsetof(SizePatch, bloom_crop), 16u);
            break;
          }
        }
      } else {
        // Only the first scatter bake's cutoff follows output width.
        copy_bytes(draw->output_constants[i].Get(), offset + OBJECT_LAST_PARAM_OFFSET, patch.Get(), offsetof(SizePatch, dof_cutoff), 4u);
      }
      saved->changed_constants |= 1u << i;
    }
    if (saved->vertex_inputs_changed && native_output_view) {
      // DoF sprite UVs use VS b0's view/buffer ratio too. The native rectangle
      // helper already supplied the output view; only its buffer grid changes.
      context1->VSGetConstantBuffers1(0u, 1u, &saved->vertex_constants, &saved->vertex_first_constant, &saved->vertex_constant_count);
      if (saved->vertex_constants == nullptr) {
        resolution::Fail("required DoF vertex constants are not bound");
        *saved = {};
        return false;
      }
      D3D11_BUFFER_DESC description;
      saved->vertex_constants->GetDesc(&description);
      const uint64_t offset_bytes = static_cast<uint64_t>(saved->vertex_first_constant) * 16u;
      if (offset_bytes > description.ByteWidth || description.ByteWidth - offset_bytes < 64u
          || static_cast<uint64_t>(saved->vertex_constant_count) * 16u < 64u) {
        resolution::Fail("required DoF vertex constant range is unavailable");
        *saved = {};
        return false;
      }
      const auto offset = static_cast<UINT>(offset_bytes);
      if (draw->vertex_output_constants == nullptr || draw->vertex_output_constant_bytes != description.ByteWidth) {
        description.Usage = D3D11_USAGE_DEFAULT;
        description.CPUAccessFlags = 0u;
        description.MiscFlags = 0u;
        description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        auto* native_device = reinterpret_cast<ID3D11Device*>(command_list->get_device()->get_native());  // NOLINT(performance-no-int-to-ptr)
        if (FAILED(native_device->CreateBuffer(&description, nullptr, draw->vertex_output_constants.ReleaseAndGetAddressOf()))) {
          resolution::Fail("DoF vertex constants allocation failed");
          *saved = {};
          return false;
        }
        draw->vertex_output_constant_bytes = description.ByteWidth;
      }
      context->CopyResource(draw->vertex_output_constants.Get(), saved->vertex_constants.Get());
      copy_bytes(draw->vertex_output_constants.Get(), offset + SYSTEM_BUFFER_SIZE_OFFSET, patch.Get(), offsetof(SizePatch, buffer_size), 16u);
      saved->vertex_constants_changed = true;
    }
    saved->rectangles_changed = true;
    if (fixed_bloom_reduction && (saved->output->observed_passes.fetch_or(8192u, std::memory_order_relaxed) & 8192u) == 0u) {
      logging::Info("recorded fixed bloom region=", BLOOM_GRID_WIDTH, "x", BLOOM_GRID_HEIGHT,
                    " buffer=", native_description.Width, "x", native_description.Height,
                    " -> ", output_description.Width, "x", output_description.Height);
    }
    context->RSSetViewports(saved->viewport_count, viewports.data());
    context->RSSetScissorRects(saved->scissor_count, scissors.data());
  }
  for (size_t i = 0u; i < pending_surfaces.size(); ++i) {
    if (pending_clears[i].has_value()) {
      context->ClearRenderTargetView(targets[i], pending_clears[i]->color.data());
    }
    if (pending_surfaces[i] != nullptr) {
      const std::unique_lock lock(saved->output->surface_mutex);
      pending_surfaces[i]->routed_frame = route.frame;
      pending_surfaces[i]->role = (scene_only ? Surface::Role::SCENE_COLOR : target_role);
      if (route.producer != nullptr) {
        route.producer->SetSurfaceRole(pending_surfaces[i]->original.Get(), pending_surfaces[i]->role);
      }
    }
  }
  if (context1 != nullptr) {
    for (size_t i = 0u; i < saved->constants.size(); ++i) {
      if ((saved->changed_constants & (1u << i)) == 0u) continue;
      context1->PSSetConstantBuffers1(CONSTANT_BINDINGS[i].slot, 1u, draw->output_constants[i].GetAddressOf(), &saved->first_constants[i], &saved->constant_counts[i]);
    }
    if (saved->vertex_constants_changed) {
      context1->VSSetConstantBuffers1(0u, 1u, draw->vertex_output_constants.GetAddressOf(), &saved->vertex_first_constant, &saved->vertex_constant_count);
    }
  }
  // Keep native UAVs and pair redirected color with its qualified depth/stencil.
  if (saved->targets_changed) {
    context->OMSetRenderTargetsAndUnorderedAccessViews(saved->rtv_count, targets.data(), depth.Get(),
                                                       0u, D3D11_KEEP_UNORDERED_ACCESS_VIEWS, nullptr, nullptr);
  }
  if (saved->inputs_changed) {
    context->PSSetShaderResources(0u, static_cast<UINT>(inputs.size()), inputs.data());
  }
  if (saved->vertex_inputs_changed) {
    context->VSSetShaderResources(0u, static_cast<UINT>(vertex_inputs.size()), vertex_inputs.data());
  }
  if (pass != nullptr && pass->shader_hash == 0x83272BCBu && input_role == Surface::Role::FLARE_LAYER
      && targets[0] == saved->output->rtv.Get()
      && (saved->output->observed_passes.fetch_or(16384u, std::memory_order_relaxed) & 16384u) == 0u) {
    logging::Info("recorded flare layer composite to output scene size=", saved->output->target.width, "x", saved->output->target.height);
  }
  if (saved->output->IsScaled() && input_role == Surface::Role::SCENE_COLOR && pass != nullptr
      && (pass->shader_hash == 0x72C1D157u || pass->shader_hash == 0xFE1DC3F8u)) {
    // Release replaced views after unlocking; their callbacks can reenter here.
    Recording retired;
    const std::unique_lock lock(state.mutex);
    const auto sequence = std::ranges::find_if(state.sequences, [&](const Recording& candidate) {
      return candidate.owner.scene == route.owner.scene && candidate.owner.generation == route.owner.generation;
    });
    if (sequence == state.sequences.end()) {
      state.sequences.push_back(route);
    } else {
      retired = std::exchange(*sequence, route);
    }
  }
  if (context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED) {
    if (auto* recording = command_list->get_private_data<Recording>(); recording != nullptr) {
      RetainOutput(command_list, recording->output);
      route.retained_outputs = std::move(recording->retained_outputs);
      *recording = std::move(route);
    }
  }
  return true;
}

// Put the original draw state back so routing one effect cannot change the
// next native draw. Unbind redirected reads before restoring original targets.
inline void End(reshade::api::command_list* command_list) {
  auto* draw = command_list->get_private_data<DrawState>();
  if (draw == nullptr || draw->bindings.output == nullptr) return;
  auto* saved = &draw->bindings;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (saved->rectangles_changed) {
    context->RSSetViewports(saved->viewport_count, saved->viewports.data());
    context->RSSetScissorRects(saved->scissor_count, saved->scissors.data());
  }
  if (saved->changed_constants != 0u || saved->vertex_constants_changed) {
    Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context1;
    if (SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&context1)))) {
      for (size_t i = 0u; i < saved->constants.size(); ++i) {
        if ((saved->changed_constants & (1u << i)) == 0u) continue;
        context1->PSSetConstantBuffers1(CONSTANT_BINDINGS[i].slot, 1u, saved->constants[i].GetAddressOf(), &saved->first_constants[i], &saved->constant_counts[i]);
      }
      if (saved->vertex_constants_changed) {
        context1->VSSetConstantBuffers1(0u, 1u, saved->vertex_constants.GetAddressOf(), &saved->vertex_first_constant, &saved->vertex_constant_count);
      }
    }
  }
  std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> targets = {};
  std::array<ID3D11ShaderResourceView*, PIXEL_INPUT_COUNT> inputs = {};
  std::array<ID3D11ShaderResourceView*, 2> vertex_inputs = {};
  if (saved->inputs_changed) {
    context->PSSetShaderResources(0u, static_cast<UINT>(inputs.size()), inputs.data());
  }
  if (saved->vertex_inputs_changed) {
    context->VSSetShaderResources(0u, static_cast<UINT>(vertex_inputs.size()), vertex_inputs.data());
  }
  if (saved->targets_changed) {
    for (size_t i = 0u; i < targets.size(); ++i) {
      targets[i] = saved->rtvs[i].Get();
    }
    context->OMSetRenderTargetsAndUnorderedAccessViews(saved->rtv_count, targets.data(), saved->dsv.Get(),
                                                       0u, D3D11_KEEP_UNORDERED_ACCESS_VIEWS, nullptr, nullptr);
  }
  if (saved->inputs_changed) {
    for (size_t i = 0u; i < inputs.size(); ++i) {
      inputs[i] = saved->srvs[i].Get();
    }
    context->PSSetShaderResources(0u, static_cast<UINT>(inputs.size()), inputs.data());
  }
  if (saved->vertex_inputs_changed) {
    for (size_t index = 0u; index < vertex_inputs.size(); ++index) {
      vertex_inputs[index] = saved->vertex_srvs[index].Get();
    }
    context->VSSetShaderResources(0u, static_cast<UINT>(vertex_inputs.size()), vertex_inputs.data());
  }
  *saved = {};
}

// Forward qualified clears to the enlarged target and remember full clears for
// aliases created later. Return false so the native target is cleared as usual.
inline bool OnClearRenderTarget(reshade::api::command_list* command_list, reshade::api::resource_view view,
                                const float* color, uint32_t rect_count, const reshade::api::rect* rects) {
  const auto owner = rendering::native_context::GetScope().owner;
  if (owner.scene != 0u && (!owner.main_view || !owner.active_member || !owner.presentation_matches)) return false;
  const uint64_t scene = (owner.scene != 0u ? owner.scene : owner.presentation_scene);
  if (scene == 0u || !owner.presentation_matches) return false;
  Recording route;
  auto output = GetActive(command_list, &route);
  const uint64_t recording_frame = (output != nullptr ? route.frame : reconstruction::state::CurrentFrameToken());
  const auto plan = (output == nullptr ? resolution::Read() : resolution::Plan{});
  if (output == nullptr && !plan.IsScaled()) return false;
  const auto clone = renodx::utils::resource::upgrade::GetResourceViewClone(view);
  const auto resource = command_list->get_device()->get_resource_from_view(clone.handle == 0u ? view : clone);
  if (output == nullptr && resource.handle != 0u) {
    auto ui_route = GetUiRoute(command_list);
    if (ui_route.output != nullptr) {
      auto* native_resource = reinterpret_cast<ID3D11Resource*>(resource.handle);  // NOLINT(performance-no-int-to-ptr)
      bool matched = ui_route.output->IsSceneTarget(native_resource);
      if (!matched) {
        auto role = Surface::Role::DATA;
        if (ui_route.output->GetSurface(native_resource, false, recording_frame, nullptr, &role) != nullptr) {
          matched = role == Surface::Role::FLARE_LAYER;
        }
      }
      if (matched) {
        // Preserve native clear order even when no scene reconstruction runs.
        route = std::move(ui_route);
        output = route.output;
      }
    }
  }
  bool full_clear = rect_count == 0u;
  if (rect_count == 1u && rects != nullptr && resource.handle != 0u) {
    const auto description = command_list->get_device()->get_resource_desc(resource);
    full_clear = description.type == reshade::api::resource_type::texture_2d && rects[0].left <= 0 && rects[0].top <= 0
                 && std::cmp_greater_equal(rects[0].right, description.texture.width)
                 && std::cmp_greater_equal(rects[0].bottom, description.texture.height);
  }
  if ((output == nullptr || output->IsScaled()) && full_clear && resource.handle != 0u) {
    ClearColor clear = {.scene = scene, .generation = owner.generation};
    std::copy_n(color, clear.color.size(), clear.color.begin());
    RecordClear(reinterpret_cast<ID3D11Resource*>(resource.handle), clear);  // NOLINT(performance-no-int-to-ptr)
  }
  if (output == nullptr) return false;
  RetainOutput(command_list, output);
  const auto* draw = command_list->get_private_data<DrawState>();
  const bool dof = resolution::dof_plan.IsScaled() && draw != nullptr && draw->dof_active
                   && draw->dof_token == resolution::dof_token && draw->dof_frame == recording_frame;
  if (resolution::dof_plan.IsScaled() && !dof && !output->IsSceneTarget(reinterpret_cast<ID3D11Resource*>(resource.handle))) return false;  // NOLINT(performance-no-int-to-ptr)
  // Mirror clears for known output targets. A clear alone cannot identify a
  // post-processing surface; its first qualified draw can use the saved clear.
  auto mapped = output->MapTarget(reinterpret_cast<ID3D11RenderTargetView*>((clone.handle == 0u ? view : clone).handle), dof, !dof,  // NOLINT(performance-no-int-to-ptr)
                                  Surface::Role::DATA, recording_frame, route.producer.get(), false);
  if (mapped != nullptr) {
    Microsoft::WRL::ComPtr<ID3D11Resource> mapped_resource;
    mapped->GetResource(&mapped_resource);
    if (reinterpret_cast<uint64_t>(mapped_resource.Get()) == resource.handle) return false;
    auto* context = reinterpret_cast<ID3D11DeviceContext*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
    if (full_clear) {
      context->ClearRenderTargetView(mapped.Get(), color);
      if (route.producer != nullptr) {
        const std::unique_lock lock(output->surface_mutex);
        if (std::ranges::find(route.producer->cleared_surfaces, reinterpret_cast<ID3D11Resource*>(resource.handle)) == route.producer->cleared_surfaces.end()) {  // NOLINT(performance-no-int-to-ptr)
          route.producer->cleared_surfaces.push_back(reinterpret_cast<ID3D11Resource*>(resource.handle));                                                         // NOLINT(performance-no-int-to-ptr)
        }
      }
    } else {
      Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context1;
      if (SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&context1)))) {
        // Native DoF rectangle helpers already use output extents. Other
        // partial clears still need the render-to-output rectangle conversion.
        std::vector<D3D11_RECT> converted;
        const auto* native_rects = reinterpret_cast<const D3D11_RECT*>(rects);
        if (output->IsScaled() && !resolution::dof_plan.IsScaled()) {
          converted.assign(native_rects, native_rects + rect_count);
          const float scale_x = static_cast<float>(output->target.width) / static_cast<float>(output->render_width);
          const float scale_y = static_cast<float>(output->target.height) / static_cast<float>(output->render_height);
          for (auto& rectangle : converted) {
            rectangle.left = static_cast<LONG>(std::floor(static_cast<float>(rectangle.left) * scale_x));
            rectangle.top = static_cast<LONG>(std::floor(static_cast<float>(rectangle.top) * scale_y));
            rectangle.right = static_cast<LONG>(std::ceil(static_cast<float>(rectangle.right) * scale_x));
            rectangle.bottom = static_cast<LONG>(std::ceil(static_cast<float>(rectangle.bottom) * scale_y));
          }
          native_rects = converted.data();
        }
        context1->ClearView(mapped.Get(), color, native_rects, rect_count);
      }
    }
  }
  return false;
}

inline bool OnClearDepth(reshade::api::command_list* command_list, reshade::api::resource_view view,
                         const float* depth, const uint8_t* stencil, uint32_t rect_count, const reshade::api::rect* /*rects*/) {
  auto output = GetActive(command_list);
  if (output == nullptr) {
    output = GetUiRoute(command_list).output;
  }
  if (output == nullptr || !output->IsScaled() || output->native_depth == nullptr) return false;
  RetainOutput(command_list, output);
  auto* original = reinterpret_cast<ID3D11DepthStencilView*>(view.handle);  // NOLINT(performance-no-int-to-ptr)
  if (original == nullptr) return false;
  Microsoft::WRL::ComPtr<ID3D11Resource> resource;
  original->GetResource(&resource);
  if (resource != output->native_depth) return false;
  if (rect_count != 0u) {
    resolution::Fail("unsupported partial output depth/stencil clear");
    return false;
  }
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  const UINT flags = (depth != nullptr ? D3D11_CLEAR_DEPTH : 0u) | (stencil != nullptr ? D3D11_CLEAR_STENCIL : 0u);
  if (output->output_depth_views[0] != nullptr) {
    context->ClearDepthStencilView(output->output_depth_views[0].Get(), flags, depth != nullptr ? *depth : 0.f, stencil != nullptr ? *stencil : 0u);
  }
  return false;
}

inline bool OnCopyResource(reshade::api::command_list* command_list, reshade::api::resource from, reshade::api::resource to) {
  Recording route;
  const auto output = GetActive(command_list, &route);
  if (output == nullptr || !output->IsScaled()) return false;
  const auto source_clone = renodx::utils::resource::upgrade::GetResourceClone(from, {.allow_create = false});
  const auto destination_clone = renodx::utils::resource::upgrade::GetResourceClone(to, {.allow_create = false});
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  const bool copied = output->CopySurface(context,
                                          reinterpret_cast<ID3D11Resource*>((source_clone.handle != 0u ? source_clone : from).handle),          // NOLINT(performance-no-int-to-ptr)
                                          reinterpret_cast<ID3D11Resource*>((destination_clone.handle != 0u ? destination_clone : to).handle),  // NOLINT(performance-no-int-to-ptr)
                                          route.frame, route.producer.get());
  if (copied) {
    RetainOutput(command_list, output);
  }
  return copied;
}

// Existing native draw families that need output-size routing. This table
// describes interception rules; its order does not schedule GPU execution.
inline constexpr auto PASSES = std::to_array<Pass>({
    {.shader_hash = 0x72C1D157u, .observation_bit = 1u, .name = "bloom reduction", .resize_targets = true, .require_scene_input = true, .role = Surface::Role::BLOOM},
    {.shader_hash = 0x463195C2u, .observation_bit = 256u, .name = "bloom bright pass", .resize_targets = true, .require_scene_input = true, .role = Surface::Role::BLOOM},
    {.shader_hash = 0x1AC1A5EFu, .observation_bit = 512u, .name = "lens flare first pass", .resize_targets = true, .require_scene_input = true, .role = Surface::Role::BLOOM},
    {.shader_hash = 0x265053A6u, .observation_bit = 1024u, .name = "lens flare second pass", .resize_targets = true, .require_scene_input = true, .role = Surface::Role::BLOOM},
    {.shader_hash = 0x9BE51873u, .observation_bit = 2048u, .name = "bloom blur", .resize_targets = true, .require_scene_input = true, .role = Surface::Role::BLOOM},
    {.shader_hash = 0xFE1DC3F8u, .observation_bit = 2u, .name = "depth of field", .resize_targets = true, .dof_pass = true},
    {.shader_hash = 0x490E22EBu, .observation_bit = 2u, .name = "depth of field", .resize_targets = true, .dof_pass = true},
    {.shader_hash = 0xE26983EEu, .observation_bit = 2u, .name = "depth of field", .resize_targets = true, .dof_pass = true},
    {.shader_hash = 0xD05F5ABCu, .observation_bit = 128u, .name = "depth of field sprites", .resize_targets = true, .dof_pass = true},
    {.shader_hash = 0xE2D609B1u, .observation_bit = 2u, .name = "depth of field", .resize_targets = true, .dof_pass = true},
    {.shader_hash = 0x7C017264u, .observation_bit = 2u, .name = "depth of field", .resize_targets = true, .dof_pass = true},
    {.shader_hash = 0xFC5542BBu, .observation_bit = 2u, .name = "depth of field", .resize_targets = true, .dof_pass = true},
    {.shader_hash = 0x83272BCBu, .observation_bit = 4u, .name = "scene copy / motion blur", .resize_targets = true, .require_scene_input = true},
    {.shader_hash = 0x512E2B48u, .observation_bit = 4u, .name = "motion blur input", .resize_targets = true, .keep_scene_size = true},
    {.shader_hash = 0xBFC7D3C2u, .observation_bit = 4u, .name = "motion blur", .resize_targets = true, .role = Surface::Role::SCENE_COLOR},
    {.shader_hash = 0xE04D1471u, .observation_bit = 8u, .name = "tonemap"},
    {.shader_hash = 0xC0C26E46u, .observation_bit = 8u, .name = "tonemap"},
    {.shader_hash = 0x900968FFu, .observation_bit = 16u, .name = "FXAA / grain"},
    {.shader_hash = 0x1FF96E86u, .observation_bit = 32u, .name = "display output", .display_output = true},
    {.shader_hash = LENS_FLARE_SHADER_HASH, .observation_bit = 4096u, .name = "depth-tested lens flares", .resize_targets = true, .role = Surface::Role::FLARE_LAYER},
});

inline constexpr Pass DRAW_2D_PASS = {.shader_hash = 0u, .observation_bit = 64u, .name = "2D / UI"};

inline constexpr auto DRAW_CALLBACK = [](auto& command) {
  using Context = std::remove_reference_t<decltype(command)>;
  if (command.IsDispatch()) return renodx::utils::command_action::CallbackResult<Context>{};
  const auto& pass = command.template GetCallbackData<Pass>();
  auto* draw = command.cmd_list->template get_private_data<DrawState>();
  if (pass.shader_hash == 0u && (draw == nullptr || !draw->draw_2d || draw->flare_sprite)) return renodx::utils::command_action::CallbackResult<Context>{};
  if (pass.shader_hash == 0u && resolution::dof_plan.IsScaled()) return renodx::utils::command_action::CallbackResult<Context>{};
  if (pass.dof_pass && draw != nullptr) {
    Recording route;
    const auto output = GetActive(command.cmd_list, &route);
    if (output != nullptr && output->IsScaled()) {
      if (pass.shader_hash == 0xFE1DC3F8u) {
        auto* context = reinterpret_cast<ID3D11DeviceContext*>(command.cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> input;
        Microsoft::WRL::ComPtr<ID3D11Resource> resource;
        context->PSGetShaderResources(0u, 1u, &input);
        if (input != nullptr) {
          input->GetResource(&resource);
        }
        if (resource == output->source) {
          draw->dof_token = resolution::dof_token;
          draw->dof_frame = route.frame;
          draw->dof_active = true;
        }
      }
      if (!draw->dof_active || draw->dof_token != resolution::dof_token
          || draw->dof_frame != route.frame) return renodx::utils::command_action::CallbackResult<Context>{};
    }
  }
  // Display conversion closes the open route. Parallel workers can still use
  // the pooled output and its clear records. Startup scenes with no output
  // release their pending clears here so they cannot accumulate across frames.
  static constexpr auto RETIRE_SCENE = [] {
    const auto owner = rendering::native_context::GetScope().owner;
    const uint64_t scene = (owner.scene != 0u ? owner.scene : owner.presentation_scene);
    if (scene == 0u) return;
    // Release resource references after dropping the state lock. Other scenes
    // keep their clears, and recorded command lists retain their output textures.
    Recording retired;
    std::vector<RecordedClear> retired_clears;
    const std::unique_lock lock(state.mutex);
    // Lookup records have no ordering requirement. Group completed entries so
    // removing several clears does not repeatedly shift the remaining records.
    const auto completed = std::ranges::partition(state.clears, [&](const RecordedClear& clear) {
      return clear.value.scene != scene || clear.value.generation != owner.generation;
    });
    retired_clears.assign(std::make_move_iterator(completed.begin()), std::make_move_iterator(completed.end()));
    state.clears.erase(completed.begin(), completed.end());
    const auto sequence = std::ranges::find_if(state.sequences, [&](const Recording& candidate) {
      return candidate.owner.scene == scene && candidate.owner.generation == owner.generation;
    });
    if (sequence != state.sequences.end()) {
      retired = std::move(*sequence);
      state.sequences.erase(sequence);
    }
    if (state.active_owner.scene == scene && state.active_owner.generation == owner.generation) {
      state.active.reset();
      state.producer.reset();
      state.active_owner = {};
    }
  };
  if (!Begin(command.cmd_list, &pass)) {
    if (!pass.display_output) return renodx::utils::command_action::CallbackResult<Context>{};
    // Startup can reach display conversion before reconstruction has valid
    // inputs. Finish its bookkeeping after the native draw even in that case.
    return renodx::utils::command_action::CallbackResult<Context>{
        .finalize_callback = [](Context&, const void*) {
          RETIRE_SCENE();
        },
    };
  }
  const auto* saved = &draw->bindings;
  if ((saved->output->observed_passes.fetch_or(pass.observation_bit, std::memory_order_relaxed) & pass.observation_bit) == 0u) {
    auto* context = reinterpret_cast<ID3D11DeviceContext*>(command.cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pixel_input;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vertex_input;
    context->OMGetRenderTargets(1u, &target, nullptr);
    context->PSGetShaderResources(0u, 1u, &pixel_input);
    context->VSGetShaderResources(0u, 1u, &vertex_input);
    const auto describe = [](auto* view) {
      if (view == nullptr) return std::string("none");
      using Description = std::conditional_t<std::is_same_v<std::remove_pointer_t<decltype(view)>, ID3D11RenderTargetView>,
                                             D3D11_RENDER_TARGET_VIEW_DESC, D3D11_SHADER_RESOURCE_VIEW_DESC>;
      Description sample;
      view->GetDesc(&sample);
      Microsoft::WRL::ComPtr<ID3D11Resource> resource;
      Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
      view->GetResource(&resource);
      if (FAILED(resource.As(&texture))) return std::string("non-texture");
      D3D11_TEXTURE2D_DESC storage;
      texture->GetDesc(&storage);
      std::ostringstream text;
      text << storage.Width << "x" << storage.Height << "/storage:" << static_cast<uint32_t>(storage.Format)
           << "/view:" << static_cast<uint32_t>(sample.Format);
      return text.str();
    };
    const auto original_input = describe(saved->srvs[0].Get());
    const auto mapped_input = describe(pixel_input.Get());
    const auto mapped_target = describe(target.Get());
    const auto mapped_vertex = describe(vertex_input.Get());
    logging::Info("recorded separate scene route pass=", pass.name, " shader=", logging::Hex{pass.shader_hash},
                  " ps0=", original_input, " -> ", mapped_input, " rtv0=", mapped_target, " vs0=", mapped_vertex);
  }
  return renodx::utils::command_action::CallbackResult<Context>{
      .post_data = &pass,
      .replay = true,
      .finalize_callback = [](Context& context, const void* data) {
        End(context.cmd_list);
        if (static_cast<const Pass*>(data)->display_output) {
          RETIRE_SCENE();
        }
      },
  };
};

// Register after shader replacements so their own clone activation runs first.
inline void Use(DWORD reason) {
  if (reason == DLL_PROCESS_ATTACH) {
    reshade::register_event<reshade::addon_event::init_command_list>(OnInitCommandList);
    reshade::register_event<reshade::addon_event::init_pipeline>(OnInitPipeline);
    reshade::register_event<reshade::addon_event::bind_pipeline>(OnBindPipeline);
    reshade::register_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(OnBindTargets);
    reshade::register_event<reshade::addon_event::reset_command_list>(OnResetCommandList);
    reshade::register_event<reshade::addon_event::destroy_command_list>(OnDestroyCommandList);
    reshade::register_event<reshade::addon_event::clear_render_target_view>(OnClearRenderTarget);
    reshade::register_event<reshade::addon_event::clear_depth_stencil_view>(OnClearDepth);
    renodx::utils::command_action::Register(DRAW_CALLBACK,
                                            {.command_types = renodx::utils::command_action::COMMAND_TYPE_DIRECT_DRAW | renodx::utils::command_action::COMMAND_TYPE_INDIRECT}, &DRAW_2D_PASS);
    for (const auto& pass : PASSES) {
      renodx::utils::command_action::Register(DRAW_CALLBACK, {.shader_hash = pass.shader_hash, .command_types = renodx::utils::command_action::COMMAND_TYPE_DIRECT_DRAW}, &pass);
    }
  } else if (reason == DLL_PROCESS_DETACH) {
    renodx::utils::command_action::Unregister(DRAW_CALLBACK);
    reshade::unregister_event<reshade::addon_event::init_command_list>(OnInitCommandList);
    reshade::unregister_event<reshade::addon_event::init_pipeline>(OnInitPipeline);
    reshade::unregister_event<reshade::addon_event::bind_pipeline>(OnBindPipeline);
    reshade::unregister_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(OnBindTargets);
    reshade::unregister_event<reshade::addon_event::reset_command_list>(OnResetCommandList);
    reshade::unregister_event<reshade::addon_event::destroy_command_list>(OnDestroyCommandList);
    reshade::unregister_event<reshade::addon_event::clear_render_target_view>(OnClearRenderTarget);
    reshade::unregister_event<reshade::addon_event::clear_depth_stencil_view>(OnClearDepth);
  }
}

}  // namespace mgsv::reconstruction::post_process

#endif  // RENODX_GAMES_MGSV_RECONSTRUCTION_POSTPROCESS_RUNTIME_HPP_
