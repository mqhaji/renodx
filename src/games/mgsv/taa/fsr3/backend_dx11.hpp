#pragma once

#include <cstddef>
#include <cstdint>

#include <d3d11.h>

#include "ffx/api/internal/ffx_interface.h"

namespace taa::fsr3::dx11 {

size_t GetScratchMemorySize(size_t max_contexts);

FfxErrorCode GetInterface(
    FfxInterface* backend_interface,
    ID3D11Device* device,
    void* scratch_buffer,
    size_t scratch_buffer_size,
    size_t max_contexts);

FfxApiResourceDescription GetResourceDescription(
    ID3D11Resource* resource,
    FfxApiResourceUsage additional_usage = FFX_API_RESOURCE_USAGE_READ_ONLY);

FfxApiResource GetResource(
    ID3D11Resource* resource,
    FfxApiResourceDescription description,
    FfxApiResourceState state = FFX_API_RESOURCE_STATE_COMPUTE_READ);

}  // namespace taa::fsr3::dx11
