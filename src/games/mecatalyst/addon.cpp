/*
 * Copyright (C) 2024 Musa Haji
 * Copyright (C) 2024 Carlos Lopez
 * SPDX-License-Identifier: MIT
 */

#define ImTextureID ImU64

#define DEBUG_LEVEL_0

#include <embed/shaders.h>

#include <deps/imgui/imgui.h>
#include <include/reshade.hpp>

#include "../../mods/shader.hpp"
#include "../../mods/swapchain.hpp"
#include "../../utils/date.hpp"
#include "../../utils/settings.hpp"
#include "./shared.h"

namespace {

renodx::mods::shader::CustomShaders custom_shaders = {
    CustomShaderEntry(0xE455F507),  // Tonemap 1
    CustomShaderEntry(0x35DF3BA8),  // Tonemap 2 - No Lens Dirt
    CustomShaderEntry(0x6C50EBC8),  // Lens Effect
    CustomShaderEntry(0xFB2ADDC7),  // Lens Effect 2
    CustomShaderEntry(0xC67F788C),  // DoF

    CustomShaderEntry(0x041FA975),  // Video
};

ShaderInjectData shader_injection;

renodx::utils::settings::Settings settings = {
    new renodx::utils::settings::Setting{
        .key = "toneMapType",
        .binding = &shader_injection.toneMapType,
        .value_type = renodx::utils::settings::SettingValueType::INTEGER,
        .default_value = 3.f,
        .can_reset = false,
        .label = "Tone Mapper",
        .section = "Tone Mapping",
        .tooltip = "Sets the tone mapper type",
        .labels = {"Vanilla", "None", "ACES", "RenoDRT"},
    },
    new renodx::utils::settings::Setting{
        .key = "toneMapPeakNits",
        .binding = &shader_injection.toneMapPeakNits,
        .default_value = 1000.f,
        .can_reset = false,
        .label = "Peak Brightness",
        .section = "Tone Mapping",
        .tooltip = "Sets the value of peak white in nits",
        .min = 48.f,
        .max = 4000.f,
        .is_enabled = []() { return shader_injection.toneMapType >= 2; },
    },
    new renodx::utils::settings::Setting{
        .key = "toneMapGameNits",
        .binding = &shader_injection.toneMapGameNits,
        .default_value = 203.f,
        .can_reset = true,
        .label = "Game Brightness",
        .section = "Tone Mapping",
        .tooltip = "Sets the value of 100%% white in nits",
        .min = 48.f,
        .max = 500.f,
    },
    new renodx::utils::settings::Setting{
        .key = "toneMapUINits",
        .binding = &shader_injection.toneMapUINits,
        .default_value = 203.f,
        .can_reset = true,
        .label = "UI Brightness",
        .section = "Tone Mapping",
        .tooltip = "Sets the brightness of UI and HUD elements in nits",
        .min = 48.f,
        .max = 500.f,
    },
    new renodx::utils::settings::Setting{
        .key = "toneMapHueCorrection",
        .binding = &shader_injection.toneMapHueCorrection,
        .default_value = 0.f,
        .can_reset = false,
        .label = "Hue Correction",
        .section = "Tone Mapping",
        .tooltip = "Emulates hue shifting from the vanilla tonemapper",
        .max = 100.f,
        .is_enabled = []() { return shader_injection.toneMapType != 0; },
        .parse = [](float value) { return value * 0.01f; },
    },
    new renodx::utils::settings::Setting{
        .key = "ToneMapScaling",
        .binding = &shader_injection.toneMapPerChannel,
        .value_type = renodx::utils::settings::SettingValueType::INTEGER,
        .default_value = 1.f,
        .label = "Scaling",
        .section = "Tone Mapping",
        .tooltip = "Luminance scales colors consistently while per-channel saturates and blows out sooner",
        .labels = {"Luminance", "Per Channel"},
        .is_enabled = []() { return shader_injection.toneMapType == 3.f; },
    },
    new renodx::utils::settings::Setting{
        .key = "toneMapBlend",
        .binding = &shader_injection.toneMapBlend,
        .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
        .default_value = 0.f,
        .can_reset = true,
        .label = "Tonemap Blend",
        .section = "Tone Mapping",
        .tooltip = "Blends the Custom tonemapper with Vanilla",
        .is_enabled = []() { return shader_injection.toneMapType > 1.f; },
    },
    new renodx::utils::settings::Setting{
        .key = "colorGradeExposure",
        .binding = &shader_injection.colorGradeExposure,
        .default_value = 1.f,
        .label = "Exposure",
        .section = "Color Grading",
        .max = 20.f,
        .format = "%.2f",
        .is_enabled = []() { return shader_injection.toneMapType != 0 && shader_injection.toneMapBlend == 0; },
    },
    new renodx::utils::settings::Setting{
        .key = "colorGradeHighlights",
        .binding = &shader_injection.colorGradeHighlights,
        .default_value = 50.f,
        .label = "Highlights",
        .section = "Color Grading",
        .max = 100.f,
        .is_enabled = []() { return shader_injection.toneMapType != 0; },
        .parse = [](float value) { return value * 0.02f; },
    },
    new renodx::utils::settings::Setting{
        .key = "colorGradeShadows",
        .binding = &shader_injection.colorGradeShadows,
        .default_value = 50.f,
        .label = "Shadows",
        .section = "Color Grading",
        .max = 100.f,
        .is_enabled = []() { return shader_injection.toneMapType != 0 && shader_injection.toneMapBlend == 0; },
        .parse = [](float value) { return value * 0.02f; },
    },
    new renodx::utils::settings::Setting{
        .key = "colorGradeContrast",
        .binding = &shader_injection.colorGradeContrast,
        .default_value = 50.f,
        .label = "Contrast",
        .section = "Color Grading",
        .max = 100.f,
        .is_enabled = []() { return shader_injection.toneMapType != 0 && shader_injection.toneMapBlend == 0; },
        .parse = [](float value) { return value * 0.02f; },
    },
    new renodx::utils::settings::Setting{
        .key = "colorGradeSaturation",
        .binding = &shader_injection.colorGradeSaturation,
        .default_value = 50.f,
        .label = "Saturation",
        .section = "Color Grading",
        .max = 100.f,
        .is_enabled = []() { return shader_injection.toneMapType != 0 && shader_injection.toneMapBlend == 0; },
        .parse = [](float value) { return value * 0.02f; },
    },
    new renodx::utils::settings::Setting{
        .key = "ColorGradeHighlightSaturation",
        .binding = &shader_injection.colorGradeBlowout,
        .default_value = 100.f,
        .label = "Highlight Saturation",
        .section = "Color Grading",
        .tooltip = "Adds or removes highlight color.",
        .max = 100.f,
        .is_enabled = []() { return shader_injection.toneMapType != 0; },
        .parse = [](float value) { return (value * -0.02f) + 1.f; },
    },
    new renodx::utils::settings::Setting{
        .key = "colorGradeFlare",
        .binding = &shader_injection.colorGradeFlare,
        .default_value = 50.f,
        .label = "Flare",
        .section = "Color Grading",
        .tooltip = "Flare/Glare",
        .max = 100.f,
        .is_enabled = []() { return shader_injection.toneMapType == 3.f && shader_injection.toneMapBlend == 0; },
        .parse = [](float value) { return value * 0.02f; },
    },
    new renodx::utils::settings::Setting{
        .key = "ColorGradeRestorationMethod",
        .binding = &shader_injection.colorGradeRestorationMethod,
        .value_type = renodx::utils::settings::SettingValueType::INTEGER,
        .default_value = 0.f,
        .label = "Grading Application",
        .section = "Color Grading",
        .tooltip = "Chooses method to apply grading for HDR.",
        .labels = {"Luminance", "Per Channel"},
        .is_enabled = []() { return shader_injection.toneMapType != 0; },
    },
    new renodx::utils::settings::Setting{
        .key = "colorGradeLUTStrength",
        .binding = &shader_injection.colorGradeLUTStrength,
        .default_value = 100.f,
        .label = "LUT Strength",
        .section = "Color Grading",
        .max = 100.f,
        .parse = [](float value) { return value * 0.01f; },
    },
    new renodx::utils::settings::Setting{
        .key = "fxBloom",
        .binding = &shader_injection.fxBloom,
        .default_value = 50.f,
        .label = "Bloom",
        .section = "Effects",
        .max = 100.f,
        .parse = [](float value) { return value * 0.02f; },
    },
    new renodx::utils::settings::Setting{
        .key = "fxDoF",
        .binding = &shader_injection.fxDoF,
        .default_value = 50.f,
        .label = "Depth of Field",
        .section = "Effects",
        .max = 100.f,
        .parse = [](float value) { return value * 0.02f; },
    },
    new renodx::utils::settings::Setting{
        .key = "fxVignette",
        .binding = &shader_injection.fxVignette,
        .default_value = 50.f,
        .label = "Vignette",
        .section = "Effects",
        .max = 100.f,
        .parse = [](float value) { return value * 0.02f; },
    },
    new renodx::utils::settings::Setting{
        .value_type = renodx::utils::settings::SettingValueType::BUTTON,
        .label = "Discord",
        .section = "Links",
        .group = "button-line-1",
        .tint = 0x5865F2,
        .on_change = []() {
          ShellExecute(0, "open", (std::string("https://discord.gg/") + "5WZXDpmbpP").c_str(), 0, 0, SW_SHOW);
        },
    },
    new renodx::utils::settings::Setting{
        .value_type = renodx::utils::settings::SettingValueType::BUTTON,
        .label = "More Mods",
        .section = "Links",
        .group = "button-line-1",
        .tint = 0x2B3137,
        .on_change = []() {
          ShellExecute(0, "open", (std::string("https://github.com/") + "clshortfuse/renodx/wiki/Mods").c_str(), 0, 0, SW_SHOW);
        },
    },
    new renodx::utils::settings::Setting{
        .value_type = renodx::utils::settings::SettingValueType::BUTTON,
        .label = "Github",
        .section = "Links",
        .group = "button-line-1",
        .tint = 0x2B3137,
        .on_change = []() {
          ShellExecute(0, "open", (std::string("https://github.com/") + "clshortfuse/renodx").c_str(), 0, 0, SW_SHOW);
        },
    },
    new renodx::utils::settings::Setting{
        .value_type = renodx::utils::settings::SettingValueType::BUTTON,
        .label = "Musa's Ko-Fi",
        .section = "Links",
        .group = "button-line-2",
        .tint = 0xFF5A16,
        .on_change = []() {
          ShellExecute(0, "open", (std::string("https://ko-fi.com/") + "musaqh").c_str(), 0, 0, SW_SHOW);
        },
    },
    new renodx::utils::settings::Setting{
        .value_type = renodx::utils::settings::SettingValueType::TEXT,
        .label = std::string("Build: ") + renodx::utils::date::ISO_DATE_TIME,
        .section = "About",
    },
};

void OnPresetOff() {
  renodx::utils::settings::UpdateSetting("toneMapType", 0.f);
  renodx::utils::settings::UpdateSetting("toneMapPeakNits", 203.f);
  renodx::utils::settings::UpdateSetting("toneMapGameNits", 203.f);
  renodx::utils::settings::UpdateSetting("toneMapUINits", 203.f);
  renodx::utils::settings::UpdateSetting("toneMapHueCorrection", 0);
  renodx::utils::settings::UpdateSetting("toneMapBlend", 0.f);
  renodx::utils::settings::UpdateSetting("colorGradeExposure", 1.f);
  renodx::utils::settings::UpdateSetting("colorGradeHighlights", 50.f);
  renodx::utils::settings::UpdateSetting("colorGradeShadows", 50.f);
  renodx::utils::settings::UpdateSetting("colorGradeContrast", 50.f);
  renodx::utils::settings::UpdateSetting("colorGradeSaturation", 50.f);
  renodx::utils::settings::UpdateSetting("ColorGradeHighlightSaturation", 50.f);
  renodx::utils::settings::UpdateSetting("colorGradeLUTStrength", 100.f);
  renodx::utils::settings::UpdateSetting("fxBloom", 50.f);
  renodx::utils::settings::UpdateSetting("fxDoF", 50.f);
  renodx::utils::settings::UpdateSetting("fxVignette", 50.f);
}

// Final shader [ty Ersh/FF14]
struct __declspec(uuid("1228220F-364A-46A2-BB29-1CCE591A018A")) DeviceData {
  reshade::api::effect_runtime* main_runtime = nullptr;
  std::atomic_bool rendered_effects = false;
  std::vector<reshade::api::resource_view> swapchain_rtvs;
  reshade::api::pipeline final_pipeline = {};
  reshade::api::resource final_texture = {};
  reshade::api::resource_view final_texture_view = {};
  reshade::api::sampler final_texture_sampler = {};
  reshade::api::pipeline_layout final_layout = {};
};

constexpr reshade::api::pipeline_layout PIPELINE_LAYOUT{0};

void OnInitDevice(reshade::api::device* device) {
  auto& data = device->create_private_data<DeviceData>();

  // create pipeline
  {
    std::vector<reshade::api::pipeline_subobject> subobjects;

    reshade::api::shader_desc vs_desc = {};
    vs_desc.code = __0xFFFFFFFD.begin();
    vs_desc.code_size = __0xFFFFFFFD.size();
    subobjects.push_back({reshade::api::pipeline_subobject_type::vertex_shader, 1, &vs_desc});

    reshade::api::shader_desc ps_desc = {};
    ps_desc.code = __0xFFFFFFFE.begin();
    ps_desc.code_size = __0xFFFFFFFE.size();
    subobjects.push_back({reshade::api::pipeline_subobject_type::pixel_shader, 1, &ps_desc});

    reshade::api::format format = reshade::api::format::r16g16b16a16_float;
    subobjects.push_back({reshade::api::pipeline_subobject_type::render_target_formats, 1, &format});

    uint32_t num_vertices = 3;
    subobjects.push_back({reshade::api::pipeline_subobject_type::max_vertex_count, 1, &num_vertices});

    auto topology = reshade::api::primitive_topology::triangle_list;
    subobjects.push_back({reshade::api::pipeline_subobject_type::primitive_topology, 1, &topology});

    reshade::api::blend_desc blend_state = {};
    subobjects.push_back({reshade::api::pipeline_subobject_type::blend_state, 1, &blend_state});

    reshade::api::rasterizer_desc rasterizer_state = {};
    rasterizer_state.cull_mode = reshade::api::cull_mode::none;
    subobjects.push_back({reshade::api::pipeline_subobject_type::rasterizer_state, 1, &rasterizer_state});

    reshade::api::depth_stencil_desc depth_stencil_state = {};
    depth_stencil_state.depth_enable = false;
    depth_stencil_state.depth_write_mask = false;
    depth_stencil_state.depth_func = reshade::api::compare_op::always;
    depth_stencil_state.stencil_enable = false;
    depth_stencil_state.front_stencil_read_mask = 0xFF;
    depth_stencil_state.front_stencil_write_mask = 0xFF;
    depth_stencil_state.front_stencil_func = depth_stencil_state.back_stencil_func;
    depth_stencil_state.front_stencil_fail_op = depth_stencil_state.back_stencil_fail_op;
    depth_stencil_state.front_stencil_depth_fail_op = depth_stencil_state.back_stencil_depth_fail_op;
    depth_stencil_state.front_stencil_pass_op = depth_stencil_state.back_stencil_pass_op;
    depth_stencil_state.back_stencil_read_mask = 0xFF;
    depth_stencil_state.back_stencil_write_mask = 0xFF;
    depth_stencil_state.back_stencil_func = reshade::api::compare_op::always;
    depth_stencil_state.back_stencil_fail_op = reshade::api::stencil_op::keep;
    depth_stencil_state.back_stencil_depth_fail_op = reshade::api::stencil_op::keep;
    depth_stencil_state.back_stencil_pass_op = reshade::api::stencil_op::keep;

    subobjects.push_back({reshade::api::pipeline_subobject_type::depth_stencil_state, 1, &depth_stencil_state});

    device->create_pipeline(PIPELINE_LAYOUT, static_cast<uint32_t>(subobjects.size()), subobjects.data(), &data.final_pipeline);
  }

  // create layout
  {
    reshade::api::pipeline_layout_param new_params;
    new_params.type = reshade::api::pipeline_layout_param_type::push_constants;
    new_params.push_constants.count = 1;
    new_params.push_constants.dx_register_index = 13;
    new_params.push_constants.visibility = reshade::api::shader_stage::vertex | reshade::api::shader_stage::pixel | reshade::api::shader_stage::compute;
    device->create_pipeline_layout(1, &new_params, &data.final_layout);
  }
}

void OnDestroyDevice(reshade::api::device* device) {
  auto& data = device->get_private_data<DeviceData>();

  device->destroy_pipeline(data.final_pipeline);
  device->destroy_pipeline_layout(data.final_layout);

  device->destroy_private_data<DeviceData>();
}

bool fired_on_init_swapchain = false;
void OnInitSwapchain(reshade::api::swapchain* swapchain) {
  auto device = swapchain->get_device();
  auto& data = device->get_private_data<DeviceData>();

  if (!fired_on_init_swapchain) {
    fired_on_init_swapchain = true;
    auto peak = renodx::utils::swapchain::GetPeakNits(swapchain);
    if (peak.has_value()) {
      settings[1]->default_value = peak.value();
      settings[1]->can_reset = true;
    }
  }

  for (int i = 0; i < swapchain->get_back_buffer_count(); ++i) {
    auto back_buffer_resource = swapchain->get_back_buffer(i);
    auto back_buffer_desc = device->get_resource_desc(back_buffer_resource);
    auto desc = reshade::api::resource_view_desc(reshade::api::resource_view_type::texture_2d, reshade::api::format_to_default_typed(back_buffer_desc.texture.format), 0, 1, 0, 1);
    device->create_resource_view(back_buffer_resource, reshade::api::resource_usage::render_target, desc, &data.swapchain_rtvs.emplace_back());
  }

  // create copy target
  {
    auto back_buffer_resource = swapchain->get_back_buffer(0);
    auto back_buffer_desc = device->get_resource_desc(back_buffer_resource);
    reshade::api::resource_desc desc = {};
    desc.type = reshade::api::resource_type::texture_2d;
    desc.texture = {
        back_buffer_desc.texture.width,
        back_buffer_desc.texture.height,
        1,
        1,
        reshade::api::format_to_typeless(back_buffer_desc.texture.format),
        1,
    };
    desc.heap = reshade::api::memory_heap::gpu_only;
    desc.usage = reshade::api::resource_usage::copy_dest | reshade::api::resource_usage::shader_resource;
    desc.flags = reshade::api::resource_flags::none;
    device->create_resource(desc, nullptr, reshade::api::resource_usage::shader_resource, &data.final_texture);
    device->create_resource_view(data.final_texture, reshade::api::resource_usage::shader_resource, reshade::api::resource_view_desc(reshade::api::format_to_default_typed(desc.texture.format)), &data.final_texture_view);
    device->create_sampler({}, &data.final_texture_sampler);
  }
}

void OnDestroySwapchain(reshade::api::swapchain* swapchain) {
  auto device = swapchain->get_device();
  auto& data = device->get_private_data<DeviceData>();

  for (const auto& rtv : data.swapchain_rtvs) {
    device->destroy_resource_view(rtv);
  }

  data.swapchain_rtvs.clear();

  device->destroy_sampler(data.final_texture_sampler);
  device->destroy_resource_view(data.final_texture_view);
  device->destroy_resource(data.final_texture);
}

// more or less the same as what reshade does to render its techniques
void OnPresent(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain, const reshade::api::rect* source_rect, const reshade::api::rect* dest_rect, uint32_t dirty_rect_count, const reshade::api::rect* dirty_rects) {
  auto device = queue->get_device();
  auto cmd_list = queue->get_immediate_command_list();

  auto& data = device->get_private_data<DeviceData>();

  auto back_buffer_resource = swapchain->get_current_back_buffer();
  auto back_buffer_desc = device->get_resource_desc(back_buffer_resource);

  // copy backbuffer
  {
    const reshade::api::resource resources[2] = {back_buffer_resource, data.final_texture};
    const reshade::api::resource_usage state_old[2] = {reshade::api::resource_usage::render_target, reshade::api::resource_usage::shader_resource};
    const reshade::api::resource_usage state_new[2] = {reshade::api::resource_usage::copy_source, reshade::api::resource_usage::copy_dest};

    cmd_list->barrier(2, resources, state_old, state_new);
    cmd_list->copy_texture_region(back_buffer_resource, 0, nullptr, data.final_texture, 0, nullptr);
    cmd_list->barrier(2, resources, state_new, state_old);
  }

  cmd_list->bind_pipeline(reshade::api::pipeline_stage::all_graphics, data.final_pipeline);

  cmd_list->barrier(back_buffer_resource, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::render_target);

  reshade::api::render_pass_render_target_desc render_target = {};
  render_target.view = data.swapchain_rtvs.at(swapchain->get_current_back_buffer_index());
  cmd_list->begin_render_pass(1, &render_target, nullptr);

  cmd_list->push_descriptors(reshade::api::shader_stage::all_graphics, PIPELINE_LAYOUT, 0, reshade::api::descriptor_table_update{{}, 0, 0, 1, reshade::api::descriptor_type::texture_shader_resource_view, &data.final_texture_view});
  cmd_list->push_descriptors(reshade::api::shader_stage::all_graphics, PIPELINE_LAYOUT, 0, reshade::api::descriptor_table_update{{}, 0, 0, 1, reshade::api::descriptor_type::sampler, &data.final_texture_sampler});

  // push the renodx settings
  cmd_list->push_constants(reshade::api::shader_stage::all_graphics, data.final_layout, 0, 0, sizeof(shader_injection) / 4, &shader_injection);

  const reshade::api::viewport viewport = {
      0.0f, 0.0f,
      static_cast<float>(back_buffer_desc.texture.width),
      static_cast<float>(back_buffer_desc.texture.height),
      0.0f, 1.0f};
  cmd_list->bind_viewports(0, 1, &viewport);

  cmd_list->draw(3, 1, 0, 0);
  cmd_list->end_render_pass();

  cmd_list->barrier(back_buffer_resource, reshade::api::resource_usage::render_target, reshade::api::resource_usage::shader_resource);
}

}  // namespace

// NOLINTBEGIN(readability-identifier-naming)

extern "C" __declspec(dllexport) const char* name = "RenoDX";
extern "C" __declspec(dllexport) const char* description = "RenoDX for Mirror's Edge: Catalyst";

// NOLINTEND(readability-identifier-naming)
const float SCREEN_WIDTH = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
const float SCREEN_HEIGHT = static_cast<float>(GetSystemMetrics(SM_CYSCREEN));
BOOL APIENTRY DllMain(HMODULE h_module, DWORD fdw_reason, LPVOID lpv_reserved) {
  switch (fdw_reason) {
    case DLL_PROCESS_ATTACH:
      if (!reshade::register_addon(h_module)) return FALSE;

      renodx::mods::shader::force_pipeline_cloning = true;
      renodx::mods::shader::expected_constant_buffer_index = 13;
      renodx::mods::shader::trace_unmodified_shaders = true;

      //   renodx::mods::swapchain::force_borderless = true;
      //   renodx::mods::swapchain::prevent_full_screen = true;

      // Final shader
      reshade::register_event<reshade::addon_event::init_device>(OnInitDevice);
      reshade::register_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
      reshade::register_event<reshade::addon_event::init_swapchain>(OnInitSwapchain);
      reshade::register_event<reshade::addon_event::destroy_swapchain>(OnDestroySwapchain);
      reshade::register_event<reshade::addon_event::present>(OnPresent);

      break;
    case DLL_PROCESS_DETACH:
      reshade::unregister_event<reshade::addon_event::init_device>(OnInitDevice);
      reshade::unregister_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
      reshade::unregister_event<reshade::addon_event::init_swapchain>(OnInitSwapchain);
      reshade::unregister_event<reshade::addon_event::destroy_swapchain>(OnDestroySwapchain);
      reshade::unregister_event<reshade::addon_event::present>(OnPresent);
      reshade::unregister_addon(h_module);
      break;
  }

  renodx::utils::settings::Use(fdw_reason, &settings, &OnPresetOff);
  renodx::mods::swapchain::Use(fdw_reason);
  renodx::mods::shader::Use(fdw_reason, custom_shaders, &shader_injection);

  return TRUE;
}