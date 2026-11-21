// SPDX-License-Identifier: MIT
// Copyright and license notices: ../lighting/indirect/README.md#credits-and-license.

#ifndef RENODX_GAMES_MGSV_RENDERING_PRODUCER_RECORDS_HPP_
#define RENODX_GAMES_MGSV_RENDERING_PRODUCER_RECORDS_HPP_

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <iterator>
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

#include "../engine/views.hpp"
#include "../lighting/reflections/inputs.hpp"
#include "../reconstruction/camera_state.hpp"
#include "inputs.hpp"

// Records which depth, motion and camera sample each view's scene was drawn
// with, so lighting history reuses data only from the same scene. Records made
// on a deferred context wait with their command list and publish when that list
// is executed; records made on the immediate context publish at once.
namespace mgsv::lighting::temporal_inputs {

inline constexpr GUID DIFFUSE_SOURCE_GUID = {.Data1 = 0xa1465c19, .Data2 = 0x1276, .Data3 = 0x4697, .Data4 = {0x8a, 0xd0, 0x9c, 0x21, 0x35, 0xf7, 0xd8, 0x43}};
inline constexpr GUID PREPARED_SOURCE_GUID = {.Data1 = 0xe7302e42, .Data2 = 0x2f76, .Data3 = 0x40fb, .Data4 = {0x91, 0x25, 0xeb, 0x27, 0x50, 0x2c, 0x45, 0x66}};

inline constexpr GUID MATERIAL_PALETTE_GUID = {.Data1 = 0x5ab2941c, .Data2 = 0x55db, .Data3 = 0x4712, .Data4 = {0x83, 0xf4, 0x2c, 0xa8, 0xe1, 0x29, 0xe8, 0xe5}};
inline constexpr GUID MATERIAL_PALETTE_GENERATION_GUID = {.Data1 = 0x9ee98019, .Data2 = 0x58a5, .Data3 = 0x46c6, .Data4 = {0x91, 0x3a, 0xa2, 0x71, 0x18, 0xb5, 0x09, 0xe4}};

// The native scene's depth allocation and the camera sample it was drawn with.
// Jitter describes the producer, not the thread reading it.
struct Scene {
  Microsoft::WRL::ComPtr<ID3D11Resource> depth;
  std::array<float, 2> current_jitter_uv = {};
  uint64_t camera_publication = 0u;
  uint64_t camera_generation = 0u;
  uint32_t camera_sample = 0u;
  bool jitter_valid = false;
};

// One submitted camera/motion pair. Read() qualifies their depth, generation,
// and camera sample against the consumer's depth before history may use them.
struct Snapshot {
  engine::views::Owner owner;
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
  std::shared_ptr<const reflections::Inputs> reflection_inputs;
};

enum class ProducerKind : uint8_t {
  SCENE,
  MOTION,
  DIFFUSE_COPY,
  MATERIAL_PALETTE,
  REFLECTION_INPUTS,
};

struct MaterialPaletteBinding {
  Microsoft::WRL::ComPtr<ID3D11Resource> specular;
  // Mutable palettes use ordered snapshots; immutable palettes are retained.
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
};

struct Producer {
  engine::views::Owner owner;
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
  MaterialPaletteBinding material_palette;
  std::shared_ptr<const reflections::Inputs> reflection_inputs;
};

struct State {
  std::shared_mutex mutex;
  // Vectors preserve producer order within a context/list. Finished vectors are
  // immutable and shared so publication can outlive removal of their map entry.
  std::unordered_map<ID3D11DeviceContext*, std::vector<Producer>> recordings;
  std::unordered_map<ID3D11CommandList*, std::shared_ptr<const std::vector<Producer>>> lists;
  std::vector<Snapshot> submitted;
  reshade::api::device* device = nullptr;
  uint64_t next_submission = 0u;
};

inline State state;
inline std::atomic<bool> enabled = true;

// Copy, material and reflection records describe lighting inputs. They do not
// advance the scene/motion submission number that decides whether history can
// advance.
inline void Publish(std::span<const Producer> producers) {
  {
    const std::unique_lock lock(state.mutex);
    // Publish the camera and motion from one list as a coherent snapshot.
    // Readers must not observe a partially updated producer sequence.
    for (const auto& producer : producers) {
      if (producer.kind != ProducerKind::SCENE && producer.kind != ProducerKind::MOTION
          && producer.kind != ProducerKind::REFLECTION_INPUTS) continue;
      if (producer.owner.scene == 0u) continue;
      if (state.device != producer.device) {
        state.submitted.clear();
        state.device = producer.device;
      }
      auto entry = std::ranges::find_if(state.submitted, [&](const Snapshot& candidate) {
        return candidate.owner.scene == producer.owner.scene && candidate.owner.generation == producer.owner.generation;
      });
      if (entry == state.submitted.end()) {
        if (state.submitted.size() == reconstruction::camera_state::MAX_SCENE_PUBLICATIONS) {
          state.submitted.erase(state.submitted.begin());
        }
        state.submitted.push_back({.owner = producer.owner, .generation = producer.generation});
        entry = std::prev(state.submitted.end());
      }
      auto& submitted = *entry;
      if (submitted.generation != producer.generation) {
        submitted = {.owner = producer.owner, .generation = producer.generation};
      }
      if (producer.kind == ProducerKind::REFLECTION_INPUTS) {
        submitted.reflection_inputs = producer.reflection_inputs;
        continue;  // Reflection inputs do not advance the submission number.
      }
      if (producer.kind == ProducerKind::MOTION) {
        submitted.motion = producer.motion;
        submitted.motion_jitter_uv = producer.motion_jitter_uv;
        submitted.velocity_projection_jitter_scale = producer.velocity_projection_jitter_scale;
        submitted.camera_publication = producer.camera_publication;
        submitted.camera_generation = producer.camera_generation;
        submitted.camera_sample = producer.camera_sample;
        submitted.jitter_valid = producer.jitter_valid;
      } else {
        submitted.scene = producer.scene;
      }
      submitted.submission = ++state.next_submission;
    }
  }
  // Readers use these associations only after this command list has run.
  // Publish them in recorded order, outside the lock: replacing a private-data
  // interface can release a native resource and reenter destruction callbacks.
  for (const auto& producer : producers) {
    if (producer.kind != ProducerKind::DIFFUSE_COPY && producer.kind != ProducerKind::MATERIAL_PALETTE) continue;
    const auto selected = inputs::ReadSelection(producer.device);
    if (selected.device == nullptr || selected.generation != producer.generation) continue;
    if (producer.kind == ProducerKind::DIFFUSE_COPY) {
      producer.copy_destination->SetPrivateDataInterface(DIFFUSE_SOURCE_GUID, producer.copy_source.Get());
      // Publish preparation beside this exact copy, including null on fallback.
      // A cached target or recent Present cannot prove that its draw executed.
      producer.copy_destination->SetPrivateDataInterface(PREPARED_SOURCE_GUID, producer.prepared_source.Get());
    } else if (producer.material_palette.specular != nullptr) {
      // Submission is serialized, so the palette and its generation are published together.
      // A worker recording another light cannot pick up this palette.
      const uint64_t invalid_generation = 0u;
      auto* destination = producer.material_palette.specular.Get();
      destination->SetPrivateData(MATERIAL_PALETTE_GENERATION_GUID, sizeof(invalid_generation), &invalid_generation);
      if (SUCCEEDED(destination->SetPrivateDataInterface(MATERIAL_PALETTE_GUID, producer.material_palette.view.Get()))) {
        destination->SetPrivateData(MATERIAL_PALETTE_GENERATION_GUID, sizeof(producer.generation), &producer.generation);
      }
    }
  }
}

// Only known game draws are recorded here. A record made on a deferred context
// takes effect when its command list is executed on the immediate context.
inline void Record(reshade::api::command_list* cmd_list, Producer producer) {
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  if (context == nullptr) return;
  producer.owner = engine::views::GetScope().owner;
  if (context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED) {
    const std::unique_lock lock(state.mutex);
    state.recordings[context].push_back(std::move(producer));
  } else {
    Publish({&producer, 1u});
  }
}

inline void CaptureScene(reshade::api::command_list* cmd_list) {
  // Pair the native scene producer's depth (t2) with this scene's camera
  // sample. Its b2 range qualifies the producer as the 480-byte scene layout.
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
    const auto camera = reconstruction::camera_state::GetForCapture();
    scene.current_jitter_uv = {camera.jitter_uv_x, camera.jitter_uv_y};
    scene.camera_publication = camera.publication_sequence;
    scene.camera_generation = camera.reset_generation;
    scene.camera_sample = camera.sample_index;
    scene.jitter_valid = camera.valid && camera.camera_matrix_valid;
  } else {
    scene.jitter_valid = true;
  }
  UINT first_constant = 0u;
  UINT constant_count = 0u;
  context->PSGetShaderResources(2u, 1u, &depth);
  if (SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&context1)))) {
    context1->PSGetConstantBuffers1(2u, 1u, &original, &first_constant, &constant_count);
  } else {
    context->PSGetConstantBuffers(2u, 1u, &original);
  }
  if (original != nullptr && depth != nullptr) {
    D3D11_BUFFER_DESC description = {};
    original->GetDesc(&description);
    if (context1 == nullptr) constant_count = description.ByteWidth / 16u;
    if (constant_count >= 30u && (static_cast<uint64_t>(first_constant) * 16u) + 480u <= description.ByteWidth) {
      depth->GetResource(&scene.depth);
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
  const auto owner = engine::views::GetScope().owner;
  if (state.device != device || owner.scene == 0u) return {};
  const auto entry = std::ranges::find_if(state.submitted, [&](const Snapshot& candidate) {
    return candidate.owner.scene == owner.scene && candidate.owner.generation == owner.generation;
  });
  if (entry == state.submitted.end() || entry->generation != selected.generation) return {};
  Snapshot result = *entry;
  result.current_jitter_uv = result.scene.current_jitter_uv;
  Microsoft::WRL::ComPtr<ID3D11Resource> motion_depth;
  if (result.motion.depth != nullptr) result.motion.depth->GetResource(&motion_depth);
  result.valid = result.scene.depth != nullptr && result.scene.depth.Get() == depth.Get()
                 && result.motion.generation == selected.generation
                 && result.motion.velocity != nullptr && result.motion.object_velocity != nullptr
                 && motion_depth.Get() == depth.Get() && result.jitter_valid && result.scene.jitter_valid
                 && result.camera_publication == result.scene.camera_publication
                 && result.camera_generation == result.scene.camera_generation
                 && result.camera_sample == result.scene.camera_sample;
  return result;
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
  state.submitted.clear();
  state.device = nullptr;
}

}  // namespace mgsv::lighting::temporal_inputs

#endif  // RENODX_GAMES_MGSV_RENDERING_PRODUCER_RECORDS_HPP_
