#include "./common.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:33 2026

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
  }
g_psScene:
  packoffset(c0);
}

cbuffer cPSObject : register(b5) {
  struct
  {
    float4x4 m_viewWorld;
    float4x4 m_world;
    float4 m_useWeightCount;
    float4 m_localParam[4];
  }
g_psObject:
  packoffset(c0);
}

cbuffer cPSSystem : register(b0) {
  struct
  {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  }
g_psSystem:
  packoffset(c0);
}

SamplerState g_samplerPoint_Clamp_s : register(s9);
SamplerState g_samplerLinear_Wrap_s : register(s10);
SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inDepthImage : register(t0);
Texture2D<float4> inColorImage : register(t1);
Texture2D<float4> inNormalImage : register(t2);
Texture2D<float4> inRoughnessImage : register(t3);
Texture2D<float4> g_tex_fog : register(t12);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position0,
    float2 v1: TEXCOORD0,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = float4(-0.5, -0.5, -0.5, -0.5) + v0.xyxy;
  r1.xy = r0.zw;
  r1.xy = r1.xy;
  r1.xy = float2(0.49609375, 0.49609375) + r1.xy;
  r1.xy = g_psSystem.m_renderBuffer.zw * r1.xy;
  r1.xy = r1.xy;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw / g_psSystem.m_renderInfo.xyxy;
  r0.xyzw = float4(2, -2, 2, -2) * r0.xyzw;
  r0.xyzw = float4(-1, 1, -1, 1) + r0.xyzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r1.xy = r1.xy;
  r0.xyzw = r0.xyzw;
  r1.xy = r1.xy;
  r1.xy = r1.xy;
  r1.zw = g_psScene.m_projectionParam.zw;
  r2.x = inDepthImage.SampleLevel(g_samplerPoint_Clamp_s, r1.xy, 0).x;
  r2.x = r2.x;
  r2.x = r2.x;
  r1.zw = r1.zw;
  r1.w = -r1.w;
  r1.w = r2.x + r1.w;
  r1.z = r1.z / r1.w;
  r1.z = r1.z;
  r1.z = r1.z;
  r0.xyzw = r0.xyzw;
  r1.z = r1.z;
  r0.xyzw = r0.xyzw;
  r1.z = r1.z;
  r1.z = r1.z;
  r1.w = g_psScene.m_fogParam[1].x;
  r1.z = log2(r1.z);
  r1.z = r1.w * r1.z;
  r1.z = r1.z;
  r1.z = max(0, r1.z);
  r1.z = min(1, r1.z);
  r1.z = 127 * r1.z;
  r0.xyzw = float4(0.0146484375, 0.123046875, 0.0146484375, 0.123046875) * r0.xyzw;
  r0.xyzw = float4(0.015625, 0.125, 0.015625, 0.125) + r0.xyzw;
  r1.w = 1 + r1.z;
  r1.w = max(0, r1.w);
  r2.w = min(127, r1.w);
  r2.y = r1.z;
  r2.yz = floor(r2.yw);
  r2.yz = r2.yz / float2(32, 32);
  r3.xy = frac(r2.yz);
  r3.xz = float2(32, 32) * r3.xy;
  r3.yw = floor(r2.yz);
  r3.xyzw = float4(0.03125, 0.25, 0.03125, 0.25) * r3.xyzw;
  r0.xyzw = r3.xyzw + r0.xyzw;
  r1.z = frac(r1.z);
  r0.x = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r0.xy).w;
  r0.x = r0.x;
  r0.y = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r0.zw).w;
  r0.y = r0.y;
  r0.z = -r1.z;
  r0.z = 1 + r0.z;
  r0.x = r0.z * r0.x;
  r0.y = r1.z * r0.y;
  r0.x = r0.x + r0.y;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.yz = v1.xy;
  r0.w = g_psObject.m_localParam[0].w;
  r0.x = r0.x;
  r0.yz = r0.yz;
  r0.w = r0.w;
  r0.yz = r0.yz;
  r3.xyzw = g_psScene.m_projectionParam.xyzw;
  r0.yz = r0.yz;
  r3.xyzw = r3.xyzw;
  r1.z = -r3.w;
  r1.z = r2.x + r1.z;
  r2.z = r3.z / r1.z;
  r4.z = r2.z;
  r0.yz = r3.xy * r0.yz;
  r2.xy = r0.yz * r4.zz;
  r4.xy = r2.xy;
  r4.xy = r4.xy;
  r4.z = r4.z;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r0.y = inRoughnessImage.Sample(g_samplerLinear_Wrap_s, r1.xy).x;
  r0.y = r0.y;
  r0.y = -r0.y;
  r0.y = 1 + r0.y;
  r0.y = r0.y * r0.y;
  r0.y = r0.y * r0.y;
  r0.y = 1 * r0.y;
  r0.z = r4.z / 100;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = -r0.z;
  r0.z = 1 + r0.z;
  r0.w = cmp(r0.w < r4.z);
  r1.xyz = inNormalImage.Sample(g_samplerPoint_Clamp_s, r1.xy).xyz;
  if (r0.w == 0) {
    r0.w = cmp(r0.z == 0.000000);
    if (r0.w == 0) {
      r2.w = 1;
      r3.x = dot(r2.xyzw, g_psScene.m_shadowProjection._m00_m10_m20_m30);
      r3.y = dot(r2.xyzw, g_psScene.m_shadowProjection._m01_m11_m21_m31);
      r3.z = dot(r2.xyzw, g_psScene.m_shadowProjection._m02_m12_m22_m32);
      r3.w = dot(r2.xyzw, g_psScene.m_shadowProjection._m03_m13_m23_m33);
      r3.xyzw = r3.xyzw;
      r2.xyz = r3.xyz / r3.www;
      r1.xy = float2(2, 2) * r1.xy;
      r1.xy = float2(-1, -1) + r1.xy;
      r0.w = r1.z * r1.z;
      r0.w = 2 * r0.w;
      r3.z = -1 + r0.w;
      r0.w = r3.z * r3.z;
      r0.w = -r0.w;
      r0.w = 1 + r0.w;
      r1.zw = r1.xy * r0.ww;
      r1.x = dot(r1.xy, r1.xy);
      r0.w = r1.x * r0.w;
      r0.w = 1.00000001e-07 + r0.w;
      r0.w = rsqrt(r0.w);
      r3.xy = r1.zw * r0.ww;
      r3.xy = r3.xy;
      r3.z = r3.z;
      r3.xyz = r3.xyz;
      r3.xyz = r3.xyz;
      r0.w = dot(r4.xyz, r4.xyz);
      r0.w = rsqrt(r0.w);
      r1.xyz = r4.xyz * r0.www;
      r0.w = dot(r1.xyz, r3.xyz);
      r0.w = r0.w + r0.w;
      r0.w = -r0.w;
      r5.xyz = r3.xyz * r0.www;
      r1.xyz = r5.xyz + r1.xyz;
      r0.w = cmp(r1.z < -0.5);
      if (r0.w == 0) {
        r5.xyz = float3(0.0500000007, 0.0500000007, 0.0500000007) * r3.xyz;
        r1.xyz = r5.xyz + r1.xyz;
        r1.xyw = r1.xyz * r4.zzz;
        r5.xyz = r4.xyz + r1.xyw;
        r5.w = 1;
        r6.x = dot(r5.xyzw, g_psScene.m_shadowProjection._m00_m10_m20_m30);
        r6.y = dot(r5.xyzw, g_psScene.m_shadowProjection._m01_m11_m21_m31);
        r6.z = dot(r5.xyzw, g_psScene.m_shadowProjection._m02_m12_m22_m32);
        r6.w = dot(r5.xyzw, g_psScene.m_shadowProjection._m03_m13_m23_m33);
        r6.xyzw = r6.xyzw;
        r1.xyw = r6.xyz / r6.www;
        r1.xyw = r1.xyw;
        r2.xyz = r2.xyz;
        r1.xyw = r1.xyw;
        r2.xyz = float3(0.5, -0.5, 0.5) * r2.xyz;
        r2.xyz = float3(0.5, 0.5, 0.5) + r2.xyz;
        r1.xyw = float3(0.5, -0.5, 0.5) * r1.xyw;
        r1.xyw = float3(0.5, 0.5, 0.5) + r1.xyw;
        r5.xy = -r2.xy;
        r5.xy = r5.xy + r1.xy;
        r0.w = dot(r5.xy, r5.xy);
        r0.w = sqrt(r0.w);
        r5.xyz = -r2.xyz;
        r1.xyw = r5.xyz + r1.xyw;
        r1.xyw = r1.xyw / r0.www;
        r0.w = 0.5 * g_psObject.m_localParam[3].z;
        r1.xyw = r1.xyw * r0.www;
        r2.xyz = r2.xyz;
        r1.xyw = r1.xyw;
        r0.w = 1;
        r2.w = (int)g_psObject.m_localParam[3].w;
        r3.w = (int)r2.w + 1;
        r5.xyzw = (int4)r3.wwww;
        r6.xyzw = float4(1, 2, 3, 4) / r5.xyzw;
        r3.w = (int)r3.w;
        r4.w = 1 / r3.w;
        r7.x = -r1.w;
        r7.x = max(r7.x, r1.w);
        r4.w = r7.x * r4.w;
        r4.w = max(0.000159999996, r4.w);
        r7.x = 0.00499999989;
        r7.y = 0;
        r8.xy = r6.xy;
        r8.zw = r6.zw;
        r7.z = r0.w;
        r9.x = r7.x;
        r7.w = r7.y;
        while (true) {
          r10.x = cmp((int)r7.w < (int)r2.w);
          if (r10.x == 0) break;
          r10.xyzw = r8.xxyy * r1.xyxy;
          r10.xyzw = r10.xyzw + r2.xyxy;
          r11.xyzw = r8.zzww * r1.xyxy;
          r11.xyzw = r11.xyzw + r2.xyxy;
          r12.xyzw = r8.xyzw * r1.wwww;
          r12.xyzw = r12.xyzw + r2.zzzz;
          r13.xyzw = cmp(r12.xyzw < float4(0, 0, 0, 0));
          r12.xyzw = r13.xyzw ? float4(1, 1, 1, 1) : r12.xyzw;
          r10.xyzw = g_psObject.m_localParam[2].xyxy * r10.xyzw;
          r11.xyzw = g_psObject.m_localParam[2].xyxy * r11.xyzw;
          r13.x = inDepthImage.SampleLevel(g_samplerPoint_Clamp_s, r10.xy, 0).x;
          r13.x = r13.x;
          r13.y = inDepthImage.SampleLevel(g_samplerPoint_Clamp_s, r10.zw, 0).x;
          r13.y = r13.y;
          r13.z = inDepthImage.SampleLevel(g_samplerPoint_Clamp_s, r11.xy, 0).x;
          r13.z = r13.z;
          r13.w = inDepthImage.SampleLevel(g_samplerPoint_Clamp_s, r11.zw, 0).x;
          r13.w = r13.w;
          r10.xy = float2(2, 2) * r10.xy;
          r10.xy = float2(-1, -1) + r10.xy;
          r14.xy = -r10.xy;
          r10.xy = max(r14.xy, r10.xy);
          r10.xy = cmp(float2(1, 1) < r10.xy);
          r10.x = (int)r10.y | (int)r10.x;
          r10.x = cmp((int)r10.x != 0);
          r14.x = r10.x ? 0 : r13.x;
          r10.xy = float2(2, 2) * r10.zw;
          r10.xy = float2(-1, -1) + r10.xy;
          r10.zw = -r10.xy;
          r10.xy = max(r10.xy, r10.zw);
          r10.xy = cmp(float2(1, 1) < r10.xy);
          r10.x = (int)r10.y | (int)r10.x;
          r10.x = cmp((int)r10.x != 0);
          r14.y = r10.x ? 0 : r13.y;
          r10.xy = float2(2, 2) * r11.xy;
          r10.xy = float2(-1, -1) + r10.xy;
          r10.zw = -r10.xy;
          r10.xy = max(r10.xy, r10.zw);
          r10.xy = cmp(float2(1, 1) < r10.xy);
          r10.x = (int)r10.y | (int)r10.x;
          r10.x = cmp((int)r10.x != 0);
          r14.z = r10.x ? 0 : r13.z;
          r10.xy = float2(2, 2) * r11.zw;
          r10.xy = float2(-1, -1) + r10.xy;
          r10.zw = -r10.xy;
          r10.xy = max(r10.xy, r10.zw);
          r10.xy = cmp(float2(1, 1) < r10.xy);
          r10.x = (int)r10.y | (int)r10.x;
          r10.x = cmp((int)r10.x != 0);
          r14.w = r10.x ? 0 : r13.w;
          r10.xyzw = -r14.xyzw;
          r10.xyzw = r12.xyzw + r10.xyzw;
          r11.xyzw = r10.xyzw + r4.wwww;
          r12.xyzw = -r11.xyzw;
          r11.xyzw = max(r12.xyzw, r11.xyzw);
          r11.xyzw = cmp(r11.xyzw < r4.wwww);
          r12.xy = (int2)r11.zw | (int2)r11.xy;
          r12.x = (int)r12.y | (int)r12.x;
          if (r12.x != 0) {
            r9.x = r9.x;
            r9.yzw = r10.xyz;
            r12.xyzw = -r10.xyzw;
            r12.xyzw = r12.xyzw + r9.xyzw;
            r12.xyzw = r9.xyzw / r12.xyzw;
            r12.xyzw = max(float4(0, 0, 0, 0), r12.xyzw);
            r12.xyzw = min(float4(1, 1, 1, 1), r12.xyzw);
            r12.xyzw = float4(-1, -1, -1, -1) + r12.xyzw;
            r12.xyzw = r12.xyzw / r5.xyzw;
            r12.xyzw = r12.xyzw + r8.xyzw;
            r11.xyzw = r11.xyzw ? r12.xyzw : float4(1, 1, 1, 1);
            r9.yz = min(r11.xy, r11.zw);
            r9.y = min(r9.y, r9.z);
            r7.z = min(1, r9.y);
            break;
          }
          r9.x = r10.w;
          r9.y = 4 / r3.w;
          r8.xyzw = r9.yyyy + r8.xyzw;
          r7.w = (int)r7.w + 4;
          r7.z = 1;
        }
        r1.xy = r7.zz * r1.xy;
        r1.xy = r2.xy + r1.xy;
        r7.z = r7.z;
        r0.w = cmp(r7.z < 1);
        if (r0.w != 0) {
          r0.w = min(0, r1.z);
          r0.w = r0.w / -0.5;
          r0.w = max(0, r0.w);
          r0.w = min(1, r0.w);
          r0.w = -r0.w;
          r0.w = 1 + r0.w;
          r2.xyz = -r4.xyz;
          r1.z = dot(r2.xyz, r2.xyz);
          r1.z = rsqrt(r1.z);
          r2.xyz = r2.xyz * r1.zzz;
          r1.z = dot(r3.xyz, r2.xyz);
          r1.z = max(0, r1.z);
          r1.z = min(1, r1.z);
          r1.z = -r1.z;
          r1.z = 1 + r1.z;
          r1.w = -r0.y;
          r1.w = 1 + r1.w;
          r1.z = r1.z * r1.w;
          r0.y = r1.z + r0.y;
          r1.zw = float2(-0.5, -0.5) + r1.xy;
          r1.z = dot(r1.zw, r1.zw);
          r1.z = sqrt(r1.z);
          r1.z = 2 * r1.z;
          r1.z = max(0, r1.z);
          r1.z = min(1, r1.z);
          r1.w = r1.z * r1.z;
          r1.w = r1.w * r1.z;
          r1.z = r1.w * r1.z;
          r1.z = -r1.z;
          r1.z = 1 + r1.z;
          r1.xy = g_psObject.m_localParam[2].xy * r1.xy;
          r1.xyw = inColorImage.SampleLevel(g_samplerLinear_Clamp_s, r1.xy, 0).xyz;
#if FIX_UNORM_SRGB
          r1.xyw = renodx::color::srgb::Decode(max(0, r1.xyw));
#endif

          r1.xyw = r1.xyw;
          r2.x = -r7.z;
          r2.x = 1 + r2.x;
          r0.y = r2.x * r0.y;
          r3.xyz = r1.xyw * r0.yyy;
          r0.y = min(r1.z, r0.z);
          r0.y = r0.y * r0.w;
          r3.w = r2.x * r0.y;
        } else {
          r3.xyzw = float4(0, 0, 0, 0);
        }
        r0.x = r0.x * r0.x;
        r0.x = r0.x * r0.x;
        r0.x = r0.x * r0.x;
        r0.x = 1 * r0.x;
        r0.xyzw = r3.xyzw * r0.xxxx;
      } else {
        r0.xyzw = float4(0, 0, 0, 0);
      }
    } else {
      r0.xyzw = float4(0, 0, 0, 0);
    }
  } else {
    r0.xyzw = float4(0, 0, 0, 0);
  }
  r0.xyzw = r0.xyzw;
  o0.xyzw = r0.xyzw;
  return;
}
