#ifndef MGSV_SPECULAR_RESULT_HPP_
#define MGSV_SPECULAR_RESULT_HPP_

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <memory>
#include <shared_mutex>

#include <include/reshade.hpp>

#include "../../../../utils/data.hpp"
#include "../../rendering/inputs.hpp"
#include "../../rendering/shared_texture.hpp"

namespace mgsv::lighting::specular_result {

struct Image {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
  std::shared_ptr<rendering::SharedTexture> shared;
};

// The Enhanced reflection images for one G-buffer material texture. Each
// material texture has one result, even when the diffuse and specular lighting
// targets alternate between several textures. The reflection composite checks
// the GPU-written readiness image, so CPU recording order cannot expose an old
// reflection.
struct Target {
  uint64_t generation = 0u;
  uint32_t width = 0u;
  uint32_t height = 0u;
  // [0]: complete reflected light, alpha marks a valid receiver.
  // [1]: environment sampling weight, alpha is directional visibility.
  // Both come from the same GGX sample.
  std::array<Image, 2> images;
  Image ready;  // R8_UNORM: 0 is unavailable, 1 means the result is complete.
  // Deferred composition writes the exact environment term already in scene
  // RGB, including the wet-ground cube map. Alpha is receiver fog transmission;
  // -1 means unavailable. The reflection composite replaces this term without
  // changing direct light.
  Image environment;
};

inline renodx::utils::data::ParallelFlatHashMap<ID3D11Resource*, std::shared_ptr<Target>, std::shared_mutex> targets;

inline std::shared_ptr<Target> Read(reshade::api::device* device, ID3D11Resource* material) {
  const auto selected = inputs::ReadSelection(device);
  std::shared_ptr<Target> result;
  if (selected.device == nullptr || material == nullptr) return result;
  targets.if_contains(material, [&](const auto& entry) {
    const auto& [native_material, capture] = entry;
    if (capture != nullptr && capture->generation == selected.generation) {
      result = capture;
    }
  });
  return result;
}

}  // namespace mgsv::lighting::specular_result

#endif  // MGSV_SPECULAR_RESULT_HPP_
