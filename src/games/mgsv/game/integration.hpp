/*
 * Copyright (C) 2024 Musa Haji
 * Copyright (C) 2026 ZioPao
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <d3d11.h>

#include <atomic>
#include <cstdint>
#include <string>

#include <wrl/client.h>

#include <include/reshade.hpp>

#include "../../../utils/state.hpp"
#include "../ao/ao.hpp"
#include "../il/il.hpp"
#include "../rendering/d3d11_compute_state.hpp"
#include "../rendering/light_pass.hpp"
#include "../shared.h"
#include "../taa/runtime/input_capture.hpp"

namespace mgsv::game {

// Add indirect lighting and AO after the game's subsurface-scattering passes.
// Indirect lighting runs first, so AO darkens both the direct and added light.

// Remember the deferred composite's albedo (t8) and specular-light (t10) views.
// These borrowed handles do not retain the views or copy their contents.
inline reshade::api::resource_view albedo_view = {0};
inline reshade::api::resource_view specular_view = {0};

inline void CaptureCompositeInputs(reshade::api::command_list* cmd_list) {
  const auto albedo = renodx::utils::state::GetBoundResourceView(
      cmd_list, reshade::api::shader_stage::pixel,
      reshade::api::descriptor_type::shader_resource_view, 8u);
  const auto specular = renodx::utils::state::GetBoundResourceView(
      cmd_list, reshade::api::shader_stage::pixel,
      reshade::api::descriptor_type::shader_resource_view, 10u);
  if (albedo.handle == 0 || specular.handle == 0) return;
  albedo_view = albedo;
  specular_view = specular;

  static bool logged = false;
  if (!logged) {
    logged = true;
    const auto* device = cmd_list->get_device();
    const std::string text =
        "mgsv: composite views albedo=" + std::to_string(static_cast<uint32_t>(device->get_resource_view_desc(albedo).format))
        + " specular=" + std::to_string(static_cast<uint32_t>(device->get_resource_view_desc(specular).format));
    reshade::log::message(reshade::log::level::info, text.c_str());
  }
}

// Called immediately after the game's scatter dispatch is recorded, not after GPU completion.
// Dispatch leaves its bindings unchanged, so read them here before recording our own passes.
inline void ApplyLightPasses(reshade::api::command_list* cmd_list, const ShaderInjectData& shader_injection) {
  if (cmd_list == nullptr || cmd_list->get_device()->get_api() != reshade::api::device_api::d3d11) return;

  const bool ao_enabled = shader_injection.ao_enabled != 0.f;
  const bool il_enabled = shader_injection.il_enabled != 0.f;
  if (!ao_enabled && !il_enabled) {
    static std::atomic_flag reported = {};
    mgsv::rendering::LogLightOnce(&reported, reshade::log::level::info, "scatter hook reached; AO and indirect lighting are disabled");
    return;
  }

  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (context == nullptr) return;

  // Keep these views alive until this callback returns, even if later bindings replace them.
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normal;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> light_source;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> light;
  context->CSGetShaderResources(1u, 1u, depth.GetAddressOf());
  context->CSGetShaderResources(2u, 1u, normal.GetAddressOf());
  context->CSGetUnorderedAccessViews(0u, 1u, light.GetAddressOf());
  if (depth == nullptr || normal == nullptr || light == nullptr) {
    static std::atomic_flag reported = {};
    mgsv::rendering::LogLightOnce(&reported, reshade::log::level::warning, "skipping lighting: missing scatter bindings depth=",
                                  depth != nullptr, " normal=", normal != nullptr, " light=", light != nullptr);
    return;
  }

  D3D11_UNORDERED_ACCESS_VIEW_DESC light_view_desc = {};
  light->GetDesc(&light_view_desc);
  if (light_view_desc.ViewDimension != D3D11_UAV_DIMENSION_TEXTURE2D
      || light_view_desc.Texture2D.MipSlice != 0u) {
    static std::atomic_flag reported = {};
    mgsv::rendering::LogLightOnce(&reported, reshade::log::level::warning,
                                  "skipping lighting: light output must be a 2D view of mip 0; dimension=",
                                  static_cast<uint32_t>(light_view_desc.ViewDimension));
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11Resource> light_resource;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> light_texture;
  light->GetResource(light_resource.GetAddressOf());
  if (FAILED(light_resource.As(&light_texture))) {
    static std::atomic_flag reported = {};
    mgsv::rendering::LogLightOnce(&reported, reshade::log::level::warning,
                                  "skipping lighting: light output is not a 2D texture");
    return;
  }

  D3D11_TEXTURE2D_DESC light_desc = {};
  light_texture->GetDesc(&light_desc);
  if (light_desc.Width == 0u || light_desc.Height == 0u
      || light_desc.ArraySize != 1u || light_desc.SampleDesc.Count != 1u) {
    static std::atomic_flag reported = {};
    mgsv::rendering::LogLightOnce(&reported, reshade::log::level::warning,
                                  "skipping lighting: unsupported light output size=", light_desc.Width, "x", light_desc.Height,
                                  " array_size=", light_desc.ArraySize, " samples=", light_desc.SampleDesc.Count);
    return;
  }

  const auto is_supported_input = [&](ID3D11ShaderResourceView* view, ID3D11Resource* excluded_resource = nullptr) {
    if (view == nullptr) return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC view_desc = {};
    view->GetDesc(&view_desc);
    if (view_desc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D
        || view_desc.Texture2D.MostDetailedMip != 0u) return false;

    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    view->GetResource(resource.GetAddressOf());
    if (resource.Get() == excluded_resource || FAILED(resource.As(&texture))) return false;

    D3D11_TEXTURE2D_DESC desc = {};
    texture->GetDesc(&desc);
    return desc.Width == light_desc.Width && desc.Height == light_desc.Height
           && desc.ArraySize == 1u && desc.SampleDesc.Count == 1u;
  };
  if (!is_supported_input(depth.Get()) || !is_supported_input(normal.Get())) {
    static std::atomic_flag reported = {};
    mgsv::rendering::LogLightOnce(&reported, reshade::log::level::warning,
                                  "skipping lighting: depth or normal view has an unsupported layout or size; output=",
                                  light_desc.Width, "x", light_desc.Height);
    return;
  }

  // Missing inputs or a light source that overlaps the output skip indirect lighting, not AO.
  if (il_enabled) {
    context->CSGetShaderResources(4u, 1u, light_source.GetAddressOf());
  }
  const bool run_il = il_enabled && is_supported_input(light_source.Get(), light_resource.Get())
                      && albedo_view.handle != 0u && specular_view.handle != 0u;
  if (il_enabled && !run_il) {
    static std::atomic_flag reported = {};
    mgsv::rendering::LogLightOnce(&reported, reshade::log::level::warning,
                                  "skipping indirect lighting: check light source layout, size, and output overlap; source_present=",
                                  light_source != nullptr, " albedo_present=", albedo_view.handle != 0u,
                                  " specular_present=", specular_view.handle != 0u, "; AO enabled=", ao_enabled);
  }
  if (!run_il && !ao_enabled) return;

  const mgsv::rendering::LightFrame frame = {
      .depth = {reinterpret_cast<uintptr_t>(depth.Get())},
      .normal = {reinterpret_cast<uintptr_t>(normal.Get())},
      .light = {reinterpret_cast<uintptr_t>(light.Get())},
      .light_source = {reinterpret_cast<uintptr_t>(light_source.Get())},
      .albedo = albedo_view,
      .specular = specular_view,
      .width = light_desc.Width,
      .height = light_desc.Height,
  };

  // Lighting commands can also unbind graphics resources. Save the affected state once
  // and restore it after both effects, including slots that were originally empty.
  const auto previous_state = mgsv::rendering::d3d11_compute_state::Capture(cmd_list);
  if (run_il) {
    // This bridge retains the existing borrowed capture/freshness behavior; it does not make GI independent.
    mgsv::il::Apply(frame, cmd_list, shader_injection, ::taa::input_capture::resources.velocity_srv);
  }
  if (ao_enabled) {
    mgsv::ao::Apply(frame, cmd_list, shader_injection);
  }
  mgsv::rendering::d3d11_compute_state::Restore(cmd_list, previous_state);
}

}  // namespace mgsv::game