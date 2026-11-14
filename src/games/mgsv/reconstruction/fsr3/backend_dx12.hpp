#ifndef RENODX_GAMES_MGSV_RECONSTRUCTION_FSR3_BACKEND_DX12_HPP_
#define RENODX_GAMES_MGSV_RECONSTRUCTION_FSR3_BACKEND_DX12_HPP_

// D3D12 host backend for the pinned FSR 3.1.5 shaders. AMD still owns the pass
// schedule. This adapter supplies descriptors and uploads, and orders resource
// reads and writes. Format and binding helpers retain the MIT-licensed
// MapleHinata backend's conventions; see README.md for attribution.

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <memory>
#include <stdexcept>
#include <vector>

#include "../../rendering/d3d12_compute.hpp"
#include "ffx/api/internal/ffx_interface.h"

namespace mgsv::reconstruction::fsr3::backend {
namespace compute = rendering::d3d12_compute;

// The fixed shader permutations use the same register ranges for every pass.
inline constexpr UINT MAX_COMPUTE_SRVS = 16u;
inline constexpr UINT MAX_COMPUTE_UAVS = 8u;

inline uint32_t FullMipCount(uint32_t width, uint32_t height) {
  uint32_t count = 1u;
  uint32_t extent = std::max(width, height);
  while (extent > 1u) {
    extent >>= 1u;
    ++count;
  }
  return count;
}

inline DXGI_FORMAT ToDxgiFormat(FfxApiSurfaceFormat format) {
  switch (format) {
    case FFX_API_SURFACE_FORMAT_R32G32B32A32_TYPELESS: return DXGI_FORMAT_R32G32B32A32_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R32G32B32A32_UINT:     return DXGI_FORMAT_R32G32B32A32_UINT;
    case FFX_API_SURFACE_FORMAT_R32G32B32A32_FLOAT:    return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case FFX_API_SURFACE_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT:    return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case FFX_API_SURFACE_FORMAT_R32G32B32_FLOAT:       return DXGI_FORMAT_R32G32B32_FLOAT;
    case FFX_API_SURFACE_FORMAT_R32G32_TYPELESS:       return DXGI_FORMAT_R32G32_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R32G32_FLOAT:          return DXGI_FORMAT_R32G32_FLOAT;
    case FFX_API_SURFACE_FORMAT_R32G32_UINT:           return DXGI_FORMAT_R32G32_UINT;
    case FFX_API_SURFACE_FORMAT_R32_TYPELESS:          return DXGI_FORMAT_R32_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R32_UINT:              return DXGI_FORMAT_R32_UINT;
    case FFX_API_SURFACE_FORMAT_R32_FLOAT:             return DXGI_FORMAT_R32_FLOAT;
    case FFX_API_SURFACE_FORMAT_R16G16_TYPELESS:       return DXGI_FORMAT_R16G16_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R16G16_FLOAT:          return DXGI_FORMAT_R16G16_FLOAT;
    case FFX_API_SURFACE_FORMAT_R16G16_UINT:           return DXGI_FORMAT_R16G16_UINT;
    case FFX_API_SURFACE_FORMAT_R16G16_SINT:           return DXGI_FORMAT_R16G16_SINT;
    case FFX_API_SURFACE_FORMAT_R16_TYPELESS:          return DXGI_FORMAT_R16_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R16_FLOAT:             return DXGI_FORMAT_R16_FLOAT;
    case FFX_API_SURFACE_FORMAT_R16_UINT:              return DXGI_FORMAT_R16_UINT;
    case FFX_API_SURFACE_FORMAT_R16_UNORM:             return DXGI_FORMAT_R16_UNORM;
    case FFX_API_SURFACE_FORMAT_R16_SNORM:             return DXGI_FORMAT_R16_SNORM;
    case FFX_API_SURFACE_FORMAT_R10G10B10A2_TYPELESS:  return DXGI_FORMAT_R10G10B10A2_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R10G10B10A2_UNORM:     return DXGI_FORMAT_R10G10B10A2_UNORM;
    case FFX_API_SURFACE_FORMAT_R11G11B10_FLOAT:       return DXGI_FORMAT_R11G11B10_FLOAT;
    case FFX_API_SURFACE_FORMAT_R8G8B8A8_TYPELESS:     return DXGI_FORMAT_R8G8B8A8_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R8G8B8A8_UNORM:        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case FFX_API_SURFACE_FORMAT_R8G8B8A8_SNORM:        return DXGI_FORMAT_R8G8B8A8_SNORM;
    case FFX_API_SURFACE_FORMAT_R8G8B8A8_SRGB:         return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case FFX_API_SURFACE_FORMAT_B8G8R8A8_TYPELESS:     return DXGI_FORMAT_B8G8R8A8_TYPELESS;
    case FFX_API_SURFACE_FORMAT_B8G8R8A8_UNORM:        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case FFX_API_SURFACE_FORMAT_B8G8R8A8_SRGB:         return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case FFX_API_SURFACE_FORMAT_R8G8_TYPELESS:         return DXGI_FORMAT_R8G8_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R8G8_UNORM:            return DXGI_FORMAT_R8G8_UNORM;
    case FFX_API_SURFACE_FORMAT_R8G8_UINT:             return DXGI_FORMAT_R8G8_UINT;
    case FFX_API_SURFACE_FORMAT_R8_TYPELESS:           return DXGI_FORMAT_R8_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R8_UNORM:              return DXGI_FORMAT_R8_UNORM;
    case FFX_API_SURFACE_FORMAT_R8_SNORM:              return DXGI_FORMAT_R8_SNORM;
    case FFX_API_SURFACE_FORMAT_R8_UINT:               return DXGI_FORMAT_R8_UINT;
    case FFX_API_SURFACE_FORMAT_R9G9B9E5_SHAREDEXP:    return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
    default:                                           return DXGI_FORMAT_UNKNOWN;
  }
}

inline FfxApiSurfaceFormat FromDxgiFormat(DXGI_FORMAT format) {
  switch (format) {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:    return FFX_API_SURFACE_FORMAT_R32G32B32A32_TYPELESS;
    case DXGI_FORMAT_R32G32B32A32_UINT:        return FFX_API_SURFACE_FORMAT_R32G32B32A32_UINT;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:       return FFX_API_SURFACE_FORMAT_R32G32B32A32_FLOAT;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:    return FFX_API_SURFACE_FORMAT_R16G16B16A16_TYPELESS;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:       return FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT;
    case DXGI_FORMAT_R32G32B32_FLOAT:          return FFX_API_SURFACE_FORMAT_R32G32B32_FLOAT;
    case DXGI_FORMAT_R32G32_TYPELESS:          return FFX_API_SURFACE_FORMAT_R32G32_TYPELESS;
    case DXGI_FORMAT_R32G32_FLOAT:             return FFX_API_SURFACE_FORMAT_R32G32_FLOAT;
    case DXGI_FORMAT_R32G32_UINT:              return FFX_API_SURFACE_FORMAT_R32G32_UINT;
    case DXGI_FORMAT_R32_TYPELESS:             return FFX_API_SURFACE_FORMAT_R32_TYPELESS;
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:                return FFX_API_SURFACE_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_R32_UINT:                 return FFX_API_SURFACE_FORMAT_R32_UINT;
    case DXGI_FORMAT_R16G16_TYPELESS:          return FFX_API_SURFACE_FORMAT_R16G16_TYPELESS;
    case DXGI_FORMAT_R16G16_FLOAT:             return FFX_API_SURFACE_FORMAT_R16G16_FLOAT;
    case DXGI_FORMAT_R16G16_UINT:              return FFX_API_SURFACE_FORMAT_R16G16_UINT;
    case DXGI_FORMAT_R16G16_SINT:              return FFX_API_SURFACE_FORMAT_R16G16_SINT;
    case DXGI_FORMAT_R16_TYPELESS:             return FFX_API_SURFACE_FORMAT_R16_TYPELESS;
    case DXGI_FORMAT_R16_FLOAT:                return FFX_API_SURFACE_FORMAT_R16_FLOAT;
    case DXGI_FORMAT_R16_UINT:                 return FFX_API_SURFACE_FORMAT_R16_UINT;
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:                return FFX_API_SURFACE_FORMAT_R16_UNORM;
    case DXGI_FORMAT_R16_SNORM:                return FFX_API_SURFACE_FORMAT_R16_SNORM;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:     return FFX_API_SURFACE_FORMAT_R10G10B10A2_TYPELESS;
    case DXGI_FORMAT_R10G10B10A2_UNORM:        return FFX_API_SURFACE_FORMAT_R10G10B10A2_UNORM;
    case DXGI_FORMAT_R11G11B10_FLOAT:          return FFX_API_SURFACE_FORMAT_R11G11B10_FLOAT;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:        return FFX_API_SURFACE_FORMAT_R8G8B8A8_TYPELESS;
    case DXGI_FORMAT_R8G8B8A8_UNORM:           return FFX_API_SURFACE_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:      return FFX_API_SURFACE_FORMAT_R8G8B8A8_SRGB;
    case DXGI_FORMAT_R8G8B8A8_SNORM:           return FFX_API_SURFACE_FORMAT_R8G8B8A8_SNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:        return FFX_API_SURFACE_FORMAT_B8G8R8A8_TYPELESS;
    case DXGI_FORMAT_B8G8R8A8_UNORM:           return FFX_API_SURFACE_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:      return FFX_API_SURFACE_FORMAT_B8G8R8A8_SRGB;
    case DXGI_FORMAT_R8G8_TYPELESS:            return FFX_API_SURFACE_FORMAT_R8G8_TYPELESS;
    case DXGI_FORMAT_R8G8_UNORM:               return FFX_API_SURFACE_FORMAT_R8G8_UNORM;
    case DXGI_FORMAT_R8G8_UINT:                return FFX_API_SURFACE_FORMAT_R8G8_UINT;
    case DXGI_FORMAT_R8_TYPELESS:              return FFX_API_SURFACE_FORMAT_R8_TYPELESS;
    case DXGI_FORMAT_R8_UNORM:                 return FFX_API_SURFACE_FORMAT_R8_UNORM;
    case DXGI_FORMAT_R8_SNORM:                 return FFX_API_SURFACE_FORMAT_R8_SNORM;
    case DXGI_FORMAT_R8_UINT:                  return FFX_API_SURFACE_FORMAT_R8_UINT;
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:       return FFX_API_SURFACE_FORMAT_R9G9B9E5_SHAREDEXP;
    default:                                   return FFX_API_SURFACE_FORMAT_UNKNOWN;
  }
}

inline uint32_t BytesPerPixel(FfxApiSurfaceFormat format) {
  switch (format) {
    case FFX_API_SURFACE_FORMAT_R32G32B32A32_TYPELESS:
    case FFX_API_SURFACE_FORMAT_R32G32B32A32_UINT:
    case FFX_API_SURFACE_FORMAT_R32G32B32A32_FLOAT:    return 16u;
    case FFX_API_SURFACE_FORMAT_R32G32B32_FLOAT:       return 12u;
    case FFX_API_SURFACE_FORMAT_R16G16B16A16_TYPELESS:
    case FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT:
    case FFX_API_SURFACE_FORMAT_R32G32_TYPELESS:
    case FFX_API_SURFACE_FORMAT_R32G32_FLOAT:
    case FFX_API_SURFACE_FORMAT_R32G32_UINT:           return 8u;
    case FFX_API_SURFACE_FORMAT_R32_TYPELESS:
    case FFX_API_SURFACE_FORMAT_R32_UINT:
    case FFX_API_SURFACE_FORMAT_R32_FLOAT:
    case FFX_API_SURFACE_FORMAT_R16G16_TYPELESS:
    case FFX_API_SURFACE_FORMAT_R16G16_FLOAT:
    case FFX_API_SURFACE_FORMAT_R16G16_UINT:
    case FFX_API_SURFACE_FORMAT_R16G16_SINT:
    case FFX_API_SURFACE_FORMAT_R10G10B10A2_TYPELESS:
    case FFX_API_SURFACE_FORMAT_R10G10B10A2_UNORM:
    case FFX_API_SURFACE_FORMAT_R11G11B10_FLOAT:
    case FFX_API_SURFACE_FORMAT_R8G8B8A8_TYPELESS:
    case FFX_API_SURFACE_FORMAT_R8G8B8A8_UNORM:
    case FFX_API_SURFACE_FORMAT_R8G8B8A8_SNORM:
    case FFX_API_SURFACE_FORMAT_R8G8B8A8_SRGB:
    case FFX_API_SURFACE_FORMAT_B8G8R8A8_TYPELESS:
    case FFX_API_SURFACE_FORMAT_B8G8R8A8_UNORM:
    case FFX_API_SURFACE_FORMAT_B8G8R8A8_SRGB:
    case FFX_API_SURFACE_FORMAT_R9G9B9E5_SHAREDEXP:    return 4u;
    case FFX_API_SURFACE_FORMAT_R16_TYPELESS:
    case FFX_API_SURFACE_FORMAT_R16_FLOAT:
    case FFX_API_SURFACE_FORMAT_R16_UINT:
    case FFX_API_SURFACE_FORMAT_R16_UNORM:
    case FFX_API_SURFACE_FORMAT_R16_SNORM:
    case FFX_API_SURFACE_FORMAT_R8G8_TYPELESS:
    case FFX_API_SURFACE_FORMAT_R8G8_UNORM:
    case FFX_API_SURFACE_FORMAT_R8G8_UINT:             return 2u;
    case FFX_API_SURFACE_FORMAT_R8_TYPELESS:
    case FFX_API_SURFACE_FORMAT_R8_UNORM:
    case FFX_API_SURFACE_FORMAT_R8_SNORM:
    case FFX_API_SURFACE_FORMAT_R8_UINT:               return 1u;
    default:                                           return 0u;
  }
}

inline uint64_t EstimateSize(const FfxApiResourceDescription& description) {
  const uint32_t bytes_per_pixel = BytesPerPixel(static_cast<FfxApiSurfaceFormat>(description.format));
  if (description.type == FFX_API_RESOURCE_TYPE_BUFFER) return description.size;
  if (bytes_per_pixel == 0u) return 0u;

  uint32_t width = description.width;
  uint32_t height = std::max(1u, description.height);
  const uint32_t mip_count = description.mipCount == 0u ? FullMipCount(width, height) : description.mipCount;
  uint64_t total = 0u;
  for (uint32_t mip = 0u; mip < mip_count; ++mip) {
    total += static_cast<uint64_t>(std::max(1u, width)) * std::max(1u, height)
             * std::max(1u, description.depth) * bytes_per_pixel;
    width = std::max(1u, width >> 1u);
    height = std::max(1u, height >> 1u);
  }
  return total;
}

inline void CopyBindings(
    FfxResourceBinding* output,
    uint32_t count,
    const char** names,
    const uint32_t* slots) {
  for (uint32_t index = 0u; index < count; ++index) {
    output[index].slotIndex = slots[index];
    output[index].arrayIndex = 0u;
    output[index].resourceIdentifier = 0u;
    output[index].name[0] = L'\0';
    if (names[index] != nullptr) {
      size_t converted = 0u;
      mbstowcs_s(&converted, output[index].name, names[index], FFX_RESOURCE_NAME_SIZE - 1u);
    }
  }
}

inline D3D12_RESOURCE_STATES GetState(uint32_t state) {
  switch (state) {
    case FFX_API_RESOURCE_STATE_UNORDERED_ACCESS: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    case FFX_API_RESOURCE_STATE_COMPUTE_READ:     return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    case FFX_API_RESOURCE_STATE_COPY_SRC:         return D3D12_RESOURCE_STATE_COPY_SOURCE;
    case FFX_API_RESOURCE_STATE_COPY_DEST:        return D3D12_RESOURCE_STATE_COPY_DEST;
    case FFX_API_RESOURCE_STATE_COMMON:           return D3D12_RESOURCE_STATE_COMMON;
    default:                                      throw std::runtime_error("Unsupported FSR resource state");
  }
}

struct Resource {
  Microsoft::WRL::ComPtr<ID3D12Resource> texture;
  Microsoft::WRL::ComPtr<ID3D12Resource> initial_upload;
  FfxApiResourceDescription description = {};
  D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
  D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
  bool upload_pending = false;
};

struct Backend {
  ID3D12Device* device = nullptr;
  // Resources use SDK indices. Zero is the SDK's null resource. Dynamic imports
  // grow down from the end; private histories grow up from the beginning.
  std::array<Resource, FFX_MAX_RESOURCE_COUNT> resources;
  uint32_t next_static = 1u;
  uint32_t next_dynamic = FFX_MAX_RESOURCE_COUNT - 1u;
  std::vector<FfxGpuJobDescription> jobs;
  alignas(16) std::array<uint8_t, FFX_CONSTANT_BUFFER_RING_BUFFER_SIZE> constants;
  uint32_t constant_cursor = 0u;
  compute::Frame* frame = nullptr;

  static void Transition(ID3D12GraphicsCommandList* commands, Resource* resource, D3D12_RESOURCE_STATES state) {
    if (resource->texture == nullptr) return;
    compute::Transition(commands, resource->texture.Get(), resource->state, state);
    resource->state = state;
  }
};

inline Backend* GetBackend(FfxInterface* backend_interface) {
  return static_cast<Backend*>(backend_interface->scratchBuffer);
}

inline FfxVersionNumber GetSdkVersion(FfxInterface* /*backend_interface*/) {
  return FFX_SDK_MAKE_VERSION(FFX_SDK_VERSION_MAJOR, FFX_SDK_VERSION_MINOR, FFX_SDK_VERSION_PATCH);
}

inline FfxErrorCode GetMemoryUsage(FfxInterface* backend_interface, FfxUInt32 /*context_id*/, FfxApiEffectMemoryUsage* output) {
  *output = {};
  for (const auto& resource : GetBackend(backend_interface)->resources) {
    output->totalUsageInBytes += EstimateSize(resource.description);
  }
  return FFX_OK;
}

inline FfxErrorCode CreateContext(FfxInterface* backend_interface, FfxEffect effect, FfxEffectBindlessConfig* bindless, FfxUInt32* id) {
  if (effect != FFX_EFFECT_FSR3UPSCALER || bindless != nullptr) return FFX_ERROR_INVALID_ARGUMENT;
  *id = 0u;
  GetBackend(backend_interface)->jobs.reserve(FFX_MAX_GPU_JOBS);
  return FFX_OK;
}

inline FfxErrorCode DestroyContext(FfxInterface* backend_interface, FfxUInt32 /*context_id*/) {
  auto* backend = GetBackend(backend_interface);
  backend->resources = {};
  backend->next_static = 1u;
  backend->next_dynamic = FFX_MAX_RESOURCE_COUNT - 1u;
  backend->jobs.clear();
  return FFX_OK;
}

inline FfxErrorCode GetCapabilities(FfxInterface* /*backend_interface*/, FfxDeviceCapabilities* capabilities) {
  *capabilities = {};
  // Keep the validated FP32/no-wave permutation until the separate FSR DLL
  // migration. D3D12 accepts the existing DXBC without changing its algorithm.
  capabilities->maximumSupportedShaderModel = FFX_SHADER_MODEL_5_1;
  capabilities->dedicatedAllocationSupported = true;
  return FFX_OK;
}

inline FfxErrorCode CreateResource(FfxInterface* backend_interface, const FfxCreateResourceDescription* description, FfxUInt32 /*context_id*/, FfxResourceInternal* output) {
  auto* backend = GetBackend(backend_interface);
  if (description->resourceDescription.type != FFX_API_RESOURCE_TYPE_TEXTURE2D
      || description->heapInfo.heapType != FFX_HEAP_TYPE_DEFAULT || description->heapInfo.usePlacementHeap) return FFX_ERROR_INVALID_ARGUMENT;
  if (backend->next_static >= backend->next_dynamic) return FFX_ERROR_INSUFFICIENT_MEMORY;
  const uint32_t index = backend->next_static++;
  auto& resource = backend->resources[index];
  resource.description = description->resourceDescription;
  if (resource.description.mipCount == 0u) {
    resource.description.mipCount = FullMipCount(resource.description.width, resource.description.height);
  }
  resource.texture = compute::CreateTexture(backend->device, resource.description.width, resource.description.height,
                                            ToDxgiFormat(static_cast<FfxApiSurfaceFormat>(resource.description.format)),
                                            static_cast<UINT16>(resource.description.mipCount));
  resource.texture->SetName(description->name);
  output->internalIndex = static_cast<int32_t>(index);

  // The SDK's Lanczos table has initial texels. Upload it once in the first
  // evaluation list, retaining the small staging allocation with its owner.
  if (description->initData.type != FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED) {
    if (resource.description.mipCount != 1u) return FFX_ERROR_INVALID_ARGUMENT;
    const auto texture_description = resource.texture->GetDesc();
    UINT64 bytes = 0u;
    backend->device->GetCopyableFootprints(&texture_description, 0u, 1u, 0u, &resource.footprint, nullptr, nullptr, &bytes);
    const D3D12_HEAP_PROPERTIES heap = {.Type = D3D12_HEAP_TYPE_UPLOAD};
    const D3D12_RESOURCE_DESC buffer = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Width = bytes,
        .Height = 1u,
        .DepthOrArraySize = 1u,
        .MipLevels = 1u,
        .SampleDesc = {.Count = 1u},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
    };
    compute::Check(backend->device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                            nullptr, IID_PPV_ARGS(&resource.initial_upload)),
                   "Create FSR table upload");
    uint8_t* mapped = nullptr;
    const D3D12_RANGE no_reads = {};
    compute::Check(resource.initial_upload->Map(0u, &no_reads, reinterpret_cast<void**>(&mapped)), "Map FSR table upload");
    const UINT row_bytes = resource.description.width * BytesPerPixel(static_cast<FfxApiSurfaceFormat>(resource.description.format));
    for (UINT row = 0u; row < resource.description.height; ++row) {
      auto* destination = mapped + (row * resource.footprint.Footprint.RowPitch);
      if (description->initData.type == FFX_RESOURCE_INIT_DATA_TYPE_BUFFER) {
        std::memcpy(destination, static_cast<const uint8_t*>(description->initData.buffer) + (row * row_bytes), row_bytes);
      } else {
        std::memset(destination, description->initData.value, row_bytes);
      }
    }
    resource.initial_upload->Unmap(0u, nullptr);
    resource.upload_pending = true;
  }
  return FFX_OK;
}

inline FfxErrorCode DestroyResource(FfxInterface* backend_interface, FfxResourceInternal resource, FfxUInt32 /*context_id*/) {
  GetBackend(backend_interface)->resources.at(resource.internalIndex) = {};
  return FFX_OK;
}

inline FfxErrorCode RegisterResource(FfxInterface* backend_interface, const FfxApiResource* input, FfxUInt32 /*context_id*/, FfxResourceInternal* output) {
  auto* backend = GetBackend(backend_interface);
  output->internalIndex = 0;
  if (input->resource == nullptr) return FFX_OK;
  // An input can appear under several SDK identifiers. All of those bindings
  // must share one tracked state, rather than issuing contradictory barriers.
  for (uint32_t index = backend->next_dynamic + 1u; index < backend->resources.size(); ++index) {
    if (backend->resources[index].texture.Get() == input->resource) {
      output->internalIndex = static_cast<int32_t>(index);
      return FFX_OK;
    }
  }
  if (backend->next_dynamic <= backend->next_static) return FFX_ERROR_INSUFFICIENT_MEMORY;
  output->internalIndex = static_cast<int32_t>(backend->next_dynamic--);
  auto& resource = backend->resources[output->internalIndex];
  resource.texture = static_cast<ID3D12Resource*>(input->resource);
  resource.description = input->description;
  resource.state = resource.initial_state = GetState(input->state);
  return FFX_OK;
}

inline FfxApiResource GetResource(FfxInterface* backend_interface, FfxResourceInternal resource) {
  const auto& entry = GetBackend(backend_interface)->resources.at(resource.internalIndex);
  return {.resource = entry.texture.Get(), .description = entry.description, .state = FFX_API_RESOURCE_STATE_COMMON};
}

inline FfxApiResourceDescription GetDescription(FfxInterface* backend_interface, FfxResourceInternal resource) {
  return GetBackend(backend_interface)->resources.at(resource.internalIndex).description;
}

inline FfxErrorCode UnregisterResources(FfxInterface* backend_interface, FfxCommandList command_list, FfxUInt32 /*context_id*/) {
  auto* backend = GetBackend(backend_interface);
  auto* commands = static_cast<ID3D12GraphicsCommandList*>(command_list);
  for (uint32_t index = backend->next_dynamic + 1u; index < backend->resources.size(); ++index) {
    auto& resource = backend->resources[index];
    Backend::Transition(commands, &resource, resource.initial_state);
    resource = {};
  }
  backend->next_dynamic = FFX_MAX_RESOURCE_COUNT - 1u;
  return FFX_OK;
}

inline FfxErrorCode StageConstants(FfxInterface* backend_interface, void* data, FfxUInt32 size, FfxConstantBuffer* output) {
  auto* backend = GetBackend(backend_interface);
  const uint32_t aligned = (size + 15u) & ~15u;
  if (aligned > backend->constants.size() - backend->constant_cursor) return FFX_ERROR_INSUFFICIENT_MEMORY;
  auto* destination = backend->constants.data() + backend->constant_cursor;
  std::memcpy(destination, data, size);
  output->data = reinterpret_cast<uint32_t*>(destination);
  output->num32BitEntries = size / sizeof(uint32_t);
  backend->constant_cursor += aligned;
  return FFX_OK;
}

inline FfxErrorCode CreatePipeline(FfxInterface* backend_interface, FfxShaderBlob* blob, const FfxPipelineDescription* description, FfxUInt32 /*context_id*/, FfxPipelineState* output) {
  auto pipeline = std::make_unique<compute::Pipeline>();
  pipeline->Create(GetBackend(backend_interface)->device, {blob->data, blob->size}, MAX_COMPUTE_SRVS, MAX_COMPUTE_UAVS, FFX_MAX_NUM_CONST_BUFFERS);
  *output = {};
  output->srvTextureCount = blob->srvTextureCount;
  output->uavTextureCount = blob->uavTextureCount;
  output->constCount = blob->cbvCount;
  CopyBindings(output->srvTextureBindings, output->srvTextureCount, blob->boundSRVTextureNames, blob->boundSRVTextures);
  CopyBindings(output->uavTextureBindings, output->uavTextureCount, blob->boundUAVTextureNames, blob->boundUAVTextures);
  CopyBindings(output->constantBufferBindings, output->constCount, blob->boundConstantBufferNames, blob->boundConstantBuffers);
  wcsncpy_s(output->name, description->name, _TRUNCATE);
  output->pipeline = pipeline.release();
  return FFX_OK;
}

inline FfxErrorCode DestroyPipeline(FfxInterface* /*backend_interface*/, FfxPipelineState* pipeline, FfxUInt32 /*context_id*/) {
  delete static_cast<compute::Pipeline*>(pipeline->pipeline);
  *pipeline = {};
  return FFX_OK;
}

inline FfxErrorCode ScheduleJob(FfxInterface* backend_interface, const FfxGpuJobDescription* job) {
  auto* backend = GetBackend(backend_interface);
  if (backend->jobs.size() >= FFX_MAX_GPU_JOBS) return FFX_ERROR_INSUFFICIENT_MEMORY;
  backend->jobs.push_back(*job);
  return FFX_OK;
}

inline void WriteUav(const Backend& backend, const Resource& resource, UINT mip, D3D12_CPU_DESCRIPTOR_HANDLE handle) {
  const D3D12_UNORDERED_ACCESS_VIEW_DESC description = {
      .Format = ToDxgiFormat(static_cast<FfxApiSurfaceFormat>(resource.description.format)),
      .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
      .Texture2D = {.MipSlice = mip},
  };
  backend.device->CreateUnorderedAccessView(resource.texture.Get(), nullptr, &description, handle);
}

inline FfxErrorCode ExecuteJobs(FfxInterface* backend_interface, FfxCommandList command_list, FfxUInt32 /*context_id*/) {
  auto* backend = GetBackend(backend_interface);
  auto* commands = static_cast<ID3D12GraphicsCommandList*>(command_list);
  if (backend->frame == nullptr || commands == nullptr) return FFX_ERROR_INVALID_POINTER;
  for (auto& resource : backend->resources) {
    if (!resource.upload_pending) continue;
    Backend::Transition(commands, &resource, D3D12_RESOURCE_STATE_COPY_DEST);
    const D3D12_TEXTURE_COPY_LOCATION source = {.pResource = resource.initial_upload.Get(), .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, .PlacedFootprint = resource.footprint};
    const D3D12_TEXTURE_COPY_LOCATION destination = {.pResource = resource.texture.Get(), .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
    commands->CopyTextureRegion(&destination, 0u, 0u, 0u, &source, nullptr);
    resource.upload_pending = false;
  }
  for (const auto& job : backend->jobs) {
    switch (job.jobType) {
      case FFX_GPU_JOB_CLEAR_FLOAT: {
        auto& resource = backend->resources.at(job.clearJobDescriptor.target.internalIndex);
        Backend::Transition(commands, &resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        const UINT index = backend->frame->Allocate(1u);
        const D3D12_CPU_DESCRIPTOR_HANDLE cpu = {backend->frame->clear_descriptors->GetCPUDescriptorHandleForHeapStart().ptr + (static_cast<SIZE_T>(index) * backend->frame->increment)};
        WriteUav(*backend, resource, 0u, cpu);
        backend->device->CopyDescriptorsSimple(1u, backend->frame->Cpu(index), cpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        commands->SetDescriptorHeaps(1u, backend->frame->descriptors.GetAddressOf());
        if (resource.description.format == FFX_API_SURFACE_FORMAT_R32_UINT) {
          const std::array<UINT, 4> values = {std::bit_cast<UINT>(job.clearJobDescriptor.color[0]), std::bit_cast<UINT>(job.clearJobDescriptor.color[1]),
                                              std::bit_cast<UINT>(job.clearJobDescriptor.color[2]), std::bit_cast<UINT>(job.clearJobDescriptor.color[3])};
          commands->ClearUnorderedAccessViewUint(backend->frame->Gpu(index), cpu, resource.texture.Get(), values.data(), 0u, nullptr);
        } else {
          commands->ClearUnorderedAccessViewFloat(backend->frame->Gpu(index), cpu, resource.texture.Get(), job.clearJobDescriptor.color, 0u, nullptr);
        }
        break;
      }
      case FFX_GPU_JOB_COMPUTE: {
        const auto& computation = job.computeJobDescriptor;
        auto* pipeline = static_cast<compute::Pipeline*>(computation.pipeline->pipeline);
        const UINT first = backend->frame->Allocate(MAX_COMPUTE_SRVS + MAX_COMPUTE_UAVS);
        // Unused table entries are real null descriptors, never stale views from a
        // preceding SDK pass. Only declared bindings receive resource transitions.
        const D3D12_SHADER_RESOURCE_VIEW_DESC null_srv = {
            .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D = {.MipLevels = 1u},
        };
        const D3D12_UNORDERED_ACCESS_VIEW_DESC null_uav = {.Format = DXGI_FORMAT_R16G16B16A16_FLOAT, .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D};
        for (UINT index = 0u; index < MAX_COMPUTE_SRVS; ++index) {
          backend->device->CreateShaderResourceView(nullptr, &null_srv, backend->frame->Cpu(first + index));
        }
        for (UINT index = 0u; index < MAX_COMPUTE_UAVS; ++index) {
          backend->device->CreateUnorderedAccessView(nullptr, nullptr, &null_uav, backend->frame->Cpu(first + MAX_COMPUTE_SRVS + index));
        }
        for (UINT index = 0u; index < computation.pipeline->srvTextureCount; ++index) {
          auto& resource = backend->resources.at(computation.srvTextures[index].resource.internalIndex);
          if (resource.texture == nullptr) continue;
          Backend::Transition(commands, &resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
          const auto& binding = computation.pipeline->srvTextureBindings[index];
          backend->device->CreateShaderResourceView(resource.texture.Get(), nullptr, backend->frame->Cpu(first + binding.slotIndex + binding.arrayIndex));
        }
        for (UINT index = 0u; index < computation.pipeline->uavTextureCount; ++index) {
          auto& resource = backend->resources.at(computation.uavTextures[index].resource.internalIndex);
          if (resource.texture == nullptr) continue;
          Backend::Transition(commands, &resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
          const auto& binding = computation.pipeline->uavTextureBindings[index];
          WriteUav(*backend, resource, computation.uavTextures[index].mip, backend->frame->Cpu(first + MAX_COMPUTE_SRVS + binding.slotIndex + binding.arrayIndex));
        }
        pipeline->Bind(commands, *backend->frame, first);
        for (UINT index = 0u; index < computation.pipeline->constCount; ++index) {
          commands->SetComputeRootConstantBufferView(computation.pipeline->constantBufferBindings[index].slotIndex + 1u,
                                                     backend->frame->Upload(computation.cbs[index].data, computation.cbs[index].num32BitEntries * sizeof(uint32_t)));
        }
        commands->Dispatch(computation.dimensions[0], computation.dimensions[1], computation.dimensions[2]);
        break;
      }
      case FFX_GPU_JOB_COPY: {
        auto& source = backend->resources.at(job.copyJobDescriptor.src.internalIndex);
        auto& destination = backend->resources.at(job.copyJobDescriptor.dst.internalIndex);
        Backend::Transition(commands, &source, D3D12_RESOURCE_STATE_COPY_SOURCE);
        Backend::Transition(commands, &destination, D3D12_RESOURCE_STATE_COPY_DEST);
        commands->CopyResource(destination.texture.Get(), source.texture.Get());
        break;
      }
      case FFX_GPU_JOB_BARRIER:
        Backend::Transition(commands, &backend->resources.at(job.barrierDescriptor.resource.internalIndex), GetState(job.barrierDescriptor.newState));
        break;
      case FFX_GPU_JOB_DISCARD:
        // These textures have dedicated allocations. Retaining their contents
        // is safe; the following SDK pass writes the required region.
        break;
      default: return FFX_ERROR_INVALID_ENUM;
    }
  }
  backend->jobs.clear();
  backend->constant_cursor = 0u;
  return FFX_OK;
}

inline FfxApiResource Describe(ID3D12Resource* texture, FfxApiResourceState state) {
  const auto description = texture->GetDesc();
  return {
      .resource = texture,
      .description = {
          .type = FFX_API_RESOURCE_TYPE_TEXTURE2D,
          .format = static_cast<uint32_t>(FromDxgiFormat(description.Format)),
          .width = static_cast<UINT>(description.Width),
          .height = description.Height,
          .depth = 1u,
          .mipCount = description.MipLevels,
          .usage = static_cast<uint32_t>((description.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0u ? FFX_API_RESOURCE_USAGE_UAV : FFX_API_RESOURCE_USAGE_READ_ONLY),
      },
      .state = static_cast<uint32_t>(state),
  };
}

}  // namespace mgsv::reconstruction::fsr3::backend

inline FfxErrorCode GetResourceSizeFromDescription(
    FfxDevice /*device*/,
    const FfxCreateResourceDescription* create_resource_description,
    uint64_t* size_in_bytes,
    uint64_t* alignment) {
  if (create_resource_description == nullptr || size_in_bytes == nullptr) return FFX_ERROR_INVALID_POINTER;
  *size_in_bytes = mgsv::reconstruction::fsr3::backend::EstimateSize(create_resource_description->resourceDescription);
  if (*size_in_bytes == 0u) return FFX_ERROR_INVALID_ARGUMENT;
  if (alignment != nullptr) {
    *alignment = 1u;
  }
  return FFX_OK;
}

#endif  // RENODX_GAMES_MGSV_RECONSTRUCTION_FSR3_BACKEND_DX12_HPP_
