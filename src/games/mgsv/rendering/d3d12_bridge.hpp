#ifndef RENODX_GAMES_MGSV_RENDERING_D3D12_BRIDGE_HPP_
#define RENODX_GAMES_MGSV_RENDERING_D3D12_BRIDGE_HPP_

#include <algorithm>
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
#include "d3d12_timing.hpp"
#include "shared_texture.hpp"

// Runs the mod's D3D12 compute work (DLSS, FSR, Analytical TAA, reflections) for
// the game's D3D11 renderer on one D3D12 device and queue. Images are shared
// between the APIs, and fences order each submission on the GPU: D3D11 signals,
// D3D12 waits and runs, then D3D11 waits before it reads the result.
namespace mgsv::rendering::d3d12_bridge {

inline void Check(HRESULT result, const char* operation) {
  if (FAILED(result)) throw std::system_error(static_cast<int>(result), std::system_category(), operation);
}

inline Microsoft::WRL::ComPtr<IUnknown> GetDeviceIdentity(ID3D11Device* device) {
  // ReShade can return its device proxy from a native texture's GetDevice.
  // The proxy and the native device publish the same association, so compare
  // that object's COM identity; otherwise one device could look like two.
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
    // Images the mod creates as shareable are imported directly. A game image
    // usually is not shareable, so it goes through a shared copy instead.
    direct = shared_texture != nullptr && CanImport(description, binding);
    const D3D11_TEXTURE2D_DESC shared_description = {
        .Width = description.Width,
        .Height = description.Height,
        .MipLevels = description.MipLevels,
        .ArraySize = description.ArraySize,
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
    // A shareable image already carries the handle its creator exported.
    // Exporting the same texture again for each slot would break DXGI's rule
    // of one export per texture.
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

// A recording callback borrows these objects for this call only and records a
// complete pass. Both images are in the COMMON state when the callback starts
// and must be back in COMMON when it returns.
struct Dispatch {
  ID3D12GraphicsCommandList* commands = nullptr;
  ID3D12Resource* input = nullptr;
  ID3D12Resource* output = nullptr;
  // A pass keeps descriptors and constants per slot; they are free to reuse once
  // this slot retires.
  size_t slot_index = 0u;
  // GPU timing of this recording, active only on a diagnostic request.
  d3d12_timing::Scope* timing = nullptr;

  // Ends a named timing span covering the work recorded since the last one.
  void Mark(const char* name) const {
    if (timing != nullptr) {
      timing->Mark(name);
    }
  }
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
// context. GPU waits order the two APIs. Fence values checked on the CPU guard
// allocator reset and slot reuse without waiting in the render loop.
class Bridge {
 public:
  static constexpr size_t MAX_IN_FLIGHT = 4u;

  struct Slot {
    SharedImage input;
    SharedImage output;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commands;
    D3D11_TEXTURE2D_DESC description = {};
    D3D11_TEXTURE2D_DESC target_description = {};
    // The slot is free once D3D11 has finished its copies to and from the shared
    // images and D3D12 has finished this command list.
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
    // The mod's D3D12 passes manage their own scheduling, state and diagnostics,
    // so create their objects below ReShade, where add-on callbacks do not see
    // them. Private data can pass through other wrappers, so first confirm the
    // returned device is exactly ReShade's proxy. Other wrappers stay in place;
    // they may need to see every call.
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

  // Reconstruction and lighting share this ordering. `prepare` and `complete`
  // run any D3D11 copies before and after the D3D12 work.
  template <typename Prepare, typename Complete>
  void Submit(ID3D11DeviceContext* context, Slot* slot, const Scene& scene,
              Prepare prepare, Complete complete, bool copied_input, bool copied_output,
              SubmissionTiming* timing = nullptr) {
    // If prepare throws, nothing has been queued yet. After that, `failed` stays
    // set until the whole submission succeeds, because a partial submission
    // must stop later bridge work.
    const auto transfer_started = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
    prepare();
    failed = true;
    const auto copied = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
    const auto ready = renodx::utils::gpu_fence::QueueSignal(context, d3d11_fence.get());
    const auto signaled11 = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
    if (ready == renodx::utils::gpu_fence::NO_SIGNAL) throw std::runtime_error("D3D11 bridge signal rejected");
    if (timing != nullptr) {
      timing->input_copy_us = (!copied_input ? 0. : std::chrono::duration<double, std::micro>(copied - transfer_started).count());
      timing->signal11_us = std::chrono::duration<double, std::micro>(signaled11 - copied).count();
    }
    const auto waiting12 = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
    // Leave explicit flushing to the game's normal submission path. The runtime
    // may also submit internally. Standalone callers must submit the D3D11
    // stream before waiting for CPU-visible completion.
    if (!renodx::utils::gpu_fence::QueueWait(queue12.Get(), d3d11_fence.get(), ready)) throw std::runtime_error("D3D12 bridge wait rejected");
    const auto waited12 = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
    const std::array<ID3D12CommandList*, 1> commands = {slot->commands.Get()};
    queue12->ExecuteCommandLists(static_cast<UINT>(commands.size()), commands.data());
    const auto executed12 = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
    slot->d3d12_completion = renodx::utils::gpu_fence::QueueSignal(queue12.Get(), d3d12_fence.get());
    if (slot->d3d12_completion == renodx::utils::gpu_fence::NO_SIGNAL) throw std::runtime_error("D3D12 bridge signal rejected");
    d3d12_timing::Submitted(slot->d3d12_completion);
    const auto signaled12 = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
    if (!renodx::utils::gpu_fence::QueueWait(context, d3d12_fence.get(), slot->d3d12_completion)) throw std::runtime_error("D3D11 bridge wait rejected");
    const auto waited11 = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
    slot->d3d11_completion = ready;
    complete();
    if (copied_output) {
      slot->d3d11_completion = renodx::utils::gpu_fence::QueueSignal(context, d3d11_fence.get());
      if (slot->d3d11_completion == renodx::utils::gpu_fence::NO_SIGNAL) throw std::runtime_error("D3D11 return signal rejected");
    }
    // Direct output has no return copy. Its D3D12 completion already covers
    // this slot's last image access, so it needs no additional D3D11 signal.
    failed = false;
    ++statistics.submitted;
    statistics.d3d11_completion = slot->d3d11_completion;
    statistics.d3d12_completion = slot->d3d12_completion;
    statistics.last_scene = scene;
    if (timing != nullptr) {
      const auto finished = std::chrono::steady_clock::now();
      timing->wait12_us = std::chrono::duration<double, std::micro>(waited12 - waiting12).count();
      timing->execute12_us = std::chrono::duration<double, std::micro>(executed12 - waited12).count();
      timing->signal12_us = std::chrono::duration<double, std::micro>(signaled12 - executed12).count();
      timing->wait11_us = std::chrono::duration<double, std::micro>(waited11 - signaled12).count();
      timing->return_copy_us = std::chrono::duration<double, std::micro>(finished - waited11).count();
      timing->queue_us = timing->input_copy_us + timing->signal11_us + timing->wait12_us
                         + timing->execute12_us + timing->signal12_us + timing->wait11_us + timing->return_copy_us;
    }
  }

  // Each consumer owns its command pool. Lighting cannot exhaust the four
  // reconstruction slots, and both still submit on the same queue and fences.
  using WorkPool = std::vector<std::unique_ptr<Slot>>;

  template <typename Record, typename Prepare, typename Complete>
  [[nodiscard]] bool ExecuteWork(ID3D11DeviceContext* context, const Scene& scene, WorkPool* pool,
                                 Record record, Prepare prepare, Complete complete, bool copied_output) {
    rejection = nullptr;
    if (failed || context == nullptr || context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE
        || scene.id == 0u || scene.generation == 0u) {
      rejection = "Compute work requires a healthy bridge and an executed scene";
      return false;
    }
    Microsoft::WRL::ComPtr<ID3D11Device> owner;
    context->GetDevice(&owner);
    if (GetDeviceIdentity(owner.Get()) != device_identity) throw std::runtime_error("Compute context belongs to another device");
    if (DeviceRemoved()) throw std::runtime_error("D3D12 compute device removed");
    const auto completed11 = d3d11_fence->GetCompletedValue();
    const auto completed12 = d3d12_fence->GetCompletedValue();
    size_t index = 0u;
    for (; index < pool->size(); ++index) {
      if ((*pool)[index]->d3d11_completion <= completed11 && (*pool)[index]->d3d12_completion <= completed12) break;
    }
    if (index == pool->size()) {
      if (pool->size() == MAX_IN_FLIGHT) {
        rejection = "Compute command slots are still in use";
        ++statistics.busy;
        return false;
      }
      auto slot = std::make_unique<Slot>();
      Check(device12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&slot->allocator)), "Create compute command allocator");
      Check(device12->CreateCommandList(0u, D3D12_COMMAND_LIST_TYPE_DIRECT, slot->allocator.Get(), nullptr, IID_PPV_ARGS(&slot->commands)), "Create compute command list");
      Check(slot->commands->Close(), "Close initial compute list");
      pool->push_back(std::move(slot));
      ++statistics.allocations;
    } else {
      ++statistics.reused;
    }
    auto* slot = (*pool)[index].get();
    Check(slot->allocator->Reset(), "Reset compute allocator");
    Check(slot->commands->Reset(slot->allocator.Get(), nullptr), "Reset compute list");
    d3d12_timing::Collect(completed12);
    d3d12_timing::Scope timing(device12.Get(), queue12.Get(), slot->commands.Get(), d3d12_timing::Stream::LIGHTING);
    try {
      if (!record(Dispatch{.commands = slot->commands.Get(), .slot_index = index, .timing = &timing})) {
        Check(slot->commands->Close(), "Close canceled compute list");
        return false;
      }
    } catch (...) {
      slot->commands->Close();
      throw;
    }
    timing.Finish();
    Check(slot->commands->Close(), "Close compute list");
    Submit(context, slot, scene, prepare, complete, true, copied_output);
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
    if (description.MipLevels != 1u || target.MipLevels != 1u || description.ArraySize != 1u || target.ArraySize != 1u
        || description.SampleDesc.Count != 1u || target.SampleDesc.Count != 1u
        || (description.Format != DXGI_FORMAT_R16G16B16A16_FLOAT && description.Format != DXGI_FORMAT_R32G32B32A32_FLOAT)
        || (target.Format != DXGI_FORMAT_R16G16B16A16_FLOAT && target.Format != DXGI_FORMAT_R32G32B32A32_FLOAT)) return reject("Unsupported color texture layout or format");
    if (DeviceRemoved()) throw std::runtime_error("D3D12 bridge device removed");

    // Prefer the same imported images. Copied native images only need matching
    // dimensions. Reuse each command allocator after both API users finish.
    const auto matches = [&](const Slot& slot) {
      return slot.description.Width == description.Width && slot.description.Height == description.Height
             && slot.description.Format == description.Format
             && slot.target_description.Width == target.Width && slot.target_description.Height == target.Height
             && slot.target_description.Format == target.Format
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
      *selected = std::move(replacement);
      ++statistics.allocations;
    } else {
      ++statistics.reused;
    }
    if (!matches(*selected)) {
      // Only retired slots reach this point. Replace their images after both
      // APIs have finished using the previous pair.
      auto input = SharedImage(device11.Get(), device12.Get(), source, description, D3D11_BIND_SHADER_RESOURCE, shared_source, &statistics.image_imports);
      auto output = SharedImage(device11.Get(), device12.Get(), destination, target, D3D11_BIND_UNORDERED_ACCESS, shared_destination, &statistics.image_imports);
      selected->input = std::move(input);
      selected->output = std::move(output);
      selected->description = description;
      selected->target_description = target;
    }

    // Finish recording before submitting anything. A rejected recording leaves
    // the destination unchanged and does not create a GPU dependency.
    const auto prepared = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});
    Check(selected->allocator->Reset(), "Reset bridge allocator");
    Check(selected->commands->Reset(selected->allocator.Get(), nullptr), "Reset bridge command list");
    d3d12_timing::Collect(d3d12_fence->GetCompletedValue());
    d3d12_timing::Scope gpu_timing(device12.Get(), queue12.Get(), selected->commands.Get(), d3d12_timing::Stream::RECONSTRUCTION);
    try {
      record(Dispatch{
          .commands = selected->commands.Get(),
          .input = selected->input.resource.Get(),
          .output = selected->output.resource.Get(),
          .slot_index = static_cast<size_t>(std::find_if(slots.begin(), slots.end(), [selected](const auto& slot) { return slot.get() == selected; }) - slots.begin()),
          .timing = &gpu_timing,
      });
    } catch (...) {
      selected->commands->Close();
      throw;
    }
    gpu_timing.Finish();
    Check(selected->commands->Close(), "Close recorded bridge command list");
    const auto recorded = (timing != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{});

    if (timing != nullptr) {
      timing->preparation_us = std::chrono::duration<double, std::micro>(prepared - started).count();
      timing->recording_us = std::chrono::duration<double, std::micro>(recorded - prepared).count();
    }
    Submit(
        context, selected, scene,
        [&]() {
          if (!selected->input.direct) {
            context->CopyResource(selected->input.texture.Get(), source);
            ++statistics.input_copies;
          }
        },
        [&]() {
          if (!selected->output.direct) {
            context->CopyResource(destination, selected->output.texture.Get());
            ++statistics.output_copies;
          }
        },
        !selected->input.direct, !selected->output.direct, timing);
    return true;
  }
};

// The reconstruction coordinator serializes every user, including lighting
// callbacks. Switching methods releases that method's state but keeps this device.
static std::unique_ptr<Bridge> backend;

inline Bridge& Get(ID3D11Device* device) {
  if (backend == nullptr) {
    backend = std::make_unique<Bridge>(device);
  }
  if (backend->device_identity != GetDeviceIdentity(device)) {
    throw std::runtime_error("Rendering device changed without teardown");
  }
  return *backend;
}

inline void Drain() {
  if (backend != nullptr && !backend->Drain()) {
    throw std::runtime_error("D3D12 rendering is still in use; retaining resources");
  }
}

inline void Destroy() {
  Drain();
  d3d12_timing::Release();
  backend.reset();
}

}  // namespace mgsv::rendering::d3d12_bridge

#endif  // RENODX_GAMES_MGSV_RENDERING_D3D12_BRIDGE_HPP_
