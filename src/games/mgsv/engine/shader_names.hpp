// SPDX-License-Identifier: MIT
#ifndef RENODX_GAMES_MGSV_ENGINE_SHADER_NAMES_HPP_
#define RENODX_GAMES_MGSV_ENGINE_SHADER_NAMES_HPP_

#include <cstdint>

// The game's shaders that feature code recognizes by CRC32, named after the
// engine source file each was compiled from, as recorded in the shader's debug
// information. The game's shaders never change, so a CRC32 is a permanent
// name. A ported shader's registration in shaders.hpp keeps the bare hash,
// because the same hash names its file and embedded code.
namespace mgsv::engine::shaders {

inline constexpr uint32_t BLUR_KAWASE_PS = 0x9BE51873u;                      // blur_kawase_ps.hlsl
inline constexpr uint32_t COPY_REDUCTION_X4_PS = 0x72C1D157u;                // CopyReductionX4_ps.hlsl
inline constexpr uint32_t COPY_RENDER_BUFFER_PS = 0x83272BCBu;               // CopyRenderBuffer_ps.hlsl
inline constexpr uint32_t DOF_SCATTER_BAKE_FIRST_PS = 0xFE1DC3F8u;           // DOF_ScatterBakeFirst_ps.hlsl
inline constexpr uint32_t DOF_SCATTER_BAKE_SECOND_PS = 0x490E22EBu;          // DOF_ScatterBakeSecond_ps.hlsl
inline constexpr uint32_t DOF_SCATTER_COMPOSITE_FAR_PS = 0x7C017264u;        // DOF_ScatterCompositeFar_ps.hlsl
inline constexpr uint32_t DOF_SCATTER_COMPOSITE_FINAL_PS = 0xFC5542BBu;      // DOF_ScatterCompositeFinal_ps.hlsl
inline constexpr uint32_t DOF_SCATTER_COMPOSITE_NEAR_PS = 0xE2D609B1u;       // DOF_ScatterCompositeNear_ps.hlsl
inline constexpr uint32_t DOF_SCATTER_GATHER1_PS = 0xE26983EEu;              // DOF_ScatterGather1_ps.hlsl
inline constexpr uint32_t DOF_SCATTER_SPRITE_PS = 0xD05F5ABCu;               // DOF_ScatterSprite_ps.hlsl
inline constexpr uint32_t DRAW2D_TPP_LENS_FLARE_PS = 0x93A0C8D8u;            // Draw2D_TppLensFlare_ps.hlsl
inline constexpr uint32_t FXAA_PS = 0x900968FFu;                             // Fxaa.hlsl
inline constexpr uint32_t GAMMA_CONTROL_PS = 0x1FF96E86u;                    // GammaControl_ps.hlsl
inline constexpr uint32_t GBUFFER_MASKED_VELOCITY_PS = 0x58C10658u;          // GBufferMaskedVelocity_ps.hlsl
inline constexpr uint32_t GBUFFER_MASKED_VELOCITY_VS = 0x7B809E72u;          // GBufferMaskedVelocity_vs.hlsl
inline constexpr uint32_t GBUFFER_VELOCITY_PS = 0x9815404Fu;                 // GBufferVelocity_ps.hlsl
inline constexpr uint32_t GBUFFER_VELOCITY_VS = 0x1D2531B7u;                 // GBufferVelocity_vs.hlsl
inline constexpr uint32_t MOTION_BLUR_BAKE_VELOCITY_DEPTH_PS = 0x512E2B48u;  // MotionBlurBakeVelocityDepth_ps.hlsl
inline constexpr uint32_t MOTION_BLUR_MCGUIRE_PS = 0xBFC7D3C2u;              // MotionBlurMcGuire_ps.hlsl
inline constexpr uint32_t SSLENS_FLARE_PASS1_PS = 0x1AC1A5EFu;               // SSLensFlarePass1_ps.hlsl
inline constexpr uint32_t SSLENS_FLARE_PASS2_PS = 0x265053A6u;               // SSLensFlarePass2_ps.hlsl
inline constexpr uint32_t TONEMAP_1DLUT_PS = 0xC0C26E46u;                    // Tonemap_1DLUT_ps.hlsl
inline constexpr uint32_t TONEMAP_BRIGHTPASS_PS = 0x463195C2u;               // TonemapBrightpass_ps.hlsl
inline constexpr uint32_t TONEMAP_PS = 0xE04D1471u;                          // Tonemap_ps.hlsl

}  // namespace mgsv::engine::shaders

#endif  // RENODX_GAMES_MGSV_ENGINE_SHADER_NAMES_HPP_
