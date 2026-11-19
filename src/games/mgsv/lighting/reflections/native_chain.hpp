// SPDX-License-Identifier: MIT
#ifndef MGSV_REFLECTIONS_NATIVE_CHAIN_HPP_
#define MGSV_REFLECTIONS_NATIVE_CHAIN_HPP_

#include <algorithm>
#include <array>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>
#include <include/reshade.hpp>

#include "../../rendering/producer_records.hpp"
#include "../indirect_lighting_mode.hpp"
#include "result.hpp"

namespace mgsv::lighting::reflections::native_chain {

// Gaussian and reduction shaders also serve other effects. Skip them only
// when their input descends from a skipped reflection trace in this recording.
// Keep allocations alive until reset so native pointer reuse cannot match one.
struct __declspec(uuid("f8459b18-f389-4996-bc13-aa10037afc29")) Recording {
  std::vector<Microsoft::WRL::ComPtr<ID3D11Resource>> skipped;
};

inline void OnInitCommandList(reshade::api::command_list* cmd_list) { cmd_list->create_private_data<Recording>(); }
inline void OnDestroyCommandList(reshade::api::command_list* cmd_list) { cmd_list->destroy_private_data<Recording>(); }
inline void OnResetCommandList(reshade::api::command_list* cmd_list) {
  if (auto* recording = cmd_list->get_private_data<Recording>()) {
    recording->skipped.clear();
  }
}

inline void MarkOutput(reshade::api::command_list* cmd_list, Recording* recording) {
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
  context->OMGetRenderTargets(1u, &target, nullptr);
  if (target == nullptr) return;
  Microsoft::WRL::ComPtr<ID3D11Resource> resource;
  target->GetResource(&resource);
  if (std::ranges::find(recording->skipped, resource) == recording->skipped.end()) {
    recording->skipped.push_back(std::move(resource));
  }
}

inline bool OnTrace(reshade::api::command_list* cmd_list) {
  if (!UsesEnhancedIndirectLighting()) return true;
  // Both native traces search the same screen depth. Enhanced uses the GGX
  // estimator for either path, including the sortie floor. Clear readiness in
  // execution order so a skipped submission retains only native environment light.
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> material;
  Microsoft::WRL::ComPtr<ID3D11Resource> resource;
  context->PSGetShaderResources(3u, 1u, &material);
  if (material != nullptr) material->GetResource(&resource);
  // The compositor takes the replacement path only when this result's readiness
  // is bound. Without a result, keep the native trace that it will composite.
  const auto result = specular_result::Read(cmd_list->get_device(), resource.Get());
  if (result == nullptr) return true;
  const std::array<float, 4> status = {};
  context->ClearUnorderedAccessViewFloat(result->ready.uav.Get(), status.data());
  auto* recording = cmd_list->get_private_data<Recording>();
  if (recording == nullptr) return true;
  recording->skipped.clear();
  MarkOutput(cmd_list, recording);
  return false;
}

inline bool OnFilter(reshade::api::command_list* cmd_list) {
  auto* recording = cmd_list->get_private_data<Recording>();
  if (recording == nullptr || recording->skipped.empty() || !UsesEnhancedIndirectLighting()) return true;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
  context->PSGetShaderResources(0u, 1u, &view);
  if (view == nullptr) return true;
  Microsoft::WRL::ComPtr<ID3D11Resource> resource;
  view->GetResource(&resource);
  if (std::ranges::find(recording->skipped, resource) == recording->skipped.end()) return true;
  MarkOutput(cmd_list, recording);
  return false;
}

inline bool OnCopyResource(reshade::api::command_list* cmd_list, reshade::api::resource source, reshade::api::resource destination) {
  auto* recording = cmd_list->get_private_data<Recording>();
  if (recording == nullptr || recording->skipped.empty() || !UsesEnhancedIndirectLighting()) return false;
  auto* from = reinterpret_cast<ID3D11Resource*>(source.handle);  // NOLINT(performance-no-int-to-ptr)
  const auto found = std::ranges::find_if(recording->skipped, [from](const auto& item) { return item.Get() == from; });
  if (found == recording->skipped.end()) return false;
  Microsoft::WRL::ComPtr<ID3D11Resource> to = reinterpret_cast<ID3D11Resource*>(destination.handle);  // NOLINT(performance-no-int-to-ptr)
  if (std::ranges::find(recording->skipped, to) == recording->skipped.end()) {
    recording->skipped.push_back(std::move(to));
  }
  return true;
}

inline bool OnCopyTexture(reshade::api::command_list* cmd_list, reshade::api::resource source, uint32_t source_subresource,
                          const reshade::api::subresource_box* source_box, reshade::api::resource destination, uint32_t destination_subresource,
                          const reshade::api::subresource_box* destination_box, reshade::api::filter_mode filter) {
  return OnCopyResource(cmd_list, source, destination);
}

}  // namespace mgsv::lighting::reflections::native_chain
#endif
