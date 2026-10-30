/*
 * Copyright (C) 2024 Musa Haji
 * Copyright (C) 2026 ZioPao
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <d3d11.h>

#include <cstdint>
#include <vector>

#include <include/reshade.hpp>

#include "../../mods/swapchain_v2.hpp"
#include "../../utils/resource.hpp"
#include "../../utils/resource_upgrade.hpp"
#include "../../utils/swapchain.hpp"

namespace mgsv::rendering::resource_upgrades {

namespace shader_hashes {

inline constexpr uint32_t CAMERA_VELOCITY_PS = 0xA13321B6u;
inline constexpr uint32_t GBUFFER_VELOCITY_VS = 0x1D2531B7u;
inline constexpr uint32_t GBUFFER_VELOCITY_PS = 0x9815404Fu;
inline constexpr uint32_t GBUFFER_MASKED_VELOCITY_VS = 0x7B809E72u;
inline constexpr uint32_t GBUFFER_MASKED_VELOCITY_PS = 0x58C10658u;

}  // namespace shader_hashes

enum class VelocityProducer : std::uint8_t {
  NONE,
  GBUFFER,
  GBUFFER_MASKED,
  CAMERA,
};

constexpr VelocityProducer IdentifyVelocityProducer(uint32_t vertex_hash, uint32_t pixel_hash) {
  switch (pixel_hash) {
    case shader_hashes::CAMERA_VELOCITY_PS:
      return VelocityProducer::CAMERA;
    case shader_hashes::GBUFFER_VELOCITY_PS:
      if (vertex_hash == shader_hashes::GBUFFER_VELOCITY_VS) return VelocityProducer::GBUFFER;
      break;
    case shader_hashes::GBUFFER_MASKED_VELOCITY_PS:
      if (vertex_hash == shader_hashes::GBUFFER_MASKED_VELOCITY_VS) return VelocityProducer::GBUFFER_MASKED;
      break;
  }
  return VelocityProducer::NONE;
}

// Descriptor addresses identify resource roles and must be shared across translation units.
inline renodx::utils::resource::ResourceUpgradeInfo lut_builder_upgrade_info = {
    .old_format = reshade::api::format::b8g8r8a8_typeless,
    .new_format = reshade::api::format::r16g16b16a16_float,
    .use_resource_view_cloning = true,
    .use_resource_view_hot_swap = true,
    .usage_set = static_cast<uint32_t>(
        reshade::api::resource_usage::shader_resource
        | reshade::api::resource_usage::render_target),
    .dimensions = {
        .width = 256,
        .height = 16,
    },
    .usage_include = reshade::api::resource_usage::render_target,
    .name = "lutbuilder_256x16_bgra8_typeless_hot_swap",
};

inline renodx::utils::resource::ResourceUpgradeInfo scene_tonemap_upgrade_info = {
    .old_format = reshade::api::format::b8g8r8a8_typeless,
    .new_format = reshade::api::format::r16g16b16a16_float,
    .shader_hash = 0xE04D1471,
    .use_resource_view_cloning = true,
    .use_resource_view_hot_swap = true,
    .usage_set = static_cast<uint32_t>(
        reshade::api::resource_usage::shader_resource
        | reshade::api::resource_usage::render_target),
    .dimensions = {
        .width = renodx::utils::resource::ResourceUpgradeInfo::BACK_BUFFER,
        .height = renodx::utils::resource::ResourceUpgradeInfo::BACK_BUFFER,
    },
    .usage_include = reshade::api::resource_usage::render_target,
    .name = "tonemap_bgra8_typeless_hot_swap",
};

inline renodx::utils::resource::ResourceUpgradeInfo motion_blur_upgrade_info = {
    .old_format = reshade::api::format::b8g8r8a8_typeless,
    .new_format = reshade::api::format::r16g16b16a16_float,
    .use_resource_view_cloning = true,
    .use_resource_view_hot_swap = true,
    .aspect_ratio = renodx::mods::swapchain::v2::SwapChainUpgradeTarget::BACK_BUFFER,
    .usage_set = static_cast<uint32_t>(
        reshade::api::resource_usage::shader_resource
        | reshade::api::resource_usage::render_target),
    .usage_include = reshade::api::resource_usage::render_target,
    .name = "motion_blur_bgra8_typeless_hot_swap",
};

inline renodx::utils::resource::ResourceUpgradeInfo velocity_upgrade_info = {
    .old_format = reshade::api::format::b8g8r8a8_typeless,
    .new_format = reshade::api::format::r16g16b16a16_float,
    .use_resource_view_cloning = true,
    .use_resource_view_hot_swap = true,
    .usage_set = static_cast<uint32_t>(
        reshade::api::resource_usage::shader_resource
        | reshade::api::resource_usage::render_target),
    .dimensions = {
        .width = renodx::utils::resource::ResourceUpgradeInfo::BACK_BUFFER,
        .height = renodx::utils::resource::ResourceUpgradeInfo::BACK_BUFFER,
    },
    .usage_include = reshade::api::resource_usage::render_target,
    .name = "taa_velocity_bgra8_typeless_hot_swap",
};

inline renodx::utils::resource::ResourceUpgradeInfo dof_final_copy_upgrade_info = {
    .old_format = reshade::api::format::b8g8r8a8_typeless,
    .new_format = reshade::api::format::r16g16b16a16_float,
    .use_resource_view_cloning = true,
    .use_resource_view_hot_swap = true,
    .usage_set = static_cast<uint32_t>(
        reshade::api::resource_usage::shader_resource
        | reshade::api::resource_usage::render_target),
    .dimensions = {
        .width = renodx::utils::resource::ResourceUpgradeInfo::BACK_BUFFER,
        .height = renodx::utils::resource::ResourceUpgradeInfo::BACK_BUFFER,
    },
    .usage_include = reshade::api::resource_usage::render_target,
    .name = "dof_final_copy_bgra8_typeless_hot_swap",
};

struct DofCopyState {
  bool upgrade_allowed = false;
  reshade::api::resource active_resource = {0u};
};

struct MotionBlurCopyState {
  std::vector<reshade::api::resource> active_resources;
  bool upgrade_allowed = false;
  bool copy_back_pending = false;
};

struct TonemapCopyState {
  bool propagation_allowed = false;
  reshade::api::resource source = {0u};
};

inline DofCopyState dof_copy_state;
inline MotionBlurCopyState motion_blur_copy_state;
inline TonemapCopyState tonemap_copy_state;

inline void ArmDofFinalCopyRenderBufferWindow(reshade::api::command_list* cmd_list) {
  (void)cmd_list;

  dof_copy_state.upgrade_allowed = true;
}

inline void DisarmDofFinalCopyRenderBufferAfterDofFinal(reshade::api::command_list* cmd_list) {
  (void)cmd_list;

  dof_copy_state.upgrade_allowed = false;
}

inline void ResetTonemapCopyTracking() {
  dof_copy_state.upgrade_allowed = false;
  motion_blur_copy_state.upgrade_allowed = false;
  motion_blur_copy_state.copy_back_pending = false;
  tonemap_copy_state.propagation_allowed = false;
  tonemap_copy_state.source = {0u};
}

inline void ArmTonemapCopyResourcePropagationForRTV(
    reshade::api::device* device,
    reshade::api::resource_view rtv) {
  if (device == nullptr || rtv.handle == 0u) return;

  const auto resource = renodx::utils::resource::GetResourceFromView(device, rtv);
  if (resource.handle == 0u) return;

  bool source_active = false;
  const auto found_resource = renodx::utils::resource::GetResourceInfo(
      resource,
      [&source_active](const renodx::utils::resource::ResourceInfo& info) {
        source_active = !info.destroyed
                        && info.clone_target == &scene_tonemap_upgrade_info
                        && info.clone_enabled;
      });
  if (!found_resource || !source_active) return;

  tonemap_copy_state.source = resource;
  tonemap_copy_state.propagation_allowed = true;
}

inline bool ActivateCloneHotSwapIfTracked(
    reshade::api::device* device,
    reshade::api::resource_view rtv) {
  if (device == nullptr || rtv.handle == 0u) return false;

  const auto resource = renodx::utils::resource::GetResourceFromView(device, rtv);
  if (resource.handle == 0u) return false;

  bool can_activate = false;
  const auto found_resource = renodx::utils::resource::GetResourceInfo(
      resource,
      [&can_activate](const renodx::utils::resource::ResourceInfo& info) {
        can_activate = !info.destroyed
                       && !info.clone_enabled
                       && info.clone_target != nullptr
                       && info.clone_target->use_resource_view_hot_swap;
      });
  if (!found_resource || !can_activate) return false;

  return renodx::utils::resource::upgrade::ActivateCloneHotSwap(device, rtv);
}

inline bool ResourceViewMatchesCloneTarget(
    reshade::api::resource_view view,
    renodx::utils::resource::ResourceUpgradeInfo* upgrade_info) {
  if (view.handle == 0u || upgrade_info == nullptr) return false;

  bool matches = false;
  renodx::utils::resource::GetResourceViewInfo(
      view,
      [&matches, upgrade_info](const renodx::utils::resource::ResourceViewInfo& info) {
        if (info.destroyed) return;

        matches = (info.clone_target == upgrade_info && info.clone_enabled)
                  || (info.resource_info != nullptr
                      && info.resource_info->clone_target == upgrade_info
                      && info.resource_info->clone_enabled);

        // D3D11 PSGetShaderResources can return the already-rewritten clone SRV instead of the
        // original SRV. Clone views do not carry clone_enabled, so confirm against the original
        // resource that owns the clone before rejecting the source gate.
        if (!matches
            && info.is_clone
            && info.clone_target == upgrade_info
            && info.original_resource.handle != 0u) {
          renodx::utils::resource::GetResourceInfo(
              info.original_resource,
              [&matches, &info, upgrade_info](const renodx::utils::resource::ResourceInfo& resource_info) {
                matches = !resource_info.destroyed
                          && resource_info.clone_target == upgrade_info
                          && resource_info.clone_enabled
                          && (info.clone_resource.handle == 0u
                              || resource_info.clone.handle == info.clone_resource.handle);
              });
        }
      });
  return matches;
}

inline bool PixelShaderResourceMatchesCloneTarget(
    reshade::api::command_list* cmd_list,
    renodx::utils::resource::ResourceUpgradeInfo* upgrade_info,
    uint32_t slot = 0u) {
  if (cmd_list == nullptr || upgrade_info == nullptr) return false;

  auto* device = cmd_list->get_device();
  if (device == nullptr || device->get_api() != reshade::api::device_api::d3d11) return false;

  auto* native_context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (native_context == nullptr) return false;

  ID3D11ShaderResourceView* shader_resource_view = nullptr;
  native_context->PSGetShaderResources(slot, 1, &shader_resource_view);
  if (shader_resource_view == nullptr) return false;

  const uint64_t srv_handle = reinterpret_cast<uint64_t>(shader_resource_view);
  const auto matches = ResourceViewMatchesCloneTarget(
      {srv_handle},
      upgrade_info);
  shader_resource_view->Release();
  return matches;
}

inline reshade::api::resource GetOriginalResourceFromView(reshade::api::resource_view view) {
  if (view.handle == 0u) return {0u};

  reshade::api::resource resource = {0u};
  bool destroyed = true;
  const auto found_view = renodx::utils::resource::GetResourceViewInfo(
      view,
      [&resource, &destroyed](const renodx::utils::resource::ResourceViewInfo& info) {
        destroyed = info.destroyed;
        if (info.destroyed) return;
        resource = info.original_resource;
      });
  if (!found_view || destroyed) return {0u};

  return resource;
}

inline void TrackActiveMotionBlurResource(reshade::api::resource_view view) {
  if (!ResourceViewMatchesCloneTarget(view, &motion_blur_upgrade_info)) return;

  const auto resource = GetOriginalResourceFromView(view);
  if (resource.handle == 0u) return;

  for (const auto active_resource : motion_blur_copy_state.active_resources) {
    if (active_resource.handle == resource.handle) return;
  }
  motion_blur_copy_state.active_resources.push_back(resource);
}

inline void DeactivateCloneHotSwapTarget(
    reshade::api::resource resource,
    renodx::utils::resource::ResourceUpgradeInfo* upgrade_info) {
  if (resource.handle == 0u || upgrade_info == nullptr) return;

  std::vector<uint64_t> view_handles;
  bool should_deactivate = false;
  const auto found_resource = renodx::utils::resource::UpdateResourceInfo(
      resource,
      [&view_handles, &should_deactivate, upgrade_info](renodx::utils::resource::ResourceInfo* info) {
        if (info->destroyed || info->clone_target != upgrade_info) return;

        // Keep the clone target/allocation for reuse, but disable hot-swap after this role ends.
        info->clone_enabled = false;
        info->clone_can_deactivate = false;
        view_handles.assign(info->resource_view_handles.begin(), info->resource_view_handles.end());
        should_deactivate = true;
      });
  if (!found_resource || !should_deactivate) return;

  renodx::utils::resource::upgrade::UpdateResourceViewsCloneState(
      view_handles,
      false,
      false);
}

inline void DeactivateDofFinalCopyRenderBufferTarget() {
  if (dof_copy_state.active_resource.handle == 0u) return;

  // MGSV reuses this full-res BGRA resource for rain/GBuffer material work near frame start;
  // leaving the clone enabled makes those passes sample the stale HDR DoF snapshot.
  DeactivateCloneHotSwapTarget(
      dof_copy_state.active_resource,
      &dof_final_copy_upgrade_info);
  dof_copy_state.active_resource = reshade::api::resource{0u};
}

inline void DeactivateMotionBlurTargets() {
  // Motion-blur intermediates can be reused for unrelated half-resolution work in the next frame.
  for (const auto resource : motion_blur_copy_state.active_resources) {
    DeactivateCloneHotSwapTarget(resource, &motion_blur_upgrade_info);
  }
  motion_blur_copy_state.active_resources.clear();
}

inline void ArmMotionBlurCopyRenderBufferWindow(reshade::api::command_list* cmd_list) {
  (void)cmd_list;

  // Tile preparation does not consume scene color. Clear any missed previous-frame state here,
  // immediately before the CopyRenderBuffer draw that populates the motion-blur scene input.
  DeactivateMotionBlurTargets();
  motion_blur_copy_state.upgrade_allowed = true;
}

inline bool MarkCloneTarget(
    reshade::api::resource_view rtv,
    renodx::utils::resource::ResourceUpgradeInfo* upgrade_info,
    reshade::api::device* device = nullptr) {
  if (rtv.handle == 0u || upgrade_info == nullptr) return false;

  const auto back_buffer_desc = device == nullptr
                                    ? reshade::api::resource_desc{}
                                    : renodx::utils::swapchain::GetBackBufferDesc(device);

  reshade::api::resource resource = {0u};
  bool view_destroyed = false;
  bool view_is_clone = false;
  const auto found_view = renodx::utils::resource::GetResourceViewInfo(
      rtv,
      [&resource, &view_destroyed, &view_is_clone](const renodx::utils::resource::ResourceViewInfo& info) {
        view_destroyed = info.destroyed;
        view_is_clone = info.is_clone;
        if (info.destroyed || info.is_clone) return;
        resource = info.original_resource;
      });
  if (!found_view || view_destroyed || view_is_clone || resource.handle == 0u) return false;

  bool marked_resource = false;
  bool conflicting_clone_target = false;
  const auto found_resource = renodx::utils::resource::UpdateResourceInfo(
      resource,
      [&marked_resource,
       &conflicting_clone_target,
       upgrade_info,
       back_buffer_desc](renodx::utils::resource::ResourceInfo* info) {
        if (info->destroyed || info->is_swap_chain) return;

        const bool matches_upgrade = upgrade_info->CheckResourceDesc(
            info->desc,
            back_buffer_desc,
            info->initial_state);
        if (!matches_upgrade) return;

        if (info->clone_target != nullptr
            && info->clone_target != upgrade_info) {
          conflicting_clone_target = true;
          return;
        }

        info->clone_target = upgrade_info;
        marked_resource = true;
      });
  if (!found_resource || conflicting_clone_target || !marked_resource) return false;

  bool marked_view = false;
  const auto found_view_update = renodx::utils::resource::UpdateResourceViewInfo(
      rtv,
      [&marked_view, resource, upgrade_info](renodx::utils::resource::ResourceViewInfo* info) {
        if (info->destroyed || info->is_clone) return;
        if (info->original_resource.handle != resource.handle) return;

        info->clone_target = upgrade_info;
        marked_view = true;
      });
  return found_view_update && marked_view;
}

inline bool MarkSceneTonemapCloneTarget(reshade::api::resource_view rtv, reshade::api::device* device = nullptr) {
  return MarkCloneTarget(rtv, &scene_tonemap_upgrade_info, device);
}

inline void RebindRenderTargetClones(
    reshade::api::command_list* cmd_list,
    const std::vector<reshade::api::resource_view>& rtvs,
    reshade::api::resource_view dsv) {
  renodx::mods::swapchain::v2::FlushDescriptors(cmd_list);
  renodx::mods::swapchain::v2::RewriteRenderTargets(cmd_list, rtvs.size(), rtvs.data(), dsv);
}

inline void UpgradeDeferredSceneColorTarget(reshade::api::command_list* cmd_list) {
  if (cmd_list == nullptr) return;

  auto rtvs = renodx::utils::swapchain::GetRenderTargets(cmd_list);
  auto* device = cmd_list->get_device();
  bool changed = false;

  // Rain/fog deferred paths can bind extra GBuffer/material RTVs alongside the scene-color output.
  // Only RTV0 is the color target that feeds the later LUT pass; upgrading the additional RTVs turns
  // rain albedo/material maps into RGBA16 clone resources and corrupts TppRainFilterGBufferMaterial_NoIR.
  if (!rtvs.empty() && rtvs[0].handle != 0u) {
    MarkSceneTonemapCloneTarget(rtvs[0], device);
    changed = ActivateCloneHotSwapIfTracked(device, rtvs[0]) || changed;
    ArmTonemapCopyResourcePropagationForRTV(device, rtvs[0]);
  }

  if (changed) {
    RebindRenderTargetClones(cmd_list, rtvs, {0});
  }
}

inline void UpgradeCopyRenderBufferTarget(reshade::api::command_list* cmd_list) {
  if (cmd_list == nullptr) return;
  if (!dof_copy_state.upgrade_allowed && !motion_blur_copy_state.upgrade_allowed) return;

  const bool is_motion_blur_copy = motion_blur_copy_state.upgrade_allowed;

  auto rtvs = renodx::utils::swapchain::GetRenderTargets(cmd_list);
  if (rtvs.empty() || rtvs[0].handle == 0u) return;

  // DoF still needs source proof because its markers open a wider window. The two motion-blur
  // tile-prep shaders identify their immediately following scene-input copy unambiguously, and
  // that full-resolution source can rotate between scene/DoF clone roles.
  if (!is_motion_blur_copy
      && !PixelShaderResourceMatchesCloneTarget(cmd_list, &scene_tonemap_upgrade_info, 0u)) {
    return;
  }

  auto* device = cmd_list->get_device();
  if (device == nullptr) return;

  // CopyRenderBuffer is generic, so DoF and motion-blur markers arm only their next
  // scene-color copy. Do not rely on previous-frame handles; MGSV rotates these resources.
  const auto rtv_resource = GetOriginalResourceFromView(rtvs[0]);
  if (rtv_resource.handle == 0u) return;

  auto* upgrade_info = is_motion_blur_copy
                           ? &motion_blur_upgrade_info
                           : &dof_final_copy_upgrade_info;
  if (!MarkCloneTarget(rtvs[0], upgrade_info, device)) return;

  const bool activated = ActivateCloneHotSwapIfTracked(device, rtvs[0]);
  if (!activated && !ResourceViewMatchesCloneTarget(rtvs[0], upgrade_info)) return;

  if (upgrade_info == &motion_blur_upgrade_info) {
    TrackActiveMotionBlurResource(rtvs[0]);
    motion_blur_copy_state.upgrade_allowed = false;
  } else {
    dof_copy_state.active_resource = rtv_resource;
    dof_copy_state.upgrade_allowed = false;
  }

  if (activated) {
    RebindRenderTargetClones(cmd_list, rtvs, {0});
  }
}

inline reshade::api::resource_view PrepareVelocityTarget(
    reshade::api::command_list* cmd_list,
    reshade::api::resource_view velocity_rtv,
    uint32_t vertex_hash,
    uint32_t pixel_hash) {
  // Precision is independent of effects/output, but only these native producers may claim this role.
  if (IdentifyVelocityProducer(vertex_hash, pixel_hash) == VelocityProducer::NONE) return velocity_rtv;
  if (cmd_list == nullptr || velocity_rtv.handle == 0u) return velocity_rtv;

  auto* device = cmd_list->get_device();
  if (device == nullptr || !MarkCloneTarget(velocity_rtv, &velocity_upgrade_info, device)) {
    return velocity_rtv;
  }

  const bool activated = ActivateCloneHotSwapIfTracked(device, velocity_rtv);
  if (!activated && !ResourceViewMatchesCloneTarget(velocity_rtv, &velocity_upgrade_info)) {
    return velocity_rtv;
  }

  if (activated) {
    auto rtvs = renodx::utils::swapchain::GetRenderTargets(cmd_list);
    if (!rtvs.empty()) {
      RebindRenderTargetClones(cmd_list, rtvs, {0});
    }
  }

  const auto clone_rtv = renodx::utils::resource::upgrade::GetResourceViewClone(velocity_rtv);
  return clone_rtv.handle != 0u ? clone_rtv : velocity_rtv;
}

inline bool OnCopyTonemapOutputResource(
    reshade::api::command_list* cmd_list,
    reshade::api::resource source,
    reshade::api::resource dest) {
  if (cmd_list == nullptr) return false;
  if (source.handle == dest.handle) return false;

  bool source_active = false;
  const auto found_source = renodx::utils::resource::GetResourceInfo(
      source,
      [&source_active](const renodx::utils::resource::ResourceInfo& info) {
        source_active = !info.destroyed
                        && info.clone_target == &scene_tonemap_upgrade_info
                        && info.clone_enabled;
      });
  if (!found_source || !source_active) return false;
  if (!tonemap_copy_state.propagation_allowed) return false;

  tonemap_copy_state.propagation_allowed = false;
  const auto expected_source = tonemap_copy_state.source;
  tonemap_copy_state.source = {0u};
  if (source.handle != expected_source.handle) return false;

  std::vector<uint64_t> dest_view_handles;
  bool marked_dest = false;
  bool dest_conflicting_clone_target = false;
  const auto back_buffer_desc = renodx::utils::swapchain::GetBackBufferDesc(cmd_list);
  const auto found_dest = renodx::utils::resource::UpdateResourceInfo(
      dest,
      [&dest_view_handles,
       &marked_dest,
       &dest_conflicting_clone_target,
       back_buffer_desc](renodx::utils::resource::ResourceInfo* info) {
        if (info->destroyed || info->is_swap_chain) return;

        const bool dest_matches_upgrade = scene_tonemap_upgrade_info.CheckResourceDesc(
            info->desc,
            back_buffer_desc,
            info->initial_state);
        if (!dest_matches_upgrade) return;
        if (info->clone_target != nullptr
            && info->clone_target != &scene_tonemap_upgrade_info) {
          dest_conflicting_clone_target = true;
          return;
        }

        info->clone_target = &scene_tonemap_upgrade_info;
        info->clone_enabled = true;
        dest_view_handles.assign(info->resource_view_handles.begin(), info->resource_view_handles.end());
        marked_dest = true;
      });
  if (!found_dest || dest_conflicting_clone_target || !marked_dest) return false;

  renodx::utils::resource::upgrade::UpdateResourceViewsCloneState(
      dest_view_handles,
      true,
      false,
      &scene_tonemap_upgrade_info);

  auto source_clone = renodx::utils::resource::upgrade::GetResourceClone(source);
  auto dest_clone = renodx::utils::resource::upgrade::GetResourceClone(dest);
  if (source_clone.handle == 0u || dest_clone.handle == 0u) return false;

  cmd_list->copy_resource(source_clone, dest_clone);
  return true;
}

// Shader registration decides whether to replace bytecode; this callback only activates clones.
inline bool OnActivateRenderTargetClones(reshade::api::command_list* cmd_list) {
  auto rtvs = renodx::utils::swapchain::GetRenderTargets(cmd_list);
  auto* device = cmd_list->get_device();
  bool changed = false;
  for (auto rtv : rtvs) {
    changed = ActivateCloneHotSwapIfTracked(device, rtv) || changed;
    ArmTonemapCopyResourcePropagationForRTV(device, rtv);
  }
  if (changed) {
    RebindRenderTargetClones(cmd_list, rtvs, {0});
  }
  return true;
}

inline bool OnCopyRenderBufferDraw(reshade::api::command_list* cmd_list) {
  UpgradeCopyRenderBufferTarget(cmd_list);
  motion_blur_copy_state.copy_back_pending = PixelShaderResourceMatchesCloneTarget(
      cmd_list, &motion_blur_upgrade_info, 0u);
  return true;
}

inline void OnCopyRenderBufferDrawn(reshade::api::command_list* cmd_list) {
  (void)cmd_list;
  if (!motion_blur_copy_state.copy_back_pending) return;
  motion_blur_copy_state.copy_back_pending = false;
  DeactivateMotionBlurTargets();
}

inline bool OnUpgradeSceneColorDraw(reshade::api::command_list* cmd_list) {
  auto rtvs = renodx::utils::swapchain::GetRenderTargets(cmd_list);
  auto* device = cmd_list->get_device();
  bool changed = false;
  for (auto rtv : rtvs) {
    MarkSceneTonemapCloneTarget(rtv, device);
    changed = ActivateCloneHotSwapIfTracked(device, rtv) || changed;
    ArmTonemapCopyResourcePropagationForRTV(device, rtv);
  }
  if (changed) {
    RebindRenderTargetClones(cmd_list, rtvs, {0});
  }
  return true;
}

inline bool OnUpgradeMotionBlurDraw(reshade::api::command_list* cmd_list) {
  auto rtvs = renodx::utils::swapchain::GetRenderTargets(cmd_list);
  auto* device = cmd_list->get_device();
  bool changed = false;
  for (auto rtv : rtvs) {
    MarkCloneTarget(rtv, &motion_blur_upgrade_info, device);
    changed = ActivateCloneHotSwapIfTracked(device, rtv) || changed;
    TrackActiveMotionBlurResource(rtv);
  }
  if (changed) {
    RebindRenderTargetClones(cmd_list, rtvs, {0});
  }
  return true;
}

}  // namespace mgsv::rendering::resource_upgrades