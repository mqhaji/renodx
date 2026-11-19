// SPDX-License-Identifier: MIT
#ifndef RENODX_GAMES_MGSV_RENDERING_NRD_HPP_
#define RENODX_GAMES_MGSV_RENDERING_NRD_HPP_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <Windows.h>
#include "../../../utils/path.hpp"
#include "../third_party/nrd/NRD.h"
#include "d3d12_compute.hpp"
#include "d3d12_timing.hpp"

namespace mgsv::rendering::denoising {

// NRD supplies algorithms and shader bytecode. Our existing backend retains
// ownership of the device, command lists, descriptors and GPU synchronization.
struct Library {
  HMODULE module = nullptr;
  decltype(&nrd::CreateInstance) create = nullptr;
  decltype(&nrd::DestroyInstance) destroy = nullptr;
  decltype(&nrd::GetInstanceDesc) describe = nullptr;
  decltype(&nrd::SetCommonSettings) set_common = nullptr;
  decltype(&nrd::SetDenoiserSettings) set_denoiser = nullptr;
  decltype(&nrd::GetComputeDispatches) get_dispatches = nullptr;

  Library() {
    const auto directory = renodx::utils::path::GetExecutableBasePath();
    if (directory.empty()) throw std::runtime_error("Cannot locate the game directory for NRD");
    module = LoadLibraryExW((directory / L"NRD.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (module == nullptr) throw std::runtime_error("NRD.dll is missing from the game directory");
    try {
      const auto load = [&](const char* name, auto* function) {
        *function = reinterpret_cast<std::remove_reference_t<decltype(*function)>>(GetProcAddress(module, name));
        if (*function == nullptr) throw std::runtime_error("NRD.dll has an incompatible API");
      };
      decltype(&nrd::GetLibraryDesc) describe_library = nullptr;
      load("CreateInstance", &create);
      load("DestroyInstance", &destroy);
      load("GetInstanceDesc", &describe);
      load("SetCommonSettings", &set_common);
      load("SetDenoiserSettings", &set_denoiser);
      load("GetComputeDispatches", &get_dispatches);
      load("GetLibraryDesc", &describe_library);
      const auto* description = describe_library();
      if (description == nullptr || description->versionMajor != NRD_VERSION_MAJOR || description->versionMinor != NRD_VERSION_MINOR || description->versionBuild != NRD_VERSION_BUILD
          || description->normalEncoding != nrd::NormalEncoding::R10_G10_B10_A2_UNORM
          || description->roughnessEncoding != nrd::RoughnessEncoding::LINEAR) {
        throw std::runtime_error("NRD.dll must be version 4.18 with normal encoding 2 and roughness encoding 1");
      }
    } catch (...) {
      FreeLibrary(module);
      module = nullptr;
      throw;
    }
  }
  Library(const Library&) = delete;
  Library& operator=(const Library&) = delete;
  ~Library() {
    if (module != nullptr) FreeLibrary(module);
  }
};

struct InstanceDeleter {
  std::shared_ptr<Library> library;
  void operator()(nrd::Instance* instance) const {
    if (instance != nullptr) library->destroy(*instance);
  }
};

struct Texture {
  Microsoft::WRL::ComPtr<ID3D12Resource> resource;
  D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
};

struct Runtime {
  static constexpr nrd::Identifier SPECULAR = 0u;
  static constexpr size_t RESOURCE_COUNT = static_cast<size_t>(nrd::ResourceType::MAX_NUM);
  std::unique_ptr<nrd::Instance, InstanceDeleter> instance;
  std::vector<d3d12_compute::Pipeline> pipelines;
  std::vector<Texture> permanent;
  std::vector<Texture> transient;
  std::array<uint32_t, 2> size = {};

  void Create(ID3D12Device* device) {
    const auto library = std::make_shared<Library>();
    const nrd::DenoiserDesc denoiser = {.identifier = SPECULAR, .denoiser = nrd::Denoiser::REBLUR_SPECULAR};
    const nrd::InstanceCreationDesc creation = {.denoisers = &denoiser, .denoisersNum = 1u};
    nrd::Instance* created = nullptr;
    if (library->create(creation, created) != nrd::Result::SUCCESS) throw std::runtime_error("Cannot create NRD specular denoiser");
    instance = {created, InstanceDeleter{.library = library}};
    const auto& description = *library->describe(*instance);
    if (description.constantBufferRegisterIndex != 0u || description.samplersBaseRegisterIndex != 0u || description.resourcesBaseRegisterIndex != 0u
        || description.samplersNum != 2u || description.samplers[0] != nrd::Sampler::NEAREST_CLAMP || description.samplers[1] != nrd::Sampler::LINEAR_CLAMP) {
      throw std::runtime_error("Unsupported NRD binding layout");
    }
    pipelines.resize(description.pipelinesNum);
    for (uint32_t index = 0u; index < description.pipelinesNum; ++index) {
      const auto& source = description.pipelines[index];
      UINT reads = 0u;
      UINT writes = 0u;
      for (uint32_t range = 0u; range < source.resourceRangesNum; ++range) {
        if (source.resourceRanges[range].descriptorType == nrd::DescriptorType::TEXTURE) {
          reads += source.resourceRanges[range].descriptorsNum;
        } else {
          writes += source.resourceRanges[range].descriptorsNum;
        }
      }
      if (source.computeShaderDXIL.bytecode == nullptr) throw std::runtime_error("NRD.dll does not contain DX12 shaders");
      pipelines[index].Create(device, {static_cast<const uint8_t*>(source.computeShaderDXIL.bytecode), static_cast<size_t>(source.computeShaderDXIL.size)},
                              reads, writes, source.hasConstantData ? 1u : 0u, description.resourcesSpaceIndex, description.constantBufferAndSamplersSpaceIndex);
    }
    Configure(nrd::ReblurSettings{});
  }

  // The render thread applies settings before asking the SDK for dispatches.
  // Existing queued work already owns its constants and keeps using the same pools.
  void Configure(const nrd::ReblurSettings& settings) {
    if (instance.get_deleter().library->set_denoiser(*instance, SPECULAR, &settings) != nrd::Result::SUCCESS) {
      throw std::runtime_error("Cannot configure NRD specular denoiser");
    }
  }

  // Called only after the bridge has retired old work. NRD histories are private
  // to this view size; they are never shared with another camera or effect.
  void Resize(ID3D12Device* device, uint32_t width, uint32_t height) {
    const auto& description = *instance.get_deleter().library->describe(*instance);
    const auto allocate = [&](std::vector<Texture>* pool, const nrd::TextureDesc* textures, uint32_t count) {
      pool->clear();
      pool->reserve(count);
      for (uint32_t index = 0u; index < count; ++index) {
        const auto divisor = textures[index].downsampleFactor;
        // SDK formats are not DXGI values; reject unknown packing explicitly.
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        switch (textures[index].format) {
          case nrd::Format::R8_UNORM:             format = DXGI_FORMAT_R8_UNORM; break;
          case nrd::Format::R8_UINT:              format = DXGI_FORMAT_R8_UINT; break;
          case nrd::Format::RG8_UNORM:            format = DXGI_FORMAT_R8G8_UNORM; break;
          case nrd::Format::RGBA8_UNORM:          format = DXGI_FORMAT_R8G8B8A8_UNORM; break;
          case nrd::Format::R16_SFLOAT:           format = DXGI_FORMAT_R16_FLOAT; break;
          case nrd::Format::R16_UINT:             format = DXGI_FORMAT_R16_UINT; break;
          case nrd::Format::R16_UNORM:            format = DXGI_FORMAT_R16_UNORM; break;
          case nrd::Format::RG16_SFLOAT:          format = DXGI_FORMAT_R16G16_FLOAT; break;
          case nrd::Format::RG16_UNORM:           format = DXGI_FORMAT_R16G16_UNORM; break;
          case nrd::Format::RGBA16_SFLOAT:        format = DXGI_FORMAT_R16G16B16A16_FLOAT; break;
          case nrd::Format::RGBA16_UNORM:         format = DXGI_FORMAT_R16G16B16A16_UNORM; break;
          case nrd::Format::R32_SFLOAT:           format = DXGI_FORMAT_R32_FLOAT; break;
          case nrd::Format::R32_UINT:             format = DXGI_FORMAT_R32_UINT; break;
          case nrd::Format::RG32_UINT:            format = DXGI_FORMAT_R32G32_UINT; break;
          case nrd::Format::RGBA32_UINT:          format = DXGI_FORMAT_R32G32B32A32_UINT; break;
          case nrd::Format::R10_G10_B10_A2_UNORM: format = DXGI_FORMAT_R10G10B10A2_UNORM; break;
          case nrd::Format::R11_G11_B10_UFLOAT:   format = DXGI_FORMAT_R11G11B10_FLOAT; break;
          default:                                throw std::runtime_error("Unsupported NRD texture format");
        }
        pool->push_back({.resource = d3d12_compute::CreateTexture(device, (width + divisor - 1u) / divisor, (height + divisor - 1u) / divisor, format)});
      }
    };
    allocate(&permanent, description.permanentPool, description.permanentPoolSize);
    allocate(&transient, description.transientPool, description.transientPoolSize);
    size = {width, height};
  }

  // Record into the caller's existing submission and retired descriptor arena.
  // Pool states persist across dispatches. Only external resources return to
  // COMMON, where the surrounding passes and the shared-resource bridge expect them.
  uint32_t Record(ID3D12Device* device, ID3D12GraphicsCommandList* commands, d3d12_compute::Frame* frame,
                  const nrd::CommonSettings& settings, const std::array<ID3D12Resource*, RESOURCE_COUNT>& resources,
                  std::span<std::byte> constant_capture = {}, d3d12_timing::Scope* timing = nullptr) {
    const auto& library = instance.get_deleter().library;
    const nrd::DispatchDesc* dispatches = nullptr;
    uint32_t count = 0u;
    uint32_t captured_bytes = 0u;
    if (library->set_common(*instance, settings) != nrd::Result::SUCCESS
        || library->get_dispatches(*instance, &SPECULAR, 1u, dispatches, count) != nrd::Result::SUCCESS) {
      throw std::runtime_error("Cannot prepare NRD dispatches");
    }
    std::array<D3D12_RESOURCE_STATES, RESOURCE_COUNT> states = {};
    D3D12_GPU_VIRTUAL_ADDRESS constant_address = 0u;
    for (uint32_t index = 0u; index < count; ++index) {
      const auto& dispatch = dispatches[index];
      // An explicit probe request can inspect the SDK's actual generated
      // camera constants. Ordinary rendering does not copy diagnostic data.
      const auto capture_size = std::min(constant_capture.size(), static_cast<size_t>(dispatch.constantBufferDataSize));
      if (capture_size > captured_bytes) {
        std::memcpy(constant_capture.data(), dispatch.constantBufferData, capture_size);
        captured_bytes = static_cast<uint32_t>(capture_size);
      }
      const UINT first = frame->Allocate(dispatch.resourcesNum);
      for (uint32_t binding = 0u; binding < dispatch.resourcesNum; ++binding) {
        const auto& source = dispatch.resources[binding];
        const auto type = static_cast<size_t>(source.type);
        auto* resource = resources[type];
        auto* state = &states[type];
        if (source.type == nrd::ResourceType::PERMANENT_POOL || source.type == nrd::ResourceType::TRANSIENT_POOL) {
          auto& pool = (source.type == nrd::ResourceType::PERMANENT_POOL ? permanent : transient);
          if (source.indexInPool >= pool.size()) throw std::runtime_error("NRD pool index is out of range");
          resource = pool[source.indexInPool].resource.Get();
          state = &pool[source.indexInPool].state;
        }
        if (resource == nullptr) throw std::runtime_error("A required NRD resource is missing");
        const bool writable = source.descriptorType == nrd::DescriptorType::STORAGE_TEXTURE;
        const auto next = (writable ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        d3d12_compute::Transition(commands, resource, *state, next);
        *state = next;
        if (writable) {
          const D3D12_UNORDERED_ACCESS_VIEW_DESC view = {.Format = resource->GetDesc().Format, .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D};
          device->CreateUnorderedAccessView(resource, nullptr, &view, frame->Cpu(first + binding));
        } else {
          const D3D12_SHADER_RESOURCE_VIEW_DESC view = {.Format = resource->GetDesc().Format, .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D, .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING, .Texture2D = {.MipLevels = 1u}};
          device->CreateShaderResourceView(resource, &view, frame->Cpu(first + binding));
        }
      }
      pipelines[dispatch.pipelineIndex].Bind(commands, *frame, first);
      if (dispatch.constantBufferDataSize != 0u) {
        if (!dispatch.constantBufferDataMatchesPreviousDispatch || constant_address == 0u) {
          constant_address = frame->Upload(dispatch.constantBufferData, dispatch.constantBufferDataSize);
        }
        commands->SetComputeRootConstantBufferView(1u, constant_address);
      }
      commands->Dispatch(dispatch.gridWidth, dispatch.gridHeight, 1u);
      // The SDK names each of its dispatches.
      if (timing != nullptr) {
        timing->Mark(dispatch.name);
      }
    }
    for (size_t index = 0u; index < resources.size(); ++index) {
      if (resources[index] != nullptr) d3d12_compute::Transition(commands, resources[index], states[index], D3D12_RESOURCE_STATE_COMMON);
    }
    return captured_bytes;
  }
};

}  // namespace mgsv::rendering::denoising
#endif  // RENODX_GAMES_MGSV_RENDERING_NRD_HPP_
