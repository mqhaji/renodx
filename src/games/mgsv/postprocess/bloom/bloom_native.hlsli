/*
 * Copyright (C) 2026 Musa Haji
 * SPDX-License-Identifier: MIT
 */

// Native constant buffers and bindings shared by the bloom and lens flare ports.
// The bloom images are RGBA16F, so each vanilla path clamps its output to the
// 0-1 range the game's original UNORM images had.
#ifndef RENODX_GAMES_MGSV_BLOOM_NATIVE_HLSLI_
#define RENODX_GAMES_MGSV_BLOOM_NATIVE_HLSLI_

#include "../../shared.h"
#include "bloom.hlsli"

cbuffer cPSSystem : register(b0) {
  struct {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem;
}

cbuffer cPSObject : register(b5) {
  struct {
    float4x4 m_viewWorld;
    float4x4 m_world;
    float4 m_useWeightCount;
    float4 m_localParam[4];
  } g_psObject;
}

SamplerState g_samplerLinear_Clamp_s : register(s11);
SamplerState g_samplerPoint_Wrap_s : register(s8);
Texture2D<float4> inImage : register(t0);

#endif
