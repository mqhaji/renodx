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

// A material allocation has one result, even when diffuse/specular lighting
// ping-pongs through several allocations. Composition reads GPU-written
// readiness, so CPU recording order cannot expose an old reflection.
struct Target {
  uint64_t generation = 0u;
  uint32_t width = 0u;
  uint32_t height = 0u;
  // Complete reflected light (alpha: valid receiver), then environment sampling
  // weight (alpha: directional visibility). Both share the same GGX sample.
  std::array<Image, 2> images;
  Image ready;  // R8 bytes: 0 unavailable, 255 enhanced result complete.
  // The deferred draw writes the exact environment term already in scene RGB,
  // including the wet-ground cube. Alpha is receiver fog transmission; -1 is
  // unavailable. Later SSR replaces this term without changing direct light.
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
