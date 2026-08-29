/*
 * Copyright (C) 2024 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#define ImTextureID                   ImU64
#define RENODX_MODS_SWAPCHAIN_VERSION 2
// #define DEBUG_LEVEL_0
// #define DEBUG_LEVEL_1
// #define DEBUG_LEVEL_2
// #define DEBUG_LEVEL_3

#include <embed/shaders.h>

#include <d3d11.h>
#include <cstdint>
#include <string>
#include <vector>

#include <deps/imgui/imgui.h>
#include <include/reshade.hpp>

#include "../../mods/shader.hpp"
#include "../../mods/swapchain_v2.hpp"
#include "../../utils/date.hpp"
#include "../../utils/settings.hpp"
#include "./shared.h"
#include "./taa/taa.hpp"

namespace renodx::mods::swapchain {
using v2::expected_constant_buffer_index;
using v2::FlushDescriptors;
using v2::force_borderless;
using v2::IsUpgraded;
using v2::prevent_full_screen;
using v2::resource_upgrade_infos;
using v2::RewriteRenderTargets;
using v2::SwapChainUpgradeTarget;
using v2::Use;
using v2::use_resource_cloning;
}  // namespace renodx::mods::swapchain

namespace {

renodx::utils::resource::ResourceUpgradeInfo scene_tonemap_upgrade_info = {
    .old_format = reshade::api::format::b8g8r8a8_typeless,
    .new_format = reshade::api::format::r16g16b16a16_float,
    .shader_hash = 0xE04D1471,
    .use_resource_view_cloning = true,
    .use_resource_view_hot_swap = true,
    .usage_set = static_cast<uint32_t>(
        reshade::api::resource_usage::shader_resource
        | reshade::api::resource_usage::render_target),
    .dimensions = {.width = renodx::utils::resource::ResourceUpgradeInfo::BACK_BUFFER,
                   .height = renodx::utils::resource::ResourceUpgradeInfo::BACK_BUFFER},
    .usage_include = reshade::api::resource_usage::render_target,
    .name = "tonemap_bgra8_typeless_hot_swap",
};

renodx::utils::resource::ResourceUpgradeInfo motion_blur_upgrade_info = {
    .old_format = reshade::api::format::b8g8r8a8_typeless,
    .new_format = reshade::api::format::r16g16b16a16_float,
    .use_resource_view_cloning = true,
    .use_resource_view_hot_swap = true,
    .aspect_ratio = renodx::mods::swapchain::SwapChainUpgradeTarget::BACK_BUFFER,
    .usage_set = static_cast<uint32_t>(
        reshade::api::resource_usage::shader_resource
        | reshade::api::resource_usage::render_target),
    .usage_include = reshade::api::resource_usage::render_target,
    .name = "motion_blur_bgra8_typeless_hot_swap",
};

renodx::utils::resource::ResourceUpgradeInfo taa_velocity_upgrade_info = {
    .old_format = reshade::api::format::b8g8r8a8_typeless,
    .new_format = reshade::api::format::r16g16b16a16_float,
    .use_resource_view_cloning = true,
    .use_resource_view_hot_swap = true,
    .usage_set = static_cast<uint32_t>(
        reshade::api::resource_usage::shader_resource
        | reshade::api::resource_usage::render_target),
    .dimensions = {.width = renodx::utils::resource::ResourceUpgradeInfo::BACK_BUFFER,
                   .height = renodx::utils::resource::ResourceUpgradeInfo::BACK_BUFFER},
    .usage_include = reshade::api::resource_usage::render_target,
    .name = "taa_velocity_bgra8_typeless_hot_swap",
};

renodx::utils::resource::ResourceUpgradeInfo dof_final_copy_upgrade_info = {
    .old_format = reshade::api::format::b8g8r8a8_typeless,
    .new_format = reshade::api::format::r16g16b16a16_float,
    .use_resource_view_cloning = true,
    .use_resource_view_hot_swap = true,
    .usage_set = static_cast<uint32_t>(
        reshade::api::resource_usage::shader_resource
        | reshade::api::resource_usage::render_target),
    .dimensions = {.width = renodx::utils::resource::ResourceUpgradeInfo::BACK_BUFFER,
                   .height = renodx::utils::resource::ResourceUpgradeInfo::BACK_BUFFER},
    .usage_include = reshade::api::resource_usage::render_target,
    .name = "dof_final_copy_bgra8_typeless_hot_swap",
};

bool dof_final_copy_upgrade_allowed = false;
reshade::api::resource dof_final_copy_active_resource = {0u};
std::vector<reshade::api::resource> motion_blur_active_resources;
bool motion_blur_copy_upgrade_allowed = false;
bool motion_blur_copy_back_pending = false;
bool tonemap_copy_resource_propagation_allowed = false;
reshade::api::resource tonemap_copy_resource_source = {0u};

void DeactivateMotionBlurTargets();

void ArmDofFinalCopyRenderBufferWindow(reshade::api::command_list* cmd_list) {
  (void)cmd_list;

  dof_final_copy_upgrade_allowed = true;
}

void DisarmDofFinalCopyRenderBufferAfterDofFinal(reshade::api::command_list* cmd_list) {
  (void)cmd_list;

  dof_final_copy_upgrade_allowed = false;
}

void ArmMotionBlurCopyRenderBufferWindow(reshade::api::command_list* cmd_list) {
  (void)cmd_list;

  // Tile preparation does not consume scene color. Clear any missed previous-frame state here,
  // immediately before the CopyRenderBuffer draw that populates the motion-blur scene input.
  DeactivateMotionBlurTargets();
  motion_blur_copy_upgrade_allowed = true;
}

void ResetTonemapCopyTracking() {
  dof_final_copy_upgrade_allowed = false;
  motion_blur_copy_upgrade_allowed = false;
  motion_blur_copy_back_pending = false;
  tonemap_copy_resource_propagation_allowed = false;
  tonemap_copy_resource_source = {0u};
}

void ArmTonemapCopyResourcePropagationForRTV(
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

  tonemap_copy_resource_source = resource;
  tonemap_copy_resource_propagation_allowed = true;
}

bool ActivateCloneHotSwapIfTracked(
    reshade::api::device* device,
    reshade::api::resource_view rtv);

bool MarkSceneTonemapCloneTarget(
    reshade::api::resource_view rtv,
    reshade::api::device* device);

bool MarkCloneTarget(
    reshade::api::resource_view rtv,
    renodx::utils::resource::ResourceUpgradeInfo* upgrade_info,
    reshade::api::device* device);

void UpgradeDeferredTonemapOutputRTVs(
    reshade::api::command_list* cmd_list,
    uint32_t shader_hash,
    const char* shader_name) {
  (void)shader_hash;
  (void)shader_name;

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
    renodx::mods::swapchain::FlushDescriptors(cmd_list);
    renodx::mods::swapchain::RewriteRenderTargets(cmd_list, rtvs.size(), rtvs.data(), {0});
  }
}

bool ActivateCloneHotSwapIfTracked(
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

bool ResourceViewMatchesCloneTarget(
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

bool PixelShaderResourceMatchesCloneTarget(
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

reshade::api::resource GetOriginalResourceFromView(reshade::api::resource_view view) {
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

void TrackActiveMotionBlurResource(reshade::api::resource_view view) {
  if (!ResourceViewMatchesCloneTarget(view, &motion_blur_upgrade_info)) return;

  const auto resource = GetOriginalResourceFromView(view);
  if (resource.handle == 0u) return;

  for (const auto active_resource : motion_blur_active_resources) {
    if (active_resource.handle == resource.handle) return;
  }
  motion_blur_active_resources.push_back(resource);
}

void UpgradeCopyRenderBufferTarget(reshade::api::command_list* cmd_list) {
  if (cmd_list == nullptr) return;
  if (!dof_final_copy_upgrade_allowed && !motion_blur_copy_upgrade_allowed) return;

  const bool is_motion_blur_copy = motion_blur_copy_upgrade_allowed;

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
    motion_blur_copy_upgrade_allowed = false;
  } else {
    dof_final_copy_active_resource = rtv_resource;
    dof_final_copy_upgrade_allowed = false;
  }

  if (activated) {
    renodx::mods::swapchain::FlushDescriptors(cmd_list);
    renodx::mods::swapchain::RewriteRenderTargets(cmd_list, rtvs.size(), rtvs.data(), {0});
  }
}

void DeactivateCloneHotSwapTarget(
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

void DeactivateDofFinalCopyRenderBufferTarget() {
  if (dof_final_copy_active_resource.handle == 0u) return;

  // MGSV reuses this full-res BGRA resource for rain/GBuffer material work near frame start;
  // leaving the clone enabled makes those passes sample the stale HDR DoF snapshot.
  DeactivateCloneHotSwapTarget(
      dof_final_copy_active_resource,
      &dof_final_copy_upgrade_info);
  dof_final_copy_active_resource = {0u};
}

void DeactivateMotionBlurTargets() {
  // Motion-blur intermediates can be reused for unrelated half-resolution work in the next frame.
  for (const auto resource : motion_blur_active_resources) {
    DeactivateCloneHotSwapTarget(resource, &motion_blur_upgrade_info);
  }
  motion_blur_active_resources.clear();
}

bool MarkCloneTarget(
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

bool MarkSceneTonemapCloneTarget(reshade::api::resource_view rtv, reshade::api::device* device = nullptr) {
  return MarkCloneTarget(rtv, &scene_tonemap_upgrade_info, device);
}

bool MarkMotionBlurCloneTarget(reshade::api::resource_view rtv, reshade::api::device* device = nullptr) {
  return MarkCloneTarget(rtv, &motion_blur_upgrade_info, device);
}

reshade::api::resource_view PrepareTaaVelocityTarget(
    reshade::api::command_list* cmd_list,
    reshade::api::resource_view velocity_rtv) {
  if (cmd_list == nullptr || velocity_rtv.handle == 0u) return velocity_rtv;

  auto* device = cmd_list->get_device();
  if (device == nullptr || !MarkCloneTarget(velocity_rtv, &taa_velocity_upgrade_info, device)) {
    return velocity_rtv;
  }

  const bool activated = ActivateCloneHotSwapIfTracked(device, velocity_rtv);
  if (!activated && !ResourceViewMatchesCloneTarget(velocity_rtv, &taa_velocity_upgrade_info)) {
    return velocity_rtv;
  }

  if (activated) {
    auto rtvs = renodx::utils::swapchain::GetRenderTargets(cmd_list);
    if (!rtvs.empty()) {
      renodx::mods::swapchain::FlushDescriptors(cmd_list);
      renodx::mods::swapchain::RewriteRenderTargets(cmd_list, rtvs.size(), rtvs.data(), {0});
    }
  }

  const auto clone_rtv = renodx::utils::resource::upgrade::GetResourceViewClone(velocity_rtv);
  return clone_rtv.handle != 0u ? clone_rtv : velocity_rtv;
}

bool OnCopyTonemapOutputResource(
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
  if (!tonemap_copy_resource_propagation_allowed) return false;

  tonemap_copy_resource_propagation_allowed = false;
  const auto expected_source = tonemap_copy_resource_source;
  tonemap_copy_resource_source = {0u};
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

#define UpgradeRTVReplaceShader(value)                                                                \
  {                                                                                                   \
      value,                                                                                          \
      {                                                                                               \
          .crc32 = value,                                                                             \
          .code = __##value,                                                                          \
          .on_draw = [](auto* cmd_list) {                                                             \
            auto rtvs = renodx::utils::swapchain::GetRenderTargets(cmd_list);                         \
            auto* device = cmd_list->get_device();                                                    \
            bool changed = false;                                                                     \
            for (auto rtv : rtvs) {                                                                   \
              changed = ActivateCloneHotSwapIfTracked(device, rtv) || changed;                        \
              ArmTonemapCopyResourcePropagationForRTV(device, rtv);                                   \
            }                                                                                         \
            if (changed) {                                                                            \
              renodx::mods::swapchain::FlushDescriptors(cmd_list);                                    \
              renodx::mods::swapchain::RewriteRenderTargets(cmd_list, rtvs.size(), rtvs.data(), {0}); \
            }                                                                                         \
            return true;                                                                              \
          },                                                                                          \
      },                                                                                              \
  }

#define UpgradeRTVReplaceShaderCallback(value, callback)                                              \
  {                                                                                                   \
      value,                                                                                          \
      {                                                                                               \
          .crc32 = value,                                                                             \
          .code = __##value,                                                                          \
          .on_draw = [](auto* cmd_list) {                                                             \
            auto rtvs = renodx::utils::swapchain::GetRenderTargets(cmd_list);                         \
            auto* device = cmd_list->get_device();                                                    \
            bool changed = false;                                                                     \
            for (auto rtv : rtvs) {                                                                   \
              changed = ActivateCloneHotSwapIfTracked(device, rtv) || changed;                        \
              ArmTonemapCopyResourcePropagationForRTV(device, rtv);                                   \
            }                                                                                         \
            if (changed) {                                                                            \
              renodx::mods::swapchain::FlushDescriptors(cmd_list);                                    \
              renodx::mods::swapchain::RewriteRenderTargets(cmd_list, rtvs.size(), rtvs.data(), {0}); \
            }                                                                                         \
            callback(cmd_list);                                                                       \
            return true;                                                                              \
          },                                                                                          \
      },                                                                                              \
  }

#define UpgradeRTVShader(value)                     \
  {                                                 \
      value,                                        \
      {                                             \
          .crc32 = value,                           \
          .on_inject = [](auto*) { return false; }, \
          .on_draw = [](auto* cmd_list) {                                                             \
            auto rtvs = renodx::utils::swapchain::GetRenderTargets(cmd_list);                         \
            auto* device = cmd_list->get_device();                                                    \
            bool changed = false;                                                                     \
            for (auto rtv : rtvs) {                                                                   \
              changed = ActivateCloneHotSwapIfTracked(device, rtv) || changed;                        \
              ArmTonemapCopyResourcePropagationForRTV(device, rtv);                                   \
            }                                                                                         \
            if (changed) {                                                                            \
              renodx::mods::swapchain::FlushDescriptors(cmd_list);                                    \
              renodx::mods::swapchain::RewriteRenderTargets(cmd_list, rtvs.size(), rtvs.data(), {0}); \
            }                                                                                         \
            return true; },        \
      },                                            \
  }

#define UpgradeTonemapOutputRTV(value)                                                                \
  {                                                                                                   \
      value,                                                                                          \
      {                                                                                               \
          .crc32 = value,                                                                             \
          .code = __##value,                                                                          \
          .on_draw = [](auto* cmd_list) {                                                             \
            auto rtvs = renodx::utils::swapchain::GetRenderTargets(cmd_list);                         \
            auto* device = cmd_list->get_device();                                                    \
            bool changed = false;                                                                     \
            for (auto rtv : rtvs) {                                                                   \
              MarkSceneTonemapCloneTarget(rtv, device);                                               \
              changed = ActivateCloneHotSwapIfTracked(device, rtv) || changed;                        \
              ArmTonemapCopyResourcePropagationForRTV(device, rtv);                                   \
            }                                                                                         \
            if (changed) {                                                                            \
              renodx::mods::swapchain::FlushDescriptors(cmd_list);                                    \
              renodx::mods::swapchain::RewriteRenderTargets(cmd_list, rtvs.size(), rtvs.data(), {0}); \
            }                                                                                         \
            return true;                                                                              \
          },                                                                                          \
      },                                                                                              \
  }

#define UpgradeDeferredTonemapOutputRTV(value, name)                 \
  {                                                                  \
      value,                                                         \
      {                                                              \
          .crc32 = value,                                            \
          .code = __##value,                                         \
          .on_draw = [](auto* cmd_list) {                            \
            UpgradeDeferredTonemapOutputRTVs(cmd_list, value, name); \
            return true;                                             \
          },                                                         \
      },                                                             \
  }

#define TrackDeferredTonemapOutputRTV(value, name)  \
  {                                                 \
      value,                                        \
      {                                             \
          .crc32 = value,                           \
          .on_inject = [](auto*) { return false; }, \
          .on_draw = [](auto* cmd_list) {                                                              \
            UpgradeDeferredTonemapOutputRTVs(cmd_list, value, name);                                   \
            return true; },        \
      },                                            \
  }

#define TrackMotionBlurTilePrep(value)              \
  {                                                 \
      value,                                        \
      {                                             \
          .crc32 = value,                           \
          .on_inject = [](auto*) { return false; }, \
          .on_draw = [](auto* cmd_list) {                  \
            ArmMotionBlurCopyRenderBufferWindow(cmd_list); \
            return true; },        \
      },                                            \
  }

#define UpgradeCopyRenderBufferRTV(value)           \
  {                                                 \
      value,                                        \
      {                                             \
          .crc32 = value,                           \
          .code = __##value,                        \
          .on_inject = [](auto*) { return false; }, \
          .on_draw = [](auto* cmd_list) {                    \
            UpgradeCopyRenderBufferTarget(cmd_list);         \
            motion_blur_copy_back_pending =                  \
                PixelShaderResourceMatchesCloneTarget(       \
                    cmd_list, &motion_blur_upgrade_info, 0u); \
            return true; },        \
          .on_drawn = [](auto*) {                            \
            if (!motion_blur_copy_back_pending) return;      \
            motion_blur_copy_back_pending = false;           \
            DeactivateMotionBlurTargets(); },                \
      },                                            \
  }

#define UpgradeMotionBlurRTV(value)                                                                   \
  {                                                                                                   \
      value,                                                                                          \
      {                                                                                               \
          .crc32 = value,                                                                             \
          .code = __##value,                                                                          \
          .on_draw = [](auto* cmd_list) {                                                             \
            auto rtvs = renodx::utils::swapchain::GetRenderTargets(cmd_list);                         \
            auto* device = cmd_list->get_device();                                                    \
            bool changed = false;                                                                     \
            for (auto rtv : rtvs) {                                                                   \
              MarkMotionBlurCloneTarget(rtv, device);                                                 \
              changed = ActivateCloneHotSwapIfTracked(device, rtv) || changed;                        \
              TrackActiveMotionBlurResource(rtv);                                                     \
            }                                                                                         \
            if (changed) {                                                                            \
              renodx::mods::swapchain::FlushDescriptors(cmd_list);                                    \
              renodx::mods::swapchain::RewriteRenderTargets(cmd_list, rtvs.size(), rtvs.data(), {0}); \
            }                                                                                         \
            return true;                                                                              \
          },                                                                                          \
      },                                                                                              \
  }

ShaderInjectData shader_injection;
static_assert(sizeof(ShaderInjectData) % 16u == 0u, "ShaderInjectData must remain 16-byte aligned");

renodx::mods::shader::CustomShaders custom_shaders = []() {
  renodx::mods::shader::CustomShaders shaders = {
      {
          0x200DBED9,
          {
              .crc32 = 0x200DBED9,
              .code = __0x200DBED9,
              .on_draw = [](auto*) {
                taa::UpdateShaderInjectionJitter(&shader_injection);
                return true;
              },
          },
      },
      UpgradeRTVReplaceShaderCallback(0xE2D609B1, ArmDofFinalCopyRenderBufferWindow),            // DOF_ScatterCompositeNear
      UpgradeRTVReplaceShaderCallback(0x7C017264, ArmDofFinalCopyRenderBufferWindow),            // DOF_ScatterCompositeFar
      UpgradeRTVReplaceShaderCallback(0xFC5542BB, DisarmDofFinalCopyRenderBufferAfterDofFinal),  // DOF_ScatterCompositeFinal

      TrackMotionBlurTilePrep(0xF05DCBFD),
      TrackMotionBlurTilePrep(0x512E2B48),
      UpgradeCopyRenderBufferRTV(0x83272BCB),

      // Motion blur ping-pongs through half-resolution scene-color intermediates.
      UpgradeMotionBlurRTV(0xBFC7D3C2),

      // Deferred rendering paths perform the actual filmic tonemap before the
      // later LUT/color-grading pass. Mark their full-size output so the extra
      // CopyRenderBuffer draws used by rain/fog paths propagate the HDR clone.
      UpgradeDeferredTonemapOutputRTV(0x410AE8C5, "DeferredRenderingFilmic"),
      UpgradeDeferredTonemapOutputRTV(0xC973024D, "DeferredRenderingFilmic_VolFog"),
      UpgradeDeferredTonemapOutputRTV(0xBDE1F4CD, "DR_TonemapRainFilter"),
      UpgradeDeferredTonemapOutputRTV(0x6E29F0AB, "DR_TonemapRainFilter_NoIR"),
      UpgradeDeferredTonemapOutputRTV(0x2EA8F13F, "DR_VolFog_TppTonemap"),
      UpgradeDeferredTonemapOutputRTV(0x59B44963, "DR_VolFog_TppTonemap_MD"),

      // LUT builders write into typeless BGRA render targets that need the clone activated.
      UpgradeRTVShader(0x244877AB),
      UpgradeRTVShader(0x3BCB1620),
      UpgradeRTVShader(0x5A8584C0),
      UpgradeRTVShader(0x5E320D1D),
      UpgradeRTVShader(0x637BB745),
      UpgradeRTVShader(0xCA7F0E3D),
      UpgradeRTVShader(0xDAA0FD48),
      UpgradeRTVShader(0xF50444CF),

      UpgradeRTVReplaceShader(0x900968FF),

      // The named Tonemap shader is the late LUT/color-grading pass in MGSV.
      UpgradeTonemapOutputRTV(0xE04D1471),

      __ALL_CUSTOM_SHADERS,
  };

  return shaders;
}();

renodx::utils::settings::Settings settings = {
    new renodx::utils::settings::Setting{
        .key = "ToneMapType",
        .binding = &shader_injection.tone_map_type,
        .value_type = renodx::utils::settings::SettingValueType::INTEGER,
        .default_value = 1.f,
        .label = "Tone Mapper",
        .section = "Tone Mapping",
        .tooltip = "Sets the tone mapper type",
        .labels = {"Vanilla", "RenoDX"},
    },
    new renodx::utils::settings::Setting{
        .key = "ToneMapPeakNits",
        .binding = &shader_injection.peak_white_nits,
        .default_value = 1000.f,
        .can_reset = false,
        .label = "Peak Brightness",
        .section = "Tone Mapping",
        .tooltip = "Sets the value of peak white in nits",
        .min = 48.f,
        .max = 4000.f,
        .is_enabled = []() { return shader_injection.tone_map_type != 0.f; },
    },
    new renodx::utils::settings::Setting{
        .key = "ToneMapGameNits",
        .binding = &shader_injection.diffuse_white_nits,
        .default_value = 203.f,
        .label = "Game Brightness",
        .section = "Tone Mapping",
        .tooltip = "Sets the value of 100% white in nits",
        .min = 48.f,
        .max = 500.f,
    },
    new renodx::utils::settings::Setting{
        .key = "ToneMapUINits",
        .binding = &shader_injection.graphics_white_nits,
        .default_value = 203.f,
        .label = "UI Brightness",
        .section = "Tone Mapping",
        .tooltip = "Sets the brightness of UI and HUD elements in nits",
        .min = 48.f,
        .max = 500.f,
    },
    new renodx::utils::settings::Setting{
        .key = "GammaCorrection",
        .binding = &shader_injection.gamma_correction,
        .value_type = renodx::utils::settings::SettingValueType::INTEGER,
        .default_value = 2.f,
        .label = "SDR EOTF Emulation",
        .section = "Tone Mapping",
        .tooltip = "Emulates a 2.2 EOTF (use with HDR or sRGB)",
        .labels = {"Off", "2.2", "2.2 (By Luminance)"},
    },
    new renodx::utils::settings::Setting{
        .key = "ColorGradeExposure",
        .binding = &shader_injection.tone_map_exposure,
        .default_value = 1.f,
        .label = "Exposure",
        .section = "Color Grading",
        .max = 2.f,
        .format = "%.2f",
        .is_enabled = []() { return shader_injection.tone_map_type != 0.f; },
    },
    new renodx::utils::settings::Setting{
        .key = "ColorGradeHighlights",
        .binding = &shader_injection.tone_map_highlights,
        .default_value = 55.f,
        .label = "Highlights",
        .section = "Color Grading",
        .max = 100.f,
        .is_enabled = []() { return shader_injection.tone_map_type != 0.f; },
        .parse = [](float value) { return value * 0.02f; },
    },
    new renodx::utils::settings::Setting{
        .key = "ColorGradeShadows",
        .binding = &shader_injection.tone_map_shadows,
        .default_value = 50.f,
        .label = "Shadows",
        .section = "Color Grading",
        .max = 100.f,
        .is_enabled = []() { return shader_injection.tone_map_type != 0.f; },
        .parse = [](float value) { return value * 0.02f; },
    },
    new renodx::utils::settings::Setting{
        .key = "ColorGradeContrast",
        .binding = &shader_injection.tone_map_contrast,
        .default_value = 50.f,
        .label = "Contrast",
        .section = "Color Grading",
        .max = 100.f,
        .is_enabled = []() { return shader_injection.tone_map_type != 0.f; },
        .parse = [](float value) { return value * 0.02f; },
    },
    new renodx::utils::settings::Setting{
        .key = "ColorGradeConeContrast",
        .binding = &shader_injection.tone_map_cone_contrast,
        .default_value = 50.f,
        .label = "Cone Contrast",
        .section = "Color Grading",
        .max = 100.f,
        .is_enabled = []() { return shader_injection.tone_map_type != 0.f; },
        .parse = [](float value) { return value * 0.02f; },
    },
    new renodx::utils::settings::Setting{
        .key = "ColorGradeSaturation",
        .binding = &shader_injection.tone_map_saturation,
        .default_value = 50.f,
        .label = "Saturation",
        .section = "Color Grading",
        .max = 100.f,
        .is_enabled = []() { return shader_injection.tone_map_type != 0.f; },
        .parse = [](float value) { return value * 0.02f; },
    },
    new renodx::utils::settings::Setting{
        .key = "ColorGradeHighlightSaturation",
        .binding = &shader_injection.tone_map_highlight_saturation,
        .default_value = 50.f,
        .label = "Highlight Saturation",
        .section = "Color Grading",
        .tooltip = "Adds or removes highlight color.",
        .max = 100.f,
        .is_enabled = []() { return shader_injection.tone_map_type != 0.f; },
        .parse = [](float value) { return value * 0.02f; },
    },
    new renodx::utils::settings::Setting{
        .key = "ColorGradeDechroma",
        .binding = &shader_injection.tone_map_dechroma,
        .default_value = 0.f,
        .label = "Dechroma",
        .section = "Color Grading",
        .tooltip = "Controls highlight desaturation due to overexposure.",
        .max = 100.f,
        .is_enabled = []() { return shader_injection.tone_map_type != 0.f; },
        .parse = [](float value) { return value * 0.01f; },
    },
    new renodx::utils::settings::Setting{
        .key = "ColorGradeFlare",
        .binding = &shader_injection.tone_map_flare,
        .default_value = 0.f,
        .label = "Flare",
        .section = "Color Grading",
        .tooltip = "Flare/Glare Compensation",
        .max = 100.f,
        .is_enabled = []() { return shader_injection.tone_map_type != 0.f; },
        .parse = [](float value) { return value * 0.01f; },
    },
    new renodx::utils::settings::Setting{
        .key = "ColorGradeScene",
        .binding = &shader_injection.scene_grade_strength,
        .default_value = 100.f,
        .label = "Scene Grading",
        .section = "Color Grading",
        .tooltip = "Selects the strength of the game's custom scene grading.",
        .max = 100.f,
        .parse = [](float value) { return value * 0.01f; },
    },
    new renodx::utils::settings::Setting{
        .key = "ColorGradeSceneScaling",
        .binding = &shader_injection.scene_grade_scaling,
        .default_value = 100.f,
        .label = "Scene Grading Scaling",
        .section = "Color Grading",
        .tooltip = "Scales the scene grading to full range when size is clamped.",
        .max = 100.f,
        .parse = [](float value) { return value * 0.01f; },
    },
    // new renodx::utils::settings::Setting{
    //     .key = "FxBloomType",
    //     .binding = &shader_injection.custom_bloom_type,
    //     .value_type = renodx::utils::settings::SettingValueType::INTEGER,
    //     .default_value = 1.f,
    //     .label = "Bloom Type",
    //     .section = "Effects",
    //     .labels = {"Vanilla", "Enhanced"},
    // },
    new renodx::utils::settings::Setting{
        .key = "FxBloom",
        .binding = &shader_injection.custom_bloom,
        .default_value = 100.f,
        .label = "Bloom",
        .section = "Effects",
        .max = 100.f,
        .parse = [](float value) { return value * 0.01f; },
    },
    new renodx::utils::settings::Setting{
        .key = "FxBoostSun",
        .binding = &shader_injection.custom_boost_sun,
        .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
        .default_value = 1.f,
        .label = "Boost Sun Brightness",
        .section = "Effects",
        .tooltip = "Boosts the brightness of the sun.",
    },
    new renodx::utils::settings::Setting{
        .value_type = renodx::utils::settings::SettingValueType::BUTTON,
        .label = "Reset All",
        .section = "Options",
        .group = "button-line-1",
        .on_change = []() {
          for (auto* setting : settings) {
            if (setting->key.empty()) continue;
            if (!setting->can_reset) continue;
            renodx::utils::settings::UpdateSetting(setting->key, setting->default_value);
          }
        },
    },
    new renodx::utils::settings::Setting{
        .value_type = renodx::utils::settings::SettingValueType::BUTTON,
        .label = "RenoDX Discord",
        .section = "Links",
        .group = "button-line-2",
        .tint = 0x5865F2,
        .on_change = []() {
          renodx::utils::platform::LaunchURL("https://discord.gg/", "Ce9bQHQrSV");
        },
    },
    new renodx::utils::settings::Setting{
        .value_type = renodx::utils::settings::SettingValueType::BUTTON,
        .label = "HDR Den Discord",
        .section = "Links",
        .group = "button-line-2",
        .tint = 0x5865F2,
        .on_change = []() {
          renodx::utils::platform::LaunchURL("https://discord.gg/", "5WZXDpmbpP");
        },
    },
    new renodx::utils::settings::Setting{
        .value_type = renodx::utils::settings::SettingValueType::BUTTON,
        .label = "More Mods",
        .section = "Links",
        .group = "button-line-2",
        .tint = 0x2B3137,
        .on_change = []() {
          renodx::utils::platform::LaunchURL("https://github.com/clshortfuse/renodx/wiki/Mods");
        },
    },
    new renodx::utils::settings::Setting{
        .value_type = renodx::utils::settings::SettingValueType::BUTTON,
        .label = "Github",
        .section = "Links",
        .group = "button-line-2",
        .tint = 0x2B3137,
        .on_change = []() {
          renodx::utils::platform::LaunchURL("https://github.com/clshortfuse/renodx");
        },
    },
    new renodx::utils::settings::Setting{
        .value_type = renodx::utils::settings::SettingValueType::BUTTON,
        .label = "Musa's Ko-Fi",
        .section = "Links",
        .group = "button-line-3",
        .tint = 0xFF5A16,
        .on_change = []() { renodx::utils::platform::LaunchURL("https://ko-fi.com/musaqh"); },
    },
    new renodx::utils::settings::Setting{
        .value_type = renodx::utils::settings::SettingValueType::BUTTON,
        .label = "ShortFuse's Ko-Fi",
        .section = "Links",
        .group = "button-line-3",
        .tint = 0xFF5A16,
        .on_change = []() { renodx::utils::platform::LaunchURL("https://ko-fi.com/shortfuse"); },
    },
    new renodx::utils::settings::Setting{
        .value_type = renodx::utils::settings::SettingValueType::TEXT,
        .label = std::string("Build: ") + renodx::utils::date::ISO_DATE_TIME,
        .section = "About",
    },
};

void OnPresetOff() {
  renodx::utils::settings::UpdateSettings({
      {"ToneMapType", 0.f},
      {"ToneMapPeakNits", 203.f},
      {"ToneMapGameNits", 203.f},
      {"ToneMapUINits", 203.f},
      {"GammaCorrection", 0.f},
      {"ColorGradeExposure", 1.f},
      {"ColorGradeHighlights", 50.f},
      {"ColorGradeShadows", 50.f},
      {"ColorGradeContrast", 50.f},
      {"ColorGradeConeContrast", 50.f},
      {"ColorGradeSaturation", 50.f},
      {"ColorGradeHighlightSaturation", 50.f},
      {"ColorGradeDechroma", 0.f},
      {"ColorGradeFlare", 0.f},
      {"ColorGradeScene", 100.f},
      {"ColorGradeSceneScaling", 0.f},
      {"FxBloomType", 0.f},
      {"FxBloom", 100.f},
      {"FxBoostSun", 0.f},
  });
  taa::OnPresetOff();
}

bool fired_on_init_swapchain = false;

void OnInitSwapchain(reshade::api::swapchain* swapchain, bool resize) {
  if (fired_on_init_swapchain) return;
  fired_on_init_swapchain = true;
  auto peak = renodx::utils::swapchain::GetPeakNits(swapchain);
  if (peak.has_value()) {
    settings[1]->default_value = peak.value();
    settings[1]->can_reset = true;
  }
  bool was_upgraded = renodx::mods::swapchain::IsUpgraded(swapchain);
  if (was_upgraded) {
    settings[1]->default_value = 100.f;
  }
}

void OnPresent(
    reshade::api::command_queue* queue,
    reshade::api::swapchain* swapchain,
    const reshade::api::rect* source_rect,
    const reshade::api::rect* dest_rect,
    uint32_t dirty_rect_count,
    const reshade::api::rect* dirty_rects) {
  (void)queue;
  (void)swapchain;
  (void)source_rect;
  (void)dest_rect;
  (void)dirty_rect_count;
  (void)dirty_rects;

  DeactivateDofFinalCopyRenderBufferTarget();
  ResetTonemapCopyTracking();
}

bool initialized = false;

}  // namespace

extern "C" __declspec(dllexport) constexpr const char* NAME = "RenoDX";
extern "C" __declspec(dllexport) constexpr const char* DESCRIPTION = "RenoDX for METAL GEAR SOLID V: THE PHANTOM PAIN";

BOOL APIENTRY DllMain(HMODULE h_module, DWORD fdw_reason, LPVOID lpv_reserved) {
  switch (fdw_reason) {
    case DLL_PROCESS_ATTACH:
      if (!reshade::register_addon(h_module)) return FALSE;

      taa::AppendSettings(settings, &shader_injection);

      if (!initialized) {
        renodx::mods::swapchain::force_borderless = true;
        renodx::mods::swapchain::prevent_full_screen = true;

        // renodx::mods::shader::force_pipeline_cloning = true;

        renodx::mods::shader::expected_constant_buffer_index = 13;
        renodx::mods::shader::minimum_constant_buffer_stages =
            reshade::api::shader_stage::vertex
            | reshade::api::shader_stage::pixel
            | reshade::api::shader_stage::compute;
        renodx::mods::swapchain::expected_constant_buffer_index = 13;

        renodx::mods::swapchain::use_resource_cloning = true;

        // DevKit shows the LUT builder and scene RTs as typeless resources with
        // unorm views. Mark them as clone targets only; UpgradeRTVReplaceShader
        // activates the clone on the shader draws that should render into HDR.
        renodx::mods::swapchain::resource_upgrade_infos.push_back({
            .old_format = reshade::api::format::b8g8r8a8_typeless,
            .new_format = reshade::api::format::r16g16b16a16_float,
            .use_resource_view_cloning = true,
            .use_resource_view_hot_swap = true,
            .usage_set = static_cast<uint32_t>(
                reshade::api::resource_usage::shader_resource
                | reshade::api::resource_usage::render_target),
            .dimensions = {.width = 256, .height = 16},
            .usage_include = reshade::api::resource_usage::render_target,
            .name = "lutbuilder_256x16_bgra8_typeless_hot_swap",
        });

        initialized = true;
      }

      reshade::register_event<reshade::addon_event::copy_resource>(OnCopyTonemapOutputResource);
      reshade::register_event<reshade::addon_event::init_swapchain>(OnInitSwapchain);  // auto detect peak and paper white
      reshade::register_event<reshade::addon_event::present>(OnPresent);

      break;
    case DLL_PROCESS_DETACH:
      reshade::unregister_event<reshade::addon_event::copy_resource>(OnCopyTonemapOutputResource);
      reshade::unregister_event<reshade::addon_event::init_swapchain>(OnInitSwapchain);  // auto detect peak and paper white
      reshade::unregister_event<reshade::addon_event::present>(OnPresent);
      reshade::unregister_addon(h_module);
      break;
  }
  // The persistent experimental toggle registers the native-jitter/resolve
  // path and remains default-off.
  taa::prepare_velocity_target = PrepareTaaVelocityTarget;
  taa::Use(fdw_reason, &shader_injection);
  renodx::utils::settings::Use(fdw_reason, &settings, &OnPresetOff);

  renodx::mods::swapchain::Use(fdw_reason, &shader_injection);

  renodx::mods::shader::Use(fdw_reason, custom_shaders, &shader_injection);

  return TRUE;
}
