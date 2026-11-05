/*
 * Copyright (C) 2026 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#ifndef RENODX_GAMES_MGSV_RECONSTRUCTION_MIP_BIAS_HPP_
#define RENODX_GAMES_MGSV_RECONSTRUCTION_MIP_BIAS_HPP_

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include <include/reshade.hpp>

#include "../../../utils/command_action.hpp"
#include "./logging.hpp"
#include "./resolution.hpp"
#include "./state.hpp"

namespace mgsv::reconstruction::mip_bias {

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
};

inline void Apply(reshade::api::command_list* command_list, CommandListData* data) {
  float bias = 0.f;
  const auto mode = state::GetTemporalMode();
  if ((mode == state::TemporalMode::AMD_FSR3 || mode == state::TemporalMode::NVIDIA_DLSS)
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
    if (bias == 0.f || samplers[index] == nullptr) continue;
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
    reshade::register_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
    reshade::register_event<reshade::addon_event::init_command_list>(OnInitCommandList);
    reshade::register_event<reshade::addon_event::reset_command_list>(OnResetCommandList);
    reshade::register_event<reshade::addon_event::destroy_command_list>(OnDestroyCommandList);
    reshade::register_event<reshade::addon_event::push_descriptors>(OnPushDescriptors);
    renodx::utils::command_action::Register(DRAW_CALLBACK, {.command_types = renodx::utils::command_action::COMMAND_TYPE_DIRECT_DRAW | renodx::utils::command_action::COMMAND_TYPE_INDIRECT});
  } else if (reason == DLL_PROCESS_DETACH) {
    renodx::utils::command_action::Unregister(DRAW_CALLBACK);
    reshade::unregister_event<reshade::addon_event::init_device>(OnInitDevice);
    reshade::unregister_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
    reshade::unregister_event<reshade::addon_event::init_command_list>(OnInitCommandList);
    reshade::unregister_event<reshade::addon_event::reset_command_list>(OnResetCommandList);
    reshade::unregister_event<reshade::addon_event::destroy_command_list>(OnDestroyCommandList);
    reshade::unregister_event<reshade::addon_event::push_descriptors>(OnPushDescriptors);
  }
}

}  // namespace mgsv::reconstruction::mip_bias

#endif  // RENODX_GAMES_MGSV_RECONSTRUCTION_MIP_BIAS_HPP_
