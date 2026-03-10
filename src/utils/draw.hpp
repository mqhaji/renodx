/*
 * Copyright (C) 2024 Carlos Lopez
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <include/reshade.hpp>

#include <d3d11.h>
#include <d3d12.h>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <utility>

#include "./mutex.hpp"
#include "./render.hpp"
#include "./resource.hpp"
#include "./resource_upgrade.hpp"

namespace renodx::utils::draw {

struct SwapchainProxyPass {
  renodx::utils::render::RenderPass pass;
  std::span<const std::uint8_t> vertex_shader;
  std::span<const std::uint8_t> pixel_shader;
  int32_t expected_constant_buffer_index = -1;
  uint32_t expected_constant_buffer_space = 0;
  bool revert_state = false;
  bool use_compatibility_mode = false;
  reshade::api::format proxy_format = reshade::api::format::unknown;
  const float* shader_injection = nullptr;
  size_t shader_injection_size = 0;
  bool auto_device_flush = true;

  void Destroy(reshade::api::device* device) {
    pass.DestroyAll(device);
  }

  bool Render(
      reshade::api::swapchain* swapchain,
      reshade::api::command_queue* queue,
      const reshade::api::resource* swapchain_clone_override = nullptr) {
    auto* cmd_list = queue->get_immediate_command_list();
    auto current_back_buffer = swapchain->get_current_back_buffer();
    auto* device = swapchain->get_device();
    const bool is_vulkan = device->get_api() == reshade::api::device_api::vulkan;

#ifdef DEBUG_LEVEL_2
    {
      std::stringstream s;
      s << "utils::draw::SwapchainProxyPass::Render(";
      s << "bb=" << PRINT_PTR(current_back_buffer.handle);
      s << ", proxy_format=" << proxy_format;
      s << ", compat=" << (use_compatibility_mode ? "true" : "false");
      s << ", vs=" << vertex_shader.size();
      s << ", ps=" << pixel_shader.size();
      s << ")";
      reshade::log::message(reshade::log::level::info, s.str().c_str());
    }
#endif

    auto* resource_info = utils::resource::GetResourceInfoUnsafe(current_back_buffer);
    if (resource_info == nullptr) {
#ifdef DEBUG_LEVEL_2
      reshade::log::message(reshade::log::level::warning, "utils::draw::SwapchainProxyPass::Render(no resource_info)");
#endif
      return false;
    }

#ifdef DEBUG_LEVEL_2
    {
      std::stringstream s;
      s << "utils::draw::SwapchainProxyPass::Render(resource clone_enabled=" << (resource_info->clone_enabled ? "true" : "false");
      s << ", clone_target=" << PRINT_PTR(reinterpret_cast<uintptr_t>(resource_info->clone_target));
      if (resource_info->clone_target != nullptr) {
        s << ", clone_target_format=" << resource_info->clone_target->new_format;
      }
      s << ")";
      reshade::log::message(reshade::log::level::info, s.str().c_str());
    }
    {
      std::stringstream s;
      s << "utils::draw::SwapchainProxyPass::Render(resource handles";
      s << " bb=" << PRINT_PTR(current_back_buffer.handle);
      s << " res=" << PRINT_PTR(resource_info->resource.handle);
      s << " clone=" << PRINT_PTR(resource_info->clone.handle);
      s << " proxy_res=" << PRINT_PTR(resource_info->proxy_resource.handle);
      s << " proxy_clone_srv=" << PRINT_PTR(resource_info->swap_chain_proxy_clone_srv.handle);
      s << " proxy_rtv=" << PRINT_PTR(resource_info->swap_chain_proxy_rtv.handle);
      s << " is_swap_chain=" << (resource_info->is_swap_chain ? "true" : "false");
      s << " upgraded=" << (resource_info->upgraded ? "true" : "false");
      const auto desc_format = (resource_info->desc.type == reshade::api::resource_type::buffer)
                                   ? reshade::api::format::unknown
                                   : resource_info->desc.texture.format;
      const auto clone_desc_format = (resource_info->clone_desc.type == reshade::api::resource_type::buffer)
                                         ? reshade::api::format::unknown
                                         : resource_info->clone_desc.texture.format;
      s << " fmt=" << desc_format;
      s << " clone_fmt=" << clone_desc_format;
      s << ")";
      reshade::log::message(reshade::log::level::info, s.str().c_str());
    }
#endif

    reshade::api::resource swapchain_clone;

    if (swapchain_clone_override != nullptr && swapchain_clone_override->handle != 0u) {
      swapchain_clone = *swapchain_clone_override;
    } else if (use_compatibility_mode) {
      auto& resource_clone = resource_info->clone;
      if (resource_clone.handle == 0u) {
        renodx::utils::resource::upgrade::CloneResource(resource_info);
      }
      if (resource_clone.handle == 0u) {
#ifdef DEBUG_LEVEL_2
        reshade::log::message(reshade::log::level::warning, "utils::draw::SwapchainProxyPass::Render(no clone after CloneResource)");
#endif
        return false;
      }
      swapchain_clone = resource_clone;

#ifdef DEBUG_LEVEL_2
      {
        std::stringstream s;
        s << "utils::draw::SwapchainProxyPass::Render(copy_resource begin";
        s << " bb=" << PRINT_PTR(current_back_buffer.handle);
        s << " clone=" << PRINT_PTR(swapchain_clone.handle);
        s << ")";
        reshade::log::message(reshade::log::level::info, s.str().c_str());
      }
#endif
      if (is_vulkan) {
        const reshade::api::resource pre_copy_resources[] = {current_back_buffer, swapchain_clone};
        const reshade::api::resource_usage pre_copy_old_states[] = {
            reshade::api::resource_usage::present
                | reshade::api::resource_usage::render_target
                | reshade::api::resource_usage::general,
            reshade::api::resource_usage::general
                | reshade::api::resource_usage::shader_resource};
        const reshade::api::resource_usage pre_copy_new_states[] = {
            reshade::api::resource_usage::copy_source,
            reshade::api::resource_usage::copy_dest};
        cmd_list->barrier(
            2u,
            pre_copy_resources,
            pre_copy_old_states,
            pre_copy_new_states);
      }
      cmd_list->copy_resource(current_back_buffer, resource_info->clone);
      if (is_vulkan) {
        const reshade::api::resource post_copy_resources[] = {swapchain_clone, current_back_buffer};
        const reshade::api::resource_usage post_copy_old_states[] = {
            reshade::api::resource_usage::copy_dest,
            reshade::api::resource_usage::copy_source};
        const reshade::api::resource_usage post_copy_new_states[] = {
            reshade::api::resource_usage::shader_resource,
            reshade::api::resource_usage::render_target};
        cmd_list->barrier(
            2u,
            post_copy_resources,
            post_copy_old_states,
            post_copy_new_states);
      }
#ifdef DEBUG_LEVEL_2
      reshade::log::message(reshade::log::level::info, "utils::draw::SwapchainProxyPass::Render(copy_resource end)");
#endif
    } else {
      if (resource_info->clone.handle == 0u) {
#ifdef DEBUG_LEVEL_2
        reshade::log::message(reshade::log::level::warning, "utils::draw::SwapchainProxyPass::Render(no clone, compat=false)");
#endif
        return false;
      }
      swapchain_clone = resource_info->clone;
    }

#ifdef DEBUG_LEVEL_2
    {
      std::stringstream s;
      s << "utils::draw::SwapchainProxyPass::Render(clone=" << PRINT_PTR(swapchain_clone.handle) << ")";
      reshade::log::message(reshade::log::level::info, s.str().c_str());
    }
#endif

    auto& pass = this->pass;
    if (pass.render_target_slots.resources.empty() || pass.render_target_slots.resources[0] != current_back_buffer) {
      pass.InvalidateRenderTargets(cmd_list);
      pass.render_target_slots.resources = {current_back_buffer};
    }

    if (pass.shader_resource_slots.resources.empty() || pass.shader_resource_slots.resources[0] != swapchain_clone) {
      renodx::utils::render::RenderPass::DestroyGeneratedViews(cmd_list, &pass.shader_resource_slots.generated_views);
      pass.shader_resource_slots.views.clear();
      pass.shader_resource_slots.view_descs.clear();
      pass.shader_resource_slots.view_infos.clear();
      pass.shader_resource_slots.resources = {swapchain_clone};
      pass.shader_resource_slots.resource_descs.clear();
      pass.shader_resource_slots.resource_infos.clear();
      pass.auto_generate_descriptor_table_updates = true;
    }
#ifdef DEBUG_LEVEL_2
    {
      std::stringstream s;
      s << "utils::draw::SwapchainProxyPass::Render(bind resources";
      s << " rt=" << PRINT_PTR(pass.render_target_slots.resources[0].handle);
      s << " srv=" << PRINT_PTR(pass.shader_resource_slots.resources[0].handle);
      s << " proxy_format=" << proxy_format;
      s << ")";
      reshade::log::message(reshade::log::level::info, s.str().c_str());
    }
#endif
    pass.revert_state_after_render = revert_state;
    pass.pipeline_subobjects.vertex_shader = vertex_shader;
    pass.pipeline_subobjects.pixel_shader = pixel_shader;
    pass.pipeline_subobjects.compute_shader = {};

    if (pass.sampler_descs.empty()) {
      pass.sampler_descs.push_back(reshade::api::sampler_desc{});
    }
    pass.push_constants.clear();

    const bool is_modern_api = device->get_api() == reshade::api::device_api::d3d12
                               || device->get_api() == reshade::api::device_api::vulkan;
    if (shader_injection_size != 0u) {
      uint8_t register_index;
      if (expected_constant_buffer_index == -1) {
        register_index = is_modern_api ? 0 : 13;
      } else {
        register_index = static_cast<uint8_t>(expected_constant_buffer_index);
      }
      uint8_t register_space = is_modern_api
                                   ? static_cast<uint8_t>(expected_constant_buffer_space)
                                   : 0;
      const renodx::utils::render::ConstantBuffersSlots slot = {
          .slot = register_index,
          .space = register_space,
      };
      pass.push_constants[slot] = std::span<const float>(shader_injection, shader_injection_size);
    }

    if (auto_device_flush && !is_modern_api) {
      pass.flush_after_render = true;
    }

    if (!pass.Render(cmd_list, queue)) {
#ifdef DEBUG_LEVEL_2
      reshade::log::message(reshade::log::level::warning, "utils::draw::SwapchainProxyPass::Render(RenderPass::Render failed)");
#endif
      return false;
    }

    if (is_vulkan) {
      const reshade::api::resource final_resources[] = {current_back_buffer};
      const reshade::api::resource_usage final_old_states[] = {
          reshade::api::resource_usage::render_target
          | reshade::api::resource_usage::general};
      const reshade::api::resource_usage final_new_states[] = {
          reshade::api::resource_usage::present};
      cmd_list->barrier(
          1u,
          final_resources,
          final_old_states,
          final_new_states);
    }

    return true;
  }
};

}  // namespace renodx::utils::draw
