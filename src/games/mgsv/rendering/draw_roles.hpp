/*
 * Copyright (C) 2026 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#ifndef RENODX_GAMES_MGSV_RENDERING_DRAW_ROLES_HPP_
#define RENODX_GAMES_MGSV_RENDERING_DRAW_ROLES_HPP_

#include <d3d11.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cstdint>

#include <include/reshade.hpp>

#include "../../../utils/resource.hpp"
#include "../../../utils/swapchain.hpp"
#include "../reconstruction/postprocess/runtime.hpp"
#include "engine_targets.hpp"

// Roles of draws that run generic or shared game shaders: velocity
// producers, the bloom chain, and reads of the engine's scene images.
namespace mgsv::rendering::draw_roles {

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

// The images the bloom chain wrote on this command list. They restrict the
// shared shader replacements to that chain.
struct __declspec(uuid("13000a80-3153-43c1-bcf1-1a1accb2bd06")) BloomChainScope {
  std::array<reshade::api::resource, 2> working = {};
};

inline void OnInitBloomCommandList(reshade::api::command_list* cmd_list) {
  if (cmd_list->get_device()->get_api() == reshade::api::device_api::d3d11) {
    cmd_list->create_private_data<BloomChainScope>();
  }
}

inline void OnResetBloomCommandList(reshade::api::command_list* cmd_list) {
  if (auto* scope = cmd_list->get_private_data<BloomChainScope>(); scope != nullptr) {
    scope->working = {};
  }
}

inline void OnDestroyBloomCommandList(reshade::api::command_list* cmd_list) {
  if (cmd_list->get_device()->get_api() == reshade::api::device_api::d3d11) {
    cmd_list->destroy_private_data<BloomChainScope>();
  }
}

// True for a draw of the bloom chain into one of the engine's bloom images: a
// reduction of the scene, or a pass whose input the chain wrote earlier on
// this command list. The replacement bloom shaders run only for these draws.
inline bool IsBloomChainDraw(reshade::api::command_list* cmd_list, bool reduction) {
  auto* scope = cmd_list->get_private_data<BloomChainScope>();
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
    const auto output = mgsv::reconstruction::post_process::GetActive(cmd_list);
    scene_input = (output != nullptr && (source.Get() == output->color.Get() || output->IsSceneTarget(source.Get())))
                  || mgsv::reconstruction::post_process::ReadSceneDepth(source.Get()) != nullptr;
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
  if (FAILED(bloom_image->GetPrivateData(engine_targets::BLOOM_IMAGE_GUID, &image_state_size, &image_state))
      || image_state_size != sizeof(image_state)) return false;
  if (image_state == engine_targets::BLOOM_IMAGE_NEW) {
    // Clear new padding before the cropped reduction writes its working region.
    constexpr std::array<float, 4> black = {};
    cmd_list->clear_render_target_view(targets[0], black.data(), 0u, nullptr);
    bloom_image->SetPrivateData(engine_targets::BLOOM_IMAGE_GUID, sizeof(engine_targets::BLOOM_IMAGE_CLEARED), &engine_targets::BLOOM_IMAGE_CLEARED);
  }
  if (reduction) {
    scope->working[scene_input ? 0u : 1u] = target;
  }
  return true;
}

// The engine's scene color or its TEMPORARY_OUTPUT copy, at a pixel shader slot.
inline bool PixelShaderInputIsSceneImage(reshade::api::command_list* cmd_list, uint32_t slot = 0u) {
  if (cmd_list == nullptr || cmd_list->get_device()->get_api() != reshade::api::device_api::d3d11) return false;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> input;
  context->PSGetShaderResources(slot, 1u, &input);
  if (input == nullptr) return false;
  Microsoft::WRL::ComPtr<ID3D11Resource> resource;
  input->GetResource(&resource);
  return engine_targets::ReadSceneRole(resource.Get()) == engine_targets::SCENE_IMAGE;
}

// The engine copies scene color into TEMPORARY_OUTPUT itself. An upscaled
// scene also needs that copy at output size; the native copy still runs.
inline bool OnCopySceneResource(
    reshade::api::command_list* cmd_list,
    reshade::api::resource source,
    reshade::api::resource dest) {
  if (cmd_list == nullptr || source.handle == dest.handle) return false;
  if (mgsv::reconstruction::post_process::OnCopyResource(cmd_list, source, dest)) return false;
  const auto output = mgsv::reconstruction::post_process::GetActive(cmd_list);
  if (output != nullptr && output->IsScaled()
      && output->IsSceneTarget(reinterpret_cast<ID3D11Resource*>(source.handle))) {  // NOLINT(performance-no-int-to-ptr)
    mgsv::reconstruction::resolution::Fail("cannot forward a scaled scene copy");
  }
  return false;
}

// CopyRenderBuffer is generic. Its copy of the bloom result ends the chain.
inline bool OnCopyRenderBufferDraw(reshade::api::command_list* cmd_list) {
  if (IsBloomChainDraw(cmd_list, false)) {
    cmd_list->get_private_data<BloomChainScope>()->working = {};
  }
  return true;
}

}  // namespace mgsv::rendering::draw_roles

#endif  // RENODX_GAMES_MGSV_RENDERING_DRAW_ROLES_HPP_
