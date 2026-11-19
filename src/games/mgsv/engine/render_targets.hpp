/*
 * Copyright (C) 2026 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#ifndef RENODX_GAMES_MGSV_ENGINE_RENDER_TARGETS_HPP_
#define RENODX_GAMES_MGSV_ENGINE_RENDER_TARGETS_HPP_

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <system_error>

#include <include/reshade.hpp>

#include "../../../utils/detour.hpp"
#include "build.hpp"
#include "image.hpp"
#include "views.hpp"

// The engine creates every render target in RenderTargetDx11::Initialize, on
// first creation and on every resize, from a RenderTargetParameter that names
// the target and gives its format code. These hooks make the engine create
// selected 8-bit targets as RGBA16F itself, so the game binds the float texture
// directly and RenoDX keeps no clone of it. A plugin that keeps scene color in
// an 8-bit temporary other plugins share gets float images of its own instead.
namespace mgsv::engine::render_targets {

inline constexpr std::array REQUIRED_ENTRIES = {
    engine::Entry::RENDER_TARGET_INITIALIZE,
    engine::Entry::GET_BUFFER_FORMAT_FROM_PARAMETER,
    engine::Entry::RESIDENT_RESOURCE_GET_HANDLE,
    engine::Entry::MOTION_BLUR_WORK_BUFFER_CONSTRUCTOR,
    engine::Entry::DEPTH_OF_FIELD_RESOURCES_CONSTRUCTOR,
    engine::Entry::RESTORE_TEMPORARY_RENDER_TARGET,
    engine::Entry::RENDER_TARGET_CREATE_RESOURCE,
    engine::Entry::RENDER_TARGET_RESTORE_WITH_RESIZE,
    engine::Entry::RENDER_TARGET_MANAGER,
};
inline constexpr engine::Requirement REQUIREMENT = {.feature = "Float render targets (HDR precision)", .entries = REQUIRED_ENTRIES};

// Private data on the D3D texture of each bloom image: NEW until the bloom
// chain has cleared it once, because the chain writes only a working region.
inline constexpr GUID BLOOM_IMAGE_GUID = {
    .Data1 = 0x8B1F0C62u, .Data2 = 0x4D37u, .Data3 = 0x45A9u, .Data4 = {0xA4, 0x0E, 0x52, 0x9B, 0x7C, 0x13, 0xE6, 0x08}};
inline constexpr uint8_t BLOOM_IMAGE_NEW = 1u;
inline constexpr uint8_t BLOOM_IMAGE_CLEARED = 2u;
// Scene roles ReadSceneRole reports: the engine's scene color and its
// TEMPORARY_OUTPUT copy, the image DoF and motion blur copy the scene into, or
// the image the exposure meter copies its source into.
inline constexpr uint8_t SCENE_IMAGE = 1u;
inline constexpr uint8_t SCENE_COPY = 2u;
inline constexpr uint8_t EXPOSURE_SOURCE = 3u;

// The engine targets temporal reconstruction reads, in the order
// ReadFrameImages returns them.
enum class FrameTarget : uint8_t {
  SCENE_COLOR,
  DEPTH,
  VELOCITY,
  OBJECT_VELOCITY,
};

// A frame target's texture and the shader view the engine itself binds for it.
struct FrameImage {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_view;
};

namespace internal {

// fox::gr::dg::RenderTargetParameter. The low 48 bits of the name are
// StrCode64 of the target's name; the rest holds MSAA, kind, the two sRGB view
// flags, an alias handle and flags.
struct Parameter {
  uint64_t name = 0u;
  uint32_t unknown_08 = 0u;
  uint32_t width = 0u;
  uint32_t height = 0u;
  uint32_t format_code = 0u;
  std::array<uint32_t, 10> rest = {};
};
static_assert(sizeof(Parameter) == 0x40u && offsetof(Parameter, format_code) == 0x14u);

// The render target manager instance: +0x08 points to its 0x70-byte entries,
// each with its parameter at +0x28; +0x14 counts live targets. The two
// constructors look up the temporaries motion blur and DoF draw into, and
// restoreTemporaryRenderTarget resizes the resident temporaries.
// A render target handle is its entry's index above bit 17 and this type below.
inline constexpr uint32_t HANDLE_TYPE_MASK = 0x7Fu;
inline constexpr uint32_t RENDER_TARGET_HANDLE = 0x0Bu;
// RenderTargetDx11 keeps its gn texture at +0x40, which keeps the D3D texture at +0x10.
inline constexpr size_t TARGET_TEXTURE_OFFSET = 0x40u;
inline constexpr size_t TEXTURE_RESOURCE_OFFSET = 0x10u;
// It keeps its gn shader view at +0x50, which keeps the D3D view at +0x08.
inline constexpr size_t TARGET_SHADER_VIEW_OFFSET = 0x50u;
inline constexpr size_t VIEW_OBJECT_OFFSET = 0x08u;
// Code 1 is a B8G8R8A8 typeless texture with UNORM or sRGB views. Code 6 is
// R16G16B16A16 with FLOAT views; it ignores the sRGB flags, so shaders convert
// where a game view was sRGB (FIX_UNORM_SRGB).
inline constexpr uint32_t BGRA8_CODE = 1u;
inline constexpr uint32_t FLOAT_CODE = 6u;
inline constexpr uint64_t NAME_MASK = 0xFFFFFFFFFFFFull;

// Targets the engine creates as float, by name hash. The engine copies scene
// color into TEMPORARY_OUTPUT, so those two change together, and each _SRGB
// entry shares its plain entry's texture.
struct FloatTarget {
  uint64_t name = 0u;
  uint8_t scene_role = 0u;
  bool is_bloom_image = false;
};
inline constexpr std::array<FloatTarget, 16> FLOAT_TARGETS = {{
    {.name = 0x90EBFD910903ull, .scene_role = SCENE_IMAGE},  // PRIMARY_COLOR_BUFFER: scene color.
    {.name = 0x93E5B5BF4BF3ull, .scene_role = SCENE_IMAGE},  // Its _SRGB entry.
    {.name = 0x7D7461002EB9ull, .scene_role = SCENE_IMAGE},  // TEMPORARY_OUTPUT: the engine's copy of scene color.
    {.name = 0x072BA1800391ull, .scene_role = SCENE_IMAGE},  // Its _SRGB entry.
    {.name = 0x49478BB4864Full},                             // Color-grading LUT the ColorCorrection plugin blends into, 256 x 16.
    {.name = 0x56D9DC1D8044ull},                             // Its second LUT, created from the same parameter.
    {.name = 0xFE772C3902FDull},                             // GBUFFER_MATERIAL: the G-buffer velocity shaders' target (object motion).
    {.name = 0x803A4617E25Dull},                             // MotionBlurCameraVelocity's target (combined camera and object motion).
    {.name = 0x291B12D22FD6ull, .is_bloom_image = true},     // BLOOM_TEXTURE_LINEAR: first bloom working image.
    {.name = 0xE72182C2F7FCull, .is_bloom_image = true},     // BLOOM_TEXTURE: its _SRGB entry.
    {.name = 0x95727B067FABull, .is_bloom_image = true},     // Second bloom working image; DoF reuses it for control data.
    {.name = 0x084C86FE904Eull, .is_bloom_image = true},     // Its _SRGB entry.
    {.name = 0x2B60105D25B4ull, .is_bloom_image = true},     // Bloom result the late Tonemap pass reads.
    {.name = 0x5DB5220066EDull},                             // RENODX_MOTION_BLUR_0: motion blur's own half-size working image.
    {.name = 0x67B86CEA6100ull},                             // RENODX_MOTION_BLUR_1.
    {.name = 0x1DDB85A4EE59ull, .scene_role = SCENE_COPY},   // RENODX_SCENE_COPY: DoF's and motion blur's own full-size scene copy.
}};
// The exposure meter's first image. It is float already and keeps its format.
inline constexpr uint64_t EXPOSURE_SOURCE_NAME = 0x80726D23BE27ull;
// The frame targets by name hash, in FrameTarget order.
inline constexpr std::array<uint64_t, 4> FRAME_TARGETS = {
    0x90EBFD910903ull,  // PRIMARY_COLOR_BUFFER: scene color. Post passes bind this entry's view.
    0x5DD84A404FCBull,  // DEPTH_BUFFER: the G-buffer depth. Its read-only alias has no shader view.
    0x803A4617E25Dull,  // MotionBlurCameraVelocity's target.
    0xFE772C3902FDull,  // GBUFFER_MATERIAL: object motion.
};
// Each frame target's image, kept from the call that creates it until the
// engine creates it again or the device ends.
struct FrameImages {
  std::shared_mutex mutex;
  std::array<FrameImage, FRAME_TARGETS.size()> images;
};
static FrameImages frame_images;
// Private data on the D3D texture of an image with a scene role; it lives and dies with the texture.
inline constexpr GUID SCENE_ROLE_GUID = {
    .Data1 = 0x3E8C1D54u, .Data2 = 0x7A21u, .Data3 = 0x4F0Bu, .Data4 = {0x9C, 0x35, 0x6D, 0x1E, 0xB2, 0x40, 0x8A, 0x17}};

// Plugins whose constructors look up temporaries they keep scene color in.
inline constexpr uint8_t MOTION_BLUR = 1u;
inline constexpr uint8_t DEPTH_OF_FIELD = 2u;

// A float working image for those plugins alone. The engine lends its 8-bit
// temporaries to several plugins by name; a plugin that keeps scene color in
// one receives this image when its constructor looks that temporary up. The
// image copies the temporary's parameter and is resized with it.
struct PrivateTarget {
  uint64_t shared_name = 0u;
  uint64_t name = 0u;
  uint8_t plugins = 0u;
  uint32_t shared_handle = 0u;
  uint32_t handle = 0u;
};
// Motion blur keeps the scene in the two half-size temporaries where SSAO
// stores byte-packed depth and DoF its parameters. Its full-size mode and
// DoF's final composite keep it in the full-size temporary that rain's
// material layer fills with raw copies of the 8-bit G-buffer.
static std::array<PrivateTarget, 3> private_targets = {{
    {.shared_name = 0xB170EE7402F8ull, .name = 0x5DB5220066EDull, .plugins = MOTION_BLUR},                   // TEMPORARY_HALF_RGBA32_0.
    {.shared_name = 0x9609073845A4ull, .name = 0x67B86CEA6100ull, .plugins = MOTION_BLUR},                   // TEMPORARY_HALF_RGBA32_1.
    {.shared_name = 0x2E6254704542ull, .name = 0x1DDB85A4EE59ull, .plugins = MOTION_BLUR | DEPTH_OF_FIELD},  // TEMPORARY_FULL_RGBA32_0.
}};
// The plugin whose constructor is running on this thread.
static thread_local uint8_t constructing_plugin = 0u;

using Initialize = void(__fastcall*)(void* target, const Parameter* parameter, uint32_t texture_handle);
using ReadTextureFormat = uint32_t(__fastcall*)(const Parameter* parameter);
using Lookup = uint32_t(__fastcall*)(const uint64_t* name);
using ConstructResources = void*(__fastcall*)(void* resources);
using RestoreTemporaries = void(__fastcall*)(void* dg);
using Create = uint32_t(__fastcall*)(const Parameter* parameter);
using Resize = void(__fastcall*)(uint32_t handle, int32_t width, int32_t height);

struct HookState {
  std::shared_mutex mutex;
  Initialize initialize = nullptr;
  ReadTextureFormat read_texture_format = nullptr;
  Lookup lookup = nullptr;
  ConstructResources construct_motion_blur = nullptr;
  ConstructResources construct_depth_of_field = nullptr;
  RestoreTemporaries restore_temporaries = nullptr;
  // Engine functions and data these hooks use without replacing them.
  Create create = nullptr;
  Resize resize = nullptr;
  const uint8_t* const* manager = nullptr;
  size_t registrations = 0u;
  bool logged_failure = false;
};
static HookState hooks;
static std::atomic<uint32_t> calls_in_flight = 0u;
static std::atomic<bool> logged_format_mismatch = false;

struct __declspec(uuid("5C0B5B0E-6E0B-4E2B-9E0A-7C1D3B6C2F41")) DeviceState {
  bool hooks_installed = false;
};

// Detach waits for calls that entered a hook before removal.
struct CallScope {
  CallScope() { calls_in_flight.fetch_add(1u, std::memory_order_acq_rel); }
  ~CallScope() { calls_in_flight.fetch_sub(1u, std::memory_order_acq_rel); }
  CallScope(const CallScope&) = delete;
  CallScope& operator=(const CallScope&) = delete;
};

inline const FloatTarget* FindFloatTarget(uint64_t name) {
  const auto found = std::ranges::find(FLOAT_TARGETS, name & NAME_MASK, &FloatTarget::name);
  if (found == FLOAT_TARGETS.end()) return nullptr;
  return &*found;
}

inline void __fastcall HookInitialize(void* target, const Parameter* parameter, uint32_t texture_handle) {
  const CallScope scope;
  const auto* float_target = FindFloatTarget(parameter->name);
  const auto frame_target = std::ranges::find(FRAME_TARGETS, parameter->name & NAME_MASK);
  const bool is_exposure_source = (parameter->name & NAME_MASK) == EXPOSURE_SOURCE_NAME;
  // A private image already asks for the float code; it still gets its tags below.
  const bool is_float = float_target != nullptr && (parameter->format_code == BGRA8_CODE || parameter->format_code == FLOAT_CODE);
  if (is_float) {
    // Callers reuse one parameter for several targets, and the manager compares
    // its stored copy when a name is created twice. Only this call sees the
    // float code; an _SRGB alias entry is listed too, so both sides match.
    Parameter float_parameter = *parameter;
    float_parameter.format_code = FLOAT_CODE;
    hooks.initialize(target, &float_parameter, texture_handle);
  } else {
    hooks.initialize(target, parameter, texture_handle);
  }
  if (!is_float && frame_target == FRAME_TARGETS.end() && !is_exposure_source) return;

  const auto* texture = *reinterpret_cast<const uint8_t* const*>(static_cast<const uint8_t*>(target) + TARGET_TEXTURE_OFFSET);
  if (texture == nullptr) return;
  auto* resource = *reinterpret_cast<ID3D11Resource* const*>(texture + TEXTURE_RESOURCE_OFFSET);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> created;
  if (resource == nullptr || FAILED(resource->QueryInterface(IID_PPV_ARGS(&created)))) return;
  if (frame_target != FRAME_TARGETS.end()) {
    const auto* shader_view = *reinterpret_cast<const uint8_t* const*>(static_cast<const uint8_t*>(target) + TARGET_SHADER_VIEW_OFFSET);
    const std::unique_lock lock(frame_images.mutex);
    frame_images.images[static_cast<size_t>(frame_target - FRAME_TARGETS.begin())] = {
        .texture = created,
        .shader_view = (shader_view != nullptr ? *reinterpret_cast<ID3D11ShaderResourceView* const*>(shader_view + VIEW_OBJECT_OFFSET) : nullptr),
    };
  }
  if (is_exposure_source) {
    resource->SetPrivateData(SCENE_ROLE_GUID, sizeof(EXPOSURE_SOURCE), &EXPOSURE_SOURCE);
  }
  if (!is_float) return;
  D3D11_TEXTURE2D_DESC description;
  created->GetDesc(&description);
  if (description.Format != DXGI_FORMAT_R16G16B16A16_FLOAT) {
    if (!logged_format_mismatch.exchange(true, std::memory_order_acq_rel)) {
      reshade::log::message(reshade::log::level::error, "MgsvTargets: a float render target was created with another format.");
    }
    return;
  }
  if (float_target->scene_role != 0u) {
    resource->SetPrivateData(SCENE_ROLE_GUID, sizeof(float_target->scene_role), &float_target->scene_role);
  }
  if (float_target->is_bloom_image) {
    resource->SetPrivateData(BLOOM_IMAGE_GUID, sizeof(BLOOM_IMAGE_NEW), &BLOOM_IMAGE_NEW);
  }
}

inline uint32_t __fastcall HookReadTextureFormat(const Parameter* parameter) {
  const CallScope scope;
  // Code 6 alone gives a typeless texture. Reconstruction, the D3D12 bridge
  // and post-processing identify scene images by the typed float format.
  if (parameter->format_code == FLOAT_CODE && FindFloatTarget(parameter->name) != nullptr) return DXGI_FORMAT_R16G16B16A16_FLOAT;
  return hooks.read_texture_format(parameter);
}

// The parameter the manager stores for a live render target handle.
inline const Parameter* ReadStoredParameter(uint32_t handle) {
  const auto* entries = *reinterpret_cast<const uint8_t* const*>(*hooks.manager + 0x08u);
  return reinterpret_cast<const Parameter*>(entries + (static_cast<size_t>(handle >> 17u) * 0x70u) + 0x28u);
}

// The engine also calls this lookup while recording draws, on several threads,
// so every other caller passes straight through. The constructor's hook holds
// the call scope for the remaining path.
inline uint32_t __fastcall HookLookup(const uint64_t* name) {
  if (constructing_plugin == 0u) return hooks.lookup(name);
  const uint32_t shared = hooks.lookup(name);
  if ((shared & HANDLE_TYPE_MASK) != RENDER_TARGET_HANDLE) return shared;
  const auto target = std::ranges::find_if(private_targets, [shared_name = *name & NAME_MASK](const PrivateTarget& candidate) {
    return candidate.shared_name == shared_name && (candidate.plugins & constructing_plugin) != 0u;
  });
  if (target == private_targets.end()) return shared;
  if (target->handle == 0u) {
    Parameter parameter = *ReadStoredParameter(shared);
    parameter.name = target->name;
    parameter.format_code = FLOAT_CODE;
    target->shared_handle = shared;
    target->handle = hooks.create(&parameter);
    if (target->handle == 0u) {
      reshade::log::message(reshade::log::level::error, "MgsvTargets: could not create a private scene image; its plugin keeps the 8-bit temporary.");
      return shared;
    }
  }
  return target->handle;
}

// Gives one plugin's constructor its private images for the lookups it makes.
template <ConstructResources HookState::* original, uint8_t plugin>
void* __fastcall HookConstructResources(void* resources) {
  const CallScope scope;
  constructing_plugin = plugin;
  auto* constructed = (hooks.*original)(resources);
  constructing_plugin = 0u;
  return constructed;
}

inline void __fastcall HookRestoreTemporaries(void* dg) {
  const CallScope scope;
  hooks.restore_temporaries(dg);
  for (const auto& target : private_targets) {
    if (target.handle == 0u) continue;
    const auto* shared = ReadStoredParameter(target.shared_handle);
    hooks.resize(target.handle, static_cast<int32_t>(shared->width), static_cast<int32_t>(shared->height));
  }
}

// The replaced functions, for installing and removing them together.
inline auto HookedFunctions() {
  return std::array{
      renodx::utils::detour::Function(&hooks.initialize, HookInitialize),
      renodx::utils::detour::Function(&hooks.read_texture_format, HookReadTextureFormat),
      renodx::utils::detour::Function(&hooks.lookup, HookLookup),
      renodx::utils::detour::Function(&hooks.construct_motion_blur, HookConstructResources<&HookState::construct_motion_blur, MOTION_BLUR>),
      renodx::utils::detour::Function(&hooks.construct_depth_of_field, HookConstructResources<&HookState::construct_depth_of_field, DEPTH_OF_FIELD>),
      renodx::utils::detour::Function(&hooks.restore_temporaries, HookRestoreTemporaries),
  };
}

}  // namespace internal

// The scene role of an engine texture that has one, otherwise 0.
inline uint8_t ReadSceneRole(ID3D11Resource* resource) {
  uint8_t role = 0u;
  UINT size = sizeof(role);
  if (resource == nullptr || FAILED(resource->GetPrivateData(internal::SCENE_ROLE_GUID, &size, &role)) || size != sizeof(role)) return 0u;
  return role;
}

// The engine's current frame images, indexed by FrameTarget. An image is empty
// until the engine has created it under these hooks.
inline std::array<FrameImage, internal::FRAME_TARGETS.size()> ReadFrameImages() {
  const std::shared_lock lock(internal::frame_images.mutex);
  return internal::frame_images.images;
}

// Call before the engine creates its render targets, and balance each
// successful call with Detach. Targets that already exist stay 8-bit until the
// engine recreates them.
inline bool Install() {
  const std::unique_lock lock(internal::hooks.mutex);
  if (internal::hooks.registrations != 0u) {
    ++internal::hooks.registrations;
    return true;
  }
  const auto fail = [](const char* message) {
    if (!internal::hooks.logged_failure) {
      reshade::log::message(reshade::log::level::error, message);
      internal::hooks.logged_failure = true;
    }
    return false;
  };
  if (!engine::Verified(REQUIRED_ENTRIES)) {
    return fail("MgsvTargets: the render target creation entries did not verify; render targets stay 8-bit and HDR output will be wrong.");
  }
  internal::hooks.initialize = reinterpret_cast<internal::Initialize>(engine::Address(engine::Entry::RENDER_TARGET_INITIALIZE));
  internal::hooks.read_texture_format = reinterpret_cast<internal::ReadTextureFormat>(engine::Address(engine::Entry::GET_BUFFER_FORMAT_FROM_PARAMETER));
  internal::hooks.lookup = reinterpret_cast<internal::Lookup>(engine::Address(engine::Entry::RESIDENT_RESOURCE_GET_HANDLE));
  internal::hooks.construct_motion_blur = reinterpret_cast<internal::ConstructResources>(engine::Address(engine::Entry::MOTION_BLUR_WORK_BUFFER_CONSTRUCTOR));
  internal::hooks.construct_depth_of_field = reinterpret_cast<internal::ConstructResources>(engine::Address(engine::Entry::DEPTH_OF_FIELD_RESOURCES_CONSTRUCTOR));
  internal::hooks.restore_temporaries = reinterpret_cast<internal::RestoreTemporaries>(engine::Address(engine::Entry::RESTORE_TEMPORARY_RENDER_TARGET));
  internal::hooks.create = reinterpret_cast<internal::Create>(engine::Address(engine::Entry::RENDER_TARGET_CREATE_RESOURCE));
  internal::hooks.resize = reinterpret_cast<internal::Resize>(engine::Address(engine::Entry::RENDER_TARGET_RESTORE_WITH_RESIZE));
  internal::hooks.manager = reinterpret_cast<const uint8_t* const*>(engine::Address(engine::Entry::RENDER_TARGET_MANAGER));
  try {
    renodx::utils::detour::Install(internal::HookedFunctions());
  } catch (const std::system_error&) {
    return fail("MgsvTargets: render target hook installation failed; render targets stay 8-bit and HDR output will be wrong.");
  }
  internal::hooks.registrations = 1u;
  internal::hooks.logged_failure = false;
  const auto* manager = *internal::hooks.manager;
  if (manager != nullptr && *reinterpret_cast<const uint32_t*>(manager + 0x14u) != 0u) {
    reshade::log::message(reshade::log::level::warning, "MgsvTargets: installed after the engine created its render targets; restart the game to create them as float.");
  } else {
    reshade::log::message(reshade::log::level::info, "MgsvTargets: installed render target creation hooks.");
  }
  return true;
}

inline bool Detach() {
  const std::unique_lock lock(internal::hooks.mutex);
  if (internal::hooks.registrations == 0u) return true;
  if (internal::hooks.registrations > 1u) {
    --internal::hooks.registrations;
    return true;
  }
  try {
    renodx::utils::detour::Uninstall(internal::HookedFunctions());
  } catch (const std::system_error&) {
    reshade::log::message(reshade::log::level::warning, "MgsvTargets: render target hook removal failed.");
    return false;
  }
  while (internal::calls_in_flight.load(std::memory_order_acquire) != 0u) {
    SwitchToThread();
  }
  // The private images end with the engine's targets when the device does.
  for (auto& target : internal::private_targets) {
    target.shared_handle = 0u;
    target.handle = 0u;
  }
  {
    const std::unique_lock images_lock(internal::frame_images.mutex);
    internal::frame_images.images = {};
  }
  internal::hooks.registrations = 0u;
  return true;
}

// The game's device is created before the engine's first render target, and
// this event runs inside that creation call.
inline void OnInitDevice(reshade::api::device* device) {
  if (device->get_api() != reshade::api::device_api::d3d11) return;
  device->create_private_data<internal::DeviceState>()->hooks_installed = Install();
}

inline void OnDestroyDevice(reshade::api::device* device) {
  if (device->get_api() != reshade::api::device_api::d3d11) return;
  if (const auto* state = device->get_private_data<internal::DeviceState>(); state != nullptr && state->hooks_installed) {
    Detach();
  }
  device->destroy_private_data<internal::DeviceState>();
}

}  // namespace mgsv::engine::render_targets

#endif  // RENODX_GAMES_MGSV_ENGINE_RENDER_TARGETS_HPP_
