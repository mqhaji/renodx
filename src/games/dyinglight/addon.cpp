/*
 * Copyright (C) 2024 Carlos Lopez
 * Copyright (C) 2024 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#define ImTextureID ImU64

#define DEBUG_LEVEL_0


/* these 3 are untested*/
// #include <embed/0x8B0B0A1E.h>    // TONEMAP?
// #include <embed/0x2E034F94.h>    // TONEMAP?
// #include <embed/0xE4562BAF.h>    // TONEMAP?

// #include <embed/0x9165A474.h> // black smoke
// #include <embed/0xEC3D14D2.h> // distant fog/smoke
// #include <embed/0x32A56036.h> // distant fog/smoke
// #include <embed/0x372AEE4E.h> // black smoke
// #include <embed/0xAB16B3F7.h> // fire
// #include <embed/0xB561688F.h> // distant smoke?
// #include <embed/0x1FB36E90.h> // screen blood effect

// #include <embed/0x95588EA5.h> // radial dirty lens effect
// #include <embed/0x8135BEA2.h> // radial lens flare
#include <embed/0xA6EC1DEC.h>    // bloom
// #include <embed/0x11737C11.h> // debris and other effects?
#include <embed/0x372BEBAB.h>    // LUT 1 (only used in title screen?)
// #include <embed/0xED9872EB.h> // AA?
#include <embed/0xD42BAD58.h>    // No Film Grain + LUT 2
#include <embed/0xC20255E1.h>    // Film Grain + LUT 2

#include <embed/0x8194877A.h>    // caps brightness

#include <embed/0x558540C8.h>    // Gamma adjust
#include <embed/0x548937E1.h>    // UI - loading screen
#include <embed/0x404A04C7.h>    // UI - highlighted stuff, orange elements
#include <embed/0xBCBEE1E5.h>    // UI - text
#include <embed/0xDC15A986.h>    // UI - Alpha, semitransparent UI boxes, quit overlay
#include <embed/0x6562755C.h>    // UI - Alpha, some text
#include <embed/0xB917BF4E.h>    // UI - HUD, nav arrows, sliders, some icons, text boxes
#include <embed/0x7E8358E3.h>    // UI - HUD numbers
#include <embed/0x929C8CA5.h>    // UI - Minimap



#include <chrono>

#include <deps/imgui/imgui.h>
#include <include/reshade.hpp>
#include "../../mods/shaderReplaceMod.hpp"
#include "../../mods/swapChainUpgradeMod.hpp"
#include "../../utils/userSettingUtil.hpp"
#include "./shared.h"

extern "C" __declspec(dllexport) const char* NAME = "RenoDX - Dying Light";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "RenoDX for Dying Light";

ShaderReplaceMod::CustomShaders customShaders = {

  /* these 3 are untested*/
  // CustomShaderEntry(0x8B0B0A1E),          // TONEMAP?
  // CustomShaderEntry(0x2E034F94),          // TONEMAP?
  // CustomShaderEntry(0xE4562BAF),          // TONEMAP?


  // CustomShaderEntry(0x1FB36E90),       // screen blood effect

  // CustomShaderEntry(0x95588EA5),       // radial dirty lens effect
  // CustomShaderEntry(0x8135BEA2),       // radial lens flare
  CustomShaderEntry(0xA6EC1DEC),          // bloom
  // CustomShaderEntry(0x11737C11),       // debris and other effects?
  CustomShaderEntry(0x372BEBAB),          // LUT 1
  // CustomShaderEntry(0xED9872EB),       // AA?
  CustomShaderEntry(0xD42BAD58),          // LUT 2a - No Film Grain
  CustomShaderEntry(0xC20255E1),          // LUT 2b - Film Grain

  CustomShaderEntry(0x8194877A),          // caps brightness

  CustomSwapchainShader(0x558540C8),      // Gamma adjust
  CustomSwapchainShader(0x548937E1),      // UI - loading screen
  CustomSwapchainShader(0x404A04C7),      // UI - highlighted stuff, orange elements
  CustomSwapchainShader(0xBCBEE1E5),      // UI - text
  CustomSwapchainShader(0xDC15A986),      // UI - Alpha, semitransparent UI boxes, quit overlay
  CustomSwapchainShader(0x6562755C),      // UI - Alpha, some text
  CustomSwapchainShader(0xB917BF4E),      // UI - HUD, nav arrows, slider outlines, some icons, some text boxes
  CustomSwapchainShader(0x7E8358E3),      // UI - HUD numbers
  CustomSwapchainShader(0x929C8CA5),      // UI - Minimap  
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
    .max = 10.f,
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
    .defaultValue = 0.18f,
    .label = "midGray",
    .section = "Tonemapper Settings",
    .max = 0.4f,
    .format = "%.4f"
  },
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
      if (!reshade::register_addon(hModule)) return FALSE;

      ShaderReplaceMod::forcePipelineCloning = true;
      ShaderReplaceMod::traceUnmodifiedShaders = true;
      SwapChainUpgradeMod::forceBorderless = false;
      SwapChainUpgradeMod::preventFullScreen = false;
      for (auto index : {3, 4}) {
        SwapChainUpgradeMod::swapChainUpgradeTargets.push_back(
          {
            .oldFormat = reshade::api::format::r8g8b8a8_typeless,
            .newFormat = reshade::api::format::r16g16b16a16_float,
            .index = index
          }
        );
      }      
      // SwapChainUpgradeMod::swapChainUpgradeTargets.push_back(
      //   {reshade::api::format::r8g8b8a8_unorm, reshade::api::format::r16g16b16a16_float} 
      // );
      // SwapChainUpgradeMod::swapChainUpgradeTargets.push_back(
      //   {reshade::api::format::r8g8b8a8_unorm_srgb, reshade::api::format::r16g16b16a16_float}
      // );
      // SwapChainUpgradeMod::swapChainUpgradeTargets.push_back(
      //   {reshade::api::format::b8g8r8a8_unorm, reshade::api::format::r16g16b16a16_float}
      // );
      // SwapChainUpgradeMod::swapChainUpgradeTargets.push_back(
      //   {reshade::api::format::r10g10b10a2_unorm, reshade::api::format::r16g16b16a16_float}
      // );
      // SwapChainUpgradeMod::swapChainUpgradeTargets.push_back(
      //   {reshade::api::format::r11g11b10_float, reshade::api::format::r16g16b16a16_float}
      // );
      break;
    case DLL_PROCESS_DETACH:
      reshade::unregister_addon(hModule);
      break;
  }

  UserSettingUtil::use(fdwReason, &userSettings, &onPresetOff);

  SwapChainUpgradeMod::use(fdwReason);

  ShaderReplaceMod::use(fdwReason, customShaders, &shaderInjection);

  return TRUE;
}
