#ifndef RENODX_GAMES_MGSV_RENDERING_D3D12_COMPUTE_HPP_
#define RENODX_GAMES_MGSV_RENDERING_D3D12_COMPUTE_HPP_

// Small building blocks for private compute passes. The bridge owns submission
// and retirement; these objects only describe resources and record commands.

#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <d3d12.h>
#include <wrl/client.h>

#include "d3d12_bridge.hpp"

namespace mgsv::rendering::d3d12_compute {

using d3d12_bridge::Check;

inline void Transition(ID3D12GraphicsCommandList* commands, ID3D12Resource* resource,
                       D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after,
                       UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
  if (before == after) {
    if (after != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) return;
    const D3D12_RESOURCE_BARRIER barrier = {.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV, .UAV = {.pResource = resource}};
    commands->ResourceBarrier(1u, &barrier);
  } else {
    const D3D12_RESOURCE_BARRIER barrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Transition = {.pResource = resource, .Subresource = subresource, .StateBefore = before, .StateAfter = after},
    };
    commands->ResourceBarrier(1u, &barrier);
  }
}

inline Microsoft::WRL::ComPtr<ID3D12Resource> CreateTexture(
    ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format, UINT16 mips = 1u) {
  const D3D12_HEAP_PROPERTIES heap = {.Type = D3D12_HEAP_TYPE_DEFAULT};
  const D3D12_RESOURCE_DESC description = {
      .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
      .Width = width,
      .Height = height,
      .DepthOrArraySize = 1u,
      .MipLevels = mips,
      .Format = format,
      .SampleDesc = {.Count = 1u},
      .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
      .Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
  };
  Microsoft::WRL::ComPtr<ID3D12Resource> texture;
  Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
                                        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&texture)),
        "Create compute texture");
  return texture;
}

// One arena belongs to one retired bridge slot. Descriptors and uploaded bytes
// remain untouched until both APIs have finished that slot's previous use.
struct Frame {
  static constexpr UINT DESCRIPTOR_COUNT = 1024u;
  static constexpr UINT UPLOAD_BYTES = 256u * 1024u;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptors;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> clear_descriptors;
  Microsoft::WRL::ComPtr<ID3D12Resource> upload;
  uint8_t* mapped = nullptr;
  UINT increment = 0u;
  UINT descriptor_cursor = 0u;
  UINT upload_cursor = 0u;

  void Reset(ID3D12Device* device) {
    if (descriptors == nullptr) {
      Frame replacement;
      D3D12_DESCRIPTOR_HEAP_DESC heap = {
          .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
          .NumDescriptors = DESCRIPTOR_COUNT,
          .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
      };
      Check(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&replacement.descriptors)), "Create compute descriptors");
      heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
      Check(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&replacement.clear_descriptors)), "Create clear descriptors");
      replacement.increment = device->GetDescriptorHandleIncrementSize(heap.Type);
      const D3D12_HEAP_PROPERTIES properties = {.Type = D3D12_HEAP_TYPE_UPLOAD};
      const D3D12_RESOURCE_DESC buffer = {
          .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
          .Width = UPLOAD_BYTES,
          .Height = 1u,
          .DepthOrArraySize = 1u,
          .MipLevels = 1u,
          .SampleDesc = {.Count = 1u},
          .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
      };
      Check(device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &buffer,
                                            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&replacement.upload)),
            "Create compute upload buffer");
      const D3D12_RANGE no_reads = {};
      Check(replacement.upload->Map(0u, &no_reads, reinterpret_cast<void**>(&replacement.mapped)), "Map compute upload buffer");
      *this = std::move(replacement);
    }
    descriptor_cursor = 0u;
    upload_cursor = 0u;
  }

  [[nodiscard]] UINT Allocate(UINT count) {
    if (count > DESCRIPTOR_COUNT - descriptor_cursor) throw std::runtime_error("Compute descriptor arena exhausted");
    const UINT first = descriptor_cursor;
    descriptor_cursor += count;
    return first;
  }

  [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE Cpu(UINT index) const {
    return {descriptors->GetCPUDescriptorHandleForHeapStart().ptr + (static_cast<SIZE_T>(index) * increment)};
  }

  [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE Gpu(UINT index) const {
    return {descriptors->GetGPUDescriptorHandleForHeapStart().ptr + (static_cast<UINT64>(index) * increment)};
  }

  [[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS Upload(const void* data, UINT bytes) {
    const UINT alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    const UINT aligned = (bytes + alignment - 1u) & ~(alignment - 1u);
    if (aligned > UPLOAD_BYTES - upload_cursor) throw std::runtime_error("Compute upload arena exhausted");
    std::memcpy(mapped + upload_cursor, data, bytes);
    const auto address = upload->GetGPUVirtualAddress() + upload_cursor;
    upload_cursor += aligned;
    return address;
  }
};

// A fixed SRV/UAV table and root CBVs cover reconstruction and the pinned FSR
// shaders. Point and linear clamp samplers are immutable and cost no heap slots.
struct Pipeline {
  Microsoft::WRL::ComPtr<ID3D12RootSignature> root;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> state;

  void Create(ID3D12Device* device, std::span<const uint8_t> shader, UINT srvs, UINT uavs, UINT cbvs = 1u) {
    const std::array<D3D12_DESCRIPTOR_RANGE, 2> ranges = {{
        {.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV, .NumDescriptors = srvs, .BaseShaderRegister = 0u, .OffsetInDescriptorsFromTableStart = 0u},
        {.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV, .NumDescriptors = uavs, .BaseShaderRegister = 0u, .OffsetInDescriptorsFromTableStart = srvs},
    }};
    std::vector<D3D12_ROOT_PARAMETER> parameters(cbvs + 1u);
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable = {.NumDescriptorRanges = static_cast<UINT>(ranges.size()), .pDescriptorRanges = ranges.data()};
    for (UINT index = 0u; index < cbvs; ++index) {
      parameters[index + 1u].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
      parameters[index + 1u].Descriptor = {.ShaderRegister = index};
    }
    std::array<D3D12_STATIC_SAMPLER_DESC, 2> samplers = {};
    for (UINT index = 0u; index < samplers.size(); ++index) {
      samplers[index] = {
          .Filter = (index == 0u ? D3D12_FILTER_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_LINEAR),
          .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
          .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
          .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
          .MaxAnisotropy = 1u,
          .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
          .MaxLOD = D3D12_FLOAT32_MAX,
          .ShaderRegister = index,
      };
    }
    const D3D12_ROOT_SIGNATURE_DESC description = {
        .NumParameters = static_cast<UINT>(parameters.size()),
        .pParameters = parameters.data(),
        .NumStaticSamplers = static_cast<UINT>(samplers.size()),
        .pStaticSamplers = samplers.data(),
    };
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    Check(renodx::utils::directx::pD3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors), "Serialize compute root signature");
    Check(device->CreateRootSignature(0u, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root)), "Create compute root signature");
    const D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline = {.pRootSignature = root.Get(), .CS = {.pShaderBytecode = shader.data(), .BytecodeLength = shader.size()}};
    Check(device->CreateComputePipelineState(&pipeline, IID_PPV_ARGS(&state)), "Create compute pipeline");
  }

  void Bind(ID3D12GraphicsCommandList* commands, const Frame& frame, UINT first) const {
    commands->SetDescriptorHeaps(1u, frame.descriptors.GetAddressOf());
    commands->SetComputeRootSignature(root.Get());
    commands->SetPipelineState(state.Get());
    commands->SetComputeRootDescriptorTable(0u, frame.Gpu(first));
  }
};

}  // namespace mgsv::rendering::d3d12_compute

#endif  // RENODX_GAMES_MGSV_RENDERING_D3D12_COMPUTE_HPP_
