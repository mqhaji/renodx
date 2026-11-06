// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:24 2026

cbuffer cPSObject : register(b5) {
  struct
  {
    float4x4 m_viewWorld;
    float4x4 m_world;
    float4 m_useWeightCount;
    float4 m_localParam[4];
  } g_psObject : packoffset(c0);
}

cbuffer cPSSystem : register(b0) {
  struct
  {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem : packoffset(c0);
}

SamplerState g_samplerPoint_Clamp_s : register(s9);
SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inTexture : register(t0);
Texture2D<float4> inTexture2 : register(t1);
Texture2D<float4> inMaskTexture : register(t2);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0 : SV_Position0,
    float2 v1 : TEXCOORD0,
    out float4 o0 : SV_Target0,
    out float4 o1 : SV_Target1,
    out float4 o2 : SV_Target2) {
  float4 r0, r1, r2, r3, r4, r5, r6;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = v1.xy;
  r0.xy = r0.xy;
  r0.xy = g_psSystem.m_renderInfo.xy * r0.xy;
  r0.xy = g_psSystem.m_renderBuffer.zw * r0.xy;
  r1.xyz = inTexture.Sample(g_samplerLinear_Clamp_s, r0.xy).xyz;
  r1.xyz = r1.xyz;
  r2.xyz = inTexture2.Sample(g_samplerLinear_Clamp_s, r0.xy).xyz;
  r2.xyz = r2.xyz;
  r0.zw = float2(-0.25, 0.25) * g_psSystem.m_renderBuffer.zw;
  r0.zw = r0.xy + r0.zw;
  r3.x = inTexture.Sample(g_samplerPoint_Clamp_s, r0.zw).w;
  r3.x = r3.x;
  r4.xy = float2(0.25, 0.25) * g_psSystem.m_renderBuffer.zw;
  r4.xy = r4.xy + r0.xy;
  r3.y = inTexture.Sample(g_samplerPoint_Clamp_s, r4.xy).w;
  r3.y = r3.y;
  r4.zw = float2(0.25, -0.25) * g_psSystem.m_renderBuffer.zw;
  r4.zw = r4.zw + r0.xy;
  r3.z = inTexture.Sample(g_samplerPoint_Clamp_s, r4.zw).w;
  r3.z = r3.z;
  r5.xy = float2(-0.25, -0.25) * g_psSystem.m_renderBuffer.zw;
  r5.xy = r5.xy + r0.xy;
  r3.w = inTexture.Sample(g_samplerPoint_Clamp_s, r5.xy).w;
  r3.w = r3.w;
  r6.x = inTexture2.Sample(g_samplerPoint_Clamp_s, r0.zw).w;
  r6.x = r6.x;
  r6.y = inTexture2.Sample(g_samplerPoint_Clamp_s, r4.xy).w;
  r6.y = r6.y;
  r6.z = inTexture2.Sample(g_samplerPoint_Clamp_s, r4.zw).w;
  r6.z = r6.z;
  r6.w = inTexture2.Sample(g_samplerPoint_Clamp_s, r5.xy).w;
  r6.w = r6.w;
  r3.x = r3.x;
  r3.y = r3.y;
  r3.z = r3.z;
  r3.w = r3.w;
  r6.x = r6.x;
  r6.y = r6.y;
  r6.z = r6.z;
  r6.w = r6.w;
  r4.xyzw = cmp(float4(0, 0, 0, 0) >= r3.xyzw);
  r4.xyzw = r4.xyzw ? float4(1, 1, 1, 1) : float4(0, 0, 0, 0);
  r5.xyzw = cmp(r6.xyzw >= float4(0, 0, 0, 0));
  r5.xyzw = r5.xyzw ? float4(1, 1, 1, 1) : float4(0, 0, 0, 0);
  r0.z = dot(r4.xyzw, r4.xyzw);
  r0.z = cmp(r0.z == 0.000000);
  if (r0.z != 0) {
    r4.xyzw = float4(1, 1, 1, 1);
  }
  r0.z = dot(r5.xyzw, r5.xyzw);
  r0.z = cmp(r0.z == 0.000000);
  if (r0.z != 0) {
    r5.xyzw = float4(1, 1, 1, 1);
  }
  r3.xyzw = float4(-0, -0, -0, -0) + r3.xyzw;
  r3.xyzw = r4.xyzw * r3.xyzw;
  r3.xyzw = float4(0, 0, 0, 0) + r3.xyzw;
  r4.xyzw = float4(-0, -0, -0, -0) + r6.xyzw;
  r4.xyzw = r5.xyzw * r4.xyzw;
  r4.xyzw = float4(0, 0, 0, 0) + r4.xyzw;
  r0.z = min(r3.x, r3.y);
  r0.w = min(r3.z, r3.w);
  r1.w = min(r0.z, r0.w);
  r0.z = max(r4.x, r4.y);
  r0.w = max(r4.z, r4.w);
  r2.w = max(r0.z, r0.w);
  r0.z = g_psObject.m_localParam[3].x;
  r0.w = g_psObject.m_localParam[3].y;
  r0.z = -r0.z;
  r3.x = r0.w + r0.z;
  r3.y = r2.w + r0.z;
  r3.y = r3.y / r3.x;
  r3.y = 4 * r3.y;
  r3.y = -3 + r3.y;
  r3.y = max(0, r3.y);
  r3.y = min(1, r3.y);
  r3.y = -r3.y;
  r3.y = 1 + r3.y;
  r3.z = -r1.w;
  r0.z = r3.z + r0.z;
  r0.z = r0.z / r3.x;
  r0.z = 4 * r0.z;
  r0.z = -3 + r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = -r0.z;
  r0.z = 1 + r0.z;
  r4.xyzw = inMaskTexture.Sample(g_samplerLinear_Clamp_s, r0.xy).xyzw;
  r5.x = r4.x + r4.z;
  r5.y = r4.y + r4.w;
  r0.x = cmp(r0.w >= r2.w);
  r0.y = -r5.x;
  r0.y = 1 + r0.y;
  r0.y = r0.y * r3.y;
  r5.z = r0.x ? r0.y : 0;
  r0.x = cmp(r0.w >= r3.z);
  r0.y = -r5.y;
  r0.y = 1 + r0.y;
  r0.y = r0.y * r0.z;
  r5.w = r0.x ? r0.y : 0;
  r1.xyz = r1.xyz;
  r1.w = r1.w;
  r2.xyz = r2.xyz;
  r2.w = r2.w;
  r5.x = r5.x;
  r5.y = r5.y;
  r5.z = r5.z;
  r5.w = r5.w;
  r1.xyz = r1.xyz;
  r1.w = r1.w;
  r2.xyz = r2.xyz;
  r2.w = r2.w;
  r5.x = r5.x;
  r5.y = r5.y;
  r5.z = r5.z;
  r5.w = r5.w;
  o0.xyzw = r1.xyzw;
  o1.xyzw = r2.xyzw;
  // The reused bloom target is now float; retain its original UNORM range.
  o2.xyzw = saturate(r5.xyzw);
  return;
}