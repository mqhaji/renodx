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
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <utility>
#include <vector>

#include <d3d11.h>
#include <d3d11_1.h>
#include <wrl/client.h>
#include <wrl/implements.h>

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
#include "../../rendering/native_context.hpp"
#include "../frame_inputs.hpp"
#include "../resolution.hpp"
#include "../state.hpp"

namespace mgsv::reconstruction::post_process {

// Reconstruction can produce a larger scene than the game rendered. Later
// effects and UI need matching destinations, while earlier passes must keep
// their smaller native textures. This file owns those larger textures and the
// views used to read or write them; runtime.hpp chooses which draws use them.

// Native bloom processes this fixed rectangle inside a reduced-size texture.
// These are the working area's dimensions, not the texture's full dimensions.
inline constexpr uint32_t BLOOM_GRID_WIDTH = 246u;
inline constexpr uint32_t BLOOM_GRID_HEIGHT = 138u;
// Recognize native intermediates from full size down to 1/16 of each axis.
inline constexpr uint32_t MAX_SURFACE_REDUCTION = 4u;
// Preserve the game's cutoff for small depth-of-field blur spots:
// min(maximum, output_width * reference_cutoff / reference_width).
// The reference width comes from the game; it is not the player's resolution.
inline constexpr float DOF_REFERENCE_WIDTH = 1920.f;
inline constexpr float DOF_REFERENCE_CUTOFF = 0.85f;
inline constexpr float DOF_MAXIMUM_CUTOFF = 2.f;

// Values shared by the small size-adjustment shaders and native-buffer copies.
// Each row is four floats (16 bytes), matching one shader constant. Native
// buffers receive only the fields they use; unrelated values stay unchanged.
// UV coordinates run from 0 to 1 across a texture; a reciprocal is 1/value.
struct SizePatch {
  std::array<float, 4> buffer_size;   // This texture's width, height, and their reciprocals.
  std::array<float, 4> output_size;   // Full output width/height in XY; ZW unused.
  std::array<float, 4> dof_cutoff;    // Small-blur cutoff in X; other components unused.
  std::array<float, 4> sample_step;   // One texture pixel in UV units in XY; ZW remain 1.
  std::array<float, 4> flare_center;  // Fixed bloom center in UV units; Z is 1/length(center).
  std::array<float, 4> flare_edge;    // Reciprocal center coordinates for the flare's edge fade.
  std::array<float, 4> bloom_crop;    // UV offset and scale for copying the fixed bloom region.
};
static_assert(sizeof(SizePatch) == 112u);

// Private-data keys label D3D objects. Their numeric values are unique names,
// not rendering parameters. The scene key retains its native depth/stencil view.
inline constexpr GUID SCENE_DEPTH_GUID = {
    .Data1 = 0x240B06A8u,
    .Data2 = 0xD21Fu,
    .Data3 = 0x415Eu,
    .Data4 = {0x9Eu, 0x64u, 0xF2u, 0x25u, 0xC6u, 0xE1u, 0x99u, 0x43u},
};

// Stores the most recent full clear, so an alias created later starts with
// the same color and alpha as the native target.
struct ClearColor {
  std::array<float, 4> color;
  uint64_t scene = 0u;
  uint64_t generation = 0u;
};

struct RecordedClear {
  Microsoft::WRL::ComPtr<ID3D11Resource> resource;
  ClearColor value;
};

// Retrieve the exact depth/stencil view observed alongside this scene target.
inline Microsoft::WRL::ComPtr<ID3D11DepthStencilView> ReadSceneDepth(ID3D11Resource* scene) {
  Microsoft::WRL::ComPtr<IUnknown> object;
  Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth;
  UINT bytes = sizeof(IUnknown*);
  if (scene != nullptr && SUCCEEDED(scene->GetPrivateData(SCENE_DEPTH_GUID, &bytes, object.GetAddressOf())) && object != nullptr) {
    object.As(&depth);
  }
  return depth;
}

// Give the copy helpers a clean set of D3D bindings, then restore the caller's
// state on every exit. This changes bindings, not the contents of textures.
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

// One native intermediate and its matching output-sized texture. A texture
// stores pixels; a view specifies how a shader reads or writes that storage.
struct Surface {
  // Role describes the use established by a qualified draw or copy. Native
  // texture pooling can reuse the same object for a different purpose later.
  enum class Role : uint8_t {
    DATA,              // Effect data, such as a DoF mask; not a generic scene-color input.
    SCENE_COLOR,       // Scene image or a color layer that can feed scene composition.
    BLOOM,             // Bloom or flare color using the texture's regular sampling region.
    BLOOM_FIXED_GRID,  // Bloom or flare color confined to the fixed working rectangle.
    FLARE_LAYER        // Separate depth-tested flare sprites blended into scene color later.
  };
  Microsoft::WRL::ComPtr<ID3D11Resource> original;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  Microsoft::WRL::ComPtr<ID3D11Buffer> size_patch;
  // Allocation alone does not make this texture a valid current input.
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
  // Reuse equivalent read/write views instead of recreating them each draw.
  // Retaining the native views also prevents their identities from being reused.
  std::vector<TargetView> targets;
  std::vector<SampleView> samples;
};

// These objects depend on the device, not on the current render/output size.
struct Pipelines {
  reshade::api::device* device;
  // Fallback resizing, native constant adjustment, and sampled-depth alignment.
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> fallback_shader;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> system_shader;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> bloom_shader;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> align_depth_shader;
  // A fullscreen triangle writes enlarged UI depth and stencil without color.
  Microsoft::WRL::ComPtr<ID3D11VertexShader> copy_vertex;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> copy_depth_pixel;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> copy_stencil_pixel;
  Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_write;
  // Each masked draw restores one stencil bit. Reuse these eight write masks
  // and bit selectors across frames and size changes.
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

// Each scene keeps the roles and initialization of its output intermediates.
// Surface bindings use Output::surface_mutex. Clear records and reconstruction
// admission use State::mutex so publication and lookup see the same scene.
struct OutputProducer {
  struct SurfaceBinding {
    ID3D11Resource* resource;  // Borrowed identity; Output retains the texture.
    Surface::Role role;
  };
  // The output owns the textures. Each producer records their roles separately:
  // the next scene can reuse a texture while this scene is still being recorded.
  std::vector<SurfaceBinding> surfaces;
  // Set under State::mutex once the reconstruction insertion is recorded.
  // Earlier UI clears can reserve storage without enabling scene routing.
  bool reconstruction_recorded = false;
  // Display recording can finish before the flare worker. Keep its clear
  // colors with this scene until the output is released or reused.
  std::vector<RecordedClear> clears;
  // These clears are commands in the native stream. Replaying that stream
  // initializes the same output again before its blended or cropped writes.
  std::vector<ID3D11Resource*> cleared_surfaces;

  void SetSurfaceRole(ID3D11Resource* resource, Surface::Role role) {
    const auto binding = std::ranges::find(surfaces, resource, &SurfaceBinding::resource);
    if (binding == surfaces.end()) {
      surfaces.push_back({.resource = resource, .role = role});
    } else {
      binding->role = role;
    }
  }
};

// Keep each output and its post-processing aliases alive through submission.
// Native resources remain available to earlier, render-sized producers.
struct Output {
  // Protects the intermediate/view caches, their roles, and producer bindings.
  // Cache hits can share this lock; new views and role changes need exclusive access.
  std::shared_mutex surface_mutex;
  // Protected by State::mutex once this allocation enters the pool.
  bool reusable = true;
  std::vector<std::shared_ptr<Surface>> surfaces;
  std::shared_ptr<Pipelines> pipelines;
  reshade::api::device* device = nullptr;
  uint32_t render_width = 0u;
  uint32_t render_height = 0u;
  // Both native scene identities can appear in later bindings: the upgraded
  // float texture and the game's original texture that it replaces.
  Microsoft::WRL::ComPtr<ID3D11Resource> source;
  Microsoft::WRL::ComPtr<ID3D11Resource> original_source;
  // Reconstruction writes here. Subsequent native effects and UI can read or
  // update it through the sample view (srv) and render-target view (rtv).
  // Float storage does not change MGSV's expected scene-color encoding.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> color;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
  // Immutable size-dependent values and the original scene's fallback input.
  Microsoft::WRL::ComPtr<ID3D11Buffer> size_patch;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> source_view;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> fallback_uav;
  // Saved-state support and dimensions/jitter shared by the copy shaders.
  Microsoft::WRL::ComPtr<ID3DDeviceContextState> empty_state;
  Microsoft::WRL::ComPtr<ID3D11Buffer> copy_constants;
  // Geometry depth is sampled by scene effects. Align it to the reconstructed
  // image's pixel grid, including the current camera jitter.
  Microsoft::WRL::ComPtr<ID3D11Resource> geometry_depth;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> geometry_depth_view;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> aligned_depth;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> aligned_depth_view;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> aligned_depth_uav;
  // UI depth/stencil can be a different native resource. Stencil is the
  // per-pixel mask used for UI clipping. Copy to readable storage first,
  // then restore the values into an output-sized depth/stencil target.
  Microsoft::WRL::ComPtr<ID3D11Resource> native_depth;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_copy;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth_view;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> stencil_view;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> output_depth;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> output_depth_sample;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> output_stencil_sample;
  // Indexed by D3D11's read-only depth/stencil flag bits: neither, depth,
  // stencil, or both. Views are created only when that flag combination is used.
  std::array<Microsoft::WRL::ComPtr<ID3D11DepthStencilView>, 4> output_depth_views;
  ColorTarget target;
  // One bit per diagnostic message group, to avoid repeating success logs.
  std::atomic<uint32_t> observed_passes = 0u;
  // Allows a UI-only scene to acquire the same resource layout without
  // borrowing an older scene's pixels or its outstanding command lists.
  ValidatedFrameInputs allocation_inputs;

  explicit Output(const ValidatedFrameInputs& inputs, std::shared_ptr<Pipelines> shared_pipelines = nullptr)
      : pipelines(std::move(shared_pipelines)), device(inputs.device), render_width(inputs.render_width), render_height(inputs.render_height), allocation_inputs(inputs) {
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
    // Color needs read, render-target, and compute-write access. Depth resources
    // below reuse the dimensions but choose their own formats and access flags.
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
    // Depth association comes from an actual target binding, not a size guess.
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
    // Typeless storage permits separate typed views of depth and stencil.
    // Preserve the native depth representation rather than converting formats.
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

  bool MatchesInputs(const ValidatedFrameInputs& inputs) const {
    if (device != inputs.device || reinterpret_cast<uint64_t>(source.Get()) != inputs.color_resource.handle
        || render_width != inputs.render_width || render_height != inputs.render_height
        || target.width != inputs.output.width || target.height != inputs.output.height
        || target.format != inputs.color_format
        || reinterpret_cast<uint64_t>(geometry_depth.Get()) != inputs.depth_resource.handle) return false;
    Microsoft::WRL::ComPtr<ID3D11Resource> current_depth;
    if (const auto view = ReadSceneDepth(source.Get()); view != nullptr) {
      view->GetResource(&current_depth);
    }
    return native_depth == current_depth;
  }

  // Canceled reconstruction still has a native remainder that reads this
  // target. Fill it from this submission's native prefix, never old output.
  void CopyNative(ID3D11DeviceContext* context) const {
    if (!IsScaled()) {
      context->CopyResource(color.Get(), source.Get());
      return;
    }
    const ContextScope restore(context, empty_state.Get());
    if (restore.saved == nullptr) {
      resolution::Fail("cannot preserve the reconstruction fallback state");
      return;
    }
    context->CSSetShader(pipelines->fallback_shader.Get(), nullptr, 0u);
    context->CSSetShaderResources(0u, 1u, source_view.GetAddressOf());
    context->CSSetUnorderedAccessViews(0u, 1u, fallback_uav.GetAddressOf(), nullptr);
    context->CSSetConstantBuffers(0u, 1u, copy_constants.GetAddressOf());
    context->Dispatch((target.width + 7u) / 8u, (target.height + 7u) / 8u, 1u);
  }

  // Called on the immediate context after the native prefix has executed.
  // Always rebuild current depth/masks on replay; CPU recording cannot seed it.
  bool PrepareDepth(ID3D11DeviceContext* context, float jitter_x, float jitter_y) const {
    if (!IsScaled()) return true;
    const ContextScope restore(context, empty_state.Get());
    if (restore.saved == nullptr) {
      resolution::Fail("cannot preserve the output depth copy state");
      return false;
    }
    // The first row stores integer input/output dimensions. The second stores
    // a reserved mode, two float jitter values as raw bits, and padding.
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
    // Keep sampled geometry separate from the UI's depth and stencil tests.
    if (native_depth == nullptr) return true;
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
    return true;
  }

  [[nodiscard]] bool IsSceneTarget(ID3D11Resource* resource) const {
    return resource != nullptr && (resource == source.Get() || resource == original_source.Get());
  }

  [[nodiscard]] bool IsScaled() const {
    return render_width != 0u && render_height != 0u && (render_width != target.width || render_height != target.height);
  }

  // Build once for each texture extent. Buffer dimensions can be reduced while
  // output_size still describes the full reconstructed scene.
  Microsoft::WRL::ComPtr<ID3D11Buffer> CreateSizePatch(uint32_t width, uint32_t height) const {
    const float inverse_width = 1.f / static_cast<float>(width);
    const float inverse_height = 1.f / static_cast<float>(height);
    // Bloom's working region stays fixed while its backing buffers resize.
    const std::array bloom_center = {
        inverse_width * (static_cast<float>(BLOOM_GRID_WIDTH) * 0.5f),
        inverse_height * (static_cast<float>(BLOOM_GRID_HEIGHT) * 0.5f),
    };
    const SizePatch values = {
        .buffer_size = {static_cast<float>(width), static_cast<float>(height), inverse_width, inverse_height},
        .output_size = {static_cast<float>(target.width), static_cast<float>(target.height), 0.f, 0.f},
        .dof_cutoff = {std::min(DOF_MAXIMUM_CUTOFF, static_cast<float>(target.width) * DOF_REFERENCE_CUTOFF * (1.f / DOF_REFERENCE_WIDTH)), 0.f, 0.f, 0.f},
        .sample_step = {inverse_width, inverse_height, 1.f, 1.f},
        .flare_center = {bloom_center[0], bloom_center[1],
                         1.f / std::sqrt((bloom_center[0] * bloom_center[0]) + (bloom_center[1] * bloom_center[1])), 0.f},
        .flare_edge = {1.f / bloom_center[0], 1.f / bloom_center[1], 0.f, 0.f},
        .bloom_crop = {-0.5f * inverse_width, -0.5f * inverse_height,
                       static_cast<float>(BLOOM_GRID_WIDTH) / (static_cast<float>(target.width) * 0.25f),
                       static_cast<float>(BLOOM_GRID_HEIGHT) / (static_cast<float>(target.height) * 0.25f)},
    };
    const D3D11_BUFFER_DESC description = {
        .ByteWidth = sizeof(values),
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
    };
    const D3D11_SUBRESOURCE_DATA initial = {.pSysMem = &values};
    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    auto* native_device = reinterpret_cast<ID3D11Device*>(device->get_native());  // NOLINT(performance-no-int-to-ptr)
    if (FAILED(native_device->CreateBuffer(&description, &initial, &buffer))) {
      throw std::runtime_error("MGSV output-size constants allocation failed");
    }
    return buffer;
  }

  // Lookup can require a producer from this recording, not just cached storage.
  // create permits allocation lookup without claiming that pixels were written;
  // MapTarget or CopySurface records that producer separately.
  std::shared_ptr<Surface> GetSurface(ID3D11Resource* original, bool create,
                                      uint64_t recording_frame = reconstruction::state::CurrentFrameToken(),
                                      const OutputProducer* producer = nullptr, Surface::Role* role = nullptr) {
    if (original == nullptr || render_width == 0u || render_height == 0u) return nullptr;
    // Called with either lock held, including the recheck after a cache miss.
    const auto find_surface = [&]() -> std::shared_ptr<Surface> {
      for (const auto& surface : surfaces) {
        if (surface->original.Get() == original) {
          const OutputProducer::SurfaceBinding* binding = nullptr;
          if (producer != nullptr) {
            const auto found = std::ranges::find(producer->surfaces, original, &OutputProducer::SurfaceBinding::resource);
            if (found != producer->surfaces.end()) {
              binding = &*found;
            }
          }
          // General intermediates can change purpose each frame. The dedicated
          // flare layer keeps its producer/consumer link: another thread can
          // record the scene copy before recording the current flare draws.
          if (!create && (producer != nullptr ? binding == nullptr : surface->routed_frame != recording_frame)
              && surface->role != Surface::Role::FLARE_LAYER) return nullptr;
          if (role != nullptr) {
            *role = (binding != nullptr ? binding->role : surface->role);
          }
          return surface;
        }
      }
      return nullptr;
    };
    {
      const std::shared_lock lock(surface_mutex);
      if (const auto surface = find_surface(); surface != nullptr) return surface;
    }
    if (!create) return nullptr;
    const std::unique_lock lock(surface_mutex);
    // Another recording worker may have filled the cache while we changed locks.
    if (const auto surface = find_surface(); surface != nullptr) return surface;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    if (FAILED(original->QueryInterface(IID_PPV_ARGS(&texture)))) return nullptr;
    D3D11_TEXTURE2D_DESC description;
    texture->GetDesc(&description);
    if (description.ArraySize != 1u || description.MipLevels != 1u || description.SampleDesc.Count != 1u) return nullptr;
    uint32_t level = 0u;
    for (; level <= MAX_SURFACE_REDUCTION; ++level) {
      if (description.Width == (render_width >> level) && description.Height == (render_height >> level)) break;
    }
    // Fixed-size UI and utility targets share these draw paths. Their size
    // alone does not make them part of the output-resolution scene chain.
    if (level > MAX_SURFACE_REDUCTION) return nullptr;
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

  // create allows new storage and views. With create enabled, record_write also
  // updates their role. Begin delays that update until the draw passes validation.
  // Existing targets still need a valid role; scene_only excludes effect data.
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> MapTarget(ID3D11RenderTargetView* view, bool create, bool scene_only = false,
                                                           Surface::Role role = Surface::Role::DATA,
                                                           uint64_t recording_frame = reconstruction::state::CurrentFrameToken(),
                                                           OutputProducer* producer = nullptr, bool record_write = true) {
    if (view == nullptr) return nullptr;
    Microsoft::WRL::ComPtr<ID3D11Resource> original;
    view->GetResource(&original);
    if (IsSceneTarget(original.Get())) return rtv;
    auto previous_role = Surface::Role::DATA;
    const auto surface = GetSurface(original.Get(), create, recording_frame, producer, &previous_role);
    if (surface == nullptr) return nullptr;
    if (scene_only && previous_role == Surface::Role::DATA && !create) return nullptr;
    const auto find_view = [&]() -> ID3D11RenderTargetView* {
      for (const auto& target_view : surface->targets) {
        if (target_view.original.Get() == view) return target_view.replacement.Get();
      }
      return nullptr;
    };
    if (!create) {
      const std::shared_lock lock(surface_mutex);
      if (auto* cached = find_view(); cached != nullptr) return cached;
    }
    const std::unique_lock lock(surface_mutex);
    auto* mapped = find_view();
    if (mapped == nullptr) {
      D3D11_RENDER_TARGET_VIEW_DESC description;
      view->GetDesc(&description);
      Surface::TargetView target_view = {.original = view};
      auto* native_device = reinterpret_cast<ID3D11Device*>(device->get_native());  // NOLINT(performance-no-int-to-ptr)
      if (FAILED(native_device->CreateRenderTargetView(surface->texture.Get(), &description, &target_view.replacement))) {
        resolution::Fail("post-processing target view allocation failed");
        return nullptr;
      }
      surface->targets.push_back(std::move(target_view));
      mapped = surface->targets.back().replacement.Get();
    }
    if (create && record_write) {
      surface->routed_frame = recording_frame;
      surface->role = (scene_only ? Surface::Role::SCENE_COLOR : role);
      if (producer != nullptr) {
        producer->SetSurfaceRole(original.Get(), surface->role);
      }
    }
    return mapped;
  }

  // Redirect reads only when this recording has a matching scene, depth, or
  // intermediate. A missing match leaves the caller's native binding unchanged.
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> MapInput(ID3D11ShaderResourceView* view, bool scene_only = false,
                                                            uint64_t recording_frame = reconstruction::state::CurrentFrameToken(),
                                                            const OutputProducer* producer = nullptr) {
    if (view == nullptr) return nullptr;
    Microsoft::WRL::ComPtr<ID3D11Resource> original;
    view->GetResource(&original);
    // The float scene uses explicit encoding. Leave unrelated native sRGB
    // views to the existing clone path rather than changing their transfer.
    if (original == source) return srv;
    if (original == original_source) {
      D3D11_SHADER_RESOURCE_VIEW_DESC description;
      view->GetDesc(&description);
      // Native UNORM and float scene views both carry encoded color. A draw
      // using the native alias still needs this frame's reconstructed image.
      // An sRGB view would decode on read, so it cannot use the same float SRV.
      if (description.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D && description.Texture2D.MostDetailedMip == 0u
          && (description.Format == DXGI_FORMAT_B8G8R8A8_UNORM || description.Format == DXGI_FORMAT_R8G8B8A8_UNORM)) {
        return srv;
      }
    }
    // Stencil is integer mask data, not sampled geometry. Keep its view separate
    // even when the game's geometry and UI share one depth/stencil texture.
    if (IsScaled() && original == native_depth) {
      D3D11_SHADER_RESOURCE_VIEW_DESC description;
      view->GetDesc(&description);
      if (description.Format == DXGI_FORMAT_X24_TYPELESS_G8_UINT || description.Format == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT) return output_stencil_sample;
      if (original != geometry_depth) return output_depth_sample;
    }
    if (IsScaled() && original == geometry_depth) return aligned_depth_view;
    auto role = Surface::Role::DATA;
    const auto surface = GetSurface(original.Get(), false, recording_frame, producer, &role);
    if (surface == nullptr) return nullptr;
    if (scene_only && role == Surface::Role::DATA) return nullptr;
    const auto find_view = [&]() -> ID3D11ShaderResourceView* {
      for (const auto& sample : surface->samples) {
        if (sample.original.Get() == view) return sample.replacement.Get();
      }
      return nullptr;
    };
    {
      const std::shared_lock lock(surface_mutex);
      if (auto* cached = find_view(); cached != nullptr) return cached;
    }
    const std::unique_lock lock(surface_mutex);
    // Recheck after changing locks so concurrent misses share one cached view.
    if (auto* cached = find_view(); cached != nullptr) return cached;
    D3D11_SHADER_RESOURCE_VIEW_DESC description;
    view->GetDesc(&description);
    Surface::SampleView sample = {.original = view};
    auto* native_device = reinterpret_cast<ID3D11Device*>(device->get_native());  // NOLINT(performance-no-int-to-ptr)
    if (FAILED(native_device->CreateShaderResourceView(surface->texture.Get(), &description, &sample.replacement))) {
      resolution::Fail("post-processing sample view allocation failed");
      return nullptr;
    }
    surface->samples.push_back(std::move(sample));
    return surface->samples.back().replacement;
  }

  // Mirror a qualified full-resource copy using matching output-sized storage.
  // An intermediate destination inherits the source's role after size/format checks.
  bool CopySurface(ID3D11DeviceContext* context, ID3D11Resource* from, ID3D11Resource* to,
                   uint64_t recording_frame = reconstruction::state::CurrentFrameToken(), OutputProducer* producer = nullptr) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> copied;
    auto role = Surface::Role::DATA;
    if (IsSceneTarget(from)) {
      copied = color;
      role = Surface::Role::SCENE_COLOR;
    } else if (const auto surface = GetSurface(from, false, recording_frame, producer, &role); surface != nullptr) {
      copied = surface->texture;
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
    const auto destination = GetSurface(to, true, recording_frame);
    if (destination == nullptr) return false;
    destination->texture->GetDesc(&destination_description);
    if (source_description.Width != destination_description.Width || source_description.Height != destination_description.Height
        || source_description.Format != destination_description.Format) return false;
    {
      const std::unique_lock lock(surface_mutex);
      destination->routed_frame = recording_frame;
      destination->role = role;
      if (producer != nullptr) {
        producer->SetSurfaceRole(to, role);
      }
    }
    context->CopyResource(destination->texture.Get(), copied.Get());
    return true;
  }
};

// Keep one destination and producer for an unfinished deferred recording.
// Successful FinishCommandList releases this reference; the wider sequence
// below can retain the same route until display conversion. Reset retires both.
struct __declspec(uuid("71618f64-fc26-4a78-91de-6e20f879a85a")) Recording {
  std::shared_ptr<Output> output;
  std::shared_ptr<OutputProducer> producer;
  uint64_t frame = 0u;       // Recording interval used to qualify intermediate writes.
  uint64_t generation = 0u;  // Reset counter used to reject retired output resources.
  rendering::native_context::Owner owner;
  // A recording can finish several scene sequences before FinishCommandList.
  // Keep every referenced output, even after display conversion closes a route.
  std::vector<std::shared_ptr<Output>> retained_outputs;
};

inline constexpr GUID OUTPUT_LEASE_GUID = {
    .Data1 = 0x5241c06eu,
    .Data2 = 0x5925u,
    .Data3 = 0x4201u,
    .Data4 = {0x96, 0xab, 0x68, 0x31, 0xed, 0xbc, 0x5d, 0xf0},
};

// The native list owns this object. It owns no list reference, so destroying
// the last recorded user makes its output available to the pool again.
struct RetainedOutputs : Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IUnknown> {
  std::vector<std::shared_ptr<Output>> outputs;
};

// Native post-processing can finish one list and continue on another while
// another worker publishes the next scene. Keep each allocation's scene identity
// until reuse, including UI workers that record after display conversion.
struct State {
  std::shared_mutex mutex;
  // Shader/state objects can be reused at any size. Cached textures match one
  // render/output size pair; outstanding recordings can retain older textures.
  std::shared_ptr<Pipelines> pipelines;
  // cached can serve independently recorded UI even without a new scene.
  // active belongs to the native scene identified by active_owner.
  std::shared_ptr<Output> cached;
  std::vector<Recording> pool;
  std::shared_ptr<Output> active;
  std::shared_ptr<OutputProducer> producer;
  std::vector<Recording> sequences;
  // Clears recorded before a scene acquires output storage. AcquireOutput
  // transfers them to its producer. Startup scenes retire them at display.
  std::vector<RecordedClear> clears;
  uint64_t frame = 0u;
  // Invalidates old Recording values without relying on CPU/GPU timing.
  uint64_t generation = 0u;
  rendering::native_context::Owner active_owner;
};
inline State state;

// A later resource upgrade can transfer the same scene's earlier clear to its
// float clone. Copies and writes use the resulting native resource identity.
inline void RecordClear(ID3D11Resource* resource, const ClearColor& clear) {
  const std::unique_lock lock(state.mutex);
  const auto route = std::ranges::find_if(state.pool, [&](const Recording& candidate) {
    return candidate.owner.scene == clear.scene && candidate.owner.generation == clear.generation
           && candidate.producer != nullptr;
  });
  auto& clears = (route != state.pool.end() ? route->producer->clears : state.clears);
  const auto existing = std::ranges::find_if(clears, [&](const RecordedClear& candidate) {
    return candidate.resource.Get() == resource && candidate.value.scene == clear.scene
           && candidate.value.generation == clear.generation;
  });
  if (existing == clears.end()) {
    clears.push_back({.resource = resource, .value = clear});
  } else {
    existing->value = clear;
  }
}

// Return a value copy so callers can release the lock before recording GPU work.
inline std::optional<ClearColor> GetRecordedClear(ID3D11Resource* resource, uint64_t scene, uint64_t generation) {
  const std::shared_lock lock(state.mutex);
  const auto route = std::ranges::find_if(state.pool, [&](const Recording& candidate) {
    return candidate.owner.scene == scene && candidate.owner.generation == generation
           && candidate.producer != nullptr;
  });
  const auto& clears = (route != state.pool.end() ? route->producer->clears : state.clears);
  const auto existing = std::ranges::find_if(clears, [&](const RecordedClear& candidate) {
    return candidate.resource.Get() == resource && candidate.value.scene == scene
           && candidate.value.generation == generation;
  });
  if (existing == clears.end()) return std::nullopt;
  return existing->value;
}

// Pool ownership alone means no recording or finished native list can still
// read these textures. A second worker for the same scene shares its route.
// D3D11 orders subsequent GPU writes after earlier immediate submissions.
inline std::shared_ptr<Output> AcquireOutput(const ValidatedFrameInputs& inputs) {
  auto owner = rendering::native_context::GetScope().owner;
  if (owner.scene == 0u) {
    owner.scene = owner.presentation_scene;
  }
  const auto matches = [&](const Recording& route) {
    return route.owner.scene == owner.scene && route.owner.generation == owner.generation
           && route.output != nullptr && route.output->device == inputs.device;
  };
  std::shared_ptr<Pipelines> pipelines;
  std::shared_ptr<Output> output;
  std::vector<Recording> retired;
  uint64_t generation = 0u;
  {
    const std::unique_lock lock(state.mutex);
    if (const auto found = std::ranges::find_if(state.sequences, matches); found != state.sequences.end()) {
      return (found->output->MatchesInputs(inputs) ? found->output : nullptr);
    }
    if (const auto found = std::ranges::find_if(state.pool, matches); found != state.pool.end()) {
      return (found->output->MatchesInputs(inputs) ? found->output : nullptr);
    }
    generation = state.generation;
    pipelines = state.pipelines;
    // cached is a UI layout reference. Release it before checking pool users.
    // A scene route or a recorded list independently retains any live output.
    if (state.cached != nullptr && std::ranges::find(state.pool, state.cached, &Recording::output) == state.pool.end()) {
      retired.push_back({.output = std::move(state.cached)});
    } else {
      state.cached.reset();
    }
    for (auto entry = state.pool.begin(); entry != state.pool.end();) {
      if (!entry->output->reusable || entry->output.use_count() != 1) {
        ++entry;
        continue;
      }
      const auto& candidate = entry->output;
      if (!candidate->MatchesInputs(inputs)) {
        retired.push_back(std::move(*entry));
        entry = state.pool.erase(entry);
        continue;
      }
      if (output == nullptr) {
        output = candidate;
        // Claim the free allocation under the lock. An older scene lookup
        // must stop matching before another worker starts recording into it.
        entry->owner = {};
        // Clear records retain native resources. Release them after unlocking,
        // since resource destruction can invoke addon callbacks.
        retired.push_back({.producer = std::move(entry->producer)});
      }
      // Several lists can finish together. Keep compatible spares so the
      // next overlapping recordings can reuse that capacity without churn.
      ++entry;
    }
  }
  if (output == nullptr) {
    output = std::make_shared<Output>(inputs, std::move(pipelines));
  }
  {
    const std::unique_lock lock(state.mutex);
    if (generation != state.generation) return nullptr;
    if (const auto found = std::ranges::find_if(state.sequences, matches); found != state.sequences.end()) {
      return (found->output->MatchesInputs(inputs) ? found->output : nullptr);
    }
    if (const auto found = std::ranges::find_if(state.pool, matches); found != state.pool.end()) {
      return (found->output->MatchesInputs(inputs) ? found->output : nullptr);
    }
    Recording route = {.output = output, .producer = std::make_shared<OutputProducer>(), .frame = inputs.frame_token, .generation = generation, .owner = owner};
    // Publish the output and its earlier clears together. A worker can then
    // recover initialization even after display closes the open sequence.
    const auto pending = std::ranges::partition(state.clears, [&](const RecordedClear& clear) {
      return clear.value.scene != owner.scene || clear.value.generation != owner.generation;
    });
    route.producer->clears.assign(std::make_move_iterator(pending.begin()), std::make_move_iterator(pending.end()));
    state.clears.erase(pending.begin(), pending.end());
    const auto pooled = std::ranges::find(state.pool, output, &Recording::output);
    if (pooled == state.pool.end()) {
      state.pool.push_back(route);
    } else {
      *pooled = route;
    }
    state.pipelines = output->pipelines;
    state.cached = output;
    state.sequences.push_back(std::move(route));
  }
  return output;
}

inline void RetainOutput(reshade::api::command_list* command_list, const std::shared_ptr<Output>& output) {
  if (command_list == nullptr || output == nullptr) return;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  // Immediate commands already enter submission order. Only deferred recording
  // needs a lease that transfers to a finished, replayable native list.
  if (context == nullptr || context->GetType() != D3D11_DEVICE_CONTEXT_DEFERRED) return;
  auto* recording = command_list->get_private_data<Recording>();
  if (recording != nullptr
      && std::ranges::find(recording->retained_outputs, output) == recording->retained_outputs.end()) {
    recording->retained_outputs.push_back(output);
  }
}

// Match the recorded native scene at every lookup. A worker may serve another
// viewport between two calls, and a pooled target may belong to either view.
inline std::shared_ptr<Output> GetActive(reshade::api::command_list* command_list, Recording* selected = nullptr, bool include_ui = false) {
  const auto owner = rendering::native_context::GetScope().owner;
  const uint64_t scene = (owner.scene != 0u ? owner.scene : owner.presentation_scene);
  if (scene == 0u || !owner.presentation_matches
      || (owner.scene != 0u && (!owner.main_view || !owner.active_member))) return nullptr;
  const std::shared_lock lock(state.mutex);
  const auto matches = [&](const Recording& recording) {
    return recording.owner.generation == owner.generation && recording.owner.scene == scene
           && recording.generation == state.generation && recording.output != nullptr
           && recording.output->device == command_list->get_device()
           && (include_ui || recording.producer == nullptr || recording.producer->reconstruction_recorded);
  };
  const auto* recording = command_list->get_private_data<Recording>();
  if (recording != nullptr && matches(*recording)) {
    if (selected != nullptr) {
      *selected = {.output = recording->output, .producer = recording->producer, .frame = recording->frame, .generation = recording->generation, .owner = recording->owner};
    }
    return recording->output;
  }
  const auto sequence = std::ranges::find_if(state.sequences, matches);
  if (sequence != state.sequences.end()) {
    if (selected != nullptr) {
      *selected = *sequence;
    }
    return sequence->output;
  }
  // Display recording releases the open sequence, but other workers can still
  // record UI for the same scene. Its pooled route keeps the original pixels
  // and producer roles available until the allocation is assigned a new owner.
  const auto pooled = std::ranges::find_if(state.pool, matches);
  if (pooled != state.pool.end()) {
    if (selected != nullptr) {
      *selected = *pooled;
    }
    return pooled->output;
  }
  if (state.active_owner.generation != owner.generation || state.active_owner.scene != scene
      || state.active == nullptr || state.active->device != command_list->get_device()) return nullptr;
  if (selected != nullptr) {
    *selected = {.output = state.active, .producer = state.producer, .frame = state.frame, .generation = state.generation, .owner = state.active_owner};
  }
  return state.active;
}

// Retire routing immediately, but release retained D3D objects after unlocking:
// their destruction callbacks can enter the addon again. Device teardown also
// retires pipelines; a size/mode reset normally keeps those reusable objects.
inline void Reset(bool release_pipelines = false) {
  std::shared_ptr<Pipelines> pipelines;
  std::shared_ptr<Output> cached;
  std::shared_ptr<Output> active;
  std::vector<Recording> sequences;
  std::vector<Recording> pool;
  std::vector<RecordedClear> clears;
  {
    const std::unique_lock lock(state.mutex);
    cached.swap(state.cached);
    active.swap(state.active);
    sequences.swap(state.sequences);
    pool.swap(state.pool);
    clears.swap(state.clears);
    state.producer.reset();
    state.active_owner = {};
    ++state.generation;
    if (release_pipelines) {
      pipelines.swap(state.pipelines);
    }
  }
}

}  // namespace mgsv::reconstruction::post_process

#endif  // RENODX_GAMES_MGSV_RECONSTRUCTION_POSTPROCESS_RESOURCES_HPP_
