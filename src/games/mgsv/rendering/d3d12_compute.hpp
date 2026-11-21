#ifndef RENODX_GAMES_MGSV_RENDERING_D3D12_COMPUTE_HPP_
#define RENODX_GAMES_MGSV_RENDERING_D3D12_COMPUTE_HPP_

// Building blocks for the mod's D3D12 compute passes: barriers, buffers,
// textures, inputs shared from D3D11, per-slot descriptor and upload arenas,
// a simple pipeline and the pass that binds and dispatches it. The bridge owns
// submission and slot retirement; these only create resources and record commands.

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>

#include "d3d12_bridge.hpp"
#include "shared_texture.hpp"

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

inline Microsoft::WRL::ComPtr<ID3D12Resource> CreateBuffer(ID3D12Device* device, UINT64 bytes, D3D12_HEAP_TYPE heap_type,
                                                           D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state) {
  const D3D12_HEAP_PROPERTIES heap = {.Type = heap_type};
  const D3D12_RESOURCE_DESC description = {
      .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
      .Width = bytes,
      .Height = 1u,
      .DepthOrArraySize = 1u,
      .MipLevels = 1u,
      .SampleDesc = {.Count = 1u},
      .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
      .Flags = flags,
  };
  Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
  Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, state, nullptr, IID_PPV_ARGS(&buffer)),
        "Create compute buffer");
  return buffer;
}

// A texture D3D11 writes and D3D12 reads in place: created shareable on D3D11
// and imported directly, so no copy is made per frame. Textures with initial
// data are read-only and get no UAV.
struct SharedInput {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
  std::shared_ptr<SharedTexture> allocation;
  d3d12_bridge::SharedImage image;
};

inline SharedInput CreateSharedInput(d3d12_bridge::Bridge* bridge, UINT width, UINT height, DXGI_FORMAT format, UINT mips = 1u,
                                     const D3D11_SUBRESOURCE_DATA* initial_data = nullptr) {
  SharedInput input;
  const D3D11_TEXTURE2D_DESC description = {
      .Width = width,
      .Height = height,
      .MipLevels = mips,
      .ArraySize = 1u,
      .Format = format,
      .SampleDesc = {.Count = 1u},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_SHADER_RESOURCE | (initial_data == nullptr ? D3D11_BIND_UNORDERED_ACCESS : 0u),
      .MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE,
  };
  Check(bridge->device11->CreateTexture2D(&description, initial_data, &input.texture), "Create shared input");
  if (initial_data == nullptr) {
    Check(bridge->device11->CreateUnorderedAccessView(input.texture.Get(), nullptr, &input.uav), "Create shared input view");
  }
  input.allocation = ExportTexture(input.texture.Get());
  if (input.allocation == nullptr) throw std::runtime_error("Cannot export shared input");
  input.image = d3d12_bridge::SharedImage(bridge->device11.Get(), bridge->device12.Get(), input.texture.Get(), description,
                                          D3D11_BIND_SHADER_RESOURCE, input.allocation.get(), &bridge->statistics.image_imports);
  if (!input.image.direct) throw std::runtime_error("Shared input must be imported directly");
  return input;
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

// Each bridge slot owns one Frame. Its descriptors and uploaded bytes stay
// untouched until both APIs have finished the slot's previous use.
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
      replacement.upload = CreateBuffer(device, UPLOAD_BYTES, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
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

// One SRV/UAV descriptor table plus root constant buffers covers reconstruction
// and the FSR shaders of the pinned SDK version. The point and linear clamp
// samplers (s0, s1) are static, so they use no heap slots.
struct Pipeline {
  Microsoft::WRL::ComPtr<ID3D12RootSignature> root;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> state;

  void Create(ID3D12Device* device, std::span<const uint8_t> shader, UINT srvs, UINT uavs, UINT cbvs = 1u,
              UINT resource_space = 0u, UINT constant_space = 0u) {
    const std::array<D3D12_DESCRIPTOR_RANGE, 2> ranges = {{
        {.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV, .NumDescriptors = srvs, .BaseShaderRegister = 0u, .RegisterSpace = resource_space, .OffsetInDescriptorsFromTableStart = 0u},
        {.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV, .NumDescriptors = uavs, .BaseShaderRegister = 0u, .RegisterSpace = resource_space, .OffsetInDescriptorsFromTableStart = srvs},
    }};
    std::vector<D3D12_ROOT_PARAMETER> parameters(cbvs + 1u);
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    // A table may contain only outputs, as with an integration lookup texture.
    // Empty descriptor ranges are invalid in a D3D12 root signature.
    parameters[0].DescriptorTable = {.NumDescriptorRanges = srvs != 0u && uavs != 0u ? 2u : 1u,
                                     .pDescriptorRanges = ranges.data() + (srvs == 0u ? 1u : 0u)};
    for (UINT index = 0u; index < cbvs; ++index) {
      parameters[index + 1u].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
      parameters[index + 1u].Descriptor = {.ShaderRegister = index, .RegisterSpace = constant_space};
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
          .RegisterSpace = constant_space,
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

struct ImageView {
  ID3D12Resource* resource = nullptr;
  UINT mip = 0u;
  UINT mips = 1u;
};

// Records one pass: width x height threads in groups of 8 x 8 (one group for
// a 1 x 1 pass). Every image begins and ends in COMMON. The views name exact
// mip subresources, so a reduction can read one level while writing the next.
// constants bind as b0 onward.
inline void DispatchPass(ID3D12Device* device, ID3D12GraphicsCommandList* commands, Frame* frame, const Pipeline& pipeline,
                         std::span<const D3D12_GPU_VIRTUAL_ADDRESS> constants, std::span<const ImageView> inputs,
                         std::span<const ImageView> outputs, UINT width, UINT height) {
  const UINT first = frame->Allocate(static_cast<UINT>(inputs.size() + outputs.size()));
  for (size_t index = 0u; index < inputs.size(); ++index) {
    const auto& input = inputs[index];
    const auto description = input.resource->GetDesc();
    const D3D12_SHADER_RESOURCE_VIEW_DESC view = {.Format = description.Format, .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D, .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING, .Texture2D = {.MostDetailedMip = input.mip, .MipLevels = input.mips}};
    device->CreateShaderResourceView(input.resource, &view, frame->Cpu(first + static_cast<UINT>(index)));
    for (UINT mip = input.mip; mip < input.mip + input.mips; ++mip) {
      Transition(commands, input.resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, mip);
    }
  }
  for (size_t index = 0u; index < outputs.size(); ++index) {
    const auto& output = outputs[index];
    const D3D12_UNORDERED_ACCESS_VIEW_DESC view = {.Format = output.resource->GetDesc().Format,
                                                   .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
                                                   .Texture2D = {.MipSlice = output.mip}};
    device->CreateUnorderedAccessView(output.resource, nullptr, &view, frame->Cpu(first + static_cast<UINT>(inputs.size() + index)));
    Transition(commands, output.resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, output.mip);
  }
  pipeline.Bind(commands, *frame, first);
  for (UINT index = 0u; index < constants.size(); ++index) {
    commands->SetComputeRootConstantBufferView(index + 1u, constants[index]);
  }
  commands->Dispatch((width + 7u) / 8u, (height + 7u) / 8u, 1u);
  for (const auto& output : outputs) {
    Transition(commands, output.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, output.mip);
  }
  for (const auto& input : inputs) {
    for (UINT mip = input.mip; mip < input.mip + input.mips; ++mip) {
      Transition(commands, input.resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON, mip);
    }
  }
}

}  // namespace mgsv::rendering::d3d12_compute

#endif  // RENODX_GAMES_MGSV_RENDERING_D3D12_COMPUTE_HPP_
