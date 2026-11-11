// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

#ifndef SRC_GAMES_MGSV_GTVBGI_AMBIENT_DIFFUSE_HPP_
#define SRC_GAMES_MGSV_GTVBGI_AMBIENT_DIFFUSE_HPP_

#include <d3d11_1.h>
#include <wrl/client.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <vector>

#include <include/reshade.hpp>

#include "../../../../utils/data.hpp"
#include "../../rendering/inputs.hpp"
#include "../../rendering/producer_records.hpp"

namespace mgsv::lighting::ambient_diffuse {

inline constexpr GUID SKY_SOURCE_GUID = {.Data1 = 0xa1465c18, .Data2 = 0x1276, .Data3 = 0x4697, .Data4 = {0x8a, 0xd0, 0x9c, 0x21, 0x35, 0xf7, 0xd8, 0x43}};

// Snapshot the small inputs to native ambient upsampling, not a new full-size
// ambient image. The application shader reproduces that upsampling later.
struct Target {
  uint64_t generation = 0u;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> image;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> clear_view;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> weights;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> weight_view;
  Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
  Microsoft::WRL::ComPtr<ID3D11Buffer> constants;
  Microsoft::WRL::ComPtr<ID3D11Resource> depth;
  UINT first_constant = 0u;
  UINT constant_count = 0u;
  UINT width = 0u;
  UINT height = 0u;
  UINT source_width = 0u;
  UINT source_height = 0u;
  UINT constant_bytes = 0u;
  DXGI_FORMAT source_format = DXGI_FORMAT_UNKNOWN;
  DXGI_FORMAT weight_format = DXGI_FORMAT_UNKNOWN;
};

// Pair each native diffuse texture with its copied ambient input. Do not keep
// the key texture alive; its destruction must retire the stored copies.
inline renodx::utils::data::ParallelFlatHashMap<ID3D11Resource*, std::shared_ptr<Target>, std::shared_mutex> targets;

inline std::shared_ptr<Target> Read(reshade::api::device* device, ID3D11Resource* diffuse) {
  const auto selected = inputs::ReadSelection(device);
  if (selected.device == nullptr || diffuse == nullptr) return {};
  std::shared_ptr<Target> result;
  targets.if_contains(diffuse, [&](const auto& entry) {
    const auto& [resource, target] = entry;
    if (target != nullptr && target->generation == selected.generation) {
      result = target;
    }
  });
  return result;
}

inline void OnSkyDrawn(reshade::api::command_list* cmd_list) {
  const auto selected = inputs::ReadSelection(cmd_list->get_device());
  if (selected.device == nullptr) return;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> output;
  Microsoft::WRL::ComPtr<ID3D11Resource> resource;
  context->OMGetRenderTargets(1u, &output, nullptr);
  if (output == nullptr) return;
  output->GetResource(&resource);
  // This tag identifies the producer family, not frame freshness. The actual
  // upsampler input is copied in GPU order, including cached native ambient data.
  resource->SetPrivateData(SKY_SOURCE_GUID, sizeof(selected.generation), &selected.generation);
}

inline void Capture(reshade::api::command_list* cmd_list) {
  // 1. Recognize the native ambient producer from its tagged half-size input.
  // t0 is ambient, t2 is full-size depth, and t3 is the 4x4 phase-weight table.
  const auto selected = inputs::ReadSelection(cmd_list->get_device());
  if (selected.device == nullptr) return;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  std::array<ID3D11ShaderResourceView*, 4> bound = {};
  context->PSGetShaderResources(0u, static_cast<UINT>(bound.size()), bound.data());
  std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 4> inputs;
  for (UINT index = 0u; index < bound.size(); ++index) {
    inputs[index].Attach(bound[index]);
  }
  if (inputs[0] == nullptr || inputs[2] == nullptr || inputs[3] == nullptr) return;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> output;
  Microsoft::WRL::ComPtr<ID3D11Resource> destination;
  Microsoft::WRL::ComPtr<ID3D11Resource> source;
  Microsoft::WRL::ComPtr<ID3D11Resource> depth;
  Microsoft::WRL::ComPtr<ID3D11Resource> weights;
  context->OMGetRenderTargets(1u, &output, nullptr);
  if (output == nullptr) return;
  output->GetResource(&destination);
  inputs[0]->GetResource(&source);
  inputs[2]->GetResource(&depth);
  inputs[3]->GetResource(&weights);
  uint64_t generation = 0u;
  UINT bytes = sizeof(generation);
  if (FAILED(source->GetPrivateData(SKY_SOURCE_GUID, &bytes, &generation)) || generation != selected.generation) return;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> source_texture;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> output_texture;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_texture;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> weight_texture;
  if (FAILED(source.As(&source_texture)) || FAILED(destination.As(&output_texture))
      || FAILED(depth.As(&depth_texture)) || FAILED(weights.As(&weight_texture))) return;
  D3D11_TEXTURE2D_DESC source_desc = {};
  D3D11_TEXTURE2D_DESC output_desc = {};
  D3D11_TEXTURE2D_DESC depth_desc = {};
  D3D11_TEXTURE2D_DESC weight_desc = {};
  source_texture->GetDesc(&source_desc);
  output_texture->GetDesc(&output_desc);
  depth_texture->GetDesc(&depth_desc);
  weight_texture->GetDesc(&weight_desc);
  // 2. Qualify the exact reconstruction convention: texture layouts, full-size
  // viewport, point-wrap sampler s8, and matching vertex/pixel b5 ranges. A
  // different upsampler must not be mistaken for the ambient contribution.
  D3D11_SHADER_RESOURCE_VIEW_DESC source_view = {};
  D3D11_SHADER_RESOURCE_VIEW_DESC weight_view = {};
  inputs[0]->GetDesc(&source_view);
  inputs[3]->GetDesc(&weight_view);
  if (source_view.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D || source_view.Texture2D.MostDetailedMip != 0u
      || source_view.Format != DXGI_FORMAT_R16G16B16A16_FLOAT
      || weight_view.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D || weight_view.Texture2D.MostDetailedMip != 0u
      || source_desc.MipLevels != 1u || source_desc.ArraySize != 1u || source_desc.SampleDesc.Count != 1u
      || weight_desc.MipLevels != 1u || weight_desc.ArraySize != 1u || weight_desc.SampleDesc.Count != 1u
      || weight_desc.Width != 4u || weight_desc.Height != 4u
      || (weight_view.Format != DXGI_FORMAT_B8G8R8A8_UNORM && weight_view.Format != DXGI_FORMAT_R8G8B8A8_UNORM)
      || source_desc.Width != output_desc.Width / 2u || source_desc.Height != output_desc.Height / 2u
      || depth_desc.Width != output_desc.Width || depth_desc.Height != output_desc.Height
      || (output_desc.Format != DXGI_FORMAT_R16G16B16A16_TYPELESS && output_desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT)
      || output_desc.MipLevels != 1u || output_desc.ArraySize != 1u || output_desc.SampleDesc.Count != 1u) return;
  D3D11_RENDER_TARGET_VIEW_DESC output_view = {};
  output->GetDesc(&output_view);
  if (output_view.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D || output_view.Texture2D.MipSlice != 0u
      || output_view.Format != DXGI_FORMAT_R16G16B16A16_FLOAT) return;
  UINT viewport_count = 1u;
  D3D11_VIEWPORT viewport = {};
  context->RSGetViewports(&viewport_count, &viewport);
  if (viewport_count != 1u || viewport.TopLeftX != 0.f || viewport.TopLeftY != 0.f
      || viewport.Width != static_cast<float>(output_desc.Width) || viewport.Height != static_cast<float>(output_desc.Height)) return;
  Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
  context->PSGetSamplers(8u, 1u, &sampler);
  if (sampler == nullptr) return;
  D3D11_SAMPLER_DESC sampler_desc = {};
  sampler->GetDesc(&sampler_desc);
  if (sampler_desc.Filter != D3D11_FILTER_MIN_MAG_MIP_POINT || sampler_desc.AddressU != D3D11_TEXTURE_ADDRESS_WRAP
      || sampler_desc.AddressV != D3D11_TEXTURE_ADDRESS_WRAP) return;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context1;
  Microsoft::WRL::ComPtr<ID3D11Buffer> pixel_constants;
  Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_constants;
  UINT first = 0u;
  UINT count = 0u;
  UINT vertex_first = 0u;
  UINT vertex_count = 0u;
  if (SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&context1)))) {
    context1->PSGetConstantBuffers1(5u, 1u, &pixel_constants, &first, &count);
    context1->VSGetConstantBuffers1(5u, 1u, &vertex_constants, &vertex_first, &vertex_count);
  } else {
    context->PSGetConstantBuffers(5u, 1u, &pixel_constants);
    context->VSGetConstantBuffers(5u, 1u, &vertex_constants);
  }
  if (pixel_constants == nullptr || pixel_constants != vertex_constants || first != vertex_first || count != vertex_count) return;
  D3D11_BUFFER_DESC constant_desc = {};
  pixel_constants->GetDesc(&constant_desc);
  if (context1 == nullptr) {
    count = constant_desc.ByteWidth / 16u;
  }
  if (count < 13u || (static_cast<uint64_t>(first) * 16u) + 208u > constant_desc.ByteWidth) return;
  // 3. Reuse snapshots while their allocation properties and binding ranges
  // agree. The native diffuse destination identifies which scene owns them.
  auto target = Read(cmd_list->get_device(), destination.Get());
  // Allocation properties are immutable; cache them instead of querying our
  // own snapshot resources on every native ambient draw.
  if (target == nullptr || target->source_width != source_desc.Width || target->source_height != source_desc.Height
      || target->constant_bytes != constant_desc.ByteWidth || target->depth.Get() != depth.Get()
      || target->weight_format != weight_view.Format || target->source_format != source_desc.Format
      || target->sampler.Get() != sampler.Get() || target->first_constant != first || target->constant_count != count) {
    auto created = std::make_shared<Target>();
    created->generation = selected.generation;
    created->width = output_desc.Width;
    created->height = output_desc.Height;
    created->source_width = source_desc.Width;
    created->source_height = source_desc.Height;
    created->constant_bytes = constant_desc.ByteWidth;
    created->source_format = source_desc.Format;
    created->weight_format = weight_view.Format;
    created->depth = depth;
    created->sampler = sampler;
    created->first_constant = first;
    created->constant_count = count;
    auto* device = reinterpret_cast<ID3D11Device*>(cmd_list->get_device()->get_native());  // NOLINT(performance-no-int-to-ptr)
    source_desc.Usage = D3D11_USAGE_DEFAULT;
    source_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    source_desc.CPUAccessFlags = source_desc.MiscFlags = 0u;
    weight_desc.Usage = D3D11_USAGE_DEFAULT;
    weight_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    weight_desc.CPUAccessFlags = weight_desc.MiscFlags = 0u;
    constant_desc.Usage = D3D11_USAGE_DEFAULT;
    constant_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constant_desc.CPUAccessFlags = constant_desc.MiscFlags = constant_desc.StructureByteStride = 0u;
    const D3D11_RENDER_TARGET_VIEW_DESC clear_desc = {.Format = DXGI_FORMAT_R16G16B16A16_FLOAT, .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D};
    if (FAILED(device->CreateTexture2D(&source_desc, nullptr, &created->image))
        || FAILED(device->CreateShaderResourceView(created->image.Get(), &source_view, &created->view))
        || FAILED(device->CreateRenderTargetView(created->image.Get(), &clear_desc, &created->clear_view))
        || FAILED(device->CreateTexture2D(&weight_desc, nullptr, &created->weights))
        || FAILED(device->CreateShaderResourceView(created->weights.Get(), &weight_view, &created->weight_view))
        || FAILED(device->CreateBuffer(&constant_desc, nullptr, &created->constants))) return;
    const std::array<float, 4> missing = {0.f, 0.f, 0.f, -1.f};
    context->ClearRenderTargetView(created->clear_view.Get(), missing.data());
    std::shared_ptr<Target> retired;
    targets.erase_if(destination.Get(), [&](auto& entry) {
      auto& [resource, previous] = entry;
      retired = std::move(previous);
      return true;
    });
    target = std::move(created);
    targets.insert_or_assign(destination.Get(), target);
  }
  // 4. Record copies beside the producer. These commands preserve pixels and
  // mutable constants in GPU order, including when the native list is replayed.
  context->CopyResource(target->image.Get(), source.Get());
  context->CopyResource(target->weights.Get(), weights.Get());
  context->CopyResource(target->constants.Get(), pixel_constants.Get());
  static std::atomic_flag reported = {};
  if (!reported.test_and_set(std::memory_order_relaxed)) {
    reshade::log::message(reshade::log::level::info, "MgsvLighting: ambient diffuse snapshot commands recorded before direct lighting");
  }
}

inline void OnDiffuseCopied(reshade::api::command_list* cmd_list,
                            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> prepared_source = {}) {
  const auto selected = inputs::ReadSelection(cmd_list->get_device());
  if (selected.device == nullptr) return;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> input;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> output;
  Microsoft::WRL::ComPtr<ID3D11Resource> source;
  Microsoft::WRL::ComPtr<ID3D11Resource> destination;
  context->PSGetShaderResources(0u, 1u, &input);
  context->OMGetRenderTargets(1u, &output, nullptr);
  if (input == nullptr || output == nullptr) return;
  input->GetResource(&source);
  output->GetResource(&destination);
  // The existing submission ledger publishes this edge after its native copy
  // and before the scatter insertion. Metadata needs no command-list split.
  temporal_inputs::Record(cmd_list, {
                                        .device = selected.device,
                                        .generation = selected.generation,
                                        .kind = temporal_inputs::ProducerKind::DIFFUSE_COPY,
                                        .copy_source = std::move(source),
                                        .copy_destination = std::move(destination),
                                        .prepared_source = std::move(prepared_source),
                                    });
}

inline bool OnClearRenderTarget(reshade::api::command_list* cmd_list, reshade::api::resource_view view,
                                const float* /*color*/, uint32_t /*count*/, const reshade::api::rect* /*rects*/) {
  // Clear validity beside the native diffuse clear. Until the ambient producer
  // overwrites this snapshot, application must leave direct lighting untouched.
  if (view.handle == 0u || cmd_list->get_device()->get_api() != reshade::api::device_api::d3d11) return false;
  Microsoft::WRL::ComPtr<ID3D11Resource> resource;
  reinterpret_cast<ID3D11RenderTargetView*>(view.handle)->GetResource(&resource);  // NOLINT(performance-no-int-to-ptr)
  const auto target = Read(cmd_list->get_device(), resource.Get());
  if (target != nullptr) {
    const std::array<float, 4> missing = {0.f, 0.f, 0.f, -1.f};
    reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native())->ClearRenderTargetView(target->clear_view.Get(), missing.data());  // NOLINT(performance-no-int-to-ptr)
  }
  return false;
}

inline void OnDestroyResource(reshade::api::device* /*device*/, reshade::api::resource resource) {
  std::shared_ptr<Target> retired;
  targets.erase_if(reinterpret_cast<ID3D11Resource*>(resource.handle), [&](auto& entry) {  // NOLINT(performance-no-int-to-ptr)
    auto& [native_diffuse, target] = entry;
    retired = std::move(target);
    return true;
  });
}

inline void Reset() {
  std::vector<std::shared_ptr<Target>> retired;
  targets.for_each_m([&](auto& entry) {
    auto& [resource, target] = entry;
    retired.push_back(std::move(target));
  });
  targets.clear();
}

}  // namespace mgsv::lighting::ambient_diffuse

#endif  // SRC_GAMES_MGSV_GTVBGI_AMBIENT_DIFFUSE_HPP_
