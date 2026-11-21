// SPDX-License-Identifier: MIT
#ifndef MGSV_REFLECTIONS_INPUTS_HPP_
#define MGSV_REFLECTIONS_INPUTS_HPP_

#include <array>
#include <cstdint>
#include <memory>

#include <d3d11.h>
#include <wrl/client.h>

#include "result.hpp"

namespace mgsv::lighting::reflections {

inline constexpr UINT INPUT_COUNT = 13u;

// Reflection inputs captured at deferred composition, published with that
// native command list, and used by the same scene's later reflection composite.
// Holding a resource does not prove which scene wrote it; the producer records
// in submission order supply that association.
struct Inputs {
  std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, INPUT_COUNT> views;
  Microsoft::WRL::ComPtr<ID3D11Buffer> scene;
  Microsoft::WRL::ComPtr<ID3D11Resource> specular;
  Microsoft::WRL::ComPtr<ID3D11Resource> material;
  std::shared_ptr<specular_result::Target> result;
  uint64_t generation = 0u;
};

}  // namespace mgsv::lighting::reflections
#endif
