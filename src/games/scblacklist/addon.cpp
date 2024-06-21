/*
 * Copyright (C) 2024 Carlos Lopez
 * Copyright (C) 2024 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#define ImTextureID ImU64

#define DEBUG_LEVEL_0

// #include <embed/0x5A048E11.h>    // autoexposure?
#include <embed/0x03DBB3FD.h>    // Tonemap

// #include <embed/0x027BD214.h>    // UI? not tested

#include <embed/0xDEB90D13.h>    // UI
#include <embed/0x08E2AD09.h>    // UI
#include <embed/0x9432363F.h>    // UI
#include <embed/0x55DEBC30.h>    // UI, main menu background, breaks some text and reflections
#include <embed/0x73DAF702.h>    // UI
#include <embed/0x5FCBD9D3.h>    // UI
#include <embed/0x410AAFCA.h>    // UI
#include <embed/0xC2CE1537.h>    // In game screens
#include <embed/0xD88AF4AD.h>    // UI
#include <embed/0xD49092F7.h>    // UI

// #include <embed/0x73DAF702.h>    // ?



#include <chrono>

#include <deps/imgui/imgui.h>
#include <include/reshade.hpp>
#include "../../mods/shaderReplaceMod.hpp"
#include "../../mods/swapChainUpgradeMod.hpp"
#include "../../utils/userSettingUtil.hpp"
#include "./shared.h"

extern "C" __declspec(dllexport) const char* NAME = "RenoDX - Splinter Cell: Blacklist";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "RenoDX for Splinter Cell: Blacklist";

ShaderReplaceMod::CustomShaders customShaders = {

  
  // CustomShaderEntry(0x5A048E11),      // Autoexposure?
  CustomShaderEntry(0x03DBB3FD),      // Tonemap  

  // CustomShaderEntry(0x027BD214),      // UI? not tested

  CustomShaderEntry(0xDEB90D13),      // UI
  CustomShaderEntry(0x08E2AD09),      // UI
  CustomShaderEntry(0x9432363F),      // UI
  CustomShaderEntry(0x55DEBC30),      // UI, main menu background, breaks some text and reflections
  CustomShaderEntry(0x73DAF702),      // UI
  CustomShaderEntry(0x5FCBD9D3),      // UI
  CustomShaderEntry(0x410AAFCA),      // UI

  CustomShaderEntry(0xC2CE1537),      // In game screens/UI
  CustomShaderEntry(0xD88AF4AD),      // UI
  CustomShaderEntry(0xD49092F7),      // UI

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
    .label = "Dechroma",
    .section = "Color Grading",
    .max = 100.f,
    .parse = [](float value) { return value * 0.01f; }
  },
  /*
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
  */
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
    .defaultValue = 50.f,
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
  UserSettingUtil::updateUserSetting("colorGradeExposure", 1.f);
  UserSettingUtil::updateUserSetting("colorGradeHighlights", 50.f);
  UserSettingUtil::updateUserSetting("colorGradeShadows", 50.f);
  UserSettingUtil::updateUserSetting("colorGradeContrast", 50.f);
  UserSettingUtil::updateUserSetting("colorGradeSaturation", 50.f);
  UserSettingUtil::updateUserSetting("colorGradeLUTStrength", 100.f);
  UserSettingUtil::updateUserSetting("colorGradeLUTScaling", 0.f);
  UserSettingUtil::updateUserSetting("fxBloom", 50.f);
  UserSettingUtil::updateUserSetting("fxAutoExposure", 50.f);
  UserSettingUtil::updateUserSetting("fxDoF", 50.f);
  UserSettingUtil::updateUserSetting("fxFilmGrain", 50.f);
}

// static auto start = std::chrono::steady_clock::now();

// static void on_present(
//   reshade::api::command_queue* queue,
//   reshade::api::swapchain* swapchain,
//   const reshade::api::rect* source_rect,
//   const reshade::api::rect* dest_rect,
//   uint32_t dirty_rect_count,
//   const reshade::api::rect* dirty_rects
// ) {
//   auto end = std::chrono::steady_clock::now();
//   shaderInjection.elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
// }

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID) {
  switch (fdwReason) {
    case DLL_PROCESS_ATTACH:

      ShaderReplaceMod::forcePipelineCloning = true;
      ShaderReplaceMod::traceUnmodifiedShaders = true;
      ShaderReplaceMod::expectedConstantBufferIndex = 11;
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

      // reshade::register_event<reshade::addon_event::present>(on_present);

      break;
    case DLL_PROCESS_DETACH:
      reshade::unregister_addon(hModule);
      // reshade::unregister_event<reshade::addon_event::present>(on_present);
      break;
  }

  UserSettingUtil::use(fdwReason, &userSettings, &onPresetOff);
  SwapChainUpgradeMod::use(fdwReason);
  ShaderReplaceMod::use(fdwReason, customShaders, &shaderInjection);

  return TRUE;
}
