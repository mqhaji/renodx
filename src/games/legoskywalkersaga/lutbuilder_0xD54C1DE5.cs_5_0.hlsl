#include "./shared.h"
// ---- Created with 3Dmigoto v1.4.1 on Fri Aug 22 07:09:18 2025

// clang-format off
cbuffer g_MainFilterPS_CB : register(b0) {
  struct
  {
    float4 glareParams;
    float4 fxaaParams;
    float4 mainFilterToneMapping;
    float4 mainFilterDof;
    float4 pixel_size;
    float4 naughtyTonemapParams1;
    float4 naughtyTonemapParams2;
    float4 vignetteQrc;
    float4 vignetteViewport;
    float4 vignetteParams;
    float4 textureMips;
  } g_MainFilterPS: packoffset(c0);
}

cbuffer g_ColourProcessPS_CB : register(b1) {
  struct
  {
    float4 colorUVScale;
    float4 colorUVBias;
    float4 colorUVScaleB;
    float4 colorUVBiasB;
  } g_ColourProcessPS: packoffset(c0);
}
// clang-format on

SamplerState AllPointClampSampler_s : register(s0);
SamplerState MinMagLinearMipDisabledClampSampler_s : register(s1);
Texture3D<float4> cubeTexA : register(t0);
Texture3D<float4> cubeTexB : register(t1);
RWTexture3D<float3> ColorCubeRWTexture : register(u0);

// 3Dmigoto declarations
#define cmp -

[numthreads(4, 4, 4)]
void main(uint3 vThreadID: SV_DispatchThreadID) {
  // Needs manual fix for instruction:
  // unknown dcl_: dcl_uav_typed_texture3d (float,float,float,float) u0
  float4 r0, r1, r2, r3, r4;
  uint4 bitmask, uiDest;
  float4 fDest;

  // Needs manual fix for instruction:
  // unknown dcl_: dcl_thread_group 4, 4, 4
  r0.xyz = (uint3)vThreadID.xyz;
  r0.xyz = float3(0.015625, 0.03125, 0.0625) * r0.xyz;
  r0.xyz = cubeTexA.SampleLevel(AllPointClampSampler_s, r0.xyz, 0).xyz;
  r0.xyz = r0.xyz * float3(32, 32, 32) + float3(1, 1, 1);
  r0.xyz = log2(r0.xyz);
  r0.xyz = g_ColourProcessPS.colorUVScale.xyz * r0.xyz;
  r0.xyz = r0.xyz * float3(0.130948052, 0.130948052, 0.130948052) + g_ColourProcessPS.colorUVBias.xyz;
  r0.xyz = cubeTexB.SampleLevel(MinMagLinearMipDisabledClampSampler_s, r0.xyz, 0).xyz;
  r0.xyz = r0.xyz * r0.xyz;
  r0.xyz = g_MainFilterPS.mainFilterToneMapping.yyy * r0.xyz;
  float tonemapping_mode = g_MainFilterPS.mainFilterToneMapping.x;  // SDR - 1, HDR - 3

  if (tonemapping_mode == 1.0) {  // SDR

    r1.xyz = g_MainFilterPS.naughtyTonemapParams1.xxx * r0.xyz + g_MainFilterPS.naughtyTonemapParams1.zzz;
    r1.xyz = r0.xyz * r1.xyz + g_MainFilterPS.naughtyTonemapParams1.www;
    r2.xyz = g_MainFilterPS.naughtyTonemapParams1.xxx * r0.xyz + g_MainFilterPS.naughtyTonemapParams1.yyy;
    r2.xyz = r0.xyz * r2.xyz + g_MainFilterPS.naughtyTonemapParams2.xxx;
    r1.xyz = r1.xyz / r2.xyz;
    r1.xyz = r1.xyz * g_MainFilterPS.naughtyTonemapParams2.zzz + -g_MainFilterPS.naughtyTonemapParams2.yyy;

  } else {
    if (tonemapping_mode == 3.0) {  // HDR
      r2.xyz = r0.xyz * float3(15.8000002, 15.8000002, 15.8000002) + float3(2.11999989, 2.11999989, 2.11999989);
      r2.xyz = r2.xyz * r0.xyz;
      r3.xyz = r0.xyz * float3(1.20000005, 1.20000005, 1.20000005) + float3(5.92000008, 5.92000008, 5.92000008);
      r3.xyz = r0.xyz * r3.xyz + float3(1.89999998, 1.89999998, 1.89999998);
      r1.xyz = r2.xyz / r3.xyz;
    } else {
      if (tonemapping_mode == 2.0) {
        r2.x = dot(float3(0.597190022, 0.354579985, 0.0482299998), r0.xyz);
        r2.y = dot(float3(0.0759999976, 0.908339977, 0.0156599991), r0.xyz);
        r2.z = dot(float3(0.0284000002, 0.133829996, 0.837769985), r0.xyz);
        r3.xyz = float3(0.0245785993, 0.0245785993, 0.0245785993) + r2.xyz;
        r3.xyz = r2.xyz * r3.xyz + float3(-9.05370034e-05, -9.05370034e-05, -9.05370034e-05);
        r4.xyz = r2.xyz * float3(0.983729005, 0.983729005, 0.983729005) + float3(0.432951003, 0.432951003, 0.432951003);
        r2.xyz = r2.xyz * r4.xyz + float3(0.238080993, 0.238080993, 0.238080993);
        r2.xyz = r3.xyz / r2.xyz;

        // AP1 -> BT.709
        r3.x = saturate(dot(float3(1.60475004, -0.531080008, -0.0736699998), r2.xyz));
        r3.y = saturate(dot(float3(-0.102080002, 1.10812998, -0.00604999997), r2.xyz));
        r3.z = saturate(dot(float3(-0.00326999999, -0.0727600008, 1.07602), r2.xyz));

        r1.xyz = sqrt(r3.xyz);
      } else {
        r1.xyz = sqrt(r0.xyz);
      }
    }
  }
  r0.x = cmp(0 != g_MainFilterPS.mainFilterToneMapping.z);
  r2.xyzw = r1.xyzx * r1.xyzx;
  r0.xyzw = r0.xxxx ? r2.xyzw : r1.xyzx;
  // No code for instruction (needs manual fix):
  ColorCubeRWTexture[vThreadID] = r0.rgb;  // store_uav_typed u0.xyzw, vThreadID.xyzz, r0.xyzw
  return;
}
