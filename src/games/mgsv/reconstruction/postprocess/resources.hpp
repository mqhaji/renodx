/*
 * Copyright (C) 2026 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#ifndef RENODX_GAMES_MGSV_RECONSTRUCTION_POSTPROCESS_RESOURCES_HPP_
#define RENODX_GAMES_MGSV_RECONSTRUCTION_POSTPROCESS_RESOURCES_HPP_

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <utility>
#include <vector>

#include <d3d11.h>
#include <d3d11_1.h>
#include <wrl/client.h>

#include <embed/post_bloom.h>
#include <embed/post_depth.h>
#include <embed/post_depth_align.h>
#include <embed/post_fullscreen.h>
#include <embed/post_scale.h>
#include <embed/post_stencil.h>
#include <embed/post_system.h>
#include <include/reshade.hpp>

#include "../../../../utils/resource.hpp"
#include "../../../../utils/resource_upgrade.hpp"
#include "../frame_inputs.hpp"
#include "../state.hpp"

namespace mgsv::reconstruction::post_process {

inline constexpr uint32_t BLOOM_GRID_WIDTH = 246u;
inline constexpr uint32_t BLOOM_GRID_HEIGHT = 138u;

inline constexpr GUID SCENE_DEPTH_GUID = {
    .Data1 = 0x240B06A8u,
    .Data2 = 0xD21Fu,
    .Data3 = 0x415Eu,
    .Data4 = {0x9Eu, 0x64u, 0xF2u, 0x25u, 0xC6u, 0xE1u, 0x99u, 0x43u},
};

inline constexpr GUID CLEAR_COLOR_GUID = {
    .Data1 = 0x79C18D48u,
    .Data2 = 0x3F10u,
    .Data3 = 0x480Bu,
    .Data4 = {0xB4u, 0x45u, 0x02u, 0x4Du, 0x33u, 0x96u, 0x65u, 0x19u},
};

struct ClearColor {
  uint64_t frame;
  std::array<float, 4> color;
};

inline Microsoft::WRL::ComPtr<ID3D11DepthStencilView> ReadSceneDepth(ID3D11Resource* scene) {
  Microsoft::WRL::ComPtr<IUnknown> object;
  Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth;
  UINT bytes = sizeof(IUnknown*);
  if (scene != nullptr && SUCCEEDED(scene->GetPrivateData(SCENE_DEPTH_GUID, &bytes, object.GetAddressOf())) && object != nullptr) {
    object.As(&depth);
  }
  return depth;
}

struct ContextScope {
  Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context;
  Microsoft::WRL::ComPtr<ID3DDeviceContextState> saved;
  ContextScope(ID3D11DeviceContext* source, ID3DDeviceContextState* empty) {
    if (SUCCEEDED(source->QueryInterface(IID_PPV_ARGS(&context)))) {
      context->SwapDeviceContextState(empty, &saved);
      if (saved != nullptr) {
        context->ClearState();
      }
    }
  }
  ContextScope(const ContextScope&) = delete;
  ContextScope& operator=(const ContextScope&) = delete;
  ~ContextScope() {
    if (context != nullptr && saved != nullptr) {
      context->SwapDeviceContextState(saved.Get(), nullptr);
    }
  }
};

struct Surface {
  enum class Role : uint8_t { DATA,
                              SCENE_COLOR,
                              BLOOM,
                              BLOOM_FIXED_GRID,
                              FLARE_LAYER };
  Microsoft::WRL::ComPtr<ID3D11Resource> original;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  Microsoft::WRL::ComPtr<ID3D11Buffer> size_patch;
  uint64_t routed_frame = std::numeric_limits<uint64_t>::max();
  Role role = Role::DATA;
  struct TargetView {
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> original;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> replacement;
  };
  struct SampleView {
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> original;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> replacement;
  };
  std::vector<TargetView> targets;
  std::vector<SampleView> samples;
};

// These objects depend on the device, not on the current render/output size.
struct Pipelines {
  reshade::api::device* device;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> fallback_shader;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> system_shader;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> bloom_shader;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> align_depth_shader;
  Microsoft::WRL::ComPtr<ID3D11VertexShader> copy_vertex;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> copy_depth_pixel;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> copy_stencil_pixel;
  Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_write;
  std::array<Microsoft::WRL::ComPtr<ID3D11DepthStencilState>, 8> stencil_write;
  std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, 8> stencil_bits;

  explicit Pipelines(reshade::api::device* device) : device(device) {
    auto* native_device = reinterpret_cast<ID3D11Device*>(device->get_native());  // NOLINT(performance-no-int-to-ptr)
    if (FAILED(native_device->CreateComputeShader(__post_scale.data(), __post_scale.size(), nullptr, &fallback_shader))
        || FAILED(native_device->CreateComputeShader(__post_system.data(), __post_system.size(), nullptr, &system_shader))
        || FAILED(native_device->CreateComputeShader(__post_bloom.data(), __post_bloom.size(), nullptr, &bloom_shader))
        || FAILED(native_device->CreateComputeShader(__post_depth_align.data(), __post_depth_align.size(), nullptr, &align_depth_shader))
        || FAILED(native_device->CreateVertexShader(__post_fullscreen.data(), __post_fullscreen.size(), nullptr, &copy_vertex))
        || FAILED(native_device->CreatePixelShader(__post_depth.data(), __post_depth.size(), nullptr, &copy_depth_pixel))
        || FAILED(native_device->CreatePixelShader(__post_stencil.data(), __post_stencil.size(), nullptr, &copy_stencil_pixel))) {
      throw std::runtime_error("MGSV scaled-output shaders allocation failed");
    }
    D3D11_DEPTH_STENCIL_DESC write = {
        .DepthEnable = TRUE,
        .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
        .DepthFunc = D3D11_COMPARISON_ALWAYS,
    };
    if (FAILED(native_device->CreateDepthStencilState(&write, &depth_write))) {
      throw std::runtime_error("MGSV depth copy state allocation failed");
    }
    const D3D11_DEPTH_STENCILOP_DESC replace = {
        .StencilFailOp = D3D11_STENCIL_OP_KEEP,
        .StencilDepthFailOp = D3D11_STENCIL_OP_KEEP,
        .StencilPassOp = D3D11_STENCIL_OP_REPLACE,
        .StencilFunc = D3D11_COMPARISON_ALWAYS,
    };
    write.StencilEnable = TRUE;
    write.StencilReadMask = 0xFFu;
    write.FrontFace = replace;
    write.BackFace = replace;
    write.DepthEnable = FALSE;
    write.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    for (size_t i = 0u; i < stencil_write.size(); ++i) {
      write.StencilWriteMask = static_cast<UINT8>(1u << i);
      const std::array<uint32_t, 4> bit = {1u << i, 0u, 0u, 0u};
      const D3D11_BUFFER_DESC constants = {.ByteWidth = sizeof(bit), .Usage = D3D11_USAGE_IMMUTABLE, .BindFlags = D3D11_BIND_CONSTANT_BUFFER};
      const D3D11_SUBRESOURCE_DATA initial = {.pSysMem = bit.data()};
      if (FAILED(native_device->CreateDepthStencilState(&write, &stencil_write[i]))
          || FAILED(native_device->CreateBuffer(&constants, &initial, &stencil_bits[i]))) {
        throw std::runtime_error("MGSV stencil copy state allocation failed");
      }
    }
  }
};

// Keep each output and its post-processing aliases alive through submission.
// Native resources remain available to earlier, render-sized producers.
struct Output {
  std::shared_mutex surface_mutex;
  std::vector<std::shared_ptr<Surface>> surfaces;
  std::shared_ptr<Pipelines> pipelines;
  reshade::api::device* device = nullptr;
  uint32_t render_width = 0u;
  uint32_t render_height = 0u;
  Microsoft::WRL::ComPtr<ID3D11Resource> source;
  Microsoft::WRL::ComPtr<ID3D11Resource> original_source;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> color;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
  Microsoft::WRL::ComPtr<ID3D11Buffer> size_patch;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> source_view;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> fallback_uav;
  Microsoft::WRL::ComPtr<ID3DDeviceContextState> empty_state;
  Microsoft::WRL::ComPtr<ID3D11Buffer> copy_constants;
  Microsoft::WRL::ComPtr<ID3D11Resource> geometry_depth;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> geometry_depth_view;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> aligned_depth;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> aligned_depth_view;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> aligned_depth_uav;
  Microsoft::WRL::ComPtr<ID3D11Resource> native_depth;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_copy;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth_view;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> stencil_view;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> output_depth;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> output_depth_sample;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> output_stencil_sample;
  std::array<Microsoft::WRL::ComPtr<ID3D11DepthStencilView>, 4> output_depth_views;
  ColorTarget target;
  std::atomic<uint32_t> observed_passes = 0u;

  explicit Output(const ValidatedFrameInputs& inputs, std::shared_ptr<Pipelines> shared_pipelines = nullptr)
      : pipelines(std::move(shared_pipelines)), device(inputs.device), render_width(inputs.render_width), render_height(inputs.render_height) {
    auto* native_device = reinterpret_cast<ID3D11Device*>(device->get_native());  // NOLINT(performance-no-int-to-ptr)
    source = reinterpret_cast<ID3D11Resource*>(inputs.color_resource.handle);     // NOLINT(performance-no-int-to-ptr)
    if (inputs.color_srv.handle != 0u && renodx::utils::resource::shared.data != nullptr) {
      renodx::utils::resource::GetResourceViewInfo(inputs.color_srv, [&](const auto& view) {
        if (!view.destroyed && view.device == device
            && (view.clone_resource == inputs.color_resource || view.original_resource == inputs.color_resource)) {
          original_source = reinterpret_cast<ID3D11Resource*>(view.original_resource.handle);  // NOLINT(performance-no-int-to-ptr)
        }
      });
    }
    const D3D11_TEXTURE2D_DESC description = {
        .Width = inputs.output.width,
        .Height = inputs.output.height,
        .MipLevels = 1u,
        .ArraySize = 1u,
        .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
        .SampleDesc = {.Count = 1u, .Quality = 0u},
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
    };
    if (FAILED(native_device->CreateTexture2D(&description, nullptr, &color))
        || FAILED(native_device->CreateShaderResourceView(color.Get(), nullptr, &srv))
        || FAILED(native_device->CreateRenderTargetView(color.Get(), nullptr, &rtv))) {
      throw std::runtime_error("MGSV post-processing output allocation failed");
    }
    target = {
        .resource = {reinterpret_cast<uint64_t>(color.Get())},
        .width = description.Width,
        .height = description.Height,
        .format = reshade::api::format::r16g16b16a16_float,
    };
    if (!IsScaled()) return;
    if (pipelines == nullptr || pipelines->device != device) {
      pipelines = std::make_shared<Pipelines>(device);
    }
    size_patch = CreateSizePatch(description.Width, description.Height);
    source_view = reinterpret_cast<ID3D11ShaderResourceView*>(inputs.color_srv.handle);          // NOLINT(performance-no-int-to-ptr)
    geometry_depth = reinterpret_cast<ID3D11Resource*>(inputs.depth_resource.handle);            // NOLINT(performance-no-int-to-ptr)
    geometry_depth_view = reinterpret_cast<ID3D11ShaderResourceView*>(inputs.depth_srv.handle);  // NOLINT(performance-no-int-to-ptr)
    Microsoft::WRL::ComPtr<ID3D11Device1> device1;
    const D3D_FEATURE_LEVEL feature = native_device->GetFeatureLevel();
    const UINT flags = ((native_device->GetCreationFlags() & D3D11_CREATE_DEVICE_SINGLETHREADED) != 0u
                            ? D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED
                            : 0u);
    const D3D11_BUFFER_DESC constants = {.ByteWidth = 32u, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_CONSTANT_BUFFER};
    if (FAILED(native_device->QueryInterface(IID_PPV_ARGS(&device1)))
        || FAILED(device1->CreateDeviceContextState(flags, &feature, 1u, D3D11_SDK_VERSION, __uuidof(ID3D11Device1), nullptr, &empty_state))
        || FAILED(native_device->CreateUnorderedAccessView(color.Get(), nullptr, &fallback_uav))) {
      throw std::runtime_error("MGSV scaled-output pipeline allocation failed");
    }
    if (FAILED(native_device->CreateBuffer(&constants, nullptr, &copy_constants))) {
      throw std::runtime_error("MGSV scaled-output constants allocation failed");
    }
    auto depth_description = description;
    depth_description.Format = DXGI_FORMAT_R32_FLOAT;
    depth_description.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    if (FAILED(native_device->CreateTexture2D(&depth_description, nullptr, &aligned_depth))
        || FAILED(native_device->CreateShaderResourceView(aligned_depth.Get(), nullptr, &aligned_depth_view))
        || FAILED(native_device->CreateUnorderedAccessView(aligned_depth.Get(), nullptr, &aligned_depth_uav))) {
      throw std::runtime_error("MGSV output geometry depth allocation failed");
    }
    const auto scene_depth = ReadSceneDepth(source.Get());
    if (scene_depth == nullptr) return;
    scene_depth->GetResource(&native_depth);
    Microsoft::WRL::ComPtr<ID3D11Texture2D> native_texture;
    if (FAILED(native_depth.As(&native_texture))) throw std::runtime_error("MGSV unsupported native scene depth");
    native_texture->GetDesc(&depth_description);
    if (depth_description.Width == target.width && depth_description.Height == target.height) return;
    if (depth_description.Width != render_width || depth_description.Height != render_height
        || depth_description.SampleDesc.Count != 1u || depth_description.ArraySize != 1u || depth_description.MipLevels != 1u) {
      throw std::runtime_error("MGSV scene depth does not match the render extent");
    }
    D3D11_DEPTH_STENCIL_VIEW_DESC depth_target;
    scene_depth->GetDesc(&depth_target);
    depth_target.Flags = 0u;
    DXGI_FORMAT sampled_depth = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT sampled_stencil = DXGI_FORMAT_UNKNOWN;
    switch (depth_target.Format) {
      case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        depth_description.Format = DXGI_FORMAT_R32G8X24_TYPELESS;
        sampled_depth = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        sampled_stencil = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
        break;
      case DXGI_FORMAT_D24_UNORM_S8_UINT:
        depth_description.Format = DXGI_FORMAT_R24G8_TYPELESS;
        sampled_depth = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        sampled_stencil = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
        break;
      case DXGI_FORMAT_D32_FLOAT:
        depth_description.Format = DXGI_FORMAT_R32_TYPELESS;
        sampled_depth = DXGI_FORMAT_R32_FLOAT;
        break;
      case DXGI_FORMAT_D16_UNORM:
        depth_description.Format = DXGI_FORMAT_R16_TYPELESS;
        sampled_depth = DXGI_FORMAT_R16_UNORM;
        break;
      default: throw std::runtime_error("MGSV unsupported native scene depth format");
    }
    depth_description.Usage = D3D11_USAGE_DEFAULT;
    depth_description.CPUAccessFlags = 0u;
    depth_description.MiscFlags = 0u;
    depth_description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SHADER_RESOURCE_VIEW_DESC sample = {.Format = sampled_depth, .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D, .Texture2D = {.MostDetailedMip = 0u, .MipLevels = 1u}};
    if (FAILED(native_device->CreateTexture2D(&depth_description, nullptr, &depth_copy))
        || FAILED(native_device->CreateShaderResourceView(depth_copy.Get(), &sample, &depth_view))) {
      throw std::runtime_error("MGSV scene depth input allocation failed");
    }
    if (sampled_stencil != DXGI_FORMAT_UNKNOWN) {
      sample.Format = sampled_stencil;
      if (FAILED(native_device->CreateShaderResourceView(depth_copy.Get(), &sample, &stencil_view))) throw std::runtime_error("MGSV stencil input allocation failed");
    }
    depth_description.Width = target.width;
    depth_description.Height = target.height;
    depth_description.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(native_device->CreateTexture2D(&depth_description, nullptr, &output_depth))
        || FAILED(native_device->CreateDepthStencilView(output_depth.Get(), &depth_target, &output_depth_views[0]))) {
      throw std::runtime_error("MGSV output depth/stencil pipeline allocation failed");
    }
    sample.Format = sampled_depth;
    if (FAILED(native_device->CreateShaderResourceView(output_depth.Get(), &sample, &output_depth_sample))) {
      throw std::runtime_error("MGSV output depth sample view allocation failed");
    }
    if (sampled_stencil != DXGI_FORMAT_UNKNOWN) {
      sample.Format = sampled_stencil;
      if (FAILED(native_device->CreateShaderResourceView(output_depth.Get(), &sample, &output_stencil_sample))) {
        throw std::runtime_error("MGSV output stencil sample view allocation failed");
      }
    }
  }

  // Canceled reconstruction still has a native remainder that reads this
  // target. Fill it from this submission's native prefix, never old output.
  void CopyNative(ID3D11DeviceContext* context) const {
    if (!IsScaled()) {
      context->CopyResource(color.Get(), source.Get());
      return;
    }
    const ContextScope restore(context, empty_state.Get());
    if (restore.saved == nullptr) return;
    context->CSSetShader(pipelines->fallback_shader.Get(), nullptr, 0u);
    context->CSSetShaderResources(0u, 1u, source_view.GetAddressOf());
    context->CSSetUnorderedAccessViews(0u, 1u, fallback_uav.GetAddressOf(), nullptr);
    context->CSSetConstantBuffers(0u, 1u, copy_constants.GetAddressOf());
    context->Dispatch((target.width + 7u) / 8u, (target.height + 7u) / 8u, 1u);
  }

  // Called on the immediate context after the native prefix has executed.
  // Always rebuild current depth/masks on replay; CPU recording cannot seed it.
  void PrepareDepth(ID3D11DeviceContext* context, float jitter_x, float jitter_y) const {
    if (!IsScaled()) return;
    const ContextScope restore(context, empty_state.Get());
    if (restore.saved == nullptr) {
      resolution::Fail("cannot preserve the output depth copy state");
      return;
    }
    const std::array<uint32_t, 8> values = {render_width, render_height, target.width, target.height,
                                            0u, std::bit_cast<uint32_t>(jitter_x), std::bit_cast<uint32_t>(jitter_y), 0u};
    context->UpdateSubresource(copy_constants.Get(), 0u, nullptr, values.data(), 0u, 0u);
    if (geometry_depth_view != nullptr) {
      context->CSSetShader(pipelines->align_depth_shader.Get(), nullptr, 0u);
      context->CSSetShaderResources(0u, 1u, geometry_depth_view.GetAddressOf());
      context->CSSetUnorderedAccessViews(0u, 1u, aligned_depth_uav.GetAddressOf(), nullptr);
      context->CSSetConstantBuffers(0u, 1u, copy_constants.GetAddressOf());
      context->Dispatch((target.width + 7u) / 8u, (target.height + 7u) / 8u, 1u);
    }
    if (native_depth == nullptr) return;
    context->CopyResource(depth_copy.Get(), native_depth.Get());
    if (stencil_view != nullptr) {
      // Stencil draws only write set bits; zero the others first.
      context->ClearDepthStencilView(output_depth_views[0].Get(), D3D11_CLEAR_STENCIL, 0.f, 0u);
    }
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(pipelines->copy_vertex.Get(), nullptr, 0u);
    const D3D11_VIEWPORT viewport = {.TopLeftX = 0.f, .TopLeftY = 0.f, .Width = static_cast<float>(target.width), .Height = static_cast<float>(target.height), .MinDepth = 0.f, .MaxDepth = 1.f};
    context->RSSetViewports(1u, &viewport);
    context->OMSetRenderTargets(0u, nullptr, output_depth_views[0].Get());
    context->PSSetConstantBuffers(0u, 1u, copy_constants.GetAddressOf());
    context->OMSetDepthStencilState(pipelines->depth_write.Get(), 0u);
    context->PSSetShader(pipelines->copy_depth_pixel.Get(), nullptr, 0u);
    context->PSSetShaderResources(0u, 1u, depth_view.GetAddressOf());
    context->Draw(3u, 0u);
    if (stencil_view != nullptr) {
      context->PSSetShader(pipelines->copy_stencil_pixel.Get(), nullptr, 0u);
      context->PSSetShaderResources(0u, 1u, stencil_view.GetAddressOf());
      for (size_t i = 0u; i < pipelines->stencil_write.size(); ++i) {
        context->OMSetDepthStencilState(pipelines->stencil_write[i].Get(), 1u << i);
        context->PSSetConstantBuffers(1u, 1u, pipelines->stencil_bits[i].GetAddressOf());
        context->Draw(3u, 0u);
      }
    }
  }

  [[nodiscard]] bool IsSceneTarget(ID3D11Resource* resource) const {
    return resource != nullptr && (resource == source.Get() || resource == original_source.Get());
  }

  [[nodiscard]] bool IsScaled() const {
    return render_width != 0u && render_height != 0u && (render_width != target.width || render_height != target.height);
  }

  Microsoft::WRL::ComPtr<ID3D11Buffer> CreateSizePatch(uint32_t width, uint32_t height) const {
    const float inverse_width = 1.f / static_cast<float>(width);
    const float inverse_height = 1.f / static_cast<float>(height);
    // Bloom's working region stays fixed while its backing buffers resize.
    const std::array bloom_center = {
        inverse_width * (static_cast<float>(BLOOM_GRID_WIDTH) * 0.5f),
        inverse_height * (static_cast<float>(BLOOM_GRID_HEIGHT) * 0.5f),
    };
    const std::array<float, 28> values = {
        static_cast<float>(width),
        static_cast<float>(height),
        inverse_width,
        inverse_height,
        static_cast<float>(target.width),
        static_cast<float>(target.height),
        0.f,
        0.f,
        std::min(2.f, static_cast<float>(target.width) * 0.85f * (1.f / 1920.f)),
        0.f,
        0.f,
        0.f,
        inverse_width,
        inverse_height,
        1.f,
        1.f,
        bloom_center[0],
        bloom_center[1],
        1.f / std::sqrt((bloom_center[0] * bloom_center[0]) + (bloom_center[1] * bloom_center[1])),
        0.f,
        1.f / bloom_center[0],
        1.f / bloom_center[1],
        0.f,
        0.f,
        -0.5f * inverse_width,
        -0.5f * inverse_height,
        static_cast<float>(BLOOM_GRID_WIDTH) / (static_cast<float>(target.width) * 0.25f),
        static_cast<float>(BLOOM_GRID_HEIGHT) / (static_cast<float>(target.height) * 0.25f),
    };
    const D3D11_BUFFER_DESC description = {
        .ByteWidth = sizeof(values),
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
    };
    const D3D11_SUBRESOURCE_DATA initial = {.pSysMem = values.data()};
    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    auto* native_device = reinterpret_cast<ID3D11Device*>(device->get_native());  // NOLINT(performance-no-int-to-ptr)
    if (FAILED(native_device->CreateBuffer(&description, &initial, &buffer))) {
      throw std::runtime_error("MGSV output-size constants allocation failed");
    }
    return buffer;
  }

  std::shared_ptr<Surface> GetSurface(ID3D11Resource* original, bool create) {
    if (original == nullptr || render_width == 0u || render_height == 0u) return nullptr;
    {
      const std::shared_lock lock(surface_mutex);
      for (const auto& surface : surfaces) {
        if (surface->original.Get() == original) {
          // General intermediates can change purpose each frame. The dedicated
          // flare layer keeps its producer/consumer link: another thread can
          // record the scene copy before recording the current flare draws.
          if (!create && surface->routed_frame != reconstruction::state::CurrentFrameToken()
              && surface->role != Surface::Role::FLARE_LAYER) return nullptr;
          return surface;
        }
      }
    }
    if (!create) return nullptr;
    const std::unique_lock lock(surface_mutex);
    for (const auto& surface : surfaces) {
      if (surface->original.Get() == original) return surface;
    }
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    if (FAILED(original->QueryInterface(IID_PPV_ARGS(&texture)))) return nullptr;
    D3D11_TEXTURE2D_DESC description;
    texture->GetDesc(&description);
    if (description.ArraySize != 1u || description.MipLevels != 1u || description.SampleDesc.Count != 1u) return nullptr;
    uint32_t level = 0u;
    for (; level <= 4u; ++level) {
      if (description.Width == (render_width >> level) && description.Height == (render_height >> level)) break;
    }
    // Fixed-size UI and utility targets share these draw paths. Their size
    // alone does not make them part of the output-resolution scene chain.
    if (level > 4u) return nullptr;
    description.Width = std::max(1u, target.width >> level);
    description.Height = std::max(1u, target.height >> level);
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    description.CPUAccessFlags = 0u;
    description.MiscFlags = 0u;
    auto surface = std::make_shared<Surface>();
    surface->original = original;
    auto* native_device = reinterpret_cast<ID3D11Device*>(device->get_native());  // NOLINT(performance-no-int-to-ptr)
    if (IsScaled()) {
      if (FAILED(native_device->CreateTexture2D(&description, nullptr, &surface->texture))) {
        resolution::Fail("post-processing resource allocation failed");
        return nullptr;
      }
      surface->size_patch = CreateSizePatch(description.Width, description.Height);
    } else {
      // Native-AA output still needs to recognize intermediate color layers
      // before they composite into the separate scene. Reuse their storage.
      surface->texture = texture;
    }
    surfaces.push_back(surface);
    return surface;
  }

  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> MapTarget(ID3D11RenderTargetView* view, bool create, bool scene_only = false,
                                                           Surface::Role role = Surface::Role::DATA) {
    if (view == nullptr) return nullptr;
    Microsoft::WRL::ComPtr<ID3D11Resource> original;
    view->GetResource(&original);
    if (IsSceneTarget(original.Get())) return rtv;
    const auto surface = GetSurface(original.Get(), create);
    if (surface == nullptr) return nullptr;
    const std::unique_lock lock(surface_mutex);
    if (scene_only && surface->role == Surface::Role::DATA && !create) return nullptr;
    if (create) {
      surface->routed_frame = reconstruction::state::CurrentFrameToken();
      surface->role = (scene_only ? Surface::Role::SCENE_COLOR : role);
    }
    for (const auto& target_view : surface->targets) {
      if (target_view.original.Get() == view) return target_view.replacement;
    }
    D3D11_RENDER_TARGET_VIEW_DESC description;
    view->GetDesc(&description);
    Surface::TargetView target_view = {.original = view};
    auto* native_device = reinterpret_cast<ID3D11Device*>(device->get_native());  // NOLINT(performance-no-int-to-ptr)
    if (FAILED(native_device->CreateRenderTargetView(surface->texture.Get(), &description, &target_view.replacement))) {
      resolution::Fail("post-processing target view allocation failed");
      return nullptr;
    }
    surface->targets.push_back(target_view);
    return target_view.replacement;
  }

  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> MapInput(ID3D11ShaderResourceView* view, bool scene_only = false) {
    if (view == nullptr) return nullptr;
    Microsoft::WRL::ComPtr<ID3D11Resource> original;
    view->GetResource(&original);
    // The float scene uses explicit encoding. Leave unrelated native sRGB
    // views to the existing clone path rather than changing their transfer.
    if (original == source) return srv;
    if (IsScaled() && original == native_depth) {
      D3D11_SHADER_RESOURCE_VIEW_DESC description;
      view->GetDesc(&description);
      if (description.Format == DXGI_FORMAT_X24_TYPELESS_G8_UINT || description.Format == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT) return output_stencil_sample;
      if (original != geometry_depth) return output_depth_sample;
    }
    if (IsScaled() && original == geometry_depth) return aligned_depth_view;
    const auto surface = GetSurface(original.Get(), false);
    if (surface == nullptr) return nullptr;
    const std::unique_lock lock(surface_mutex);
    if (scene_only && surface->role == Surface::Role::DATA) return nullptr;
    for (const auto& sample : surface->samples) {
      if (sample.original.Get() == view) return sample.replacement;
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC description;
    view->GetDesc(&description);
    Surface::SampleView sample = {.original = view};
    auto* native_device = reinterpret_cast<ID3D11Device*>(device->get_native());  // NOLINT(performance-no-int-to-ptr)
    if (FAILED(native_device->CreateShaderResourceView(surface->texture.Get(), &description, &sample.replacement))) {
      resolution::Fail("post-processing sample view allocation failed");
      return nullptr;
    }
    surface->samples.push_back(sample);
    return sample.replacement;
  }

  bool CopySurface(ID3D11DeviceContext* context, ID3D11Resource* from, ID3D11Resource* to) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> copied;
    auto role = Surface::Role::DATA;
    if (IsSceneTarget(from)) {
      copied = color;
      role = Surface::Role::SCENE_COLOR;
    } else if (const auto surface = GetSurface(from, false); surface != nullptr) {
      copied = surface->texture;
      const std::shared_lock lock(surface_mutex);
      role = surface->role;
    }
    if (copied == nullptr) return false;
    if (IsSceneTarget(to)) {
      if (copied != color) {
        context->CopyResource(color.Get(), copied.Get());
      }
      return true;
    }
    Microsoft::WRL::ComPtr<ID3D11Texture2D> native_destination;
    if (to == nullptr || FAILED(to->QueryInterface(IID_PPV_ARGS(&native_destination)))) return false;
    D3D11_TEXTURE2D_DESC source_description;
    D3D11_TEXTURE2D_DESC destination_description;
    copied->GetDesc(&source_description);
    native_destination->GetDesc(&destination_description);
    if (source_description.Format != destination_description.Format) return false;
    const auto destination = GetSurface(to, true);
    if (destination == nullptr) return false;
    destination->texture->GetDesc(&destination_description);
    if (source_description.Width != destination_description.Width || source_description.Height != destination_description.Height
        || source_description.Format != destination_description.Format) return false;
    {
      const std::unique_lock lock(surface_mutex);
      destination->routed_frame = reconstruction::state::CurrentFrameToken();
      destination->role = role;
    }
    context->CopyResource(destination->texture.Get(), copied.Get());
    return true;
  }
};

struct State {
  std::shared_mutex mutex;
  std::shared_ptr<Pipelines> pipelines;
  std::shared_ptr<Output> cached;
  std::shared_ptr<Output> active;
  uint64_t frame = 0u;
};
inline State state;

inline std::shared_ptr<Output> GetActive(reshade::api::command_list* command_list) {
  const std::shared_lock lock(state.mutex);
  if (state.frame != reconstruction::state::CurrentFrameToken()
      || state.active == nullptr || state.active->device != command_list->get_device()) return nullptr;
  return state.active;
}

inline void Reset(bool release_pipelines = false) {
  std::shared_ptr<Pipelines> pipelines;
  std::shared_ptr<Output> cached;
  std::shared_ptr<Output> active;
  {
    const std::unique_lock lock(state.mutex);
    cached.swap(state.cached);
    active.swap(state.active);
    if (release_pipelines) {
      pipelines.swap(state.pipelines);
    }
  }
}

}  // namespace mgsv::reconstruction::post_process

#endif  // RENODX_GAMES_MGSV_RECONSTRUCTION_POSTPROCESS_RESOURCES_HPP_
