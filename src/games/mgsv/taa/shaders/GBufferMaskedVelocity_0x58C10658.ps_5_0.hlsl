// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:26 2026

#include "../../shared.h"

// clang-format off
cbuffer cPSSystem : register(b0) {
    struct
    {
        float4 m_param;
        float4 m_renderInfo;
        float4 m_renderBuffer;
        float4 m_dominantLightDir;
    } g_psSystem:
    packoffset(c0);
}
// clang-format on

SamplerState g_sampler_diffuse_s : register(s0);
SamplerState g_samplerPoint_Wrap_s : register(s8);
Texture2D<float4> g_tex_diffuse : register(t0);
Texture2D<float4> g_tex_mesh : register(t15);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position0,
    float4 v1: COLOR0,
    float2 v2: TEXCOORD0,
    float2 w2: TEXCOORD9,
    float4 v3: TEXCOORD5,
    float4 v4: TEXCOORD6,
    float4 v5: TEXCOORD7,
    float4 v6: TEXCOORD8,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5, -0.5) + v0.xy;
  r1.xyzw = v6.xyzw;
  r2.xyzw = w2.xxyy;
  r1.xyzw = r1.xyzw;
  r2.xyzw = r2.xyzw;
  r0.zw = g_psSystem.m_renderInfo.xy;
  r0.zw = float2(0.5, 0.5) * r0.zw;
  r0.zw = r0.zw / float2(64, 64);
  r1.xyzw = r1.xyzw / r2.xyzw;
  r1.zw = -r1.zw;
  r1.xy = r1.xy + r1.zw;
  r0.zw = r1.xy * r0.zw;
  r1.x = dot(r0.zw, r0.zw);
  r1.x = sqrt(r1.x);
  r1.y = cmp(0 < r1.x);
  r0.zw = r0.zw / r1.xx;
  r1.x = max(0, r1.x);
  if (CUSTOM_TAA == 0.f || CUSTOM_UNCLAMP_MOTION_VECTORS == 0.f) r1.x = min(1, r1.x);
  r0.zw = r1.xx * r0.zw;
  r1.xy = r1.yy ? r0.zw : float2(0, 0);
  r1.z = -r1.y;
  r0.zw = float2(0.5, 0.5) * r1.xz;
  r1.zw = float2(0.5, 0.5) + r0.zw;
  r1.zw = r1.zw;
  r0.z = g_tex_diffuse.Sample(g_sampler_diffuse_s, v2.xy).w;
  r0.z = r0.z;
  r0.z = r0.z;
  r0.z = r0.z;
  r0.w = v1.w;
  r0.xy = r0.xy;
  r0.z = r0.z;
  r0.w = r0.w;
  r0.xy = r0.xy;
  r2.x = g_psSystem.m_param.w;
  r0.xy = float2(0.125, 0.125) * r0.xy;
  r0.xy = g_tex_mesh.Sample(g_samplerPoint_Wrap_s, r0.xy).xw;
  r0.xy = r0.xy;
  r0.y = r0.y * r2.x;
  r0.y = -r0.y;
  r0.y = r0.z + r0.y;
  r0.y = cmp(r0.y < 0);
  if (r0.y != 0) discard;
  r0.x = -r0.x;
  r0.x = r0.w + r0.x;
  r0.x = cmp(r0.x < 0);
  if (r0.x != 0) discard;
  r1.zw = r1.zw;
  r1.zw = r1.zw;
  r1.y = 0;
  r1.x = 1;
  r1.zw = r1.zw;
  r1.xyzw = r1.xyzw;
  o0.xyzw = r1.xyzw;
  return;
}
