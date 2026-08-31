// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:29 2026

#include "../../shared.h"

// clang-format off
cbuffer cPSSystem : register(b0) {
  struct
  {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  }
g_psSystem: packoffset(c0);
}
// clang-format on

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

  r0.xyzw = v6.xyzw;
  r1.xyzw = w2.xxyy;
  r0.xyzw = r0.xyzw;
  r1.xyzw = r1.xyzw;
  r2.xy = g_psSystem.m_renderInfo.xy;
  r2.xy = float2(0.5, 0.5) * r2.xy;
  r2.xy = r2.xy / float2(64, 64);
  r0.xyzw = r0.xyzw / r1.xyzw;
  r0.zw = -r0.zw;
  r0.xy = r0.xy + r0.zw;
  r0.xy = r2.xy * r0.xy;
  r0.z = dot(r0.xy, r0.xy);
  r0.z = sqrt(r0.z);
  r0.w = cmp(0 < r0.z);
  r0.xy = r0.xy / r0.zz;
  r0.z = max(0, r0.z);
  if (CUSTOM_TAA == 0.f || CUSTOM_UNCLAMP_MOTION_VECTORS == 0.f) r0.z = min(1, r0.z);
  r0.xy = r0.xy * r0.zz;
  r0.xy = r0.ww ? r0.xy : float2(0, 0);
  r0.z = -r0.y;
  r0.xy = float2(0.5, 0.5) * r0.xz;
  r0.zw = float2(0.5, 0.5) + r0.xy;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r0.y = 0;
  r0.x = 1;
  r0.zw = r0.zw;
  r0.xyzw = r0.xyzw;
  o0.xyzw = r0.xyzw;
  return;
}
