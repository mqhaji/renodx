/*
 * Copyright (C) 2024 Carlos Lopez
 * Copyright (C) 2024 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#define ImTextureID ImU64

#define DEBUG_LEVEL_0

// #include <embed/0xEFF57007.h>    // 
#include <embed/0xA6F33860.h>    // Final Tonemap
#include <embed/0x5BBBD356.h>    // ?



#include <chrono>

#include <deps/imgui/imgui.h>
#include <include/reshade.hpp>
#include "../../mods/shaderReplaceMod.hpp"
#include "../../mods/swapChainUpgradeMod.hpp"
#include "../../utils/userSettingUtil.hpp"
#include "./shared.h"

extern "C" __declspec(dllexport) const char* NAME = "RenoDX - Dishonored 2";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "RenoDX for Dishonored 2";

ShaderReplaceMod::CustomShaders customShaders = {

  CustomShaderEntry(0xA6F33860),
  CustomShaderEntry(0x5BBBD356),
  // CustomSwapchainShader(),      // 


};

ShaderInjectData shaderInjection;

// clang-format off
UserSettingUtil::UserSettings userSettings = {
  new UserSettingUtil::UserSetting {
    .key = "toneMapType",
    .binding = &shaderInjection.toneMapType,
    .valueType = UserSettingUtil::UserSettingValueType::integer,
    .defaultValue = 3.f,
    .canReset = false,
    .label = "Tone Mapper",
    .section = "Tone Mapping",
    .tooltip = "Sets the tone mapper type",
    .labels = {"Vanilla", "None", "ACES", "RenoDRT"}
  },
  new UserSettingUtil::UserSetting {
    .key = "toneMapPeakNits",
    .binding = &shaderInjection.toneMapPeakNits,
    .defaultValue = 1000.f,
    .canReset = false,
    .label = "Peak Brightness",
    .section = "Tone Mapping",
    .tooltip = "Sets the value of peak white in nits",
    .min = 48.f,
    .max = 4000.f
  },
  new UserSettingUtil::UserSetting {
    .key = "toneMapGameNits",
    .binding = &shaderInjection.toneMapGameNits,
    .defaultValue = 203.f,
    .canReset = false,
    .label = "Game Brightness",
    .section = "Tone Mapping",
    .tooltip = "Sets the value of 100%% white in nits",
    .min = 48.f,
    .max = 500.f
  },
  new UserSettingUtil::UserSetting {
    .key = "toneMapUINits",
    .binding = &shaderInjection.toneMapUINits,
    .defaultValue = 203.f,
    .canReset = false,
    .label = "UI Brightness",
    .section = "Tone Mapping",
    .tooltip = "Sets the brightness of UI and HUD elements in nits",
    .min = 48.f,
    .max = 500.f
  },
  new UserSettingUtil::UserSetting {
    .key = "toneMapGammaCorrection",
    .binding = &shaderInjection.toneMapGammaCorrection,
    .valueType = UserSettingUtil::UserSettingValueType::boolean,
    .canReset = false,
    .label = "Gamma Correction",
    .section = "Tone Mapping",
    .tooltip = "Emulates a 2.2 EOTF (use with HDR or sRGB)",
  },
  new UserSettingUtil::UserSetting {
    .key = "colorGradeExposure",
    .binding = &shaderInjection.colorGradeExposure,
    .defaultValue = 1.f,
    .label = "Exposure",
    .section = "Color Grading",
    .max = 20.f,
    .format = "%.2f"
  },
  new UserSettingUtil::UserSetting {
    .key = "colorGradeHighlights",
    .binding = &shaderInjection.colorGradeHighlights,
    .defaultValue = 50.f,
    .label = "Highlights",
    .section = "Color Grading",
    .max = 100.f,
    .parse = [](float value) { return value * 0.02f; }
  },
  new UserSettingUtil::UserSetting {
    .key = "colorGradeShadows",
    .binding = &shaderInjection.colorGradeShadows,
    .defaultValue = 50.f,
    .label = "Shadows",
    .section = "Color Grading",
    .max = 100.f,
    .parse = [](float value) { return value * 0.02f; }
  },
  new UserSettingUtil::UserSetting {
    .key = "colorGradeContrast",
    .binding = &shaderInjection.colorGradeContrast,
    .defaultValue = 50.f,
    .label = "Contrast",
    .section = "Color Grading",
    .max = 100.f,
    .parse = [](float value) { return value * 0.02f; }
  },
  new UserSettingUtil::UserSetting {
    .key = "colorGradeSaturation",
    .binding = &shaderInjection.colorGradeSaturation,
    .defaultValue = 50.f,
    .label = "Saturation",
    .section = "Color Grading",
    .max = 100.f,
    .parse = [](float value) { return value * 0.02f; }
  },
  new UserSettingUtil::UserSetting {
    .key = "colorGradeDechroma",
    .binding = &shaderInjection.colorGradeDechroma,
    .defaultValue = 50.f,
    .label = "Blowout",
    .section = "Color Grading",
    .max = 100.f,
    .parse = [](float value) { return value * 0.01f; }
  },
  new UserSettingUtil::UserSetting {
    .key = "colorGradeLUTStrength",
    .binding = &shaderInjection.colorGradeLUTStrength,
    .defaultValue = 100.f,
    .label = "LUT Strength",
    .section = "Color Grading",
    .max = 100.f,
    .parse = [](float value) { return value * 0.01f; }
  },
  new UserSettingUtil::UserSetting {
    .key = "colorGradeLUTScaling",
    .binding = &shaderInjection.colorGradeLUTScaling,
    .defaultValue = 100.f,
    .label = "LUT Scaling",
    .section = "Color Grading",
    .tooltip = "Scales the color grade LUT to full range when size is clamped.",
    .max = 100.f,
    .parse = [](float value) { return value * 0.01f; }
  },
  new UserSettingUtil::UserSetting {
    .key = "lensDirt",
    .binding = &shaderInjection.lensDirt,
    .valueType = UserSettingUtil::UserSettingValueType::boolean,
    .canReset = false,
    .label = "Lens Dirt",
    .section = "Effects",
    .tooltip = "Toggles Dirty Lens Effect",
  },
  new UserSettingUtil::UserSetting {
    .key = "fxBloom",
    .binding = &shaderInjection.fxBloom,
    .defaultValue = 50.f,
    .label = "Bloom",
    .section = "Effects",
    .max = 100.f,
    .parse = [](float value) { return value * 0.02f; }
  },
  new UserSettingUtil::UserSetting {
    .key = "fxAutoExposure",
    .binding = &shaderInjection.fxAutoExposure,
    .defaultValue = 50.f,
    .label = "Auto Exposure",
    .section = "Effects",
    .max = 100.f,
    .parse = [](float value) { return value * 0.02f; }
  },
  new UserSettingUtil::UserSetting {
    .key = "fxDoF",
    .binding = &shaderInjection.fxDoF,
    .defaultValue = 50.f,
    .label = "Depth of Field",
    .section = "Effects",
    .max = 100.f,
    .parse = [](float value) { return value * 0.02f; }
  },
  new UserSettingUtil::UserSetting {
    .key = "fxFilmGrain",
    .binding = &shaderInjection.fxFilmGrain,
    .defaultValue = 0.f,
    .label = "Film Grain",
    .section = "Effects",
    .max = 100.f,
    .parse = [](float value) { return value * 0.02f; }
  },
  new UserSettingUtil::UserSetting {
    .key = "midGray",
    .binding = &shaderInjection.midGray,
    .defaultValue = .18f,
    .label = "vanillaMidGray",
    .section = "Tonemapper Advanced Settings",
    .max = .4f,
    .format = "%.4f"
  },
  new UserSettingUtil::UserSetting {
    .key = "renoDRTFlare",
    .binding = &shaderInjection.renoDRTFlare,
    .defaultValue = 0.f,
    .label = "renoDRTFlare",
    .section = "Tonemapper Advanced Settings",
    .max = 0.5f,
    .format = "%.4f"
  }

};

// clang-format on

static void onPresetOff() {
  UserSettingUtil::updateUserSetting("toneMapType", 0.f);
  UserSettingUtil::updateUserSetting("toneMapPeakNits", 203.f);
  UserSettingUtil::updateUserSetting("toneMapGameNits", 203.f);
  UserSettingUtil::updateUserSetting("toneMapUINits", 203.f);
  UserSettingUtil::updateUserSetting("toneMapGammaCorrection", 0);
  UserSettingUtil::updateUserSetting("lensDirt", 1);
  UserSettingUtil::updateUserSetting("colorGradeExposure", 1.f);
  UserSettingUtil::updateUserSetting("colorGradeHighlights", 50.f);
  UserSettingUtil::updateUserSetting("colorGradeShadows", 50.f);
  UserSettingUtil::updateUserSetting("colorGradeContrast", 50.f);
  UserSettingUtil::updateUserSetting("colorGradeSaturation", 50.f);
  UserSettingUtil::updateUserSetting("colorGradeLUTStrength", 100.f);
  UserSettingUtil::updateUserSetting("colorGradeLUTScaling", 0.f);
  UserSettingUtil::updateUserSetting("fxBloom", 50.f);
  UserSettingUtil::updateUserSetting("fxAutoExposure", 50.f);
  UserSettingUtil::updateUserSetting("fxSceneFilter", 50.f);
  UserSettingUtil::updateUserSetting("fxDoF", 50.f);
  UserSettingUtil::updateUserSetting("fxFilmGrain", 0.f);
}

static auto start = std::chrono::steady_clock::now();

static void on_present(
  reshade::api::command_queue* queue,
  reshade::api::swapchain* swapchain,
  const reshade::api::rect* source_rect,
  const reshade::api::rect* dest_rect,
  uint32_t dirty_rect_count,
  const reshade::api::rect* dirty_rects
) {
  auto end = std::chrono::steady_clock::now();
  shaderInjection.elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID) {
  switch (fdwReason) {
    case DLL_PROCESS_ATTACH:

      ShaderReplaceMod::forcePipelineCloning = true;
      ShaderReplaceMod::traceUnmodifiedShaders = true;
      ShaderReplaceMod::expectedConstantBufferIndex = 13;
      SwapChainUpgradeMod::swapChainUpgradeTargets.push_back(
        {reshade::api::format::r8g8b8a8_unorm, reshade::api::format::r16g16b16a16_float} 
      );
      SwapChainUpgradeMod::swapChainUpgradeTargets.push_back(
        {reshade::api::format::r8g8b8a8_typeless, reshade::api::format::r16g16b16a16_float} 
      );
      SwapChainUpgradeMod::swapChainUpgradeTargets.push_back(
        {reshade::api::format::r8g8b8a8_unorm_srgb, reshade::api::format::r16g16b16a16_float}
      );
      SwapChainUpgradeMod::swapChainUpgradeTargets.push_back(
        {reshade::api::format::r10g10b10a2_unorm, reshade::api::format::r16g16b16a16_float}
      );      if (!reshade::register_addon(hModule)) return FALSE;

      reshade::register_event<reshade::addon_event::present>(on_present);

      break;
    case DLL_PROCESS_DETACH:
      reshade::unregister_addon(hModule);
      reshade::unregister_event<reshade::addon_event::present>(on_present);
      break;
  }

  UserSettingUtil::use(fdwReason, &userSettings, &onPresetOff);
  SwapChainUpgradeMod::use(fdwReason);
  ShaderReplaceMod::use(fdwReason, customShaders, &shaderInjection);

  return TRUE;
}