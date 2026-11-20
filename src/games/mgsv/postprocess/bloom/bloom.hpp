/*
 * Copyright (C) 2026 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#ifndef RENODX_GAMES_MGSV_BLOOM_HPP_
#define RENODX_GAMES_MGSV_BLOOM_HPP_

#include <d3d11.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cstdint>

#include <include/reshade.hpp>

#include "../../../../utils/resource.hpp"
#include "../../../../utils/settings.hpp"
#include "../../../../utils/swapchain.hpp"
#include "../../engine/render_targets.hpp"
#include "../../reconstruction/output_size/runtime.hpp"
#include "../../shared.h"

namespace mgsv::bloom {

inline void AppendSettings(renodx::utils::settings::Settings* registry, ShaderInjectData* injection) {
  if (std::ranges::any_of(*registry, [](auto* setting) { return setting->key == "FxBloomType"; })) return;
  const auto position = std::ranges::find_if(*registry, [](auto* setting) { return setting->key == "FxSunBrightnessBoostEnabled"; });
  registry->insert(
      position, {
                    new renodx::utils::settings::Setting{
                        .key = "FxBloomType",
                        .binding = &injection->bloom_type,
                        .value_type = renodx::utils::settings::SettingValueType::INTEGER,
                        .default_value = 1.f,
                        .label = "Bloom Type",
                        .section = "Effects",
                        .tooltip = "Vanilla preserves the game's bloom. Enhanced spreads linear scene light into a tighter glow.",
                        .labels = {"Vanilla", "Enhanced"},
                        .on_change_value = [](float, float current) { reshade::log::message(reshade::log::level::info, current >= 0.5f ? "Bloom: Enhanced" : "Bloom: Vanilla"); },
                    },
                    new renodx::utils::settings::Setting{
                        .key = "FxBloomStrength",
                        .binding = &injection->bloom_strength,
                        .default_value = 100.f,
                        .label = "Bloom",
                        .section = "Effects",
                        .tooltip = "Controls bloom and fullscreen lens flares. In Enhanced mode, 100 blends in 10% blurred light.",
                        .max = 100.f,
                        .parse = [](float value) { return value * 0.01f; },
                    },
                });
}

// The images the bloom chain wrote on this command list. They restrict the
// shared shader replacements to that chain.
struct __declspec(uuid("13000a80-3153-43c1-bcf1-1a1accb2bd06")) ChainScope {
  std::array<reshade::api::resource, 2> working = {};
};

inline void OnInitCommandList(reshade::api::command_list* cmd_list) {
  if (cmd_list->get_device()->get_api() == reshade::api::device_api::d3d11) {
    cmd_list->create_private_data<ChainScope>();
  }
}

inline void OnResetCommandList(reshade::api::command_list* cmd_list) {
  if (auto* scope = cmd_list->get_private_data<ChainScope>(); scope != nullptr) {
    scope->working = {};
  }
}

inline void OnDestroyCommandList(reshade::api::command_list* cmd_list) {
  if (cmd_list->get_device()->get_api() == reshade::api::device_api::d3d11) {
    cmd_list->destroy_private_data<ChainScope>();
  }
}

// True for a draw of the bloom chain into one of the engine's bloom images: a
// reduction of the scene, or a pass whose input the chain wrote earlier on
// this command list. The replacement bloom shaders run only for these draws.
inline bool IsChainDraw(reshade::api::command_list* cmd_list, bool reduction) {
  auto* scope = cmd_list->get_private_data<ChainScope>();
  if (scope == nullptr) return false;
  auto* device = cmd_list->get_device();
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> input;
  Microsoft::WRL::ComPtr<ID3D11Resource> source;
  context->PSGetShaderResources(0u, 1u, &input);
  if (input == nullptr) return false;
  input->GetResource(&source);
  const reshade::api::resource source_resource = {reinterpret_cast<uint64_t>(source.Get())};
  const bool bloom_input = std::ranges::find(scope->working, source_resource) != scope->working.end();
  bool scene_input = false;
  if (reduction && !bloom_input) {
    const auto output = reconstruction::output_size::GetActive(cmd_list);
    scene_input = (output != nullptr && (source.Get() == output->color.Get() || output->IsSceneTarget(source.Get())))
                  || reconstruction::output_size::ReadSceneDepth(source.Get()) != nullptr;
  }
  if (!bloom_input && !scene_input) return false;
  const auto& targets = renodx::utils::swapchain::GetRenderTargets(cmd_list);
  if (targets.empty() || targets[0].handle == 0u) return false;
  const auto target = renodx::utils::resource::GetResourceFromView(device, targets[0]);
  if (target.handle == 0u) return false;
  const auto source_description = device->get_resource_desc(source_resource);
  const auto target_description = device->get_resource_desc(target);
  if (source_description.type != reshade::api::resource_type::texture_2d || target_description.type != reshade::api::resource_type::texture_2d) return false;
  const uint32_t divisor = (scene_input ? 4u : 1u);
  if (target_description.texture.width != source_description.texture.width / divisor
      || target_description.texture.height != source_description.texture.height / divisor) return false;
  if (scene_input) {
    scope->working = {};
  }
  auto* bloom_image = reinterpret_cast<ID3D11Resource*>(target.handle);  // NOLINT(performance-no-int-to-ptr)
  uint8_t image_state = 0u;
  UINT image_state_size = sizeof(image_state);
  if (FAILED(bloom_image->GetPrivateData(engine::render_targets::BLOOM_IMAGE_GUID, &image_state_size, &image_state))
      || image_state_size != sizeof(image_state)) return false;
  if (image_state == engine::render_targets::BLOOM_IMAGE_NEW) {
    // The chain writes only a working region. Clear the whole image the first
    // time the chain uses it, so the unwritten border stays black.
    constexpr std::array<float, 4> black = {};
    cmd_list->clear_render_target_view(targets[0], black.data(), 0u, nullptr);
    bloom_image->SetPrivateData(engine::render_targets::BLOOM_IMAGE_GUID, sizeof(engine::render_targets::BLOOM_IMAGE_CLEARED), &engine::render_targets::BLOOM_IMAGE_CLEARED);
  }
  if (reduction) {
    scope->working[scene_input ? 0u : 1u] = target;
  }
  return true;
}

// CopyRenderBuffer is a generic copy shader. When it copies a bloom image, the
// bloom chain is finished for this command list.
inline bool OnCopyRenderBufferDraw(reshade::api::command_list* cmd_list) {
  if (IsChainDraw(cmd_list, false)) {
    cmd_list->get_private_data<ChainScope>()->working = {};
  }
  return true;
}

}  // namespace mgsv::bloom

#endif
