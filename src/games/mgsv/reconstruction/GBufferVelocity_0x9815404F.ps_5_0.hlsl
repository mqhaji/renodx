// Port of GBufferVelocity_ps, the G-buffer pass that writes the motion of
// moving objects. It runs only while temporal reconstruction is on, and writes
// the motion in precise form (rendering/motion.hlsli) in place of the game's
// 8-bit encoding.
#include "../rendering/motion.hlsli"

cbuffer cPSSystem : register(b0) {
  struct {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem : packoffset(c0);
}

// The input signature matches the game's vertex shader: TEXCOORD9 shares a
// register with TEXCOORD0 and holds the current and previous w.
void main(
    float4 v0 : SV_Position0,
    float4 v1 : COLOR0,
    float2 v2 : TEXCOORD0,
    float2 w2 : TEXCOORD9,
    float3 v3 : TEXCOORD5,
    float3 v4 : TEXCOORD6,
    float3 v5 : TEXCOORD7,
    float4 v6 : TEXCOORD8,
    out float4 o0 : SV_Target0) {
  o0 = ComputePreciseObjectMotion(v6, w2, g_psSystem.m_renderInfo.xy);
}
