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
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <deps/imgui/imgui.h>
#include <include/reshade.hpp>

#include "../../mods/shader.hpp"
#include "../../mods/swapchain_v2.hpp"
#include "../../utils/date.hpp"
#include "../../utils/random.hpp"
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

bool OnUpgradeRTVReplaceShaderDraw(reshade::api::command_list* cmd_list) {
  auto rtvs = renodx::utils::swapchain::GetRenderTargets(cmd_list);
  auto* device = cmd_list->get_device();
  bool changed = false;
  for (auto rtv : rtvs) {
    changed = ActivateCloneHotSwapIfTracked(device, rtv) || changed;
    ArmTonemapCopyResourcePropagationForRTV(device, rtv);
  }
  if (changed) {
    renodx::mods::swapchain::FlushDescriptors(cmd_list);
    renodx::mods::swapchain::RewriteRenderTargets(cmd_list, rtvs.size(), rtvs.data(), {0});
  }
  return true;
}

#define UpgradeRTVReplaceShader(value)               \
  {                                                  \
      value,                                         \
      {                                              \
          .crc32 = value,                            \
          .code = __##value,                         \
          .on_draw = &OnUpgradeRTVReplaceShaderDraw, \
      },                                             \
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
float output_mode = 1.f;
// Whether the march may fetch distant steps from the reduced depth chain. Host-side, so
// "off" never builds the chain and the shader falls back to the exact fetch.
float ao_depth_mips_enabled = 0.f;
renodx::utils::settings::Setting* tone_map_peak_nits_setting = nullptr;
renodx::utils::settings::Setting* tone_map_game_nits_setting = nullptr;
renodx::utils::settings::Setting* tone_map_ui_nits_setting = nullptr;

// The 1x1 stand-in both extra AO bindings fall back to. The mod treats a null view as a
// missing binding and skips the draw, which would disable the AO whenever a capture or the
// mip chain is unavailable, so a binding must always return something.
reshade::api::resource ao_fallback_resource = {0};
reshade::api::resource_view ao_fallback_view = {0};

void DestroyAoFallbackView(reshade::api::device* device) {
  if (ao_fallback_view.handle != 0) {
    device->destroy_resource_view(ao_fallback_view);
    ao_fallback_view = {0};
  }
  if (ao_fallback_resource.handle != 0) {
    device->destroy_resource(ao_fallback_resource);
    ao_fallback_resource = {0};
  }
}

reshade::api::resource_view GetAoFallbackView(reshade::api::command_list* cmd_list) {
  if (ao_fallback_view.handle != 0) return ao_fallback_view;
  auto* device = cmd_list->get_device();
  const reshade::api::resource_desc desc(
      1u, 1u, 1u, 1u,
      reshade::api::format::r16g16b16a16_float,
      1u,
      reshade::api::memory_heap::gpu_only,
      reshade::api::resource_usage::shader_resource
          | reshade::api::resource_usage::copy_dest);
  if (device->create_resource(desc, nullptr, desc.usage, &ao_fallback_resource)
      && device->create_resource_view(
          ao_fallback_resource,
          reshade::api::resource_usage::shader_resource,
          reshade::api::resource_view_desc(reshade::api::format::r16g16b16a16_float),
          &ao_fallback_view)) {
    reshade::log::message(reshade::log::level::info, "mgsv: AO fallback view created");
    return ao_fallback_view;
  }
  DestroyAoFallbackView(device);
  reshade::log::message(reshade::log::level::error, "mgsv: AO fallback view failed");
  return {0};
}

// The AO march's depth mip chain.
//
// The march samples the depth buffer at its own sample positions, so an occluder that falls
// between two of them is invisible however many slices are added - measured as the reason the
// effect wanted a high step count. Every level here holds the *nearest* of the four texels
// below it, so a distant step can fetch one texel that already covers the footprint its
// spacing would otherwise skip over. XeGTAO's depth mip sampling is the same idea.
//
// Raw depth is reduced, not linearised. The pass linearises as z / (depth - w), so a larger
// raw value is nearer and background sits at the small end; a max-reduce is therefore already
// the conservative direction and keeps the projection constants out of the prefilter.
//
// Level k covers W >> (k + 1), so the chain starts at half resolution and stops at a
// sixteenth - the span a step of tens of pixels needs, without paying for levels the march
// never reaches.
constexpr uint32_t kDepthMipLevels = 4;

struct DepthMipChain {
  reshade::api::resource resource = {0};
  reshade::api::resource_view view = {0};  // all levels, bound at t6
  std::array<reshade::api::resource_view, kDepthMipLevels> level_views = {};         // u0
  std::array<reshade::api::resource_view, kDepthMipLevels> level_source_views = {};  // t0
  reshade::api::pipeline_layout layout = {0};
  reshade::api::pipeline pipeline = {0};
  uint32_t width = 0;
  uint32_t height = 0;
  bool ready = false;
};
DepthMipChain depth_mip_chain;

void DestroyDepthMipChain(reshade::api::device* device) {
  depth_mip_chain.ready = false;
  for (auto& view : depth_mip_chain.level_views) {
    if (view.handle != 0) {
      device->destroy_resource_view(view);
      view = {0};
    }
  }
  for (auto& view : depth_mip_chain.level_source_views) {
    if (view.handle != 0) {
      device->destroy_resource_view(view);
      view = {0};
    }
  }
  if (depth_mip_chain.view.handle != 0) {
    device->destroy_resource_view(depth_mip_chain.view);
    depth_mip_chain.view = {0};
  }
  if (depth_mip_chain.resource.handle != 0) {
    device->destroy_resource(depth_mip_chain.resource);
    depth_mip_chain.resource = {0};
  }
  if (depth_mip_chain.pipeline.handle != 0) {
    device->destroy_pipeline(depth_mip_chain.pipeline);
    depth_mip_chain.pipeline = {0};
  }
  if (depth_mip_chain.layout.handle != 0) {
    device->destroy_pipeline_layout(depth_mip_chain.layout);
    depth_mip_chain.layout = {0};
  }
  depth_mip_chain.width = 0;
  depth_mip_chain.height = 0;
}

bool EnsureDepthMipChain(reshade::api::device* device, uint32_t width, uint32_t height) {
  if (depth_mip_chain.ready
      && depth_mip_chain.width == width
      && depth_mip_chain.height == height) {
    return true;
  }
  DestroyDepthMipChain(device);

  const reshade::api::resource_desc desc(
      std::max(width / 2u, 1u), std::max(height / 2u, 1u), 1u, kDepthMipLevels,
      reshade::api::format::r32_float,
      1u,
      reshade::api::memory_heap::gpu_only,
      reshade::api::resource_usage::shader_resource
          | reshade::api::resource_usage::unordered_access);
  if (!device->create_resource(desc, nullptr, desc.usage, &depth_mip_chain.resource)) {
    reshade::log::message(reshade::log::level::error, "mgsv: depth mip chain allocation failed");
    return false;
  }
  if (!device->create_resource_view(
          depth_mip_chain.resource,
          reshade::api::resource_usage::shader_resource,
          reshade::api::resource_view_desc(reshade::api::format::r32_float),
          &depth_mip_chain.view)) {
    reshade::log::message(reshade::log::level::error, "mgsv: depth mip chain read view failed");
    return false;
  }
  for (uint32_t level = 0; level < kDepthMipLevels; ++level) {
    const reshade::api::resource_view_desc level_desc(
        reshade::api::format::r32_float, level, 1u, 0u, 1u);
    if (!device->create_resource_view(
            depth_mip_chain.resource,
            reshade::api::resource_usage::unordered_access,
            level_desc,
            &depth_mip_chain.level_views[level])) {
      reshade::log::message(reshade::log::level::error, "mgsv: depth mip chain write view failed");
      return false;
    }
    if (!device->create_resource_view(
            depth_mip_chain.resource,
            reshade::api::resource_usage::shader_resource,
            level_desc,
            &depth_mip_chain.level_source_views[level])) {
      reshade::log::message(reshade::log::level::error, "mgsv: depth mip chain level view failed");
      return false;
    }
  }

  std::array<reshade::api::pipeline_layout_param, 2> params = {};
  params[0].type = reshade::api::pipeline_layout_param_type::push_descriptors;
  params[0].push_descriptors.count = 1u;
  params[0].push_descriptors.type = reshade::api::descriptor_type::shader_resource_view;
  params[0].push_descriptors.dx_register_index = 0u;
  params[0].push_descriptors.dx_register_space = 0u;
  params[1].type = reshade::api::pipeline_layout_param_type::push_descriptors;
  params[1].push_descriptors.count = 1u;
  params[1].push_descriptors.type = reshade::api::descriptor_type::unordered_access_view;
  params[1].push_descriptors.dx_register_index = 0u;
  params[1].push_descriptors.dx_register_space = 0u;
  if (!device->create_pipeline_layout(
          static_cast<uint32_t>(params.size()), params.data(), &depth_mip_chain.layout)) {
    reshade::log::message(reshade::log::level::error, "mgsv: depth mip chain layout failed");
    return false;
  }

  reshade::api::shader_desc shader_description = {
      .code = __mgsv_ao_depth_mips.data(),
      .code_size = __mgsv_ao_depth_mips.size(),
  };
  const reshade::api::pipeline_subobject subobject = {
      .type = reshade::api::pipeline_subobject_type::compute_shader,
      .count = 1,
      .data = &shader_description,
  };
  if (!device->create_pipeline(depth_mip_chain.layout, 1, &subobject, &depth_mip_chain.pipeline)) {
    reshade::log::message(reshade::log::level::error, "mgsv: depth mip chain pipeline failed");
    return false;
  }

  depth_mip_chain.width = width;
  depth_mip_chain.height = height;
  depth_mip_chain.ready = true;
  reshade::log::message(reshade::log::level::info, "mgsv: depth mip chain created");
  return true;
}

// Builds the chain immediately before the AO dispatch, from the depth the AO is about to read.
//
// The prefilter writes t0 and u0, the same registers the replacement uses for its input and
// its output, so both are saved and put back before the replacement runs.
bool BuildDepthMips(reshade::api::command_list* cmd_list) {
  shader_injection.ao_depth_mips = 0.f;
  if (shader_injection.ao_replace == 0.f || ao_depth_mips_enabled == 0.f) return true;

  auto* device = cmd_list->get_device();
  const auto depth_view = renodx::utils::state::GetBoundResourceView(
      cmd_list,
      reshade::api::shader_stage::compute,
      reshade::api::descriptor_type::shader_resource_view,
      1u);
  if (depth_view.handle == 0) return true;

  const auto depth_desc = device->get_resource_desc(device->get_resource_from_view(depth_view));
  if (depth_desc.type != reshade::api::resource_type::texture_2d) return true;
  const uint32_t width = depth_desc.texture.width;
  const uint32_t height = depth_desc.texture.height;
  if (width < 2u || height < 2u) return true;

  if (!EnsureDepthMipChain(device, width, height)) return true;

  const auto previous_source = renodx::utils::state::GetBoundResourceView(
      cmd_list,
      reshade::api::shader_stage::compute,
      reshade::api::descriptor_type::shader_resource_view,
      0u);
  const auto previous_target = renodx::utils::state::GetBoundResourceView(
      cmd_list,
      reshade::api::shader_stage::compute,
      reshade::api::descriptor_type::unordered_access_view,
      0u);

  cmd_list->bind_pipeline(reshade::api::pipeline_stage::all_compute, depth_mip_chain.pipeline);
  for (uint32_t level = 0; level < kDepthMipLevels; ++level) {
    const reshade::api::resource_view source =
        level == 0 ? depth_view : depth_mip_chain.level_source_views[level - 1];
    const reshade::api::resource_view target = depth_mip_chain.level_views[level];
    const reshade::api::descriptor_table_update updates[] = {
        {.table = {},
         .binding = 0u,
         .array_offset = 0u,
         .count = 1u,
         .type = reshade::api::descriptor_type::shader_resource_view,
         .descriptors = &source},
        {.table = {},
         .binding = 0u,
         .array_offset = 0u,
         .count = 1u,
         .type = reshade::api::descriptor_type::unordered_access_view,
         .descriptors = &target},
    };
    cmd_list->push_descriptors(
        reshade::api::shader_stage::all_compute, depth_mip_chain.layout, 0u, updates[0]);
    cmd_list->push_descriptors(
        reshade::api::shader_stage::all_compute, depth_mip_chain.layout, 1u, updates[1]);

    const uint32_t level_width = std::max(width >> (level + 1u), 1u);
    const uint32_t level_height = std::max(height >> (level + 1u), 1u);
    cmd_list->dispatch((level_width + 7u) / 8u, (level_height + 7u) / 8u, 1u);
  }

  if (previous_source.handle != 0) {
    const reshade::api::descriptor_table_update restore = {
        .table = {},
        .binding = 0u,
        .array_offset = 0u,
        .count = 1u,
        .type = reshade::api::descriptor_type::shader_resource_view,
        .descriptors = &previous_source,
    };
    cmd_list->push_descriptors(
        reshade::api::shader_stage::all_compute, depth_mip_chain.layout, 0u, restore);
  }
  if (previous_target.handle != 0) {
    const reshade::api::descriptor_table_update restore = {
        .table = {},
        .binding = 0u,
        .array_offset = 0u,
        .count = 1u,
        .type = reshade::api::descriptor_type::unordered_access_view,
        .descriptors = &previous_target,
    };
    cmd_list->push_descriptors(
        reshade::api::shader_stage::all_compute, depth_mip_chain.layout, 1u, restore);
  }

  shader_injection.ao_depth_mips = 1.f;
  return true;
}

reshade::api::resource_view GetDepthMipView(reshade::api::command_list* cmd_list) {
  if (depth_mip_chain.ready) return depth_mip_chain.view;
  return GetAoFallbackView(cmd_list);
}


bool IsHDROutputSelected() { return output_mode != 0.f; }

void SyncOutputMode() {
  if (IsHDROutputSelected()) {
    shader_injection.peak_white_nits = tone_map_peak_nits_setting->GetValue();
    shader_injection.diffuse_white_nits = tone_map_game_nits_setting->GetValue();
    shader_injection.graphics_white_nits = tone_map_ui_nits_setting->GetValue();
    shader_injection.swap_chain_output_preset = 1.f;
  } else {
    shader_injection.peak_white_nits = 80.f;
    shader_injection.diffuse_white_nits = 80.f;
    shader_injection.graphics_white_nits = 80.f;
    shader_injection.swap_chain_output_preset = 0.f;
  }
}

// The AO pass, dispatched by the addon so it can write the occlusion somewhere other than the
// light. The light output is an exact copy of its input: this pass must not be able to affect
// anything that reads the light, which is what killed the skin's scattering when the occlusion
// was applied here.
struct AoPass {
  reshade::api::pipeline_layout layout = {0};
  reshade::api::pipeline pipeline = {0};
  reshade::api::resource occlusion = {0};
  reshade::api::resource_view occlusion_view = {0};
  reshade::api::resource_view occlusion_uav = {0};
  uint32_t width = 0;
  uint32_t height = 0;
  bool ready = false;
};
AoPass ao_pass;

void DestroyAoPass(reshade::api::device* device) {
  ao_pass.ready = false;
  if (ao_pass.occlusion_uav.handle != 0) {
    device->destroy_resource_view(ao_pass.occlusion_uav);
    ao_pass.occlusion_uav = {0};
  }
  if (ao_pass.occlusion_view.handle != 0) {
    device->destroy_resource_view(ao_pass.occlusion_view);
    ao_pass.occlusion_view = {0};
  }
  if (ao_pass.occlusion.handle != 0) {
    device->destroy_resource(ao_pass.occlusion);
    ao_pass.occlusion = {0};
  }
  if (ao_pass.pipeline.handle != 0) {
    device->destroy_pipeline(ao_pass.pipeline);
    ao_pass.pipeline = {0};
  }
  if (ao_pass.layout.handle != 0) {
    device->destroy_pipeline_layout(ao_pass.layout);
    ao_pass.layout = {0};
  }
  ao_pass.width = 0;
  ao_pass.height = 0;
}

bool EnsureAoPass(reshade::api::device* device, uint32_t width, uint32_t height) {
  if (ao_pass.ready && ao_pass.width == width && ao_pass.height == height) return true;
  DestroyAoPass(device);

  const reshade::api::resource_desc desc(
      width, height, 1u, 1u,
      reshade::api::format::r16g16b16a16_float,
      1u,
      reshade::api::memory_heap::gpu_only,
      reshade::api::resource_usage::shader_resource
          | reshade::api::resource_usage::unordered_access);
  if (!device->create_resource(desc, nullptr, desc.usage, &ao_pass.occlusion)
      || !device->create_resource_view(
          ao_pass.occlusion, reshade::api::resource_usage::shader_resource,
          reshade::api::resource_view_desc(reshade::api::format::r16g16b16a16_float),
          &ao_pass.occlusion_view)
      || !device->create_resource_view(
          ao_pass.occlusion, reshade::api::resource_usage::unordered_access,
          reshade::api::resource_view_desc(reshade::api::format::r16g16b16a16_float),
          &ao_pass.occlusion_uav)) {
    reshade::log::message(reshade::log::level::error, "mgsv: AO occlusion target failed");
    return false;
  }

  std::array<reshade::api::pipeline_layout_param, 3> params = {};
  params[0].type = reshade::api::pipeline_layout_param_type::push_descriptors;
  params[0].push_descriptors.count = 7u;  // t0..t6
  params[0].push_descriptors.type = reshade::api::descriptor_type::texture_shader_resource_view;
  params[0].push_descriptors.dx_register_index = 0u;
  params[0].push_descriptors.dx_register_space = 0u;
  params[1].type = reshade::api::pipeline_layout_param_type::push_descriptors;
  params[1].push_descriptors.count = 2u;  // u0 light, u1 occlusion
  params[1].push_descriptors.type = reshade::api::descriptor_type::texture_unordered_access_view;
  params[1].push_descriptors.dx_register_index = 0u;
  params[1].push_descriptors.dx_register_space = 0u;
  params[2].type = reshade::api::pipeline_layout_param_type::push_constants;
  params[2].push_constants.count = static_cast<uint32_t>(sizeof(shader_injection) / sizeof(uint32_t));
  params[2].push_constants.dx_register_index = 13u;
  params[2].push_constants.dx_register_space = 0u;
  params[2].push_constants.visibility = reshade::api::shader_stage::compute;
  if (!device->create_pipeline_layout(
          static_cast<uint32_t>(params.size()), params.data(), &ao_pass.layout)) {
    reshade::log::message(reshade::log::level::error, "mgsv: AO layout failed");
    return false;
  }

  reshade::api::shader_desc shader_description = {
      .code = __0xB2601810.data(),
      .code_size = __0xB2601810.size(),
  };
  const reshade::api::pipeline_subobject subobject = {
      .type = reshade::api::pipeline_subobject_type::compute_shader,
      .count = 1,
      .data = &shader_description,
  };
  if (!device->create_pipeline(ao_pass.layout, 1, &subobject, &ao_pass.pipeline)) {
    reshade::log::message(reshade::log::level::error, "mgsv: AO pipeline failed");
    return false;
  }

  ao_pass.width = width;
  ao_pass.height = height;
  ao_pass.ready = true;
  reshade::log::message(reshade::log::level::info, "mgsv: AO pass created (occlusion split)");
  return true;
}

// Returns false when the addon takes the dispatch.
bool DispatchAo(reshade::api::command_list* cmd_list) {
  if (shader_injection.ao_replace == 0.f) return true;

  auto* device = cmd_list->get_device();

  const auto depth_view = renodx::utils::state::GetBoundResourceView(
      cmd_list, reshade::api::shader_stage::compute,
      reshade::api::descriptor_type::shader_resource_view, 1u);
  const auto normal_view = renodx::utils::state::GetBoundResourceView(
      cmd_list, reshade::api::shader_stage::compute,
      reshade::api::descriptor_type::shader_resource_view, 2u);
  const auto light_view = renodx::utils::state::GetBoundResourceView(
      cmd_list, reshade::api::shader_stage::compute,
      reshade::api::descriptor_type::shader_resource_view, 4u);
  const auto target_view = renodx::utils::state::GetBoundResourceView(
      cmd_list, reshade::api::shader_stage::compute,
      reshade::api::descriptor_type::unordered_access_view, 0u);
  if (depth_view.handle == 0 || normal_view.handle == 0 || light_view.handle == 0
      || target_view.handle == 0) {
    return true;
  }

  const auto depth_desc = device->get_resource_desc(device->get_resource_from_view(depth_view));
  if (depth_desc.type != reshade::api::resource_type::texture_2d) return true;
  const uint32_t width = depth_desc.texture.width;
  const uint32_t height = depth_desc.texture.height;
  if (!EnsureAoPass(device, width, height)) return true;

  BuildDepthMips(cmd_list);

  std::array<reshade::api::resource_view, 7> saved;
  for (uint32_t slot = 0; slot < saved.size(); ++slot) {
    saved[slot] = renodx::utils::state::GetBoundResourceView(
        cmd_list, reshade::api::shader_stage::compute,
        reshade::api::descriptor_type::shader_resource_view, slot);
  }

  const auto fallback = GetAoFallbackView(cmd_list);
  const std::array<reshade::api::resource_view, 7> sources = {
      fallback,                  // t0, unused
      depth_view,                // t1
      normal_view,               // t2
      fallback,                  // t3, unused
      light_view,                // t4
      fallback,                  // t5, unused
      GetDepthMipView(cmd_list), // t6
  };
  const std::array<reshade::api::resource_view, 2> targets = {
      target_view, ao_pass.occlusion_uav};
  const reshade::api::descriptor_table_update source_update = {
      .table = {0u}, .binding = 0u, .array_offset = 0u,
      .count = static_cast<uint32_t>(sources.size()),
      .type = reshade::api::descriptor_type::texture_shader_resource_view,
      .descriptors = sources.data(),
  };
  const reshade::api::descriptor_table_update target_update = {
      .table = {0u}, .binding = 0u, .array_offset = 0u,
      .count = static_cast<uint32_t>(targets.size()),
      .type = reshade::api::descriptor_type::texture_unordered_access_view,
      .descriptors = targets.data(),
  };

  cmd_list->bind_pipeline(reshade::api::pipeline_stage::all_compute, ao_pass.pipeline);
  cmd_list->push_descriptors(
      reshade::api::shader_stage::all_compute, ao_pass.layout, 0u, source_update);
  cmd_list->push_descriptors(
      reshade::api::shader_stage::all_compute, ao_pass.layout, 1u, target_update);
  cmd_list->push_constants(
      reshade::api::shader_stage::all_compute, ao_pass.layout, 2u, 0u,
      static_cast<uint32_t>(sizeof(shader_injection) / sizeof(uint32_t)), &shader_injection);
  // Transposed, as the retail pass is.
  cmd_list->dispatch(height, (width + 127u) / 128u, 1u);

  for (uint32_t slot = 0; slot < saved.size(); ++slot) {
    if (saved[slot].handle == 0) continue;
    const reshade::api::descriptor_table_update restore = {
        .table = {0u}, .binding = slot, .array_offset = 0u, .count = 1u,
        .type = reshade::api::descriptor_type::texture_shader_resource_view,
        .descriptors = &saved[slot],
    };
    cmd_list->push_descriptors(
        reshade::api::shader_stage::all_compute, ao_pass.layout, 0u, restore);
  }
  return false;
}

renodx::mods::shader::CustomShaders custom_shaders = []() {
  renodx::mods::shader::CustomShaders shaders = {
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

      // The AO replacement replaces the *second* stage of the game's AO chain
      // (0xB2601810), the one that applies the occlusion to the light. The first stage
      // (0x6DAF0C4A) is left vanilla: its output only feeds this one, and it gates that
      // input by a fresnel term, so an occlusion value fed into it reached nothing but
      // characters. The later blur and composite of the chain do not run in normal play,
      // so nothing downstream smooths this pass and the game's own AO is inactive.
      // The decision is host-side, so "off" runs the original shader.
      //
      // t6 carries the depth mip chain the march fetches its distant steps from; it is
      // rebuilt for every dispatch in the callback below.
      {0xB2601810,
       [] {
         auto shader = renodx::mods::shader::CreateCallbackShader(
             0xB2601810, __0xB2601810, [](reshade::api::command_list*) {
               return shader_injection.ao_replace != 0.f;
             });
         // The addon dispatches this itself: the pass writes the light through unchanged and
         // delivers the occlusion separately, which the module's view path cannot do.
         shader.on_draw = &DispatchAo;
         return shader;
       }()},

      __ALL_CUSTOM_SHADERS,
  };

  return shaders;
}();

renodx::utils::settings::Settings settings = {
    new renodx::utils::settings::Setting{
        .key = "OutputMode",
        .binding = &output_mode,
        .value_type = renodx::utils::settings::SettingValueType::INTEGER,
        .default_value = 1.f,
        .can_reset = false,
        .label = "Output Mode",
        .section = "Output",
        .tooltip = "Selects SDR gamma 2.2 or HDR10 PQ output.",
        .labels = {"SDR", "HDR"},
        .on_change_value = [](float, float) { SyncOutputMode(); },
    },
    tone_map_peak_nits_setting = new renodx::utils::settings::Setting{
        .key = "ToneMapPeakNits",
        .binding = &shader_injection.peak_white_nits,
        .default_value = 1000.f,
        .can_reset = false,
        .label = "Peak Brightness",
        .section = "Output",
        .tooltip = "Sets the value of peak white in nits",
        .min = 48.f,
        .max = 4000.f,
        .is_enabled = []() { return IsHDROutputSelected() && shader_injection.tone_map_type != 0.f; },
    },
    tone_map_game_nits_setting = new renodx::utils::settings::Setting{
        .key = "ToneMapGameNits",
        .binding = &shader_injection.diffuse_white_nits,
        .default_value = 203.f,
        .label = "Game Brightness",
        .section = "Output",
        .tooltip = "Sets the value of 100% white in nits",
        .min = 48.f,
        .max = 500.f,
        .is_enabled = &IsHDROutputSelected,
    },
    tone_map_ui_nits_setting = new renodx::utils::settings::Setting{
        .key = "ToneMapUINits",
        .binding = &shader_injection.graphics_white_nits,
        .default_value = 203.f,
        .label = "UI Brightness",
        .section = "Output",
        .tooltip = "Sets the brightness of UI and HUD elements in nits",
        .min = 48.f,
        .max = 500.f,
        .is_enabled = &IsHDROutputSelected,
    },
    new renodx::utils::settings::Setting{
        .key = "GammaCorrection",
        .binding = &shader_injection.gamma_correction,
        .value_type = renodx::utils::settings::SettingValueType::INTEGER,
        .default_value = 1.f,
        .label = "SDR EOTF Emulation",
        .section = "Output",
        .tooltip = "Emulates a 2.2 EOTF (use with HDR or sRGB)",
        .labels = {"Off", "2.2"},
    },
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
        .default_value = 50.f,
        .label = "Highlights",
        .section = "Color Grading",
        .max = 100.f,
        .is_enabled = []() { return shader_injection.tone_map_type != 0.f; },
        .parse = [](float value) { return value * 0.02f; },
    },
    new renodx::utils::settings::Setting{
        .key = "ColorGradeHighlightContrast",
        .binding = &shader_injection.tone_map_contrast_highlights,
        .default_value = 55.f,
        .label = "Highlight Contrast",
        .section = "Color Grading",
        .max = 100.f,
        .is_enabled = []() { return shader_injection.tone_map_type != 0; },
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
        .key = "ColorGradeShadowContrast",
        .binding = &shader_injection.tone_map_contrast_shadows,
        .default_value = 50.f,
        .label = "Shadow Contrast",
        .section = "Color Grading",
        .max = 100.f,
        .is_enabled = []() { return shader_injection.tone_map_type != 0; },
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
        .key = "FxMotionBlur",
        .binding = &shader_injection.custom_motion_blur,
        .default_value = 100.f,
        .label = "Motion Blur",
        .section = "Effects",
        .min = 0.f,
        .max = 100.f,
        .parse = [](float value) { return value * 0.01f; },
    },
    new renodx::utils::settings::Setting{
        .key = "FxFilmGrain",
        .binding = &shader_injection.custom_grain_strength,
        .default_value = 0.f,
        .label = "Film Grain",
        .section = "Effects",
        .max = 100.f,
        .parse = [](float value) { return value * 0.02f; },
    },
    new renodx::utils::settings::Setting{
        .key = "FxSunContactHardening",
        .binding = &shader_injection.sun_contact_hardening,
        .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
        .default_value = 0.f,
        .label = "Contact-Hardening Sun Shadows",
        .section = "Shadows",
        .tooltip = "Softens sun shadows with distance from whatever casts them: sharp "
                   "where an object meets the ground, wider further away, in place of "
                   "the game's fixed-radius filter. Changes how the scene's shadows "
                   "read, so it is off by default. Vanilla shadows are kept when off.",
    },
    new renodx::utils::settings::Setting{
        .key = "FxSunShadowSoftness",
        .binding = &shader_injection.sun_shadow_softness,
        .default_value = 50.f,
        .label = "Sun Shadow Softness",
        .section = "Shadows",
        .tooltip = "Widest penumbra a fully separated shadow gets, in millimetres. "
                   "Raise it if a shadow-map mod stretches the cascade range: that "
                   "makes each shadow-map texel cover more ground, which would "
                   "otherwise shrink the softening out of sight.",
        .max = 150.f,
        .parse = [](float value) { return value * 0.001f; },
    },
    new renodx::utils::settings::Setting{
        .key = "FxSunShadowK",
        .binding = &shader_injection.sun_shadow_k,
        .default_value = 24.f,
        .label = "Penumbra Response",
        .section = "Shadows",
        .tooltip = "How quickly separation between caster and receiver reaches full "
                   "softness. Too high and every shadow saturates, which flattens "
                   "the gradient the hardening is made of.",
        .max = 100.f,
    },
    new renodx::utils::settings::Setting{
        .key = "FxSunShadowBlurPixels",
        .binding = &shader_injection.sun_shadow_blur_pixels,
        .default_value = 24.f,
        .label = "Penumbra Blur Radius",
        .section = "Shadows",
        .tooltip = "Spacing of the penumbra mask's neighbours, in screen pixels. "
                   "Larger smooths the penumbra over more of the screen.",
        .max = 50.f,
    },
    new renodx::utils::settings::Setting{
        .key = "FxSpotContactHardening",
        .binding = &shader_injection.spot_contact_hardening,
        .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
        .default_value = 0.f,
        .label = "Contact-Hardening Spot Shadows",
        .section = "Shadows",
        .tooltip = "Same treatment for spot lights. Their values are separate from "
                   "the sun's because the scales differ - a spot's cone is tens of "
                   "metres where the sun's cascade is hundreds, so the same caster "
                   "separation gives a far larger ratio. Off by default.",
    },
    new renodx::utils::settings::Setting{
        .key = "FxSpotShadowSoftness",
        .binding = &shader_injection.spot_shadow_softness,
        .default_value = 80.f,
        .label = "Spot Shadow Softness",
        .section = "Shadows",
        .tooltip = "Widest penumbra a fully separated spot shadow gets, in "
                   "millimetres. The blur at the far end of the ramp.",
        .max = 300.f,
        .parse = [](float value) { return value * 0.001f; },
    },
    new renodx::utils::settings::Setting{
        .key = "FxSpotShadowK",
        .binding = &shader_injection.spot_shadow_k,
        .default_value = 4.f,
        .label = "Spot Penumbra Response",
        .section = "Shadows",
        .tooltip = "How fast separation reaches full softness. Lower stretches the "
                   "ramp, so the blur keeps growing over a longer range. Too high "
                   "and the radius saturates, which makes the shadow grow uniformly "
                   "instead of trailing off.",
        .max = 40.f,
    },
    new renodx::utils::settings::Setting{
        .key = "FxSpotShadowBlurPixels",
        .binding = &shader_injection.spot_shadow_blur_pixels,
        .default_value = 24.f,
        .label = "Spot Penumbra Blur Radius",
        .section = "Shadows",
        .tooltip = "Spacing of the penumbra mask's neighbours, in screen pixels.",
        .max = 50.f,
    },
    new renodx::utils::settings::Setting{
        .key = "FxSpotShadowDarkness",
        .binding = &shader_injection.spot_shadow_darkness,
        .default_value = 100.f,
        .label = "Spot Shadow Darkness",
        .section = "Shadows",
        .tooltip = "Scales the shadow's occlusion, so higher is darker. The result "
                   "is clamped at fully occluded, which is why this cannot go "
                   "negative the way an unclamped multiplier can.",
        .max = 300.f,
        .parse = [](float value) { return value * 0.01f; },
    },
    new renodx::utils::settings::Setting{
        .key = "FxSpotConeFalloff",
        .binding = &shader_injection.spot_cone_falloff,
        .default_value = 100.f,
        .label = "Spot Cone Falloff",
        .section = "Shadows",
        .tooltip = "How much of the game's cone-edge falloff scales the shadow. At "
                   "100 a shadow fades toward the cone's rim, which is why vanilla "
                   "spot shadows are faint over most of the cone. Lowering it "
                   "darkens them, but going too low runs the shadow at full "
                   "strength right up to the rim, which reads as a stretch.",
        .max = 100.f,
        .parse = [](float value) { return value * 0.01f; },
    },
    new renodx::utils::settings::Setting{
        .key = "FxScreenSpaceShadows",
        .binding = &shader_injection.screen_space_shadows,
        .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
        .default_value = 0.f,
        .label = "Screen-Space Shadows",
        .section = "Shadows",
        .tooltip = "Ray-marches the depth buffer toward the sun and darkens where "
                   "the ray passes behind something nearer. Supplies the detail the "
                   "cascade map cannot resolve at its texel size - grass, low bushes, "
                   "the contact where a trunk meets the ground - out to the end of "
                   "the cascade range. Off by default.",
    },
    new renodx::utils::settings::Setting{
        .key = "FxScreenSpaceShadowStrength",
        .binding = &shader_injection.ss_shadow_strength,
        .default_value = 200.f,
        .label = "Screen-Space Shadow Strength",
        .section = "Shadows",
        .tooltip = "Attenuation per unit of occlusion, so this is the main look "
                   "dial. The march reports the fraction of its steps that found an "
                   "occluder, and that fraction is usually well below one, so the "
                   "setting's scale runs past its own value: 150 removes 60% of the "
                   "light where 40% of the march was occluded. Around 200 the core "
                   "of a strong contact flattens to black.",
        .max = 400.f,
        .parse = [](float value) { return value * 0.01f; },
    },
    new renodx::utils::settings::Setting{
        .key = "FxScreenSpaceShadowReach",
        .binding = &shader_injection.ss_shadow_reach,
        .default_value = 5.0f,
        .label = "Screen-Space Shadow Reach",
        .section = "Shadows",
        .tooltip = "How far the ray travels, as a percentage of screen height, so "
                   "the world distance grows with depth: 2.5% is about 54 px at "
                   "2160p, and reaches 29 cm at 10 m, 1.2 m at 40 m and 2.9 m at "
                   "100 m. That growth is what lets a tree shade the ground under "
                   "it at range. Raise it to catch more, at the cost of the ray "
                   "crossing geometry that is merely nearby.",
        .max = 10.f,
        .parse = [](float value) { return value * 0.02f; },
    },
    new renodx::utils::settings::Setting{
        .key = "FxScreenSpaceShadowThickness",
        .binding = &shader_injection.ss_shadow_thickness,
        .default_value = 5.f,
        .label = "Screen-Space Shadow Thickness",
        .section = "Shadows",
        .tooltip = "How far in front of the ray a surface must sit before it counts "
                   "as an occluder. Too high and thin casters are missed - at 20 mm "
                   "it exceeded a grass blade, so the ray passed beside the grass.",
        .max = 100.f,
        .parse = [](float value) { return value * 0.001f; },
    },
    new renodx::utils::settings::Setting{
        .key = "FxScreenSpaceShadowMaterialMax",
        .binding = &shader_injection.ss_shadow_material_max,
        .default_value = 100.f,
        .label = "Screen-Space Shadow Material Limit",
        .section = "Shadows",
        .tooltip = "The term runs only where the G-buffer's material index is below "
                   "this. It exists because the depth test cannot tell a caster from "
                   "the receiver's own surface on a solid object, which outlined "
                   "characters; the material separates them. Grass, terrain and "
                   "rocks all measure below 25, the character's body at 25-50. "
                   "Lowering it risks cutting foliage; raising it lets characters "
                   "shadow themselves and the ground again.",
        .max = 100.f,
        .parse = [](float value) { return value * 0.01f; },
    },
    new renodx::utils::settings::Setting{
        .key = "FxScreenSpaceShadowFadeEnd",
        .binding = &shader_injection.ss_shadow_fade_end,
        .default_value = 123.f,
        .label = "Screen-Space Shadow Fade End",
        .section = "Shadows",
        .tooltip = "Distance at which it is gone, in metres. The last cascade "
                   "reaches 122.9 m, so past this the cascade map is on its own.",
        .max = 500.f,
    },
    new renodx::utils::settings::Setting{
        .key = "AoReplace",
        .binding = &shader_injection.ao_replace,
        .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
        .default_value = 1.f,
        .label = "AO (replaces the game's)",
        .section = "Ambient Occlusion",
        .tooltip = "Replaces the second stage of the game's ambient-occlusion chain - "
                   "the one that applies the occlusion to the light - with a "
                   "visibility-bitmask AO computed from the depth and normal buffers. "
                   "Off runs the original shader, so the comparison is against the "
                   "game's own effect rather than a lookalike. Record and open "
                   "questions: MGSV_AmbientOcclusion.md.",
    },
    new renodx::utils::settings::Setting{
        .key = "AoStrength",
        .binding = &shader_injection.ao_strength,
        .default_value = 150.f,
        .label = "AO Amount",
        .section = "Ambient Occlusion",
        .tooltip = "How much of the computed visibility is applied, as a percentage. "
                   "100 is the paper's own occlusion; above that overshoots toward a "
                   "full darkening, below it fades the effect out. The default is 150, "
                   "because the paper's own thickness reads quiet at full strength. This "
                   "buffer is the whole light, so the visibility scales sunlit surfaces "
                   "too - the paper applies it to the ambient and indirect terms only, "
                   "which this buffer does not separate.",
        .max = 400.f,
        .parse = [](float value) { return value * 0.01f; },
    },
    new renodx::utils::settings::Setting{
        .key = "AoRadius",
        .binding = &shader_injection.ao_radius,
        .default_value = 50.f,
        .label = "AO Radius",
        .section = "Ambient Occlusion",
        .tooltip = "Sample radius as a fraction of the screen. Larger "
                   "gives more large-scale occlusion and less fine detail. The "
                   "occluder thickness is derived from this, so the two can never "
                   "disagree.",
        .max = 100.f,
        .parse = [](float value) { return value * 0.01f; },
    },
    new renodx::utils::settings::Setting{
        .key = "AoSlices",
        .binding = &shader_injection.ao_slices,
        .default_value = 2.f,
        .label = "AO Slices",
        .section = "Ambient Occlusion",
        .tooltip = "Directions sampled per pixel. The pattern rotates every frame, so a "
                   "frame's temporal reconstruction - the analytical TAA, FSR3 or DLSS - "
                   "accumulates the phases and a low count converges: eight phases turn "
                   "two directions into sixteen. With reconstruction Off the term is "
                   "inert and slices and steps have to carry the averaging themselves. "
                   "Cost is slices x steps x 2 depth fetches per pixel.",
        .max = 8.f,
    },
    new renodx::utils::settings::Setting{
        .key = "AoSteps",
        .binding = &shader_injection.ao_steps,
        .default_value = 4.f,
        .label = "AO Steps",
        .section = "Ambient Occlusion",
        .tooltip = "Samples taken along each slice, per side, on an exponential "
                   "distribution that is dense near the pixel and sparse further out.",
        .max = 20.f,
    },
    new renodx::utils::settings::Setting{
        .key = "AoThickness",
        .binding = &shader_injection.ao_thickness,
        .default_value = 800.f,
        .label = "AO Thickness",
        .section = "Ambient Occlusion",
        .tooltip = "Multiplier on the occluder thickness, which the VBAO paper derives "
                   "as a small constant fraction of the radius - its benchmark is five "
                   "percent, which is what 100 gives. The default is 800, forty percent; "
                   "at the paper's own five percent the effect is almost invisible here. "
                   "It is the parameter that decides "
                   "visual fidelity: too large and surfaces start to occlude themselves, "
                   "too small and every sample's angular band falls below one sector and "
                   "the occlusion disappears entirely.",
        .max = 2000.f,
        .parse = [](float value) { return value * 0.01f; },
    },
    new renodx::utils::settings::Setting{
        .key = "AoFade",
        .binding = &shader_injection.ao_fade,
        .default_value = 100.f,
        .label = "AO Fade Distance",
        .section = "Ambient Occlusion",
        .tooltip = "How far out the effect reaches before fading to nothing, as a "
                   "fraction of the depth range. Screen-space occlusion has nothing "
                   "to say at distance, and fading it out there is what keeps the "
                   "far field clean. Not part of the paper, whose equivalent is an "
                   "optional thickness that grows linearly with distance.",
        .max = 100.f,
        .parse = [](float value) { return value * 0.01f; },
    },
    new renodx::utils::settings::Setting{
        .key = "AoDepthMips",
        .binding = &ao_depth_mips_enabled,
        .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
        .default_value = 0.f,
        .label = "Coarse Depth",
        .section = "Ambient Occlusion",
        .tooltip = "Lets distant steps fetch their depth from a reduced chain, so one texel "
                   "covers the footprint the step spacing would otherwise skip over. That "
                   "is what a step far from the pixel needs: an occluder narrower than the "
                   "spacing is otherwise invisible between two samples. Pure A/B, since "
                   "whether it helps is measurable: with it off the march reads the plain "
                   "depth buffer at every step.",
    },
    new renodx::utils::settings::Setting{
        .value_type = renodx::utils::settings::SettingValueType::BUTTON,
        .label = "Reset All",
        .section = "Options",
        .group = "button-line-1",
        .on_change = []() {
          for (auto* setting : settings) {
            if (setting->key.empty()) continue;
            if (setting->is_global) continue;
            if (!setting->can_reset) continue;
            renodx::utils::settings::UpdateSetting(setting->key, setting->default_value);
          }
          SyncOutputMode();
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
    new renodx::utils::settings::Setting{
        .value_type = renodx::utils::settings::SettingValueType::TEXT,
        .label = std::string("- Thanks to Maple for the FSR3 implementation\n"),
        .section = "About",
    },
};

void OnPresetOff() {
  renodx::utils::settings::UpdateSettings({
      {"OutputMode", 0.f},
      {"ToneMapType", 0.f},
      {"ToneMapPeakNits", 203.f},
      {"ToneMapGameNits", 203.f},
      {"ToneMapUINits", 203.f},
      {"GammaCorrection", 0.f},
      {"ColorGradeExposure", 1.f},
      {"ColorGradeHighlights", 50.f},
      {"ColorGradeHighlightContrast", 50.f},
      {"ColorGradeShadows", 50.f},
      {"ColorGradeShadowContrast", 50.f},
      {"ColorGradeContrast", 50.f},
      {"ColorGradeSaturation", 50.f},
      {"ColorGradeHighlightSaturation", 50.f},
      {"ColorGradeDechroma", 0.f},
      {"ColorGradeFlare", 0.f},
      {"ColorGradeScene", 100.f},
      {"ColorGradeSceneScaling", 0.f},
      {"FxBloomType", 0.f},
      {"FxBloom", 100.f},
      {"FxBoostSun", 0.f},
      {"FxFilmGrain", 0.f},
      {"FxMotionBlur", 100.f},
  });
  SyncOutputMode();
}

bool fired_on_init_swapchain = false;

void OnInitSwapchain(reshade::api::swapchain* swapchain, bool resize) {
  if (fired_on_init_swapchain) return;
  fired_on_init_swapchain = true;
  auto peak = renodx::utils::swapchain::GetPeakNits(swapchain);
  if (peak.has_value()) {
    tone_map_peak_nits_setting->default_value = peak.value();
    tone_map_peak_nits_setting->can_reset = true;
  }
  bool was_upgraded = renodx::mods::swapchain::IsUpgraded(swapchain);
  if (was_upgraded) {
    tone_map_peak_nits_setting->default_value = 100.f;
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
  (void)source_rect;
  (void)dest_rect;
  (void)dirty_rect_count;
  (void)dirty_rects;

  SyncOutputMode();
  const auto output_color_space = IsHDROutputSelected()
                                      ? reshade::api::color_space::hdr10_st2084
                                      : reshade::api::color_space::srgb_nonlinear;
  if (swapchain->get_color_space() != output_color_space) {
    renodx::utils::swapchain::ChangeColorSpace(swapchain, output_color_space);
  }

  DeactivateDofFinalCopyRenderBufferTarget();
  ResetTonemapCopyTracking();
}

void OnDestroyDevice(reshade::api::device* device) {
  DestroyAoPass(device);
  DestroyDepthMipChain(device);
  DestroyAoFallbackView(device);
}

bool initialized = false;

}  // namespace

extern "C" __declspec(dllexport) constexpr const char* NAME = "RenoDX";
extern "C" __declspec(dllexport) constexpr const char* DESCRIPTION = "RenoDX for METAL GEAR SOLID V: THE PHANTOM PAIN";

BOOL APIENTRY DllMain(HMODULE h_module, DWORD fdw_reason, LPVOID lpv_reserved) {
  switch (fdw_reason) {
    case DLL_PROCESS_ATTACH:
      if (!reshade::register_addon(h_module)) return FALSE;

      taa::settings::AppendSettings(settings, &shader_injection);

      if (!initialized) {
        renodx::mods::swapchain::force_borderless = true;
        renodx::mods::swapchain::prevent_full_screen = true;

        renodx::mods::swapchain::v2::SetUseHDR10();
        renodx::mods::swapchain::v2::set_color_space = false;

        renodx::utils::settings::on_preset_changed_callbacks.emplace_back(
            taa::settings::ApplySettingsSnapshot);

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
  reshade::register_event<reshade::addon_event::destroy_device>(OnDestroyDevice);

      break;
    case DLL_PROCESS_DETACH:
      reshade::unregister_event<reshade::addon_event::copy_resource>(OnCopyTonemapOutputResource);
      reshade::unregister_event<reshade::addon_event::init_swapchain>(OnInitSwapchain);  // auto detect peak and paper white
      reshade::unregister_event<reshade::addon_event::present>(OnPresent);
  reshade::unregister_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
      renodx::mods::swapchain::v2::set_color_space = true;
      reshade::unregister_addon(h_module);
      break;
  }
  // Load the persisted temporal mode before the runtime reads it so shaders
  // and native reconstruction cannot disagree during the first frame.
  renodx::utils::settings::Use(fdw_reason, &settings, &OnPresetOff);
  if (fdw_reason == DLL_PROCESS_ATTACH) SyncOutputMode();

  taa::prepare_velocity_target = PrepareTaaVelocityTarget;
  taa::Use(fdw_reason);

  renodx::utils::random::Use(fdw_reason, {&shader_injection.custom_random});

  renodx::mods::swapchain::Use(fdw_reason, &shader_injection);

  renodx::mods::shader::Use(fdw_reason, custom_shaders, &shader_injection);

  return TRUE;
}
