#pragma once

#include <cstdint>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <type_traits>
#include <utility>

#include <d3d11.h>
#include <wrl/client.h>

#include <include/reshade.hpp>

#include "../../utils/bitwise.hpp"
#include "../../utils/descriptor.hpp"
#include "../../utils/pipeline_layout.hpp"
#include "../../utils/resource.hpp"
#include "../../utils/resource_upgrade.hpp"

namespace mgsv::inputs::descriptor_tracker {

using RegisterSlot = std::pair<uint32_t, uint32_t>;

struct __declspec(uuid("9F1A3A38-9D2A-4E5B-B5A3-3F2D0F38CD33")) CommandListData {
  // Draw-time pixel SRVs used by the TAA router.
  reshade::api::resource_view pixel_srv_t0 = {0};  // HDR color at insertion draws
  reshade::api::resource_view pixel_srv_t2 = {0};  // Depth at MotionBlurCameraVelocity
  reshade::api::resource_view pixel_srv_t3 = {0};  // Object velocity at MotionBlurCameraVelocity
};

inline CommandListData* Get(reshade::api::command_list* cmd_list) {
  if (cmd_list == nullptr) return nullptr;
  // ReShade creates this state in init_command_list. Do not allocate lazily
  // from descriptor or draw callbacks, which may run concurrently.
  return cmd_list->get_private_data<CommandListData>();
}

inline bool ResolveRegister(
    reshade::api::pipeline_layout layout,
    uint32_t layout_param,
    const reshade::api::descriptor_table_update& update,
    uint32_t descriptor_index,
    RegisterSlot* slot) {
  const auto* layout_data = renodx::utils::pipeline_layout::GetPipelineLayoutData(layout);
  if (layout_data == nullptr || layout_param >= layout_data->params.size()) return false;

  const auto& param = layout_data->params[layout_param];
  const uint32_t binding = update.binding + descriptor_index;

  switch (param.type) {
    case reshade::api::pipeline_layout_param_type::push_descriptors:
      *slot = {param.push_descriptors.dx_register_index + binding, param.push_descriptors.dx_register_space};
      return true;
    case reshade::api::pipeline_layout_param_type::descriptor_table:
    case reshade::api::pipeline_layout_param_type::push_descriptors_with_ranges:
    case reshade::api::pipeline_layout_param_type::push_descriptors_with_static_samplers:
    case reshade::api::pipeline_layout_param_type::descriptor_table_with_static_samplers: {
      if (layout_param >= layout_data->ranges.size()) return false;
      for (const auto& range : layout_data->ranges[layout_param]) {
        const bool in_range = binding >= range.binding
                              && (range.count == UINT32_MAX || binding < range.binding + range.count);
        if (!in_range) continue;
        *slot = {range.dx_register_index + (binding - range.binding), range.dx_register_space};
        return true;
      }
      return false;
    }
    default:
      return false;
  }
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
    reshade::api::pipeline_layout layout,
    uint32_t layout_param,
    const reshade::api::descriptor_table_update& update) {
  const bool tracks_pixel = renodx::utils::bitwise::HasFlag(stages, reshade::api::shader_stage::pixel);

  switch (update.type) {
    case reshade::api::descriptor_type::sampler_with_resource_view:
    case reshade::api::descriptor_type::shader_resource_view:
    case reshade::api::descriptor_type::buffer_shader_resource_view:
      if (!tracks_pixel) return;
      break;
    default:
      return;
  }

  auto* data = Get(cmd_list);
  if (data == nullptr) return;

  for (uint32_t i = 0u; i < update.count; ++i) {
    RegisterSlot slot = {};
    if (!ResolveRegister(layout, layout_param, update, i, &slot)) continue;
    if (slot.second != 0u || (slot.first != 0u && slot.first != 2u && slot.first != 3u)) continue;

    const auto view = renodx::utils::descriptor::GetResourceViewFromDescriptorUpdate(update, i);
    switch (slot.first) {
      case 0u: data->pixel_srv_t0 = view; break;
      case 2u: data->pixel_srv_t2 = view; break;
      case 3u: data->pixel_srv_t3 = view; break;
      default: break;
    }
  }
}

}  // namespace mgsv::inputs::descriptor_tracker

namespace mgsv::inputs {

// Retains the views and underlying resources, not the pixels stored in them.
// Neither a nonempty Snapshot nor Motion proves current contents or GPU execution order.
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
  reshade::api::swapchain* swapchain = nullptr;  // Borrowed selection, including during resize retirement.
  reshade::api::device* device = nullptr;
  uint64_t generation = 0;
  Motion motion;
  Motion lighting_motion;  // Only the existing AA capture path may change this compatibility selection.
  Materials materials;
};

namespace internal {

struct State {
  std::shared_mutex mutex;
  Snapshot snapshot;
  bool accepting = false;
  bool generation_exhausted = false;
  uint64_t next_observation = 0;
  // Keep selection watermarks after a failed creation clears its observation; old captures cannot refill it.
  uint64_t motion_ticket = 0;
  uint64_t lighting_ticket = 0;
  uint64_t materials_ticket = 0;
};

static State state;

// These borrowed native contexts are used only within the caller's callback lifetime.
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

// Selection identity remains visible while captures are stopped for retirement or resize.
inline bool IsSelectedDevice(reshade::api::device* device) {
  const std::shared_lock lock(internal::state.mutex);
  return device != nullptr && internal::state.snapshot.device == device;
}

inline bool IsSelectedSwapchain(reshade::api::swapchain* swapchain) {
  const std::shared_lock lock(internal::state.mutex);
  return swapchain != nullptr && internal::state.snapshot.swapchain == swapchain;
}

inline Snapshot Read(reshade::api::device* device) {
  if (!IsSelectedDevice(device)) return {};
  const std::shared_lock lock(internal::state.mutex);
  if (!internal::state.accepting || device == nullptr || internal::state.snapshot.device != device) return {};
  // Copying owned ComPtrs under this leaf lock is the only permitted native reference acquisition here.
  return internal::state.snapshot;
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

// Composition calls this before reconstruction's Present callback, and OnInitSwapchain at init.
// Like the existing reconstruction selector, this is a first-present, single-device contract,
// not proof that the selected swapchain belongs to an engine scene rather than auxiliary rendering.
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

// Composition must call this before native ResizeBuffers, not wait for retained resources to be destroyed.
// Consumers must also release their copied snapshots before resize; no waiting is performed here.
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

// Composition keeps the existing execution gate across precision preparation and both motion capture paths.
// Generation checks reject late publication; they do not synchronize native context use or device destruction.
inline Motion CaptureMotion(
    reshade::api::command_list* cmd_list,
    reshade::api::resource_view prepared_velocity_rtv,
    bool publish_lighting) {
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
  ID3D11ShaderResourceView* pixel_views[2] = {};
  context->PSGetShaderResources(2, 2, pixel_views);
  Motion motion;
  motion.depth.Attach(pixel_views[0]);
  motion.object_velocity.Attach(pixel_views[1]);
  // Missing bindings preserve the previous observation, as in the original capture path.
  if (producer == nullptr || motion.depth == nullptr || motion.object_velocity == nullptr) return {};
  const auto retain_enabled_clone = [device]<typename T>(const Microsoft::WRL::ComPtr<T>& original) {
    static_assert(std::is_same_v<T, ID3D11RenderTargetView> || std::is_same_v<T, ID3D11ShaderResourceView>);
    if (renodx::utils::resource::shared.data == nullptr) return original;
    const reshade::api::resource_view original_view = {reinterpret_cast<uintptr_t>(original.Get())};
    reshade::api::resource original_resource = {0};
    bool use_clone = false;
    renodx::utils::resource::GetLiveResourceViewInfo(
        original_view, [&](const renodx::utils::resource::ResourceViewInfo& info) {
          use_clone = info.device == device && !info.is_clone && info.clone_enabled;
          original_resource = info.original_resource;
        });
    if (!use_clone) return original;
    // Creation can reenter callbacks. No input or resource-map lock spans this call.
    const auto clone = renodx::utils::resource::upgrade::GetResourceViewClone(original_view);
    if (clone.handle == 0) return original;
    Microsoft::WRL::ComPtr<T> retained;
    renodx::utils::resource::GetLiveResourceViewInfo(
        clone, [&](const renodx::utils::resource::ResourceViewInfo& info) {
          if (info.device != device || !info.is_clone || info.view.handle != clone.handle
              || info.original_resource.handle != original_resource.handle) return;
          if constexpr (std::is_same_v<T, ID3D11RenderTargetView>) {
            if (info.usage != reshade::api::resource_usage::render_target) return;
          } else {
            if (info.usage != reshade::api::resource_usage::shader_resource
                && info.usage != reshade::api::resource_usage::shader_resource_pixel
                && info.usage != reshade::api::resource_usage::shader_resource_non_pixel) return;
          }
          // Queued destruction marks this clone entry before releasing its native reference.
          retained = reinterpret_cast<T*>(clone.handle);  // NOLINT(performance-no-int-to-ptr)
        });
    return retained;
  };
  producer = retain_enabled_clone(producer);
  motion.object_velocity = retain_enabled_clone(motion.object_velocity);
  if (producer == nullptr || motion.object_velocity == nullptr) return {};
  // Only retain the expected target through the live getter or its protected clone mapping.
  // A nonzero prepared identity is never converted directly into an owning COM pointer.
  if (prepared_velocity_rtv.handle != 0
      && prepared_velocity_rtv.handle != reinterpret_cast<uintptr_t>(producer.Get())) return {};

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
  // Reconstruction checks matching extents and single-mip/sample layouts at insertion.
  // Those temporal requirements must not restrict the existing GI velocity-cache selection.

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

  // The velocity SRV always covers mip 0 / slice 0, as in the original cache.
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
      Motion retired_lighting;
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
          if (publish_lighting && ticket > internal::state.lighting_ticket
              && snapshot.lighting_motion.observation == previous.lighting_motion.observation) {
            internal::state.lighting_ticket = ticket;
            retired_lighting = std::move(snapshot.lighting_motion);
            snapshot.lighting_motion = {};
            cleared = cleared || retired_lighting.velocity != nullptr;
          }
        }
      }
      // A creation failure drops the old latest cache, and AA also drops its compatibility selection.
      // Off-mode observation failures never alter lighting's compatibility selection.
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
  Motion retired_lighting;
  bool published_motion = false;
  bool published_lighting = false;
  {
    const std::unique_lock lock(internal::state.mutex);
    auto& snapshot = internal::state.snapshot;
    if (!internal::state.accepting || snapshot.device != device || snapshot.generation != previous.generation) return {};
    if (ticket > internal::state.motion_ticket) {
      internal::state.motion_ticket = ticket;
      retired_motion = std::move(snapshot.motion);
      snapshot.motion = motion;
      published_motion = true;
    }
    if (publish_lighting && ticket > internal::state.lighting_ticket) {
      internal::state.lighting_ticket = ticket;
      retired_lighting = std::move(snapshot.lighting_motion);
      snapshot.lighting_motion = motion;
      published_lighting = true;
    }
  }
  if (!published_motion && !published_lighting) return {};
  return motion;
}

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
  // Preserve the native pair without requiring matching material base extents or motion demand.
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