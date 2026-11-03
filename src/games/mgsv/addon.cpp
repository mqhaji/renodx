/*
 * Copyright (C) 2026 Musa Haji
 * Copyright (C) 2026 ZioPao
 * SPDX-License-Identifier: MIT
 */

#define ImTextureID                   ImU64
#define RENODX_MODS_SWAPCHAIN_VERSION 2
// #define DEBUG_LEVEL_0
// #define DEBUG_LEVEL_1
// #define DEBUG_LEVEL_2
// #define DEBUG_LEVEL_3

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

#include <windows.h>

#include <d3d11.h>
#include <wrl/client.h>

#include <deps/imgui/imgui.h>
#include <include/reshade.hpp>
#include <include/reshade_api.hpp>
#include <include/reshade_api_device.hpp>
#include <include/reshade_api_format.hpp>
#include <include/reshade_api_pipeline.hpp>
#include <include/reshade_api_resource.hpp>
#include <include/reshade_events.hpp>

#include "../../mods/shader.hpp"
#include "../../mods/swapchain_v2.hpp"
#include "../../utils/mutex.hpp"
#include "../../utils/random.hpp"
#include "../../utils/resource.hpp"
#include "../../utils/settings.hpp"
#include "../../utils/shader.hpp"
#include "../../utils/state.hpp"
#include "../../utils/swapchain.hpp"
#include "./gtvbgi/environment_specular.hpp"
#include "./gtvbgi/lighting.hpp"
#include "./gtvbgi/temporal_inputs.hpp"
#include "./inputs.hpp"
#include "./output/output.hpp"
#include "./reconstruction/coordinator.hpp"
#include "./reconstruction/input_capture.hpp"
#include "./reconstruction/runtime.hpp"
#include "./reconstruction/settings.hpp"
#include "./resource_upgrades.hpp"
#include "./settings.hpp"
#include "./shaders.hpp"
#include "./shadows/settings.hpp"
#include "./shared.h"

namespace {

// These objects outlive all settings and shader callbacks that borrow them.
ShaderInjectData shader_injection = {};
mgsv::output::State output_state;
mgsv::reconstruction::settings::State reconstruction_settings;
renodx::utils::settings::Settings settings;
renodx::mods::shader::CustomShaders custom_shaders = mgsv::game::CreateShaders(&shader_injection);

constexpr auto ADDON_SETTINGS = [](reshade::api::effect_runtime* /*runtime*/) {
  if (!ImGui::TreeNode("Third-Party Licenses")) return;
  if (ImGui::TreeNode("EA FAST Noise (BSD-3-Clause)")) {
    ImGui::PushTextWrapPos();
    ImGui::TextUnformatted(R"license(Copyright (c) 2023 Electronic Arts Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1.  Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
3.  Neither the name of Electronic Arts, Inc. ("EA") nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.)license");
    ImGui::PopTextWrapPos();
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("GT-VBGI (MIT)")) {
    ImGui::PushTextWrapPos();
    ImGui::TextUnformatted(R"license(GT-VBGI reference: Mirko Salm (TinyTexel).
https://www.shadertoy.com/view/lfdBWn

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.)license");
    ImGui::PopTextWrapPos();
    ImGui::TreePop();
  }
  ImGui::TreePop();
};

void OnPresetOff() {
  mgsv::settings::ApplyPresetOff(&settings, &shader_injection, output_state);
}

void OnInitSwapchain(reshade::api::swapchain* swapchain, bool resize) {
  mgsv::inputs::OnInitSwapchain(swapchain, resize);
  mgsv::output::OnInitSwapchain(&output_state, swapchain);
}

void OnDestroySwapchain(reshade::api::swapchain* swapchain, bool resize) {
  const mgsv::reconstruction::coordinator::ExecutionGuard execution_guard;
  if (!mgsv::inputs::IsSelectedSwapchain(swapchain)) return;
  mgsv::inputs::OnDestroySwapchain(swapchain, resize);
  mgsv::lighting::environment_specular::Reset();
  mgsv::reconstruction::coordinator::ResetTemporalState("input swapchain retired");
  // Cancel reconstruction but retain every original game prefix/post list for its existing replay path.
  std::vector<std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 4>> retired_views;
  std::vector<std::function<void(reshade::api::command_list*, bool)>> retired_lighting;
  {
    const std::unique_lock submission_lock(mgsv::reconstruction::coordinator::submission.mutex);
    for (auto& pending : mgsv::reconstruction::coordinator::submission.pending_dispatches) {
      retired_views.push_back({
          std::move(pending.color_srv),
          std::move(pending.velocity_srv),
          std::move(pending.depth_srv),
          std::move(pending.object_velocity_srv),
      });
      pending.inputs = {};
      if (pending.is_lighting) retired_lighting.push_back(std::move(pending.lighting_callback));
    }
  }
}

bool OnDraw(reshade::api::command_list* cmd_list, uint32_t vertex_count, uint32_t instance_count,
            uint32_t first_vertex, uint32_t first_instance) {
  (void)vertex_count;
  (void)instance_count;
  (void)first_vertex;
  (void)first_instance;
  return mgsv::game::reconstruction::HandleDraw(cmd_list, &shader_injection);
}

bool OnDrawIndexed(reshade::api::command_list* cmd_list, uint32_t index_count, uint32_t instance_count,
                   uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) {
  (void)index_count;
  (void)instance_count;
  (void)first_index;
  (void)vertex_offset;
  (void)first_instance;
  return mgsv::game::reconstruction::HandleDraw(cmd_list, &shader_injection);
}

bool OnDrawOrDispatchIndirect(reshade::api::command_list* cmd_list, reshade::api::indirect_command type,
                              reshade::api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride) {
  (void)type;
  (void)buffer;
  (void)offset;
  (void)draw_count;
  (void)stride;
  return mgsv::game::reconstruction::HandleDraw(cmd_list, &shader_injection);
}

void OnPresent(
    reshade::api::command_queue* queue,
    reshade::api::swapchain* swapchain,
    const reshade::api::rect* source_rect,
    const reshade::api::rect* dest_rect,
    uint32_t dirty_rect_count,
    const reshade::api::rect* dirty_rects) {
  bool temporal_lighting_enabled = false;
  bool lighting_enabled = false;
  {
    const std::shared_lock settings_lock(renodx::utils::mutex::global_mutex);
    lighting_enabled = shader_injection.gt_enabled >= 0.5f;
    temporal_lighting_enabled = lighting_enabled && shader_injection.gt_temporal_enabled >= 0.5f;
  }
  {
    const mgsv::reconstruction::coordinator::ExecutionGuard execution_guard;
    mgsv::inputs::OnPresent(queue, swapchain, source_rect, dest_rect, dirty_rect_count, dirty_rects);
    if (mgsv::inputs::IsSelectedSwapchain(swapchain)) {
      if (!lighting_enabled) {
        mgsv::lighting::environment_specular::Reset();
      }
      mgsv::lighting::temporal_inputs::enabled.store(temporal_lighting_enabled, std::memory_order_release);
      mgsv::lighting::temporal_inputs::BeginFrame(queue->get_device());
      mgsv::lighting::noise_frame.fetch_add(1u, std::memory_order_relaxed);
    }
  }
  (void)source_rect;
  (void)dest_rect;
  (void)dirty_rect_count;
  (void)dirty_rects;

  mgsv::output::SyncOutputMode(&shader_injection, output_state);
  const auto output_color_space = (mgsv::output::IsHDROutputSelected(output_state)
                                       ? reshade::api::color_space::hdr10_st2084
                                       : reshade::api::color_space::srgb_nonlinear);
  if (swapchain->get_color_space() != output_color_space) {
    renodx::utils::swapchain::ChangeColorSpace(swapchain, output_color_space);
  }

  mgsv::rendering::resource_upgrades::DeactivateDofFinalCopyRenderBufferTarget();
  mgsv::rendering::resource_upgrades::ResetTonemapCopyTracking();
}

void OnDestroyDevice(reshade::api::device* device) {
  const mgsv::reconstruction::coordinator::ExecutionGuard execution_guard;
  if (!mgsv::inputs::IsSelectedDevice(device)) return;
  mgsv::inputs::OnDestroyDevice(device);
  mgsv::lighting::environment_specular::Reset();
  mgsv::reconstruction::input_capture::InvalidateCapturedFrame();
  mgsv::ao::Destroy(device);
  mgsv::il::Destroy(device);
  mgsv::lighting::DestroyLightPassLayout(device);
  mgsv::lighting::DestroyFallbackViews(device);
  mgsv::rendering::resource_upgrades::dof_copy_state = {};
  mgsv::rendering::resource_upgrades::motion_blur_copy_state = {};
  mgsv::rendering::resource_upgrades::tonemap_copy_state = {};
}

bool initialized = false;

}  // namespace

extern "C" __declspec(dllexport) constexpr const char* NAME = "RenoDX";
extern "C" __declspec(dllexport) constexpr const char* DESCRIPTION = "RenoDX for METAL GEAR SOLID V: THE PHANTOM PAIN";

BOOL APIENTRY DllMain(HMODULE h_module, DWORD fdw_reason, LPVOID lpv_reserved) {
  switch (fdw_reason) {
    case DLL_PROCESS_ATTACH:
      if (!reshade::register_addon(h_module)) return FALSE;
      reshade::register_overlay(nullptr, ADDON_SETTINGS);

      mgsv::settings::AppendSettings(&settings, &shader_injection, &output_state);
      mgsv::shadows::AppendSettings(&settings, &shader_injection);
      mgsv::reconstruction::settings::AppendSettings(&settings, &shader_injection, &reconstruction_settings);
      mgsv::lighting::AppendSettings(&settings, &shader_injection);

      if (!initialized) {
        renodx::mods::swapchain::v2::force_borderless = true;
        renodx::mods::swapchain::v2::prevent_full_screen = true;

        renodx::mods::swapchain::v2::SetUseHDR10();
        renodx::mods::swapchain::v2::set_color_space = false;

        renodx::utils::settings::on_preset_changed_callbacks.emplace_back(
            [] { mgsv::reconstruction::transitions::ApplySettingsSnapshot(reconstruction_settings); });

        // renodx::mods::shader::force_pipeline_cloning = true;

        renodx::mods::shader::expected_constant_buffer_index = 13;
        renodx::mods::shader::minimum_constant_buffer_stages =
            reshade::api::shader_stage::vertex
            | reshade::api::shader_stage::pixel
            | reshade::api::shader_stage::compute;
        renodx::mods::swapchain::v2::expected_constant_buffer_index = 13;

        renodx::mods::swapchain::v2::use_resource_cloning = true;

        renodx::mods::swapchain::v2::resource_upgrade_infos.push_back(mgsv::rendering::resource_upgrades::lut_builder_upgrade_info);

        // Keep the tuning and diagnostic groups out of the way on first open.
        // Every other section stays expanded, so only the "(Advanced)" groups start collapsed.
        renodx::utils::settings::open_sections_by_default = false;
        renodx::utils::settings::default_open_sections = {
            "Temporal Anti-Aliasing",
            "Presets",
            "Output",
            "Tone Mapping",
            "Color Grading",
            "Effects",
            "Contact-Hardening Shadows",
            "Screen-Space Shadows",
            "Indirect Lighting",
            "Options",
            "Links",
            "About",
        };

        initialized = true;
      }

      reshade::register_event<reshade::addon_event::copy_resource>(mgsv::rendering::resource_upgrades::OnCopyTonemapOutputResource);
      reshade::register_event<reshade::addon_event::init_swapchain>(OnInitSwapchain);  // detect peak brightness
      reshade::register_event<reshade::addon_event::destroy_swapchain>(OnDestroySwapchain);
      reshade::register_event<reshade::addon_event::present>(OnPresent);
      reshade::register_event<reshade::addon_event::destroy_device>(OnDestroyDevice);

      break;
    case DLL_PROCESS_DETACH:
      reshade::unregister_overlay(nullptr, ADDON_SETTINGS);
      reshade::unregister_event<reshade::addon_event::copy_resource>(mgsv::rendering::resource_upgrades::OnCopyTonemapOutputResource);
      reshade::unregister_event<reshade::addon_event::init_swapchain>(OnInitSwapchain);
      reshade::unregister_event<reshade::addon_event::destroy_swapchain>(OnDestroySwapchain);
      reshade::unregister_event<reshade::addon_event::present>(OnPresent);
      reshade::unregister_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
      renodx::mods::swapchain::v2::set_color_space = true;
      reshade::unregister_addon(h_module);
      break;
  }
  // Load the persisted temporal mode before the runtime reads it so shaders
  // and native reconstruction cannot disagree during the first frame.
  renodx::utils::settings::Use(fdw_reason, &settings, &OnPresetOff);
  if (fdw_reason == DLL_PROCESS_ATTACH) {
    mgsv::output::SyncOutputMode(&shader_injection, output_state);
  }

  if (fdw_reason == DLL_PROCESS_ATTACH) {
    renodx::utils::state::use_snapshot = true;
  }
  renodx::utils::resource::Use(fdw_reason);
  renodx::utils::shader::Use(fdw_reason);
  renodx::utils::state::Use(fdw_reason);
  if (fdw_reason == DLL_PROCESS_ATTACH) {
    reshade::register_event<reshade::addon_event::init_device>(mgsv::lighting::environment_specular::OnInitDevice);
    reshade::register_event<reshade::addon_event::destroy_device>(mgsv::lighting::environment_specular::OnDestroyDevice);
    reshade::register_event<reshade::addon_event::init_command_list>(mgsv::inputs::descriptor_tracker::OnInitCommandList);
    reshade::register_event<reshade::addon_event::init_command_list>(mgsv::lighting::environment_specular::OnInitCommandList);
    reshade::register_event<reshade::addon_event::reset_command_list>(mgsv::lighting::environment_specular::OnResetCommandList);
    reshade::register_event<reshade::addon_event::destroy_command_list>(mgsv::lighting::environment_specular::OnDestroyCommandList);
    reshade::register_event<reshade::addon_event::clear_render_target_view>(mgsv::lighting::environment_specular::OnClearRenderTarget);
    reshade::register_event<reshade::addon_event::destroy_resource>(mgsv::lighting::environment_specular::OnDestroyResource);
    reshade::register_event<reshade::addon_event::destroy_command_list>(mgsv::inputs::descriptor_tracker::OnDestroyCommandList);
    reshade::register_event<reshade::addon_event::reset_command_list>(mgsv::inputs::descriptor_tracker::OnResetCommandList);
    reshade::register_event<reshade::addon_event::push_descriptors>(mgsv::inputs::descriptor_tracker::OnPushDescriptors);
    reshade::register_event<reshade::addon_event::draw>(OnDraw);
    reshade::register_event<reshade::addon_event::draw_indexed>(OnDrawIndexed);
    reshade::register_event<reshade::addon_event::draw_or_dispatch_indirect>(OnDrawOrDispatchIndirect);
  } else if (fdw_reason == DLL_PROCESS_DETACH) {
    reshade::unregister_event<reshade::addon_event::init_device>(mgsv::lighting::environment_specular::OnInitDevice);
    reshade::unregister_event<reshade::addon_event::destroy_device>(mgsv::lighting::environment_specular::OnDestroyDevice);
    reshade::unregister_event<reshade::addon_event::init_command_list>(mgsv::inputs::descriptor_tracker::OnInitCommandList);
    reshade::unregister_event<reshade::addon_event::init_command_list>(mgsv::lighting::environment_specular::OnInitCommandList);
    reshade::unregister_event<reshade::addon_event::reset_command_list>(mgsv::lighting::environment_specular::OnResetCommandList);
    reshade::unregister_event<reshade::addon_event::destroy_command_list>(mgsv::lighting::environment_specular::OnDestroyCommandList);
    reshade::unregister_event<reshade::addon_event::clear_render_target_view>(mgsv::lighting::environment_specular::OnClearRenderTarget);
    reshade::unregister_event<reshade::addon_event::destroy_resource>(mgsv::lighting::environment_specular::OnDestroyResource);
    reshade::unregister_event<reshade::addon_event::destroy_command_list>(mgsv::inputs::descriptor_tracker::OnDestroyCommandList);
    reshade::unregister_event<reshade::addon_event::reset_command_list>(mgsv::inputs::descriptor_tracker::OnResetCommandList);
    reshade::unregister_event<reshade::addon_event::push_descriptors>(mgsv::inputs::descriptor_tracker::OnPushDescriptors);
    reshade::unregister_event<reshade::addon_event::draw>(OnDraw);
    reshade::unregister_event<reshade::addon_event::draw_indexed>(OnDrawIndexed);
    reshade::unregister_event<reshade::addon_event::draw_or_dispatch_indirect>(OnDrawOrDispatchIndirect);
  }
  mgsv::reconstruction::Use(fdw_reason, reconstruction_settings);

  renodx::utils::random::Use(fdw_reason, {&shader_injection.custom_random});

  renodx::mods::swapchain::v2::Use(fdw_reason, &shader_injection);

  renodx::mods::shader::Use(fdw_reason, custom_shaders, &shader_injection);

  return TRUE;
}
