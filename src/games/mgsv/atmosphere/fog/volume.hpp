// SPDX-License-Identifier: MIT
#ifndef MGSV_ATMOSPHERE_FOG_VOLUME_HPP_
#define MGSV_ATMOSPHERE_FOG_VOLUME_HPP_

#include <d3d11_1.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <utility>
#include <vector>

#include <embed/shaders.h>
#include <include/reshade.hpp>

#include "../../../../utils/settings.hpp"
#include "../../engine/views.hpp"
#include "../../lighting/shadows/batch.hpp"
#include "../../rendering/d3d11_state.hpp"
#include "../../rendering/live_shaders.hpp"
#include "../../rendering/submission.hpp"
#include "../../shared.h"

// The engine's fog volume, which engine/render_targets.hpp creates with more
// cells per slice than the engine asks for. The engine fills it with one draw
// at the start of a view (VolFog_TppVolFog, or Fog_DrawVolume without the
// game's fog material) under a viewport fixed at 512 x 256. That draw's vertex
// shader works in fractions of the image, so it fills any size once the
// viewport covers the whole image.
//
// With Fog set to Enhanced, the mod fills the main view's volume again just
// before the first skin-scatter pass: after the sun's shadow maps are drawn
// and before deferred composition, the volume's first reader. It evaluates the
// game's fog with the fog draw's constants and adds what that draw lacks: the
// sun's or moon's light reaches a cell only where the cascade shadow maps and
// the cloud shadow let it. Two compute passes do it: fog_visibility (one
// frame's shadows per cell) and fog_volume (those blended over time, and the
// light gathered along each screen column). Other views keep the game's draw,
// and so does a view whose volume Fog_DrawVolume fills: that shader reads none
// of the fog material.
//
// The cascade shadow maps end near the camera. Where the map has terrain, two
// more passes give the fog the ground's shadows across the whole map:
// fog_terrain_heights gathers the engine's height pages into one image and
// fog_terrain_shadow turns it into the height the light clears at each place.
//
// Local lights light the fog too, which the game's fog does not have. Their
// list is the one Enhanced shadows gather for contact shadows
// (lighting/shadows/batch.hpp), so this part runs only with Shadows set to
// Enhanced. fog_light_bins sorts the lights into blocks of cells and
// fog_volume adds each cell's few lights. A shadowed spot light's shadow map
// is copied at its draw (fog_spot_shadow), because the game reuses one map
// for every such light.
//
// fog_volume also thins the fog with height and makes thick fog uneven with a
// drifting noise made here once. The ports of the lit particle shaders the
// game draws without sun shadows read a copy of the visibility image, which
// their draws get bound here.
namespace mgsv::atmosphere::fog {

// Present stores here whether Fog is Enhanced, so every pass recorded for
// one frame reads the same answer without taking the settings lock.
inline std::atomic<bool> volumetric = false;

// The settings under Atmosphere (Advanced), as Present last read them. The
// local lights' strength is zero while Shadows is Vanilla, which gathers none.
struct Controls {
  float shadow_strength = 1.f;
  bool terrain_shadows = true;
  uint32_t debug_view = 0u;
  float local_lights = 1.f;
  // The share of the fog lost per unit of height.
  float height_falloff = 0.f;
  float unevenness = 1.f;
};

struct ConstantRange {
  UINT slot = 0u;
  UINT bytes = 0u;
};
// The pixel-stage constants kept from two draws, by register. The fog draw:
// system, scene, fog material, object (eye position and sun direction). The
// shadowed sun or moon draw: scene (cascade matrices), light (where its
// shadows end), object (cloud shadow).
inline constexpr std::array<ConstantRange, 4> FOG_DRAW_RANGES = {{
    {.slot = 0u, .bytes = 64u},
    {.slot = 2u, .bytes = 480u},
    {.slot = 4u, .bytes = 128u},
    {.slot = 5u, .bytes = 208u},
}};
inline constexpr std::array<ConstantRange, 3> SUN_DRAW_RANGES = {{{.slot = 2u, .bytes = 480u}, {.slot = 3u, .bytes = 176u}, {.slot = 5u, .bytes = 208u}}};
// The vertex-stage constants kept from a terrain draw: the material's first
// vector (the map's tiles and a page's share of the height atlas) and the
// object (the map's place and size in the world).
inline constexpr std::array<ConstantRange, 2> TERRAIN_DRAW_RANGES = {{{.slot = 4u, .bytes = 16u}, {.slot = 5u, .bytes = 208u}}};
// Terrain draws refresh the kept textures and constants this often.
inline constexpr auto TERRAIN_CAPTURE_INTERVAL = std::chrono::milliseconds(250);
// The terrain images' side in texels. The game's maps hold 4096 heights a
// side at full detail and 256 at the coarsest level, which covers the map.
inline constexpr UINT TERRAIN_TEXELS = 256u;

// The latest main view builds a draw was recorded for. A build without the
// draw (the game's fog is off, a view without a shadowed sun) is not in the list.
struct RecordedScenes {
  std::array<uint64_t, 4> scenes = {};

  void Add(uint64_t scene) {
    if (Has(scene)) return;
    std::ranges::rotate(scenes, scenes.begin() + 1);
    scenes.back() = scene;
  }
  [[nodiscard]] bool Has(uint64_t scene) const { return std::ranges::find(scenes, scene) != scenes.end(); }
};

// The local lights' bins, as in fog_lights.hlsli: a bin's side in cells, its
// depth in slices, and the lights it keeps.
inline constexpr UINT BIN_CELLS = 8u;
inline constexpr UINT BIN_SLICES = 4u;
inline constexpr UINT BIN_LIGHTS = 12u;

// The passes' constants (FogPass in fog_volume.hlsli).
struct PassConstants {
  uint32_t frame = 0u;
  uint32_t flags = 0u;
  float shadow_strength = 1.f;
  uint32_t debug_view = 0u;
  float local_lights = 1.f;
  uint32_t cells_x = 0u;
  uint32_t cells_y = 0u;
  float height_falloff = 0.f;
  float time = 0.f;
  float unevenness = 1.f;
  std::array<uint32_t, 2> padding = {};
};
inline constexpr uint32_t FLAG_SHADOW_MAPS = 1u;
inline constexpr uint32_t FLAG_CLOUD_SHADOW = 2u;
inline constexpr uint32_t FLAG_HISTORY = 4u;
inline constexpr uint32_t FLAG_WORLD_SHADOW_MASK = 8u;
inline constexpr uint32_t FLAG_TERRAIN = 16u;
inline constexpr uint32_t FLAG_LOCAL_LIGHTS = 32u;
inline constexpr uint32_t FLAG_TERRAIN_MAP = 64u;
// The spot shadow maps kept for the fog, as in fog_lights.hlsli: their count,
// the atlas's tiles a side, a tile's side in texels, and a tile's record (the
// light's position and its four shadow projection vectors).
inline constexpr UINT SPOT_SHADOWS = 16u;
inline constexpr UINT SPOT_SHADOW_TILES = 4u;
inline constexpr UINT SPOT_SHADOW_TEXELS = 512u;
inline constexpr UINT SPOT_SHADOW_RECORD_BYTES = 80u;
// The noise that makes thick fog uneven: its side in texels.
inline constexpr UINT NOISE_TEXELS = 64u;
// Haze thins to 1/e over about this many units of height (the lower
// atmosphere's dust and moisture do over 1 to 2 km). Fog Height Falloff at
// 100% uses it.
inline constexpr float HAZE_SCALE_HEIGHT = 1200.f;

struct State {
  std::shared_mutex mutex;
  Controls controls;
  // Copies of the draws' constants, made in each draw's command list, so they
  // hold the latest executed draw's values when a later list reads them.
  std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, FOG_DRAW_RANGES.size()> fog_constants;
  std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, SUN_DRAW_RANGES.size()> sun_constants;
  std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, TERRAIN_DRAW_RANGES.size()> terrain_constants;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> engine_volume;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> rain_occlusion;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shadow_map;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cloud_shadow;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> world_shadow_mask;
  RecordedScenes fog_scenes;
  RecordedScenes shadow_scenes;
  // A terrain draw's tile map (each tile's page in the height atlas and its
  // level) and height atlas. They stay until another map's terrain replaces
  // them; a view build uses them only when it drew terrain itself.
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> terrain_tiles;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> terrain_atlas;
  RecordedScenes terrain_scenes;

  // The passes' resources, used on the immediate context. The volume is
  // computed here and copied into the engine's, which cannot be written from a
  // compute shader. The estimate is one frame's visibility. The two
  // visibility images alternate as its blend over time and that blend's
  // history.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> volume;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> volume_output;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> estimate_output;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> estimate_input;
  std::array<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>, 2> visibility_outputs;
  std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2> visibility_inputs;
  // A copy of the latest visibility image under one view, for draws recorded
  // before the passes run: the lit particle ports read it (fog_particle.hlsli).
  Microsoft::WRL::ComPtr<ID3D11Texture2D> particle_visibility;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particle_visibility_input;
  // Whether the passes have written the copy since it was created.
  bool particle_visibility_filled = false;
  // The map's heights, and the height the light clears at each place on it.
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> terrain_heights_output;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> terrain_heights_input;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> terrain_shadow_output;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> terrain_shadow_input;
  // The local lights each block of cells keeps: a row per bin.
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> light_bins_output;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> light_bins_input;
  // The shadowed spot lights' shadow maps, copied at each light's draw into
  // a tile of an atlas, with a record per tile; the tile's index for the copy
  // pass; and the view build the tiles were last handed out for.
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> spot_shadows_output;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spot_shadows_input;
  Microsoft::WRL::ComPtr<ID3D11Buffer> spot_shadow_records;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spot_shadow_records_input;
  std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, SPOT_SHADOWS> spot_shadow_tiles;
  uint64_t spot_shadow_scene = 0u;
  UINT spot_shadow_count = 0u;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noise;
  Microsoft::WRL::ComPtr<ID3D11Buffer> previous_scene;
  Microsoft::WRL::ComPtr<ID3D11Buffer> pass_constants;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> visibility_shader;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> volume_shader;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> terrain_heights_shader;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> terrain_shadow_shader;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> light_bins_shader;
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> spot_shadow_shader;
  uint32_t shader_revision = 0u;
  Microsoft::WRL::ComPtr<ID3D11SamplerState> linear_clamp;
  Microsoft::WRL::ComPtr<ID3D11SamplerState> less_linear_clamp;
  Microsoft::WRL::ComPtr<ID3D11SamplerState> linear_wrap;
  uint32_t frame = 0u;
  bool history = false;
};
inline State state;

// When a terrain draw last tried to refresh the kept terrain inputs, as a
// steady clock count, and the main view build a terrain draw was last
// recorded for. Every terrain draw reads them, so they stay outside the lock.
inline std::atomic<std::chrono::steady_clock::rep> terrain_capture_tried = 0;
inline std::atomic<uint64_t> terrain_scene = 0u;

// The viewport the engine set for the draw in progress on this thread. The
// draw and both callbacks run in sequence on the thread that records it.
inline thread_local std::optional<D3D11_VIEWPORT> engine_viewport;

// The pixel shader input slot the lit particle ports read the visibility
// image at (fog_particle.hlsli), and the view the particle draw in progress
// on this thread had there.
inline constexpr UINT PARTICLE_VISIBILITY_SLOT = 22u;
inline thread_local std::optional<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> displaced_particle_input;

// Adds the Atmosphere sections after the lighting sections, before Options.
inline void AppendSettings(renodx::utils::settings::Settings* registry, ShaderInjectData* shader_injection) {
  if (std::ranges::any_of(*registry, [](const auto* setting) { return setting->key == "AtmosphereFogMode"; })) return;
  const auto is_volumetric = [shader_injection] { return shader_injection->fog_mode >= 0.5f; };
  registry->insert(
      std::ranges::find_if(*registry, [](const auto* setting) { return setting->section == "Options"; }),
      {
          new renodx::utils::settings::Setting{
              .key = "AtmosphereFogMode",
              .binding = &shader_injection->fog_mode,
              .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
              .default_value = 1.f,
              .label = "Fog",
              .section = "Atmosphere",
              .tooltip = "Enhanced lights the main view's fog as light behaves: the sun and moon reach it only where nothing shades them (shadow maps nearby, hills across the map), lamps light it, it thins with height, and thick fog drifts unevenly. The fog's amount and colors for each weather and time stay the game's.",
              .labels = {"Vanilla", "Enhanced"},
          },
          new renodx::utils::settings::Setting{
              .key = "AtmosphereFogShadowStrength",
              .binding = &shader_injection->fog_shadow_strength,
              .default_value = 100.f,
              .label = "Fog Shadow Strength",
              .section = "Atmosphere (Advanced)",
              .tooltip = "How much of the sun's and moon's light a shadow takes from the fog. 100% is a full shadow; 0% gives the game's fog.",
              .max = 100.f,
              .format = "%.0f%%",
              .parse = [](float value) { return value * 0.01f; },
              .is_visible = is_volumetric,
          },
          new renodx::utils::settings::Setting{
              .key = "AtmosphereFogHeightFalloff",
              .binding = &shader_injection->fog_height_falloff,
              .default_value = 100.f,
              .label = "Fog Height Falloff",
              .section = "Atmosphere (Advanced)",
              .tooltip = "How fast the fog thins with height. The game's fog is equally thick at every height; real haze thins as you climb. The game's thickness is kept at the map's lowest ground. 100% thins it to about a third per 1,200 units of height; 0% is the game's even fog.",
              .max = 100.f,
              .format = "%.0f%%",
              .parse = [](float value) { return value * 0.01f; },
              .is_visible = is_volumetric,
          },
          new renodx::utils::settings::Setting{
              .key = "AtmosphereFogUnevenness",
              .binding = &shader_injection->fog_unevenness,
              .default_value = 100.f,
              .label = "Fog Unevenness",
              .section = "Atmosphere (Advanced)",
              .tooltip = "How uneven thick fog and blowing dust are: drifting banks and wisps around the game's fog amount. It applies only where the game's fog is thick (fog, sandstorms, a little in rain at night); clear-weather haze stays even. 0% is the game's even fog.",
              .max = 100.f,
              .format = "%.0f%%",
              .parse = [](float value) { return value * 0.01f; },
              .is_visible = is_volumetric,
          },
          new renodx::utils::settings::Setting{
              .key = "AtmosphereFogLocalLights",
              .binding = &shader_injection->fog_local_lights,
              .default_value = 100.f,
              .label = "Local Lights in Fog",
              .section = "Atmosphere (Advanced)",
              .tooltip = "How strongly lamps, spot lights and other local lights light the fog around them, which the game's fog does not do. 100% is what fog of this thickness scatters of the light the lamps put on surfaces; 0% turns it off. Needs Shadows set to Enhanced, which gathers the lights.",
              .max = 100.f,
              .format = "%.0f%%",
              .is_enabled = [shader_injection] { return shader_injection->shadows_mode >= 0.5f; },
              .parse = [](float value) { return value * 0.01f; },
              .is_visible = is_volumetric,
          },
          new renodx::utils::settings::Setting{
              .key = "AtmosphereFogTerrainShadows",
              .binding = &shader_injection->fog_terrain_shadows,
              .value_type = renodx::utils::settings::SettingValueType::BOOLEAN,
              .default_value = 1.f,
              .label = "Terrain Shadows",
              .section = "Atmosphere (Advanced)",
              .tooltip = "Lets hills and mountains shade the fog and haze across the whole map, far past the reach of the shadow maps. Most visible at a low sun or moon. Off keeps only the nearby shadows.",
              .labels = {"Off", "On"},
              .is_visible = is_volumetric,
          },
          new renodx::utils::settings::Setting{
              .key = "AtmosphereFogDebugView",
              .binding = &shader_injection->fog_debug_view,
              .value_type = renodx::utils::settings::SettingValueType::INTEGER,
              .default_value = 0.f,
              .label = "Fog Debug View",
              .section = "Atmosphere (Advanced)",
              .tooltip = "Over a dimmed scene: Shadows shows how much of the sun's or moon's light reaches the fog at each pixel's distance, white where it is lit and black in shadow. Local Lights shows only the light that lamps and spot lights add to the fog.",
              .labels = {"Off", "Shadows", "Local Lights"},
              .is_visible = is_volumetric,
          },
      });
}

inline void Destroy() {
  const std::unique_lock lock(state.mutex);
  state.fog_constants = {};
  state.sun_constants = {};
  state.terrain_constants = {};
  state.engine_volume.Reset();
  state.rain_occlusion.Reset();
  state.shadow_map.Reset();
  state.cloud_shadow.Reset();
  state.world_shadow_mask.Reset();
  state.fog_scenes = {};
  state.shadow_scenes = {};
  state.terrain_tiles.Reset();
  state.terrain_atlas.Reset();
  state.terrain_scenes = {};
  state.volume.Reset();
  state.volume_output.Reset();
  state.estimate_output.Reset();
  state.estimate_input.Reset();
  state.visibility_outputs = {};
  state.visibility_inputs = {};
  state.particle_visibility.Reset();
  state.particle_visibility_input.Reset();
  state.particle_visibility_filled = false;
  state.terrain_heights_output.Reset();
  state.terrain_heights_input.Reset();
  state.terrain_shadow_output.Reset();
  state.terrain_shadow_input.Reset();
  state.light_bins_output.Reset();
  state.light_bins_input.Reset();
  state.spot_shadows_output.Reset();
  state.spot_shadows_input.Reset();
  state.spot_shadow_records.Reset();
  state.spot_shadow_records_input.Reset();
  state.spot_shadow_tiles = {};
  state.spot_shadow_scene = 0u;
  state.spot_shadow_count = 0u;
  state.noise.Reset();
  state.previous_scene.Reset();
  state.pass_constants.Reset();
  state.visibility_shader.Reset();
  state.volume_shader.Reset();
  state.terrain_heights_shader.Reset();
  state.terrain_shadow_shader.Reset();
  state.light_bins_shader.Reset();
  state.spot_shadow_shader.Reset();
  state.linear_clamp.Reset();
  state.less_linear_clamp.Reset();
  state.linear_wrap.Reset();
  state.history = false;
}

// ID3D11DeviceContext1's PSGetConstantBuffers1 or VSGetConstantBuffers1.
using BoundConstants = void (STDMETHODCALLTYPE ID3D11DeviceContext1::*)(UINT, UINT, ID3D11Buffer**, UINT*, UINT*);

// Records copies of one stage's bound constants into buffers that persist,
// one per range. False when a range is not bound in full. Call with
// state.mutex held.
inline bool CopyConstants(ID3D11DeviceContext* context, BoundConstants bound, std::span<const ConstantRange> ranges,
                          std::span<Microsoft::WRL::ComPtr<ID3D11Buffer>> copies) {
  Microsoft::WRL::ComPtr<ID3D11DeviceContext1> ranged;
  if (FAILED(context->QueryInterface(IID_PPV_ARGS(&ranged)))) return false;
  Microsoft::WRL::ComPtr<ID3D11Device> device;
  context->GetDevice(&device);
  for (size_t index = 0u; index < ranges.size(); ++index) {
    const auto& range = ranges[index];
    Microsoft::WRL::ComPtr<ID3D11Buffer> source;
    UINT first = 0u;
    UINT count = 0u;
    (ranged.Get()->*bound)(range.slot, 1u, &source, &first, &count);
    if (source == nullptr) return false;
    D3D11_BUFFER_DESC source_description;
    source->GetDesc(&source_description);
    if (first > source_description.ByteWidth / 16u || source_description.ByteWidth - (first * 16u) < range.bytes) return false;
    if (copies[index] == nullptr) {
      // Zeros until the list holding the first copy has run; the passes
      // treat zeros as a draw not yet seen.
      const std::vector<uint8_t> zeros(range.bytes);
      const D3D11_BUFFER_DESC copy = {.ByteWidth = range.bytes, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_CONSTANT_BUFFER};
      const D3D11_SUBRESOURCE_DATA initial = {.pSysMem = zeros.data()};
      if (FAILED(device->CreateBuffer(&copy, &initial, &copies[index]))) return false;
    }
    const D3D11_BOX box = {.left = first * 16u, .top = 0u, .front = 0u, .right = (first * 16u) + range.bytes, .bottom = 1u, .back = 1u};
    context->CopySubresourceRegion(copies[index].Get(), 0u, 0u, 0u, 0u, source.Get(), 0u, &box);
  }
  return true;
}

inline bool OnVolumeDraw(reshade::api::command_list* cmd_list, bool fog_material) {
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
  context->OMGetRenderTargets(1u, &target, nullptr);
  if (target == nullptr) return true;
  Microsoft::WRL::ComPtr<ID3D11Resource> resource;
  target->GetResource(&resource);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  if (FAILED(resource.As(&texture))) return true;
  D3D11_TEXTURE2D_DESC description;
  texture->GetDesc(&description);
  UINT viewport_count = 1u;
  D3D11_VIEWPORT viewport = {};
  context->RSGetViewports(&viewport_count, &viewport);
  if (viewport_count == 0u) return true;
  engine_viewport = viewport;
  viewport.Width = static_cast<float>(description.Width);
  viewport.Height = static_cast<float>(description.Height);
  context->RSSetViewports(1u, &viewport);

  // Keep the main view's draw inputs for the passes that fill its volume.
  const auto owner = engine::views::GetScope().owner;
  if (!fog_material || !volumetric.load(std::memory_order_acquire) || !engine::views::IsPresentedMainView(owner)) return true;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> rain_occlusion;
  context->PSGetShaderResources(0u, 1u, &rain_occlusion);
  const std::unique_lock lock(state.mutex);
  if (rain_occlusion == nullptr || !CopyConstants(context, &ID3D11DeviceContext1::PSGetConstantBuffers1, FOG_DRAW_RANGES, state.fog_constants)) return true;
  state.rain_occlusion = std::move(rain_occlusion);
  state.engine_volume = texture;
  state.fog_scenes.Add(owner.scene);
  return true;
}

inline void OnVolumeDrawn(reshade::api::command_list* cmd_list) {
  if (!engine_viewport.has_value()) return;
  reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native())->RSSetViewports(1u, &*engine_viewport);  // NOLINT(performance-no-int-to-ptr)
  engine_viewport.reset();
}

// Called for the main view's sun or moon draw inside the cascades' reach,
// which binds the shadow maps (t3) and the cloud shadow (t4).
// SSLighting2_Sun_LargestCascade also binds the world shadow mask its shadows
// fade to (t13).
inline void OnShadowedSunDraw(reshade::api::command_list* cmd_list, bool has_world_shadow_mask) {
  const auto owner = engine::views::GetScope().owner;
  if (!volumetric.load(std::memory_order_acquire) || !engine::views::IsPresentedMainView(owner)) return;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shadow_map;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cloud_shadow;
  context->PSGetShaderResources(3u, 1u, &shadow_map);
  context->PSGetShaderResources(4u, 1u, &cloud_shadow);
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> world_shadow_mask;
  if (has_world_shadow_mask) {
    context->PSGetShaderResources(13u, 1u, &world_shadow_mask);
  }
  const std::unique_lock lock(state.mutex);
  if (shadow_map == nullptr || !CopyConstants(context, &ID3D11DeviceContext1::PSGetConstantBuffers1, SUN_DRAW_RANGES, state.sun_constants)) return;
  state.shadow_map = std::move(shadow_map);
  state.cloud_shadow = std::move(cloud_shadow);
  state.world_shadow_mask = std::move(world_shadow_mask);
  state.shadow_scenes.Add(owner.scene);
}

// Draw callback of the terrain depth and shadow shaders, in any view. Every
// terrain draw binds the same tile map (VS t0) and height atlas (VS t1), and
// map-wide values in its material and object constants, so one draw a few
// times a second keeps them current. A main view build that draws terrain is
// marked as one, which stays true for every replay of its lists; a map
// without terrain builds none so marked.
inline bool OnTerrainDraw(reshade::api::command_list* cmd_list) {
  if (!volumetric.load(std::memory_order_acquire)) return true;
  if (const auto owner = engine::views::GetScope().owner; engine::views::IsPresentedMainView(owner) && terrain_scene.exchange(owner.scene, std::memory_order_relaxed) != owner.scene) {
    const std::unique_lock lock(state.mutex);
    state.terrain_scenes.Add(owner.scene);
  }
  const auto now = std::chrono::steady_clock::now();
  auto tried = terrain_capture_tried.load(std::memory_order_relaxed);
  if (now.time_since_epoch().count() - tried < std::chrono::steady_clock::duration(TERRAIN_CAPTURE_INTERVAL).count()
      || !terrain_capture_tried.compare_exchange_strong(tried, now.time_since_epoch().count(), std::memory_order_relaxed)) {
    return true;
  }
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tiles;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> atlas;
  context->VSGetShaderResources(0u, 1u, &tiles);
  context->VSGetShaderResources(1u, 1u, &atlas);
  const std::unique_lock lock(state.mutex);
  if (tiles == nullptr || atlas == nullptr || !CopyConstants(context, &ID3D11DeviceContext1::VSGetConstantBuffers1, TERRAIN_DRAW_RANGES, state.terrain_constants)) return true;
  state.terrain_tiles = std::move(tiles);
  state.terrain_atlas = std::move(atlas);
  return true;
}

// Draw callbacks of the lit particle shaders the game draws without the sun's
// shadow maps. In the main view, once the passes have run, the draw gets the
// visibility image and the port that reads it; otherwise the game's shader
// stays.
inline bool OnParticleDraw(reshade::api::command_list* cmd_list) {
  const auto owner = engine::views::GetScope().owner;
  if (!volumetric.load(std::memory_order_acquire) || !engine::views::IsPresentedMainView(owner)) return true;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> visibility;
  {
    // Only in a view build whose volume the passes fill: another build's
    // image would be an old frame's.
    const std::shared_lock lock(state.mutex);
    if (state.particle_visibility_filled && state.fog_scenes.Has(owner.scene)) {
      visibility = state.particle_visibility_input;
    }
  }
  if (visibility == nullptr) return true;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  context->PSGetShaderResources(PARTICLE_VISIBILITY_SLOT, 1u, displaced_particle_input.emplace().GetAddressOf());
  context->PSSetShaderResources(PARTICLE_VISIBILITY_SLOT, 1u, visibility.GetAddressOf());
  return true;
}

inline bool IsParticleInputBound(reshade::api::command_list* /*cmd_list*/) {
  return displaced_particle_input.has_value();
}

inline void OnParticleDrawn(reshade::api::command_list* cmd_list) {
  if (!displaced_particle_input.has_value()) return;
  reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native())->PSSetShaderResources(PARTICLE_VISIBILITY_SLOT, 1u, displaced_particle_input->GetAddressOf());  // NOLINT(performance-no-int-to-ptr)
  displaced_particle_input.reset();
}

// Called for the main view's draws of a shadowed spot light. Copies the light's
// shadow map, which the next shadowed light overwrites, into a tile of the
// fog's atlas, and the light's position and shadow projection into the
// tile's record, both in the draw's command list.
inline void OnShadowedSpotDraw(reshade::api::command_list* cmd_list) {
  const auto owner = engine::views::GetScope().owner;
  if (!volumetric.load(std::memory_order_acquire) || !engine::views::IsPresentedMainView(owner)) return;
  auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());  // NOLINT(performance-no-int-to-ptr)
  // A light draws twice with this shader: first its stencil with color
  // writes off, then its light. One copy serves.
  Microsoft::WRL::ComPtr<ID3D11BlendState> blend;
  UINT sample_mask = 0u;
  context->OMGetBlendState(&blend, nullptr, &sample_mask);
  if (blend != nullptr) {
    D3D11_BLEND_DESC description = {};
    blend->GetDesc(&description);
    if ((description.RenderTarget[0].RenderTargetWriteMask & 0x7u) == 0u) return;
  }
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shadow_map;
  context->PSGetShaderResources(3u, 1u, &shadow_map);
  Microsoft::WRL::ComPtr<ID3D11DeviceContext1> ranged;
  if (shadow_map == nullptr || FAILED(context->QueryInterface(IID_PPV_ARGS(&ranged)))) return;
  // The draw's scene (b2) and light (b3) constants.
  std::array<ID3D11Buffer*, 2> bound = {};
  std::array<UINT, 2> first = {};
  std::array<UINT, 2> counts = {};
  ranged->PSGetConstantBuffers1(2u, static_cast<UINT>(bound.size()), bound.data(), first.data(), counts.data());
  std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, 2> constants;
  constants[0].Attach(bound[0]);
  constants[1].Attach(bound[1]);
  if (constants[0] == nullptr || constants[1] == nullptr) return;
  for (size_t index = 0u; index < constants.size(); ++index) {
    D3D11_BUFFER_DESC description = {};
    constants[index]->GetDesc(&description);
    if ((static_cast<uint64_t>(first[index]) * 16u) + (index == 0u ? 256u : 64u) > description.ByteWidth) return;
  }
  Microsoft::WRL::ComPtr<ID3D11ComputeShader> shader;
  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> output;
  Microsoft::WRL::ComPtr<ID3D11Buffer> records;
  Microsoft::WRL::ComPtr<ID3D11Buffer> tile_constants;
  UINT tile = 0u;
  {
    const std::unique_lock lock(state.mutex);
    if (state.controls.local_lights <= 0.f || state.spot_shadow_shader == nullptr || state.spot_shadows_output == nullptr
        || state.spot_shadow_records == nullptr) return;
    if (state.spot_shadow_scene != owner.scene) {
      state.spot_shadow_scene = owner.scene;
      state.spot_shadow_count = 0u;
    }
    if (state.spot_shadow_count == SPOT_SHADOWS) return;
    tile = state.spot_shadow_count++;
    shader = state.spot_shadow_shader;
    output = state.spot_shadows_output;
    records = state.spot_shadow_records;
    tile_constants = state.spot_shadow_tiles[tile];
  }
  // The record: the light's position (b3 vector 3), then its shadow
  // projection (b2 vectors 12 to 15).
  const D3D11_BOX position = {.left = (first[1] * 16u) + 48u, .top = 0u, .front = 0u, .right = (first[1] * 16u) + 64u, .bottom = 1u, .back = 1u};
  context->CopySubresourceRegion(records.Get(), 0u, tile * SPOT_SHADOW_RECORD_BYTES, 0u, 0u, constants[1].Get(), 0u, &position);
  const D3D11_BOX projection = {.left = (first[0] * 16u) + 192u, .top = 0u, .front = 0u, .right = (first[0] * 16u) + 256u, .bottom = 1u, .back = 1u};
  context->CopySubresourceRegion(records.Get(), 0u, (tile * SPOT_SHADOW_RECORD_BYTES) + 16u, 0u, 0u, constants[0].Get(), 0u, &projection);
  const auto saved = rendering::d3d11_compute_state::Capture(cmd_list);
  const std::array<ID3D11UnorderedAccessView*, 1> no_output = {};
  context->CSSetShader(shader.Get(), nullptr, 0u);
  context->CSSetConstantBuffers(11u, 1u, tile_constants.GetAddressOf());
  context->CSSetShaderResources(0u, 1u, shadow_map.GetAddressOf());
  context->CSSetUnorderedAccessViews(0u, 1u, output.GetAddressOf(), nullptr);
  context->Dispatch(SPOT_SHADOW_TEXELS / 8u, SPOT_SHADOW_TEXELS / 8u, 1u);
  context->CSSetUnorderedAccessViews(0u, 1u, no_output.data(), nullptr);
  rendering::d3d11_compute_state::Restore(cmd_list, saved);
}

// Called at the first skin-scatter pass. Cuts the list there and fills the
// main view's volume when the cut executes.
inline void Apply(reshade::api::command_list* cmd_list) {
  const auto owner = engine::views::GetScope().owner;
  if (!volumetric.load(std::memory_order_acquire) || !engine::views::IsPresentedMainView(owner)) return;
  // The game replays its lists for many frames; a replay runs the recorded
  // copies of the draws' constants again, so the job runs on each execution
  // with the view build it was recorded for.
  const auto fill = [scene = owner.scene](reshade::api::command_list* immediate, const rendering::submission::Execution& /*execution*/) {
    const std::unique_lock lock(state.mutex);
    if (!volumetric.load(std::memory_order_acquire) || !state.fog_scenes.Has(scene) || state.engine_volume == nullptr) return;
    auto* context = reinterpret_cast<ID3D11DeviceContext*>(immediate->get_native());  // NOLINT(performance-no-int-to-ptr)
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    context->GetDevice(&device);
    D3D11_TEXTURE2D_DESC description;
    state.engine_volume->GetDesc(&description);
    if (description.Format != DXGI_FORMAT_R16G16B16A16_FLOAT && description.Format != DXGI_FORMAT_R16G16B16A16_TYPELESS) {
      static std::atomic_flag reported = {};
      if (!reported.test_and_set()) {
        reshade::log::message(reshade::log::level::warning, "MgsvFog: the engine's fog volume is not a 16-bit float image; the game's fog stays.");
      }
      return;
    }

    // 1. Resources: the images at the volume's size, the shaders, then what
    // is made once.
    D3D11_TEXTURE2D_DESC current = {};
    if (state.volume != nullptr) {
      state.volume->GetDesc(&current);
    }
    if (current.Width != description.Width || current.Height != description.Height) {
      D3D11_TEXTURE2D_DESC created = {
          .Width = description.Width,
          .Height = description.Height,
          .MipLevels = 1u,
          .ArraySize = 1u,
          .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
          .SampleDesc = {.Count = 1u},
          .Usage = D3D11_USAGE_DEFAULT,
          .BindFlags = D3D11_BIND_UNORDERED_ACCESS,
      };
      state.volume.Reset();
      state.history = false;
      bool failed = FAILED(device->CreateTexture2D(&created, nullptr, &state.volume))
                    || FAILED(device->CreateUnorderedAccessView(state.volume.Get(), nullptr, state.volume_output.ReleaseAndGetAddressOf()));
      // The views keep their images.
      Microsoft::WRL::ComPtr<ID3D11Texture2D> image;
      created.Format = DXGI_FORMAT_R8_UNORM;
      created.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
      failed = failed
               || FAILED(device->CreateTexture2D(&created, nullptr, image.ReleaseAndGetAddressOf()))
               || FAILED(device->CreateUnorderedAccessView(image.Get(), nullptr, state.estimate_output.ReleaseAndGetAddressOf()))
               || FAILED(device->CreateShaderResourceView(image.Get(), nullptr, state.estimate_input.ReleaseAndGetAddressOf()));
      // 16 bits, so a blend that keeps most of the old value still moves it.
      created.Format = DXGI_FORMAT_R16_UNORM;
      for (size_t index = 0u; index < state.visibility_outputs.size() && !failed; ++index) {
        failed = FAILED(device->CreateTexture2D(&created, nullptr, image.ReleaseAndGetAddressOf()))
                 || FAILED(device->CreateUnorderedAccessView(image.Get(), nullptr, state.visibility_outputs[index].ReleaseAndGetAddressOf()))
                 || FAILED(device->CreateShaderResourceView(image.Get(), nullptr, state.visibility_inputs[index].ReleaseAndGetAddressOf()));
      }
      state.particle_visibility_filled = false;
      failed = failed
               || FAILED(device->CreateTexture2D(&created, nullptr, state.particle_visibility.ReleaseAndGetAddressOf()))
               || FAILED(device->CreateShaderResourceView(state.particle_visibility.Get(), nullptr, state.particle_visibility_input.ReleaseAndGetAddressOf()));
      // A row per bin of the volume's cells, an entry per kept light. 32
      // bits, the one integer size a compute shader may read back.
      created.Format = DXGI_FORMAT_R32_UINT;
      created.Width = BIN_LIGHTS;
      created.Height = (((description.Width / 32u) + BIN_CELLS - 1u) / BIN_CELLS) * (((description.Height / 4u) + BIN_CELLS - 1u) / BIN_CELLS) * (128u / BIN_SLICES);
      failed = failed
               || FAILED(device->CreateTexture2D(&created, nullptr, image.ReleaseAndGetAddressOf()))
               || FAILED(device->CreateUnorderedAccessView(image.Get(), nullptr, state.light_bins_output.ReleaseAndGetAddressOf()))
               || FAILED(device->CreateShaderResourceView(image.Get(), nullptr, state.light_bins_input.ReleaseAndGetAddressOf()));
      if (failed) {
        static std::atomic_flag reported = {};
        if (!reported.test_and_set()) {
          reshade::log::message(reshade::log::level::error, "MgsvFog: could not create the fog volume images; the game's fog stays.");
        }
        state.volume.Reset();
        return;
      }
    }
    if (const uint32_t revision = rendering::live_shaders::Revision(); state.volume_shader == nullptr || state.shader_revision != revision) {
      struct Source {
        const char* name;
        std::span<const uint8_t> embedded;
        Microsoft::WRL::ComPtr<ID3D11ComputeShader>* shader;
      };
      const std::array<Source, 6> sources = {{
          {.name = "fog_visibility", .embedded = __fog_visibility, .shader = &state.visibility_shader},
          {.name = "fog_volume", .embedded = __fog_volume, .shader = &state.volume_shader},
          {.name = "fog_terrain_heights", .embedded = __fog_terrain_heights, .shader = &state.terrain_heights_shader},
          {.name = "fog_terrain_shadow", .embedded = __fog_terrain_shadow, .shader = &state.terrain_shadow_shader},
          {.name = "fog_light_bins", .embedded = __fog_light_bins, .shader = &state.light_bins_shader},
          {.name = "fog_spot_shadow", .embedded = __fog_spot_shadow, .shader = &state.spot_shadow_shader},
      }};
      std::array<Microsoft::WRL::ComPtr<ID3D11ComputeShader>, 6> created;
      bool failed = false;
      for (size_t index = 0u; index < sources.size() && !failed; ++index) {
        const auto code = rendering::live_shaders::Bytes(sources[index].name, sources[index].embedded);
        failed = FAILED(device->CreateComputeShader(code.data(), code.size(), nullptr, &created[index]));
      }
      if (failed) {
        static std::atomic_flag reported = {};
        if (!reported.test_and_set()) {
          reshade::log::message(reshade::log::level::error, "MgsvFog: could not create the fog shaders.");
        }
      } else {
        for (size_t index = 0u; index < sources.size(); ++index) {
          *sources[index].shader = std::move(created[index]);
        }
      }
      // Replacement shaders that fail keep the ones they would have replaced.
      state.shader_revision = revision;
    }
    if (state.linear_clamp == nullptr) {
      D3D11_SAMPLER_DESC sampler = {
          .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
          .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
          .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
          .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
          .ComparisonFunc = D3D11_COMPARISON_NEVER,
          .MaxLOD = D3D11_FLOAT32_MAX,
      };
      const D3D11_BUFFER_DESC previous_scene = {.ByteWidth = FOG_DRAW_RANGES[1].bytes, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_CONSTANT_BUFFER};
      const D3D11_BUFFER_DESC pass_constants = {.ByteWidth = sizeof(PassConstants), .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_CONSTANT_BUFFER};
      bool failed = FAILED(device->CreateSamplerState(&sampler, &state.linear_clamp))
                    || FAILED(device->CreateBuffer(&previous_scene, nullptr, state.previous_scene.ReleaseAndGetAddressOf()))
                    || FAILED(device->CreateBuffer(&pass_constants, nullptr, state.pass_constants.ReleaseAndGetAddressOf()));
      sampler.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
      sampler.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
      sampler.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
      failed = failed || FAILED(device->CreateSamplerState(&sampler, state.linear_wrap.ReleaseAndGetAddressOf()));
      // The game compares its shadow maps the same way: a lookup returns the
      // share of the footprint whose stored depth is greater than the
      // reference, which is nearer the light.
      sampler.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
      sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
      sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
      sampler.ComparisonFunc = D3D11_COMPARISON_LESS;
      failed = failed || FAILED(device->CreateSamplerState(&sampler, state.less_linear_clamp.ReleaseAndGetAddressOf()));
      // Heights as the engine's atlas stores them, and the shadow's top in
      // the same measure beside the distance to the ground that casts it.
      D3D11_TEXTURE2D_DESC terrain = {
          .Width = TERRAIN_TEXELS,
          .Height = TERRAIN_TEXELS,
          .MipLevels = 1u,
          .ArraySize = 1u,
          .Format = DXGI_FORMAT_R16_UNORM,
          .SampleDesc = {.Count = 1u},
          .Usage = D3D11_USAGE_DEFAULT,
          .BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
      };
      Microsoft::WRL::ComPtr<ID3D11Texture2D> image;
      failed = failed
               || FAILED(device->CreateTexture2D(&terrain, nullptr, image.ReleaseAndGetAddressOf()))
               || FAILED(device->CreateUnorderedAccessView(image.Get(), nullptr, state.terrain_heights_output.ReleaseAndGetAddressOf()))
               || FAILED(device->CreateShaderResourceView(image.Get(), nullptr, state.terrain_heights_input.ReleaseAndGetAddressOf()));
      terrain.Format = DXGI_FORMAT_R16G16_UNORM;
      failed = failed
               || FAILED(device->CreateTexture2D(&terrain, nullptr, image.ReleaseAndGetAddressOf()))
               || FAILED(device->CreateUnorderedAccessView(image.Get(), nullptr, state.terrain_shadow_output.ReleaseAndGetAddressOf()))
               || FAILED(device->CreateShaderResourceView(image.Get(), nullptr, state.terrain_shadow_input.ReleaseAndGetAddressOf()));
      // The spot shadow atlas: stored depths as the game's maps hold them,
      // which are small and close together far from the light, so half
      // floats. Then its records, zeroed, and each tile's index.
      D3D11_TEXTURE2D_DESC atlas = terrain;
      atlas.Width = SPOT_SHADOW_TILES * SPOT_SHADOW_TEXELS;
      atlas.Height = SPOT_SHADOW_TILES * SPOT_SHADOW_TEXELS;
      atlas.Format = DXGI_FORMAT_R16_FLOAT;
      failed = failed
               || FAILED(device->CreateTexture2D(&atlas, nullptr, image.ReleaseAndGetAddressOf()))
               || FAILED(device->CreateUnorderedAccessView(image.Get(), nullptr, state.spot_shadows_output.ReleaseAndGetAddressOf()))
               || FAILED(device->CreateShaderResourceView(image.Get(), nullptr, state.spot_shadows_input.ReleaseAndGetAddressOf()));
      const std::array<float, SPOT_SHADOWS * SPOT_SHADOW_RECORD_BYTES / 4u> no_records = {};
      const D3D11_BUFFER_DESC records = {.ByteWidth = sizeof(no_records), .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_SHADER_RESOURCE};
      const D3D11_SUBRESOURCE_DATA records_data = {.pSysMem = no_records.data()};
      D3D11_SHADER_RESOURCE_VIEW_DESC records_view = {.Format = DXGI_FORMAT_R32G32B32A32_FLOAT, .ViewDimension = D3D11_SRV_DIMENSION_BUFFER};
      records_view.Buffer.NumElements = sizeof(no_records) / 16u;
      failed = failed
               || FAILED(device->CreateBuffer(&records, &records_data, state.spot_shadow_records.ReleaseAndGetAddressOf()))
               || FAILED(device->CreateShaderResourceView(state.spot_shadow_records.Get(), &records_view, state.spot_shadow_records_input.ReleaseAndGetAddressOf()));
      for (uint32_t tile = 0u; tile < SPOT_SHADOWS && !failed; ++tile) {
        const std::array<uint32_t, 4> index = {tile};
        const D3D11_BUFFER_DESC constants = {.ByteWidth = sizeof(index), .Usage = D3D11_USAGE_IMMUTABLE, .BindFlags = D3D11_BIND_CONSTANT_BUFFER};
        const D3D11_SUBRESOURCE_DATA constants_data = {.pSysMem = index.data()};
        failed = FAILED(device->CreateBuffer(&constants, &constants_data, state.spot_shadow_tiles[tile].ReleaseAndGetAddressOf()));
      }

      // The noise for uneven fog: a volume that repeats in every direction,
      // with its smaller copies. Three layers of smooth random values on
      // lattices of 8, 16 and 32 points a side, the finer ones weaker, then
      // stretched about a mean of a half.
      // Kept if something else here fails and this block runs again.
      if (state.noise == nullptr) {
        std::vector<float> values(size_t{NOISE_TEXELS} * NOISE_TEXELS * NOISE_TEXELS);
        double sum = 0.;
        for (uint32_t z = 0u; z < NOISE_TEXELS; ++z) {
          for (uint32_t y = 0u; y < NOISE_TEXELS; ++y) {
            for (uint32_t x = 0u; x < NOISE_TEXELS; ++x) {
              float value = 0.f;
              float weight = 4.f / 7.f;
              for (uint32_t period = 8u; period <= 32u; period *= 2u) {
                const float scale = static_cast<float>(period) / static_cast<float>(NOISE_TEXELS);
                const std::array<float, 3> place = {static_cast<float>(x) * scale, static_cast<float>(y) * scale, static_cast<float>(z) * scale};
                const std::array<uint32_t, 3> corner = {static_cast<uint32_t>(place[0]), static_cast<uint32_t>(place[1]), static_cast<uint32_t>(place[2])};
                std::array<float, 3> blend = {};
                for (size_t axis = 0u; axis < blend.size(); ++axis) {
                  const float fraction = place[axis] - static_cast<float>(corner[axis]);
                  blend[axis] = fraction * fraction * (3.f - (2.f * fraction));
                }
                float layer = 0.f;
                for (uint32_t offset = 0u; offset < 8u; ++offset) {
                  // The lattice point's random value: a hash of its place,
                  // wrapped at the lattice's side so the volume repeats.
                  uint32_t hash = (((corner[0] + (offset & 1u)) % period) * 73856093u)
                                  ^ (((corner[1] + ((offset >> 1u) & 1u)) % period) * 19349663u)
                                  ^ (((corner[2] + (offset >> 2u)) % period) * 83492791u)
                                  ^ (period * 2654435761u);
                  hash ^= hash >> 15u;
                  hash *= 0x2C1B3C6Du;
                  hash ^= hash >> 12u;
                  hash *= 0x297A2D39u;
                  hash ^= hash >> 15u;
                  layer += (static_cast<float>(hash & 0xFFFFFFu) / 16777215.f)
                           * ((offset & 1u) != 0u ? blend[0] : 1.f - blend[0])
                           * (((offset >> 1u) & 1u) != 0u ? blend[1] : 1.f - blend[1])
                           * ((offset >> 2u) != 0u ? blend[2] : 1.f - blend[2]);
                }
                value += layer * weight;
                weight *= 0.5f;
              }
              values[(((size_t{z} * NOISE_TEXELS) + y) * NOISE_TEXELS) + x] = value;
              sum += value;
            }
          }
        }
        const auto mean = static_cast<float>(sum / static_cast<double>(values.size()));
        std::vector<std::vector<uint8_t>> levels;
        std::vector<D3D11_SUBRESOURCE_DATA> level_data;
        for (uint32_t side = NOISE_TEXELS; side != 0u; side /= 2u) {
          auto& level = levels.emplace_back(size_t{side} * side * side);
          for (size_t texel = 0u; texel < level.size(); ++texel) {
            level[texel] = static_cast<uint8_t>(std::lround(std::clamp(0.5f + ((values[texel] - mean) * 2.5f), 0.f, 1.f) * 255.f));
          }
          level_data.push_back({.pSysMem = level.data(), .SysMemPitch = side, .SysMemSlicePitch = side * side});
          // The next level's values: each the mean of eight of this level's.
          const uint32_t half = side / 2u;
          for (uint32_t z = 0u; z < half; ++z) {
            for (uint32_t y = 0u; y < half; ++y) {
              for (uint32_t x = 0u; x < half; ++x) {
                float total = 0.f;
                for (uint32_t offset = 0u; offset < 8u; ++offset) {
                  total += values[(((size_t{(2u * z) + (offset >> 2u)} * side) + (2u * y) + ((offset >> 1u) & 1u)) * side) + (2u * x) + (offset & 1u)];
                }
                values[(((size_t{z} * half) + y) * half) + x] = total / 8.f;
              }
            }
          }
        }
        const D3D11_TEXTURE3D_DESC noise = {
            .Width = NOISE_TEXELS,
            .Height = NOISE_TEXELS,
            .Depth = NOISE_TEXELS,
            .MipLevels = static_cast<UINT>(levels.size()),
            .Format = DXGI_FORMAT_R8_UNORM,
            .Usage = D3D11_USAGE_IMMUTABLE,
            .BindFlags = D3D11_BIND_SHADER_RESOURCE,
        };
        Microsoft::WRL::ComPtr<ID3D11Texture3D> noise_volume;
        // Made whatever failed before it, so a later attempt finds it.
        const bool noise_failed = FAILED(device->CreateTexture3D(&noise, level_data.data(), &noise_volume))
                                  || FAILED(device->CreateShaderResourceView(noise_volume.Get(), nullptr, state.noise.ReleaseAndGetAddressOf()));
        failed = failed || noise_failed;
      }
      if (failed) {
        static std::atomic_flag reported = {};
        if (!reported.test_and_set()) {
          reshade::log::message(reshade::log::level::error, "MgsvFog: could not create the fog passes' samplers, buffers, noise or terrain and shadow images; the game's fog stays.");
        }
        state.linear_clamp.Reset();
      }
    }
    if (state.volume_shader == nullptr || state.linear_clamp == nullptr) return;

    // 2. What this view build recorded. Without a shadowed sun draw and
    // without terrain every cell is lit, which gives the game's fog.
    const bool shadow_maps = state.shadow_scenes.Has(scene) && state.shadow_map != nullptr;
    const bool terrain_map = state.terrain_scenes.Has(scene) && state.terrain_tiles != nullptr;
    const bool terrain = state.controls.terrain_shadows && terrain_map;
    // The local lights of this view build, if Enhanced shadows gathered any:
    // their batch fills its light buffer before the lights draw, which is
    // before this point of the frame.
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> lights;
    if (auto* gathered = immediate->get_device()->get_private_data<shadows::batch::DeviceState>(); gathered != nullptr && state.controls.local_lights > 0.f) {
      const std::shared_lock gathered_lock(gathered->mutex);
      if (gathered->frame_storage != nullptr && gathered->frame_storage->lights_scene.load(std::memory_order_acquire) == scene) {
        lights = gathered->frame_storage->light_srv;
      }
    }
    if (terrain) {
      static std::atomic_flag reported = {};
      if (!reported.test_and_set()) {
        reshade::log::message(reshade::log::level::info, "MgsvFog: the main view draws terrain; its shadows reach the fog.");
      }
    }
    const PassConstants constants = {
        .frame = state.frame,
        .flags = (shadow_maps ? FLAG_SHADOW_MAPS : 0u)
                 | (shadow_maps && state.cloud_shadow != nullptr ? FLAG_CLOUD_SHADOW : 0u)
                 | (state.history ? FLAG_HISTORY : 0u)
                 | (shadow_maps && state.world_shadow_mask != nullptr ? FLAG_WORLD_SHADOW_MASK : 0u)
                 | (terrain ? FLAG_TERRAIN : 0u)
                 | (terrain_map ? FLAG_TERRAIN_MAP : 0u)
                 | (lights != nullptr ? FLAG_LOCAL_LIGHTS : 0u),
        .shadow_strength = state.controls.shadow_strength,
        .debug_view = state.controls.debug_view,
        .local_lights = state.controls.local_lights,
        .cells_x = description.Width / 32u,
        .cells_y = description.Height / 4u,
        .height_falloff = state.controls.height_falloff,
        // Wrapped where a float still counts hundredths of a second.
        .time = static_cast<float>(std::fmod(std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(), 131072.)),
        .unevenness = state.controls.unevenness,
    };
    context->UpdateSubresource(state.pass_constants.Get(), 0u, nullptr, &constants, 0u, 0u);

    // 3. The passes. Registers follow fog_volume.hlsli and the shaders.
    const auto saved = rendering::d3d11_compute_state::Capture(immediate);
    const size_t result = state.frame & 1u;
    const std::array<ID3D11Buffer*, 11> buffers = {
        state.fog_constants[0].Get(),
        state.terrain_constants[0].Get(),
        state.fog_constants[1].Get(),
        state.sun_constants[1].Get(),
        state.fog_constants[2].Get(),
        state.fog_constants[3].Get(),
        state.sun_constants[0].Get(),
        state.sun_constants[2].Get(),
        state.terrain_constants[1].Get(),
        state.previous_scene.Get(),
        state.pass_constants.Get(),
    };
    const std::array<ID3D11SamplerState*, 3> samplers = {state.linear_clamp.Get(), state.less_linear_clamp.Get(), state.linear_wrap.Get()};
    const std::array<ID3D11UnorderedAccessView*, 2> no_outputs = {};
    context->CSSetConstantBuffers(0u, static_cast<UINT>(buffers.size()), buffers.data());
    context->CSSetSamplers(0u, static_cast<UINT>(samplers.size()), samplers.data());
    // Every pass sets the same twelve input slots, so none keeps an image a
    // later pass writes.
    const auto run = [context, &no_outputs](ID3D11ComputeShader* shader, const std::array<ID3D11ShaderResourceView*, 12>& inputs,
                                            const std::array<ID3D11UnorderedAccessView*, 2>& outputs, UINT width, UINT height) {
      context->CSSetShader(shader, nullptr, 0u);
      context->CSSetShaderResources(0u, static_cast<UINT>(inputs.size()), inputs.data());
      context->CSSetUnorderedAccessViews(0u, static_cast<UINT>(outputs.size()), outputs.data(), nullptr);
      context->Dispatch((width + 7u) / 8u, (height + 7u) / 8u, 1u);
      context->CSSetUnorderedAccessViews(0u, static_cast<UINT>(no_outputs.size()), no_outputs.data(), nullptr);
    };
    if (terrain) {
      run(state.terrain_heights_shader.Get(), {state.terrain_tiles.Get(), state.terrain_atlas.Get()}, {state.terrain_heights_output.Get()}, TERRAIN_TEXELS, TERRAIN_TEXELS);
      run(state.terrain_shadow_shader.Get(), {state.terrain_heights_input.Get()}, {state.terrain_shadow_output.Get()}, TERRAIN_TEXELS, TERRAIN_TEXELS);
    }
    run(state.visibility_shader.Get(),
        {state.shadow_map.Get(), state.cloud_shadow.Get(), nullptr, nullptr, nullptr, state.world_shadow_mask.Get(), state.terrain_shadow_input.Get()},
        {state.estimate_output.Get()}, description.Width, description.Height);
    if (lights != nullptr) {
      // One thread per bin: the bins along x and those along depth side by
      // side, by the bins along y.
      run(state.light_bins_shader.Get(), {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, lights.Get(), nullptr, state.spot_shadow_records_input.Get()},
          {state.light_bins_output.Get()},
          (((description.Width / 32u) + BIN_CELLS - 1u) / BIN_CELLS) * (128u / BIN_SLICES), ((description.Height / 4u) + BIN_CELLS - 1u) / BIN_CELLS);
    }
    // One thread per screen column: 32 x 4 tiles of them make the volume.
    run(state.volume_shader.Get(),
        {nullptr, nullptr, state.visibility_inputs[1u - result].Get(), state.estimate_input.Get(), state.rain_occlusion.Get(), nullptr, nullptr, lights.Get(),
         state.light_bins_input.Get(), state.spot_shadow_records_input.Get(), state.spot_shadows_input.Get(), state.noise.Get()},
        {state.volume_output.Get(), state.visibility_outputs[result].Get()}, description.Width / 32u, description.Height / 4u);
    // The particle ports' copy of this frame's visibility.
    Microsoft::WRL::ComPtr<ID3D11Resource> visibility;
    state.visibility_inputs[result]->GetResource(&visibility);
    context->CopyResource(state.particle_visibility.Get(), visibility.Get());
    state.particle_visibility_filled = true;
    // A tile is good for the frame its light drew in: empty the records, so
    // a light that stops drawing leaves no shadow behind.
    const std::array<float, SPOT_SHADOWS * SPOT_SHADOW_RECORD_BYTES / 4u> no_records = {};
    context->UpdateSubresource(state.spot_shadow_records.Get(), 0u, nullptr, no_records.data(), 0u, 0u);

    context->CopySubresourceRegion(state.engine_volume.Get(), 0u, 0u, 0u, 0u, state.volume.Get(), 0u, nullptr);
    context->CopyResource(state.previous_scene.Get(), state.fog_constants[1].Get());
    rendering::d3d11_compute_state::Restore(immediate, saved);
    ++state.frame;
    state.history = true;
    static std::atomic_flag reported = {};
    if (!reported.test_and_set()) {
      reshade::log::message(reshade::log::level::info, "MgsvFog: the mod fills the main view's fog volume.");
    }
  };
  if (!rendering::submission::Schedule(cmd_list, {.run = fill})) {
    static std::atomic_flag reported = {};
    if (!reported.test_and_set()) {
      reshade::log::message(reshade::log::level::warning, "MgsvFog: could not insert the fog passes; the game's fog stays in that recorded frame and its replays.");
    }
  }
}

}  // namespace mgsv::atmosphere::fog

#endif  // MGSV_ATMOSPHERE_FOG_VOLUME_HPP_
