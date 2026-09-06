#pragma once

/* NVIDIA DLSS discovery, capability probing, model selection, and D3D11 dispatch. */

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <system_error>
#include <vector>

#include <d3d11.h>
#include <dxgi.h>
#include <nvsdk_ngx.h>
#include <winver.h>
#include <wrl/client.h>

#include <embed/dlss_encode_game_output.h>
#include <embed/dlss_prepare_game_inputs.h>
#include <include/reshade.hpp>

#include "../runtime/camera_state.hpp"
#include "../runtime/frame_inputs.hpp"
#include "../runtime/logging.hpp"
#include "../runtime/state.hpp"

#if defined(_WIN64)
#if defined(_DEBUG)
#pragma comment(lib, "../external/DLSS/lib/Windows_x86_64/x64/nvsdk_ngx_s_dbg.lib")
#else
#pragma comment(lib, "../external/DLSS/lib/Windows_x86_64/x64/nvsdk_ngx_s.lib")
#endif
#pragma comment(lib, "version.lib")
#endif

namespace taa::dlss {

inline constexpr char PROJECT_ID[] = "98dbfc11-0026-4cd1-b573-168ceb4c7bb5";
inline constexpr char ENGINE_VERSION[] = "MGSV-RenoDX-1";
inline constexpr uint32_t NVIDIA_VENDOR_ID = 0x10DEu;
inline constexpr uint32_t THREAD_GROUP_SIZE = 8u;

// These values match NVSDK_NGX_DLSS_Hint_Render_Preset. The current public
// header no longer names A-D, but compatible feature DLLs still accept their
// stable numeric preset identifiers.
enum class Model : uint8_t {
  DEFAULT = 0u,
  A = 1u,
  B = 2u,
  C = 3u,
  D = 4u,
  E = 5u,
  F = 6u,
  J = 10u,
  K = 11u,
  L = 12u,
  M = 13u,
};

struct ModelOption {
  Model model;
  const char* label;
  const char* description;
};

inline constexpr std::array MODEL_OPTIONS = {
    ModelOption{Model::DEFAULT, "DLL Default", "Uses the model selected by the feature DLL."},
    ModelOption{Model::A, "Preset A", "History-conservative CNN; soft and prone to ghosting."},
    ModelOption{Model::B, "Preset B", "CNN tuned for extremely low input resolution."},
    ModelOption{Model::C, "Preset C", "Current-frame-biased CNN; sharper motion with more shimmer."},
    ModelOption{Model::D, "Preset D", "History-heavy CNN; stable but more prone to ghosting."},
    ModelOption{Model::E, "Preset E", "Balanced CNN model and the default selection."},
    ModelOption{Model::F, "Preset F", "Conservative, consistent CNN; typically the softest good CNN model."},
    ModelOption{Model::J, "Preset J", "Sharp first-generation transformer with more flicker than K."},
    ModelOption{Model::K, "Preset K", "Stable first-generation transformer and general native-resolution model."},
    ModelOption{Model::L, "Preset L", "Second-generation transformer optimized for very small inputs."},
    ModelOption{Model::M, "Preset M", "Aggressive second-generation transformer with strong detail recovery."},
};

inline constexpr Model DEFAULT_MODEL = Model::E;

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

struct Texture {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
};

struct alignas(16) PrepareConstants {
  std::array<float, 2> current_jitter_uv = {};
  float velocity_projection_jitter_scale = 1.f;
  float camera_reprojection_valid = 0.f;
  std::array<uint32_t, 2> render_size = {};
  std::array<float, 2> reciprocal_render_size = {};
  std::array<float, 16> current_to_previous_clip = {};
};

static_assert(sizeof(PrepareConstants) == 96u, "MGSV DLSS input constants must occupy 24 dwords");

struct Resources {
  // Non-owning identity, cleared at device destruction; guarded by the coordinator.
  reshade::api::device* adapter_device = nullptr;
  Microsoft::WRL::ComPtr<ID3D11Device> device;
  Microsoft::WRL::ComPtr<ID3D11Device> probed_device;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  NVSDK_NGX_Parameter* parameters = nullptr;
  NVSDK_NGX_Handle* feature = nullptr;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> prepare_shader;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> encode_shader;
  Microsoft::WRL::ComPtr<ID3D11Buffer> prepare_constant_buffer;
  Texture linear_input;
  Texture motion_input;
  Texture depth_input;
  Texture linear_output;
  Texture encoded_output;
  uint32_t width = 0u;
  uint32_t height = 0u;
  Model feature_model = Model::DEFAULT;
  bool ngx_initialized = false;
  bool probe_complete = false;
  bool work_submitted = false;
  bool history_initialized = false;
  bool suspended = false;
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
inline std::atomic<uint32_t> rejected_models = 0u;
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
  if ((rejected_models.load(std::memory_order_acquire) & (1u << static_cast<uint32_t>(model))) != 0u) return false;

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

inline const ModelOption* FindModelOption(Model model) {
  const auto found = std::find_if(
      MODEL_OPTIONS.begin(),
      MODEL_OPTIONS.end(),
      [model](const ModelOption& option) { return option.model == model; });
  return found == MODEL_OPTIONS.end() ? nullptr : &*found;
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

inline HRESULT WaitForGpu() {
  if (!resources.work_submitted) return S_OK;
  if (resources.device == nullptr || resources.context == nullptr
      || resources.context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) {
    return E_UNEXPECTED;
  }
  D3D11_QUERY_DESC description = {.Query = D3D11_QUERY_EVENT};
  Microsoft::WRL::ComPtr<ID3D11Query> query;
  const HRESULT query_result = resources.device->CreateQuery(&description, &query);
  if (FAILED(query_result)) return query_result;
  resources.context->End(query.Get());
  resources.context->Flush();
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  for (;;) {
    BOOL complete = FALSE;
    const HRESULT result = resources.context->GetData(
        query.Get(), &complete, sizeof(complete), D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if (result == S_OK && complete != FALSE) return S_OK;
    if (FAILED(result)) return result;
    if (std::chrono::steady_clock::now() >= deadline) return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    SwitchToThread();
  }
}

inline bool ReleaseFeatureResources() {
  const HRESULT wait_result = WaitForGpu();
  if (FAILED(wait_result)) {
    logging::Warn("NVIDIA DLSS GPU completion not proven; retaining resources result=", static_cast<uint32_t>(wait_result),
                  " device_status=", static_cast<uint32_t>(
                      resources.device != nullptr ? resources.device->GetDeviceRemovedReason() : E_UNEXPECTED));
    availability.store(Availability::INITIALIZATION_FAILED, std::memory_order_release);
    QueueFatalFailure();
    return false;
  }
  if (resources.feature != nullptr) {
    const NVSDK_NGX_Result result = NVSDK_NGX_D3D11_ReleaseFeature(resources.feature);
    if (NVSDK_NGX_FAILED(result)) {
      logging::Warn("NVIDIA DLSS feature release failed; retaining resources result=", static_cast<uint32_t>(result));
      availability.store(Availability::INITIALIZATION_FAILED, std::memory_order_release);
      QueueFatalFailure();
      return false;
    }
    resources.feature = nullptr;
  }
  if (resources.parameters != nullptr) {
    NVSDK_NGX_D3D11_DestroyParameters(resources.parameters);
    resources.parameters = nullptr;
  }
  resources.prepare_shader.Reset();
  resources.encode_shader.Reset();
  resources.prepare_constant_buffer.Reset();
  resources.context.Reset();
  for (Texture* texture : std::array{
           &resources.linear_input,
           &resources.motion_input,
           &resources.depth_input,
           &resources.linear_output,
           &resources.encoded_output,
       }) {
    texture->texture.Reset();
    texture->srv.Reset();
    texture->uav.Reset();
  }
  resources.width = 0u;
  resources.height = 0u;
  resources.feature_model = Model::DEFAULT;
  resources.work_submitted = false;
  resources.history_initialized = false;
  resources.suspended = false;
  resources.previous_dispatch_time = {};
  return true;
}

inline void Shutdown(bool reset_probe = true) {
  runtime_ready.store(false, std::memory_order_release);
  if (!ReleaseFeatureResources()) return;
  if (resources.ngx_initialized && resources.device != nullptr) {
    NVSDK_NGX_D3D11_Shutdown1(resources.device.Get());
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
    rejected_models.store(0u, std::memory_order_release);
    availability.store(Availability::UNCHECKED, std::memory_order_release);
  }
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
  const NVSDK_NGX_Result requirement_result = NVSDK_NGX_D3D11_GetFeatureRequirements(
      adapter.Get(),
      &discovery,
      &requirement);
  if (NVSDK_NGX_SUCCEED(requirement_result)) {
    const uint32_t unsupported = static_cast<uint32_t>(requirement.FeatureSupported);
    if ((unsupported & NVSDK_NGX_FeatureSupportResult_DriverVersionUnsupported) != 0u) {
      availability.store(Availability::DRIVER_OUTDATED, std::memory_order_release);
      return false;
    }
    if ((unsupported & (NVSDK_NGX_FeatureSupportResult_AdapterUnsupported
                        | NVSDK_NGX_FeatureSupportResult_OSVersionBelowMinimumSupported
                        | NVSDK_NGX_FeatureSupportResult_NotImplemented))
        != 0u) {
      availability.store(Availability::GPU_UNSUPPORTED, std::memory_order_release);
      return false;
    }
  }
  const NVSDK_NGX_Result init_result = NVSDK_NGX_D3D11_Init_with_ProjectID(
      PROJECT_ID,
      NVSDK_NGX_ENGINE_TYPE_CUSTOM,
      ENGINE_VERSION,
      data_path.c_str(),
      native_device,
      &feature_info);
  if (NVSDK_NGX_FAILED(init_result)) {
    Availability failure = Availability::INITIALIZATION_FAILED;
    if (init_result == NVSDK_NGX_Result_FAIL_OutOfDate) {
      failure = Availability::DRIVER_OUTDATED;
    } else if (init_result == NVSDK_NGX_Result_FAIL_FeatureNotSupported) {
      failure = Availability::GPU_UNSUPPORTED;
    }
    availability.store(failure, std::memory_order_release);
    if (LogEvery(1u)) logging::Warn("NVIDIA NGX D3D11 initialization failed result=", static_cast<uint32_t>(init_result));
    return false;
  }

  resources.device = native_device;
  resources.ngx_initialized = true;
  NVSDK_NGX_Parameter* capability_parameters = nullptr;
  const NVSDK_NGX_Result capability_result = NVSDK_NGX_D3D11_GetCapabilityParameters(&capability_parameters);
  int available = 0;
  int needs_updated_driver = 0;
  NVSDK_NGX_Result available_result = NVSDK_NGX_Result_FAIL_InvalidParameter;
  NVSDK_NGX_Result driver_result = NVSDK_NGX_Result_FAIL_InvalidParameter;
  if (capability_parameters != nullptr) {
    available_result = capability_parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &available);
    driver_result = capability_parameters->Get(
        NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver,
        &needs_updated_driver);
    NVSDK_NGX_D3D11_DestroyParameters(capability_parameters);
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
  if (device != nullptr
      && resources.device != nullptr
      && reinterpret_cast<ID3D11Device*>(device->get_native()) != resources.device.Get()) {  // NOLINT(performance-no-int-to-ptr)
    return;
  }
  Shutdown();
}

inline void SuspendTemporalResources(reshade::api::device* device) {
  if (device != nullptr
      && resources.device != nullptr
      && reinterpret_cast<ID3D11Device*>(device->get_native()) != resources.device.Get()) {  // NOLINT(performance-no-int-to-ptr)
    return;
  }
  if (resources.suspended || (resources.feature == nullptr && resources.width == 0u)) return;

  // Present does not own all immediate-context submissions. Retain one bundle
  // across method switches rather than waiting/releasing NGX on that thread.
  // Recreate incompatible bundles only from immediate dispatch; destroy at
  // device shutdown. Keep submission ownership for the eventual GPU wait.
  resources.suspended = true;
  resources.history_initialized = false;
  resources.previous_dispatch_time = {};
  logging::Info("suspended NVIDIA DLSS; retaining resources size=", resources.width, "x", resources.height);
}

inline void InvalidateHistory() {
  resources.history_initialized = false;
}

inline bool CreateTexture(ID3D11Device* device, DXGI_FORMAT format, uint32_t width, uint32_t height, Texture& output) {
  D3D11_TEXTURE2D_DESC description = {};
  description.Width = width;
  description.Height = height;
  description.MipLevels = 1u;
  description.ArraySize = 1u;
  description.Format = format;
  description.SampleDesc.Count = 1u;
  description.Usage = D3D11_USAGE_DEFAULT;
  description.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
  return SUCCEEDED(device->CreateTexture2D(&description, nullptr, &output.texture))
         && SUCCEEDED(device->CreateShaderResourceView(output.texture.Get(), nullptr, &output.srv))
         && SUCCEEDED(device->CreateUnorderedAccessView(output.texture.Get(), nullptr, &output.uav));
}

inline bool CreateComputeShader(
    ID3D11Device* device,
    std::span<const uint8_t> code,
    Microsoft::WRL::ComPtr<ID3D11ComputeShader>& output) {
  return SUCCEEDED(device->CreateComputeShader(code.data(), code.size(), nullptr, &output));
}

inline void SetModelHint(NVSDK_NGX_Parameter* parameters, Model model) {
  parameters->Set(NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA, static_cast<uint32_t>(model));
}

inline bool CreateResources(
    reshade::api::device* device,
    ID3D11DeviceContext* context,
    uint32_t width,
    uint32_t height) {
  if (!runtime_ready.load(std::memory_order_acquire) || !resources.ngx_initialized
      || device != resources.adapter_device
      || context == nullptr || context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) return false;
  if (!ReleaseFeatureResources()) return false;

  ID3D11Device* native_device = resources.device.Get();
  resources.context = context;
  const Model model = NormalizeModel(static_cast<float>(GetModel()));
  SetModel(model);
  if (!CreateComputeShader(native_device, __dlss_prepare_game_inputs, resources.prepare_shader)
      || !CreateComputeShader(native_device, __dlss_encode_game_output, resources.encode_shader)
      || !CreateTexture(native_device, DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, resources.linear_input)
      || !CreateTexture(native_device, DXGI_FORMAT_R16G16_FLOAT, width, height, resources.motion_input)
      || !CreateTexture(native_device, DXGI_FORMAT_R32_FLOAT, width, height, resources.depth_input)
      || !CreateTexture(native_device, DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, resources.linear_output)
      || !CreateTexture(native_device, DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, resources.encoded_output)) {
    ReleaseFeatureResources();
    return false;
  }

  D3D11_BUFFER_DESC constant_buffer_description = {};
  constant_buffer_description.ByteWidth = sizeof(PrepareConstants);
  constant_buffer_description.Usage = D3D11_USAGE_DYNAMIC;
  constant_buffer_description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  constant_buffer_description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  if (FAILED(native_device->CreateBuffer(
          &constant_buffer_description,
          nullptr,
          &resources.prepare_constant_buffer))) {
    ReleaseFeatureResources();
    return false;
  }

  const NVSDK_NGX_Result parameter_result = NVSDK_NGX_D3D11_AllocateParameters(&resources.parameters);
  if (NVSDK_NGX_FAILED(parameter_result) || resources.parameters == nullptr) {
    ReleaseFeatureResources();
    return false;
  }
  resources.parameters->Set(NVSDK_NGX_Parameter_Width, width);
  resources.parameters->Set(NVSDK_NGX_Parameter_Height, height);
  resources.parameters->Set(NVSDK_NGX_Parameter_OutWidth, width);
  resources.parameters->Set(NVSDK_NGX_Parameter_OutHeight, height);
  resources.parameters->Set(NVSDK_NGX_Parameter_PerfQualityValue, static_cast<int>(NVSDK_NGX_PerfQuality_Value_DLAA));
  resources.parameters->Set(
      NVSDK_NGX_Parameter_DLSS_Feature_Create_Flags,
      static_cast<int>(NVSDK_NGX_DLSS_Feature_Flags_IsHDR
                       | NVSDK_NGX_DLSS_Feature_Flags_MVLowRes
                       | NVSDK_NGX_DLSS_Feature_Flags_DepthInverted
                       | NVSDK_NGX_DLSS_Feature_Flags_AutoExposure));
  resources.parameters->Set(NVSDK_NGX_Parameter_DLSS_Enable_Output_Subrects, 0);
  resources.parameters->Set(NVSDK_NGX_Parameter_RTXValue, 0);
  SetModelHint(resources.parameters, model);

  Model effective_model = model;
  // NGX creation can submit work, even before our first input-preparation pass.
  resources.work_submitted = true;
  NVSDK_NGX_Result create_result = NVSDK_NGX_D3D11_CreateFeature(
      context,
      NVSDK_NGX_Feature_SuperSampling,
      resources.parameters,
      &resources.feature);
  if (NVSDK_NGX_FAILED(create_result) && model != Model::DEFAULT) {
    rejected_models.fetch_or(1u << static_cast<uint32_t>(model), std::memory_order_acq_rel);
    SetModelHint(resources.parameters, Model::DEFAULT);
    create_result = NVSDK_NGX_D3D11_CreateFeature(
        context,
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

  resources.width = width;
  resources.height = height;
  resources.feature_model = effective_model;
  logging::Info("NVIDIA DLSS feature created size=", width, "x", height,
                " model=", FindModelOption(effective_model)->label);
  return true;
}

inline bool EnsureResources(
    reshade::api::device* device,
    ID3D11DeviceContext* context,
    uint32_t width,
    uint32_t height) {
  if (!runtime_ready.load(std::memory_order_acquire)) return false;
  const Model model = NormalizeModel(static_cast<float>(GetModel()));
  if (resources.feature != nullptr
      && availability.load(std::memory_order_acquire) == Availability::AVAILABLE
      && device != nullptr
      && reinterpret_cast<ID3D11Device*>(device->get_native()) == resources.device.Get()  // NOLINT(performance-no-int-to-ptr)
      && context == resources.context.Get()
      && resources.width == width
      && resources.height == height
      && resources.feature_model == model) {
    if (resources.suspended) {
      resources.suspended = false;
      resources.history_initialized = false;
      resources.previous_dispatch_time = {};
      logging::Info("resumed NVIDIA DLSS with retained resources size=", width, "x", height,
                    " model=", FindModelOption(model)->label);
    }
    return true;
  }
  return CreateResources(device, context, width, height);
}

inline void UnbindComputeResources(ID3D11DeviceContext* context) {
  static const std::array<ID3D11ShaderResourceView*, 16> NULL_SRVS = {};
  static const std::array<ID3D11UnorderedAccessView*, 8> NULL_UAVS = {};
  context->CSSetShaderResources(0u, static_cast<uint32_t>(NULL_SRVS.size()), NULL_SRVS.data());
  context->CSSetUnorderedAccessViews(0u, static_cast<uint32_t>(NULL_UAVS.size()), NULL_UAVS.data(), nullptr);
}

inline bool PrepareGameInputs(ID3D11DeviceContext* context, const ValidatedFrameInputs& inputs, bool reset) {
  const std::array<ID3D11ShaderResourceView*, 4> srvs = {
      reinterpret_cast<ID3D11ShaderResourceView*>(inputs.color_srv.handle),            // NOLINT(performance-no-int-to-ptr)
      reinterpret_cast<ID3D11ShaderResourceView*>(inputs.velocity_srv.handle),         // NOLINT(performance-no-int-to-ptr)
      reinterpret_cast<ID3D11ShaderResourceView*>(inputs.depth_srv.handle),            // NOLINT(performance-no-int-to-ptr)
      reinterpret_cast<ID3D11ShaderResourceView*>(inputs.object_velocity_srv.handle),  // NOLINT(performance-no-int-to-ptr)
  };
  const std::array<ID3D11UnorderedAccessView*, 3> uavs = {
      resources.linear_input.uav.Get(),
      resources.motion_input.uav.Get(),
      resources.depth_input.uav.Get(),
  };
  const PrepareConstants constants = {
      .current_jitter_uv = {inputs.camera.jitter_uv_x, inputs.camera.jitter_uv_y},
      .velocity_projection_jitter_scale = state::GetProjectionJitterScale(state::ProjectionJitterPath::VELOCITY),
      .camera_reprojection_valid = !reset && inputs.camera.camera_reprojection_valid ? 1.f : 0.f,
      .render_size = {resources.width, resources.height},
      .reciprocal_render_size = {
          1.f / static_cast<float>(resources.width),
          1.f / static_cast<float>(resources.height),
      },
      .current_to_previous_clip = inputs.camera.current_to_previous_clip,
  };

  D3D11_MAPPED_SUBRESOURCE mapped = {};
  if (FAILED(context->Map(resources.prepare_constant_buffer.Get(), 0u, D3D11_MAP_WRITE_DISCARD, 0u, &mapped))) {
    return false;
  }
  std::memcpy(mapped.pData, &constants, sizeof(constants));
  context->Unmap(resources.prepare_constant_buffer.Get(), 0u);

  UnbindComputeResources(context);
  context->CSSetShaderResources(0u, static_cast<uint32_t>(srvs.size()), srvs.data());
  context->CSSetUnorderedAccessViews(0u, static_cast<uint32_t>(uavs.size()), uavs.data(), nullptr);
  ID3D11Buffer* constant_buffer = resources.prepare_constant_buffer.Get();
  context->CSSetConstantBuffers(0u, 1u, &constant_buffer);
  context->CSSetShader(resources.prepare_shader.Get(), nullptr, 0u);
  context->Dispatch(
      (resources.width + THREAD_GROUP_SIZE - 1u) / THREAD_GROUP_SIZE,
      (resources.height + THREAD_GROUP_SIZE - 1u) / THREAD_GROUP_SIZE,
      1u);
  resources.work_submitted = true;
  UnbindComputeResources(context);
  return true;
}

inline void EncodeGameOutput(ID3D11DeviceContext* context, reshade::api::resource_view encoded_scene_srv) {
  const std::array<ID3D11ShaderResourceView*, 2> srvs = {
      resources.linear_output.srv.Get(),
      reinterpret_cast<ID3D11ShaderResourceView*>(encoded_scene_srv.handle),  // NOLINT(performance-no-int-to-ptr)
  };
  ID3D11UnorderedAccessView* output = resources.encoded_output.uav.Get();
  UnbindComputeResources(context);
  context->CSSetShaderResources(0u, static_cast<uint32_t>(srvs.size()), srvs.data());
  context->CSSetUnorderedAccessViews(0u, 1u, &output, nullptr);
  context->CSSetShader(resources.encode_shader.Get(), nullptr, 0u);
  context->Dispatch(
      (resources.width + THREAD_GROUP_SIZE - 1u) / THREAD_GROUP_SIZE,
      (resources.height + THREAD_GROUP_SIZE - 1u) / THREAD_GROUP_SIZE,
      1u);
  UnbindComputeResources(context);
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

inline bool Dispatch(const ValidatedFrameInputs& inputs, MethodOutput& output) {
  if (inputs.device == nullptr || inputs.device->get_api() != reshade::api::device_api::d3d11) {
    QueueFatalFailure();
    return false;
  }
  if (inputs.color_format != reshade::api::format::r16g16b16a16_float
      || (inputs.depth_format != reshade::api::format::r32_float
          && inputs.depth_format != reshade::api::format::r32_float_x8_uint)) {
    if (LogEvery(30u)) logging::Warn("rejecting NVIDIA DLSS dispatch with incompatible resources");
    QueueFatalFailure();
    return false;
  }

  auto* context = reinterpret_cast<ID3D11DeviceContext*>(inputs.cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (context == nullptr || context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) {
    if (LogEvery(1u)) logging::Warn("NVIDIA DLSS requires an immediate D3D11 context");
    QueueFatalFailure();
    return false;
  }
  if (!EnsureResources(inputs.device, context, inputs.width, inputs.height)) {
    QueueFatalFailure();
    return false;
  }

  const bool reset = !resources.history_initialized;
  if (!PrepareGameInputs(context, inputs, reset)) {
    QueueFatalFailure();
    return false;
  }

  resources.parameters->Set(NVSDK_NGX_Parameter_Color, resources.linear_input.texture.Get());
  resources.parameters->Set(NVSDK_NGX_Parameter_Output, resources.linear_output.texture.Get());
  resources.parameters->Set(NVSDK_NGX_Parameter_Depth, resources.depth_input.texture.Get());
  resources.parameters->Set(NVSDK_NGX_Parameter_MotionVectors, resources.motion_input.texture.Get());
  resources.parameters->Set(
      NVSDK_NGX_Parameter_Jitter_Offset_X,
      inputs.camera.jitter_uv_x * static_cast<float>(resources.width));
  resources.parameters->Set(
      NVSDK_NGX_Parameter_Jitter_Offset_Y,
      inputs.camera.jitter_uv_y * static_cast<float>(resources.height));
  resources.parameters->Set(NVSDK_NGX_Parameter_Reset, reset ? 1 : 0);
  resources.parameters->Set(NVSDK_NGX_Parameter_MV_Scale_X, static_cast<float>(resources.width));
  resources.parameters->Set(NVSDK_NGX_Parameter_MV_Scale_Y, static_cast<float>(resources.height));
  resources.parameters->Set(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Width, resources.width);
  resources.parameters->Set(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Height, resources.height);
  resources.parameters->Set(NVSDK_NGX_Parameter_DLSS_Pre_Exposure, 1.f);
  resources.parameters->Set(NVSDK_NGX_Parameter_DLSS_Exposure_Scale, 1.f);
  resources.parameters->Set(NVSDK_NGX_Parameter_Sharpness, 0.f);
  resources.parameters->Set(NVSDK_NGX_Parameter_FrameTimeDeltaInMsec, FrameDeltaMilliseconds(reset));

  const NVSDK_NGX_Result result = NVSDK_NGX_D3D11_EvaluateFeature_C(
      context,
      resources.feature,
      resources.parameters,
      nullptr);
  if (NVSDK_NGX_FAILED(result)) {
    resources.history_initialized = false;
    if (LogEvery(30u)) logging::Warn("NVIDIA DLSS evaluation failed result=", static_cast<uint32_t>(result));
    QueueFatalFailure();
    return false;
  }

  EncodeGameOutput(context, inputs.color_srv);
  if (reset) {
    logging::Info("NVIDIA DLSS accumulation started insertion=", inputs.insertion_name,
                  " frame=", inputs.frame_token,
                  " sample=", inputs.sample_index,
                  " size=", resources.width, "x", resources.height,
                  " model=", FindModelOption(resources.feature_model)->label);
  }
  resources.history_initialized = true;
  output = {
      .resource = reshade::api::resource{reinterpret_cast<uint64_t>(resources.encoded_output.texture.Get())},
      .final_usage = reshade::api::resource_usage::unordered_access,
  };
  return true;
}

}  // namespace taa::dlss
