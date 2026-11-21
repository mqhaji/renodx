// SPDX-License-Identifier: MIT
#ifndef RENODX_GAMES_MGSV_ENGINE_BUILD_HPP_
#define RENODX_GAMES_MGSV_ENGINE_BUILD_HPP_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

// Everything the mod relies on at a fixed place in the game's executable, and
// every engine structure layout it reads. No other file holds an address, a
// structure offset or the bytes expected in the game's code; features ask
// engine/image.hpp for an entry and engine::Value for a field by name. Game
// updates do not change the renderer, but a rebuilt executable moves its code,
// so every entry keeps the bytes that identify it, every reference between
// entries is checked, and every field is checked in an instruction that uses it.
namespace mgsv::engine {

// The executable the entries were read from, built 2026-07-07.
inline constexpr uint32_t IMAGE_TIMESTAMP = 0x6A4CB898u;
inline constexpr uint32_t IMAGE_SIZE = 0xA01D000u;

enum class Entry : uint8_t {
  // Renderer, view and command-list lifetimes (engine/views.hpp).
  RENDER_BUILD_RENDERING,
  DG_INIT_RENDER,
  DG_INIT_VIEWPORT,
  DG_END_VIEWPORT,
  DG_BRANCH,
  COMMAND_BUFFER_BRANCH,
  COMMAND_BUFFER_CONSTRUCTOR,
  COMMAND_BUFFER_DESTRUCTOR,
  ALLOCATE_COMMAND_LIST_NODE,
  COMMAND_LIST_NODE_BEGIN_RECORD,
  COMMAND_LIST_NODE_END_RECORD,
  VIEWPORT_DESTRUCTOR,
  RENDER_DESTRUCTOR,
  RENDER_UPDATE_VIEWPORT_LIST,
  DEPTH_OF_FIELD_MAIN_EXEC,
  GET_MAIN_VIEWPORT,

  // Render target creation (engine/render_targets.hpp).
  RENDER_TARGET_INITIALIZE,
  GET_BUFFER_FORMAT_FROM_PARAMETER,
  RESIDENT_RESOURCE_GET_HANDLE,
  MOTION_BLUR_WORK_BUFFER_CONSTRUCTOR,
  DEPTH_OF_FIELD_RESOURCES_CONSTRUCTOR,
  RESTORE_TEMPORARY_RENDER_TARGET,
  RENDER_TARGET_CREATE_RESOURCE,
  RENDER_TARGET_RESTORE_WITH_RESIZE,
  RENDER_TARGET_MANAGER,

  // Motion vectors (engine/motion_vectors.hpp).
  DEFERRED_GEOMETRY_POST_EXEC,
  DG_END_SCREEN_SPACE_VELOCITY,

  // Start of post-processing (engine/post_filter.hpp).
  DG_START_POST_FILTER,

  // Local lights and their constants (engine/local_lights.hpp).
  LOCAL_LIGHT_MAIN_EXEC,
  COMMAND_BUFFER_SET_VECTOR,
  SHADOW_CONSTANT_REGISTER_SET_VECTOR,
  SHADOW_CONSTANT_REGISTER_FLUSH,

  // Render size and depth of field at output size (reconstruction/resolution.hpp).
  DG_START,
  DEPTH_OF_FIELD_DRAW,
  DEPTH_OF_FIELD_SET_RENDER_AREA,
  DEPTH_OF_FIELD_DRAW_SPRITE_SUB,
  DEPTH_OF_FIELD_NEAR_RADIUS_CAP,
  DEPTH_OF_FIELD_SCATTER_SCALE,
  DEPTH_OF_FIELD_REFERENCE_WIDTH,

  // Projection jitter (reconstruction/projection_jitter.hpp).
  FORWARD_RENDERING_PRE_EXEC,
  REQUEST_DRAW_ALL_LOCAL_LIGHT_ON_JOB,
  LOCAL_LIGHT_REQUEST_SLOT,
  LOCAL_LIGHT_REQUEST_RETURN,
  LOCAL_LIGHT_VIEWPORT_COPY,
  CONSTANT_BUFFER_MAP_DATA,
  COMMAND_BUFFER_FLUSH_RENDER_RESOURCE,
  COMMAND_BUFFER_FLUSH_CONSTANT_REGISTER,
  MODEL_FLUSH_RENDER_RESOURCE_CALL,
  MODEL_CONSTANT_UPLOAD_RETURN,
  SCATTER_UPLOAD_RETURN,
  PROJECTION_COMMIT,
  RENDER_SET_VIEW_RETURN,
  SHADER_MANAGER_SET_VIEW_MATRIX,
  SHADER_MANAGER,
  DEFERRED_GEOMETRY_SET_VIEW_RETURN,
  MODEL_SET_VIEW_RETURN,
  ALPHA_MODEL_SET_VIEW_RETURN,
  OVERLAY_MODEL_SET_VIEW_RETURN,
  NEW_THERMOGRAPHY_SET_VIEW_RETURN,
  THERMOGRAPHY_SET_VIEW_RETURN,
  SUN_LIGHT_DRAW,
  DRAW_SHADOW_BOX,
  SUN_SHADOW_BOX_RETURN,
  SPHERICAL_HARMONICS_CALCULATE_LIGHT_VOLUME,
  SPHERICAL_HARMONICS_LIGHT_VOLUME_RETURN,
  SPHERICAL_HARMONICS_REGISTER_DRAW_ON_JOB,
  SPHERICAL_HARMONICS_SET_OUTPUT,
  SPHERICAL_HARMONICS_SET_OUTPUT_RETURN,
  SPHERICAL_HARMONICS_VOLUME_VERTEX_COUNTS,
  SKY_CALC_SHADER_PARAMETERS,
  SKY_PARAMETERS_RETURN,
  TERRAIN_DECAL_DRAW_ON_JOB,
  TERRAIN_DECAL_SET_INPUT,
  TERRAIN_DECAL_SET_INPUT_RETURN,

  // Game pause (engine/pause.hpp). Read only, never hooked.
  GAME_TIME_UPDATE,
  GET_PAUSE_SYSTEM,
  PAUSE_SYSTEM,

  // Read only to verify structure fields; never hooked.
  SHADOW_CONSTANT_REGISTER_DUPLICATE,
  RENDER_INFO_RESIZE_SWAP_CHAIN,
  SWAP_CHAIN_PRESENT,
  SHADER_RESOURCE_VIEW_CREATE,

  COUNT,
};
inline constexpr size_t ENTRY_COUNT = static_cast<size_t>(Entry::COUNT);

struct EntryInfo {
  Entry entry = Entry::COUNT;
  // The engine's name where the community decompile has one, otherwise
  // the containing function and what the entry is inside it.
  std::string_view name;
  // Address in the known build, relative to the executable's base.
  uint32_t rva = 0u;
  // The expected bytes start this far before the address: a return address is
  // identified by the call that precedes it.
  uint32_t bytes_before = 0u;
  // Hexadecimal bytes; ?? marks a byte that moves with the code, such as a
  // call target. Empty when a reference from another entry identifies it.
  std::string_view bytes;
};

inline constexpr std::array<EntryInfo, ENTRY_COUNT> ENTRIES = {{
    // Renderer, view and command-list lifetimes (engine/views.hpp).
    {.entry = Entry::RENDER_BUILD_RENDERING, .name = "fox::gr::Render::buildRendering", .rva = 0x1BEEC0u, .bytes = "40 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24"},
    {.entry = Entry::DG_INIT_RENDER, .name = "fox::gr::dg::DgDx11::InitRender", .rva = 0x2496A0u, .bytes = "40 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24"},
    {.entry = Entry::DG_INIT_VIEWPORT, .name = "fox::gr::dg::DgDx11::InitViewport", .rva = 0x249BE0u, .bytes = "40 55 56 57 41 56 41 57 48 81 EC 80 00 00 00 48"},
    {.entry = Entry::DG_END_VIEWPORT, .name = "fox::gr::dg::DgDx11::EndViewport", .rva = 0x247EB0u, .bytes = "40 53 55 56 41 56 41 57 48 81 EC 80 00 00 00 48"},
    {.entry = Entry::DG_BRANCH, .name = "fox::gr::dg::DgDx11::Branch", .rva = 0x2C5220u, .bytes = "48 89 5C 24 10 57 48 83 EC 20 F6 81 58 01 00 00"},
    {.entry = Entry::COMMAND_BUFFER_BRANCH, .name = "fox::gr::dg::CommandBuffer::Branch", .rva = 0x2D6F50u, .bytes = "48 89 5C 24 10 57 48 83 EC 20 F6 41 10 01 48"},
    {.entry = Entry::COMMAND_BUFFER_CONSTRUCTOR, .name = "fox::gr::dg::CommandBuffer::CommandBuffer", .rva = 0x2D6B80u, .bytes = "48 8B C4 57 48 83 EC 60 48 C7 40 C0 FE FF FF FF"},
    {.entry = Entry::COMMAND_BUFFER_DESTRUCTOR, .name = "fox::gr::dg::CommandBuffer::~CommandBuffer", .rva = 0x2D6E80u, .bytes = "48 89 4C 24 08 53 48 83 EC 30 48 C7 44 24 20 FE"},
    {.entry = Entry::ALLOCATE_COMMAND_LIST_NODE, .name = "fox::gr::dg::allocateCommandListNode", .rva = 0x2C8130u, .bytes = "48 89 5C 24 20 57 48 83 EC 20 8B 41 20 48 89 74"},
    {.entry = Entry::COMMAND_LIST_NODE_BEGIN_RECORD, .name = "fox::gr::dg::CommandListNode::BeginRecord", .rva = 0x2C7C30u, .bytes = "83 61 18 FE 48 8B C2 48 8B 51 10 48 8B C8 E9 2D"},
    {.entry = Entry::COMMAND_LIST_NODE_END_RECORD, .name = "fox::gr::dg::CommandListNode::EndRecord", .rva = 0x2C7D70u, .bytes = "40 53 48 83 EC 20 48 8B D9 48 8B CA E8 FF CA 72"},
    {.entry = Entry::VIEWPORT_DESTRUCTOR, .name = "fox::gr::Viewport::~Viewport", .rva = 0x1B8600u, .bytes = "48 89 4C 24 08 57 48 83 EC 30 48 C7 44 24 20 FE"},
    {.entry = Entry::RENDER_DESTRUCTOR, .name = "fox::gr::Render::~Render", .rva = 0x1BCA60u, .bytes = "48 89 4C 24 08 57 48 83 EC 30 48 C7 44 24 20 FE"},
    {.entry = Entry::RENDER_UPDATE_VIEWPORT_LIST, .name = "fox::gr::Render::UpdateViewportList", .rva = 0x1BEA90u, .bytes = "48 8B C4 55 57 41 56 48 8B EC 48 83 EC 60 48 C7"},
    {.entry = Entry::DEPTH_OF_FIELD_MAIN_EXEC, .name = "fox::gr::PluginDepthOfField::MainExec", .rva = 0x1DD1F0u, .bytes = "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 30"},
    {.entry = Entry::GET_MAIN_VIEWPORT, .name = "tpp::ef::Utility::GetMainViewport", .rva = 0x6AD870u, .bytes = "48 8B 05 09 3A 53 02 C3"},

    // Render target creation (engine/render_targets.hpp).
    {.entry = Entry::RENDER_TARGET_INITIALIZE, .name = "fox::gr::dg::RenderTargetDx11::Initialize", .rva = 0x2B2F90u, .bytes = "48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 D9 48 81 EC F0 00 00 00"},
    {.entry = Entry::GET_BUFFER_FORMAT_FROM_PARAMETER, .name = "fox::gr::dg::GetBufferFormatFromParameter", .rva = 0x2B29D0u, .bytes = "48 63 41 14 83 F8 0D 77 49"},
    {.entry = Entry::RESIDENT_RESOURCE_GET_HANDLE, .name = "fox::gr::dg::ResidentResourceManager::GetResourceHandle", .rva = 0x23BBE0u, .bytes = "48 8B D1 48 8B 0D"},
    {.entry = Entry::MOTION_BLUR_WORK_BUFFER_CONSTRUCTOR, .name = "fox::gr::WorkBuffer::WorkBuffer", .rva = 0x2C2E10u, .bytes = "48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 4C 89 74 24 20 55 48 8D 6C 24 A9 48 81 EC B0 00 00 00"},
    {.entry = Entry::DEPTH_OF_FIELD_RESOURCES_CONSTRUCTOR, .name = "fox::gr::dg::DgResources::DgResources", .rva = 0x2EFE50u, .bytes = "48 8B C4 55 41 54 41 55 41 56 41 57 48 8D A8 08 FE FF FF 48 81 EC D0 02 00 00"},
    {.entry = Entry::RESTORE_TEMPORARY_RENDER_TARGET, .name = "fox::gr::dg::restoreTemporaryRenderTarget", .rva = 0x27D330u, .bytes = "48 89 5C 24 08 57 48 83 EC 20 48 8B 01 48 8B D9 FF 90 F8 04 00 00"},
    {.entry = Entry::RENDER_TARGET_CREATE_RESOURCE, .name = "fox::gr::dg::RenderTargetManager::CreateResource", .rva = 0x242620u, .bytes = "48 8B C4 55 56 57 41 56 41 57 48 83 EC 40 48 C7 40 B8 FE FF FF FF 48 89 58 18 4C 8B F1 48 8B 35"},
    {.entry = Entry::RENDER_TARGET_RESTORE_WITH_RESIZE, .name = "fox::gr::dg::RenderTargetManager::RestoreHandleWithResize", .rva = 0x242AC0u, .bytes = "40 53 48 83 EC 60 4C 8B 0D"},
    {.entry = Entry::RENDER_TARGET_MANAGER, .name = "fox::gr::ResourceManagerBase<RenderTargetResource>::m_instance", .rva = 0x2B73428u},

    // Motion vectors (engine/motion_vectors.hpp).
    {.entry = Entry::DEFERRED_GEOMETRY_POST_EXEC, .name = "fox::gr::PluginDeferredGeometry::PostExec", .rva = 0x1C54D0u, .bytes = "48 8B C4 53 55 56 57 41 56 48 81 EC 40 01 00 00 48 C7 44 24 40 FE FF FF FF"},
    {.entry = Entry::DG_END_SCREEN_SPACE_VELOCITY, .name = "fox::gr::dg::DgDx11::EndScreenSpaceVelocity", .rva = 0x2BF7A0u, .bytes = "40 53 48 83 EC 20 48 8B D9 48 8B 0D 10 49 8D 02"},

    // Start of post-processing (engine/post_filter.hpp).
    {.entry = Entry::DG_START_POST_FILTER, .name = "fox::gr::dg::DgDx11::StartPostFilter", .rva = 0x2C2D90u, .bytes = "48 89 5C 24 10 57 48 83 EC 40 48 8B 05"},

    // Local lights and their constants (engine/local_lights.hpp).
    {.entry = Entry::LOCAL_LIGHT_MAIN_EXEC, .name = "fox::gr::PluginLocalLight::MainExec", .rva = 0x20AA00u, .bytes = "48 8B C4 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 18 FF FF FF 48 81 EC B0 01 00 00"},
    {.entry = Entry::COMMAND_BUFFER_SET_VECTOR, .name = "fox::gr::dg::CommandBuffer::SetVector", .rva = 0x2D8B70u, .bytes = "48 83 EC 38 48 8B 49 28 4D 8B D0 8B C2 C1 F8 04"},
    {.entry = Entry::SHADOW_CONSTANT_REGISTER_SET_VECTOR, .name = "fox::gr::dg::ShadowConstantRegister::SetVector", .rva = 0x2C5F30u, .bytes = "48 89 5C 24 08 57 48 83 EC 20 48 8B F9 BB 01 00 00 00"},
    {.entry = Entry::SHADOW_CONSTANT_REGISTER_FLUSH, .name = "fox::gr::dg::ShadowConstantRegister::Flush", .rva = 0x2C5A10u, .bytes = "48 89 4C 24 08 55 56 57 41 54 41 55 41 56 41 57 48 81 EC B0 00 00 00 8B 81 14 01 00 00"},

    // Render size and depth of field at output size (reconstruction/resolution.hpp).
    {.entry = Entry::DG_START, .name = "fox::gr::dg::DgDx11::Start", .rva = 0x24C220u, .bytes = "40 53 48 83 EC 20 48 8B D9 8B 89 98 01 00 00"},
    {.entry = Entry::DEPTH_OF_FIELD_DRAW, .name = "fox::gr::dg::plugins::DepthOfFieldDx11::Draw", .rva = 0x2F1780u, .bytes = "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48"},
    {.entry = Entry::DEPTH_OF_FIELD_SET_RENDER_AREA, .name = "fox::gr::dg::DgResources::SetRenderArea", .rva = 0x2F64A0u, .bytes = "48 89 5C 24 20 48 89 4C 24 08 55 56 57 41 54 41"},
    {.entry = Entry::DEPTH_OF_FIELD_DRAW_SPRITE_SUB, .name = "fox::gr::dg::plugins::DepthOfFieldDx11::DrawSpriteSub", .rva = 0x2F5D70u, .bytes = "48 8B C4 48 89 58 08 55 56 57 41 54 41 55 41 56"},
    {.entry = Entry::DEPTH_OF_FIELD_NEAR_RADIUS_CAP, .name = "depth of field scatter: near radius cap of 6", .rva = 0x20AAE50u, .bytes = "00 00 C0 40"},
    {.entry = Entry::DEPTH_OF_FIELD_SCATTER_SCALE, .name = "depth of field scatter: blur scale of 0.85", .rva = 0x211E04Cu, .bytes = "9A 99 59 3F"},
    {.entry = Entry::DEPTH_OF_FIELD_REFERENCE_WIDTH, .name = "depth of field scatter: reciprocal of its 1920-pixel reference width", .rva = 0x211E048u, .bytes = "89 88 08 3A"},

    // Projection jitter (reconstruction/projection_jitter.hpp).
    {.entry = Entry::FORWARD_RENDERING_PRE_EXEC, .name = "fox::gr::PluginForwardRendering::PreExec", .rva = 0x1CD460u, .bytes = "40 53 48 83 EC 20 8B 0D 4C 7D 81 02 49 8B D8 E8"},
    {.entry = Entry::REQUEST_DRAW_ALL_LOCAL_LIGHT_ON_JOB, .name = "fox::gr::dg::plugins::LightRendering::RequestDrawAllLocalLightOnJob", .rva = 0x2A43E0u, .bytes = "40 57 41 56 41 57 48 83 EC 40 48 C7 44 24 30 FE"},
    {.entry = Entry::LOCAL_LIGHT_REQUEST_SLOT, .name = "fox::gr::dg::plugins::LightRendering virtual table: RequestDrawAllLocalLightOnJob", .rva = 0x211E8D8u},
    {.entry = Entry::LOCAL_LIGHT_REQUEST_RETURN, .name = "fox::gr::PluginLocalLight::MainExec: return from RequestDrawAllLocalLightOnJob", .rva = 0x20B8A3u, .bytes_before = 38u, .bytes = "48 89 5C 24 38 4C 89 7C 24 30 48 89 44 24 28 48 89 54 24 20 4C 8B 4C 24 78 4C 8B 45 88 48 8B 54 24 58 41 FF 52 48"},
    {.entry = Entry::LOCAL_LIGHT_VIEWPORT_COPY, .name = "fox::gr::PluginLocalLight::MainExec: viewport projection copy", .rva = 0x20B48Cu, .bytes = "41 0F 28 87 80 02 00 00 41 0F 29 86 A0 01 00 00"},
    {.entry = Entry::CONSTANT_BUFFER_MAP_DATA, .name = "fox::gr::dg::ConstantBuffer::MapData", .rva = 0x311E40u, .bytes = "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57"},
    {.entry = Entry::COMMAND_BUFFER_FLUSH_RENDER_RESOURCE, .name = "fox::gr::dg::CommandBuffer::FlushRenderResource", .rva = 0x2D73D0u},
    {.entry = Entry::COMMAND_BUFFER_FLUSH_CONSTANT_REGISTER, .name = "fox::gr::dg::CommandBuffer::FlushConstantRegister", .rva = 0x2D7390u},
    {.entry = Entry::MODEL_FLUSH_RENDER_RESOURCE_CALL, .name = "fox::gr::dg::BuildCommandsDx11::DrawModelMacroPacket: call to FlushRenderResource", .rva = 0x2D6642u, .bytes = "E8 ?? ?? ?? ??"},
    {.entry = Entry::MODEL_CONSTANT_UPLOAD_RETURN, .name = "fox::gr::dg::BuildCommandsDx11::DrawModelMacroPacket: return from FlushConstantRegister", .rva = 0x2D664Fu, .bytes_before = 5u, .bytes = "E8 ?? ?? ?? ??"},
    {.entry = Entry::SCATTER_UPLOAD_RETURN, .name = "fox::gr::dg::plugins::SubSurfaceScatterPluginDx11::Draw: return from MapData", .rva = 0x30E7EFu, .bytes_before = 5u, .bytes = "E8 ?? ?? ?? ??"},
    {.entry = Entry::PROJECTION_COMMIT, .name = "fox::gr::Render::buildRendering: projection commit", .rva = 0x1BF26Fu, .bytes_before = 63u, .bytes = "48 8B 05 ?? ?? ?? ?? 0F 28 87 80 02 00 00 0F 29 80 80 06 00 00 0F 28 8F 90 02 00 00 0F 29 88 90 06 00 00 0F 28 87 A0 02 00 00 0F 29 80 A0 06 00 00 0F 28 8F B0 02 00 00 0F 29 88 B0 06 00 00 83 88 90 08 00 00 09 83 88 94 08 00 00 08 48 8D 8F C0 02 00 00 E8 ?? ?? ?? ??"},
    {.entry = Entry::RENDER_SET_VIEW_RETURN, .name = "fox::gr::Render::buildRendering: return from SetViewMatrix after the projection commit", .rva = 0x1BF289u, .bytes_before = 5u, .bytes = "E8 ?? ?? ?? ??"},
    {.entry = Entry::SHADER_MANAGER_SET_VIEW_MATRIX, .name = "fox::gr::dg::ShaderManager::SetViewMatrix", .rva = 0x1BE640u},
    {.entry = Entry::SHADER_MANAGER, .name = "fox::gr::dg::ShaderManager instance", .rva = 0x2B73668u},
    {.entry = Entry::DEFERRED_GEOMETRY_SET_VIEW_RETURN, .name = "fox::gr::PluginDeferredGeometry::PostExec: return from SetViewMatrix", .rva = 0x1C57C8u, .bytes_before = 5u, .bytes = "E8 ?? ?? ?? ??"},
    {.entry = Entry::MODEL_SET_VIEW_RETURN, .name = "fox::gr::PluginModel::PreExec: return from SetViewMatrix", .rva = 0x1CD63Au, .bytes_before = 5u, .bytes = "E8 ?? ?? ?? ??"},
    {.entry = Entry::ALPHA_MODEL_SET_VIEW_RETURN, .name = "fox::gr::PluginAlphaModel::PreExec: return from SetViewMatrix", .rva = 0x1CFA0Du, .bytes_before = 5u, .bytes = "E8 ?? ?? ?? ??"},
    {.entry = Entry::OVERLAY_MODEL_SET_VIEW_RETURN, .name = "fox::gr::PluginOverlayModel::PreExec: return from SetViewMatrix", .rva = 0x208D9Cu, .bytes_before = 5u, .bytes = "E8 ?? ?? ?? ??"},
    {.entry = Entry::NEW_THERMOGRAPHY_SET_VIEW_RETURN, .name = "fox::gr::PluginThermography::MainExec: mode-2 return from SetViewMatrix", .rva = 0x214531u, .bytes_before = 5u, .bytes = "E8 ?? ?? ?? ??"},
    {.entry = Entry::THERMOGRAPHY_SET_VIEW_RETURN, .name = "fox::gr::PluginThermography::MainExec: return from SetViewMatrix", .rva = 0x214AC3u, .bytes_before = 5u, .bytes = "E8 ?? ?? ?? ??"},
    {.entry = Entry::SUN_LIGHT_DRAW, .name = "fox::gr::dg::plugins::SunLightRenderingDx11::Draw", .rva = 0x30AC50u, .bytes = "48 89 5C 24 08 44 89 4C 24 20 44 89 44 24 18 48"},
    {.entry = Entry::DRAW_SHADOW_BOX, .name = "DgPluginSunLightDx11 DrawShadowBox", .rva = 0x30B680u, .bytes = "48 8B C4 48 89 58 10 48 89 70 18 48 89 78 20 55"},
    {.entry = Entry::SUN_SHADOW_BOX_RETURN, .name = "fox::gr::dg::plugins::SunLightRenderingDx11::Draw: return from DrawShadowBox", .rva = 0x30B07Du, .bytes_before = 5u, .bytes = "E8 ?? ?? ?? ??"},
    {.entry = Entry::SPHERICAL_HARMONICS_CALCULATE_LIGHT_VOLUME, .name = "fox::gr::dg::plugins::SphericalHarmonicsLighting::CalculateLightVolume", .rva = 0x27EF80u, .bytes = "48 89 5C 24 08 55 56 57 41 56 41 57 48 81 EC 80"},
    {.entry = Entry::SPHERICAL_HARMONICS_LIGHT_VOLUME_RETURN, .name = "fox::gr::PluginSphericalHarmonics::ExecSingles: return from CalculateLightVolume", .rva = 0x1F2529u, .bytes_before = 5u, .bytes = "E8 ?? ?? ?? ??"},
    {.entry = Entry::SPHERICAL_HARMONICS_REGISTER_DRAW_ON_JOB, .name = "fox::gr::dg::plugins::SphericalHarmonicsLighting::RegisterDrawOnJob", .rva = 0x281EF0u, .bytes = "48 8B C4 55 53 56 57 41 54 41 55 41 56 41 57"},
    {.entry = Entry::SPHERICAL_HARMONICS_SET_OUTPUT, .name = "fox::gr::dg::BuildSphericalHarmonicsSphereMapsJob::SetOutputBuildSphericalHarmonicLightCommandsParameters", .rva = 0x283B90u, .bytes = "40 57 48 83 EC 30 48 C7 44 24 20 FE FF FF FF 48"},
    {.entry = Entry::SPHERICAL_HARMONICS_SET_OUTPUT_RETURN, .name = "fox::gr::dg::plugins::SphericalHarmonicsLighting::RegisterDrawOnJob: return from SetOutput", .rva = 0x282A8Bu, .bytes_before = 5u, .bytes = "E8 ?? ?? ?? ??"},
    {.entry = Entry::SPHERICAL_HARMONICS_VOLUME_VERTEX_COUNTS, .name = "spherical-harmonic light volume vertex counts", .rva = 0x21102B8u, .bytes = "08 00 00 00 06 00 00 00 12 00 00 00 08 00 00 00"},
    {.entry = Entry::SKY_CALC_SHADER_PARAMETERS, .name = "fox::gr::PluginSky::CalcShaderParameters", .rva = 0x1F50D0u, .bytes = "48 8B C4 53 55 56 57 41 56 48 81 EC 30 01 00 00"},
    {.entry = Entry::SKY_PARAMETERS_RETURN, .name = "fox::gr::PluginSky::MainExec: return from CalcShaderParameters", .rva = 0x1F5990u, .bytes_before = 5u, .bytes = "E8 ?? ?? ?? ??"},
    {.entry = Entry::TERRAIN_DECAL_DRAW_ON_JOB, .name = "fox::gr::dg::plugins::TerrainRendering::TerrainDecalDrawOnJob", .rva = 0x275280u, .bytes = "40 53 55 56 57 41 54 41 55 41 56 41 57 48 81 EC"},
    {.entry = Entry::TERRAIN_DECAL_SET_INPUT, .name = "fox::gr::dg::BuildTerrainDecalJob::SetInputBuildParameters", .rva = 0x2D18D0u, .bytes = "40 57 48 83 EC 30 48 C7 44 24 20 FE FF FF FF 48"},
    {.entry = Entry::TERRAIN_DECAL_SET_INPUT_RETURN, .name = "fox::gr::dg::plugins::TerrainRendering::TerrainDecalDrawOnJob: return from SetInputBuildParameters", .rva = 0x2755CAu, .bytes_before = 5u, .bytes = "E8 ?? ?? ?? ??"},

    // Game pause (engine/pause.hpp).
    {.entry = Entry::GAME_TIME_UPDATE, .name = "fox::GameTime::Update", .rva = 0x816A0u, .bytes = "48 89 5C 24 10 4C 89 44 24 18 57 48 83 EC 40 0F 29 74 24 30"},
    {.entry = Entry::GET_PAUSE_SYSTEM, .name = "fox::GetPauseSystem", .rva = 0xF1D80u, .bytes = "48 8D 05 ?? ?? ?? ?? C3"},
    {.entry = Entry::PAUSE_SYSTEM, .name = "fox::PauseSystemImpl instance", .rva = 0x2B65D40u},

    // Read only to verify structure fields; never hooked.
    {.entry = Entry::SHADOW_CONSTANT_REGISTER_DUPLICATE, .name = "fox::gr::dg::ShadowConstantRegister::Duplicate", .rva = 0x2C5790u, .bytes = "40 55 56 57 48 81 EC A0 00 00 00 48 C7 44 24 28 FE FF FF FF"},
    {.entry = Entry::RENDER_INFO_RESIZE_SWAP_CHAIN, .name = "fox::gr::dg::RenderInfoDx11::ResizeSwapChain", .rva = 0x24AD80u, .bytes = "40 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 D9 48 81 EC B0 00 00 00"},
    {.entry = Entry::SWAP_CHAIN_PRESENT, .name = "gn::swapchain::Present", .rva = 0x19F5990u, .bytes = "48 8B 49 18 45 33 C0 48 8B 01 48 FF 60 40"},
    {.entry = Entry::SHADER_RESOURCE_VIEW_CREATE, .name = "gn::shaderresourceview::Create", .rva = 0x19F5400u, .bytes = "48 89 5C 24 18 57 48 83 EC 40 48 8B 79 10 48 8B CF 48 8B 07 FF 50 08"},
}};

// An instruction in one entry's code that refers to another entry: a call
// (E8 and a 32-bit displacement) or a load relative to the instruction pointer.
// Both entries are verified only if the instruction leads exactly to the target.
struct Reference {
  Entry source = Entry::COUNT;
  // Start of the instruction, relative to the source entry's address.
  int32_t instruction = 0;
  // Offset of the 32-bit displacement within the instruction.
  uint32_t displacement = 0u;
  uint32_t length = 0u;
  Entry target = Entry::COUNT;
};

inline constexpr std::array REFERENCES = {
    Reference{.source = Entry::RENDER_TARGET_RESTORE_WITH_RESIZE, .instruction = 6, .displacement = 3u, .length = 7u, .target = Entry::RENDER_TARGET_MANAGER},
    Reference{.source = Entry::PROJECTION_COMMIT, .instruction = 21, .displacement = 1u, .length = 5u, .target = Entry::SHADER_MANAGER_SET_VIEW_MATRIX},
    Reference{.source = Entry::PROJECTION_COMMIT, .instruction = -63, .displacement = 3u, .length = 7u, .target = Entry::SHADER_MANAGER},
    Reference{.source = Entry::RENDER_SET_VIEW_RETURN, .instruction = -5, .displacement = 1u, .length = 5u, .target = Entry::SHADER_MANAGER_SET_VIEW_MATRIX},
    Reference{.source = Entry::DEFERRED_GEOMETRY_SET_VIEW_RETURN, .instruction = -5, .displacement = 1u, .length = 5u, .target = Entry::SHADER_MANAGER_SET_VIEW_MATRIX},
    Reference{.source = Entry::MODEL_SET_VIEW_RETURN, .instruction = -5, .displacement = 1u, .length = 5u, .target = Entry::SHADER_MANAGER_SET_VIEW_MATRIX},
    Reference{.source = Entry::ALPHA_MODEL_SET_VIEW_RETURN, .instruction = -5, .displacement = 1u, .length = 5u, .target = Entry::SHADER_MANAGER_SET_VIEW_MATRIX},
    Reference{.source = Entry::OVERLAY_MODEL_SET_VIEW_RETURN, .instruction = -5, .displacement = 1u, .length = 5u, .target = Entry::SHADER_MANAGER_SET_VIEW_MATRIX},
    Reference{.source = Entry::NEW_THERMOGRAPHY_SET_VIEW_RETURN, .instruction = -5, .displacement = 1u, .length = 5u, .target = Entry::SHADER_MANAGER_SET_VIEW_MATRIX},
    Reference{.source = Entry::THERMOGRAPHY_SET_VIEW_RETURN, .instruction = -5, .displacement = 1u, .length = 5u, .target = Entry::SHADER_MANAGER_SET_VIEW_MATRIX},
    Reference{.source = Entry::MODEL_FLUSH_RENDER_RESOURCE_CALL, .instruction = 0, .displacement = 1u, .length = 5u, .target = Entry::COMMAND_BUFFER_FLUSH_RENDER_RESOURCE},
    Reference{.source = Entry::MODEL_CONSTANT_UPLOAD_RETURN, .instruction = -5, .displacement = 1u, .length = 5u, .target = Entry::COMMAND_BUFFER_FLUSH_CONSTANT_REGISTER},
    Reference{.source = Entry::SCATTER_UPLOAD_RETURN, .instruction = -5, .displacement = 1u, .length = 5u, .target = Entry::CONSTANT_BUFFER_MAP_DATA},
    Reference{.source = Entry::SUN_SHADOW_BOX_RETURN, .instruction = -5, .displacement = 1u, .length = 5u, .target = Entry::DRAW_SHADOW_BOX},
    Reference{.source = Entry::SPHERICAL_HARMONICS_LIGHT_VOLUME_RETURN, .instruction = -5, .displacement = 1u, .length = 5u, .target = Entry::SPHERICAL_HARMONICS_CALCULATE_LIGHT_VOLUME},
    Reference{.source = Entry::SPHERICAL_HARMONICS_SET_OUTPUT_RETURN, .instruction = -5, .displacement = 1u, .length = 5u, .target = Entry::SPHERICAL_HARMONICS_SET_OUTPUT},
    Reference{.source = Entry::SKY_PARAMETERS_RETURN, .instruction = -5, .displacement = 1u, .length = 5u, .target = Entry::SKY_CALC_SHADER_PARAMETERS},
    Reference{.source = Entry::TERRAIN_DECAL_SET_INPUT_RETURN, .instruction = -5, .displacement = 1u, .length = 5u, .target = Entry::TERRAIN_DECAL_SET_INPUT},
    Reference{.source = Entry::GAME_TIME_UPDATE, .instruction = 0x1F, .displacement = 1u, .length = 5u, .target = Entry::GET_PAUSE_SYSTEM},
    Reference{.source = Entry::GET_PAUSE_SYSTEM, .instruction = 0, .displacement = 3u, .length = 7u, .target = Entry::PAUSE_SYSTEM},
};

// A virtual table slot that must hold another entry's address. PluginLocalLight
// calls the hooked request function through LightRendering's virtual table.
struct Slot {
  Entry slot = Entry::COUNT;
  Entry target = Entry::COUNT;
};

inline constexpr std::array SLOTS = {
    Slot{.slot = Entry::LOCAL_LIGHT_REQUEST_SLOT, .target = Entry::REQUEST_DRAW_ALL_LOCAL_LIGHT_ON_JOB},
};

inline constexpr bool ENTRIES_IN_ORDER = [] {
  for (size_t index = 0u; index < ENTRY_COUNT; ++index) {
    if (ENTRIES[index].entry != static_cast<Entry>(index)) return false;
  }
  return true;
}();
static_assert(ENTRIES_IN_ORDER, "ENTRIES must list every entry in the order of Entry");

inline constexpr bool ENTRIES_WELL_FORMED = std::ranges::all_of(ENTRIES, [](const EntryInfo& info) {
  const size_t byte_count = (info.bytes.empty() ? 0u : (info.bytes.size() + 1u) / 3u);
  return (info.bytes.empty() || info.bytes.size() % 3u == 2u)
         && info.bytes_before <= info.rva
         && info.rva - info.bytes_before + byte_count <= IMAGE_SIZE;
});
static_assert(ENTRIES_WELL_FORMED, "every entry's bytes must be space-separated pairs inside the image");

// A value inside an engine structure that the mod reads or writes: a field
// offset, an array stride or shift, a mask or a flag. Each stays a compile-time
// constant. At startup one instruction in a verified entry must still encode
// it, so a changed structure turns off the features that use it instead of
// letting them read the wrong memory.
enum class Field : uint8_t {
  // fox::gr::Viewport.
  VIEWPORT_NEXT,
  VIEWPORT_PROJECTION,
  VIEWPORT_VIEW,
  VIEWPORT_PREVIOUS_VIEW,
  VIEWPORT_PREVIOUS_PROJECTION,
  VIEWPORT_CAMERA,
  VIEWPORT_RECTANGLE,
  VIEWPORT_WIDTH,
  VIEWPORT_HEIGHT,
  VIEWPORT_FLAGS,
  VIEWPORT_FLAG_BUILD,

  // fox::gr::Render.
  RENDER_INFO,
  RENDER_VIEWPORTS,
  RENDER_FRAME_FLAGS,
  FRAME_FLAG_MOTION_VECTORS,

  // The backend (fox::gr::dg::DgDx11), its render info and swap chain.
  BACKEND_WANTED_SIZE,
  BACKEND_RENDER_SIZE,
  BACKEND_FLAGS,
  BACKEND_FLAG_RESIZE,
  BACKEND_FLAG_VIDEO_SETTINGS,
  RENDER_INFO_RENDER_SIZE,
  RENDER_INFO_SWAP_CHAIN,
  SWAP_CHAIN_NATIVE,

  // Command buffers, their render context and branch records.
  COMMAND_BUFFER_CONTEXT,
  COMMAND_BUFFER_CONSTANTS,
  CONTEXT_D3D_CONTEXT,
  BRANCH_NODE,
  BRANCH_TARGET,
  BRANCH_MATRICES,

  // Depth of field.
  DEPTH_OF_FIELD_EFFECT,

  // Render targets, their D3D11 objects and the manager that stores them.
  RENDER_TARGET_TEXTURE,
  RENDER_TARGET_SHADER_VIEW,
  TEXTURE_RESOURCE,
  SHADER_VIEW_OBJECT,
  HANDLE_TYPE_MASK,
  HANDLE_TYPE_RENDER_TARGET,
  HANDLE_INDEX_SHIFT,
  RESOURCE_ENTRY_SIZE,
  RESOURCE_MANAGER_ENTRIES,
  RESOURCE_ENTRY_PARAMETER,
  RESOURCE_MANAGER_USED,
  PARAMETER_FORMAT_CODE,

  // Constant banks (fox::gr::dg::ShadowConstantRegister).
  CONSTANT_BANK_SHIFT,
  CONSTANT_BANK_DATA,
  CONSTANT_BANK_VECTORS,
  CONSTANT_BANK_COUNT,
  CONSTANT_BANKS_DIRTY,
  CONSTANT_BANKS_ACTIVE,

  // Camera and shader manager.
  CAMERA_CENTER,
  SHADER_MANAGER_PROJECTION,
  SHADER_MANAGER_DIRTY,
  SHADER_MANAGER_DIRTY_PROJECTION,
  SHADER_MANAGER_DIRTY_SECOND,
  SHADER_MANAGER_DIRTY_SECOND_PROJECTION,

  // Draw packets and sky parameters.
  LIGHT_PACKET_PROJECTION,
  LIGHT_PACKET_RECTANGLE,
  DRAW_PACKET_VIEW,
  DRAW_PACKET_PROJECTION,
  SKY_PARAMETERS_INVERSE_PROJECTION,

  // fox::PauseSystemImpl.
  PAUSE_FLAGS,
  PAUSE_FLAGS_GAME_TIME,

  COUNT,
};
inline constexpr size_t FIELD_COUNT = static_cast<size_t>(Field::COUNT);

struct FieldInfo {
  Field field = Field::COUNT;
  // The structure and what the value is in it.
  std::string_view name;
  uint32_t value = 0u;
  // The entry whose code encodes the value, the start of that instruction
  // relative to the entry, and where in the instruction the value sits.
  Entry source = Entry::COUNT;
  uint32_t instruction = 0u;
  uint8_t position = 0u;
  // 1 or 4 bytes, little-endian.
  uint8_t size = 0u;
};

inline constexpr std::array<FieldInfo, FIELD_COUNT> FIELDS = {{
    // fox::gr::Viewport.
    {.field = Field::VIEWPORT_NEXT, .name = "fox::gr::Viewport: next viewport in the renderer's list", .value = 0x30u, .source = Entry::RENDER_DESTRUCTOR, .instruction = 0xC8u, .position = 3u, .size = 1u},
    {.field = Field::VIEWPORT_PROJECTION, .name = "fox::gr::Viewport: projection matrix", .value = 0x280u, .source = Entry::FORWARD_RENDERING_PRE_EXEC, .instruction = 0x22u, .position = 3u, .size = 4u},
    {.field = Field::VIEWPORT_VIEW, .name = "fox::gr::Viewport: view matrix", .value = 0x2C0u, .source = Entry::FORWARD_RENDERING_PRE_EXEC, .instruction = 0x1Bu, .position = 3u, .size = 4u},
    {.field = Field::VIEWPORT_PREVIOUS_VIEW, .name = "fox::gr::Viewport: previous frame's view matrix", .value = 0x3C0u, .source = Entry::DEFERRED_GEOMETRY_POST_EXEC, .instruction = 0x1F1u, .position = 3u, .size = 4u},
    {.field = Field::VIEWPORT_PREVIOUS_PROJECTION, .name = "fox::gr::Viewport: previous frame's projection matrix", .value = 0x400u, .source = Entry::DEFERRED_GEOMETRY_POST_EXEC, .instruction = 0xDBu, .position = 4u, .size = 4u},
    {.field = Field::VIEWPORT_CAMERA, .name = "fox::gr::Viewport: camera", .value = 0x570u, .source = Entry::DG_INIT_VIEWPORT, .instruction = 0x4Au, .position = 3u, .size = 4u},
    {.field = Field::VIEWPORT_RECTANGLE, .name = "fox::gr::Viewport: rectangle (x, y, width, height)", .value = 0x5D0u, .source = Entry::DG_INIT_VIEWPORT, .instruction = 0x2E1u, .position = 2u, .size = 4u},
    {.field = Field::VIEWPORT_WIDTH, .name = "fox::gr::Viewport: width", .value = 0x5D8u, .source = Entry::RENDER_BUILD_RENDERING, .instruction = 0x118u, .position = 3u, .size = 4u},
    {.field = Field::VIEWPORT_HEIGHT, .name = "fox::gr::Viewport: height", .value = 0x5DCu, .source = Entry::RENDER_BUILD_RENDERING, .instruction = 0x121u, .position = 3u, .size = 4u},
    {.field = Field::VIEWPORT_FLAGS, .name = "fox::gr::Viewport: flags", .value = 0x6E2u, .source = Entry::RENDER_BUILD_RENDERING, .instruction = 0x10Fu, .position = 2u, .size = 4u},
    {.field = Field::VIEWPORT_FLAG_BUILD, .name = "fox::gr::Viewport flags: bit buildRendering requires before it uses the viewport", .value = 0x1u, .source = Entry::RENDER_BUILD_RENDERING, .instruction = 0x10Fu, .position = 6u, .size = 1u},

    // fox::gr::Render.
    {.field = Field::RENDER_INFO, .name = "fox::gr::Render: render info", .value = 0x98u, .source = Entry::RENDER_BUILD_RENDERING, .instruction = 0x5Au, .position = 3u, .size = 4u},
    {.field = Field::RENDER_VIEWPORTS, .name = "fox::gr::Render: first active viewport", .value = 0xA0u, .source = Entry::RENDER_BUILD_RENDERING, .instruction = 0xEDu, .position = 3u, .size = 4u},
    {.field = Field::RENDER_FRAME_FLAGS, .name = "fox::gr::Render: frame flags of the build in progress", .value = 0xD0u, .source = Entry::RENDER_BUILD_RENDERING, .instruction = 0x80u, .position = 3u, .size = 4u},
    {.field = Field::FRAME_FLAG_MOTION_VECTORS, .name = "frame flags: bit that asks for motion vectors", .value = 0x1u, .source = Entry::DEFERRED_GEOMETRY_POST_EXEC, .instruction = 0xBFu, .position = 2u, .size = 1u},

    // The backend (fox::gr::dg::DgDx11), its render info and swap chain.
    {.field = Field::BACKEND_WANTED_SIZE, .name = "fox::gr::dg::DgDx11: requested render size", .value = 0x188u, .source = Entry::DG_START, .instruction = 0x42u, .position = 2u, .size = 4u},
    {.field = Field::BACKEND_RENDER_SIZE, .name = "fox::gr::dg::DgDx11: render size", .value = 0x190u, .source = Entry::DG_START, .instruction = 0x48u, .position = 2u, .size = 4u},
    {.field = Field::BACKEND_FLAGS, .name = "fox::gr::dg::DgDx11: flags", .value = 0x198u, .source = Entry::DG_START, .instruction = 0x9u, .position = 2u, .size = 4u},
    {.field = Field::BACKEND_FLAG_RESIZE, .name = "fox::gr::dg::DgDx11 flags: bit that requests a resize", .value = 0x2u, .source = Entry::DG_START, .instruction = 0x39u, .position = 6u, .size = 1u},
    {.field = Field::BACKEND_FLAG_VIDEO_SETTINGS, .name = "fox::gr::dg::DgDx11 flags: bit set while changed video settings wait for Start", .value = 0x4u, .source = Entry::DG_START, .instruction = 0xFu, .position = 2u, .size = 1u},
    {.field = Field::RENDER_INFO_RENDER_SIZE, .name = "fox::gr::dg::RenderInfoDx11: render size", .value = 0x14u, .source = Entry::DG_INIT_RENDER, .instruction = 0x36Cu, .position = 2u, .size = 1u},
    {.field = Field::RENDER_INFO_SWAP_CHAIN, .name = "fox::gr::dg::RenderInfoDx11: swap chain wrapper", .value = 0x20u, .source = Entry::RENDER_INFO_RESIZE_SWAP_CHAIN, .instruction = 0x32Au, .position = 3u, .size = 1u},
    {.field = Field::SWAP_CHAIN_NATIVE, .name = "gn swap chain wrapper: IDXGISwapChain", .value = 0x18u, .source = Entry::SWAP_CHAIN_PRESENT, .instruction = 0x0u, .position = 3u, .size = 1u},

    // Command buffers, their render context and branch records.
    {.field = Field::COMMAND_BUFFER_CONTEXT, .name = "fox::gr::dg::CommandBuffer: render context", .value = 0x8u, .source = Entry::COMMAND_BUFFER_FLUSH_CONSTANT_REGISTER, .instruction = 0x24u, .position = 3u, .size = 1u},
    {.field = Field::COMMAND_BUFFER_CONSTANTS, .name = "fox::gr::dg::CommandBuffer: constant registers", .value = 0x28u, .source = Entry::COMMAND_BUFFER_FLUSH_CONSTANT_REGISTER, .instruction = 0x28u, .position = 3u, .size = 1u},
    {.field = Field::CONTEXT_D3D_CONTEXT, .name = "render context: D3D11 device context", .value = 0x8u, .source = Entry::SHADOW_CONSTANT_REGISTER_FLUSH, .instruction = 0xA0u, .position = 3u, .size = 1u},
    {.field = Field::BRANCH_NODE, .name = "branch subcontext: command list node", .value = 0x10u, .source = Entry::COMMAND_BUFFER_BRANCH, .instruction = 0x52u, .position = 3u, .size = 1u},
    {.field = Field::BRANCH_TARGET, .name = "branch subcontext: target", .value = 0x18u, .source = Entry::COMMAND_BUFFER_BRANCH, .instruction = 0x11Au, .position = 3u, .size = 1u},
    {.field = Field::BRANCH_MATRICES, .name = "branch subcontext: two 4x4 matrices", .value = 0x40u, .source = Entry::COMMAND_BUFFER_BRANCH, .instruction = 0x5Du, .position = 3u, .size = 1u},

    // Depth of field.
    {.field = Field::DEPTH_OF_FIELD_EFFECT, .name = "fox::gr::PluginDepthOfField: effect", .value = 0x68u, .source = Entry::DEPTH_OF_FIELD_MAIN_EXEC, .instruction = 0x32Eu, .position = 3u, .size = 1u},

    // Render targets, their D3D11 objects and the manager that stores them.
    {.field = Field::RENDER_TARGET_TEXTURE, .name = "fox::gr::dg::RenderTargetDx11: texture wrapper", .value = 0x40u, .source = Entry::RENDER_TARGET_INITIALIZE, .instruction = 0x23Eu, .position = 3u, .size = 1u},
    {.field = Field::RENDER_TARGET_SHADER_VIEW, .name = "fox::gr::dg::RenderTargetDx11: shader view wrapper", .value = 0x50u, .source = Entry::RENDER_TARGET_INITIALIZE, .instruction = 0x3CEu, .position = 3u, .size = 1u},
    {.field = Field::TEXTURE_RESOURCE, .name = "gn texture wrapper: ID3D11Resource", .value = 0x10u, .source = Entry::RENDER_TARGET_INITIALIZE, .instruction = 0x242u, .position = 3u, .size = 1u},
    {.field = Field::SHADER_VIEW_OBJECT, .name = "gn shader view wrapper: ID3D11ShaderResourceView", .value = 0x8u, .source = Entry::SHADER_RESOURCE_VIEW_CREATE, .instruction = 0x68u, .position = 3u, .size = 1u},
    {.field = Field::HANDLE_TYPE_MASK, .name = "resource handle: type mask", .value = 0x7Fu, .source = Entry::RENDER_TARGET_RESTORE_WITH_RESIZE, .instruction = 0x20u, .position = 2u, .size = 1u},
    {.field = Field::HANDLE_TYPE_RENDER_TARGET, .name = "resource handle: render target type", .value = 0xBu, .source = Entry::RENDER_TARGET_RESTORE_WITH_RESIZE, .instruction = 0x23u, .position = 1u, .size = 1u},
    {.field = Field::HANDLE_INDEX_SHIFT, .name = "resource handle: shift of the entry index", .value = 0x11u, .source = Entry::RENDER_TARGET_RESTORE_WITH_RESIZE, .instruction = 0x2Bu, .position = 2u, .size = 1u},
    {.field = Field::RESOURCE_ENTRY_SIZE, .name = "render target manager: entry size", .value = 0x70u, .source = Entry::RENDER_TARGET_RESTORE_WITH_RESIZE, .instruction = 0x30u, .position = 3u, .size = 1u},
    {.field = Field::RESOURCE_MANAGER_ENTRIES, .name = "render target manager: entries", .value = 0x8u, .source = Entry::RENDER_TARGET_RESTORE_WITH_RESIZE, .instruction = 0x34u, .position = 3u, .size = 1u},
    {.field = Field::RESOURCE_ENTRY_PARAMETER, .name = "render target manager entry: parameter", .value = 0x28u, .source = Entry::RENDER_TARGET_RESTORE_WITH_RESIZE, .instruction = 0x38u, .position = 3u, .size = 1u},
    {.field = Field::RESOURCE_MANAGER_USED, .name = "render target manager: number of entries in use", .value = 0x14u, .source = Entry::RENDER_TARGET_CREATE_RESOURCE, .instruction = 0x19Eu, .position = 2u, .size = 1u},
    {.field = Field::PARAMETER_FORMAT_CODE, .name = "fox::gr::dg::RenderTargetParameter: format code", .value = 0x14u, .source = Entry::GET_BUFFER_FORMAT_FROM_PARAMETER, .instruction = 0x0u, .position = 3u, .size = 1u},

    // Constant banks (fox::gr::dg::ShadowConstantRegister).
    {.field = Field::CONSTANT_BANK_SHIFT, .name = "fox::gr::dg::ShadowConstantRegister: bank record size as a power of two", .value = 0x5u, .source = Entry::SHADOW_CONSTANT_REGISTER_SET_VECTOR, .instruction = 0x2Cu, .position = 3u, .size = 1u},
    {.field = Field::CONSTANT_BANK_DATA, .name = "fox::gr::dg::ShadowConstantRegister bank: CPU data", .value = 0x8u, .source = Entry::SHADOW_CONSTANT_REGISTER_SET_VECTOR, .instruction = 0x38u, .position = 4u, .size = 1u},
    {.field = Field::CONSTANT_BANK_VECTORS, .name = "fox::gr::dg::ShadowConstantRegister bank: number of float4 vectors", .value = 0x18u, .source = Entry::SHADOW_CONSTANT_REGISTER_FLUSH, .instruction = 0xFAu, .position = 3u, .size = 1u},
    {.field = Field::CONSTANT_BANK_COUNT, .name = "fox::gr::dg::ShadowConstantRegister: bank count", .value = 0x108u, .source = Entry::SHADOW_CONSTANT_REGISTER_DUPLICATE, .instruction = 0x74u, .position = 3u, .size = 4u},
    {.field = Field::CONSTANT_BANKS_DIRTY, .name = "fox::gr::dg::ShadowConstantRegister: dirty banks", .value = 0x110u, .source = Entry::SHADOW_CONSTANT_REGISTER_SET_VECTOR, .instruction = 0x45u, .position = 2u, .size = 4u},
    {.field = Field::CONSTANT_BANKS_ACTIVE, .name = "fox::gr::dg::ShadowConstantRegister: active banks", .value = 0x114u, .source = Entry::SHADOW_CONSTANT_REGISTER_SET_VECTOR, .instruction = 0x19u, .position = 2u, .size = 4u},

    // Camera and shader manager.
    {.field = Field::CAMERA_CENTER, .name = "fox::gr::Camera: center", .value = 0x118u, .source = Entry::DG_INIT_VIEWPORT, .instruction = 0x2B2u, .position = 4u, .size = 4u},
    {.field = Field::SHADER_MANAGER_PROJECTION, .name = "fox::gr::dg::ShaderManager: active projection", .value = 0x680u, .source = Entry::FORWARD_RENDERING_PRE_EXEC, .instruction = 0x29u, .position = 3u, .size = 4u},
    {.field = Field::SHADER_MANAGER_DIRTY, .name = "fox::gr::dg::ShaderManager: first dirty mask", .value = 0x890u, .source = Entry::FORWARD_RENDERING_PRE_EXEC, .instruction = 0x5Au, .position = 2u, .size = 4u},
    {.field = Field::SHADER_MANAGER_DIRTY_PROJECTION, .name = "fox::gr::dg::ShaderManager: first dirty mask bits a projection commit sets", .value = 0x9u, .source = Entry::FORWARD_RENDERING_PRE_EXEC, .instruction = 0x5Au, .position = 6u, .size = 1u},
    {.field = Field::SHADER_MANAGER_DIRTY_SECOND, .name = "fox::gr::dg::ShaderManager: second dirty mask", .value = 0x894u, .source = Entry::FORWARD_RENDERING_PRE_EXEC, .instruction = 0x61u, .position = 2u, .size = 4u},
    {.field = Field::SHADER_MANAGER_DIRTY_SECOND_PROJECTION, .name = "fox::gr::dg::ShaderManager: second dirty mask bits a projection commit sets", .value = 0x8u, .source = Entry::FORWARD_RENDERING_PRE_EXEC, .instruction = 0x61u, .position = 6u, .size = 1u},

    // Draw packets and sky parameters.
    {.field = Field::LIGHT_PACKET_PROJECTION, .name = "local-light draw packet: projection and view", .value = 0x1A0u, .source = Entry::LOCAL_LIGHT_MAIN_EXEC, .instruction = 0xA94u, .position = 4u, .size = 4u},
    {.field = Field::LIGHT_PACKET_RECTANGLE, .name = "local-light draw packet: viewport rectangle", .value = 0x370u, .source = Entry::LOCAL_LIGHT_MAIN_EXEC, .instruction = 0x7D1u, .position = 3u, .size = 4u},
    {.field = Field::DRAW_PACKET_VIEW, .name = "spherical-harmonic lighting and terrain decal draw packet: view", .value = 0x120u, .source = Entry::TERRAIN_DECAL_DRAW_ON_JOB, .instruction = 0xC1u, .position = 3u, .size = 4u},
    {.field = Field::DRAW_PACKET_PROJECTION, .name = "spherical-harmonic lighting and terrain decal draw packet: projection", .value = 0x160u, .source = Entry::TERRAIN_DECAL_DRAW_ON_JOB, .instruction = 0xFDu, .position = 3u, .size = 4u},
    {.field = Field::SKY_PARAMETERS_INVERSE_PROJECTION, .name = "sky shader parameters: inverse projection rows", .value = 0x40u, .source = Entry::SKY_CALC_SHADER_PARAMETERS, .instruction = 0x30Eu, .position = 3u, .size = 1u},

    // fox::PauseSystemImpl.
    {.field = Field::PAUSE_FLAGS, .name = "fox::PauseSystemImpl: pause flags of every active pause source, combined once per game frame", .value = 0x8u, .source = Entry::GAME_TIME_UPDATE, .instruction = 0x24u, .position = 2u, .size = 1u},
    {.field = Field::PAUSE_FLAGS_GAME_TIME, .name = "fox::PauseSystemImpl pause flags: any of these stops game time", .value = 0x7FFFFFFFu, .source = Entry::GAME_TIME_UPDATE, .instruction = 0x24u, .position = 3u, .size = 4u},
}};

inline constexpr bool FIELDS_WELL_FORMED = [] {
  for (size_t index = 0u; index < FIELD_COUNT; ++index) {
    if (FIELDS[index].field != static_cast<Field>(index)) return false;
    if (FIELDS[index].size != 1u && FIELDS[index].size != 4u) return false;
  }
  return true;
}();
static_assert(FIELDS_WELL_FORMED, "FIELDS must list every field in the order of Field, each 1 or 4 bytes");

constexpr uint32_t Value(Field field) {
  return FIELDS[static_cast<size_t>(field)].value;
}

// The entries and fields one feature needs. A feature whose entries and fields
// do not all verify stays off, and the menu names it.
struct Requirement {
  std::string_view feature;
  std::span<const Entry> entries;
  std::span<const Field> fields;
};

}  // namespace mgsv::engine

#endif  // RENODX_GAMES_MGSV_ENGINE_BUILD_HPP_
