#pragma once

/* NVIDIA DLSS discovery, capability probing, model selection, and D3D12 dispatch. */

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <d3d11.h>
#include <d3d12.h>
#include <dxgi.h>
#include <nvsdk_ngx.h>
#include <nvsdk_ngx_helpers.h>
#include <winver.h>
#include <wrl/client.h>

#include <include/reshade.hpp>

#include "../camera_state.hpp"
#include "../d3d12.hpp"
#include "../frame_inputs.hpp"
#include "../logging.hpp"
#include "../resolution.hpp"
#include "../state.hpp"
#include "model.hpp"

#if defined(_WIN64)
#if defined(_DEBUG)
#pragma comment(lib, "../external/DLSS/lib/Windows_x86_64/x64/nvsdk_ngx_s_dbg.lib")
#else
#pragma comment(lib, "../external/DLSS/lib/Windows_x86_64/x64/nvsdk_ngx_s.lib")
#endif
#pragma comment(lib, "version.lib")
#endif

namespace mgsv::reconstruction::dlss {

inline constexpr char PROJECT_ID[] = "98dbfc11-0026-4cd1-b573-168ceb4c7bb5";
inline constexpr char ENGINE_VERSION[] = "MGSV-RenoDX-1";
inline constexpr uint32_t NVIDIA_VENDOR_ID = 0x10DEu;

enum class Availability : uint8_t {
  UNCHECKED,
  ELIGIBLE,
  AVAILABLE,
  ADAPTER_QUERY_FAILED,
  DLL_MISSING,
  GPU_UNSUPPORTED,
  DRIVER_OUTDATED,
  INITIALIZATION_FAILED,
};

struct Resources {
  // Non-owning identity, cleared at device destruction; guarded by the coordinator.
  reshade::api::device* adapter_device = nullptr;
  Microsoft::WRL::ComPtr<ID3D12Device> device;
  Microsoft::WRL::ComPtr<ID3D11Device> probed_device;
  NVSDK_NGX_Parameter* parameters = nullptr;
  NVSDK_NGX_Handle* feature = nullptr;
  Microsoft::WRL::ComPtr<ID3D12Resource> linear_output;
  uint32_t render_width = 0u;
  uint32_t render_height = 0u;
  uint32_t output_width = 0u;
  uint32_t output_height = 0u;
  Model feature_model = Model::DEFAULT;
  resolution::Quality feature_quality = resolution::Quality::NATIVE_AA;
  bool ngx_initialized = false;
  bool probe_complete = false;
  bool history_initialized = false;
  std::chrono::steady_clock::time_point previous_dispatch_time;
};

inline Resources resources = {};
inline std::wstring dll_path;
inline std::wstring dll_directory;
inline std::wstring application_data_path;
inline std::atomic<bool> dll_present = false;
inline std::atomic<uint32_t> snippet_version = 0u;
inline std::atomic<Availability> availability = Availability::UNCHECKED;
inline std::atomic<Model> selected_model = DEFAULT_MODEL;
inline std::array<std::atomic<uint32_t>, resolution::QUALITY_LABELS.size()> rejected_models;
inline std::atomic<bool> fatal_failure_pending = false;
inline std::atomic<bool> activation_requested = false;
inline std::atomic<bool> runtime_ready = false;
inline uint64_t last_failure_log = std::numeric_limits<uint64_t>::max();

inline bool LogEvery(uint64_t interval = 120u) {
  return logging::ShouldLogFrame(state::CurrentFrameToken(), last_failure_log, interval);
}

inline void CheckAdapter(reshade::api::device* device) {
  if (device == nullptr || resources.adapter_device != nullptr) return;
  resources.adapter_device = device;
  uint32_t vendor_id = 0u;
  if (!device->get_property(reshade::api::device_properties::vendor_id, &vendor_id)) {
    availability.store(Availability::ADAPTER_QUERY_FAILED, std::memory_order_release);
    logging::Warn("DLSS adapter vendor query failed; no DLL inspection or NGX initialization");
    return;
  }
  availability.store(
      device->get_api() == reshade::api::device_api::d3d11 && vendor_id == NVIDIA_VENDOR_ID
          ? Availability::ELIGIBLE
          : Availability::GPU_UNSUPPORTED,
      std::memory_order_release);
  logging::Info("DLSS adapter gate vendor=", logging::Hex{vendor_id},
                "; DLL inspection, NGX and hooks deferred until DLSS is requested");
}

inline void Discover() {
  std::array<wchar_t, 32768> executable_path = {};
  const DWORD length = GetModuleFileNameW(nullptr, executable_path.data(), static_cast<DWORD>(executable_path.size()));
  if (length == 0u || length == executable_path.size()) {
    dll_path.clear();
    dll_directory.clear();
    dll_present.store(false, std::memory_order_release);
    return;
  }

  const std::filesystem::path directory = std::filesystem::path(executable_path.data()).parent_path();
  const std::filesystem::path feature_path = directory / L"nvngx_dlss.dll";
  std::error_code error;
  const bool found = std::filesystem::is_regular_file(feature_path, error);
  dll_path = feature_path.wstring();
  dll_directory = directory.wstring();
  dll_present.store(found && !error, std::memory_order_release);
}

inline uint32_t ReadDllVersion() {
  if (!dll_present.load(std::memory_order_acquire)) return 0u;
  DWORD unused = 0u;
  const DWORD size = GetFileVersionInfoSizeW(dll_path.c_str(), &unused);
  if (size == 0u) return 0u;
  std::vector<uint8_t> version_data(size);
  if (GetFileVersionInfoW(dll_path.c_str(), 0u, size, version_data.data()) == FALSE) return 0u;
  VS_FIXEDFILEINFO* information = nullptr;
  UINT information_size = 0u;
  if (VerQueryValueW(
          version_data.data(),
          L"\\",
          reinterpret_cast<void**>(&information),
          &information_size)
          == FALSE
      || information == nullptr
      || information_size < sizeof(*information)) {
    return 0u;
  }
  const uint32_t major = HIWORD(information->dwFileVersionMS);
  const uint32_t minor = LOWORD(information->dwFileVersionMS) & 0xFFu;
  const uint32_t patch = HIWORD(information->dwFileVersionLS) & 0xFFu;
  return (major << 16u) | (minor << 8u) | patch;
}

inline uint32_t VersionMajor() {
  return snippet_version.load(std::memory_order_acquire) >> 16u;
}

inline uint32_t VersionMinor() {
  return (snippet_version.load(std::memory_order_acquire) >> 8u) & 0xFFu;
}

inline uint32_t VersionPatch() {
  return snippet_version.load(std::memory_order_acquire) & 0xFFu;
}

inline bool IsModelAvailable(Model model) {
  // NGX exposes preset hints but no API that enumerates accepted presets.
  // Filter their stable numeric identifiers by the feature DLL generation.
  if (model == Model::DEFAULT) return true;
  if ((rejected_models[static_cast<size_t>(resolution::Read().quality)].load(std::memory_order_acquire)
       & (1u << static_cast<uint32_t>(model)))
      != 0u) return false;

  const uint32_t major = VersionMajor();
  // Preserve the saved hint without inspecting the DLL before a DLSS request.
  if (major == 0u) return true;
  if (major >= 310u) {
    if (model == Model::L || model == Model::M) return major > 310u || VersionMinor() >= 7u;
    return true;
  }
  if (major == 3u && VersionMinor() >= 10u) {
    if (model == Model::L || model == Model::M) return VersionPatch() >= 7u;
    return true;
  }
  if (major >= 2u) return model >= Model::A && model <= Model::F;
  return false;
}

inline Model NormalizeModel(float value) {
  const auto requested = std::find_if(
      MODEL_OPTIONS.begin(),
      MODEL_OPTIONS.end(),
      [value](const ModelOption& option) { return value == static_cast<float>(option.model); });
  if (requested != MODEL_OPTIONS.end() && IsModelAvailable(requested->model)) return requested->model;
  if (IsModelAvailable(DEFAULT_MODEL)) return DEFAULT_MODEL;
  const auto fallback = std::find_if(
      MODEL_OPTIONS.begin(),
      MODEL_OPTIONS.end(),
      [](const ModelOption& option) { return IsModelAvailable(option.model); });
  return fallback == MODEL_OPTIONS.end() ? Model::DEFAULT : fallback->model;
}

inline void SetModel(Model model) {
  selected_model.store(model, std::memory_order_release);
}

inline Model GetModel() {
  return selected_model.load(std::memory_order_acquire);
}

inline const char* UnavailableReason() {
  switch (availability.load(std::memory_order_acquire)) {
    case Availability::ADAPTER_QUERY_FAILED:
      return "The rendering adapter vendor could not be identified. DLSS has not been initialized.";
    case Availability::DLL_MISSING:
      return "nvngx_dlss.dll was not found next to mgsvtpp.exe. Add a compatible DLL and restart the game.";
    case Availability::GPU_UNSUPPORTED:
      return "DLSS is not supported on this rendering device. An NVIDIA GPU with DLSS support is required.";
    case Availability::DRIVER_OUTDATED:
      return "The NVIDIA driver does not meet the DLSS requirement.";
    case Availability::INITIALIZATION_FAILED:
      return "NVIDIA DLSS initialization or runtime failed. Check ReShade.log, the driver and DLSS DLL; restart to retry.";
    case Availability::ELIGIBLE:
    case Availability::AVAILABLE:
      return nullptr;
    case Availability::UNCHECKED:
    default:
      return "The game rendering adapter has not been identified yet.";
  }
}

inline bool ActivationPending() {
  const auto status = availability.load(std::memory_order_acquire);
  return status == Availability::UNCHECKED || status == Availability::ELIGIBLE
         || (status == Availability::AVAILABLE && !runtime_ready.load(std::memory_order_acquire));
}

inline bool ConsumeFatalFailure() {
  return fatal_failure_pending.exchange(false, std::memory_order_acq_rel);
}

inline void QueueFatalFailure() {
  runtime_ready.store(false, std::memory_order_release);
  availability.store(Availability::INITIALIZATION_FAILED, std::memory_order_release);
  fatal_failure_pending.store(true, std::memory_order_release);
}

inline bool ReleaseFeatureResources() {
  if (resources.feature == nullptr && resources.parameters == nullptr && resources.linear_output == nullptr) return true;
  dx12::Drain();
  if (resources.feature != nullptr) {
    const NVSDK_NGX_Result result = NVSDK_NGX_D3D12_ReleaseFeature(resources.feature);
    if (NVSDK_NGX_FAILED(result)) {
      logging::Warn("NVIDIA DLSS feature release failed; retaining resources result=", static_cast<uint32_t>(result));
      availability.store(Availability::INITIALIZATION_FAILED, std::memory_order_release);
      QueueFatalFailure();
      return false;
    }
    resources.feature = nullptr;
  }
  if (resources.parameters != nullptr) {
    NVSDK_NGX_D3D12_DestroyParameters(resources.parameters);
    resources.parameters = nullptr;
  }
  resources.linear_output.Reset();
  resources.render_width = 0u;
  resources.render_height = 0u;
  resources.output_width = 0u;
  resources.output_height = 0u;
  resources.feature_model = Model::DEFAULT;
  resources.history_initialized = false;
  resources.previous_dispatch_time = {};
  return true;
}

inline bool Shutdown(bool reset_probe = true) {
  runtime_ready.store(false, std::memory_order_release);
  if (!ReleaseFeatureResources()) return false;
  if (resources.ngx_initialized && resources.device != nullptr) {
    const NVSDK_NGX_Result result = NVSDK_NGX_D3D12_Shutdown1(resources.device.Get());
    if (NVSDK_NGX_FAILED(result)) {
      logging::Warn("NVIDIA DLSS shutdown failed; retaining SDK instance result=", static_cast<uint32_t>(result));
      QueueFatalFailure();
      return false;
    }
  }
  resources.ngx_initialized = false;
  resources.device.Reset();
  if (reset_probe) {
    resources.adapter_device = nullptr;
    resources.probed_device.Reset();
    resources.probe_complete = false;
    activation_requested.store(false, std::memory_order_release);
    fatal_failure_pending.store(false, std::memory_order_release);
    snippet_version.store(0u, std::memory_order_release);
    for (auto& rejected : rejected_models) rejected.store(0u, std::memory_order_release);
    availability.store(Availability::UNCHECKED, std::memory_order_release);
  }
  return true;
}

inline std::wstring GetApplicationDataPath() {
  if (!application_data_path.empty()) return application_data_path;
  std::array<wchar_t, MAX_PATH> temporary_path = {};
  const DWORD length = GetTempPathW(static_cast<DWORD>(temporary_path.size()), temporary_path.data());
  if (length == 0u || length >= temporary_path.size()) return {};
  const std::filesystem::path path = std::filesystem::path(temporary_path.data()) / L"RenoDX" / L"MGSV" / L"NGX";
  std::error_code error;
  std::filesystem::create_directories(path, error);
  if (error) return {};
  application_data_path = path.wstring();
  return application_data_path;
}

inline NVSDK_NGX_FeatureCommonInfo MakeFeatureInfo(std::array<const wchar_t*, 1>& paths) {
  paths = {dll_directory.c_str()};
  NVSDK_NGX_FeatureCommonInfo result = {};
  result.PathListInfo.Path = paths.data();
  result.PathListInfo.Length = static_cast<uint32_t>(paths.size());
  return result;
}

inline bool ProbeDevice(reshade::api::device* device) {
  // Serviced only from Present after selection, never from a draw or vendor check.
  if (!activation_requested.load(std::memory_order_acquire)
      || device == nullptr || device != resources.adapter_device) return false;

  auto* native_device = reinterpret_cast<ID3D11Device*>(device->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (native_device == nullptr) {
    availability.store(Availability::INITIALIZATION_FAILED, std::memory_order_release);
    return false;
  }
  if (resources.probe_complete) {
    if (resources.probed_device.Get() != native_device) return false;
    return availability.load(std::memory_order_acquire) == Availability::AVAILABLE;
  }
  if (availability.load(std::memory_order_acquire) != Availability::ELIGIBLE) return false;
  resources.probed_device = native_device;
  resources.probe_complete = true;

  Discover();
  if (!dll_present.load(std::memory_order_acquire)) {
    availability.store(Availability::DLL_MISSING, std::memory_order_release);
    return false;
  }

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  DXGI_ADAPTER_DESC adapter_description = {};
  const bool has_adapter = SUCCEEDED(native_device->QueryInterface(IID_PPV_ARGS(&dxgi_device)))
                           && SUCCEEDED(dxgi_device->GetAdapter(&adapter))
                           && SUCCEEDED(adapter->GetDesc(&adapter_description));
  if (!has_adapter || adapter_description.VendorId != NVIDIA_VENDOR_ID) {
    availability.store(Availability::GPU_UNSUPPORTED, std::memory_order_release);
    return false;
  }

  snippet_version.store(ReadDllVersion(), std::memory_order_release);
  std::array<const wchar_t*, 1> feature_paths = {};
  const auto feature_info = MakeFeatureInfo(feature_paths);
  const std::wstring data_path = GetApplicationDataPath();
  if (data_path.empty()) {
    availability.store(Availability::INITIALIZATION_FAILED, std::memory_order_release);
    return false;
  }

  NVSDK_NGX_Application_Identifier identifier = {};
  identifier.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Project_Id;
  identifier.v.ProjectDesc = {
      .ProjectId = PROJECT_ID,
      .EngineType = NVSDK_NGX_ENGINE_TYPE_CUSTOM,
      .EngineVersion = ENGINE_VERSION,
  };
  const NVSDK_NGX_FeatureDiscoveryInfo discovery = {
      .SDKVersion = NVSDK_NGX_Version_API,
      .FeatureID = NVSDK_NGX_Feature_SuperSampling,
      .Identifier = identifier,
      .ApplicationDataPath = data_path.c_str(),
      .FeatureInfo = &feature_info,
  };
  NVSDK_NGX_FeatureRequirement requirement = {};
  const NVSDK_NGX_Result requirement_result = NVSDK_NGX_D3D12_GetFeatureRequirements(
      adapter.Get(),
      &discovery,
      &requirement);
  if (NVSDK_NGX_SUCCEED(requirement_result)) {
    const uint32_t unsupported = static_cast<uint32_t>(requirement.FeatureSupported);
    if ((unsupported & NVSDK_NGX_FeatureSupportResult_DriverVersionUnsupported) != 0u) {
      availability.store(Availability::DRIVER_OUTDATED, std::memory_order_release);
      return false;
    }
    if ((unsupported & (NVSDK_NGX_FeatureSupportResult_AdapterUnsupported | NVSDK_NGX_FeatureSupportResult_OSVersionBelowMinimumSupported | NVSDK_NGX_FeatureSupportResult_NotImplemented))
        != 0u) {
      availability.store(Availability::GPU_UNSUPPORTED, std::memory_order_release);
      return false;
    }
  }
  try {
    resources.device = dx12::GetBridge(native_device).device12;
  } catch (const std::exception& error) {
    logging::Warn("NVIDIA DLSS backend initialization failed: ", error.what());
    availability.store(Availability::INITIALIZATION_FAILED, std::memory_order_release);
    return false;
  }
  const NVSDK_NGX_Result init_result = NVSDK_NGX_D3D12_Init_with_ProjectID(
      PROJECT_ID,
      NVSDK_NGX_ENGINE_TYPE_CUSTOM,
      ENGINE_VERSION,
      data_path.c_str(),
      resources.device.Get(),
      &feature_info);
  if (NVSDK_NGX_FAILED(init_result)) {
    Availability failure = Availability::INITIALIZATION_FAILED;
    if (init_result == NVSDK_NGX_Result_FAIL_OutOfDate) {
      failure = Availability::DRIVER_OUTDATED;
    } else if (init_result == NVSDK_NGX_Result_FAIL_FeatureNotSupported) {
      failure = Availability::GPU_UNSUPPORTED;
    }
    availability.store(failure, std::memory_order_release);
    if (LogEvery(1u)) logging::Warn("NVIDIA NGX D3D12 initialization failed result=", static_cast<uint32_t>(init_result));
    return false;
  }

  resources.ngx_initialized = true;
  NVSDK_NGX_Parameter* capability_parameters = nullptr;
  const NVSDK_NGX_Result capability_result = NVSDK_NGX_D3D12_GetCapabilityParameters(&capability_parameters);
  int available = 0;
  int needs_updated_driver = 0;
  NVSDK_NGX_Result available_result = NVSDK_NGX_Result_FAIL_InvalidParameter;
  NVSDK_NGX_Result driver_result = NVSDK_NGX_Result_FAIL_InvalidParameter;
  if (capability_parameters != nullptr) {
    available_result = capability_parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &available);
    driver_result = capability_parameters->Get(
        NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver,
        &needs_updated_driver);
    NVSDK_NGX_D3D12_DestroyParameters(capability_parameters);
  }
  if (NVSDK_NGX_FAILED(capability_result)
      || NVSDK_NGX_FAILED(available_result)
      || NVSDK_NGX_FAILED(driver_result)) {
    Shutdown(false);
    availability.store(Availability::INITIALIZATION_FAILED, std::memory_order_release);
    return false;
  }
  if (needs_updated_driver != 0) {
    Shutdown(false);
    availability.store(Availability::DRIVER_OUTDATED, std::memory_order_release);
    return false;
  }
  if (available == 0) {
    Shutdown(false);
    availability.store(Availability::GPU_UNSUPPORTED, std::memory_order_release);
    return false;
  }

  availability.store(Availability::AVAILABLE, std::memory_order_release);
  logging::Info("NVIDIA DLSS available dll_version=", VersionMajor(), ".", VersionMinor(), ".", VersionPatch());
  return true;
}

inline void Destroy(reshade::api::device* device) {
  if (resources.probed_device == nullptr && resources.device == nullptr
      && resources.feature == nullptr && resources.parameters == nullptr) return;
  if (device != nullptr
      && resources.device != nullptr
      && reinterpret_cast<ID3D11Device*>(device->get_native()) != resources.probed_device.Get()) {  // NOLINT(performance-no-int-to-ptr)
    return;
  }
  if (!Shutdown()) {
    throw std::runtime_error("NVIDIA DLSS could not release its resources; method switch postponed");
  }
  logging::Info("released NVIDIA DLSS feature and output; NGX shut down");
}

inline void InvalidateHistory() {
  resources.history_initialized = false;
}

inline NVSDK_NGX_PerfQuality_Value GetPerformanceQuality(resolution::Quality selected) {
  switch (selected) {
    case resolution::Quality::QUALITY:           return NVSDK_NGX_PerfQuality_Value_MaxQuality;
    case resolution::Quality::BALANCED:          return NVSDK_NGX_PerfQuality_Value_Balanced;
    case resolution::Quality::PERFORMANCE:       return NVSDK_NGX_PerfQuality_Value_MaxPerf;
    case resolution::Quality::ULTRA_PERFORMANCE: return NVSDK_NGX_PerfQuality_Value_UltraPerformance;
    default:                                     return NVSDK_NGX_PerfQuality_Value_DLAA;
  }
}

inline void SetModelHint(NVSDK_NGX_Parameter* parameters, Model model) {
  const auto selected = resolution::Read().quality;
  const char* key = NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA;
  switch (selected) {
    case resolution::Quality::QUALITY:           key = NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Quality; break;
    case resolution::Quality::BALANCED:          key = NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Balanced; break;
    case resolution::Quality::PERFORMANCE:       key = NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Performance; break;
    case resolution::Quality::ULTRA_PERFORMANCE: key = NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_UltraPerformance; break;
    default:                                     break;
  }
  parameters->Set(key, static_cast<uint32_t>(model));
}

inline bool CreateResources(
    reshade::api::device* device,
    ID3D12GraphicsCommandList* commands,
    const ValidatedFrameInputs& inputs) {
  if (inputs.render_width == 0u || inputs.render_height == 0u
      || inputs.output.width < inputs.render_width || inputs.output.height < inputs.render_height) return false;
  const uint32_t width = inputs.render_width;
  const uint32_t height = inputs.render_height;
  if (!runtime_ready.load(std::memory_order_acquire) || !resources.ngx_initialized
      || device != resources.adapter_device
      || commands == nullptr) return false;
  if (!ReleaseFeatureResources()) return false;

  const Model model = NormalizeModel(static_cast<float>(GetModel()));
  SetModel(model);
  resources.linear_output = dx12::compute::CreateTexture(resources.device.Get(), inputs.output.width, inputs.output.height, DXGI_FORMAT_R16G16B16A16_FLOAT);

  const NVSDK_NGX_Result parameter_result = NVSDK_NGX_D3D12_AllocateParameters(&resources.parameters);
  if (NVSDK_NGX_FAILED(parameter_result) || resources.parameters == nullptr) {
    ReleaseFeatureResources();
    return false;
  }
  resources.parameters->Set(NVSDK_NGX_Parameter_Width, width);
  resources.parameters->Set(NVSDK_NGX_Parameter_Height, height);
  resources.parameters->Set(NVSDK_NGX_Parameter_OutWidth, inputs.output.width);
  resources.parameters->Set(NVSDK_NGX_Parameter_OutHeight, inputs.output.height);
  resources.parameters->Set(NVSDK_NGX_Parameter_PerfQualityValue, static_cast<int>(GetPerformanceQuality(resolution::Read().quality)));
  resources.parameters->Set(
      NVSDK_NGX_Parameter_DLSS_Feature_Create_Flags,
      static_cast<int>(NVSDK_NGX_DLSS_Feature_Flags_IsHDR
                       | NVSDK_NGX_DLSS_Feature_Flags_MVLowRes
                       | NVSDK_NGX_DLSS_Feature_Flags_DepthInverted
                       | NVSDK_NGX_DLSS_Feature_Flags_AutoExposure));
  resources.parameters->Set(NVSDK_NGX_Parameter_DLSS_Enable_Output_Subrects, 0);
  // Free the feature's cached allocations on a method switch while keeping
  // NGX initialized. Switching back needs a new feature, not another SDK probe.
  resources.parameters->Set(NVSDK_NGX_Parameter_FreeMemOnReleaseFeature, 1);
  resources.parameters->Set(NVSDK_NGX_Parameter_RTXValue, 0);
  SetModelHint(resources.parameters, model);

  Model effective_model = model;
  // Creation and evaluation record into the same list. The bridge submits it
  // only after every step, including output encoding, has been accepted.
  NVSDK_NGX_Result create_result = NVSDK_NGX_D3D12_CreateFeature(
      commands,
      NVSDK_NGX_Feature_SuperSampling,
      resources.parameters,
      &resources.feature);
  if (NVSDK_NGX_FAILED(create_result) && model != Model::DEFAULT) {
    rejected_models[static_cast<size_t>(resolution::Read().quality)].fetch_or(1u << static_cast<uint32_t>(model), std::memory_order_acq_rel);
    SetModelHint(resources.parameters, Model::DEFAULT);
    create_result = NVSDK_NGX_D3D12_CreateFeature(
        commands,
        NVSDK_NGX_Feature_SuperSampling,
        resources.parameters,
        &resources.feature);
    if (NVSDK_NGX_SUCCEED(create_result)) {
      effective_model = Model::DEFAULT;
      SetModel(Model::DEFAULT);
      logging::Warn("requested NVIDIA DLSS model unavailable; using DLL default");
    }
  }
  if (NVSDK_NGX_FAILED(create_result) || resources.feature == nullptr) {
    if (LogEvery(1u)) logging::Warn("NVIDIA DLSS feature creation failed result=", static_cast<uint32_t>(create_result));
    ReleaseFeatureResources();
    availability.store(Availability::INITIALIZATION_FAILED, std::memory_order_release);
    return false;
  }

  resources.render_width = width;
  resources.render_height = height;
  resources.output_width = inputs.output.width;
  resources.output_height = inputs.output.height;
  resources.feature_model = effective_model;
  resources.feature_quality = resolution::Read().quality;
  logging::Info("NVIDIA DLSS feature created render=", width, "x", height,
                " output=", inputs.output.width, "x", inputs.output.height,
                " model=", FindModelOption(effective_model)->label);
  return true;
}

inline bool EnsureResources(
    reshade::api::device* device,
    ID3D12GraphicsCommandList* commands,
    const ValidatedFrameInputs& inputs) {
  if (!runtime_ready.load(std::memory_order_acquire)) return false;
  const Model model = NormalizeModel(static_cast<float>(GetModel()));
  if (resources.feature != nullptr
      && availability.load(std::memory_order_acquire) == Availability::AVAILABLE
      && device != nullptr
      && reinterpret_cast<ID3D11Device*>(device->get_native()) == resources.probed_device.Get()  // NOLINT(performance-no-int-to-ptr)
      && resources.render_width == inputs.render_width
      && resources.render_height == inputs.render_height
      && resources.output_width == inputs.output.width
      && resources.output_height == inputs.output.height
      && resources.feature_model == model
      && resources.feature_quality == resolution::Read().quality) {
    return true;
  }
  return CreateResources(device, commands, inputs);
}

inline float FrameDeltaMilliseconds(bool reset) {
  const auto now = std::chrono::steady_clock::now();
  float milliseconds = 1000.f / 60.f;
  if (!reset && resources.previous_dispatch_time.time_since_epoch().count() != 0) {
    milliseconds = std::chrono::duration<float, std::milli>(now - resources.previous_dispatch_time).count();
  }
  resources.previous_dispatch_time = now;
  return std::clamp(milliseconds, 1.f, 1000.f);
}

// The coordinator owns one bridge submission. NGX records into that same list;
// the encoder immediately follows evaluation and writes the shared destination.
inline bool Dispatch(const ValidatedFrameInputs& inputs) {
  const bool succeeded = dx12::Execute(inputs, !resources.history_initialized, [&](const dx12::Evaluation& evaluation) {
    if (!EnsureResources(inputs.device, evaluation.commands, inputs)) throw std::runtime_error("Cannot create D3D12 DLSS feature");
    const bool reset = !resources.history_initialized;
    dx12::compute::Transition(evaluation.commands, resources.linear_output.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    resources.parameters->Set(NVSDK_NGX_Parameter_Color, evaluation.color);
    resources.parameters->Set(NVSDK_NGX_Parameter_Output, resources.linear_output.Get());
    resources.parameters->Set(NVSDK_NGX_Parameter_Depth, evaluation.depth);
    resources.parameters->Set(NVSDK_NGX_Parameter_MotionVectors, evaluation.motion);
    resources.parameters->Set(NVSDK_NGX_Parameter_Jitter_Offset_X, inputs.camera.jitter_uv_x * static_cast<float>(inputs.render_width));
    resources.parameters->Set(NVSDK_NGX_Parameter_Jitter_Offset_Y, inputs.camera.jitter_uv_y * static_cast<float>(inputs.render_height));
    resources.parameters->Set(NVSDK_NGX_Parameter_Reset, reset ? 1 : 0);
    resources.parameters->Set(NVSDK_NGX_Parameter_MV_Scale_X, static_cast<float>(inputs.render_width));
    resources.parameters->Set(NVSDK_NGX_Parameter_MV_Scale_Y, static_cast<float>(inputs.render_height));
    resources.parameters->Set(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Width, inputs.render_width);
    resources.parameters->Set(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Height, inputs.render_height);
    resources.parameters->Set(NVSDK_NGX_Parameter_DLSS_Pre_Exposure, 1.f);
    resources.parameters->Set(NVSDK_NGX_Parameter_DLSS_Exposure_Scale, 1.f);
    resources.parameters->Set(NVSDK_NGX_Parameter_Sharpness, 0.f);
    resources.parameters->Set(NVSDK_NGX_Parameter_FrameTimeDeltaInMsec, FrameDeltaMilliseconds(reset));
    const auto result = NVSDK_NGX_D3D12_EvaluateFeature_C(evaluation.commands, resources.feature, resources.parameters, nullptr);
    if (NVSDK_NGX_FAILED(result)) {
      QueueFatalFailure();
      throw std::runtime_error("D3D12 DLSS evaluation failed: " + std::to_string(static_cast<uint32_t>(result)));
    }
    dx12::compute::Transition(evaluation.commands, resources.linear_output.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    dx12::Encode(evaluation, resources.linear_output.Get());
    dx12::compute::Transition(evaluation.commands, resources.linear_output.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
  });
  if (succeeded) {
    if (!resources.history_initialized) {
      logging::Info("D3D12 DLSS accumulation started render=", inputs.render_width, "x", inputs.render_height,
                    " output=", inputs.output.width, "x", inputs.output.height);
    }
    resources.history_initialized = true;
  }
  return succeeded;
}

}  // namespace mgsv::reconstruction::dlss
