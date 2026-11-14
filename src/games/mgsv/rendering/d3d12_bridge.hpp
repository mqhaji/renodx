#ifndef RENODX_GAMES_MGSV_RENDERING_D3D12_BRIDGE_HPP_
#define RENODX_GAMES_MGSV_RENDERING_D3D12_BRIDGE_HPP_

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include "../../../utils/directx.hpp"
#include "../../../utils/gpu_fence.hpp"
#include "shared_texture.hpp"

namespace mgsv::rendering::d3d12_bridge {

inline void Check(HRESULT result, const char* operation) {
  if (FAILED(result)) throw std::system_error(static_cast<int>(result), std::system_category(), operation);
}

inline Microsoft::WRL::ComPtr<IUnknown> GetDeviceIdentity(ID3D11Device* device) {
  // ReShade can return its device proxy from a native texture's GetDevice.
  // Its published association is shared by both interfaces. Compare that
  // object's COM identity so wrappers cannot make one device look like two.
  constexpr GUID reshade_device = {.Data1 = 0x72299288, .Data2 = 0x2C68, .Data3 = 0x4AD8, .Data4 = {0x94, 0x5D, 0x2B, 0xFB, 0x5A, 0xA9, 0xC6, 0x09}};
  IUnknown* proxy = nullptr;
  UINT size = sizeof(proxy);
  const bool associated = SUCCEEDED(device->GetPrivateData(reshade_device, &size, &proxy))
                          && size == sizeof(proxy) && proxy != nullptr;
  IUnknown* associated_device = (associated ? proxy : device);
  Microsoft::WRL::ComPtr<IUnknown> identity;
  Check(associated_device->QueryInterface(IID_PPV_ARGS(&identity)), "Query D3D11 device identity");
  return identity;
}

struct Scene {
  uint64_t id = 0u;
  uint64_t generation = 0u;
};

struct SharedImage {
  Microsoft::WRL::ComPtr<ID3D12Resource> resource;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  bool direct = false;

  static bool CanImport(const D3D11_TEXTURE2D_DESC& description, UINT binding) {
    return (description.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE) != 0u
           && (description.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) == 0u
           && (description.BindFlags & binding) == binding;
  }

  SharedImage() = default;
  SharedImage(ID3D11Device* device11, ID3D12Device* device12, ID3D11Texture2D* original,
              const D3D11_TEXTURE2D_DESC& description, UINT binding, SharedTexture* shared_texture, uint64_t* import_count) {
    // Addon-owned images can be shared at creation. Import that allocation
    // directly. A native game image may still require a shared copy.
    direct = shared_texture != nullptr && CanImport(description, binding);
    const D3D11_TEXTURE2D_DESC shared_description = {
        .Width = description.Width,
        .Height = description.Height,
        .MipLevels = 1u,
        .ArraySize = 1u,
        .Format = description.Format,
        .SampleDesc = {.Count = 1u},
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = binding,
        .MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE,
    };
    if (direct) {
      texture = original;
    } else {
      Check(device11->CreateTexture2D(&shared_description, nullptr, &texture), "Create shared image");
    }
    // Existing allocations carry their producer's handle. Re-exporting the
    // same texture for each in-flight slot would violate DXGI's one-export rule.
    const auto exported = (direct ? std::shared_ptr<SharedTexture>{} : ExportTexture(texture.Get()));
    auto* shared = (direct ? shared_texture : exported.get());
    if (shared == nullptr) throw std::runtime_error("Export shared image failed");
    const std::unique_lock lock(shared->mutex);
    if (shared->device.Get() != device12) {
      Microsoft::WRL::ComPtr<ID3D12Resource> imported;
      Check(device12->OpenSharedHandle(shared->handle.get(), IID_PPV_ARGS(&imported)), "Import shared image into D3D12");
      shared->resource = std::move(imported);
      shared->device = device12;
      ++*import_count;
    }
    resource = shared->resource;
  }
};

// A recording callback borrows these objects for this call only. Both images
// enter and leave the callback in COMMON. The callback records a complete pass.
struct Dispatch {
  ID3D12GraphicsCommandList* commands = nullptr;
  ID3D12Resource* input = nullptr;
  ID3D12Resource* output = nullptr;
  ID3D12DescriptorHeap* descriptors = nullptr;
  // One descriptor table: input SRV followed by output UAV.
  D3D12_GPU_DESCRIPTOR_HANDLE input_view = {};
  uint32_t width = 0u;
  uint32_t height = 0u;
  Scene scene;
};

struct Statistics {
  uint64_t submitted = 0u;
  uint64_t allocations = 0u;
  uint64_t reused = 0u;
  uint64_t busy = 0u;
  uint64_t input_copies = 0u;
  uint64_t output_copies = 0u;
  uint64_t image_imports = 0u;
  uint64_t d3d11_completion = 0u;
  uint64_t d3d12_completion = 0u;
  Scene last_scene;
};

// Optional CPU measurements for one submission. Ordinary rendering leaves
// this disabled and only updates the inexpensive lifetime counters above.
struct SubmissionTiming {
  double preparation_us = 0.;
  double recording_us = 0.;
  double queue_us = 0.;
  double input_copy_us = 0.;
  double signal11_us = 0.;
  double wait12_us = 0.;
  double execute12_us = 0.;
  double signal12_us = 0.;
  double wait11_us = 0.;
  double return_copy_us = 0.;
};

// The caller serializes submission and destruction with the D3D11 immediate
// context. GPU waits order the APIs. CPU completion checks protect allocator
// reset and pool reuse without waiting in the render loop.
class Bridge {
 public:
  static constexpr size_t MAX_IN_FLIGHT = 4u;

  struct Slot {
    SharedImage input;
    SharedImage output;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commands;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptors;
    D3D11_TEXTURE2D_DESC description = {};
    // D3D11 must finish any mirror copies; D3D12 must finish this command list.
    uint64_t d3d11_completion = 0u;
    uint64_t d3d12_completion = 0u;
  };

  Microsoft::WRL::ComPtr<ID3D11Device5> device11;
  Microsoft::WRL::ComPtr<IUnknown> device_identity;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context11;
  // Keep the outer device alive until its private backend objects are released.
  // ReShade owns addon registration and teardown through that outer interface.
  Microsoft::WRL::ComPtr<ID3D12Device> device12_owner;
  Microsoft::WRL::ComPtr<ID3D12Device> device12;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue12;
  // Each API signals only its own timeline and waits on the other API's fence.
  std::unique_ptr<renodx::utils::gpu_fence::Fence> d3d11_fence;
  std::unique_ptr<renodx::utils::gpu_fence::Fence> d3d12_fence;
  std::vector<std::unique_ptr<Slot>> slots;
  Statistics statistics;
  const char* rejection = nullptr;
  bool failed = false;

  explicit Bridge(ID3D11Device* device) {
    if (device == nullptr) throw std::invalid_argument("D3D12 bridge requires a D3D11 device");
    Check(device->QueryInterface(IID_PPV_ARGS(&device11)), "D3D11 shared-fence support");
    device_identity = GetDeviceIdentity(device);
    device->GetImmediateContext(&context11);
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    Check(device->QueryInterface(IID_PPV_ARGS(&dxgi_device)), "D3D11 adapter query");
    Check(dxgi_device->GetAdapter(&adapter), "D3D11 adapter lookup");
    if (!renodx::utils::directx::Initialize()) throw std::runtime_error("DirectX entry points unavailable");
    // Use the game's adapter so shared images stay on the same GPU.
    Check(renodx::utils::directx::pD3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device12_owner)), "Create D3D12 bridge device");
    device12 = device12_owner;
    // Private passes already have their own scheduling, state, and diagnostics.
    // Create their objects below ReShade so game callbacks cannot intercept them.
    // Private data can pass through other wrappers, so first verify that the
    // returned device is exactly ReShade's published proxy. Leave other outer
    // wrappers intact; their own API contracts may require interception.
    constexpr GUID reshade_device = {.Data1 = 0x2523AFF4, .Data2 = 0x978B, .Data3 = 0x4939, .Data4 = {0xBA, 0x16, 0x8E, 0xE8, 0x76, 0xA4, 0xCB, 0x2A}};
    IUnknown* associated = nullptr;
    UINT association_size = sizeof(associated);
    Microsoft::WRL::ComPtr<ID3D12Device> proxy;
    if (SUCCEEDED(device12_owner->GetPrivateData(reshade_device, &association_size, &associated))
        && association_size == sizeof(associated) && associated != nullptr
        && SUCCEEDED(associated->QueryInterface(IID_PPV_ARGS(&proxy))) && proxy.Get() == device12_owner.Get()) {
      auto* downstream = proxy.Get();
      if (renodx::utils::directx::NativeFromReShadeProxy(&downstream)) {
        device12 = downstream;
      }
    }
    const D3D12_COMMAND_QUEUE_DESC queue_description = {.Type = D3D12_COMMAND_LIST_TYPE_DIRECT};
    Check(device12->CreateCommandQueue(&queue_description, IID_PPV_ARGS(&queue12)), "Create D3D12 bridge queue");
    d3d11_fence = std::make_unique<renodx::utils::gpu_fence::Fence>(device);
    d3d12_fence = std::make_unique<renodx::utils::gpu_fence::Fence>(device12.Get());
    slots.reserve(MAX_IN_FLIGHT);
  }

  Bridge(const Bridge&) = delete;
  Bridge& operator=(const Bridge&) = delete;

  [[nodiscard]] bool DeviceRemoved() const {
    return FAILED(device11->GetDeviceRemovedReason()) || FAILED(device12->GetDeviceRemovedReason());
  }

  // Use only at explicit teardown, outside DllMain. Rendering uses GPU waits.
  [[nodiscard]] bool Drain() const {
    context11->Flush();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (failed || d3d11_fence->IsBusy() || d3d12_fence->IsBusy()) {
      if (DeviceRemoved()) return true;
      if (failed || std::chrono::steady_clock::now() >= deadline) return false;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
  }

  // Returns false before changing the destination when no retired slot is
  // available. Each API owns its fence timeline. A copied output also needs
  // its D3D11 return copy to finish before D3D12 can overwrite that mirror.
  template <typename Record>
  [[nodiscard]] bool Execute(ID3D11DeviceContext* context, ID3D11Texture2D* source, ID3D11Texture2D* destination,
                             const Scene& scene, Record record, SharedTexture* shared_source = nullptr, SharedTexture* shared_destination = nullptr,
                             SubmissionTiming* timing = nullptr) {
    const auto started = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
    if (timing != nullptr) {
      *timing = {};
    }
    rejection = nullptr;
    const auto reject = [this](const char* reason) {
      rejection = reason;
      return false;
    };
    // Accept only an executed scene on the game's immediate context. Texture
    // ownership and layout must agree before allocating or submitting work.
    if (failed) return reject("Previous GPU submission failed");
    if (context == nullptr || source == nullptr || destination == nullptr) return reject("Missing context or color texture");
    if (scene.id == 0u || scene.generation == 0u) return reject("Missing executed scene identity");
    if (context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) return reject("Submission requires an immediate context");
    Microsoft::WRL::ComPtr<ID3D11Device> owner;
    context->GetDevice(&owner);
    if (GetDeviceIdentity(owner.Get()) != device_identity) return reject("Context belongs to another device");
    for (auto* texture : {source, destination}) {
      texture->GetDevice(&owner);
      if (GetDeviceIdentity(owner.Get()) != device_identity) return reject("Texture belongs to another device");
    }
    D3D11_TEXTURE2D_DESC description;
    D3D11_TEXTURE2D_DESC target;
    source->GetDesc(&description);
    destination->GetDesc(&target);
    if (source == destination) return reject("Input and output must be separate images");
    if (description.Width != target.Width || description.Height != target.Height || description.Format != target.Format
        || description.MipLevels != 1u || target.MipLevels != 1u || description.ArraySize != 1u || target.ArraySize != 1u
        || description.SampleDesc.Count != 1u || target.SampleDesc.Count != 1u
        || (description.Format != DXGI_FORMAT_R16G16B16A16_FLOAT && description.Format != DXGI_FORMAT_R32G32B32A32_FLOAT)) return reject("Unsupported color texture layout or format");
    if (DeviceRemoved()) throw std::runtime_error("D3D12 bridge device removed");

    // Prefer the same imported images. Copied native images only need matching
    // dimensions. Reuse each command allocator after both API users finish.
    const auto matches = [&](const Slot& slot) {
      return slot.description.Width == description.Width && slot.description.Height == description.Height
             && slot.description.Format == description.Format
             && slot.input.direct == (shared_source != nullptr && SharedImage::CanImport(description, D3D11_BIND_SHADER_RESOURCE))
             && slot.output.direct == (shared_destination != nullptr && SharedImage::CanImport(target, D3D11_BIND_UNORDERED_ACCESS))
             && (!slot.input.direct || slot.input.texture.Get() == source)
             && (!slot.output.direct || slot.output.texture.Get() == destination);
    };
    Slot* selected = nullptr;
    const uint64_t d3d11_completed = d3d11_fence->GetCompletedValue();
    const uint64_t d3d12_completed = d3d12_fence->GetCompletedValue();
    for (const auto& slot : slots) {
      if (slot->d3d11_completion > d3d11_completed || slot->d3d12_completion > d3d12_completed) continue;
      if (selected == nullptr) {
        selected = slot.get();
      }
      if (matches(*slot)) {
        selected = slot.get();
        break;
      }
    }
    if (selected == nullptr) {
      if (slots.size() == MAX_IN_FLIGHT) {
        ++statistics.busy;
        return false;
      }
      slots.push_back(std::make_unique<Slot>());
      selected = slots.back().get();
    }
    if (selected->commands == nullptr) {
      // Build a complete replacement before publishing it. Allocation failure
      // leaves this slot's last valid resources available for a later attempt.
      Slot replacement;
      Check(device12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&replacement.allocator)), "Create bridge command allocator");
      Check(device12->CreateCommandList(0u, D3D12_COMMAND_LIST_TYPE_DIRECT, replacement.allocator.Get(), nullptr, IID_PPV_ARGS(&replacement.commands)), "Create bridge command list");
      Check(replacement.commands->Close(), "Close initial bridge command list");
      const D3D12_DESCRIPTOR_HEAP_DESC heap = {.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, .NumDescriptors = 2u, .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
      Check(device12->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&replacement.descriptors)), "Create bridge descriptors");
      *selected = std::move(replacement);
      ++statistics.allocations;
    } else {
      ++statistics.reused;
    }
    if (!matches(*selected)) {
      // Only retired slots reach this point. Their descriptors can now refer to
      // different images without changing a command list still using them.
      auto input = SharedImage(device11.Get(), device12.Get(), source, description, D3D11_BIND_SHADER_RESOURCE, shared_source, &statistics.image_imports);
      auto output = SharedImage(device11.Get(), device12.Get(), destination, target, D3D11_BIND_UNORDERED_ACCESS, shared_destination, &statistics.image_imports);
      auto view = selected->descriptors->GetCPUDescriptorHandleForHeapStart();
      device12->CreateShaderResourceView(input.resource.Get(), nullptr, view);
      view.ptr += device12->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
      device12->CreateUnorderedAccessView(output.resource.Get(), nullptr, nullptr, view);
      selected->input = std::move(input);
      selected->output = std::move(output);
      selected->description = description;
    }

    // Finish recording before submitting anything. A rejected recording leaves
    // the destination unchanged and does not create a GPU dependency.
    const auto prepared = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
    Check(selected->allocator->Reset(), "Reset bridge allocator");
    Check(selected->commands->Reset(selected->allocator.Get(), nullptr), "Reset bridge command list");
    try {
      record(Dispatch{
          .commands = selected->commands.Get(),
          .input = selected->input.resource.Get(),
          .output = selected->output.resource.Get(),
          .descriptors = selected->descriptors.Get(),
          .input_view = selected->descriptors->GetGPUDescriptorHandleForHeapStart(),
          .width = description.Width,
          .height = description.Height,
          .scene = scene,
      });
    } catch (...) {
      selected->commands->Close();
      throw;
    }
    Check(selected->commands->Close(), "Close recorded bridge command list");
    const auto recorded = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});

    // A partial submission retains its slot until explicit recovery.
    failed = true;
    if (!selected->input.direct) {
      context->CopyResource(selected->input.texture.Get(), source);
      ++statistics.input_copies;
    }
    const auto copied = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
    const auto ready = renodx::utils::gpu_fence::QueueSignal(context, d3d11_fence.get());
    const auto signaled11 = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
    if (ready == renodx::utils::gpu_fence::NO_SIGNAL) throw std::runtime_error("D3D11 bridge signal rejected");
    if (timing != nullptr) {
      timing->input_copy_us = (selected->input.direct ? 0. : std::chrono::duration<double, std::micro>(copied - recorded).count());
      timing->signal11_us = std::chrono::duration<double, std::micro>(signaled11 - copied).count();
    }
    const auto waiting12 = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
    // Leave explicit flushing to the game's normal submission path. The runtime
    // may also submit internally. Standalone callers must submit the D3D11
    // stream before waiting for CPU-visible completion.
    if (!renodx::utils::gpu_fence::QueueWait(queue12.Get(), d3d11_fence.get(), ready)) throw std::runtime_error("D3D12 bridge wait rejected");
    const auto waited12 = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
    const std::array<ID3D12CommandList*, 1> commands = {selected->commands.Get()};
    queue12->ExecuteCommandLists(static_cast<UINT>(commands.size()), commands.data());
    const auto executed12 = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
    selected->d3d12_completion = renodx::utils::gpu_fence::QueueSignal(queue12.Get(), d3d12_fence.get());
    if (selected->d3d12_completion == renodx::utils::gpu_fence::NO_SIGNAL) throw std::runtime_error("D3D12 bridge signal rejected");
    const auto signaled12 = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
    if (!renodx::utils::gpu_fence::QueueWait(context, d3d12_fence.get(), selected->d3d12_completion)) throw std::runtime_error("D3D11 bridge wait rejected");
    const auto waited11 = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
    selected->d3d11_completion = ready;
    if (!selected->output.direct) {
      context->CopyResource(destination, selected->output.texture.Get());
      ++statistics.output_copies;
      selected->d3d11_completion = renodx::utils::gpu_fence::QueueSignal(context, d3d11_fence.get());
      if (selected->d3d11_completion == renodx::utils::gpu_fence::NO_SIGNAL) throw std::runtime_error("D3D11 return signal rejected");
    }
    // Direct output has no return copy. Its D3D12 completion already covers
    // this slot's last image access, so it needs no additional D3D11 signal.
    failed = false;
    ++statistics.submitted;
    statistics.d3d11_completion = selected->d3d11_completion;
    statistics.d3d12_completion = selected->d3d12_completion;
    statistics.last_scene = scene;
    if (timing != nullptr) {
      const auto finished = std::chrono::steady_clock::now();
      timing->preparation_us = std::chrono::duration<double, std::micro>(prepared - started).count();
      timing->recording_us = std::chrono::duration<double, std::micro>(recorded - prepared).count();
      timing->wait12_us = std::chrono::duration<double, std::micro>(waited12 - waiting12).count();
      timing->execute12_us = std::chrono::duration<double, std::micro>(executed12 - waited12).count();
      timing->signal12_us = std::chrono::duration<double, std::micro>(signaled12 - executed12).count();
      timing->wait11_us = std::chrono::duration<double, std::micro>(waited11 - signaled12).count();
      timing->return_copy_us = std::chrono::duration<double, std::micro>(finished - waited11).count();
      timing->queue_us = timing->input_copy_us + timing->signal11_us + timing->wait12_us
                         + timing->execute12_us + timing->signal12_us + timing->wait11_us + timing->return_copy_us;
    }
    return true;
  }
};

}  // namespace mgsv::rendering::d3d12_bridge

#endif  // RENODX_GAMES_MGSV_RENDERING_D3D12_BRIDGE_HPP_
