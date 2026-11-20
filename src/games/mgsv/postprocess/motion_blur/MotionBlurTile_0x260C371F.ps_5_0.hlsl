// Port of MotionBlurTile_ps, which keeps the longest of four motions around each
// pixel of a smaller tile image. Its first pass reads the combined motion image
// in either form (rendering/motion.hlsli); its later passes read its own 8-bit
// tile images, which are always in the game's form. Everything else follows
// the game's shader.
#include "../../rendering/motion.hlsli"

cbuffer cPSObject : register(b5) {
  struct {
    float4x4 m_viewWorld;
    float4x4 m_world;
    float4 m_useWeightCount;
    float4 m_localParam[4];
  } g_psObject : packoffset(c0);
}

cbuffer cPSSystem : register(b0) {
  struct {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem : packoffset(c0);
}

SamplerState g_samplerPoint_Clamp_s : register(s9);
Texture2D<float4> inVelocity : register(t0);

void main(
    float4 v0 : SV_Position0,
    float2 v1 : TEXCOORD0,
    out float4 o0 : SV_Target0) {
  // m_localParam[0] holds the source pixel size in xy and the largest UV to
  // sample in zw.
  const float2 uv = g_psSystem.m_renderInfo.xy * v1 * g_psSystem.m_renderBuffer.zw;
  const float2 offset = 0.25 * g_psObject.m_localParam[0].xy;
  const float2 clamp_uv = g_psObject.m_localParam[0].zw;
  static const float2 directions[4] = {float2(-1.0, -1.0), float2(1.0, -1.0), float2(-1.0, 1.0), float2(1.0, 1.0)};

  // Longest motion first; a later sample replaces it only if strictly longer.
  float2 longest = 0.0;
  float longest_squared = 0.0;
  [unroll]
  for (int i = 0; i < 4; ++i) {
    const float2 velocity = MotionBlurVelocity(inVelocity.Sample(g_samplerPoint_Clamp_s, min(uv + directions[i] * offset, clamp_uv)));
    const float velocity_squared = dot(velocity, velocity);
    if (velocity_squared > longest_squared) {
      longest = velocity;
      longest_squared = velocity_squared;
    }
  }
  o0 = float4(0.0, 0.0, 0.5 + 0.5 * longest);
}
