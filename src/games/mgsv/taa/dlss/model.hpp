#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace mgsv::reconstruction::dlss {

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
    ModelOption{.model = Model::DEFAULT, .label = "DLL Default", .description = "Uses the model selected by the feature DLL."},
    ModelOption{.model = Model::A, .label = "Preset A", .description = "History-conservative CNN; soft and prone to ghosting."},
    ModelOption{.model = Model::B, .label = "Preset B", .description = "CNN tuned for extremely low input resolution."},
    ModelOption{.model = Model::C, .label = "Preset C", .description = "Current-frame-biased CNN; sharper motion with more shimmer."},
    ModelOption{.model = Model::D, .label = "Preset D", .description = "History-heavy CNN; stable but more prone to ghosting."},
    ModelOption{.model = Model::E, .label = "Preset E", .description = "Balanced CNN model."},
    ModelOption{.model = Model::F, .label = "Preset F", .description = "Conservative, consistent CNN; the default selection."},
    ModelOption{.model = Model::J, .label = "Preset J", .description = "Sharp first-generation transformer with more flicker than K."},
    ModelOption{.model = Model::K, .label = "Preset K", .description = "Stable first-generation transformer and general native-resolution model."},
    ModelOption{.model = Model::L, .label = "Preset L", .description = "Second-generation transformer optimized for very small inputs."},
    ModelOption{.model = Model::M, .label = "Preset M", .description = "Aggressive second-generation transformer with strong detail recovery."},
};

inline constexpr Model DEFAULT_MODEL = Model::F;
static_assert(static_cast<uint8_t>(DEFAULT_MODEL) == 6u);

inline const ModelOption* FindModelOption(Model model) {
  const auto found = std::ranges::find_if(
      MODEL_OPTIONS, [model](const ModelOption& option) { return option.model == model; });
  return found == MODEL_OPTIONS.end() ? nullptr : &*found;
}

}  // namespace mgsv::reconstruction::dlss