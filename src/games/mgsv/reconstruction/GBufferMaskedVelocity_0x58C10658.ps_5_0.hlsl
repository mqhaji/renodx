// Port of GBufferMaskedVelocity_ps, the alpha-tested form of GBufferVelocity_ps.
// It runs only while temporal reconstruction is on, keeps the game's two alpha
// tests, and writes the motion in precise form (rendering/motion.hlsli) in
// place of the game's 8-bit encoding.
#include "../rendering/motion.hlsli"

cbuffer cPSSystem : register(b0) {
  struct {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem : packoffset(c0);
}

SamplerState g_sampler_diffuse_s : register(s0);
SamplerState g_samplerPoint_Wrap_s : register(s8);
Texture2D<float4> g_tex_diffuse : register(t0);
Texture2D<float4> g_tex_mesh : register(t15);

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
  // The game's alpha tests, against g_tex_mesh tiled every 8 pixels across the
  // screen: the diffuse alpha against its alpha times m_param.w, and the vertex
  // alpha against its red channel.
  const float alpha = g_tex_diffuse.Sample(g_sampler_diffuse_s, v2.xy).w;
  const float2 pattern = g_tex_mesh.Sample(g_samplerPoint_Wrap_s, (v0.xy - 0.5) * 0.125).xw;
  if (alpha - pattern.y * g_psSystem.m_param.w < 0.0) discard;
  if (v1.w - pattern.x < 0.0) discard;
  o0 = ComputePreciseObjectMotion(v6, w2, g_psSystem.m_renderInfo.xy);
}
