// D3D11 backend for FidelityFX SDK 2.3.0.
// Based on the MIT-licensed MapleHinata FidelityFX D3D11 backend design,
// ported to the current interface and MGSV's FL11_0 requirements.

#include "backend_dx11.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

#include <d3d11_1.h>

#include "ffx/api/internal/ffx_assert.h"
#include "ffx/api/internal/ffx_util.h"

namespace taa::fsr3::dx11 {
namespace {

constexpr uint32_t kMaxMipViews = 16u;
constexpr uint32_t kMaxComputeSrvs = 16u;
constexpr uint32_t kMaxComputeUavs = 8u;

struct Resource {
  ID3D11Resource* resource = nullptr;
  ID3D11ShaderResourceView* srv = nullptr;
  std::array<ID3D11UnorderedAccessView*, kMaxMipViews> uavs = {};
  FfxApiResourceDescription description = {};
  uint64_t estimated_size = 0u;
  bool integer_format = false;
  bool reserved = false;
};

struct EffectContext {
  FfxEffect effect = FFX_EFFECT_FSR3UPSCALER;
  uint32_t next_static_resource = 0u;
  uint32_t next_dynamic_resource = 0u;
  FfxApiEffectMemoryUsage memory_usage = {};
  bool active = false;
};

struct BackendContext {
  uint32_t reference_count = 0u;
  uint32_t max_effect_contexts = 0u;
  ID3D11Device* device = nullptr;
  std::array<ID3D11SamplerState*, 2> samplers = {};
  std::array<ID3D11Buffer*, FFX_MAX_NUM_CONST_BUFFERS> constant_buffers = {};
  std::array<uint32_t, FFX_MAX_NUM_CONST_BUFFERS> constant_buffer_sizes = {};
  FfxGpuJobDescription* gpu_jobs = nullptr;
  Resource* resources = nullptr;
  EffectContext* effect_contexts = nullptr;
  uint8_t* constant_staging = nullptr;
  uint32_t gpu_job_count = 0u;
  uint32_t constant_staging_offset = 0u;
};

uint32_t AlignUp(uint32_t value, uint32_t alignment) {
  return (value + alignment - 1u) & ~(alignment - 1u);
}

uint32_t FullMipCount(uint32_t width, uint32_t height) {
  uint32_t count = 1u;
  uint32_t extent = std::max(width, height);
  while (extent > 1u) {
    extent >>= 1u;
    ++count;
  }
  return count;
}

DXGI_FORMAT ToDxgiFormat(FfxApiSurfaceFormat format) {
  switch (format) {
    case FFX_API_SURFACE_FORMAT_R32G32B32A32_TYPELESS: return DXGI_FORMAT_R32G32B32A32_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R32G32B32A32_UINT: return DXGI_FORMAT_R32G32B32A32_UINT;
    case FFX_API_SURFACE_FORMAT_R32G32B32A32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case FFX_API_SURFACE_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case FFX_API_SURFACE_FORMAT_R32G32B32_FLOAT: return DXGI_FORMAT_R32G32B32_FLOAT;
    case FFX_API_SURFACE_FORMAT_R32G32_TYPELESS: return DXGI_FORMAT_R32G32_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R32G32_FLOAT: return DXGI_FORMAT_R32G32_FLOAT;
    case FFX_API_SURFACE_FORMAT_R32G32_UINT: return DXGI_FORMAT_R32G32_UINT;
    case FFX_API_SURFACE_FORMAT_R32_TYPELESS: return DXGI_FORMAT_R32_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R32_UINT: return DXGI_FORMAT_R32_UINT;
    case FFX_API_SURFACE_FORMAT_R32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
    case FFX_API_SURFACE_FORMAT_R16G16_TYPELESS: return DXGI_FORMAT_R16G16_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R16G16_FLOAT: return DXGI_FORMAT_R16G16_FLOAT;
    case FFX_API_SURFACE_FORMAT_R16G16_UINT: return DXGI_FORMAT_R16G16_UINT;
    case FFX_API_SURFACE_FORMAT_R16G16_SINT: return DXGI_FORMAT_R16G16_SINT;
    case FFX_API_SURFACE_FORMAT_R16_TYPELESS: return DXGI_FORMAT_R16_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R16_FLOAT: return DXGI_FORMAT_R16_FLOAT;
    case FFX_API_SURFACE_FORMAT_R16_UINT: return DXGI_FORMAT_R16_UINT;
    case FFX_API_SURFACE_FORMAT_R16_UNORM: return DXGI_FORMAT_R16_UNORM;
    case FFX_API_SURFACE_FORMAT_R16_SNORM: return DXGI_FORMAT_R16_SNORM;
    case FFX_API_SURFACE_FORMAT_R10G10B10A2_TYPELESS: return DXGI_FORMAT_R10G10B10A2_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R10G10B10A2_UNORM: return DXGI_FORMAT_R10G10B10A2_UNORM;
    case FFX_API_SURFACE_FORMAT_R11G11B10_FLOAT: return DXGI_FORMAT_R11G11B10_FLOAT;
    case FFX_API_SURFACE_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case FFX_API_SURFACE_FORMAT_R8G8B8A8_SNORM: return DXGI_FORMAT_R8G8B8A8_SNORM;
    case FFX_API_SURFACE_FORMAT_R8G8B8A8_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case FFX_API_SURFACE_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_TYPELESS;
    case FFX_API_SURFACE_FORMAT_B8G8R8A8_UNORM: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case FFX_API_SURFACE_FORMAT_B8G8R8A8_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case FFX_API_SURFACE_FORMAT_R8G8_TYPELESS: return DXGI_FORMAT_R8G8_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R8G8_UNORM: return DXGI_FORMAT_R8G8_UNORM;
    case FFX_API_SURFACE_FORMAT_R8G8_UINT: return DXGI_FORMAT_R8G8_UINT;
    case FFX_API_SURFACE_FORMAT_R8_TYPELESS: return DXGI_FORMAT_R8_TYPELESS;
    case FFX_API_SURFACE_FORMAT_R8_UNORM: return DXGI_FORMAT_R8_UNORM;
    case FFX_API_SURFACE_FORMAT_R8_SNORM: return DXGI_FORMAT_R8_SNORM;
    case FFX_API_SURFACE_FORMAT_R8_UINT: return DXGI_FORMAT_R8_UINT;
    case FFX_API_SURFACE_FORMAT_R9G9B9E5_SHAREDEXP: return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
    default: return DXGI_FORMAT_UNKNOWN;
  }
}

FfxApiSurfaceFormat FromDxgiFormat(DXGI_FORMAT format) {
  switch (format) {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS: return FFX_API_SURFACE_FORMAT_R32G32B32A32_TYPELESS;
    case DXGI_FORMAT_R32G32B32A32_UINT: return FFX_API_SURFACE_FORMAT_R32G32B32A32_UINT;
    case DXGI_FORMAT_R32G32B32A32_FLOAT: return FFX_API_SURFACE_FORMAT_R32G32B32A32_FLOAT;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: return FFX_API_SURFACE_FORMAT_R16G16B16A16_TYPELESS;
    case DXGI_FORMAT_R16G16B16A16_FLOAT: return FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT;
    case DXGI_FORMAT_R32G32B32_FLOAT: return FFX_API_SURFACE_FORMAT_R32G32B32_FLOAT;
    case DXGI_FORMAT_R32G32_TYPELESS: return FFX_API_SURFACE_FORMAT_R32G32_TYPELESS;
    case DXGI_FORMAT_R32G32_FLOAT: return FFX_API_SURFACE_FORMAT_R32G32_FLOAT;
    case DXGI_FORMAT_R32G32_UINT: return FFX_API_SURFACE_FORMAT_R32G32_UINT;
    case DXGI_FORMAT_R32_TYPELESS: return FFX_API_SURFACE_FORMAT_R32_TYPELESS;
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT: return FFX_API_SURFACE_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_R32_UINT: return FFX_API_SURFACE_FORMAT_R32_UINT;
    case DXGI_FORMAT_R16G16_TYPELESS: return FFX_API_SURFACE_FORMAT_R16G16_TYPELESS;
    case DXGI_FORMAT_R16G16_FLOAT: return FFX_API_SURFACE_FORMAT_R16G16_FLOAT;
    case DXGI_FORMAT_R16G16_UINT: return FFX_API_SURFACE_FORMAT_R16G16_UINT;
    case DXGI_FORMAT_R16G16_SINT: return FFX_API_SURFACE_FORMAT_R16G16_SINT;
    case DXGI_FORMAT_R16_TYPELESS: return FFX_API_SURFACE_FORMAT_R16_TYPELESS;
    case DXGI_FORMAT_R16_FLOAT: return FFX_API_SURFACE_FORMAT_R16_FLOAT;
    case DXGI_FORMAT_R16_UINT: return FFX_API_SURFACE_FORMAT_R16_UINT;
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM: return FFX_API_SURFACE_FORMAT_R16_UNORM;
    case DXGI_FORMAT_R16_SNORM: return FFX_API_SURFACE_FORMAT_R16_SNORM;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: return FFX_API_SURFACE_FORMAT_R10G10B10A2_TYPELESS;
    case DXGI_FORMAT_R10G10B10A2_UNORM: return FFX_API_SURFACE_FORMAT_R10G10B10A2_UNORM;
    case DXGI_FORMAT_R11G11B10_FLOAT: return FFX_API_SURFACE_FORMAT_R11G11B10_FLOAT;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: return FFX_API_SURFACE_FORMAT_R8G8B8A8_TYPELESS;
    case DXGI_FORMAT_R8G8B8A8_UNORM: return FFX_API_SURFACE_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return FFX_API_SURFACE_FORMAT_R8G8B8A8_SRGB;
    case DXGI_FORMAT_R8G8B8A8_SNORM: return FFX_API_SURFACE_FORMAT_R8G8B8A8_SNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS: return FFX_API_SURFACE_FORMAT_B8G8R8A8_TYPELESS;
    case DXGI_FORMAT_B8G8R8A8_UNORM: return FFX_API_SURFACE_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return FFX_API_SURFACE_FORMAT_B8G8R8A8_SRGB;
    case DXGI_FORMAT_R8G8_TYPELESS: return FFX_API_SURFACE_FORMAT_R8G8_TYPELESS;
    case DXGI_FORMAT_R8G8_UNORM: return FFX_API_SURFACE_FORMAT_R8G8_UNORM;
    case DXGI_FORMAT_R8G8_UINT: return FFX_API_SURFACE_FORMAT_R8G8_UINT;
    case DXGI_FORMAT_R8_TYPELESS: return FFX_API_SURFACE_FORMAT_R8_TYPELESS;
    case DXGI_FORMAT_R8_UNORM: return FFX_API_SURFACE_FORMAT_R8_UNORM;
    case DXGI_FORMAT_R8_SNORM: return FFX_API_SURFACE_FORMAT_R8_SNORM;
    case DXGI_FORMAT_R8_UINT: return FFX_API_SURFACE_FORMAT_R8_UINT;
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP: return FFX_API_SURFACE_FORMAT_R9G9B9E5_SHAREDEXP;
    default: return FFX_API_SURFACE_FORMAT_UNKNOWN;
  }
}

uint32_t BytesPerPixel(FfxApiSurfaceFormat format) {
  switch (format) {
    case FFX_API_SURFACE_FORMAT_R32G32B32A32_TYPELESS:
    case FFX_API_SURFACE_FORMAT_R32G32B32A32_UINT:
    case FFX_API_SURFACE_FORMAT_R32G32B32A32_FLOAT: return 16u;
    case FFX_API_SURFACE_FORMAT_R32G32B32_FLOAT: return 12u;
    case FFX_API_SURFACE_FORMAT_R16G16B16A16_TYPELESS:
    case FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT:
    case FFX_API_SURFACE_FORMAT_R32G32_TYPELESS:
    case FFX_API_SURFACE_FORMAT_R32G32_FLOAT:
    case FFX_API_SURFACE_FORMAT_R32G32_UINT: return 8u;
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
    case FFX_API_SURFACE_FORMAT_R9G9B9E5_SHAREDEXP: return 4u;
    case FFX_API_SURFACE_FORMAT_R16_TYPELESS:
    case FFX_API_SURFACE_FORMAT_R16_FLOAT:
    case FFX_API_SURFACE_FORMAT_R16_UINT:
    case FFX_API_SURFACE_FORMAT_R16_UNORM:
    case FFX_API_SURFACE_FORMAT_R16_SNORM:
    case FFX_API_SURFACE_FORMAT_R8G8_TYPELESS:
    case FFX_API_SURFACE_FORMAT_R8G8_UNORM:
    case FFX_API_SURFACE_FORMAT_R8G8_UINT: return 2u;
    case FFX_API_SURFACE_FORMAT_R8_TYPELESS:
    case FFX_API_SURFACE_FORMAT_R8_UNORM:
    case FFX_API_SURFACE_FORMAT_R8_SNORM:
    case FFX_API_SURFACE_FORMAT_R8_UINT: return 1u;
    default: return 0u;
  }
}

uint64_t EstimateSize(const FfxApiResourceDescription& description) {
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

DXGI_FORMAT ViewFormat(DXGI_FORMAT native_format, FfxApiSurfaceFormat requested_format, bool uav) {
  DXGI_FORMAT format = native_format;
  switch (native_format) {
    case DXGI_FORMAT_UNKNOWN:
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
      format = ToDxgiFormat(requested_format);
      break;
    case DXGI_FORMAT_D32_FLOAT: format = DXGI_FORMAT_R32_FLOAT; break;
    case DXGI_FORMAT_D16_UNORM: format = DXGI_FORMAT_R16_UNORM; break;
    case DXGI_FORMAT_D24_UNORM_S8_UINT: format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; break;
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS; break;
    default: break;
  }
  if (uav && format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) return DXGI_FORMAT_R8G8B8A8_UNORM;
  if (uav && format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) return DXGI_FORMAT_B8G8R8A8_UNORM;
  return format;
}

void ReleaseResource(Resource* resource) {
  if (resource == nullptr) return;
  if (resource->srv != nullptr) resource->srv->Release();
  for (auto*& uav : resource->uavs) {
    if (uav != nullptr) uav->Release();
  }
  if (resource->resource != nullptr) resource->resource->Release();
  *resource = {};
}

FfxErrorCode CreateViews(BackendContext* context, Resource* resource) {
  if (context == nullptr || resource == nullptr || resource->resource == nullptr) return FFX_ERROR_INVALID_POINTER;

  D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
  resource->resource->GetType(&dimension);
  if (dimension != D3D11_RESOURCE_DIMENSION_TEXTURE2D) return FFX_ERROR_INVALID_ARGUMENT;

  D3D11_TEXTURE2D_DESC texture_desc = {};
  reinterpret_cast<ID3D11Texture2D*>(resource->resource)->GetDesc(&texture_desc);
  const auto requested_format = static_cast<FfxApiSurfaceFormat>(resource->description.format);

  if ((texture_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0u) {
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = ViewFormat(texture_desc.Format, requested_format, false);
    if (texture_desc.ArraySize > 1u) {
      srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
      srv_desc.Texture2DArray.MostDetailedMip = 0u;
      srv_desc.Texture2DArray.MipLevels = texture_desc.MipLevels;
      srv_desc.Texture2DArray.FirstArraySlice = 0u;
      srv_desc.Texture2DArray.ArraySize = texture_desc.ArraySize;
    } else {
      srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
      srv_desc.Texture2D.MostDetailedMip = 0u;
      srv_desc.Texture2D.MipLevels = texture_desc.MipLevels;
    }
    if (FAILED(context->device->CreateShaderResourceView(resource->resource, &srv_desc, &resource->srv))) {
      return FFX_ERROR_BACKEND_API_ERROR;
    }
  }

  if ((texture_desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) != 0u) {
    const uint32_t mip_count = std::min<uint32_t>(texture_desc.MipLevels, kMaxMipViews);
    for (uint32_t mip = 0u; mip < mip_count; ++mip) {
      D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
      uav_desc.Format = ViewFormat(texture_desc.Format, requested_format, true);
      if (texture_desc.ArraySize > 1u) {
        uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
        uav_desc.Texture2DArray.MipSlice = mip;
        uav_desc.Texture2DArray.FirstArraySlice = 0u;
        uav_desc.Texture2DArray.ArraySize = texture_desc.ArraySize;
      } else {
        uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        uav_desc.Texture2D.MipSlice = mip;
      }
      if (FAILED(context->device->CreateUnorderedAccessView(resource->resource, &uav_desc, &resource->uavs[mip]))) {
        return FFX_ERROR_BACKEND_API_ERROR;
      }
    }
  }

  resource->integer_format = requested_format == FFX_API_SURFACE_FORMAT_R32_UINT
                             || requested_format == FFX_API_SURFACE_FORMAT_R16_UINT
                             || requested_format == FFX_API_SURFACE_FORMAT_R16G16_UINT
                             || requested_format == FFX_API_SURFACE_FORMAT_R8_UINT
                             || requested_format == FFX_API_SURFACE_FORMAT_R8G8_UINT;
  return FFX_OK;
}

BackendContext* GetContext(FfxInterface* backend_interface) {
  return backend_interface == nullptr ? nullptr : static_cast<BackendContext*>(backend_interface->scratchBuffer);
}

FfxVersionNumber GetSdkVersion(FfxInterface*) {
  return FFX_SDK_MAKE_VERSION(FFX_SDK_VERSION_MAJOR, FFX_SDK_VERSION_MINOR, FFX_SDK_VERSION_PATCH);
}

FfxErrorCode GetEffectGpuMemoryUsage(
    FfxInterface* backend_interface,
    FfxUInt32 effect_context_id,
    FfxApiEffectMemoryUsage* output) {
  BackendContext* context = GetContext(backend_interface);
  if (context == nullptr || output == nullptr || effect_context_id >= context->max_effect_contexts) {
    return FFX_ERROR_INVALID_POINTER;
  }
  *output = context->effect_contexts[effect_context_id].memory_usage;
  return FFX_OK;
}

FfxErrorCode CreateBackendContext(
    FfxInterface* backend_interface,
    FfxEffect effect,
    FfxEffectBindlessConfig* bindless_config,
    FfxUInt32* effect_context_id) {
  BackendContext* context = GetContext(backend_interface);
  if (context == nullptr || context->device == nullptr || effect_context_id == nullptr) return FFX_ERROR_INVALID_POINTER;
  if (bindless_config != nullptr || effect != FFX_EFFECT_FSR3UPSCALER) return FFX_ERROR_INVALID_ARGUMENT;

  if (context->reference_count == 0u) {
    context->device->AddRef();

    D3D11_SAMPLER_DESC sampler_desc = {};
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampler_desc.MinLOD = 0.f;
    sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    if (FAILED(context->device->CreateSamplerState(&sampler_desc, &context->samplers[0]))) {
      context->device->Release();
      return FFX_ERROR_BACKEND_API_ERROR;
    }
    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    if (FAILED(context->device->CreateSamplerState(&sampler_desc, &context->samplers[1]))) {
      context->samplers[0]->Release();
      context->samplers[0] = nullptr;
      context->device->Release();
      return FFX_ERROR_BACKEND_API_ERROR;
    }
  }

  for (uint32_t index = 0u; index < context->max_effect_contexts; ++index) {
    EffectContext& effect_context = context->effect_contexts[index];
    if (effect_context.active) continue;
    effect_context = {};
    effect_context.active = true;
    effect_context.effect = effect;
    effect_context.next_static_resource = index * FFX_MAX_RESOURCE_COUNT + 1u;
    effect_context.next_dynamic_resource = (index + 1u) * FFX_MAX_RESOURCE_COUNT - 1u;
    *effect_context_id = index;
    ++context->reference_count;
    return FFX_OK;
  }
  return FFX_ERROR_OUT_OF_RANGE;
}

FfxErrorCode GetDeviceCapabilities(FfxInterface*, FfxDeviceCapabilities* capabilities) {
  if (capabilities == nullptr) return FFX_ERROR_INVALID_POINTER;
  *capabilities = {};
  capabilities->maximumSupportedShaderModel = FFX_SHADER_MODEL_5_1;
  capabilities->dedicatedAllocationSupported = true;
  return FFX_OK;
}

FfxErrorCode DestroyResource(
    FfxInterface* backend_interface,
    FfxResourceInternal internal_resource,
    FfxUInt32 effect_context_id);

FfxErrorCode DestroyBackendContext(FfxInterface* backend_interface, FfxUInt32 effect_context_id) {
  BackendContext* context = GetContext(backend_interface);
  if (context == nullptr || effect_context_id >= context->max_effect_contexts) return FFX_ERROR_INVALID_POINTER;
  EffectContext& effect_context = context->effect_contexts[effect_context_id];
  if (!effect_context.active || context->reference_count == 0u) return FFX_ERROR_INVALID_ARGUMENT;

  const uint32_t begin = effect_context_id * FFX_MAX_RESOURCE_COUNT;
  const uint32_t end = (effect_context_id + 1u) * FFX_MAX_RESOURCE_COUNT;
  for (uint32_t index = begin; index < end; ++index) {
    ReleaseResource(&context->resources[index]);
  }
  effect_context = {};

  --context->reference_count;
  if (context->reference_count == 0u) {
    for (auto*& sampler : context->samplers) {
      if (sampler != nullptr) sampler->Release();
      sampler = nullptr;
    }
    for (auto*& buffer : context->constant_buffers) {
      if (buffer != nullptr) buffer->Release();
      buffer = nullptr;
    }
    context->constant_buffer_sizes = {};
    context->gpu_job_count = 0u;
    context->constant_staging_offset = 0u;
    context->device->Release();
  }
  return FFX_OK;
}

FfxErrorCode CreateResource(
    FfxInterface* backend_interface,
    const FfxCreateResourceDescription* create_description,
    FfxUInt32 effect_context_id,
    FfxResourceInternal* output) {
  BackendContext* context = GetContext(backend_interface);
  if (context == nullptr || create_description == nullptr || output == nullptr
      || effect_context_id >= context->max_effect_contexts) {
    return FFX_ERROR_INVALID_POINTER;
  }
  if (create_description->heapInfo.heapType != FFX_HEAP_TYPE_DEFAULT
      || create_description->heapInfo.usePlacementHeap
      || create_description->resourceDescription.type != FFX_API_RESOURCE_TYPE_TEXTURE2D) {
    return FFX_ERROR_INVALID_ARGUMENT;
  }

  EffectContext& effect_context = context->effect_contexts[effect_context_id];
  const bool initialized = create_description->initData.type != FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED;
  const uint32_t allocation_count = initialized ? 2u : 1u;
  if (effect_context.next_static_resource + allocation_count >= effect_context.next_dynamic_resource) {
    return FFX_ERROR_INSUFFICIENT_MEMORY;
  }

  const uint32_t index = effect_context.next_static_resource;
  effect_context.next_static_resource += allocation_count;
  output->internalIndex = static_cast<int32_t>(index);
  Resource& resource = context->resources[index];
  resource = {};
  resource.description = create_description->resourceDescription;
  if (resource.description.mipCount == 0u) {
    resource.description.mipCount = FullMipCount(resource.description.width, resource.description.height);
  }
  if (initialized) context->resources[index + 1u].reserved = true;

  D3D11_TEXTURE2D_DESC texture_desc = {};
  texture_desc.Width = resource.description.width;
  texture_desc.Height = resource.description.height;
  texture_desc.MipLevels = resource.description.mipCount;
  texture_desc.ArraySize = std::max(1u, resource.description.depth);
  texture_desc.Format = ToDxgiFormat(static_cast<FfxApiSurfaceFormat>(resource.description.format));
  texture_desc.SampleDesc.Count = 1u;
  texture_desc.Usage = D3D11_USAGE_DEFAULT;
  texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  if ((resource.description.usage & FFX_API_RESOURCE_USAGE_UAV) != 0u) {
    texture_desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
  }

  D3D11_SUBRESOURCE_DATA subresource = {};
  D3D11_SUBRESOURCE_DATA* subresource_pointer = nullptr;
  std::vector<uint8_t> value_data;
  if (initialized) {
    if (resource.description.mipCount != 1u || texture_desc.ArraySize != 1u) return FFX_ERROR_INVALID_ARGUMENT;
    const uint32_t row_pitch = resource.description.width
                               * BytesPerPixel(static_cast<FfxApiSurfaceFormat>(resource.description.format));
    if (row_pitch == 0u) return FFX_ERROR_INVALID_ENUM;
    if (create_description->initData.type == FFX_RESOURCE_INIT_DATA_TYPE_BUFFER) {
      subresource.pSysMem = create_description->initData.buffer;
    } else if (create_description->initData.type == FFX_RESOURCE_INIT_DATA_TYPE_VALUE) {
      value_data.resize(static_cast<size_t>(row_pitch) * resource.description.height);
      std::memset(value_data.data(), create_description->initData.value, value_data.size());
      subresource.pSysMem = value_data.data();
    } else {
      return FFX_ERROR_INVALID_ARGUMENT;
    }
    subresource.SysMemPitch = row_pitch;
    subresource.SysMemSlicePitch = row_pitch * resource.description.height;
    subresource_pointer = &subresource;
  }

  ID3D11Texture2D* texture = nullptr;
  if (FAILED(context->device->CreateTexture2D(&texture_desc, subresource_pointer, &texture))) {
    context->resources[index + (initialized ? 1u : 0u)] = {};
    resource = {};
    return FFX_ERROR_BACKEND_API_ERROR;
  }
  resource.resource = texture;
  resource.estimated_size = EstimateSize(resource.description);
  const FfxErrorCode view_result = CreateViews(context, &resource);
  if (view_result != FFX_OK) {
    ReleaseResource(&resource);
    return view_result;
  }

  effect_context.memory_usage.totalUsageInBytes += resource.estimated_size;
  if ((resource.description.flags & FFX_API_RESOURCE_FLAGS_ALIASABLE) != 0u) {
    effect_context.memory_usage.aliasableUsageInBytes += resource.estimated_size;
  }
  return FFX_OK;
}

FfxErrorCode DestroyResource(
    FfxInterface* backend_interface,
    FfxResourceInternal internal_resource,
    FfxUInt32 effect_context_id) {
  BackendContext* context = GetContext(backend_interface);
  if (context == nullptr || effect_context_id >= context->max_effect_contexts) return FFX_ERROR_INVALID_POINTER;
  const int32_t begin = static_cast<int32_t>(effect_context_id * FFX_MAX_RESOURCE_COUNT);
  const int32_t end = static_cast<int32_t>((effect_context_id + 1u) * FFX_MAX_RESOURCE_COUNT);
  if (internal_resource.internalIndex < begin || internal_resource.internalIndex >= end) return FFX_ERROR_OUT_OF_RANGE;

  Resource& resource = context->resources[internal_resource.internalIndex];
  if (resource.reserved) {
    resource = {};
    return FFX_OK;
  }
  EffectContext& effect_context = context->effect_contexts[effect_context_id];
  effect_context.memory_usage.totalUsageInBytes -= std::min(
      effect_context.memory_usage.totalUsageInBytes,
      resource.estimated_size);
  if ((resource.description.flags & FFX_API_RESOURCE_FLAGS_ALIASABLE) != 0u) {
    effect_context.memory_usage.aliasableUsageInBytes -= std::min(
        effect_context.memory_usage.aliasableUsageInBytes,
        resource.estimated_size);
  }
  ReleaseResource(&resource);
  return FFX_OK;
}

FfxErrorCode RegisterResource(
    FfxInterface* backend_interface,
    const FfxApiResource* input,
    FfxUInt32 effect_context_id,
    FfxResourceInternal* output) {
  BackendContext* context = GetContext(backend_interface);
  if (context == nullptr || input == nullptr || output == nullptr
      || effect_context_id >= context->max_effect_contexts) {
    return FFX_ERROR_INVALID_POINTER;
  }
  if (input->resource == nullptr) {
    output->internalIndex = 0;
    return FFX_OK;
  }

  EffectContext& effect_context = context->effect_contexts[effect_context_id];
  if (effect_context.next_dynamic_resource <= effect_context.next_static_resource) {
    return FFX_ERROR_INSUFFICIENT_MEMORY;
  }
  const uint32_t index = effect_context.next_dynamic_resource--;
  output->internalIndex = static_cast<int32_t>(index);
  Resource& resource = context->resources[index];
  ReleaseResource(&resource);
  resource.description = input->description;
  resource.resource = static_cast<ID3D11Resource*>(input->resource);
  resource.resource->AddRef();

  if (resource.description.width == 0u || resource.description.height == 0u) {
    resource.description = GetResourceDescription(resource.resource);
  }
  const FfxErrorCode result = CreateViews(context, &resource);
  if (result != FFX_OK) {
    ReleaseResource(&resource);
    return result;
  }
  return FFX_OK;
}

FfxApiResource GetRegisteredResource(FfxInterface* backend_interface, FfxResourceInternal internal_resource) {
  FfxApiResource output = {};
  BackendContext* context = GetContext(backend_interface);
  if (context == nullptr || internal_resource.internalIndex <= 0) return output;
  const Resource& resource = context->resources[internal_resource.internalIndex];
  output.resource = resource.resource;
  output.description = resource.description;
  output.state = FFX_API_RESOURCE_STATE_COMMON;
  return output;
}

FfxErrorCode UnregisterResources(FfxInterface* backend_interface, FfxCommandList, FfxUInt32 effect_context_id) {
  BackendContext* context = GetContext(backend_interface);
  if (context == nullptr || effect_context_id >= context->max_effect_contexts) return FFX_ERROR_INVALID_POINTER;
  EffectContext& effect_context = context->effect_contexts[effect_context_id];
  const uint32_t end = (effect_context_id + 1u) * FFX_MAX_RESOURCE_COUNT;
  for (uint32_t index = effect_context.next_dynamic_resource + 1u; index < end; ++index) {
    ReleaseResource(&context->resources[index]);
  }
  effect_context.next_dynamic_resource = end - 1u;
  return FFX_OK;
}

FfxApiResourceDescription GetInternalResourceDescription(
    FfxInterface* backend_interface,
    FfxResourceInternal internal_resource) {
  BackendContext* context = GetContext(backend_interface);
  if (context == nullptr || internal_resource.internalIndex < 0) return {};
  return context->resources[internal_resource.internalIndex].description;
}

FfxErrorCode StageConstantBufferData(
    FfxInterface* backend_interface,
    void* data,
    FfxUInt32 size,
    FfxConstantBuffer* constant_buffer) {
  BackendContext* context = GetContext(backend_interface);
  if (context == nullptr || data == nullptr || constant_buffer == nullptr || size == 0u) {
    return FFX_ERROR_INVALID_POINTER;
  }
  const uint32_t aligned_size = AlignUp(size, 256u);
  if (aligned_size > FFX_CONSTANT_BUFFER_RING_BUFFER_SIZE) return FFX_ERROR_INVALID_SIZE;
  if (context->constant_staging_offset + aligned_size > FFX_CONSTANT_BUFFER_RING_BUFFER_SIZE) {
    context->constant_staging_offset = 0u;
  }
  uint8_t* destination = context->constant_staging + context->constant_staging_offset;
  std::memcpy(destination, data, size);
  constant_buffer->data = reinterpret_cast<uint32_t*>(destination);
  constant_buffer->num32BitEntries = size / sizeof(uint32_t);
  context->constant_staging_offset += aligned_size;
  return FFX_OK;
}

void CopyBindingName(wchar_t (&destination)[FFX_RESOURCE_NAME_SIZE], const char* source) {
  destination[0] = L'\0';
  if (source == nullptr) return;
  size_t converted = 0u;
  mbstowcs_s(&converted, destination, source, FFX_RESOURCE_NAME_SIZE - 1u);
}

void CopyBindings(
    FfxResourceBinding* output,
    uint32_t count,
    const char** names,
    const uint32_t* slots) {
  for (uint32_t index = 0u; index < count; ++index) {
    output[index].slotIndex = slots[index];
    output[index].arrayIndex = 0u;
    output[index].resourceIdentifier = 0u;
    CopyBindingName(output[index].name, names[index]);
  }
}

FfxErrorCode CreatePipeline(
    FfxInterface* backend_interface,
    FfxShaderBlob* shader_blob,
    const FfxPipelineDescription* pipeline_description,
    FfxUInt32,
    FfxPipelineState* output) {
  BackendContext* context = GetContext(backend_interface);
  if (context == nullptr || shader_blob == nullptr || pipeline_description == nullptr || output == nullptr) {
    return FFX_ERROR_INVALID_POINTER;
  }
  if (pipeline_description->stage != FFX_BIND_COMPUTE_SHADER_STAGE
      || shader_blob->data == nullptr || shader_blob->size == 0u
      || shader_blob->srvTextureCount > FFX_MAX_NUM_SRVS
      || shader_blob->uavTextureCount > FFX_MAX_NUM_UAVS
      || shader_blob->cbvCount > FFX_MAX_NUM_CONST_BUFFERS) {
    return FFX_ERROR_INVALID_ARGUMENT;
  }

  *output = {};
  ID3D11ComputeShader* shader = nullptr;
  if (FAILED(context->device->CreateComputeShader(shader_blob->data, shader_blob->size, nullptr, &shader))) {
    return FFX_ERROR_BACKEND_API_ERROR;
  }
  output->pipeline = shader;
  output->srvTextureCount = shader_blob->srvTextureCount;
  output->uavTextureCount = shader_blob->uavTextureCount;
  output->srvBufferCount = shader_blob->srvBufferCount;
  output->uavBufferCount = shader_blob->uavBufferCount;
  output->constCount = shader_blob->cbvCount;
  CopyBindings(
      output->srvTextureBindings,
      output->srvTextureCount,
      shader_blob->boundSRVTextureNames,
      shader_blob->boundSRVTextures);
  CopyBindings(
      output->uavTextureBindings,
      output->uavTextureCount,
      shader_blob->boundUAVTextureNames,
      shader_blob->boundUAVTextures);
  CopyBindings(
      output->constantBufferBindings,
      output->constCount,
      shader_blob->boundConstantBufferNames,
      shader_blob->boundConstantBuffers);
  wcsncpy_s(output->name, pipeline_description->name, _TRUNCATE);

  for (uint32_t index = 0u; index < output->srvTextureCount; ++index) {
    output->maxSrvTextureAndBufferIndex = std::max(
        output->maxSrvTextureAndBufferIndex,
        output->srvTextureBindings[index].slotIndex);
  }
  for (uint32_t index = 0u; index < output->uavTextureCount; ++index) {
    output->maxUavTextureAndBufferIndex = std::max(
        output->maxUavTextureAndBufferIndex,
        output->uavTextureBindings[index].slotIndex);
  }
  return FFX_OK;
}

FfxErrorCode DestroyPipeline(FfxInterface*, FfxPipelineState* pipeline, FfxUInt32) {
  if (pipeline == nullptr) return FFX_OK;
  if (pipeline->pipeline != nullptr) static_cast<ID3D11ComputeShader*>(pipeline->pipeline)->Release();
  *pipeline = {};
  return FFX_OK;
}

FfxErrorCode ScheduleGpuJob(FfxInterface* backend_interface, const FfxGpuJobDescription* job) {
  BackendContext* context = GetContext(backend_interface);
  if (context == nullptr || job == nullptr) return FFX_ERROR_INVALID_POINTER;
  if (context->gpu_job_count >= FFX_MAX_GPU_JOBS) return FFX_ERROR_INSUFFICIENT_MEMORY;
  context->gpu_jobs[context->gpu_job_count++] = *job;
  return FFX_OK;
}

FfxErrorCode QueryNextGpuJob(FfxInterface* backend_interface, FfxGpuJobDescription** job) {
  BackendContext* context = GetContext(backend_interface);
  if (context == nullptr || job == nullptr) return FFX_ERROR_INVALID_POINTER;
  if (context->gpu_job_count >= FFX_MAX_GPU_JOBS) return FFX_ERROR_INSUFFICIENT_MEMORY;
  *job = &context->gpu_jobs[context->gpu_job_count++];
  **job = {};
  return FFX_OK;
}

void UnbindComputeResources(ID3D11DeviceContext* device_context) {
  static const std::array<ID3D11ShaderResourceView*, kMaxComputeSrvs> null_srvs = {};
  static const std::array<ID3D11UnorderedAccessView*, kMaxComputeUavs> null_uavs = {};
  device_context->CSSetShaderResources(0u, static_cast<UINT>(null_srvs.size()), null_srvs.data());
  device_context->CSSetUnorderedAccessViews(0u, static_cast<UINT>(null_uavs.size()), null_uavs.data(), nullptr);
}

FfxErrorCode EnsureConstantBuffer(
    BackendContext* context,
    uint32_t slot,
    uint32_t byte_size) {
  if (slot >= context->constant_buffers.size()) return FFX_ERROR_OUT_OF_RANGE;
  const uint32_t aligned_size = AlignUp(byte_size, 16u);
  if (context->constant_buffers[slot] != nullptr && context->constant_buffer_sizes[slot] >= aligned_size) {
    return FFX_OK;
  }
  if (context->constant_buffers[slot] != nullptr) context->constant_buffers[slot]->Release();
  context->constant_buffers[slot] = nullptr;

  D3D11_BUFFER_DESC description = {};
  description.ByteWidth = aligned_size;
  description.Usage = D3D11_USAGE_DYNAMIC;
  description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  if (FAILED(context->device->CreateBuffer(&description, nullptr, &context->constant_buffers[slot]))) {
    context->constant_buffer_sizes[slot] = 0u;
    return FFX_ERROR_BACKEND_API_ERROR;
  }
  context->constant_buffer_sizes[slot] = aligned_size;
  return FFX_OK;
}

FfxErrorCode ExecuteCompute(
    BackendContext* context,
    ID3D11DeviceContext* device_context,
    const FfxComputeJobDescription& job) {
  if (job.pipeline == nullptr || job.pipeline->pipeline == nullptr) return FFX_ERROR_INVALID_POINTER;
  UnbindComputeResources(device_context);

  for (uint32_t index = 0u; index < job.pipeline->srvTextureCount; ++index) {
    const FfxResourceBinding& binding = job.pipeline->srvTextureBindings[index];
    const int32_t resource_index = job.srvTextures[index].resource.internalIndex;
    ID3D11ShaderResourceView* view = resource_index > 0 ? context->resources[resource_index].srv : nullptr;
    const uint32_t slot = binding.slotIndex + binding.arrayIndex;
    device_context->CSSetShaderResources(slot, 1u, &view);
  }
  for (uint32_t index = 0u; index < job.pipeline->uavTextureCount; ++index) {
    const FfxResourceBinding& binding = job.pipeline->uavTextureBindings[index];
    const int32_t resource_index = job.uavTextures[index].resource.internalIndex;
    const uint32_t mip = job.uavTextures[index].mip;
    ID3D11UnorderedAccessView* view = resource_index > 0 && mip < kMaxMipViews
                                         ? context->resources[resource_index].uavs[mip]
                                         : nullptr;
    const uint32_t slot = binding.slotIndex + binding.arrayIndex;
    if (slot >= kMaxComputeUavs) return FFX_ERROR_OUT_OF_RANGE;
    device_context->CSSetUnorderedAccessViews(slot, 1u, &view, nullptr);
  }

  device_context->CSSetSamplers(0u, static_cast<UINT>(context->samplers.size()), context->samplers.data());
  device_context->CSSetShader(static_cast<ID3D11ComputeShader*>(job.pipeline->pipeline), nullptr, 0u);

  for (uint32_t index = 0u; index < job.pipeline->constCount; ++index) {
    const uint32_t slot = job.pipeline->constantBufferBindings[index].slotIndex;
    const uint32_t byte_size = job.cbs[index].num32BitEntries * sizeof(uint32_t);
    FFX_VALIDATE(EnsureConstantBuffer(context, slot, byte_size));

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(device_context->Map(
            context->constant_buffers[slot],
            0u,
            D3D11_MAP_WRITE_DISCARD,
            0u,
            &mapped))) {
      return FFX_ERROR_BACKEND_API_ERROR;
    }
    std::memcpy(mapped.pData, job.cbs[index].data, byte_size);
    device_context->Unmap(context->constant_buffers[slot], 0u);
    device_context->CSSetConstantBuffers(slot, 1u, &context->constant_buffers[slot]);
  }

  device_context->Dispatch(job.dimensions[0], job.dimensions[1], job.dimensions[2]);
  UnbindComputeResources(device_context);
  return FFX_OK;
}

FfxErrorCode ExecuteGpuJobs(
    FfxInterface* backend_interface,
    FfxCommandList command_list,
    FfxUInt32) {
  BackendContext* context = GetContext(backend_interface);
  auto* device_context = static_cast<ID3D11DeviceContext*>(command_list);
  if (context == nullptr || device_context == nullptr) return FFX_ERROR_INVALID_POINTER;

  FfxErrorCode result = FFX_OK;
  for (uint32_t index = 0u; index < context->gpu_job_count && result == FFX_OK; ++index) {
    const FfxGpuJobDescription& job = context->gpu_jobs[index];
    switch (job.jobType) {
      case FFX_GPU_JOB_CLEAR_FLOAT: {
        const Resource& resource = context->resources[job.clearJobDescriptor.target.internalIndex];
        if (resource.uavs[0] == nullptr) {
          result = FFX_ERROR_INVALID_POINTER;
          break;
        }
        if (resource.integer_format) {
          const std::array<uint32_t, 4> values = {
              std::bit_cast<uint32_t>(job.clearJobDescriptor.color[0]),
              std::bit_cast<uint32_t>(job.clearJobDescriptor.color[1]),
              std::bit_cast<uint32_t>(job.clearJobDescriptor.color[2]),
              std::bit_cast<uint32_t>(job.clearJobDescriptor.color[3]),
          };
          device_context->ClearUnorderedAccessViewUint(resource.uavs[0], values.data());
        } else {
          device_context->ClearUnorderedAccessViewFloat(resource.uavs[0], job.clearJobDescriptor.color);
        }
        break;
      }
      case FFX_GPU_JOB_COPY: {
        const Resource& source = context->resources[job.copyJobDescriptor.src.internalIndex];
        const Resource& destination = context->resources[job.copyJobDescriptor.dst.internalIndex];
        if (source.resource == nullptr || destination.resource == nullptr) {
          result = FFX_ERROR_INVALID_POINTER;
        } else {
          UnbindComputeResources(device_context);
          device_context->CopyResource(destination.resource, source.resource);
        }
        break;
      }
      case FFX_GPU_JOB_COMPUTE:
        result = ExecuteCompute(context, device_context, job.computeJobDescriptor);
        break;
      case FFX_GPU_JOB_DISCARD: {
        const Resource& resource = context->resources[job.discardJobDescriptor.target.internalIndex];
        ID3D11DeviceContext1* device_context_1 = nullptr;
        if (resource.resource != nullptr
            && SUCCEEDED(device_context->QueryInterface(IID_PPV_ARGS(&device_context_1)))) {
          device_context_1->DiscardResource(resource.resource);
          device_context_1->Release();
        }
        break;
      }
      case FFX_GPU_JOB_BARRIER:
        // D3D11 hazards are handled by explicit SRV/UAV unbinding around every job.
        break;
      default:
        result = FFX_ERROR_INVALID_ENUM;
        break;
    }
  }
  context->gpu_job_count = 0u;
  context->constant_staging_offset = 0u;
  return result;
}

}  // namespace

size_t GetScratchMemorySize(size_t max_contexts) {
  const size_t jobs_size = max_contexts * FFX_MAX_GPU_JOBS * sizeof(FfxGpuJobDescription);
  const size_t resources_size = max_contexts * FFX_MAX_RESOURCE_COUNT * sizeof(Resource);
  const size_t effects_size = max_contexts * sizeof(EffectContext);
  const size_t staging_size = FFX_CONSTANT_BUFFER_RING_BUFFER_SIZE;
  return sizeof(BackendContext) + AlignUp(static_cast<uint32_t>(jobs_size), 16u)
         + AlignUp(static_cast<uint32_t>(resources_size), 16u)
         + AlignUp(static_cast<uint32_t>(effects_size), 16u) + staging_size;
}

FfxErrorCode GetInterface(
    FfxInterface* backend_interface,
    ID3D11Device* device,
    void* scratch_buffer,
    size_t scratch_buffer_size,
    size_t max_contexts) {
  if (backend_interface == nullptr || device == nullptr || scratch_buffer == nullptr) {
    return FFX_ERROR_INVALID_POINTER;
  }
  if (max_contexts == 0u || scratch_buffer_size < GetScratchMemorySize(max_contexts)) {
    return FFX_ERROR_INSUFFICIENT_MEMORY;
  }

  std::memset(scratch_buffer, 0, scratch_buffer_size);
  std::memset(backend_interface, 0, sizeof(*backend_interface));
  auto* context = static_cast<BackendContext*>(scratch_buffer);
  context->max_effect_contexts = static_cast<uint32_t>(max_contexts);
  context->device = device;

  uint8_t* memory = reinterpret_cast<uint8_t*>(context + 1);
  context->gpu_jobs = reinterpret_cast<FfxGpuJobDescription*>(memory);
  memory += AlignUp(static_cast<uint32_t>(max_contexts * FFX_MAX_GPU_JOBS * sizeof(FfxGpuJobDescription)), 16u);
  context->resources = reinterpret_cast<Resource*>(memory);
  memory += AlignUp(static_cast<uint32_t>(max_contexts * FFX_MAX_RESOURCE_COUNT * sizeof(Resource)), 16u);
  context->effect_contexts = reinterpret_cast<EffectContext*>(memory);
  memory += AlignUp(static_cast<uint32_t>(max_contexts * sizeof(EffectContext)), 16u);
  context->constant_staging = memory;

  backend_interface->fpGetSDKVersion = GetSdkVersion;
  backend_interface->fpGetEffectGpuMemoryUsage = GetEffectGpuMemoryUsage;
  backend_interface->fpCreateBackendContext = CreateBackendContext;
  backend_interface->fpGetDeviceCapabilities = GetDeviceCapabilities;
  backend_interface->fpDestroyBackendContext = DestroyBackendContext;
  backend_interface->fpCreateResource = CreateResource;
  backend_interface->fpRegisterResource = RegisterResource;
  backend_interface->fpGetResource = GetRegisteredResource;
  backend_interface->fpUnregisterResources = UnregisterResources;
  backend_interface->fpGetResourceDescription = GetInternalResourceDescription;
  backend_interface->fpDestroyResource = DestroyResource;
  backend_interface->fpStageConstantBufferDataFunc = StageConstantBufferData;
  backend_interface->fpCreatePipeline = CreatePipeline;
  backend_interface->fpDestroyPipeline = DestroyPipeline;
  backend_interface->fpScheduleGpuJob = ScheduleGpuJob;
  backend_interface->fpExecuteGpuJobs = ExecuteGpuJobs;
  backend_interface->fpQueryNextGpuJobDesc = QueryNextGpuJob;
  backend_interface->scratchBuffer = scratch_buffer;
  backend_interface->scratchBufferSize = scratch_buffer_size;
  backend_interface->device = device;
  return FFX_OK;
}

FfxApiResourceDescription GetResourceDescription(
    ID3D11Resource* resource,
    FfxApiResourceUsage additional_usage) {
  FfxApiResourceDescription output = {};
  if (resource == nullptr) return output;
  D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
  resource->GetType(&dimension);
  if (dimension != D3D11_RESOURCE_DIMENSION_TEXTURE2D) return output;

  D3D11_TEXTURE2D_DESC description = {};
  static_cast<ID3D11Texture2D*>(resource)->GetDesc(&description);
  output.type = FFX_API_RESOURCE_TYPE_TEXTURE2D;
  output.format = FromDxgiFormat(description.Format);
  output.width = description.Width;
  output.height = description.Height;
  output.depth = description.ArraySize;
  output.mipCount = description.MipLevels;
  output.flags = FFX_API_RESOURCE_FLAGS_NONE;
  output.usage = additional_usage;
  if ((description.BindFlags & D3D11_BIND_UNORDERED_ACCESS) != 0u) {
    output.usage |= FFX_API_RESOURCE_USAGE_UAV;
  }
  if ((description.BindFlags & D3D11_BIND_RENDER_TARGET) != 0u) {
    output.usage |= FFX_API_RESOURCE_USAGE_RENDERTARGET;
  }
  if ((description.BindFlags & D3D11_BIND_DEPTH_STENCIL) != 0u) {
    output.usage |= FFX_API_RESOURCE_USAGE_DEPTHTARGET;
  }
  return output;
}

FfxApiResource GetResource(
    ID3D11Resource* resource,
    FfxApiResourceDescription description,
    FfxApiResourceState state) {
  return FfxApiResource{
      .resource = resource,
      .description = description,
      .state = static_cast<uint32_t>(state),
  };
}

}  // namespace taa::fsr3::dx11

FfxErrorCode GetResourceSizeFromDescription(
    FfxDevice,
    const FfxCreateResourceDescription* create_resource_description,
    uint64_t* size_in_bytes,
    uint64_t* alignment) {
  if (create_resource_description == nullptr || size_in_bytes == nullptr) return FFX_ERROR_INVALID_POINTER;
  const FfxApiResourceDescription& description = create_resource_description->resourceDescription;
  const uint32_t bytes_per_pixel = [] (FfxApiSurfaceFormat format) {
    switch (format) {
      case FFX_API_SURFACE_FORMAT_R32G32B32A32_TYPELESS:
      case FFX_API_SURFACE_FORMAT_R32G32B32A32_UINT:
      case FFX_API_SURFACE_FORMAT_R32G32B32A32_FLOAT: return 16u;
      case FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT:
      case FFX_API_SURFACE_FORMAT_R32G32_FLOAT:
      case FFX_API_SURFACE_FORMAT_R32G32_UINT: return 8u;
      case FFX_API_SURFACE_FORMAT_R32_UINT:
      case FFX_API_SURFACE_FORMAT_R32_FLOAT:
      case FFX_API_SURFACE_FORMAT_R16G16_FLOAT:
      case FFX_API_SURFACE_FORMAT_R8G8B8A8_UNORM: return 4u;
      case FFX_API_SURFACE_FORMAT_R16_FLOAT:
      case FFX_API_SURFACE_FORMAT_R16_SNORM: return 2u;
      case FFX_API_SURFACE_FORMAT_R8_UNORM: return 1u;
      default: return 0u;
    }
  }(static_cast<FfxApiSurfaceFormat>(description.format));
  if (bytes_per_pixel == 0u) return FFX_ERROR_INVALID_ENUM;
  uint32_t width = description.width;
  uint32_t height = std::max(1u, description.height);
  uint32_t mip_count = description.mipCount;
  if (mip_count == 0u) {
    mip_count = 1u;
    for (uint32_t extent = std::max(width, height); extent > 1u; extent >>= 1u) ++mip_count;
  }
  uint64_t total = 0u;
  for (uint32_t mip = 0u; mip < mip_count; ++mip) {
    total += static_cast<uint64_t>(std::max(1u, width)) * std::max(1u, height)
             * std::max(1u, description.depth) * bytes_per_pixel;
    width = std::max(1u, width >> 1u);
    height = std::max(1u, height >> 1u);
  }
  *size_in_bytes = total;
  if (alignment != nullptr) *alignment = 1u;
  return FFX_OK;
}
