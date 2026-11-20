/*
 * Copyright (C) 2026 Musa Haji
 * SPDX-License-Identifier: MIT
 */
#ifndef RENODX_GAMES_MGSV_ENGINE_VIEWS_HPP_
#define RENODX_GAMES_MGSV_ENGINE_VIEWS_HPP_

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
#include "build.hpp"
#include "image.hpp"

// Tracks which renderer and view (camera viewport) each piece of the game's
// rendering belongs to. Hooks record an Owner when a view starts, copy it into
// each job the engine hands to a worker thread, and restore it while that worker
// records, so D3D11 callbacks can tell which scene they are drawing.
namespace mgsv::engine::views {

inline constexpr std::array REQUIRED_ENTRIES = {
    engine::Entry::RENDER_BUILD_RENDERING,
    engine::Entry::DG_INIT_RENDER,
    engine::Entry::DG_INIT_VIEWPORT,
    engine::Entry::DG_END_VIEWPORT,
    engine::Entry::DG_BRANCH,
    engine::Entry::COMMAND_BUFFER_BRANCH,
    engine::Entry::COMMAND_BUFFER_CONSTRUCTOR,
    engine::Entry::COMMAND_BUFFER_DESTRUCTOR,
    engine::Entry::ALLOCATE_COMMAND_LIST_NODE,
    engine::Entry::COMMAND_LIST_NODE_BEGIN_RECORD,
    engine::Entry::COMMAND_LIST_NODE_END_RECORD,
    engine::Entry::VIEWPORT_DESTRUCTOR,
    engine::Entry::RENDER_DESTRUCTOR,
    engine::Entry::RENDER_UPDATE_VIEWPORT_LIST,
    engine::Entry::DEPTH_OF_FIELD_MAIN_EXEC,
    engine::Entry::GET_MAIN_VIEWPORT,
};
inline constexpr std::array REQUIRED_FIELDS = {
    Field::RENDER_INFO,
    Field::RENDER_VIEWPORTS,
    Field::BACKEND_RENDER_SIZE,
    Field::RENDER_INFO_RENDER_SIZE,
    Field::RENDER_INFO_SWAP_CHAIN,
    Field::SWAP_CHAIN_NATIVE,
    Field::VIEWPORT_CAMERA,
    Field::VIEWPORT_RECTANGLE,
    Field::VIEWPORT_NEXT,
    Field::BRANCH_NODE,
    Field::BRANCH_TARGET,
    Field::BRANCH_MATRICES,
    Field::CONTEXT_D3D_CONTEXT,
    Field::DEPTH_OF_FIELD_EFFECT,
};
inline constexpr engine::Requirement REQUIREMENT = {.feature = "View tracking (needed by enhanced lighting and reconstruction)", .entries = REQUIRED_ENTRIES, .fields = REQUIRED_FIELDS};
// Engine pointers are read only while the engine call that owns them is running;
// everything kept afterward is a copy. These limits cap the node and branch tables
// and the active-view list walk, so an unexpected layout stops tracking instead of
// growing without bound.
inline constexpr size_t MAX_RECORDS = 32768u;
inline constexpr size_t MAX_ACTIVE_VIEWS = 256u;
inline constexpr GUID UNWRAPPED_OBJECT = {.Data1 = 0x7F2C9A11, .Data2 = 0x3B4E, .Data3 = 0x4D6A, .Data4 = {0x81, 0x2F, 0x5E, 0x9C, 0xD3, 0x7A, 0x1B, 0x42}};

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
  // scene is unique to one InitView call. generation changes whenever the
  // selected swapchain changes, so a resized output never matches an old scene.
  // The other IDs stay stable until then. Addresses only record where the copied
  // values came from; they are never used to reopen a finished owner.
  uint64_t generation = 0u;
  uint64_t render_id = 0u;
  uint64_t view_id = 0u;
  uint64_t scene = 0u;
  uint64_t membership = 0u;
  // The engine's rendering backend (DgDx11), shared by every renderer; it
  // holds the render size.
  uintptr_t backend = 0u;
  uintptr_t renderer = 0u;
  uintptr_t render_info = 0u;
  uintptr_t viewport = 0u;
  uintptr_t camera = 0u;
  std::array<uint32_t, 2> render_size = {};
  std::array<int32_t, 4> rectangle = {};
  bool main_view = false;
  bool active_member = false;
  bool presentation_matches = false;
  bool presentation_unwrapped = false;
  bool presentation_by_buffer = false;
  // The renderer records final display work after EndView, outside any view.
  // This keeps the main view's scene from the same build available to it.
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
  // Both branch hooks fill this before the game hands the job to a worker thread.
  // The node serial stops a reused node address from matching an older view.
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

// Depth of field's MainExec receives the real viewport and effect before its
// low-level draws. The scope is saved and restored around each call, so nested
// effects keep their own.
struct DofScope {
  Scope scope;
  uintptr_t effect = 0u;
  uint64_t invocation = 0u;
  bool matches_view = false;
};
using Observer = void (*)(const Event&);
// Runs on the recording thread as soon as the engine has begun a command list,
// before anything is recorded into it. GetRecording() names its D3D context.
using RecordingBegan = void (*)();

// The native objects recording on the calling thread. Both are borrowed: read
// them only inside the native call that is using them.
struct Recording {
  // The D3D11 context the engine records into between BeginRecord and EndRecord.
  // It can be a ReShade proxy; Unwrap() returns the native context.
  uintptr_t d3d_context = 0u;
  // The innermost worker CommandBuffer, which holds its CPU constant banks.
  uintptr_t command_buffer = 0u;
};

// Shared by every add-on in the process that uses view tracking. The first one to
// install owns the hooks and all bookkeeping; the others read copied scopes
// through these function pointers, so no heap object crosses a DLL boundary.
// The owning DLL stays pinned because engine jobs can return through its hooks.
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
  Recording (*read_recording)() = nullptr;
  bool (*set_recording_began)(RecordingBegan, RecordingBegan) = nullptr;
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
static thread_local uintptr_t recording_d3d_context = 0u;
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
static std::atomic<RecordingBegan> recording_began = nullptr;

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

// If tracking throws, it stops until the hooks are installed again. Engine calls
// still run and scopes read as empty, so features fall back to the native path
// instead of using another view's data.
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

// The view build whose image the selected presentation shows.
inline bool IsPresentedMainView(const Owner& owner) {
  return owner.scene != 0u && owner.main_view && owner.active_member && owner.presentation_matches;
}

inline Owner GetPresentationOwner() {
  if (shared.data == nullptr || shared.data->read_presentation_owner == nullptr) return {};
  return shared.data->read_presentation_owner();
}

inline DofScope GetDofScope() {
  if (shared.data == nullptr || shared.data->read_dof == nullptr) return {};
  return shared.data->read_dof();
}

inline Recording GetRecording() {
  if (shared.data == nullptr || shared.data->read_recording == nullptr) return {};
  return shared.data->read_recording();
}

// The engine stores ReShade's proxy. Its get_native() and D3D callbacks use the
// original object, which the proxy exposes through this interface ID.
inline Microsoft::WRL::ComPtr<ID3D11DeviceContext> Unwrap(uintptr_t stored_context) {
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  auto* proxy = reinterpret_cast<ID3D11DeviceContext*>(stored_context);  // NOLINT(performance-no-int-to-ptr)
  if (proxy != nullptr && FAILED(proxy->QueryInterface(UNWRAPPED_OBJECT, reinterpret_cast<void**>(context.GetAddressOf())))) {
    context = proxy;
  }
  return context;
}

// Add-on work submitted later on the immediate context runs after the engine
// worker has returned. This sets the thread's scope to the one copied into the
// recorded job for the guard's lifetime.
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

// Replaces the callback only if it is still the expected one, and returns only
// after any call of the previous one has finished.
inline bool SetRecordingBegan(RecordingBegan expected, RecordingBegan replacement) {
  return shared.data != nullptr && shared.data->set_recording_began != nullptr
         && shared.data->set_recording_began(expected, replacement);
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
        .render_info = Read<uintptr_t>(renderer, Value(Field::RENDER_INFO)),
        .render_size = Read<std::array<uint32_t, 2>>(backend, Value(Field::BACKEND_RENDER_SIZE)),
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
    owner.render_size = Read<std::array<uint32_t, 2>>(render_info, Value(Field::RENDER_INFO_RENDER_SIZE));
    const auto wrapper = Read<uintptr_t>(render_info, Value(Field::RENDER_INFO_SWAP_CHAIN));
    auto* native = Read<IDXGISwapChain*>(wrapper, Value(Field::SWAP_CHAIN_NATIVE));
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
    owner.camera = Read<uintptr_t>(viewport, Value(Field::VIEWPORT_CAMERA));
    owner.rectangle = Read<std::array<int32_t, 4>>(viewport, Value(Field::VIEWPORT_RECTANGLE));
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
    // Dg Start (reconstruction/resolution.hpp) runs before this frame's views,
    // so it reads this owner on the next frame. That gives it the verified
    // backend instead of guessing one by matching sizes.
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
    const auto effect = Read<uintptr_t>(plugin, Value(Field::DEPTH_OF_FIELD_EFFECT));
    const auto camera = Read<uintptr_t>(viewport, Value(Field::VIEWPORT_CAMERA));
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
  // Dg and CommandBuffer branches share this subcontext layout. Copy the owner
  // when each job is created, including jobs created by a running worker.
  Track([&] {
    Branch captured = {.node_address = Read<uintptr_t>(subcontext, Value(Field::BRANCH_NODE)), .target = Read<uintptr_t>(subcontext, Value(Field::BRANCH_TARGET)), .matrices = Read<std::array<float, 32>>(subcontext, Value(Field::BRANCH_MATRICES))};
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
  // Worker threads can serve several views. Start with no owner and accept the
  // recorded branch only if its node serial is still current and its matrices
  // match this job's.
  Track([&] {
    const std::unique_lock lock(runtime.mutex);
    if (runtime.failed.load(std::memory_order_relaxed)) return;
    workers.push_back({.object = object, .previous_owner = owner, .previous_node = node_serial});
    owner = {};
    node_serial = 0u;
    const auto node = Read<uintptr_t>(subcontext, Value(Field::BRANCH_NODE));
    const auto matrices = Read<std::array<float, 32>>(subcontext, Value(Field::BRANCH_MATRICES));
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
    const auto native_context = Read<uintptr_t>(context, Value(Field::CONTEXT_D3D_CONTEXT));
    const std::unique_lock lock(runtime.mutex);
    if (runtime.failed.load(std::memory_order_relaxed)) return;
    node_serial = 0u;
    if (const auto found = runtime.nodes.find(node); found != runtime.nodes.end()) {
      node_serial = found->second.serial;
      if (owner.generation == runtime.generation) {
        found->second.owner = owner;
      }
    }
    // The stored context can be a ReShade proxy. Observers record the real D3D11
    // context from their own hooks, so they can match the two.
    Emit({.kind = Kind::BEGIN_RECORDING, .owner = owner, .subject = node, .related = native_context, .node = node_serial});
  });
  recording_d3d_context = Read<uintptr_t>(context, Value(Field::CONTEXT_D3D_CONTEXT));
  begin_record(node, context);
  if (recording_began.load(std::memory_order_acquire) == nullptr) return;
  // The read lock lets a replacement wait for this call.
  const std::shared_lock lock(observer_mutex);
  if (const auto callback = recording_began.load(std::memory_order_relaxed); callback != nullptr) {
    callback();
  }
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
  recording_d3d_context = 0u;
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
    // The native call has applied the queued additions and removals. Walk the
    // live viewport list (linked through Field::VIEWPORT_NEXT), not the queues.
    auto view = Read<uintptr_t>(renderer, Value(Field::RENDER_VIEWPORTS));
    while (view != 0u && members.size() < MAX_ACTIVE_VIEWS) {
      auto [entry, new_view] = runtime.views.try_emplace(view, 0u);
      if (new_view) {
        entry->second = ++runtime.next_id;
      }
      members.push_back(entry->second);
      view = Read<uintptr_t>(view, Value(Field::VIEWPORT_NEXT));
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
      if (!engine::Verified(REQUIREMENT)) {
        installation_status = "Native ownership needs engine entries or structure fields that did not verify; see the log";
        return false;
      }
      const std::array hooked = {
          std::pair{engine::Entry::RENDER_BUILD_RENDERING, renodx::utils::detour::Function{&build_render, BuildRender}},
          std::pair{engine::Entry::DG_INIT_RENDER, renodx::utils::detour::Function{&init_render, InitRender}},
          std::pair{engine::Entry::DG_INIT_VIEWPORT, renodx::utils::detour::Function{&init_view, InitView}},
          std::pair{engine::Entry::DG_END_VIEWPORT, renodx::utils::detour::Function{&end_view, EndView}},
          std::pair{engine::Entry::DG_BRANCH, renodx::utils::detour::Function{&branch, BranchContext}},
          std::pair{engine::Entry::COMMAND_BUFFER_BRANCH, renodx::utils::detour::Function{&worker_branch, BranchWorker}},
          std::pair{engine::Entry::COMMAND_BUFFER_CONSTRUCTOR, renodx::utils::detour::Function{&worker_begin, WorkerBegin}},
          std::pair{engine::Entry::COMMAND_BUFFER_DESTRUCTOR, renodx::utils::detour::Function{&worker_end, WorkerEnd}},
          std::pair{engine::Entry::ALLOCATE_COMMAND_LIST_NODE, renodx::utils::detour::Function{&allocate_node, AllocateNode}},
          std::pair{engine::Entry::COMMAND_LIST_NODE_BEGIN_RECORD, renodx::utils::detour::Function{&begin_record, BeginRecord}},
          std::pair{engine::Entry::COMMAND_LIST_NODE_END_RECORD, renodx::utils::detour::Function{&end_record, EndRecord}},
          std::pair{engine::Entry::VIEWPORT_DESTRUCTOR, renodx::utils::detour::Function{&destroy_view, DestroyView}},
          std::pair{engine::Entry::RENDER_DESTRUCTOR, renodx::utils::detour::Function{&destroy_render, DestroyRender}},
          std::pair{engine::Entry::RENDER_UPDATE_VIEWPORT_LIST, renodx::utils::detour::Function{&update_views, UpdateViews}},
          std::pair{engine::Entry::DEPTH_OF_FIELD_MAIN_EXEC, renodx::utils::detour::Function{&dof_main, DofMainExec}},
      };
      get_main_view = reinterpret_cast<Accessor>(engine::Address(engine::Entry::GET_MAIN_VIEWPORT));
      // An engine job can still return through a detour during device teardown,
      // so pin this DLL for the life of the process.
      HMODULE module = nullptr;
      if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                             reinterpret_cast<LPCWSTR>(&BuildRender), &module)
          == FALSE) return false;
      hooks.reserve(hooked.size());
      for (const auto& [entry, function] : hooked) {
        *function.original = engine::Address(entry);
        hooks.push_back(function);
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
    shared.data->set_recording_began = +[](RecordingBegan expected, RecordingBegan replacement) {
      const std::unique_lock lock(observer_mutex);
      if (recording_began.load(std::memory_order_relaxed) != expected) return false;
      recording_began.store(replacement, std::memory_order_release);
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
    shared.data->read_recording = +[]() -> Recording {
      return {.d3d_context = recording_d3d_context, .command_buffer = workers.empty() ? 0u : workers.back().object};
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

}  // namespace mgsv::engine::views
#endif  // RENODX_GAMES_MGSV_ENGINE_VIEWS_HPP_
