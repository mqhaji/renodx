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
#include <type_traits>
#include <utility>
#include <vector>

#include <windows.h>

#include <d3d11.h>
#include <dxgi.h>
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
#include "../../utils/command_action.hpp"
#include "../../utils/mutex.hpp"
#include "../../utils/random.hpp"
#include "../../utils/resource.hpp"
#include "../../utils/settings.hpp"
#include "../../utils/shader.hpp"
#include "../../utils/state.hpp"
#include "../../utils/swapchain.hpp"
#include "lighting/indirect/ambient_diffuse.hpp"
#include "lighting/indirect/environment_specular.hpp"
#include "lighting/indirect/runtime.hpp"
#include "lighting/indirect/source_preparation.hpp"
#include "lighting/shadows/screen_space_shadows.hpp"
#include "lighting/shadows/settings.hpp"
#include "materials/capture.hpp"
#include "output/output.hpp"
#include "postprocess/bloom/bloom.hpp"
#include "reconstruction/coordinator.hpp"
#include "reconstruction/input_capture.hpp"
#include "reconstruction/logging.hpp"
#include "reconstruction/postprocess/resources.hpp"
#include "reconstruction/postprocess/runtime.hpp"
#include "reconstruction/resolution.hpp"
#include "reconstruction/runtime.hpp"
#include "reconstruction/settings.hpp"
#include "rendering/inputs.hpp"
#include "rendering/native_context.hpp"
#include "rendering/producer_records.hpp"
#include "rendering/resource_upgrades.hpp"
#include "settings.hpp"
#include "shaders.hpp"
#include "shared.h"

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
  if (ImGui::TreeNode("Bend Screen-Space Shadows (Apache 2.0)")) {
    ImGui::PushTextWrapPos();
    ImGui::TextUnformatted(R"license(Copyright 2023 Sony Interactive Entertainment.
Adapted for MGSV and D3D11.

Apache License
                           Version 2.0, January 2004
                        http://www.apache.org/licenses/

   TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION

   1. Definitions.

      "License" shall mean the terms and conditions for use, reproduction,
      and distribution as defined by Sections 1 through 9 of this document.

      "Licensor" shall mean the copyright owner or entity authorized by
      the copyright owner that is granting the License.

      "Legal Entity" shall mean the union of the acting entity and all
      other entities that control, are controlled by, or are under common
      control with that entity. For the purposes of this definition,
      "control" means (i) the power, direct or indirect, to cause the
      direction or management of such entity, whether by contract or
      otherwise, or (ii) ownership of fifty percent (50%) or more of the
      outstanding shares, or (iii) beneficial ownership of such entity.

      "You" (or "Your") shall mean an individual or Legal Entity
      exercising permissions granted by this License.

      "Source" form shall mean the preferred form for making modifications,
      including but not limited to software source code, documentation
      source, and configuration files.

      "Object" form shall mean any form resulting from mechanical
      transformation or translation of a Source form, including but
      not limited to compiled object code, generated documentation,
      and conversions to other media types.

      "Work" shall mean the work of authorship, whether in Source or
      Object form, made available under the License, as indicated by a
      copyright notice that is included in or attached to the work
      (an example is provided in the Appendix below).

      "Derivative Works" shall mean any work, whether in Source or Object
      form, that is based on (or derived from) the Work and for which the
      editorial revisions, annotations, elaborations, or other modifications
      represent, as a whole, an original work of authorship. For the purposes
      of this License, Derivative Works shall not include works that remain
      separable from, or merely link (or bind by name) to the interfaces of,
      the Work and Derivative Works thereof.

      "Contribution" shall mean any work of authorship, including
      the original version of the Work and any modifications or additions
      to that Work or Derivative Works thereof, that is intentionally
      submitted to Licensor for inclusion in the Work by the copyright owner
      or by an individual or Legal Entity authorized to submit on behalf of
      the copyright owner. For the purposes of this definition, "submitted"
      means any form of electronic, verbal, or written communication sent
      to the Licensor or its representatives, including but not limited to
      communication on electronic mailing lists, source code control systems,
      and issue tracking systems that are managed by, or on behalf of, the
      Licensor for the purpose of discussing and improving the Work, but
      excluding communication that is conspicuously marked or otherwise
      designated in writing by the copyright owner as "Not a Contribution."

      "Contributor" shall mean Licensor and any individual or Legal Entity
      on behalf of whom a Contribution has been received by Licensor and
      subsequently incorporated within the Work.

   2. Grant of Copyright License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      copyright license to reproduce, prepare Derivative Works of,
      publicly display, publicly perform, sublicense, and distribute the
      Work and such Derivative Works in Source or Object form.

   3. Grant of Patent License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      (except as stated in this section) patent license to make, have made,
      use, offer to sell, sell, import, and otherwise transfer the Work,
      where such license applies only to those patent claims licensable
      by such Contributor that are necessarily infringed by their
      Contribution(s) alone or by combination of their Contribution(s)
      with the Work to which such Contribution(s) was submitted. If You
      institute patent litigation against any entity (including a
      cross-claim or counterclaim in a lawsuit) alleging that the Work
      or a Contribution incorporated within the Work constitutes direct
      or contributory patent infringement, then any patent licenses
      granted to You under this License for that Work shall terminate
      as of the date such litigation is filed.

   4. Redistribution. You may reproduce and distribute copies of the
      Work or Derivative Works thereof in any medium, with or without
      modifications, and in Source or Object form, provided that You
      meet the following conditions:

      (a) You must give any other recipients of the Work or
          Derivative Works a copy of this License; and

      (b) You must cause any modified files to carry prominent notices
          stating that You changed the files; and

      (c) You must retain, in the Source form of any Derivative Works
          that You distribute, all copyright, patent, trademark, and
          attribution notices from the Source form of the Work,
          excluding those notices that do not pertain to any part of
          the Derivative Works; and

      (d) If the Work includes a "NOTICE" text file as part of its
          distribution, then any Derivative Works that You distribute must
          include a readable copy of the attribution notices contained
          within such NOTICE file, excluding those notices that do not
          pertain to any part of the Derivative Works, in at least one
          of the following places: within a NOTICE text file distributed
          as part of the Derivative Works; within the Source form or
          documentation, if provided along with the Derivative Works; or,
          within a display generated by the Derivative Works, if and
          wherever such third-party notices normally appear. The contents
          of the NOTICE file are for informational purposes only and
          do not modify the License. You may add Your own attribution
          notices within Derivative Works that You distribute, alongside
          or as an addendum to the NOTICE text from the Work, provided
          that such additional attribution notices cannot be construed
          as modifying the License.

      You may add Your own copyright statement to Your modifications and
      may provide additional or different license terms and conditions
      for use, reproduction, or distribution of Your modifications, or
      for any such Derivative Works as a whole, provided Your use,
      reproduction, and distribution of the Work otherwise complies with
      the conditions stated in this License.

   5. Submission of Contributions. Unless You explicitly state otherwise,
      any Contribution intentionally submitted for inclusion in the Work
      by You to the Licensor shall be under the terms and conditions of
      this License, without any additional terms or conditions.
      Notwithstanding the above, nothing herein shall supersede or modify
      the terms of any separate license agreement you may have executed
      with Licensor regarding such Contributions.

   6. Trademarks. This License does not grant permission to use the trade
      names, trademarks, service marks, or product names of the Licensor,
      except as required for reasonable and customary use in describing the
      origin of the Work and reproducing the content of the NOTICE file.

   7. Disclaimer of Warranty. Unless required by applicable law or
      agreed to in writing, Licensor provides the Work (and each
      Contributor provides its Contributions) on an "AS IS" BASIS,
      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
      implied, including, without limitation, any warranties or conditions
      of TITLE, NON-INFRINGEMENT, MERCHANTABILITY, or FITNESS FOR A
      PARTICULAR PURPOSE. You are solely responsible for determining the
      appropriateness of using or redistributing the Work and assume any
      risks associated with Your exercise of permissions under this License.

   8. Limitation of Liability. In no event and under no legal theory,
      whether in tort (including negligence), contract, or otherwise,
      unless required by applicable law (such as deliberate and grossly
      negligent acts) or agreed to in writing, shall any Contributor be
      liable to You for damages, including any direct, indirect, special,
      incidental, or consequential damages of any character arising as a
      result of this License or out of the use or inability to use the
      Work (including but not limited to damages for loss of goodwill,
      work stoppage, computer failure or malfunction, or any and all
      other commercial damages or losses), even if such Contributor
      has been advised of the possibility of such damages.

   9. Accepting Warranty or Additional Liability. While redistributing
      the Work or Derivative Works thereof, You may choose to offer,
      and charge a fee for, acceptance of support, warranty, indemnity,
      or other liability obligations and/or rights consistent with this
      License. However, in accepting such obligations, You may act only
      on Your own behalf and on Your sole responsibility, not on behalf
      of any other Contributor, and only if You agree to indemnify,
      defend, and hold each Contributor harmless for any liability
      incurred by, or claims asserted against, such Contributor by reason
      of your accepting any such warranty or additional liability.

   END OF TERMS AND CONDITIONS

   APPENDIX: How to apply the Apache License to your work.

      To apply the Apache License to your work, attach the following
      boilerplate notice, with the fields enclosed by brackets "[]"
      replaced with your own identifying information. (Don't include
      the brackets!)  The text should be enclosed in the appropriate
      comment syntax for the file format. We also recommend that a
      file or class name and description of purpose be included on the
      same "printed page" as the copyright notice for easier
      identification within third-party archives.

   Copyright [yyyy] [name of copyright owner]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
)license");
    ImGui::PopTextWrapPos();
    ImGui::TreePop();
  }
  ImGui::TreePop();
};

void OnPresetOff() {
  mgsv::settings::ApplyPresetOff(&settings, &shader_injection, output_state);
}

void UpdateNativePresentation(reshade::api::swapchain* swapchain) {
  if (mgsv::rendering::native_context::Install()) {
    mgsv::rendering::native_context::SetPresentation(reinterpret_cast<IDXGISwapChain*>(swapchain->get_native()));  // NOLINT(performance-no-int-to-ptr)
    mgsv::reconstruction::logging::Info("native view ownership ready for selected presentation");
  } else {
    mgsv::reconstruction::logging::Warn("native view ownership unavailable: ", mgsv::rendering::native_context::installation_status);
    mgsv::reconstruction::resolution::Fail("native view ownership unavailable");
  }
}

void OnInitSwapchain(reshade::api::swapchain* swapchain, bool resize) {
  mgsv::inputs::OnInitSwapchain(swapchain, resize);
  mgsv::output::OnInitSwapchain(&output_state, swapchain);
  if (mgsv::inputs::IsSelectedSwapchain(swapchain)) {
    UpdateNativePresentation(swapchain);
  }
}

void OnDestroySwapchain(reshade::api::swapchain* swapchain, bool resize) {
  const mgsv::reconstruction::coordinator::ExecutionGuard execution_guard;
  if (!mgsv::inputs::IsSelectedSwapchain(swapchain)) return;
  mgsv::rendering::native_context::SetPresentation(nullptr);
  mgsv::inputs::OnDestroySwapchain(swapchain, resize);
  mgsv::lighting::environment_specular::Reset();
  mgsv::lighting::material_inputs::Reset();
  mgsv::lighting::source_preparation::Reset();
  mgsv::lighting::ambient_diffuse::Reset();
  mgsv::lighting::ResetNativeLightViews(swapchain->get_device());
  mgsv::reconstruction::coordinator::ResetTemporalState("input swapchain retired");
  mgsv::reconstruction::post_process::Reset();
  // Cancel reconstruction but retain every original game prefix/post list for its existing replay path.
  std::vector<std::array<Microsoft::WRL::ComPtr<ID3D11DeviceChild>, 5>> retired_resources;
  std::vector<std::function<void(reshade::api::command_list*, bool)>> retired_lighting;
  {
    const std::unique_lock submission_lock(mgsv::reconstruction::coordinator::submission.mutex);
    for (auto& pending : mgsv::reconstruction::coordinator::submission.pending_dispatches) {
      retired_resources.push_back({
          std::move(pending.color_srv),
          std::move(pending.velocity_srv),
          std::move(pending.depth_srv),
          std::move(pending.object_velocity_srv),
          std::move(pending.output_resource),
      });
      pending.inputs = {};
      if (pending.is_lighting) retired_lighting.push_back(std::move(pending.lighting_callback));
    }
  }
}

// Scheduling must precede the pipeline actions that redirect this same draw.
// Keeping both in command_action also preserves order when DevKit owns its hook.
constexpr auto RECONSTRUCTION_DRAW = [](auto& command) {
  using Context = std::remove_reference_t<decltype(command)>;
  if (command.IsDispatch()) return renodx::utils::command_action::CallbackResult<Context>{};
  return renodx::utils::command_action::CallbackResult<Context>{
      .bypass = mgsv::game::reconstruction::HandleDraw(command.cmd_list),
  };
};

void OnPresent(
    reshade::api::command_queue* queue,
    reshade::api::swapchain* swapchain,
    const reshade::api::rect* source_rect,
    const reshade::api::rect* dest_rect,
    uint32_t dirty_rect_count,
    const reshade::api::rect* dirty_rects) {
  bool temporal_lighting_enabled = false;
  bool lighting_enabled = false;
  bool selected_present = false;
  uint32_t shadow_noise_frame = 0u;
  {
    const std::shared_lock settings_lock(renodx::utils::mutex::global_mutex);
    lighting_enabled = shader_injection.indirect_lighting_enabled >= 0.5f;
    temporal_lighting_enabled = lighting_enabled && shader_injection.lighting_temporal_enabled >= 0.5f;
  }
  {
    const mgsv::reconstruction::coordinator::ExecutionGuard execution_guard;
    const bool was_selected = mgsv::inputs::IsSelectedSwapchain(swapchain);
    mgsv::inputs::OnPresent(queue, swapchain, source_rect, dest_rect, dirty_rect_count, dirty_rects);
    if (mgsv::inputs::IsSelectedSwapchain(swapchain)) {
      // Initial creation precedes selection. Join the ownership provider when
      // the first Present selects this swapchain, and refresh it after resize.
      if (!was_selected) {
        UpdateNativePresentation(swapchain);
      }
      if (!lighting_enabled) {
        mgsv::lighting::environment_specular::Reset();
        mgsv::lighting::material_inputs::Reset();
        mgsv::lighting::source_preparation::Reset();
        mgsv::lighting::ambient_diffuse::Reset();
        mgsv::lighting::ResetNativeLightViews(queue->get_device());
      }
      mgsv::lighting::temporal_inputs::enabled.store(temporal_lighting_enabled, std::memory_order_release);
      shadow_noise_frame = mgsv::lighting::noise_frame.fetch_add(1u, std::memory_order_relaxed) + 1u;
      selected_present = true;
    }
  }
  if (selected_present) {
    const std::unique_lock settings_lock(renodx::utils::mutex::global_mutex);
    shader_injection.shadow_noise_frame = static_cast<float>(shadow_noise_frame & 31u);
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
  mgsv::rendering::native_context::Shutdown();
  mgsv::inputs::OnDestroyDevice(device);
  mgsv::lighting::environment_specular::Reset();
  mgsv::lighting::material_inputs::Reset();
  mgsv::lighting::source_preparation::Reset();
  mgsv::lighting::ambient_diffuse::Reset();
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
      mgsv::bloom::AppendSettings(&settings, &shader_injection);
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
            "Shadows",
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
      mgsv::rendering::native_context::Shutdown();
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
    reshade::register_event<reshade::addon_event::init_device>(mgsv::shadows::screen_space::OnInitDevice);
    reshade::register_event<reshade::addon_event::destroy_device>(mgsv::shadows::screen_space::OnDestroyDevice);
    reshade::register_event<reshade::addon_event::init_command_list>(mgsv::shadows::screen_space::OnInitCommandList);
    reshade::register_event<reshade::addon_event::reset_command_list>(mgsv::shadows::screen_space::OnResetCommandList);
    reshade::register_event<reshade::addon_event::destroy_command_list>(mgsv::shadows::screen_space::OnDestroyCommandList);
    reshade::register_event<reshade::addon_event::init_device>(mgsv::lighting::environment_specular::OnInitDevice);
    reshade::register_event<reshade::addon_event::init_device>(mgsv::lighting::material_inputs::OnInitDevice);
    reshade::register_event<reshade::addon_event::destroy_device>(mgsv::lighting::environment_specular::OnDestroyDevice);
    reshade::register_event<reshade::addon_event::destroy_device>(mgsv::lighting::material_inputs::OnDestroyDevice);
    reshade::register_event<reshade::addon_event::init_command_list>(mgsv::inputs::descriptor_tracker::OnInitCommandList);
    reshade::register_event<reshade::addon_event::init_command_list>(mgsv::rendering::resource_upgrades::OnInitBloomCommandList);
    reshade::register_event<reshade::addon_event::init_command_list>(mgsv::lighting::environment_specular::OnInitCommandList);
    reshade::register_event<reshade::addon_event::init_command_list>(mgsv::lighting::material_inputs::OnInitCommandList);
    reshade::register_event<reshade::addon_event::init_command_list>(mgsv::lighting::source_preparation::OnInitCommandList);
    reshade::register_event<reshade::addon_event::reset_command_list>(mgsv::lighting::environment_specular::OnResetCommandList);
    reshade::register_event<reshade::addon_event::reset_command_list>(mgsv::lighting::material_inputs::OnResetCommandList);
    reshade::register_event<reshade::addon_event::reset_command_list>(mgsv::lighting::source_preparation::OnResetCommandList);
    reshade::register_event<reshade::addon_event::destroy_command_list>(mgsv::lighting::environment_specular::OnDestroyCommandList);
    reshade::register_event<reshade::addon_event::destroy_command_list>(mgsv::lighting::material_inputs::OnDestroyCommandList);
    reshade::register_event<reshade::addon_event::destroy_command_list>(mgsv::lighting::source_preparation::OnDestroyCommandList);
    reshade::register_event<reshade::addon_event::clear_render_target_view>(mgsv::lighting::environment_specular::OnClearRenderTarget);
    reshade::register_event<reshade::addon_event::clear_render_target_view>(mgsv::lighting::material_inputs::OnClearRenderTarget);
    reshade::register_event<reshade::addon_event::clear_render_target_view>(mgsv::shadows::screen_space::OnClearRenderTarget);
    reshade::register_event<reshade::addon_event::destroy_resource>(mgsv::lighting::environment_specular::OnDestroyResource);
    reshade::register_event<reshade::addon_event::destroy_resource>(mgsv::lighting::material_inputs::OnDestroyResource);
    reshade::register_event<reshade::addon_event::destroy_resource>(mgsv::lighting::source_preparation::OnDestroyResource);
    reshade::register_event<reshade::addon_event::destroy_resource>(mgsv::lighting::ambient_diffuse::OnDestroyResource);
    reshade::register_event<reshade::addon_event::clear_render_target_view>(mgsv::lighting::ambient_diffuse::OnClearRenderTarget);
    reshade::register_event<reshade::addon_event::destroy_command_list>(mgsv::inputs::descriptor_tracker::OnDestroyCommandList);
    reshade::register_event<reshade::addon_event::destroy_command_list>(mgsv::rendering::resource_upgrades::OnDestroyBloomCommandList);
    reshade::register_event<reshade::addon_event::reset_command_list>(mgsv::inputs::descriptor_tracker::OnResetCommandList);
    reshade::register_event<reshade::addon_event::reset_command_list>(mgsv::rendering::resource_upgrades::OnResetBloomCommandList);
    reshade::register_event<reshade::addon_event::push_descriptors>(mgsv::inputs::descriptor_tracker::OnPushDescriptors);
  } else if (fdw_reason == DLL_PROCESS_DETACH) {
    reshade::unregister_event<reshade::addon_event::init_device>(mgsv::shadows::screen_space::OnInitDevice);
    reshade::unregister_event<reshade::addon_event::destroy_device>(mgsv::shadows::screen_space::OnDestroyDevice);
    reshade::unregister_event<reshade::addon_event::init_command_list>(mgsv::shadows::screen_space::OnInitCommandList);
    reshade::unregister_event<reshade::addon_event::reset_command_list>(mgsv::shadows::screen_space::OnResetCommandList);
    reshade::unregister_event<reshade::addon_event::destroy_command_list>(mgsv::shadows::screen_space::OnDestroyCommandList);
    reshade::unregister_event<reshade::addon_event::init_device>(mgsv::lighting::environment_specular::OnInitDevice);
    reshade::unregister_event<reshade::addon_event::init_device>(mgsv::lighting::material_inputs::OnInitDevice);
    reshade::unregister_event<reshade::addon_event::destroy_device>(mgsv::lighting::environment_specular::OnDestroyDevice);
    reshade::unregister_event<reshade::addon_event::destroy_device>(mgsv::lighting::material_inputs::OnDestroyDevice);
    reshade::unregister_event<reshade::addon_event::init_command_list>(mgsv::inputs::descriptor_tracker::OnInitCommandList);
    reshade::unregister_event<reshade::addon_event::init_command_list>(mgsv::rendering::resource_upgrades::OnInitBloomCommandList);
    reshade::unregister_event<reshade::addon_event::init_command_list>(mgsv::lighting::environment_specular::OnInitCommandList);
    reshade::unregister_event<reshade::addon_event::init_command_list>(mgsv::lighting::material_inputs::OnInitCommandList);
    reshade::unregister_event<reshade::addon_event::init_command_list>(mgsv::lighting::source_preparation::OnInitCommandList);
    reshade::unregister_event<reshade::addon_event::reset_command_list>(mgsv::lighting::environment_specular::OnResetCommandList);
    reshade::unregister_event<reshade::addon_event::reset_command_list>(mgsv::lighting::material_inputs::OnResetCommandList);
    reshade::unregister_event<reshade::addon_event::reset_command_list>(mgsv::lighting::source_preparation::OnResetCommandList);
    reshade::unregister_event<reshade::addon_event::destroy_command_list>(mgsv::lighting::environment_specular::OnDestroyCommandList);
    reshade::unregister_event<reshade::addon_event::destroy_command_list>(mgsv::lighting::material_inputs::OnDestroyCommandList);
    reshade::unregister_event<reshade::addon_event::destroy_command_list>(mgsv::lighting::source_preparation::OnDestroyCommandList);
    reshade::unregister_event<reshade::addon_event::clear_render_target_view>(mgsv::lighting::environment_specular::OnClearRenderTarget);
    reshade::unregister_event<reshade::addon_event::clear_render_target_view>(mgsv::lighting::material_inputs::OnClearRenderTarget);
    reshade::unregister_event<reshade::addon_event::clear_render_target_view>(mgsv::shadows::screen_space::OnClearRenderTarget);
    reshade::unregister_event<reshade::addon_event::destroy_resource>(mgsv::lighting::environment_specular::OnDestroyResource);
    reshade::unregister_event<reshade::addon_event::destroy_resource>(mgsv::lighting::material_inputs::OnDestroyResource);
    reshade::unregister_event<reshade::addon_event::destroy_resource>(mgsv::lighting::source_preparation::OnDestroyResource);
    reshade::unregister_event<reshade::addon_event::destroy_resource>(mgsv::lighting::ambient_diffuse::OnDestroyResource);
    reshade::unregister_event<reshade::addon_event::clear_render_target_view>(mgsv::lighting::ambient_diffuse::OnClearRenderTarget);
    reshade::unregister_event<reshade::addon_event::destroy_command_list>(mgsv::inputs::descriptor_tracker::OnDestroyCommandList);
    reshade::unregister_event<reshade::addon_event::destroy_command_list>(mgsv::rendering::resource_upgrades::OnDestroyBloomCommandList);
    reshade::unregister_event<reshade::addon_event::reset_command_list>(mgsv::inputs::descriptor_tracker::OnResetCommandList);
    reshade::unregister_event<reshade::addon_event::reset_command_list>(mgsv::rendering::resource_upgrades::OnResetBloomCommandList);
    reshade::unregister_event<reshade::addon_event::push_descriptors>(mgsv::inputs::descriptor_tracker::OnPushDescriptors);
  }
  mgsv::reconstruction::Use(fdw_reason, reconstruction_settings);

  renodx::utils::random::Use(fdw_reason, {&shader_injection.film_grain_random});

  renodx::mods::swapchain::v2::Use(fdw_reason, &shader_injection);

  if (fdw_reason == DLL_PROCESS_ATTACH) {
    // Register before the shared dispatcher selects its draw events. Its global
    // callback publishes reconstruction output before per-shader scene routing.
    renodx::utils::command_action::Register(RECONSTRUCTION_DRAW,
                                            {.command_types = renodx::utils::command_action::COMMAND_TYPE_DIRECT_DRAW | renodx::utils::command_action::COMMAND_TYPE_INDIRECT});
  } else if (fdw_reason == DLL_PROCESS_DETACH) {
    mgsv::reconstruction::post_process::Use(fdw_reason);
    renodx::utils::command_action::Unregister(RECONSTRUCTION_DRAW);
  }
  renodx::mods::shader::Use(fdw_reason, custom_shaders, &shader_injection);
  if (fdw_reason == DLL_PROCESS_ATTACH) {
    mgsv::reconstruction::post_process::Use(fdw_reason);
  }

  return TRUE;
}
