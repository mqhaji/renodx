#ifndef RENODX_GAMES_MGSV_RECONSTRUCTION_FSR_RUNTIME_HPP_
#define RENODX_GAMES_MGSV_RECONSTRUCTION_FSR_RUNTIME_HPP_

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

#include <d3d12.h>
#include <Windows.h>
#include <wrl/client.h>

#include "../d3d12.hpp"
#include "sdk/api/include/dx12/ffx_api_dx12.h"
#include "sdk/upscalers/include/ffx_upscale.h"

namespace mgsv::reconstruction::fsr {

// Present and the menu read this independently of the serialized SDK state.
// A failed runtime stays unavailable until restart instead of retrying every frame.
static std::atomic<bool> runtime_failed = false;

// The menu checks files without loading the SDK. Only selecting FSR loads AMD's
// code; DLSS and TAA do not pay its initialization or memory cost.
inline const std::filesystem::path& GetDllDirectory() {
  static const std::filesystem::path DIRECTORY = [] {
    std::array<wchar_t, 32768> executable = {};
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (length == 0u || length == executable.size()) return std::filesystem::path{};
    return std::filesystem::path(executable.data()).parent_path();
  }();
  return DIRECTORY;
}

inline const char* UnavailableReason() {
  if (runtime_failed.load(std::memory_order_acquire)) return "FSR could not run. Check ReShade.log for the reason, then restart the game.";
  static const char* const REASON = []() -> const char* {
    const auto& directory = GetDllDirectory();
    if (directory.empty()) return "Could not locate the game folder for FSR.";
    std::error_code error;
    if (!std::filesystem::is_regular_file(directory / L"amd_fidelityfx_loader_dx12.dll", error)) {
      return "Add amd_fidelityfx_loader_dx12.dll and amd_fidelityfx_upscaler_dx12.dll beside mgsvtpp.exe, then restart the game.";
    }
    if (!std::filesystem::is_regular_file(directory / L"amd_fidelityfx_upscaler_dx12.dll", error)) {
      return "amd_fidelityfx_upscaler_dx12.dll is missing. Place it beside mgsvtpp.exe, then restart the game.";
    }
    return nullptr;
  }();
  return REASON;
}

// Device teardown unloads these DLLs explicitly. A static destructor would run
// under Windows' loader lock, where FreeLibrary is unsafe. If cleanup cannot
// finish, keep the references and let Windows reclaim them at process exit.
struct Library {
  HMODULE upscaler = nullptr;
  HMODULE module = nullptr;
  PfnFfxCreateContext create = nullptr;
  PfnFfxDestroyContext destroy = nullptr;
  PfnFfxDispatch dispatch = nullptr;
  PfnFfxQuery query = nullptr;

  Library() = default;
  Library(const Library&) = delete;
  Library& operator=(const Library&) = delete;
  Library(Library&&) = delete;
  Library& operator=(Library&&) = delete;

  void Reset() {
    // The loader borrows an already loaded effect DLL. Release the loader first
    // so its cached entry points cannot outlive our reference to the upscaler.
    if (module != nullptr) {
      FreeLibrary(module);
      module = nullptr;
    }
    create = nullptr;
    destroy = nullptr;
    dispatch = nullptr;
    query = nullptr;
    if (upscaler != nullptr) {
      FreeLibrary(upscaler);
      upscaler = nullptr;
    }
  }
};

// The coordinator serializes SDK access. Keep the loaded API until device
// teardown; switching methods releases the context and its GPU allocations.
static Library library;

struct Resources {
  ffxContext context = nullptr;
  ffxCreateBackendDX12Desc backend = {};
  ffxCreateContextDescUpscale description = {};
  Microsoft::WRL::ComPtr<ID3D12Resource> linear_output;
  bool initialized = false;
  bool recording_failed = false;
  std::chrono::steady_clock::time_point previous_dispatch_time;
};

static Resources resources;

inline void CheckFsr(ffxReturnCode_t result, const char* operation) {
  if (result != FFX_API_RETURN_OK) throw std::runtime_error(std::string(operation) + ": FSR API error " + std::to_string(result));
}

inline void ReleaseContext() {
  if (resources.context == nullptr && resources.linear_output == nullptr && !resources.recording_failed) return;
  dx12::Drain();
  if (resources.context != nullptr) {
    CheckFsr(library.destroy(&resources.context, nullptr), "Destroy FSR context");
  }
  resources = {};
}

inline void Destroy() {
  if (library.module == nullptr && library.upscaler == nullptr) return;
  ReleaseContext();
  library.Reset();
  logging::Info("released FSR context, output, and DLL references");
}

inline void InvalidateHistory() {
  resources.initialized = false;
}

inline bool Dispatch(const ValidatedFrameInputs& inputs) {
  if (const char* reason = UnavailableReason(); reason != nullptr) throw std::runtime_error(reason);
  if (library.create == nullptr) {
    if (library.upscaler == nullptr) {
      // Load the effect ourselves before the loader finds it. This gives us a
      // balanced LoadLibrary/FreeLibrary pair instead of a process-long load
      // owned by the loader, which has no public unload operation.
      library.upscaler = LoadLibraryExW((GetDllDirectory() / L"amd_fidelityfx_upscaler_dx12.dll").c_str(), nullptr,
                                        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
      if (library.upscaler == nullptr) {
        throw std::runtime_error("Load AMD FSR upscaler: Windows error " + std::to_string(GetLastError()));
      }
    }
    if (library.module == nullptr) {
      library.module = LoadLibraryExW((GetDllDirectory() / L"amd_fidelityfx_loader_dx12.dll").c_str(), nullptr,
                                      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
      if (library.module == nullptr) {
        throw std::runtime_error("Load AMD FSR loader: Windows error " + std::to_string(GetLastError()));
      }
    }
    // Resolve the complete interface before publishing a usable entry point.
    const auto create = reinterpret_cast<PfnFfxCreateContext>(GetProcAddress(library.module, "ffxCreateContext"));
    library.destroy = reinterpret_cast<PfnFfxDestroyContext>(GetProcAddress(library.module, "ffxDestroyContext"));
    library.dispatch = reinterpret_cast<PfnFfxDispatch>(GetProcAddress(library.module, "ffxDispatch"));
    library.query = reinterpret_cast<PfnFfxQuery>(GetProcAddress(library.module, "ffxQuery"));
    if (create == nullptr || library.destroy == nullptr || library.dispatch == nullptr || library.query == nullptr) {
      throw std::runtime_error("AMD FSR loader is missing required API exports. Install the matching SDK DLLs.");
    }
    library.create = create;
  }

  const bool succeeded = dx12::Execute(inputs, !resources.initialized, [&](const dx12::Evaluation& evaluation) {
    if (resources.context == nullptr || resources.recording_failed
        || resources.description.maxRenderSize.width != inputs.render_width || resources.description.maxRenderSize.height != inputs.render_height
        || resources.description.maxUpscaleSize.width != inputs.output.width || resources.description.maxUpscaleSize.height != inputs.output.height) {
      ReleaseContext();
      // AMD retains pointers from the creation descriptor. Keep this chain at
      // stable addresses until the context is destroyed, including on failure.
      resources.backend = {
          .header = {.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12},
          .device = evaluation.device,
      };
      resources.description = {
          .header = {.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE, .pNext = &resources.backend.header},
          .flags = FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE | FFX_UPSCALE_ENABLE_DEPTH_INVERTED | FFX_UPSCALE_ENABLE_AUTO_EXPOSURE,
          .maxRenderSize = {.width = inputs.render_width, .height = inputs.render_height},
          .maxUpscaleSize = {.width = inputs.output.width, .height = inputs.output.height},
      };
      resources.recording_failed = true;
      // No version or tuning overrides: AMD chooses the newest compatible
      // provider, including a newer driver implementation when available.
      CheckFsr(library.create(&resources.context, &resources.description.header, nullptr), "Create FSR context");
      resources.linear_output = dx12::compute::CreateTexture(evaluation.device, inputs.output.width, inputs.output.height, DXGI_FORMAT_R16G16B16A16_FLOAT);
      ffxQueryGetProviderVersion version = {.header = {.type = FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION}};
      const auto queried = library.query(&resources.context, &version.header);
      logging::Info("created D3D12 FSR context provider=",
                    (queried == FFX_API_RETURN_OK && version.versionName != nullptr ? version.versionName : "unknown"),
                    " render=", inputs.render_width, "x", inputs.render_height,
                    " output=", inputs.output.width, "x", inputs.output.height);
    }

    // Recording advances SDK history. Recreate the context if this command list
    // is discarded rather than using history that the GPU never produced.
    resources.recording_failed = true;
    const bool reset = !resources.initialized;
    const auto now = std::chrono::steady_clock::now();
    const float frame_ms = (reset ? 1000.f / 60.f : std::clamp(std::chrono::duration<float, std::milli>(now - resources.previous_dispatch_time).count(), 1.f, 1000.f));
    resources.previous_dispatch_time = now;

    // Recover physical camera distances from MGSV's reverse-Z projection.
    const float depth_x = inputs.camera.device_to_view_depth[0];
    const float depth_y = inputs.camera.device_to_view_depth[1];
    float camera_near = depth_y / (1.f - depth_x);
    float camera_far = -depth_y / depth_x;
    if (!std::isfinite(camera_near) || !std::isfinite(camera_far) || camera_near <= 0.f || camera_far <= camera_near) {
      camera_near = 0.1f;
      camera_far = 4000.f;
    }

    // Preparation already supplied linear HDR, unjittered UV motion, and depth
    // in shared textures. The DLL records on our existing list and queue.
    const ffxDispatchDescUpscale dispatch = {
        .header = {.type = FFX_API_DISPATCH_DESC_TYPE_UPSCALE},
        .commandList = evaluation.commands,
        .color = ffxApiGetResourceDX12(evaluation.color),
        .depth = ffxApiGetResourceDX12(evaluation.depth),
        .motionVectors = ffxApiGetResourceDX12(evaluation.motion),
        .output = ffxApiGetResourceDX12(resources.linear_output.Get(), FFX_API_RESOURCE_STATE_COMMON),
        .jitterOffset = {.x = inputs.camera.jitter_uv_x * static_cast<float>(inputs.render_width), .y = inputs.camera.jitter_uv_y * static_cast<float>(inputs.render_height)},
        .motionVectorScale = {.x = static_cast<float>(inputs.render_width), .y = static_cast<float>(inputs.render_height)},
        .renderSize = {.width = inputs.render_width, .height = inputs.render_height},
        .upscaleSize = {.width = inputs.output.width, .height = inputs.output.height},
        .frameTimeDelta = frame_ms,
        .preExposure = 1.f,
        .reset = reset,
        .cameraNear = camera_near,
        .cameraFar = camera_far,
        .cameraFovAngleVertical = 2.f * std::atan(std::abs(inputs.camera.device_to_view_depth[3])),
        .viewSpaceToMetersFactor = 1.f,
    };
    CheckFsr(library.dispatch(&resources.context, &dispatch.header), "Evaluate FSR");
    // The SDK restores external resources to their declared states. Encode
    // into the shared game destination in the same D3D12 submission.
    dx12::compute::Transition(evaluation.commands, resources.linear_output.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    dx12::Encode(evaluation, resources.linear_output.Get());
    dx12::compute::Transition(evaluation.commands, resources.linear_output.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
  });
  if (succeeded) {
    resources.recording_failed = false;
    resources.initialized = true;
  }
  return succeeded;
}

}  // namespace mgsv::reconstruction::fsr

#endif  // RENODX_GAMES_MGSV_RECONSTRUCTION_FSR_RUNTIME_HPP_
