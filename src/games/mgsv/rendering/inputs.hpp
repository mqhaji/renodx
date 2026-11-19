#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>

#include <d3d11.h>
#include <wrl/client.h>

#include <include/reshade.hpp>

#include "../../../utils/bitwise.hpp"
#include "../../../utils/descriptor.hpp"

// Records the pixel shader views bound at t2 and t3 on each command list, so
// the camera-velocity draw can find its depth and object-velocity inputs.
namespace mgsv::inputs::descriptor_tracker {

struct __declspec(uuid("9F1A3A38-9D2A-4E5B-B5A3-3F2D0F38CD33")) CommandListData {
  // The pixel shader views most recently bound on this command list.
  reshade::api::resource_view pixel_srv_t2 = {0};  // Depth at MotionBlurCameraVelocity
  reshade::api::resource_view pixel_srv_t3 = {0};  // Object velocity at MotionBlurCameraVelocity
};

inline CommandListData* Get(reshade::api::command_list* cmd_list) {
  if (cmd_list == nullptr) return nullptr;
  // ReShade creates this state in init_command_list. Do not allocate lazily
  // from descriptor or draw callbacks, which may run concurrently.
  return cmd_list->get_private_data<CommandListData>();
}

inline void OnInitCommandList(reshade::api::command_list* cmd_list) {
  cmd_list->create_private_data<CommandListData>();
}

inline void OnDestroyCommandList(reshade::api::command_list* cmd_list) {
  cmd_list->destroy_private_data<CommandListData>();
}

inline void OnResetCommandList(reshade::api::command_list* cmd_list) {
  auto* data = Get(cmd_list);
  if (data != nullptr) *data = {};
}

inline void OnPushDescriptors(
    reshade::api::command_list* cmd_list,
    reshade::api::shader_stage stages,
    reshade::api::pipeline_layout /*layout*/,
    uint32_t /*layout_param*/,
    const reshade::api::descriptor_table_update& update) {
  if (!renodx::utils::bitwise::HasFlag(stages, reshade::api::shader_stage::pixel)
      || update.type != reshade::api::descriptor_type::shader_resource_view) return;

  auto* data = Get(cmd_list);
  if (data == nullptr) return;

  for (uint32_t i = 0u; i < update.count; ++i) {
    // For D3D11, ReShade reports bindings as shader register numbers.
    const uint32_t slot = update.binding + i;
    if (slot != 2u && slot != 3u) continue;

    const auto view = renodx::utils::descriptor::GetResourceViewFromDescriptorUpdate(update, i);
    switch (slot) {
      case 2u: data->pixel_srv_t2 = view; break;
      case 3u: data->pixel_srv_t3 = view; break;
      default: break;
    }
  }
}

}  // namespace mgsv::inputs::descriptor_tracker

// Selects the game's device and swapchain (the first D3D11 swapchain to present)
// and keeps the native views later passes read: depth and velocity from the
// camera-velocity draw, and the albedo and specular views from deferred
// composition. The generation changes whenever the swapchain or device retires,
// so a capture that finishes after that is rejected.
namespace mgsv::inputs {

// Holds references to views and their resources, not their contents. A filled-in
// Motion or Snapshot does not mean the contents are current or that the GPU has
// run the draw that writes them.
struct Motion {
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> velocity;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> object_velocity;
  reshade::api::resource velocity_resource = {0};
  reshade::api::format velocity_format = reshade::api::format::unknown;
  uint64_t generation = 0;
  uint64_t observation = 0;
  uintptr_t context = 0;
  uintptr_t producer_rtv = 0;  // Nonowning identity; never dereferenced.
};

struct Materials {
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> albedo;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> specular;
  uint64_t generation = 0;
  uint64_t observation = 0;
  uintptr_t context = 0;
};

struct Snapshot {
  reshade::api::swapchain* swapchain = nullptr;  // Borrowed; stays selected across a resize.
  reshade::api::device* device = nullptr;
  uint64_t generation = 0;
  Motion motion;
  Materials materials;
};

struct Selection {
  reshade::api::device* device = nullptr;
  uint64_t generation = 0u;
};

namespace internal {

struct State {
  std::shared_mutex mutex;
  Snapshot snapshot;
  bool accepting = false;
  bool generation_exhausted = false;
  uint64_t next_observation = 0;
  // The newest capture ticket published to each slot. A ticket stays raised after a failed view
  // creation clears its slot, so an older capture cannot fill the slot again.
  uint64_t motion_ticket = 0;
  uint64_t materials_ticket = 0;
};

static State state;

// The returned context is borrowed; use it only inside the caller's callback.
inline ID3D11DeviceContext* GetContext(reshade::api::command_list* cmd_list, reshade::api::device* device) {
  if (cmd_list == nullptr || device == nullptr || device->get_api() != reshade::api::device_api::d3d11) return nullptr;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (context == nullptr) return nullptr;
  Microsoft::WRL::ComPtr<ID3D11Device> owner;
  context->GetDevice(&owner);
  if (owner.Get() != reinterpret_cast<ID3D11Device*>(device->get_native())) return nullptr;  // NOLINT(performance-no-int-to-ptr)
  return context;
}

inline Microsoft::WRL::ComPtr<ID3D11Texture2D> ReadTexture(
    ID3D11View* view, ID3D11Device* device, D3D11_TEXTURE2D_DESC* description) {
  if (view == nullptr) return {};
  Microsoft::WRL::ComPtr<ID3D11Device> owner;
  view->GetDevice(&owner);
  if (owner.Get() != device) return {};
  Microsoft::WRL::ComPtr<ID3D11Resource> resource;
  view->GetResource(&resource);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  if (resource == nullptr || FAILED(resource.As(&texture))) return {};
  texture->GetDesc(description);
  return texture;
}

}  // namespace internal

// The selected device and swapchain stay visible while capture is paused for a resize or retirement.
inline bool IsSelectedDevice(reshade::api::device* device) {
  const std::shared_lock lock(internal::state.mutex);
  return device != nullptr && internal::state.snapshot.device == device;
}

inline bool IsSelectedSwapchain(reshade::api::swapchain* swapchain) {
  const std::shared_lock lock(internal::state.mutex);
  return swapchain != nullptr && internal::state.snapshot.swapchain == swapchain;
}

inline Snapshot Read(reshade::api::device* device) {
  const std::shared_lock lock(internal::state.mutex);
  if (!internal::state.accepting || device == nullptr || internal::state.snapshot.device != device) return {};
  // This is a leaf lock: copying the ComPtrs is the only work done while it is held.
  return internal::state.snapshot;
}

// Consumers that only need retirement metadata do not copy retained views.
inline Selection ReadSelection(reshade::api::device* device) {
  const std::shared_lock lock(internal::state.mutex);
  if (!internal::state.accepting || device == nullptr || internal::state.snapshot.device != device) return {};
  return {.device = device, .generation = internal::state.snapshot.generation};
}

inline void OnInitSwapchain(reshade::api::swapchain* swapchain, bool resize) {
  if (!resize || !IsSelectedSwapchain(swapchain)) return;
  auto* device = swapchain->get_device();
  if (device == nullptr || device->get_api() != reshade::api::device_api::d3d11) return;
  const std::unique_lock lock(internal::state.mutex);
  if (internal::state.generation_exhausted || internal::state.snapshot.swapchain != swapchain
      || internal::state.snapshot.device != device) return;
  internal::state.accepting = true;
}

// addon.cpp calls this from its Present callback, which ReShade runs before reconstruction's.
// The first D3D11 swapchain to present is selected, and only one device is ever selected. This
// does not prove that the swapchain shows the game's scene rather than other rendering.
inline void OnPresent(
    reshade::api::command_queue* queue,
    reshade::api::swapchain* swapchain,
    const reshade::api::rect* source_rect,
    const reshade::api::rect* dest_rect,
    uint32_t dirty_rect_count,
    const reshade::api::rect* dirty_rects) {
  (void)source_rect;
  (void)dest_rect;
  (void)dirty_rect_count;
  (void)dirty_rects;
  if (queue == nullptr || swapchain == nullptr) return;
  auto* device = swapchain->get_device();
  if (device == nullptr || device != queue->get_device()
      || device->get_api() != reshade::api::device_api::d3d11) return;
  uint64_t generation = 0;
  {
    const std::unique_lock lock(internal::state.mutex);
    auto& snapshot = internal::state.snapshot;
    if (internal::state.generation_exhausted || snapshot.swapchain != nullptr
        || (snapshot.device != nullptr && snapshot.device != device)) return;
    if (snapshot.generation == std::numeric_limits<uint64_t>::max()) {
      internal::state.generation_exhausted = true;
      return;
    }
    ++snapshot.generation;
    snapshot.swapchain = swapchain;
    snapshot.device = device;
    internal::state.accepting = true;
    generation = snapshot.generation;
  }
  reshade::log::message(reshade::log::level::info,
                        ("MgsvInputs: device bound; generation=" + std::to_string(generation)).c_str());
}

// addon.cpp calls this from destroy_swapchain, which runs before the native ResizeBuffers.
// Consumers must release their copied snapshots before the resize; nothing here waits for them.
inline void OnDestroySwapchain(reshade::api::swapchain* swapchain, bool resize) {
  if (!IsSelectedSwapchain(swapchain)) return;
  Snapshot retired;
  uint64_t generation = 0;
  {
    const std::unique_lock lock(internal::state.mutex);
    if (swapchain == nullptr || internal::state.snapshot.swapchain != swapchain) return;
    internal::state.accepting = false;
    retired = std::move(internal::state.snapshot);
    generation = retired.generation;
    if (generation == std::numeric_limits<uint64_t>::max()) {
      internal::state.generation_exhausted = true;
    } else {
      ++generation;
    }
    internal::state.snapshot = {
        .swapchain = (resize ? retired.swapchain : nullptr),
        .device = retired.device,
        .generation = generation,
    };
  }
  retired = {};
  reshade::log::message(reshade::log::level::info,
                        ("MgsvInputs: swapchain retired; generation=" + std::to_string(generation)).c_str());
}

inline void OnDestroyDevice(reshade::api::device* device) {
  if (!IsSelectedDevice(device)) return;
  Snapshot retired;
  uint64_t generation = 0;
  {
    const std::unique_lock lock(internal::state.mutex);
    if (device == nullptr || internal::state.snapshot.device != device) return;
    internal::state.accepting = false;
    retired = std::move(internal::state.snapshot);
    generation = retired.generation;
    if (generation == std::numeric_limits<uint64_t>::max()) {
      internal::state.generation_exhausted = true;
    } else {
      ++generation;
    }
    internal::state.snapshot = {.generation = generation};
  }
  retired = {};
  reshade::log::message(reshade::log::level::info,
                        ("MgsvInputs: device retired; generation=" + std::to_string(generation)).c_str());
}

// Called at the camera-velocity draw; both callers hold the reconstruction ExecutionGuard.
// Generation checks reject a capture that
// finishes after its selection retired; they do not synchronize native context use or
// device destruction.
inline Motion CaptureMotion(
    reshade::api::command_list* cmd_list,
    reshade::api::resource_view expected_velocity_rtv) {
  if (cmd_list == nullptr) return {};
  auto* device = cmd_list->get_device();
  Snapshot previous;
  uint64_t ticket = 0;
  {
    const std::unique_lock lock(internal::state.mutex);
    if (!internal::state.accepting || device == nullptr || internal::state.snapshot.device != device
        || internal::state.next_observation == std::numeric_limits<uint64_t>::max()) return {};
    ticket = ++internal::state.next_observation;
    previous = internal::state.snapshot;
  }
  auto* context = internal::GetContext(cmd_list, device);
  if (context == nullptr) return {};

  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> producer;
  context->OMGetRenderTargets(1, producer.GetAddressOf(), nullptr);
  std::array<ID3D11ShaderResourceView*, 2> pixel_views = {};
  context->PSGetShaderResources(2, static_cast<UINT>(pixel_views.size()), pixel_views.data());
  Motion motion;
  motion.depth.Attach(pixel_views[0]);
  motion.object_velocity.Attach(pixel_views[1]);
  // Missing bindings leave the previous capture in place.
  if (producer == nullptr || motion.depth == nullptr || motion.object_velocity == nullptr) return {};
  // Compare the expected target only by address. The reference kept is the one the
  // getter returned; the expected handle is never turned into an owning COM pointer.
  if (expected_velocity_rtv.handle != 0
      && expected_velocity_rtv.handle != reinterpret_cast<uintptr_t>(producer.Get())) return {};

  auto* native_device = reinterpret_cast<ID3D11Device*>(device->get_native());  // NOLINT(performance-no-int-to-ptr)
  D3D11_TEXTURE2D_DESC velocity_description = {};
  D3D11_TEXTURE2D_DESC depth_description = {};
  D3D11_TEXTURE2D_DESC object_description = {};
  const auto velocity_texture = internal::ReadTexture(producer.Get(), native_device, &velocity_description);
  const auto depth_texture = internal::ReadTexture(motion.depth.Get(), native_device, &depth_description);
  const auto object_texture = internal::ReadTexture(motion.object_velocity.Get(), native_device, &object_description);
  if (velocity_texture == nullptr || depth_texture == nullptr || object_texture == nullptr) return {};

  D3D11_RENDER_TARGET_VIEW_DESC producer_description = {};
  producer->GetDesc(&producer_description);
  // Reconstruction checks sizes and single-mip, single-sample layouts itself when it runs.
  // Lighting also uses this capture, so those checks do not belong here.

  motion.velocity_resource = {reinterpret_cast<uintptr_t>(velocity_texture.Get())};
  motion.velocity_format = static_cast<reshade::api::format>(producer_description.Format);
  if (motion.velocity_format == reshade::api::format::unknown) {
    motion.velocity_format = reshade::api::format_to_default_typed(
        static_cast<reshade::api::format>(velocity_description.Format));
  }
  if (motion.velocity_format == reshade::api::format::unknown
      || reshade::api::format_is_typeless(motion.velocity_format)) return {};
  motion.generation = previous.generation;
  motion.observation = ticket;
  motion.context = reinterpret_cast<uintptr_t>(context);
  motion.producer_rtv = reinterpret_cast<uintptr_t>(producer.Get());

  // The velocity SRV covers mip 0 and slice 0. Reuse the cached one while the texture and
  // format are unchanged.
  if (previous.motion.velocity != nullptr
      && previous.motion.velocity_resource.handle == motion.velocity_resource.handle
      && previous.motion.velocity_format == motion.velocity_format) {
    motion.velocity = previous.motion.velocity;
  } else {
    reshade::api::resource_view velocity_view = {0};
    if (!device->create_resource_view(
            motion.velocity_resource,
            reshade::api::resource_usage::shader_resource,
            reshade::api::resource_view_desc(reshade::api::resource_view_type::texture_2d,
                                             motion.velocity_format, 0, 1, 0, 1),
            &velocity_view)) {
      Motion retired_motion;
      bool cleared = false;
      {
        const std::unique_lock lock(internal::state.mutex);
        auto& snapshot = internal::state.snapshot;
        if (internal::state.accepting && snapshot.device == device && snapshot.generation == previous.generation) {
          // Do not erase a newer publication if a callback reentered during creation.
          if (ticket > internal::state.motion_ticket
              && snapshot.motion.observation == previous.motion.observation) {
            internal::state.motion_ticket = ticket;
            retired_motion = std::move(snapshot.motion);
            snapshot.motion = {};
            cleared = retired_motion.velocity != nullptr;
          }
        }
      }
      // A failed view creation clears the cached capture.
      if (cleared) {
        reshade::log::message(reshade::log::level::warning,
                              "MgsvInputs: velocity SRV creation failed; previous selection retired");
      }
      return {};
    }
    // D3D11 create_resource_view returns one owned COM reference. Adopt it exactly once.
    motion.velocity.Attach(reinterpret_cast<ID3D11ShaderResourceView*>(velocity_view.handle));  // NOLINT(performance-no-int-to-ptr)
    reshade::log::message(reshade::log::level::info,
                          (std::string("MgsvInputs: created velocity SRV; format=")
                           + std::to_string(static_cast<uint32_t>(motion.velocity_format)))
                              .c_str());
  }

  Motion retired_motion;
  {
    const std::unique_lock lock(internal::state.mutex);
    auto& snapshot = internal::state.snapshot;
    if (!internal::state.accepting || snapshot.device != device || snapshot.generation != previous.generation) return {};
    // An older capture that finishes after a newer one is not published.
    if (ticket <= internal::state.motion_ticket) return {};
    internal::state.motion_ticket = ticket;
    retired_motion = std::move(snapshot.motion);
    snapshot.motion = motion;
  }
  return motion;
}

// Called at each deferred composition draw to keep the G-buffer albedo (t8) and
// specular (t10) views for the lighting passes.
inline void CaptureCompositeInputs(reshade::api::command_list* cmd_list) {
  if (cmd_list == nullptr) return;
  auto* device = cmd_list->get_device();
  uint64_t generation = 0;
  uint64_t ticket = 0;
  {
    const std::unique_lock lock(internal::state.mutex);
    if (!internal::state.accepting || device == nullptr || internal::state.snapshot.device != device
        || internal::state.next_observation == std::numeric_limits<uint64_t>::max()) return;
    ticket = ++internal::state.next_observation;
    generation = internal::state.snapshot.generation;
  }
  auto* context = internal::GetContext(cmd_list, device);
  if (context == nullptr) return;
  Materials materials;
  context->PSGetShaderResources(8, 1, materials.albedo.GetAddressOf());
  context->PSGetShaderResources(10, 1, materials.specular.GetAddressOf());
  if (materials.albedo == nullptr || materials.specular == nullptr) return;

  auto* native_device = reinterpret_cast<ID3D11Device*>(device->get_native());  // NOLINT(performance-no-int-to-ptr)
  D3D11_TEXTURE2D_DESC description = {};
  D3D11_SHADER_RESOURCE_VIEW_DESC albedo_description = {};
  D3D11_SHADER_RESOURCE_VIEW_DESC specular_description = {};
  materials.albedo->GetDesc(&albedo_description);
  materials.specular->GetDesc(&specular_description);
  if (albedo_description.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D
      || specular_description.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D
      || internal::ReadTexture(materials.albedo.Get(), native_device, &description) == nullptr
      || internal::ReadTexture(materials.specular.Get(), native_device, &description) == nullptr) return;
  // Keep the albedo and specular views as a pair. Their sizes need not match, and they are
  // captured whether or not any feature needs motion.
  materials.generation = generation;
  materials.observation = ticket;
  materials.context = reinterpret_cast<uintptr_t>(context);
  Materials retired;
  bool first_capture = false;
  {
    const std::unique_lock lock(internal::state.mutex);
    auto& snapshot = internal::state.snapshot;
    if (!internal::state.accepting || snapshot.device != device || snapshot.generation != generation
        || ticket <= internal::state.materials_ticket) return;
    first_capture = snapshot.materials.observation == 0;
    internal::state.materials_ticket = ticket;
    retired = std::move(snapshot.materials);
    snapshot.materials = std::move(materials);
  }
  if (first_capture) {
    reshade::log::message(reshade::log::level::info,
                          (std::string("MgsvInputs: composite views albedo=")
                           + std::to_string(static_cast<uint32_t>(albedo_description.Format))
                           + " specular=" + std::to_string(static_cast<uint32_t>(specular_description.Format)))
                              .c_str());
  }
}

}  // namespace mgsv::inputs
