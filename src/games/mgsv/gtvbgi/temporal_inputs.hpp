// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

#ifndef SRC_GAMES_MGSV_GTVBGI_TEMPORAL_INPUTS_HPP_
#define SRC_GAMES_MGSV_GTVBGI_TEMPORAL_INPUTS_HPP_

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include <d3d11_1.h>
#include <wrl/client.h>

#include <include/reshade.hpp>

#include "../inputs.hpp"
#include "../reconstruction/camera_state.hpp"

namespace mgsv::lighting::temporal_inputs {

inline constexpr GUID DIFFUSE_SOURCE_GUID = {.Data1 = 0xa1465c19, .Data2 = 0x1276, .Data3 = 0x4697, .Data4 = {0x8a, 0xd0, 0x9c, 0x21, 0x35, 0xf7, 0xd8, 0x43}};
inline constexpr GUID PREPARED_SOURCE_GUID = {.Data1 = 0xe7302e42, .Data2 = 0x2f76, .Data3 = 0x40fb, .Data4 = {0x91, 0x25, 0xeb, 0x27, 0x50, 0x2c, 0x45, 0x66}};

// Own the native scene-buffer copy and its original D3D11.1 binding range.
// Camera identity and jitter describe the producer, not the thread reading it.
struct Scene {
  Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
  Microsoft::WRL::ComPtr<ID3D11Resource> depth;
  UINT first_constant = 0u;
  UINT constant_count = 0u;
  uintptr_t depth_identity = 0u;
  std::array<float, 2> current_jitter_uv = {};
  uint64_t camera_publication = 0u;
  uint64_t camera_generation = 0u;
  uint32_t camera_sample = 0u;
  uintptr_t viewport_identity = 0u;
  uintptr_t camera_identity = 0u;
  bool jitter_valid = false;
};

// One submitted camera/motion pair. Read() qualifies their depth, generation,
// and camera sample against the scatter before history may use these pixels.
struct Snapshot {
  inputs::Motion motion;
  Scene scene;
  uint64_t generation = 0u;
  uint64_t submission = 0u;
  std::array<float, 2> motion_jitter_uv = {};
  std::array<float, 2> current_jitter_uv = {};
  float velocity_projection_jitter_scale = 0.f;
  uint64_t camera_publication = 0u;
  uint64_t camera_generation = 0u;
  uint32_t camera_sample = 0u;
  bool jitter_valid = false;
  bool valid = false;
};

enum class ProducerKind : uint8_t {
  SCENE,
  MOTION,
  DIFFUSE_COPY,
};

struct Producer {
  reshade::api::device* device = nullptr;
  uint64_t generation = 0u;
  inputs::Motion motion;
  Scene scene;
  std::array<float, 2> motion_jitter_uv = {};
  float velocity_projection_jitter_scale = 0.f;
  uint64_t camera_publication = 0u;
  uint64_t camera_generation = 0u;
  uint32_t camera_sample = 0u;
  bool jitter_valid = false;
  ProducerKind kind = ProducerKind::SCENE;
  Microsoft::WRL::ComPtr<ID3D11Resource> copy_source;
  Microsoft::WRL::ComPtr<ID3D11Resource> copy_destination;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> prepared_source;
};

struct State {
  std::shared_mutex mutex;
  // Vectors preserve producer order within a context/list. Finished vectors are
  // immutable and shared so publication can outlive removal of their map entry.
  std::unordered_map<ID3D11DeviceContext*, std::vector<Producer>> recordings;
  std::unordered_map<ID3D11CommandList*, std::shared_ptr<const std::vector<Producer>>> lists;
  Snapshot submitted;
  reshade::api::device* device = nullptr;
  uint64_t next_submission = 0u;
};

inline State state;
inline std::atomic<bool> enabled = true;

// Copy edges qualify one lighting application. They do not advance the camera
// or motion submission number used to decide whether history can advance.
inline void Publish(std::span<const Producer> producers) {
  {
    const std::unique_lock lock(state.mutex);
    // Publish the camera and motion from one list as a coherent snapshot.
    // Readers must not observe a partially updated producer sequence.
    for (const auto& producer : producers) {
      if (producer.kind == ProducerKind::DIFFUSE_COPY) continue;
      if (state.device != producer.device || state.submitted.generation != producer.generation) {
        state.submitted = {};
        state.device = producer.device;
        state.submitted.generation = producer.generation;
      }
      if (producer.kind == ProducerKind::MOTION) {
        state.submitted.motion = producer.motion;
        state.submitted.motion_jitter_uv = producer.motion_jitter_uv;
        state.submitted.velocity_projection_jitter_scale = producer.velocity_projection_jitter_scale;
        state.submitted.camera_publication = producer.camera_publication;
        state.submitted.camera_generation = producer.camera_generation;
        state.submitted.camera_sample = producer.camera_sample;
        state.submitted.jitter_valid = producer.jitter_valid;
      } else {
        state.submitted.scene = producer.scene;
      }
      state.submitted.submission = ++state.next_submission;
    }
  }
  // Copy metadata is consumed after this prefix. Publish its final associations
  // in recorded order, outside the lock: replacing an interface can release
  // a native resource and reenter destruction callbacks.
  for (const auto& producer : producers) {
    if (producer.kind != ProducerKind::DIFFUSE_COPY) continue;
    const auto selected = inputs::ReadSelection(producer.device);
    if (selected.device != nullptr && selected.generation == producer.generation) {
      producer.copy_destination->SetPrivateDataInterface(DIFFUSE_SOURCE_GUID, producer.copy_source.Get());
      // Publish preparation beside this exact copy, including null on fallback.
      // A cached target or recent Present cannot prove that its draw executed.
      producer.copy_destination->SetPrivateDataInterface(PREPARED_SOURCE_GUID, producer.prepared_source.Get());
    }
  }
}

// Only known native producers enter this ledger. Recorded observations become
// eligible after their containing command list reaches immediate submission.
inline void Record(reshade::api::command_list* cmd_list, Producer producer) {
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (context == nullptr) return;
  if (context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED) {
    const std::unique_lock lock(state.mutex);
    state.recordings[context].push_back(std::move(producer));
  } else {
    Publish({&producer, 1u});
  }
}

inline void CaptureScene(reshade::api::command_list* cmd_list) {
  // Capture the supported native scene producer's b2 and t2 together. Keeping
  // the buffer object alive would not protect its contents from later writes.
  if (cmd_list == nullptr || !enabled.load(std::memory_order_acquire)) return;
  const auto selected = inputs::ReadSelection(cmd_list->get_device());
  if (selected.device == nullptr) return;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (context == nullptr) return;
  Microsoft::WRL::ComPtr<ID3D11Buffer> original;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context1;
  Scene scene;
  if (reconstruction::state::IsEnabled()) {
    const auto camera = reconstruction::camera_state::Get();
    scene.current_jitter_uv = {camera.jitter_uv_x, camera.jitter_uv_y};
    scene.camera_publication = camera.publication_sequence;
    scene.camera_generation = camera.reset_generation;
    scene.camera_sample = camera.sample_index;
    scene.viewport_identity = camera.viewport_identity;
    scene.camera_identity = camera.camera_identity;
    scene.jitter_valid = camera.valid && camera.camera_matrix_valid;
  } else {
    scene.jitter_valid = true;
  }
  context->PSGetShaderResources(2u, 1u, &depth);
  if (SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&context1)))) {
    context1->PSGetConstantBuffers1(2u, 1u, &original, &scene.first_constant, &scene.constant_count);
  } else {
    context->PSGetConstantBuffers(2u, 1u, &original);
  }
  if (original != nullptr && depth != nullptr) {
    D3D11_BUFFER_DESC description = {};
    original->GetDesc(&description);
    if (context1 == nullptr) scene.constant_count = description.ByteWidth / 16u;
    const uint64_t first_byte = static_cast<uint64_t>(scene.first_constant) * 16u;
    if (scene.constant_count >= 30u && first_byte + 480u <= description.ByteWidth) {
      // Copy the allocation, not just the reflected 480 bytes. D3D11 constant
      // buffers cannot be partially copied; keep the original binding range.
      description.Usage = D3D11_USAGE_DEFAULT;
      description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
      description.CPUAccessFlags = 0u;
      description.MiscFlags = 0u;
      description.StructureByteStride = 0u;
      Microsoft::WRL::ComPtr<ID3D11Device> device;
      original->GetDevice(&device);
      if (device != nullptr && SUCCEEDED(device->CreateBuffer(&description, nullptr, &scene.buffer))) {
        context->CopyResource(scene.buffer.Get(), original.Get());
        depth->GetResource(&scene.depth);
        scene.depth_identity = reinterpret_cast<uintptr_t>(scene.depth.Get());
      }
    }
  }
  Record(cmd_list, {
                       .device = selected.device,
                       .generation = selected.generation,
                       .scene = std::move(scene),
                   });
}

// Finishing a recording is not submission. Keep the ordered records attached
// to that native list until Submit publishes them or destruction retires them.
inline void Finish(ID3D11DeviceContext* context, ID3D11CommandList* list) {
  const std::unique_lock lock(state.mutex);
  if (const auto recording = state.recordings.find(context); recording != state.recordings.end()) {
    state.lists[list] = std::make_shared<const std::vector<Producer>>(std::move(recording->second));
    state.recordings.erase(recording);
  }
}

inline void Submit(ID3D11CommandList* list) {
  std::shared_ptr<const std::vector<Producer>> producers;
  {
    const std::shared_lock lock(state.mutex);
    const auto recorded = state.lists.find(list);
    if (recorded == state.lists.end()) return;
    producers = recorded->second;
  }
  // Retain immutable records across publication and native destruction callbacks.
  Publish(*producers);
}

inline Snapshot Read(reshade::api::device* device, reshade::api::resource_view scatter_depth) {
  // Scene and motion must name the same depth allocation and jitter sample.
  // A recent Present or two matching dimensions cannot establish that pairing.
  const auto selected = inputs::ReadSelection(device);
  if (selected.device == nullptr || scatter_depth.handle == 0u) return {};
  auto* depth_view = reinterpret_cast<ID3D11ShaderResourceView*>(scatter_depth.handle);  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11Resource> depth;
  depth_view->GetResource(&depth);
  const std::shared_lock lock(state.mutex);
  if (state.device != device || state.submitted.generation != selected.generation) return {};
  Snapshot result = state.submitted;
  result.current_jitter_uv = result.scene.current_jitter_uv;
  Microsoft::WRL::ComPtr<ID3D11Resource> motion_depth;
  if (result.motion.depth != nullptr) result.motion.depth->GetResource(&motion_depth);
  result.valid = result.scene.buffer != nullptr && result.scene.depth.Get() == depth.Get()
                 && result.motion.generation == selected.generation
                 && result.motion.velocity != nullptr && result.motion.object_velocity != nullptr
                 && motion_depth.Get() == depth.Get() && result.jitter_valid && result.scene.jitter_valid
                 && result.camera_publication == result.scene.camera_publication
                 && result.camera_generation == result.scene.camera_generation
                 && result.camera_sample == result.scene.camera_sample;
  return result;
}

inline void BeginFrame(reshade::api::device* device) {
  const std::unique_lock lock(state.mutex);
  if (state.device == device) state.submitted = {};
}

inline void Forget(ID3D11CommandList* list) {
  const std::unique_lock lock(state.mutex);
  state.lists.erase(list);
}

inline void Destroy(reshade::api::command_list* cmd_list) {
  if (cmd_list == nullptr) return;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  auto* native = reinterpret_cast<IUnknown*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (native == nullptr) return;
  const std::unique_lock lock(state.mutex);
  if (SUCCEEDED(native->QueryInterface(IID_PPV_ARGS(&context)))) {
    state.recordings.erase(context.Get());
  } else {
    state.lists.erase(reinterpret_cast<ID3D11CommandList*>(native));  // NOLINT(performance-no-int-to-ptr)
  }
}

inline void Reset() {
  const std::unique_lock lock(state.mutex);
  state.recordings.clear();
  state.lists.clear();
  state.submitted = {};
  state.device = nullptr;
}

}  // namespace mgsv::lighting::temporal_inputs

#endif  // SRC_GAMES_MGSV_GTVBGI_TEMPORAL_INPUTS_HPP_
