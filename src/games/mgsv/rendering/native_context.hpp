/*
 * Copyright (C) 2026 Musa Haji
 * SPDX-License-Identifier: MIT
 */
#ifndef RENODX_GAMES_MGSV_RENDERING_NATIVE_CONTEXT_HPP_
#define RENODX_GAMES_MGSV_RENDERING_NATIVE_CONTEXT_HPP_

#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../../../utils/cross_addon.hpp"
#include "../../../utils/data.hpp"
#include "../../../utils/detour.hpp"

namespace mgsv::rendering::native_context {

// These offsets describe the verified retail executable. Native pointers are
// read only while their native owner is executing. Recorded data contains copies.
inline constexpr uint32_t IMAGE_TIMESTAMP = 0x6A4CB898u;
inline constexpr uint32_t IMAGE_SIZE = 0xA01D000u;
inline constexpr size_t MAIN_VIEW_ACCESSOR_RVA = 0x6AD870u;
inline constexpr std::array<uint8_t, 8> MAIN_VIEW_ACCESSOR_BYTES = {0x48, 0x8B, 0x05, 0x09, 0x3A, 0x53, 0x02, 0xC3};
// Bound native pool bookkeeping and linked-list traversal if the layout changes.
inline constexpr size_t MAX_RECORDS = 32768u;
inline constexpr size_t MAX_ACTIVE_VIEWS = 256u;
inline constexpr GUID UNWRAPPED_OBJECT = {.Data1 = 0x7F2C9A11, .Data2 = 0x3B4E, .Data3 = 0x4D6A, .Data4 = {0x81, 0x2F, 0x5E, 0x9C, 0xD3, 0x7A, 0x1B, 0x42}};

// Offsets belong to the object named by each constant. Dg is the shared backend.
inline constexpr size_t RENDER_INFO_OFFSET = 0x98u;
inline constexpr size_t ACTIVE_VIEWS_OFFSET = 0xA0u;
inline constexpr size_t OUTPUT_SIZE_OFFSET = 0x180u;
inline constexpr size_t RENDER_SIZE_OFFSET = 0x190u;
inline constexpr size_t INFO_RENDER_SIZE_OFFSET = 0x14u;
inline constexpr size_t INFO_PRESENTATION_OFFSET = 0x20u;
inline constexpr size_t NATIVE_SWAPCHAIN_OFFSET = 0x18u;
inline constexpr size_t VIEW_CAMERA_OFFSET = 0x570u;
inline constexpr size_t VIEW_RECTANGLE_OFFSET = 0x5D0u;
inline constexpr size_t NEXT_VIEW_OFFSET = 0x30u;
inline constexpr size_t BRANCH_NODE_OFFSET = 0x10u;
inline constexpr size_t BRANCH_TARGET_OFFSET = 0x18u;
inline constexpr size_t BRANCH_MATRICES_OFFSET = 0x40u;
inline constexpr size_t D3D_CONTEXT_OFFSET = 8u;
inline constexpr size_t DOF_EFFECT_OFFSET = 0x68u;

enum class Kind : uint8_t {
  RENDER,
  VIEW,
  END_VIEW,
  BRANCH,
  WORKER_BRANCH,
  WORKER,
  END_WORKER,
  BEGIN_RECORDING,
  END_RECORDING,
  FINISH,
  EXECUTE,
  MERGE,
  DESTROY_VIEW,
  DESTROY_RENDER,
  MEMBERSHIP,
  PRESENTATION,
  DOF,
  END_DOF,
};
inline constexpr std::array KIND_NAMES = {"render", "view", "end_view", "branch", "worker_branch", "worker", "end_worker", "begin_recording", "end_recording", "finish", "execute", "merge", "destroy_view", "destroy_render", "membership", "presentation", "dof", "end_dof"};

struct Owner {
  // scene identifies one InitView invocation. generation changes when the
  // selected presentation changes, so resized output cannot reuse an old scene.
  // IDs identify lifetimes until the presentation changes. Addresses only explain where
  // the copied values came from and are never used to reopen a finished owner.
  uint64_t generation = 0u;
  uint64_t render_id = 0u;
  uint64_t view_id = 0u;
  uint64_t scene = 0u;
  uint64_t membership = 0u;
  uintptr_t backend = 0u;
  uintptr_t renderer = 0u;
  uintptr_t render_info = 0u;
  uintptr_t viewport = 0u;
  uintptr_t camera = 0u;
  std::array<uint32_t, 2> render_size = {};
  std::array<uint32_t, 2> output_size = {};
  std::array<int32_t, 4> rectangle = {};
  bool main_view = false;
  bool active_member = false;
  bool presentation_matches = false;
  bool presentation_unwrapped = false;
  bool presentation_by_buffer = false;
  // Final display work is recorded by the enclosing renderer after EndView.
  // Keep the main scene from this build available to that continuation.
  uint64_t presentation_scene = 0u;
  bool operator==(const Owner&) const = default;
};

struct Node {
  uint64_t serial = 0u;
  Owner owner;
};

struct Renderer {
  uint64_t serial = 0u;
  uint64_t membership = 0u;
  std::vector<uint64_t> views;
};

struct Branch {
  // Both branch paths fill this before the game releases work to another thread.
  // The node serial prevents a reused pool address from selecting an old view.
  Node node;
  uintptr_t node_address = 0u;
  uintptr_t target = 0u;
  std::array<float, 32> matrices = {};
};

struct Event {
  Kind kind = Kind::RENDER;
  uint64_t sequence = 0u;
  uint32_t thread = 0u;
  Owner owner;
  uintptr_t subject = 0u;
  uintptr_t related = 0u;
  uint64_t node = 0u;
  uint64_t recording = 0u;
  uint64_t list = 0u;
  int64_t result = 0;
  bool restore = false;
  // Borrowed only for the observer call. The observer copies anything it keeps.
  const Branch* branch = nullptr;
  std::span<const uint64_t> members;
};

struct Scope {
  Owner owner;
  uint64_t node = 0u;
};

// MainExec supplies the actual viewport and effect before the low-level draw.
// Keep this association local to the invocation, including nested effects.
struct DofScope {
  Scope scope;
  uintptr_t effect = 0u;
  uint64_t invocation = 0u;
  bool matches_view = false;
};
using Observer = void (*)(const Event&);

// One module owns the hooks and all native bookkeeping. Other addons exchange
// copied scopes through these entry points. No allocator-owned object crosses
// the DLL boundary. The provider is pinned while native jobs can return to it.
struct __declspec(uuid("e9ee935e-2efd-4613-9cad-15e506da643c")) Shared {
  std::shared_mutex mutex;
  std::atomic<Scope (*)()> read = nullptr;
  Scope (*exchange)(Scope) = nullptr;
  void (*set_presentation)(IDXGISwapChain*) = nullptr;
  bool (*set_observer)(Observer, Observer) = nullptr;
  void (*remove_hooks)() = nullptr;
  uint32_t consumers = 0u;
  std::atomic<uint64_t> next_generation = 1u;
  bool (*healthy)() = nullptr;
  Owner (*read_presentation_owner)() = nullptr;
  DofScope (*read_dof)() = nullptr;
};
static renodx::utils::cross_addon::Shared<Shared> shared;
static bool registered = false;

struct Runtime {
  // A branch lookup must observe its node and view from the same update.
  // This lock protects all four indexes as one operation.
  std::shared_mutex mutex;
  uint64_t generation = 1u;
  uint64_t next_id = 0u;
  std::atomic<bool> failed = false;
  renodx::utils::data::ParallelFlatHashMap<uintptr_t, Node> nodes;
  renodx::utils::data::ParallelFlatHashMap<uintptr_t, Branch> branches;
  renodx::utils::data::ParallelFlatHashMap<uintptr_t, Renderer> renders;
  renodx::utils::data::ParallelFlatHashMap<uintptr_t, uint64_t> views;
  Microsoft::WRL::ComPtr<IDXGISwapChain> presentation;
  Owner presentation_owner;
};
static Runtime runtime;
static thread_local DofScope dof_scope;
static thread_local Owner owner;
static thread_local uint64_t node_serial = 0u;
struct Worker {
  uintptr_t object = 0u;
  Owner previous_owner;
  uint64_t previous_node = 0u;
};
static thread_local std::vector<Worker> workers;
struct ViewScope {
  uintptr_t viewport = 0u;
  Owner previous_owner;
};
static thread_local std::vector<ViewScope> view_owners;
static std::shared_mutex observer_mutex;
static std::atomic<Observer> observer = nullptr;

// An observer only copies events. It must not call back into ownership or make
// D3D calls. Holding this read lock lets unsubscription wait for every callback.
inline void Emit(Event event) {
  if (observer.load(std::memory_order_relaxed) == nullptr) return;
  const std::shared_lock lock(observer_mutex);
  if (const auto callback = observer.load(std::memory_order_relaxed); callback != nullptr) {
    event.thread = GetCurrentThreadId();
    callback(event);
  }
}

// Failure to track a native job disables qualification. Native calls still run,
// and consumers can use their existing fallback without borrowing another view.
template <typename Callback>
inline void Track(Callback&& callback) noexcept {
  if (runtime.failed.load(std::memory_order_relaxed)) return;
  try {
    callback();
  } catch (...) {
    runtime.failed.store(true, std::memory_order_relaxed);
  }
}

template <typename T>
inline T Read(uintptr_t object, size_t offset) {
  T result{};
  if (object != 0u) {
    std::memcpy(&result, reinterpret_cast<const void*>(object + offset), sizeof(result));  // NOLINT(performance-no-int-to-ptr)
  }
  return result;
}

inline Scope GetScope() {
  if (shared.data == nullptr) return {};
  const auto read = shared.data->read.load(std::memory_order_acquire);
  return (read != nullptr ? read() : Scope{});
}

inline Owner GetPresentationOwner() {
  if (shared.data == nullptr || shared.data->read_presentation_owner == nullptr) return {};
  return shared.data->read_presentation_owner();
}

inline DofScope GetDofScope() {
  if (shared.data == nullptr || shared.data->read_dof == nullptr) return {};
  return shared.data->read_dof();
}

// A submitted addon dispatch runs on the immediate context, after the native
// worker has returned. Give it the scene copied into its recorded job.
class ScopedOwner {
 public:
  explicit ScopedOwner(Scope selected) {
    if (shared.data != nullptr) {
      exchange = shared.data->exchange;
    }
    if (exchange != nullptr) {
      previous = exchange(selected);
    }
  }
  ScopedOwner(const ScopedOwner&) = delete;
  ScopedOwner& operator=(const ScopedOwner&) = delete;
  ~ScopedOwner() {
    if (exchange != nullptr) {
      exchange(previous);
    }
  }

 private:
  Scope (*exchange)(Scope) = nullptr;
  Scope previous;
};

inline void SetPresentation(IDXGISwapChain* presentation) {
  if (shared.data != nullptr && shared.data->set_presentation != nullptr) {
    shared.data->set_presentation(presentation);
  }
}

inline bool SetObserver(Observer expected, Observer replacement) {
  return shared.data != nullptr && shared.data->set_observer != nullptr
         && shared.data->set_observer(expected, replacement);
}
using Build = void(__fastcall*)(uintptr_t, uintptr_t, uintptr_t, uint32_t);
using Pair = void(__fastcall*)(uintptr_t, uintptr_t);
using Single = void(__fastcall*)(uintptr_t);
using Allocate = uintptr_t(__fastcall*)(uintptr_t);
using Accessor = uintptr_t(__fastcall*)();
static Build build_render = nullptr;
static Pair init_render = nullptr;
static Pair init_view = nullptr;
static Pair end_view = nullptr;
static Pair branch = nullptr;
static Pair worker_branch = nullptr;
static Pair worker_begin = nullptr;
static Single worker_end = nullptr;
static Allocate allocate_node = nullptr;
static Pair begin_record = nullptr;
static Pair end_record = nullptr;
static Single destroy_view = nullptr;
static Single destroy_render = nullptr;
static Single update_views = nullptr;
static Accessor get_main_view = nullptr;
using DofMain = void(__fastcall*)(uintptr_t, uintptr_t, uintptr_t);
static DofMain dof_main = nullptr;
static std::shared_mutex hook_mutex;
static std::vector<renodx::utils::detour::Function> hooks;
static bool installed = false;
static std::string installation_status = "Native ownership hooks have not been installed";

inline void __fastcall BuildRender(uintptr_t renderer, uintptr_t backend, uintptr_t argument, uint32_t flags) {
  // The backend is shared. Bind this renderer only for its synchronous build,
  // then let the retained branch records carry that association to workers.
  const auto previous = owner;
  const auto previous_node = node_serial;
  const auto previous_views = view_owners.size();
  Track([&] {
    const std::unique_lock lock(runtime.mutex);
    if (runtime.failed.load(std::memory_order_relaxed)) return;
    owner = {
        .generation = runtime.generation,
        .backend = backend,
        .renderer = renderer,
        .render_info = Read<uintptr_t>(renderer, RENDER_INFO_OFFSET),
        .render_size = Read<std::array<uint32_t, 2>>(backend, RENDER_SIZE_OFFSET),
        .output_size = Read<std::array<uint32_t, 2>>(backend, OUTPUT_SIZE_OFFSET),
    };
    node_serial = 0u;
    auto [entry, added] = runtime.renders.try_emplace(renderer);
    if (added) {
      entry->second.serial = ++runtime.next_id;
    }
    owner.render_id = entry->second.serial;
    Emit({.kind = Kind::RENDER, .owner = owner, .subject = renderer, .related = backend});
  });
  build_render(renderer, backend, argument, flags);
  owner = previous;
  node_serial = previous_node;
  // Restore the enclosing scope even if a view failed to acquire an owner.
  view_owners.resize(previous_views);
}

inline void __fastcall InitRender(uintptr_t backend, uintptr_t render_info) {
  init_render(backend, render_info);
  Track([&] {
    owner.render_info = render_info;
    owner.render_size = Read<std::array<uint32_t, 2>>(render_info, INFO_RENDER_SIZE_OFFSET);
    const auto wrapper = Read<uintptr_t>(render_info, INFO_PRESENTATION_OFFSET);
    auto* native = Read<IDXGISwapChain*>(wrapper, NATIVE_SWAPCHAIN_OFFSET);
    Microsoft::WRL::ComPtr<IDXGISwapChain> expected;
    {
      const std::shared_lock lock(runtime.mutex);
      if (owner.generation != runtime.generation) return;
      expected = runtime.presentation;
    }
    bool used_unwrap = false;
    const auto canonical = [&used_unwrap](IUnknown* object) {
      Microsoft::WRL::ComPtr<IUnknown> result;
      if (object == nullptr) return result;
      Microsoft::WRL::ComPtr<IUnknown> unwrapped;
      if (SUCCEEDED(object->QueryInterface(UNWRAPPED_OBJECT, reinterpret_cast<void**>(unwrapped.GetAddressOf())))) {
        object = unwrapped.Get();
        used_unwrap = true;
      }
      object->QueryInterface(IID_PPV_ARGS(&result));
      return result;
    };
    if (native != nullptr && expected != nullptr) {
      const auto native_identity = canonical(native);
      const auto expected_identity = canonical(expected.Get());
      owner.presentation_matches = native_identity != nullptr && native_identity == expected_identity;
      if (!owner.presentation_matches) {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> native_buffer;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> expected_buffer;
        if (SUCCEEDED(native->GetBuffer(0u, IID_PPV_ARGS(&native_buffer))) && SUCCEEDED(expected->GetBuffer(0u, IID_PPV_ARGS(&expected_buffer)))) {
          const auto native_buffer_identity = canonical(native_buffer.Get());
          owner.presentation_matches = native_buffer_identity != nullptr && native_buffer_identity == canonical(expected_buffer.Get());
          owner.presentation_by_buffer = owner.presentation_matches;
        }
      }
    }
    owner.presentation_unwrapped = used_unwrap;
    const std::unique_lock lock(runtime.mutex);
    if (runtime.failed.load(std::memory_order_relaxed)) return;
    Emit({.kind = Kind::PRESENTATION, .owner = owner});
  });
}

inline void __fastcall InitView(uintptr_t backend, uintptr_t viewport) {
  Track([&] {
    const std::unique_lock lock(runtime.mutex);
    if (runtime.failed.load(std::memory_order_relaxed) || owner.generation != runtime.generation) return;
    view_owners.push_back({.viewport = viewport, .previous_owner = owner});
    owner.viewport = viewport;
    owner.camera = Read<uintptr_t>(viewport, VIEW_CAMERA_OFFSET);
    owner.rectangle = Read<std::array<int32_t, 4>>(viewport, VIEW_RECTANGLE_OFFSET);
    owner.main_view = get_main_view != nullptr && get_main_view() == viewport;
    auto [entry, added] = runtime.views.try_emplace(viewport, 0u);
    if (added) {
      entry->second = ++runtime.next_id;
    }
    owner.view_id = entry->second;
    owner.scene = ++runtime.next_id;
    if (const auto render = runtime.renders.find(owner.renderer); render != runtime.renders.end()) {
      owner.membership = render->second.membership;
      owner.active_member = std::ranges::find(render->second.views, owner.view_id) != render->second.views.end();
    }
    // Start runs before this frame's views. The next native size update can
    // use this verified backend instead of choosing one by matching dimensions.
    if (owner.main_view && owner.active_member && owner.presentation_matches) {
      runtime.presentation_owner = owner;
    }
    Emit({.kind = Kind::VIEW, .owner = owner, .subject = viewport, .related = backend});
  });
  init_view(backend, viewport);
}

inline void __fastcall DofMainExec(uintptr_t plugin, uintptr_t renderer, uintptr_t viewport) {
  const auto previous = dof_scope;
  dof_scope = {};
  Track([&] {
    const auto effect = Read<uintptr_t>(plugin, DOF_EFFECT_OFFSET);
    const auto camera = Read<uintptr_t>(viewport, VIEW_CAMERA_OFFSET);
    const std::unique_lock lock(runtime.mutex);
    if (runtime.failed.load(std::memory_order_relaxed) || owner.generation != runtime.generation) return;
    dof_scope = {
        .scope = {.owner = owner, .node = node_serial},
        .effect = effect,
        .invocation = ++runtime.next_id,
        .matches_view = owner.scene != 0u && owner.renderer == renderer && owner.viewport == viewport
                        && owner.camera != 0u && owner.camera == camera,
    };
    Emit({.kind = Kind::DOF, .owner = owner, .subject = viewport, .related = effect, .node = node_serial, .recording = dof_scope.invocation, .result = dof_scope.matches_view ? 1 : 0});
  });
  dof_main(plugin, renderer, viewport);
  Track([&] {
    Emit({.kind = Kind::END_DOF, .owner = dof_scope.scope.owner, .subject = viewport, .related = dof_scope.effect, .node = dof_scope.scope.node, .recording = dof_scope.invocation});
  });
  dof_scope = previous;
}

inline void __fastcall EndView(uintptr_t backend, uintptr_t viewport) {
  const uint64_t main_scene = (owner.main_view && owner.active_member ? owner.scene : 0u);
  end_view(backend, viewport);
  Track([&] {
    const std::unique_lock lock(runtime.mutex);
    if (runtime.failed.load(std::memory_order_relaxed) || owner.generation != runtime.generation) return;
    Emit({.kind = Kind::END_VIEW, .owner = owner, .subject = viewport});
  });
  if (!view_owners.empty() && view_owners.back().viewport == viewport) {
    owner = view_owners.back().previous_owner;
    view_owners.pop_back();
    if (main_scene != 0u && owner.scene == 0u) {
      owner.presentation_scene = main_scene;
    }
  }
}

inline uintptr_t __fastcall AllocateNode(uintptr_t pool) {
  const uintptr_t node = allocate_node(pool);
  Track([&] {
    const std::unique_lock lock(runtime.mutex);
    if (runtime.failed.load(std::memory_order_relaxed) || node == 0u) return;
    if (runtime.nodes.size() < MAX_RECORDS || runtime.nodes.contains(node)) {
      runtime.nodes.insert_or_assign(node, Node{.serial = ++runtime.next_id, .owner = (owner.generation == runtime.generation ? owner : Owner{})});
    } else {
      runtime.failed.store(true, std::memory_order_relaxed);
    }
  });
  return node;
}

inline void CaptureBranch(uintptr_t subcontext, Kind kind) {
  // Dg and CommandBuffer both produce this layout. Copy the owner at the point
  // each job is created, including jobs created by an already running worker.
  Track([&] {
    Branch captured = {.node_address = Read<uintptr_t>(subcontext, BRANCH_NODE_OFFSET), .target = Read<uintptr_t>(subcontext, BRANCH_TARGET_OFFSET), .matrices = Read<std::array<float, 32>>(subcontext, BRANCH_MATRICES_OFFSET)};
    const std::unique_lock lock(runtime.mutex);
    if (runtime.failed.load(std::memory_order_relaxed)) return;
    const auto found = runtime.nodes.find(captured.node_address);
    if (found != runtime.nodes.end()) {
      captured.node = found->second;
      captured.node.owner = (owner.generation == runtime.generation ? owner : Owner{});
      found->second.owner = captured.node.owner;
    }
    if (runtime.branches.size() < MAX_RECORDS || runtime.branches.contains(captured.node_address)) {
      runtime.branches.insert_or_assign(captured.node_address, captured);
    } else {
      runtime.failed.store(true, std::memory_order_relaxed);
    }
    Emit({.kind = kind, .owner = captured.node.owner, .subject = subcontext, .related = captured.node_address, .node = captured.node.serial, .branch = &captured});
  });
}

inline void __fastcall BranchContext(uintptr_t backend, uintptr_t subcontext) {
  branch(backend, subcontext);
  CaptureBranch(subcontext, Kind::BRANCH);
}

inline void __fastcall BranchWorker(uintptr_t object, uintptr_t subcontext) {
  worker_branch(object, subcontext);
  CaptureBranch(subcontext, Kind::WORKER_BRANCH);
}

inline void __fastcall WorkerBegin(uintptr_t object, uintptr_t subcontext) {
  // Worker threads can serve several views. Start with an empty association and
  // accept only the branch whose native node still has the recorded generation.
  Track([&] {
    const std::unique_lock lock(runtime.mutex);
    if (runtime.failed.load(std::memory_order_relaxed)) return;
    workers.push_back({.object = object, .previous_owner = owner, .previous_node = node_serial});
    owner = {};
    node_serial = 0u;
    const auto node = Read<uintptr_t>(subcontext, BRANCH_NODE_OFFSET);
    const auto matrices = Read<std::array<float, 32>>(subcontext, BRANCH_MATRICES_OFFSET);
    int64_t worker_result = 0;
    const auto captured = runtime.branches.find(node);
    const auto live_node = runtime.nodes.find(node);
    if (captured != runtime.branches.end() && live_node != runtime.nodes.end()
        && captured->second.node_address == node && captured->second.node.serial == live_node->second.serial) {
      owner = captured->second.node.owner;
      node_serial = captured->second.node.serial;
      if (std::memcmp(matrices.data(), captured->second.matrices.data(), sizeof(matrices)) != 0) {
        worker_result = 2;
        owner = {};
        node_serial = 0u;
      }
    } else {
      worker_result = 1;
    }
    Emit({.kind = Kind::WORKER, .owner = owner, .subject = object, .related = subcontext, .node = node_serial, .result = worker_result});
  });
  worker_begin(object, subcontext);
}

inline void __fastcall WorkerEnd(uintptr_t object) {
  worker_end(object);
  if (workers.empty() || workers.back().object != object) return;
  Track([&] {
    const std::unique_lock lock(runtime.mutex);
    if (runtime.failed.load(std::memory_order_relaxed) || owner.generation != runtime.generation) return;
    Emit({.kind = Kind::END_WORKER, .owner = owner, .subject = object, .node = node_serial});
  });
  owner = workers.back().previous_owner;
  node_serial = workers.back().previous_node;
  workers.pop_back();
}

inline void __fastcall BeginRecord(uintptr_t node, uintptr_t context) {
  Track([&] {
    const auto native_context = Read<uintptr_t>(context, D3D_CONTEXT_OFFSET);
    const std::unique_lock lock(runtime.mutex);
    if (runtime.failed.load(std::memory_order_relaxed)) return;
    node_serial = 0u;
    if (const auto found = runtime.nodes.find(node); found != runtime.nodes.end()) {
      node_serial = found->second.serial;
      if (owner.generation == runtime.generation) {
        found->second.owner = owner;
      }
    }
    // The GN pointer may contain a ReShade proxy. The finish/work hooks record
    // the actual D3D context separately so the report can prove their crosswalk.
    Emit({.kind = Kind::BEGIN_RECORDING, .owner = owner, .subject = node, .related = native_context, .node = node_serial});
  });
  begin_record(node, context);
}

inline void __fastcall EndRecord(uintptr_t node, uintptr_t context) {
  // FinishCommandList runs inside this native call. Keep the node visible to the
  // D3D hook until Finish has returned, then restore the enclosing worker scope.
  const auto previous = node_serial;
  Track([&] {
    const std::unique_lock lock(runtime.mutex);
    if (runtime.failed.load(std::memory_order_relaxed)) return;
    node_serial = 0u;
    if (const auto found = runtime.nodes.find(node); found != runtime.nodes.end()) {
      node_serial = found->second.serial;
    }
    Emit({.kind = Kind::END_RECORDING, .owner = owner, .subject = node, .related = context, .node = node_serial});
  });
  end_record(node, context);
  node_serial = previous;
}

inline void __fastcall DestroyView(uintptr_t viewport) {
  Track([&] {
    const std::unique_lock lock(runtime.mutex);
    if (runtime.failed.load(std::memory_order_relaxed)) return;
    runtime.views.erase(viewport);
    if (runtime.presentation_owner.viewport == viewport) {
      runtime.presentation_owner = {};
    }
    Emit({.kind = Kind::DESTROY_VIEW, .subject = viewport});
  });
  destroy_view(viewport);
}

inline void __fastcall DestroyRender(uintptr_t renderer) {
  Track([&] {
    const std::unique_lock lock(runtime.mutex);
    if (runtime.failed.load(std::memory_order_relaxed)) return;
    runtime.renders.erase(renderer);
    if (runtime.presentation_owner.renderer == renderer) {
      runtime.presentation_owner = {};
    }
    Emit({.kind = Kind::DESTROY_RENDER, .subject = renderer});
  });
  destroy_render(renderer);
}

inline void __fastcall UpdateViews(uintptr_t renderer) {
  update_views(renderer);
  Track([&] {
    const std::unique_lock lock(runtime.mutex);
    if (runtime.failed.load(std::memory_order_relaxed)) return;
    auto [render, added] = runtime.renders.try_emplace(renderer);
    if (added) {
      render->second.serial = ++runtime.next_id;
    }
    std::vector<uint64_t> members;
    // This call applies the game's queued additions and removals. Each entry is
    // a live Viewport linked through +0x30, separate from the pending queues.
    auto view = Read<uintptr_t>(renderer, ACTIVE_VIEWS_OFFSET);
    while (view != 0u && members.size() < MAX_ACTIVE_VIEWS) {
      auto [entry, new_view] = runtime.views.try_emplace(view, 0u);
      if (new_view) {
        entry->second = ++runtime.next_id;
      }
      members.push_back(entry->second);
      view = Read<uintptr_t>(view, NEXT_VIEW_OFFSET);
    }
    if (view != 0u) {
      runtime.failed.store(true, std::memory_order_relaxed);
      return;
    }
    if (render->second.membership == 0u || render->second.views != members) {
      if (runtime.presentation_owner.renderer == renderer
          && std::ranges::find(members, runtime.presentation_owner.view_id) == members.end()) {
        runtime.presentation_owner = {};
      }
      render->second.views = std::move(members);
      render->second.membership = ++runtime.next_id;
      Emit({.kind = Kind::MEMBERSHIP, .owner = {.generation = runtime.generation, .render_id = render->second.serial, .membership = render->second.membership}, .subject = renderer, .members = render->second.views});
    }
  });
}

inline void RemoveHooks() {
  const std::unique_lock lock(hook_mutex);
  if (!installed) return;
  try {
    renodx::utils::detour::Uninstall(hooks);
    installed = false;
    hooks.clear();
    installation_status = "Native ownership hooks removed";
  } catch (const std::exception& error) {
    installation_status = error.what();
  }
}

inline bool Install() {
  shared.RegisterModule();
  if (shared.data == nullptr) return false;
  const std::unique_lock lock(shared.data->mutex);
  if (registered) return true;
  if (shared.data->consumers == 0u) {
    const std::unique_lock hook_lock(hook_mutex);
    if (!installed) {
      auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
      const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
      if (dos == nullptr || dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
      const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
      if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.TimeDateStamp != IMAGE_TIMESTAMP || pe->OptionalHeader.SizeOfImage != IMAGE_SIZE) {
        installation_status = "Native ownership requires the verified MGSV executable";
        return false;
      }
      struct Signature {
        size_t rva;
        std::string_view bytes;
        renodx::utils::detour::Function function;
      };
      const std::array signatures = {
          Signature{.rva = 0x1BEEC0u, .bytes = "405556574154415541564157488d6c24", .function = {&build_render, BuildRender}},
          Signature{.rva = 0x2496A0u, .bytes = "405556574154415541564157488d6c24", .function = {&init_render, InitRender}},
          Signature{.rva = 0x249BE0u, .bytes = "40555657415641574881ec8000000048", .function = {&init_view, InitView}},
          Signature{.rva = 0x247EB0u, .bytes = "40535556415641574881ec8000000048", .function = {&end_view, EndView}},
          Signature{.rva = 0x2C5220u, .bytes = "48895c2410574883ec20f68158010000", .function = {&branch, BranchContext}},
          Signature{.rva = 0x2D6F50u, .bytes = "48895c2410574883ec20f641100148", .function = {&worker_branch, BranchWorker}},
          Signature{.rva = 0x2D6B80u, .bytes = "488bc4574883ec6048c740c0feffffff", .function = {&worker_begin, WorkerBegin}},
          Signature{.rva = 0x2D6E80u, .bytes = "48894c2408534883ec3048c7442420fe", .function = {&worker_end, WorkerEnd}},
          Signature{.rva = 0x2C8130u, .bytes = "48895c2420574883ec208b4120488974", .function = {&allocate_node, AllocateNode}},
          Signature{.rva = 0x2C7C30u, .bytes = "836118fe488bc2488b5110488bc8e92d", .function = {&begin_record, BeginRecord}},
          Signature{.rva = 0x2C7D70u, .bytes = "40534883ec20488bd9488bcae8ffca72", .function = {&end_record, EndRecord}},
          Signature{.rva = 0x1B8600u, .bytes = "48894c2408574883ec3048c7442420fe", .function = {&destroy_view, DestroyView}},
          Signature{.rva = 0x1BCA60u, .bytes = "48894c2408574883ec3048c7442420fe", .function = {&destroy_render, DestroyRender}},
          Signature{.rva = 0x1BEA90u, .bytes = "488bc455574156488bec4883ec6048c7", .function = {&update_views, UpdateViews}},
          Signature{.rva = 0x1DD1F0u, .bytes = "48895c240848896c2410488974241857415641574883ec30", .function = {&dof_main, DofMainExec}},
      };
      for (const auto& signature : signatures) {
        for (size_t index = 0u; index < signature.bytes.size() / 2u; ++index) {
          const auto digit = [](char value) { return static_cast<uint8_t>(value <= '9' ? value - '0' : value - 'a' + 10); };
          const auto expected = static_cast<uint8_t>((digit(signature.bytes[index * 2u]) << 4u) | digit(signature.bytes[(index * 2u) + 1u]));
          if (base[signature.rva + index] != expected) {
            installation_status = "Native ownership hook bytes differ at RVA " + std::to_string(signature.rva);
            return false;
          }
        }
      }
      if (std::memcmp(base + MAIN_VIEW_ACCESSOR_RVA, MAIN_VIEW_ACCESSOR_BYTES.data(), MAIN_VIEW_ACCESSOR_BYTES.size()) != 0) {
        installation_status = "Native main-view accessor differs from the verified executable";
        return false;
      }
      get_main_view = reinterpret_cast<Accessor>(base + MAIN_VIEW_ACCESSOR_RVA);
      // A native job can still return through a detour during device teardown.
      // Pin its code for the process lifetime, as the existing native size hook does.
      HMODULE module = nullptr;
      if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                             reinterpret_cast<LPCWSTR>(&BuildRender), &module)
          == FALSE) return false;
      hooks.reserve(signatures.size());
      for (const auto& signature : signatures) {
        *signature.function.original = base + signature.rva;
        hooks.push_back(signature.function);
      }
      try {
        renodx::utils::detour::Install(hooks);
        installed = true;
        installation_status = "Native ownership hooks ready";
      } catch (const std::exception& error) {
        hooks.clear();
        installation_status = error.what();
      }
      if (!installed) return false;
    }
    {
      const std::unique_lock runtime_lock(runtime.mutex);
      runtime.generation = shared.data->next_generation.fetch_add(1u, std::memory_order_relaxed);
      runtime.failed.store(false, std::memory_order_relaxed);
      runtime.nodes.clear();
      runtime.branches.clear();
      runtime.renders.clear();
      runtime.views.clear();
      runtime.presentation_owner = {};
    }
    shared.data->exchange = +[](Scope selected) {
      const Scope previous = {.owner = owner, .node = node_serial};
      owner = selected.owner;
      node_serial = selected.node;
      return previous;
    };
    shared.data->set_presentation = +[](IDXGISwapChain* presentation) {
      Microsoft::WRL::ComPtr<IDXGISwapChain> retired;
      const std::unique_lock lock(runtime.mutex);
      if (runtime.presentation.Get() == presentation) return;
      retired.Swap(runtime.presentation);
      runtime.presentation = presentation;
      runtime.presentation_owner = {};
      runtime.generation = shared.data->next_generation.fetch_add(1u, std::memory_order_relaxed);
    };
    shared.data->set_observer = +[](Observer expected, Observer replacement) {
      const std::unique_lock lock(observer_mutex);
      if (observer.load(std::memory_order_relaxed) != expected) return false;
      observer.store(replacement, std::memory_order_relaxed);
      return true;
    };
    shared.data->remove_hooks = RemoveHooks;
    shared.data->healthy = +[] { return !runtime.failed.load(std::memory_order_relaxed); };
    shared.data->read_presentation_owner = +[]() -> Owner {
      const std::shared_lock lock(runtime.mutex);
      if (runtime.failed.load(std::memory_order_relaxed)) return {};
      return runtime.presentation_owner;
    };
    shared.data->read_dof = +[]() -> DofScope {
      const std::shared_lock lock(runtime.mutex);
      if (runtime.failed.load(std::memory_order_relaxed)
          || dof_scope.scope.owner.generation != runtime.generation) return {};
      return dof_scope;
    };
    shared.data->read.store(+[]() -> Scope {
      if (runtime.failed.load(std::memory_order_relaxed)) return {};
      return {.owner = owner, .node = node_serial}; }, std::memory_order_release);
  }
  ++shared.data->consumers;
  registered = true;
  return true;
}

inline void Shutdown() {
  if (shared.data == nullptr) return;
  {
    const std::unique_lock lock(shared.data->mutex);
    if (registered && --shared.data->consumers == 0u) {
      shared.data->read.store(nullptr, std::memory_order_release);
      shared.data->remove_hooks();
    }
    registered = false;
  }
  shared.UnregisterModule();
}

}  // namespace mgsv::rendering::native_context
#endif  // RENODX_GAMES_MGSV_RENDERING_NATIVE_CONTEXT_HPP_
