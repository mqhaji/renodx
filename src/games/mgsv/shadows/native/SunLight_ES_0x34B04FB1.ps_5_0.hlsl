// Native environment-only lighting; capture its existing specular output separately.
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:23 2026

cbuffer cPSScene : register(b2) {
  struct
  {
    float4x4 m_projectionView;
    float4x4 m_projection;
    float4x4 m_view;
    float4x4 m_shadowProjection;
    float4x4 m_shadowProjection2;
    float4 m_eyepos;
    float4 m_projectionParam;
    float4 m_viewportSize;
    float4 m_exposure;
    float4 m_fogParam[3];
    float4 m_fogColor;
    float4 m_cameraCenterOffset;
    float4 m_shadowMapResolutions;
  } g_psScene : packoffset(c0);
}

cbuffer cPSLight : register(b3) {
  struct
  {
    float4 m_lightParams[11];
  } g_psLight : packoffset(c0);
}

SamplerState g_samplerPoint_Wrap_s : register(s8);
SamplerState g_samplerPoint_Clamp_s : register(s9);
SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> g_NormalTexture : register(t0);
Texture2D<float4> g_SpecularTexture : register(t1);
Texture2D<float4> g_DepthTexture : register(t2);
Texture2D<float4> g_lut1 : register(t9);
Texture2D<float4> g_MaterialTexture : register(t10);
Texture2D<float4> g_SkyReflectTexture : register(t15);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0 : SV_Position0,
    float4 v1 : TEXCOORD0,
    float4 v2 : TEXCOORD1,
    out float4 o0 : SV_Target0,
    out float4 o1 : SV_Target1,
    out float4 o2 : SV_Target2,
    out float4 specular_reset : SV_Target3,
    out float4 composition_reset : SV_Target4) {
  float4 r0, r1, r2, r3;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyz = v2.xyw;
  r0.xyz = r0.xyz;
  r0.xy = r0.xy / r0.zz;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r1.xyz = g_NormalTexture.Sample(g_samplerPoint_Wrap_s, r0.xy).xyz;
  r1.xyz = r1.xyz;
  r2.xyz = g_SpecularTexture.Sample(g_samplerPoint_Wrap_s, r0.xy).yxz;
  r2.xyz = r2.xyz;
  r0.x = g_DepthTexture.Sample(g_samplerPoint_Wrap_s, r0.xy).x;
  r0.x = r0.x;
  r1.xyz = r1.xyz;
  r0.yz = float2(2, 2) * r1.xy;
  r0.yz = float2(-1, -1) + r0.yz;
  r0.w = r1.z * r1.z;
  r0.w = 2 * r0.w;
  r1.z = -1 + r0.w;
  r0.w = r1.z * r1.z;
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r3.xy = r0.yz * r0.ww;
  r0.y = dot(r0.yz, r0.yz);
  r0.y = r0.w * r0.y;
  r0.y = 1.00000001e-07 + r0.y;
  r0.y = rsqrt(r0.y);
  r1.xy = r3.xy * r0.yy;
  r1.xy = r1.xy;
  r1.z = r1.z;
  r1.xyz = r1.xyz;
  r0.y = r2.y;
  r2.x = r2.x;
  r2.z = r2.z;
  r1.xyz = r1.xyz;
  r2.z = r2.z;
  r2.x = r2.x;
  r0.y = r0.y;
  r0.x = r0.x;
  r0.x = r0.x;
  r3.xyz = v1.xyw;
  r0.x = r0.x;
  r3.xyz = r3.xyz;
  r0.zw = r3.xy / r3.zz;
  r3.xy = -g_psScene.m_cameraCenterOffset.xy;
  r0.zw = r3.xy + r0.zw;
  r0.x = r0.x;
  r3.xyzw = g_psScene.m_projectionParam.xyzw;
  r0.zw = r3.xy * r0.zw;
  r1.w = -r3.w;
  r0.x = r1.w + r0.x;
  r3.z = r3.z / r0.x;
  r3.xy = r3.zz * r0.zw;
  r3.xy = r3.xy;
  r3.z = r3.z;
  r3.xyz = r3.xyz;
  r0.x = dot(r3.xyz, r3.xyz);
  r0.x = rsqrt(r0.x);
  r0.xzw = r3.xyz * r0.xxx;
  r0.xzw = r0.xzw;
  r1.xyz = r1.xyz;
  r0.xzw = r0.xzw;
  r2.x = r2.x;
  r2.z = r2.z;
  r0.y = r0.y;
  r1.xyz = r1.xyz;
  r0.xzw = r0.xzw;
  r2.x = r2.x;
  r2.z = r2.z;
  r0.y = r0.y;
  r0.xzw = r0.xzw;
  r1.xyz = r1.xyz;
  r2.z = r2.z;
  r2.y = 0.25;
  r3.xyz = g_MaterialTexture.Sample(g_samplerPoint_Clamp_s, r2.zy).xyz;
  r3.xyz = r3.xyz;
  r2.y = 0.75;
  r1.w = g_MaterialTexture.Sample(g_samplerPoint_Clamp_s, r2.zy).x;
  r3.xyz = r3.xyz;
  r1.w = r1.w;
  r2.y = dot(r0.xzw, r1.xyz);
  r2.y = r2.y + r2.y;
  r2.y = -r2.y;
  r2.yzw = r2.yyy * r1.xyz;
  r2.yzw = r2.yzw + r0.xzw;
  r0.xzw = -r0.xzw;
  r0.x = dot(r0.xzw, r1.xyz);
  r0.x = max(0, r0.x);
  r1.x = min(1, r0.x);
  r1.y = max(g_psLight.m_lightParams[2].z, r1.w);
  r0.x = g_lut1.Sample(g_samplerLinear_Clamp_s, r1.xy).x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.zw = -r2.yz;
  r0.zw = r0.zw;
  r0.zw = float2(0.96875, 0.96875) * r0.zw;
  r0.zw = float2(0.5, 0.5) * r0.zw;
  r0.zw = float2(0.5, 0.5) + r0.zw;
  r0.zw = float2(0.25, 0.125) * r0.zw;
  r0.zw = r0.zw;
  r1.x = cmp(0 < r2.w);
  r1.xy = r1.xx ? float2(0.25, 0) : float2(0, 0);
  r0.zw = r1.xy + r0.zw;
  r0.zw = g_SkyReflectTexture.Sample(g_samplerLinear_Clamp_s, r0.zw).xy;
  r0.zw = float2(4, 4) * r0.zw;
  r1.x = 1.42799997 * r0.y;
  r1.x = -0.300000012 + r1.x;
  r1.x = max(0, r1.x);
  r1.x = min(1, r1.x);
  r1.y = -r0.z;
  r0.w = r1.y + r0.w;
  r0.w = r1.x * r0.w;
  r1.xyz = r0.zzz + r0.www;
  r2.yzw = g_psLight.m_lightParams[3].xyz;
  r1.xyz = r2.yzw * r1.xyz;
  r0.y = -r0.y;
  r0.y = 1 + r0.y;
  r0.y = 0.5 * r0.y;
  r0.x = r0.y * r0.x;
  r0.x = r0.x * r2.x;
  r0.xyz = r1.xyz * r0.xxx;
  r0.xyz = r0.xyz * r3.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = max(0, g_psLight.m_lightParams[2].x);
  r0.w = g_psLight.m_lightParams[2].y * r0.w;
  r0.xyz = r0.xyz * r0.www;
  r0.xyz = float3(0, 0, 0) + r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = 1;
  r0.xyz = r0.xyz;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  o1.xyzw = r0.xyzw;
  o0.xyzw = float4(0, 0, 0, 1);
  o2 = o1;
  specular_reset = 0.0;
  composition_reset = 0.0;
  return;
}