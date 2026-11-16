/*
 * Copyright (C) 2026 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#ifndef RENODX_GAMES_MGSV_RECONSTRUCTION_MIP_BIAS_HPP_
#define RENODX_GAMES_MGSV_RECONSTRUCTION_MIP_BIAS_HPP_

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <type_traits>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include <include/reshade.hpp>
#include "../../../../external/reshade/deps/d3d12/include/directx/D3D12TokenizedProgramFormat.hpp"

#include "../../../utils/command_action.hpp"
#include "logging.hpp"
#include "resolution.hpp"
#include "state.hpp"

namespace mgsv::reconstruction::mip_bias {

inline constexpr GUID AUTOMATIC_MIP_SAMPLERS_GUID = {
    .Data1 = 0x7325E037, .Data2 = 0x0645, .Data3 = 0x4A7A, .Data4 = {0xAA, 0x72, 0x34, 0x37, 0x83, 0xD2, 0x5C, 0xBF}};
inline constexpr uint32_t DXBC_FOURCC = 0x43425844u;
inline constexpr uint32_t SHDR_FOURCC = 0x52444853u;
inline constexpr uint32_t SHEX_FOURCC = 0x58454853u;

struct CachedSampler {
  D3D11_SAMPLER_DESC description;
  Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
};

struct __declspec(uuid("F67CDEAE-A544-4A20-AD71-00DDA37694AB")) DeviceData {
  std::shared_mutex mutex;
  // Key by description rather than a borrowed sampler pointer that can be reused.
  std::vector<CachedSampler> samplers;
  float bias = 0.f;
};

struct __declspec(uuid("602A0ECA-31F0-4BD9-90DD-77CB84D3348D")) CommandListData {
  std::array<Microsoft::WRL::ComPtr<ID3D11SamplerState>, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT> native;
  std::array<Microsoft::WRL::ComPtr<ID3D11SamplerState>, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT> biased;
  float bias = 0.f;
  bool dirty = false;
  uint16_t automatic_mip_samplers = 0u;
};

inline void OnInitPipeline(reshade::api::device* device, reshade::api::pipeline_layout /*layout*/,
                           uint32_t count, const reshade::api::pipeline_subobject* objects,
                           reshade::api::pipeline pipeline) {
  if (device->get_api() != reshade::api::device_api::d3d11 || pipeline.handle == 0u || count != 1u || objects == nullptr
      || objects[0].type != reshade::api::pipeline_subobject_type::pixel_shader || objects[0].data == nullptr) return;
  const auto& shader = *static_cast<const reshade::api::shader_desc*>(objects[0].data);
  // Legacy DXBC debug chunks can end between words. Only the instruction chunk
  // must be word-aligned; requiring that of the whole file rejects native code.
  if (shader.code == nullptr || shader.code_size < 32u) return;
  const size_t words = shader.code_size / sizeof(uint32_t);
  const auto read_word = [&](size_t index) {
    uint32_t word = 0u;
    std::memcpy(&word, static_cast<const uint8_t*>(shader.code) + (index * sizeof(word)), sizeof(word));
    return word;
  };
  const auto chunk_count = read_word(7u);
  if (read_word(0u) != DXBC_FOURCC || read_word(6u) != shader.code_size || chunk_count > words - 8u) return;

  // Inspect the actual shader once, without shader hashes or pass-name lists.
  // A sampler used for an explicit level must retain its native bias: those
  // levels can encode roughness, blur width or another shader-defined quantity.
  for (size_t chunk = 0u; chunk < chunk_count; ++chunk) {
    const auto byte_offset = read_word(8u + chunk);
    if ((byte_offset % sizeof(uint32_t)) != 0u || byte_offset / sizeof(uint32_t) > words - 2u) return;
    const size_t start = byte_offset / sizeof(uint32_t);
    if (read_word(start) != SHDR_FOURCC && read_word(start) != SHEX_FOURCC) continue;
    const auto byte_size = read_word(start + 1u);
    if ((byte_size % sizeof(uint32_t)) != 0u || byte_size < 8u || byte_size / sizeof(uint32_t) > words - start - 2u) return;
    const auto program_words = read_word(start + 3u);
    if (program_words < 2u || program_words > byte_size / sizeof(uint32_t)) return;
    const size_t program_end = start + 2u + program_words;
    uint16_t automatic_samplers = 0u;
    uint16_t explicit_samplers = 0u;
    for (size_t instruction = start + 4u; instruction < program_end;) {
      const auto token = read_word(instruction);
      const auto opcode = DECODE_D3D10_SB_OPCODE_TYPE(token);
      size_t length = DECODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(token);
      if (opcode == D3D10_SB_OPCODE_CUSTOMDATA) {
        if (instruction + 1u >= program_end) return;
        length = read_word(instruction + 1u);
      }
      if (length == 0u || length > program_end - instruction) return;
      const size_t end = instruction + length;
      switch (opcode) {
        case D3D10_SB_OPCODE_SAMPLE:
        case D3D10_SB_OPCODE_SAMPLE_B:
        case D3D10_SB_OPCODE_SAMPLE_D:
        case D3D10_SB_OPCODE_SAMPLE_L:
        case D3D10_SB_OPCODE_SAMPLE_C:
        case D3D10_SB_OPCODE_SAMPLE_C_LZ: {
          size_t cursor = instruction + 1u;
          for (auto extension = token; DECODE_IS_D3D10_SB_OPCODE_EXTENDED(extension) != 0u;) {
            if (cursor >= end) return;
            extension = read_word(cursor++);
          }
          struct Operand {
            D3D10_SB_OPERAND_TYPE type;
            uint32_t index = 0u;
            bool direct = false;
          };
          const auto read_operand = [&]() -> std::optional<Operand> {
            if (cursor >= end) return std::nullopt;
            const auto operand = read_word(cursor++);
            Operand result = {.type = DECODE_D3D10_SB_OPERAND_TYPE(operand)};
            for (auto extension = operand; DECODE_IS_D3D10_SB_OPERAND_EXTENDED(extension) != 0u;) {
              if (cursor >= end) return std::nullopt;
              extension = read_word(cursor++);
            }
            const auto dimensions = DECODE_D3D10_SB_OPERAND_INDEX_DIMENSION(operand);
            for (uint32_t index = 0u; index < dimensions; ++index) {
              // Each index has three representation bits, starting at bit 22.
              // Read all three so immediate64-plus-relative is rejected too.
              const auto representation = (operand >> (22u + (3u * index))) & 7u;
              // Unsupported relative bindings remain unbiased. Do not guess a
              // register from the words belonging to a nested operand.
              if (representation != D3D10_SB_OPERAND_INDEX_IMMEDIATE32 || cursor >= end) return std::nullopt;
              const auto value = read_word(cursor++);
              if (dimensions == D3D10_SB_OPERAND_INDEX_1D) {
                result.index = value;
                result.direct = true;
              }
            }
            if (result.type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32 || result.type == D3D10_SB_OPERAND_TYPE_IMMEDIATE64) {
              const auto components = DECODE_D3D10_SB_OPERAND_NUM_COMPONENTS(operand);
              if (components != D3D10_SB_OPERAND_1_COMPONENT && components != D3D10_SB_OPERAND_4_COMPONENT) return std::nullopt;
              const size_t literals = (components == D3D10_SB_OPERAND_1_COMPONENT ? 1u : 4u)
                                      * (result.type == D3D10_SB_OPERAND_TYPE_IMMEDIATE64 ? 2u : 1u);
              if (literals > end - cursor) return std::nullopt;
              cursor += literals;
            }
            return result;
          };
          // Destination, coordinates and texture precede the sampler in these
          // instructions. Decode their lengths rather than scanning literal data.
          for (uint32_t operand = 0u; operand < 3u; ++operand) {
            if (!read_operand()) return;
          }
          const auto sampler = read_operand();
          if (!sampler || sampler->type != D3D10_SB_OPERAND_TYPE_SAMPLER || !sampler->direct || sampler->index >= 16u) return;
          const auto bit = static_cast<uint16_t>(1u << sampler->index);
          if (opcode == D3D10_SB_OPCODE_SAMPLE || opcode == D3D10_SB_OPCODE_SAMPLE_B || opcode == D3D10_SB_OPCODE_SAMPLE_D) {
            automatic_samplers |= bit;
          } else {
            explicit_samplers |= bit;
          }
          break;
        }
        default: break;
      }
      instruction = end;
    }
    // A shared sampler cannot apply two different biases in the same shader.
    // Preserve explicit levels when both sampling styles use that sampler.
    const auto mask = static_cast<uint16_t>(automatic_samplers & ~explicit_samplers);
    auto* object = reinterpret_cast<ID3D11DeviceChild*>(pipeline.handle);  // NOLINT(performance-no-int-to-ptr)
    object->SetPrivateData(AUTOMATIC_MIP_SAMPLERS_GUID, sizeof(mask), &mask);
    return;
  }
}

inline void OnBindPipeline(reshade::api::command_list* command_list, reshade::api::pipeline_stage stages,
                           reshade::api::pipeline pipeline) {
  if ((stages & reshade::api::pipeline_stage::pixel_shader) == reshade::api::pipeline_stage{}) return;
  auto* data = command_list->get_private_data<CommandListData>();
  if (data == nullptr) return;
  uint16_t mask = 0u;
  if (stages == reshade::api::pipeline_stage::pixel_shader && pipeline.handle != 0u) {
    auto* object = reinterpret_cast<ID3D11DeviceChild*>(pipeline.handle);  // NOLINT(performance-no-int-to-ptr)
    UINT bytes = sizeof(mask);
    if (FAILED(object->GetPrivateData(AUTOMATIC_MIP_SAMPLERS_GUID, &bytes, &mask)) || bytes != sizeof(mask)) {
      mask = 0u;
    }
  }
  if (mask != data->automatic_mip_samplers) {
    data->automatic_mip_samplers = mask;
    data->dirty = true;
  }
}

inline void Apply(reshade::api::command_list* command_list, CommandListData* data) {
  float bias = 0.f;
  const auto mode = state::GetTemporalMode();
  if ((mode == state::TemporalMode::AMD_FSR || mode == state::TemporalMode::NVIDIA_DLSS)
      && !resolution::failed.load(std::memory_order_acquire)) {
    bias = resolution::mip_bias.load(std::memory_order_acquire);
  }
  if (!data->dirty && bias == data->bias) return;
  if (bias == 0.f && data->bias == 0.f) {
    data->dirty = false;
    return;
  }
  auto* device = command_list->get_device();
  auto* cache = device->get_private_data<DeviceData>();
  if (cache == nullptr) return;
  std::vector<CachedSampler> retired;
  {
    const std::unique_lock lock(cache->mutex);
    if (cache->bias != bias) {
      retired.swap(cache->samplers);
      cache->bias = bias;
    }
  }
  // Bound contexts and recorded command lists keep their own sampler references.
  // Drop old cache entries outside the lock, without disturbing that work.
  if (data->bias != bias) {
    for (auto& sampler : data->biased) {
      sampler.Reset();
    }
  }
  std::array<ID3D11SamplerState*, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT> samplers = {};
  for (size_t index = 0u; index < samplers.size(); ++index) {
    samplers[index] = data->native[index].Get();
    if (bias == 0.f || samplers[index] == nullptr || (data->automatic_mip_samplers & (1u << index)) == 0u) continue;
    if (data->biased[index] == nullptr) {
      D3D11_SAMPLER_DESC description = {};
      samplers[index]->GetDesc(&description);
      {
        const std::shared_lock lock(cache->mutex);
        for (const auto& entry : cache->samplers) {
          if (cache->bias == bias && std::memcmp(&entry.description, &description, sizeof(description)) == 0) {
            data->biased[index] = entry.sampler;
            break;
          }
        }
      }
      if (data->biased[index] == nullptr) {
        auto adjusted = description;
        adjusted.MipLODBias = std::clamp(description.MipLODBias + bias, D3D11_MIP_LOD_BIAS_MIN, D3D11_MIP_LOD_BIAS_MAX);
        auto* native_device = reinterpret_cast<ID3D11Device*>(device->get_native());  // NOLINT(performance-no-int-to-ptr)
        if (SUCCEEDED(native_device->CreateSamplerState(&adjusted, &data->biased[index]))) {
          const std::unique_lock lock(cache->mutex);
          if (cache->bias == bias && std::ranges::none_of(cache->samplers, [&](const auto& entry) {
                return std::memcmp(&entry.description, &description, sizeof(description)) == 0;
              })) {
            cache->samplers.push_back({.description = description, .sampler = data->biased[index]});
          }
        } else {
          data->biased[index] = data->native[index];
          logging::Warn("could not create mip-biased sampler; retaining native sampler");
        }
      }
    }
    samplers[index] = data->biased[index].Get();
  }
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(command_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  // Native calls avoid reporting our replacements as new game sampler bindings.
  context->PSSetSamplers(0u, static_cast<UINT>(samplers.size()), samplers.data());
  data->bias = bias;
  data->dirty = false;
}

inline void OnPushDescriptors(reshade::api::command_list* command_list, reshade::api::shader_stage stages,
                              reshade::api::pipeline_layout /*layout*/, uint32_t /*parameter*/,
                              const reshade::api::descriptor_table_update& update) {
  if ((static_cast<uint32_t>(stages) & static_cast<uint32_t>(reshade::api::shader_stage::pixel)) == 0u
      || update.type != reshade::api::descriptor_type::sampler) return;
  auto* data = command_list->get_private_data<CommandListData>();
  if (data == nullptr) return;
  const auto* samplers = static_cast<const reshade::api::sampler*>(update.descriptors);
  for (uint32_t index = 0u; index < update.count && update.binding + index < data->native.size(); ++index) {
    const auto slot = update.binding + index;
    auto* sampler = reinterpret_cast<ID3D11SamplerState*>(samplers[index].handle);  // NOLINT(performance-no-int-to-ptr)
    if (data->native[slot].Get() != sampler) {
      data->native[slot] = sampler;
      data->biased[slot].Reset();
    }
  }
  data->dirty = true;
  Apply(command_list, data);
}

inline constexpr auto DRAW_CALLBACK = [](auto& command) {
  if (!command.IsDispatch()) {
    if (auto* data = command.cmd_list->template get_private_data<CommandListData>(); data != nullptr) {
      Apply(command.cmd_list, data);
    }
  }
  return renodx::utils::command_action::CallbackResult<std::remove_reference_t<decltype(command)>>{};
};

inline void OnInitDevice(reshade::api::device* device) {
  if (device->get_api() == reshade::api::device_api::d3d11) {
    device->create_private_data<DeviceData>();
  }
}

inline void OnDestroyDevice(reshade::api::device* device) {
  if (device->get_api() == reshade::api::device_api::d3d11) {
    device->destroy_private_data<DeviceData>();
  }
}

inline void OnInitCommandList(reshade::api::command_list* command_list) {
  if (command_list->get_device()->get_api() == reshade::api::device_api::d3d11) {
    command_list->create_private_data<CommandListData>();
  }
}

inline void OnResetCommandList(reshade::api::command_list* command_list) {
  if (auto* data = command_list->get_private_data<CommandListData>(); data != nullptr) {
    *data = {};
  }
}

inline void OnDestroyCommandList(reshade::api::command_list* command_list) {
  if (command_list->get_device()->get_api() == reshade::api::device_api::d3d11) {
    command_list->destroy_private_data<CommandListData>();
  }
}

inline void Use(DWORD reason) {
  if (reason == DLL_PROCESS_ATTACH) {
    reshade::register_event<reshade::addon_event::init_device>(OnInitDevice);
    reshade::register_event<reshade::addon_event::init_pipeline>(OnInitPipeline);
    reshade::register_event<reshade::addon_event::bind_pipeline>(OnBindPipeline);
    reshade::register_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
    reshade::register_event<reshade::addon_event::init_command_list>(OnInitCommandList);
    reshade::register_event<reshade::addon_event::reset_command_list>(OnResetCommandList);
    reshade::register_event<reshade::addon_event::destroy_command_list>(OnDestroyCommandList);
    reshade::register_event<reshade::addon_event::push_descriptors>(OnPushDescriptors);
    renodx::utils::command_action::Register(DRAW_CALLBACK, {.command_types = renodx::utils::command_action::COMMAND_TYPE_DIRECT_DRAW | renodx::utils::command_action::COMMAND_TYPE_INDIRECT});
  } else if (reason == DLL_PROCESS_DETACH) {
    renodx::utils::command_action::Unregister(DRAW_CALLBACK);
    reshade::unregister_event<reshade::addon_event::init_device>(OnInitDevice);
    reshade::unregister_event<reshade::addon_event::init_pipeline>(OnInitPipeline);
    reshade::unregister_event<reshade::addon_event::bind_pipeline>(OnBindPipeline);
    reshade::unregister_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
    reshade::unregister_event<reshade::addon_event::init_command_list>(OnInitCommandList);
    reshade::unregister_event<reshade::addon_event::reset_command_list>(OnResetCommandList);
    reshade::unregister_event<reshade::addon_event::destroy_command_list>(OnDestroyCommandList);
    reshade::unregister_event<reshade::addon_event::push_descriptors>(OnPushDescriptors);
  }
}

}  // namespace mgsv::reconstruction::mip_bias

#endif  // RENODX_GAMES_MGSV_RECONSTRUCTION_MIP_BIAS_HPP_
