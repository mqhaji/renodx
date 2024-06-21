#include "../../shaders/color.hlsl"
#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Thu Jun 06 03:06:14 2024

cbuffer PER_BATCH : register(b0)
{
  float4 HDRParams5 : packoffset(c0);
  float4 HDRParams1 : packoffset(c1);
  float4 HistogramParams : packoffset(c2);
  float4 SunShafts_SunCol : packoffset(c3);
  float4 HDRParams8 : packoffset(c4);
  float4 HDRParams0 : packoffset(c5);
  float4 PS_NearFarClipDist : packoffset(c6);
  float4 HDRParams4 : packoffset(c7);
  float4 MotionBlurVersion : packoffset(c8);
  float4 PS_SceneRenderSize : packoffset(c9);
  float4 HDRParams6 : packoffset(c10);
  float4 HDRParams2 : packoffset(c11);
  float4 PS_ScreenSize : packoffset(c12);
  float4 HDRDofParams : packoffset(c13);
  float4 HDRParams7 : packoffset(c14);
}

SamplerState baseMap_s : register(s0);
SamplerState lumMap_s : register(s1);
SamplerState bloomMap0_s : register(s2);
SamplerState sceneScaledMap0_s : register(s3);
SamplerState depthMap_s : register(s5);
SamplerState vignettingMap_s : register(s7);
SamplerState colorChartMap_s : register(s8);
SamplerState sunshaftsMap_s : register(s9);
Texture2D<float4> baseMap : register(t0);
Texture2D<float4> lumMap : register(t1);
Texture2D<float4> bloomMap0 : register(t2);
Texture2D<float4> sceneScaledMap0 : register(t3);
Texture2D<float4> depthMap : register(t5);
Texture2D<float4> vignettingMap : register(t7);
Texture2D<float4> colorChartMap : register(t8);
Texture2D<float4> sunshaftsMap : register(t9);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float4 v1 : TEXCOORD0,
  float3 v2 : TEXCOORD1,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyz = baseMap.Sample(baseMap_s, v1.xy).xyz;
  r0.w = vignettingMap.Sample(vignettingMap_s, v1.xy).x;
  r1.x = cmp(MotionBlurVersion.x == 0.000000);
  if (r1.x != 0) {
    r1.x = depthMap.Sample(depthMap_s, v1.xy).x;
    r1.x = PS_NearFarClipDist.y * r1.x;
    r1.x = saturate(r1.x * HDRDofParams.x + HDRDofParams.y);
    r2.xyzw = sceneScaledMap0.Sample(sceneScaledMap0_s, v1.xy).xyzw;
    r1.x = saturate(r2.w + r1.x);
    r1.yzw = r2.xyz + -r0.xyz;
    r0.xyz = r1.xxx * r1.yzw + r0.xyz;
  }
  if (injectedData.toneMapType == 0) {
    r0.xyz = HDRParams2.yyy * r0.xyz;
    r1.xyzw = bloomMap0.Sample(bloomMap0_s, v1.xy).xyzw;
    r1.w = r1.w * r1.w;
    r1.w = 32 * r1.w;
    r1.xyz = r1.xyz * r1.www;
    r2.xyz = float3(0.375,0.375,0.375) * HDRParams5.xyz;
    r1.xyz = r2.xyz * r1.xyz;
    r1.w = lumMap.Sample(lumMap_s, v1.xy).x;
    r2.x = saturate(dot(r0.xyz, float3(0.212599993,0.715200007,0.0722000003))); // saturate
    r2.y = 1 + -HDRParams2.z;
    r2.x = r2.x * r2.y + HDRParams2.z;
    r0.xyz = log2(r0.xyz);
    r0.xyz = r2.xxx * r0.xyz;
    r0.xyz = exp2(r0.xyz);
    r2.x = 1 + r1.w;
    r2.x = log2(r2.x);
    r2.x = 2 + r2.x;
    r2.x = 2 / r2.x;
    r2.x = 1.04999995 + -r2.x;
    r1.w = r2.x / r1.w;
    r1.w = max(HDRParams1.x, r1.w);
    r1.w = min(HDRParams1.z, r1.w);
    r0.xyz = r1.www * r0.xyz + r1.xyz;
    r1.xyz = r0.www * r0.xyz;
    r1.x = dot(r1.xyz, float3(0.212599993,0.715200007,0.0722000003));
    r0.xyz = r0.www * r0.xyz + -r1.xxx;
    r0.xyz = r0.xyz * float3(0.899999976,0.899999976,0.899999976) + r1.xxx;
    r0.xyz = HDRParams4.xyz * r0.xyz;
    r0.xyz = max(float3(0,0,0), r0.xyz);
    r1.xyzw = float4(0.219999999,0.300000012,0.0300000012,0.00200000009) * HDRParams8.xyyz;
    r0.w = HDRParams8.w;
    r2.xyzw = r1.xxxx * r0.xyzw + r1.zzzz;
    r2.xyzw = r0.xyzw * r2.xyzw + r1.wwww;
    r1.xyzw = r1.xxxx * r0.xyzw + r1.yyyy;
    r0.xyzw = r0.xyzw * r1.xyzw + float4(0.0599999987,0.0599999987,0.0599999987,0.0599999987);
    r0.xyzw = r2.xyzw / r0.xyzw;
    r0.xyzw = -HDRParams8.zzzz * float4(0.0333333313,0.0333333313,0.0333333313,0.0333333313) + r0.xyzw;
    r0.xyz = max(0, r0.xyz / r0.www); //  r0.xyz = saturate(r0.xyz / r0.www);
  }
  r0.xyz = log2(r0.xyz);
  r0.xyz = float3(0.454545468,0.454545468,0.454545468) * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  r1.xyz = sunshaftsMap.Sample(sunshaftsMap_s, v1.xy).xyz;
  r1.xyz = SunShafts_SunCol.xyz * r1.xyz;
  r2.xyz = float3(1,1,1) + -r0.xyz;
  r0.xyz = max(0, r1.xyz * r2.xyz + r0.xyz);  //  r0.xyz = saturate(r1.xyz * r2.xyz + r0.xyz);
  r0.yzw = r0.xyz * float3(0.9375,0.9375,0.9375) + float3(0.03125,0.03125,0);
  r1.x = 16 * r0.w;
  r1.x = frac(r1.x);
  r0.w = r0.w * 16 + -r1.x;
  r0.y = r0.y + r0.w;
  r0.x = 0.0625 * r0.y;
  r1.yzw = colorChartMap.Sample(colorChartMap_s, r0.xz).xyz;
  r0.xy = float2(0.0625,0) + r0.xz;
  r0.xyz = colorChartMap.Sample(colorChartMap_s, r0.xy).xyz;
  r0.xyz = r0.xyz + -r1.yzw;
  r0.xyz = r0.xyz * r1.xxx + r1.yzw;
  r0.w = dot(v0.xy, float2(34.4830017,89.637001));
  r0.w = sin(r0.w);
  r1.xyz = float3(29156.4766,38273.5625,47843.7539) * r0.www;
  r1.xyz = frac(r1.xyz);
  r2.xy = float2(0.57889998,0.57889998) + v0.xy;
  r0.w = dot(r2.xy, float2(34.4830017,89.637001));
  r0.w = sin(r0.w);
  r2.xyz = float3(29156.4766,38273.5625,47843.7539) * r0.www;
  r2.xyz = frac(r2.xyz);
  r1.xyz = r2.xyz + r1.xyz;
  r1.xyz = float3(-0.5,-0.5,-0.5) + r1.xyz;
  o0.xyz = r1.xyz * float3(0.00392156886,0.00392156886,0.00392156886) + r0.xyz;
  o0.w = 1;
  return;
}