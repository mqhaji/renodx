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

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>

#include <windows.h>

#include <dxgi.h>

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
#include "engine/build.hpp"
#include "engine/image.hpp"
#include "engine/local_lights.hpp"
#include "engine/motion_vectors.hpp"
#include "engine/post_filter.hpp"
#include "engine/render_targets.hpp"
#include "engine/views.hpp"
#include "lighting/environment_specular.hpp"
#include "lighting/indirect/ambient_diffuse.hpp"
#include "lighting/indirect/runtime.hpp"
#include "lighting/indirect/source_preparation.hpp"
#include "lighting/indirect_lighting_mode.hpp"
#include "lighting/reflections/native_chain.hpp"
#include "lighting/reflections/runtime.hpp"
#include "lighting/shadows/screen_space_shadows.hpp"
#include "lighting/shadows/settings.hpp"
#include "materials/capture.hpp"
#include "output/output.hpp"
#include "postprocess/bloom/bloom.hpp"
#include "reconstruction/d3d12.hpp"
#include "reconstruction/dispatch.hpp"
#include "reconstruction/input_capture.hpp"
#include "reconstruction/logging.hpp"
#include "reconstruction/output_size/resources.hpp"
#include "reconstruction/output_size/runtime.hpp"
#include "reconstruction/projection_jitter.hpp"
#include "reconstruction/resolution.hpp"
#include "reconstruction/runtime.hpp"
#include "reconstruction/settings.hpp"
#include "reconstruction/state.hpp"
#include "rendering/inputs.hpp"
#include "rendering/probe_controls.hpp"
#include "rendering/producer_records.hpp"
#include "rendering/submission.hpp"
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
  if (ImGui::TreeNode("NVIDIA Real-Time Denoisers (NRD)")) {
    ImGui::PushTextWrapPos();
    ImGui::TextUnformatted(
        "NRD 4.18.0. Copyright (c) 2022 NVIDIA CORPORATION.\n"
        "Distributed under the NVIDIA RTX SDK license. See NRD_LICENSE.txt supplied with NRD.dll.\n"
        "https://github.com/NVIDIA-RTX/NRD/blob/v4.18.0/LICENSE.txt");
    ImGui::PopTextWrapPos();
    ImGui::TreePop();
  }
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

// View tracking needs the selected swapchain to tell which view is presented, so
// it and the hooks that read its scopes install once a swapchain is selected.
void UpdateNativePresentation(reshade::api::swapchain* swapchain) {
  if (mgsv::engine::views::Install()) {
    mgsv::engine::views::SetPresentation(reinterpret_cast<IDXGISwapChain*>(swapchain->get_native()));  // NOLINT(performance-no-int-to-ptr)
    mgsv::engine::post_filter::Install(mgsv::reconstruction::OnPostFilterStart);
    mgsv::engine::motion_vectors::Install(mgsv::reconstruction::OnCameraVelocityRecorded);
    mgsv::reconstruction::logging::Info("native view ownership ready for selected presentation");
  } else {
    mgsv::reconstruction::logging::Warn("native view ownership unavailable: ", mgsv::engine::views::installation_status);
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
  const mgsv::rendering::submission::ExecutionGuard execution_guard;
  if (!mgsv::inputs::IsSelectedSwapchain(swapchain)) return;
  mgsv::engine::views::SetPresentation(nullptr);
  mgsv::inputs::OnDestroySwapchain(swapchain, resize);
  mgsv::lighting::environment_specular::Reset();
  mgsv::lighting::material_inputs::Reset();
  mgsv::lighting::source_preparation::Reset();
  mgsv::lighting::ambient_diffuse::Reset();
  mgsv::lighting::ResetNativeLightViews(swapchain->get_device());
  mgsv::reconstruction::dispatch::ResetTemporalState("input swapchain retired");
  mgsv::reconstruction::output_size::Reset();
  mgsv::rendering::submission::CancelJobs();
}

void OnPresent(
    reshade::api::command_queue* queue,
    reshade::api::swapchain* swapchain,
    const reshade::api::rect* source_rect,
    const reshade::api::rect* dest_rect,
    uint32_t dirty_rect_count,
    const reshade::api::rect* dirty_rects) {
  bool lighting_enabled = false;
  bool selected_present = false;
  uint32_t shadow_noise_frame = 0u;
  {
    const std::shared_lock settings_lock(renodx::utils::mutex::global_mutex);
    // Enhanced indirect lighting needs the RenoDX tone mapper, and its controls say so. This is the
    // host side of CUSTOM_MGSV_ENHANCED_INDIRECT_LIGHTING in shared.h.
    lighting_enabled = shader_injection.indirect_lighting_mode >= 0.5f && shader_injection.tone_map_type != 0.f;
  }
  {
    const mgsv::rendering::submission::ExecutionGuard execution_guard;
    const bool was_selected = mgsv::inputs::IsSelectedSwapchain(swapchain);
    mgsv::inputs::OnPresent(queue, swapchain, source_rect, dest_rect, dirty_rect_count, dirty_rects);
    if (mgsv::inputs::IsSelectedSwapchain(swapchain)) {
      // The swapchain is created before Present selects it, so view tracking
      // starts on the first selecting Present. OnInitSwapchain refreshes it
      // after a resize.
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
      // The scene camera and motion records feed reflection history and the
      // lighting noise phase, so they are kept only while enhanced lighting is on.
      mgsv::lighting::temporal_inputs::enabled.store(lighting_enabled, std::memory_order_release);
      mgsv::lighting::indirect_lighting_enhanced.store(lighting_enabled, std::memory_order_release);
      mgsv::engine::motion_vectors::wanted.store(lighting_enabled || mgsv::reconstruction::state::IsEnabled(), std::memory_order_release);
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
}

void OnDestroyDevice(reshade::api::device* device) {
  // Only the game's D3D11 device matters here. The bridge's D3D12 device can
  // be released during D3D11 teardown and must not start this.
  if (device->get_api() != reshade::api::device_api::d3d11) return;
  if (!mgsv::inputs::IsSelectedDevice(device)) return;
  // Remove these hooks before taking the execution lock: a cut in progress
  // inside them can be waiting for that lock.
  mgsv::engine::post_filter::Detach();
  mgsv::engine::motion_vectors::Detach();
  const mgsv::rendering::submission::ExecutionGuard execution_guard;
  mgsv::engine::views::Shutdown();
  mgsv::lighting::reflections::Destroy();
  mgsv::inputs::OnDestroyDevice(device);
  mgsv::lighting::environment_specular::Reset();
  mgsv::lighting::material_inputs::Reset();
  mgsv::lighting::source_preparation::Reset();
  mgsv::lighting::ambient_diffuse::Reset();
  mgsv::ao::Destroy(device);
  mgsv::il::Destroy(device);
  mgsv::lighting::DestroyLightPassLayout(device);
  mgsv::lighting::DestroyFallbackViews(device);
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

      // Build the menu. Core settings come first because bloom inserts its
      // controls among them in Effects; each other feature appends its section.
      mgsv::settings::AppendSettings(&settings, &shader_injection, &output_state);
      mgsv::bloom::AppendSettings(&settings, &shader_injection);
      mgsv::shadows::AppendSettings(&settings, &shader_injection);
      mgsv::reconstruction::settings::AppendSettings(&settings, &shader_injection, &reconstruction_settings);
      mgsv::lighting::AppendSettings(&settings, &shader_injection);

      // Before the menu is first drawn, list the features whose engine entries
      // or structure fields did not verify in the running executable.
      if (std::ranges::none_of(settings, [](auto* setting) { return setting->section == "Game Version"; })) {
        std::string unavailable;
        for (const auto& requirement : {
                 mgsv::engine::views::REQUIREMENT,
                 mgsv::engine::render_targets::REQUIREMENT,
                 mgsv::engine::motion_vectors::REQUIREMENT,
                 mgsv::engine::post_filter::REQUIREMENT,
                 mgsv::reconstruction::projection_jitter::REQUIREMENT,
                 mgsv::reconstruction::resolution::REQUIREMENT,
                 mgsv::engine::local_lights::REQUIREMENT,
             }) {
          if (mgsv::engine::Verified(requirement)) continue;
          unavailable.append(unavailable.empty() ? "" : ", ").append(requirement.feature);
        }
        if (!unavailable.empty()) {
          settings.insert(
              settings.begin(),
              new renodx::utils::settings::Setting{
                  .value_type = renodx::utils::settings::SettingValueType::TEXT,
                  .label = "This version of the game is not supported by: " + unavailable + ". They stay off; the ReShade log lists what did not match.",
                  .section = "Game Version",
                  .tint = 0xFF0000,
              });
        }
      }

      if (!initialized) {
        renodx::mods::swapchain::v2::force_borderless = true;
        renodx::mods::swapchain::v2::prevent_full_screen = true;

        renodx::mods::swapchain::v2::SetUseHDR10();
        renodx::mods::swapchain::v2::set_color_space = false;

        renodx::utils::settings::on_preset_changed_callbacks.emplace_back(
            [] { mgsv::reconstruction::transitions::ApplySettingsSnapshot(reconstruction_settings); });

        // Settings reach shaders in b13 (RenoDXSettings in shared.h). Only the
        // game's D3D11 shaders get that buffer; the D3D12 passes bind their own.
        renodx::mods::shader::expected_constant_buffer_index = 13;
        renodx::mods::shader::on_create_pipeline_layout = [](reshade::api::device* device, std::span<reshade::api::pipeline_layout_param>) {
          return device->get_api() == reshade::api::device_api::d3d11;
        };
        renodx::mods::shader::minimum_constant_buffer_stages =
            reshade::api::shader_stage::vertex
            | reshade::api::shader_stage::pixel
            | reshade::api::shader_stage::compute;
        renodx::mods::swapchain::v2::expected_constant_buffer_index = 13;

        // Keep the tuning and diagnostic groups out of the way on first open.
        // Every other section stays expanded, so only the "(Advanced)" groups start collapsed.
        renodx::utils::settings::open_sections_by_default = false;
        renodx::utils::settings::default_open_sections = {
            "Game Version",
            "Temporal Anti-Aliasing",
            "Presets",
            "Output",
            "Tone Mapping",
            "Color Grading",
            "Effects",
            "Direct Lighting",
            "Indirect Lighting",
            "Options",
            "Links",
            "About",
        };

        initialized = true;
      }

      // Swapchain selection, Present and device teardown for the whole mod.
      reshade::register_event<reshade::addon_event::copy_resource>(mgsv::reconstruction::output_size::OnCopyResource);
      reshade::register_event<reshade::addon_event::init_swapchain>(OnInitSwapchain);
      reshade::register_event<reshade::addon_event::destroy_swapchain>(OnDestroySwapchain);
      reshade::register_event<reshade::addon_event::present>(OnPresent);
      reshade::register_event<reshade::addon_event::destroy_device>(OnDestroyDevice);

      break;
    case DLL_PROCESS_DETACH:
      mgsv::engine::post_filter::Detach();
      mgsv::engine::motion_vectors::Detach();
      mgsv::engine::views::Shutdown();
      reshade::unregister_overlay(nullptr, ADDON_SETTINGS);
      reshade::unregister_event<reshade::addon_event::copy_resource>(mgsv::reconstruction::output_size::OnCopyResource);
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
  // Interfaces for the development probe, then each feature's device, command
  // list and resource callbacks. render_targets must hook at init_device, which
  // runs inside device creation before the engine creates its first target.
  if (fdw_reason == DLL_PROCESS_ATTACH) {
    mgsv::lighting::reflections::UseProbe(true);
    mgsv::reconstruction::dx12::UseProbe(true);
    mgsv::rendering::probe::UseProbe(true);
    mgsv::rendering::probe::transition.store(
        +[](std::string_view key, float value) {
          namespace reconstruction = mgsv::reconstruction;
          if (key == "FxUpscalingQuality") {
            return reconstruction::settings::UpdateSetting(reconstruction_settings.upscaling_quality_setting, value);
          }
          // Not a saved setting: 1 fills upscaled scenes without reconstructing them.
          if (key == "ReconstructionFillOnly") {
            reconstruction::input_capture::probe_fill_only.store(value >= 0.5f, std::memory_order_release);
            return true;
          }
          if (key != "FxTemporalReconstructionMode") return false;
          const auto mode = reconstruction::state::NormalizeTemporalMode(value);
          const auto* option = reconstruction::transitions::FindTemporalModeOption(mode);
          if (option == nullptr || (option->unavailable_reason != nullptr && option->unavailable_reason() != nullptr)
              || !reconstruction::settings::UpdateSetting(reconstruction_settings.temporal_mode_setting, static_cast<float>(mode))) return false;
          reconstruction::transitions::TransitionTemporalMode(reconstruction_settings, static_cast<float>(mode), "probe request");
          return true;
        },
        std::memory_order_release);
    reshade::register_event<reshade::addon_event::init_device>(mgsv::engine::render_targets::OnInitDevice);
    reshade::register_event<reshade::addon_event::destroy_device>(mgsv::engine::render_targets::OnDestroyDevice);
    reshade::register_event<reshade::addon_event::init_device>(mgsv::shadows::screen_space::OnInitDevice);
    reshade::register_event<reshade::addon_event::destroy_device>(mgsv::shadows::screen_space::OnDestroyDevice);
    reshade::register_event<reshade::addon_event::init_command_list>(mgsv::shadows::screen_space::OnInitCommandList);
    reshade::register_event<reshade::addon_event::reset_command_list>(mgsv::shadows::screen_space::OnResetCommandList);
    reshade::register_event<reshade::addon_event::destroy_command_list>(mgsv::shadows::screen_space::OnDestroyCommandList);
    reshade::register_event<reshade::addon_event::init_device>(mgsv::lighting::environment_specular::OnInitDevice);
    reshade::register_event<reshade::addon_event::init_device>(mgsv::lighting::material_inputs::OnInitDevice);
    reshade::register_event<reshade::addon_event::destroy_device>(mgsv::lighting::environment_specular::OnDestroyDevice);
    reshade::register_event<reshade::addon_event::destroy_device>(mgsv::lighting::material_inputs::OnDestroyDevice);
    reshade::register_event<reshade::addon_event::init_command_list>(mgsv::bloom::OnInitCommandList);
    reshade::register_event<reshade::addon_event::init_command_list>(mgsv::lighting::environment_specular::OnInitCommandList);
    reshade::register_event<reshade::addon_event::init_command_list>(mgsv::lighting::material_inputs::OnInitCommandList);
    reshade::register_event<reshade::addon_event::init_command_list>(mgsv::lighting::source_preparation::OnInitCommandList);
    reshade::register_event<reshade::addon_event::reset_command_list>(mgsv::lighting::environment_specular::OnResetCommandList);
    reshade::register_event<reshade::addon_event::reset_command_list>(mgsv::lighting::material_inputs::OnResetCommandList);
    reshade::register_event<reshade::addon_event::reset_command_list>(mgsv::lighting::source_preparation::OnResetCommandList);
    reshade::register_event<reshade::addon_event::init_command_list>(mgsv::lighting::reflections::native_chain::OnInitCommandList);
    reshade::register_event<reshade::addon_event::init_command_list>(mgsv::lighting::reflections::OnInitCommandList);
    reshade::register_event<reshade::addon_event::destroy_command_list>(mgsv::lighting::reflections::native_chain::OnDestroyCommandList);
    reshade::register_event<reshade::addon_event::destroy_command_list>(mgsv::lighting::reflections::OnDestroyCommandList);
    reshade::register_event<reshade::addon_event::reset_command_list>(mgsv::lighting::reflections::native_chain::OnResetCommandList);
    reshade::register_event<reshade::addon_event::reset_command_list>(mgsv::lighting::reflections::OnResetCommandList);
    reshade::register_event<reshade::addon_event::copy_resource>(mgsv::lighting::reflections::native_chain::OnCopyResource);
    reshade::register_event<reshade::addon_event::copy_texture_region>(mgsv::lighting::reflections::native_chain::OnCopyTexture);
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
    reshade::register_event<reshade::addon_event::destroy_command_list>(mgsv::bloom::OnDestroyCommandList);
    reshade::register_event<reshade::addon_event::reset_command_list>(mgsv::bloom::OnResetCommandList);
  } else if (fdw_reason == DLL_PROCESS_DETACH) {
    mgsv::lighting::reflections::UseProbe(false);
    mgsv::reconstruction::dx12::UseProbe(false);
    mgsv::rendering::probe::transition.store(nullptr, std::memory_order_release);
    mgsv::rendering::probe::UseProbe(false);
    reshade::unregister_event<reshade::addon_event::init_device>(mgsv::engine::render_targets::OnInitDevice);
    reshade::unregister_event<reshade::addon_event::destroy_device>(mgsv::engine::render_targets::OnDestroyDevice);
    reshade::unregister_event<reshade::addon_event::init_device>(mgsv::shadows::screen_space::OnInitDevice);
    reshade::unregister_event<reshade::addon_event::destroy_device>(mgsv::shadows::screen_space::OnDestroyDevice);
    reshade::unregister_event<reshade::addon_event::init_command_list>(mgsv::shadows::screen_space::OnInitCommandList);
    reshade::unregister_event<reshade::addon_event::reset_command_list>(mgsv::shadows::screen_space::OnResetCommandList);
    reshade::unregister_event<reshade::addon_event::destroy_command_list>(mgsv::shadows::screen_space::OnDestroyCommandList);
    reshade::unregister_event<reshade::addon_event::init_device>(mgsv::lighting::environment_specular::OnInitDevice);
    reshade::unregister_event<reshade::addon_event::init_device>(mgsv::lighting::material_inputs::OnInitDevice);
    reshade::unregister_event<reshade::addon_event::destroy_device>(mgsv::lighting::environment_specular::OnDestroyDevice);
    reshade::unregister_event<reshade::addon_event::destroy_device>(mgsv::lighting::material_inputs::OnDestroyDevice);
    reshade::unregister_event<reshade::addon_event::init_command_list>(mgsv::bloom::OnInitCommandList);
    reshade::unregister_event<reshade::addon_event::init_command_list>(mgsv::lighting::environment_specular::OnInitCommandList);
    reshade::unregister_event<reshade::addon_event::init_command_list>(mgsv::lighting::material_inputs::OnInitCommandList);
    reshade::unregister_event<reshade::addon_event::init_command_list>(mgsv::lighting::source_preparation::OnInitCommandList);
    reshade::unregister_event<reshade::addon_event::reset_command_list>(mgsv::lighting::environment_specular::OnResetCommandList);
    reshade::unregister_event<reshade::addon_event::reset_command_list>(mgsv::lighting::material_inputs::OnResetCommandList);
    reshade::unregister_event<reshade::addon_event::reset_command_list>(mgsv::lighting::source_preparation::OnResetCommandList);
    reshade::unregister_event<reshade::addon_event::init_command_list>(mgsv::lighting::reflections::native_chain::OnInitCommandList);
    reshade::unregister_event<reshade::addon_event::init_command_list>(mgsv::lighting::reflections::OnInitCommandList);
    reshade::unregister_event<reshade::addon_event::destroy_command_list>(mgsv::lighting::reflections::native_chain::OnDestroyCommandList);
    reshade::unregister_event<reshade::addon_event::destroy_command_list>(mgsv::lighting::reflections::OnDestroyCommandList);
    reshade::unregister_event<reshade::addon_event::reset_command_list>(mgsv::lighting::reflections::native_chain::OnResetCommandList);
    reshade::unregister_event<reshade::addon_event::reset_command_list>(mgsv::lighting::reflections::OnResetCommandList);
    reshade::unregister_event<reshade::addon_event::copy_resource>(mgsv::lighting::reflections::native_chain::OnCopyResource);
    reshade::unregister_event<reshade::addon_event::copy_texture_region>(mgsv::lighting::reflections::native_chain::OnCopyTexture);
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
    reshade::unregister_event<reshade::addon_event::destroy_command_list>(mgsv::bloom::OnDestroyCommandList);
    reshade::unregister_event<reshade::addon_event::reset_command_list>(mgsv::bloom::OnResetCommandList);
  }
  mgsv::reconstruction::Use(fdw_reason, reconstruction_settings);

  renodx::utils::random::Use(fdw_reason, {&shader_injection.film_grain_random});

  renodx::mods::swapchain::v2::Use(fdw_reason, &shader_injection);

  if (fdw_reason == DLL_PROCESS_DETACH) {
    mgsv::reconstruction::output_size::Use(fdw_reason);
  }
  renodx::mods::shader::Use(fdw_reason, custom_shaders, &shader_injection);
  if (fdw_reason == DLL_PROCESS_ATTACH) {
    mgsv::reconstruction::output_size::Use(fdw_reason);
  }

  return TRUE;
}
