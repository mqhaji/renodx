#include "./shared.h"

// ---- Created with 3Dmigoto v1.4.1 on Sat Oct 18 15:10:50 2025

cbuffer PerObjectConsts : register(b1) {
  float4x4 worldViewMatrix : packoffset(c0);
  float4x4 worldViewProjectionMatrix : packoffset(c4);
  float4x4 inverseTransposeWorldViewMatrix : packoffset(c8);
  float4x4 inverseWorldViewMatrix : packoffset(c12);
  float4 clipSpaceLookupScale : packoffset(c16);
  float4 clipSpaceLookupOffset : packoffset(c17);
  float4 particleCloudColor : packoffset(c18);
  float4 particleCloudMatrix : packoffset(c19);
  float4 particleCloudVelWorld : packoffset(c20);
  float4 codeMeshArg[2] : packoffset(c21);
  float4 scriptVector0 : packoffset(c23);
  float4 scriptVector1 : packoffset(c24);
  float4 scriptVector2 : packoffset(c25);
  float4 scriptVector3 : packoffset(c26);
  float4 scriptVector4 : packoffset(c27);
  float4 scriptVector5 : packoffset(c28);
  float4 scriptVector6 : packoffset(c29);
  float4 scriptVector7 : packoffset(c30);
  float4 weaponParam0 : packoffset(c31);
  float4 weaponParam1 : packoffset(c32);
  float4 weaponParam2 : packoffset(c33);
  float4 weaponParam3 : packoffset(c34);
  float4 weaponParam4 : packoffset(c35);
  float4 weaponParam5 : packoffset(c36);
  float4 weaponParam6 : packoffset(c37);
  float4 weaponParam7 : packoffset(c38);
  float4 weaponParam8 : packoffset(c39);
  float4 weaponParam9 : packoffset(c40);
  float4 flagParams : packoffset(c41);
  float3 occlusionAmount : packoffset(c42);
  float4 colorObjMin : packoffset(c43);
  float4 colorObjMax : packoffset(c44);
  float colorObjMinBaseBlend : packoffset(c45);
  float colorObjMaxBaseBlend : packoffset(c45.y);
  float2 uvScroll : packoffset(c45.z);
  float4 featherParms : packoffset(c46);
  float4 falloffParms : packoffset(c47);
  float4 falloffBeginColor : packoffset(c48);
  float4 falloffEndColor : packoffset(c49);
  float4 eyeOffsetParms : packoffset(c50);
  float4 alphaDissolveParms : packoffset(c51);
  float4 spotLightWeight : packoffset(c52);
  float4 detailScale : packoffset(c53);
  float4 detailScale1 : packoffset(c54);
  float4 detailScale2 : packoffset(c55);
  float4 detailScale3 : packoffset(c56);
  float4 detailScale4 : packoffset(c57);
  float4 alphaRevealParms : packoffset(c58);
  float4 alphaRevealParms1 : packoffset(c59);
  float4 alphaRevealParms2 : packoffset(c60);
  float4 alphaRevealParms3 : packoffset(c61);
  float4 alphaRevealParms4 : packoffset(c62);
  float4 colorDetailScale : packoffset(c63);
  float4 colorTint : packoffset(c64);
}

SamplerState colorMapSampler_s : register(s0);
Texture2D<float4> colorMapSampler : register(t0);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position0,
    float4 v1: TEXCOORD0,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2, r3, r4;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = float4(16, 16, 16, 16) + v1.zwzw;
  r0.xyzw = float4(0.03125, 0.03125, 0.03125, 0.03125) * r0.xyzw;
  r0.xyzw = floor(r0.xyzw);
  r0.xy = float2(0.0166666675, 0.0444444455) * r0.xy;
  r1.xyzw = r0.zwzw * float4(0.00833333377, 0.0222222228, 0.00833333377, 0.0222222228) + float4(0.666666687, 0, 0.666666687, 0.5);
  r0.xyzw = colorMapSampler.Sample(colorMapSampler_s, r0.xy).wxyz;
  r2.xyzw = colorMapSampler.Sample(colorMapSampler_s, r1.xy).xyzw;
  r1.xyzw = colorMapSampler.Sample(colorMapSampler_s, r1.zw).xyzw;
  r0.z = r1.w;
  r0.y = r2.w;
  r0.xyz = float3(-0, -0.5, -0.5) + r0.xyz;
  r0.w = 0.580600023 * r0.z;
  r0.w = r0.y * -0.394650012 + -r0.w;
  r1.y = r0.x + r0.w;
  r1.xz = r0.zy * float2(1.13982999, 2.03221011) + r0.xx;
  r0.xy = float2(0.000781250012, 0.00138888892) * v1.xy;
  r2.xyzw = max(float4(0.000390625006, 0, 0.000781250012, 0.00138888892), r0.xyxy);
  r2.xyzw = min(float4(0.999609351, 1, 0.999218762, 0.998611093), r2.xyzw);
  r0.zw = float2(0.666666687, 1) * r2.xy;
  r2.xyzw = r2.zwzw * float4(0.333333343, 0.5, 0.333333343, 0.5) + float4(0.666666687, 0, 0.666666687, 0.5);
  r3.xyzw = colorMapSampler.Sample(colorMapSampler_s, r0.zw).xyzw;
  r3.x = r3.w;
  r0.z = cmp(0.0156862754 >= r3.w);
  r4.xyzw = colorMapSampler.Sample(colorMapSampler_s, r2.xy).xyzw;
  r2.xyzw = colorMapSampler.Sample(colorMapSampler_s, r2.zw).xyzw;
  r3.z = r2.w;
  r3.y = r4.w;
  r2.xyz = float3(-0, -0.5, -0.5) + r3.xyz;
  r0.w = 0.580600023 * r2.z;
  r0.w = r2.y * -0.394650012 + -r0.w;
  r3.y = r2.x + r0.w;
  r3.xz = r2.zy * float2(1.13982999, 2.03221011) + r2.xx;
  r2.xyz = r0.zzz ? float3(0, 0, 0) : r3.xyz;
  r1.xyz = -r2.xyz + r1.xyz;
  r0.zw = cmp(r0.xy >= scriptVector0.xz);
  r0.xy = cmp(scriptVector0.yw >= r0.xy);
  r0.x = r0.x ? r0.z : 0;
  r0.x = r0.w ? r0.x : 0;
  r0.x = r0.y ? r0.x : 0;
  r0.x = r0.x ? 1.000000 : 0;
  o0.xyz = r0.xxx * r1.xyz + r2.xyz;
  o0.w = 1;
  return;
}
