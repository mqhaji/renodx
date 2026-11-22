// SPDX-License-Identifier: MIT
#ifndef RENODX_GAMES_MGSV_RENDERING_PROBE_CONTROLS_HPP_
#define RENODX_GAMES_MGSV_RENDERING_PROBE_CONTROLS_HPP_

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>

#include "../../../utils/mutex.hpp"
#include "../../../utils/settings.hpp"
#include "d3d12_timing.hpp"
#include "live_shaders.hpp"
#include "probe_api.hpp"

// Provides probe_api.hpp. Nothing here runs unless the probe calls it.
namespace mgsv::rendering::probe {

// Applies a setting whose change is a transition owned by one feature, the way
// that feature's menu does. Returns false for a key it does not own.
using Transition = bool (*)(std::string_view key, float value);
static std::atomic<Transition> transition = nullptr;

// Pair each attach with a detach before the module unloads.
inline void UseProbe(bool attach) {
  if (attach) {
    shared.RegisterModule();
    if (shared.data == nullptr) return;
    const std::unique_lock lock(shared.data->mutex);
    shared.data->get_setting = +[](const char* key, float* value) {
      if (key == nullptr || value == nullptr || renodx::utils::settings::settings == nullptr) return false;
      const auto* setting = renodx::utils::settings::FindSetting(key);
      if (setting == nullptr || !setting->HasNumericValue()) return false;
      const std::shared_lock settings_lock(renodx::utils::mutex::global_mutex);
      *value = setting->GetValue();
      return true;
    };
    shared.data->set_setting = +[](const char* key, float value) {
      if (key == nullptr || !std::isfinite(value) || renodx::utils::settings::settings == nullptr) return false;
      const std::string_view name = key;
      if (!name.starts_with("DirectLighting") && !name.starts_with("IndirectLighting") && !name.starts_with("Atmosphere")) {
        // Other settings change only through their feature's transition.
        const auto apply = transition.load(std::memory_order_acquire);
        return apply != nullptr && apply(name, value);
      }
      auto* setting = renodx::utils::settings::FindSetting(key);
      if (setting == nullptr || !setting->HasNumericValue()) return false;
      // A labeled setting is an index into its labels.
      const float maximum = (setting->labels.empty() ? setting->max : static_cast<float>(setting->labels.size() - 1u));
      const float applied = (setting->value_type == renodx::utils::settings::SettingValueType::BOOLEAN
                                 ? std::round(std::clamp(value, 0.f, 1.f))
                                 : std::clamp(value, setting->min, maximum));
      const std::unique_lock settings_lock(renodx::utils::mutex::global_mutex);
      const float previous = setting->GetValue();
      setting->Set(applied)->Write();
      // The menu reports a change the same way, after writing the bound value.
      setting->on_change_value(previous, setting->GetValue());
      return true;
    };
    shared.data->request_d3d12_timing = d3d12_timing::Request;
    shared.data->read_d3d12_timing = d3d12_timing::Read;
    shared.data->replace_shader = live_shaders::Replace;
  } else if (shared.data != nullptr) {
    {
      const std::unique_lock lock(shared.data->mutex);
      shared.data->get_setting = nullptr;
      shared.data->set_setting = nullptr;
      shared.data->request_d3d12_timing = nullptr;
      shared.data->read_d3d12_timing = nullptr;
      shared.data->replace_shader = nullptr;
    }
    shared.UnregisterModule();
  }
}

}  // namespace mgsv::rendering::probe

#endif  // RENODX_GAMES_MGSV_RENDERING_PROBE_CONTROLS_HPP_
