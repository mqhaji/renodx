#include "./shared.h"

// ---- Created with 3Dmigoto v1.4.1 on Sat Oct 18 15:10:58 2025

cbuffer PerSceneConsts : register(b0) {
  float4 windDirection : packoffset(c0);
  float4 adsZScale : packoffset(c1);
  float4 variantWindSpring[16] : packoffset(c2);
  float4 sunPosition : packoffset(c18);
  float4 sunDiffuse : packoffset(c19);
  float4 hdrControl0 : packoffset(c20);
  float4 hdrControl1 : packoffset(c21);
  float4 dlightShadowLookupMatrix0 : packoffset(c22);
  float4 dlightShadowLookupMatrix1 : packoffset(c23);
  float4 dlightShadowLookupMatrix2 : packoffset(c24);
  float4 dlightShadowLookupMatrix3 : packoffset(c25);
  float4 fogColor : packoffset(c26);
  float4 fogConsts : packoffset(c27);
  float4 fogConsts2 : packoffset(c28);
  float3 sunFogDir : packoffset(c29);
  float4 sunFogColor : packoffset(c30);
  float2 sunFog : packoffset(c31);
  float4x4 projectionMatrix : packoffset(c32);
  float4x4 viewProjectionMatrix : packoffset(c36);
  float4x4 inverseViewMatrix : packoffset(c40);
  float4x4 inverseViewProjectionMatrix : packoffset(c44);
  float4x4 shadowLookupMatrix : packoffset(c48);
  float4x4 worldOutdoorLookupMatrix : packoffset(c52);
  float4 lightingLookupScale : packoffset(c56);
  float4 lightHeroScale : packoffset(c57);
  float4 zNear : packoffset(c58);
  float4 outdoorFeatherParms : packoffset(c59);
  float4 glightPosXs : packoffset(c60);
  float4 glightPosYs : packoffset(c61);
  float4 glightPosZs : packoffset(c62);
  float4 glightFallOffs : packoffset(c63);
  float4 glightReds : packoffset(c64);
  float4 glightGreens : packoffset(c65);
  float4 glightBlues : packoffset(c66);
  float4 dlightPosition : packoffset(c67);
  float4 dlightDiffuse : packoffset(c68);
  float4 dlightAttenuation : packoffset(c69);
  float4 dlightFallOff : packoffset(c70);
  float4 dlightSpotMatrix0 : packoffset(c71);
  float4 dlightSpotMatrix1 : packoffset(c72);
  float4 dlightSpotMatrix2 : packoffset(c73);
  float4 dlightSpotMatrix3 : packoffset(c74);
  float4 dlightSpotDir : packoffset(c75);
  float4 dlightSpotFactors : packoffset(c76);
  float4 lightPosition : packoffset(c77);
  float4 lightDiffuse : packoffset(c78);
  float4 lightSpotDir : packoffset(c79);
  float4 lightSpotFactors : packoffset(c80);
  float4 lightAttenuation : packoffset(c81);
  float4 lightFallOffA : packoffset(c82);
  float4 lightFallOffB : packoffset(c83);
  float4 lightSpotMatrix0 : packoffset(c84);
  float4 lightSpotMatrix1 : packoffset(c85);
  float4 lightSpotMatrix2 : packoffset(c86);
  float4 lightSpotMatrix3 : packoffset(c87);
  float4 lightSpotAABB : packoffset(c88);
  float4 lightConeControl1 : packoffset(c89);
  float4 lightConeControl2 : packoffset(c90);
  float4 lightSpotCookieSlideControl : packoffset(c91);
  float4 spotShadowmapPixelAdjust : packoffset(c92);
  float4 dlightSpotShadowmapPixelAdjust : packoffset(c93);
  float4 _characterCharredAmount : packoffset(c94);
  float4 renderTargetSize : packoffset(c95);
  float4 upscaledTargetSize : packoffset(c96);
  float4 shadowmapSwitchPartition : packoffset(c97);
  float4 shadowmapPolygonOffset : packoffset(c98);
  float4 sunShadowmapPixelSize : packoffset(c99);
  float4 materialColor : packoffset(c100);
  float4 skyTransition : packoffset(c101);
  float rimIntensity : packoffset(c102);
  float4 filterTap[8] : packoffset(c103);
  float4 cameraUp : packoffset(c111);
  float4 cameraLook : packoffset(c112);
  float4 cameraSide : packoffset(c113);
  float4 heroLightingR : packoffset(c114);
  float4 heroLightingG : packoffset(c115);
  float4 heroLightingB : packoffset(c116);
  float4 eyeOffset : packoffset(c117);
  float4 genericEyeOffset : packoffset(c118);
  float4 genericQuadIntensity : packoffset(c119);
  float4 postFxControl0 : packoffset(c120);
  float4 postFxControl1 : packoffset(c121);
  float4 postFxControl2 : packoffset(c122);
  float4 postFxControl3 : packoffset(c123);
  float4 postFxControl4 : packoffset(c124);
  float4 postFxControl5 : packoffset(c125);
  float4 postFxControl6 : packoffset(c126);
  float4 postFxControl7 : packoffset(c127);
  float4 postFxControl8 : packoffset(c128);
  float4 postFxControl9 : packoffset(c129);
  float4 postFxControlA : packoffset(c130);
  float4 postFxControlB : packoffset(c131);
  float4 postFxControlC : packoffset(c132);
  float4 postFxControlD : packoffset(c133);
  float4 postFxControlE : packoffset(c134);
  float4 postFxControlF : packoffset(c135);
  float4 cloudLayerControl0 : packoffset(c136);
  float4 cloudLayerControl1 : packoffset(c137);
  float4 cloudLayerControl2 : packoffset(c138);
  float4 cloudLayerControl3 : packoffset(c139);
  float4 emblemLUTSelector : packoffset(c140);
  float skyColorMultiplier : packoffset(c141);
  float extraCamParam : packoffset(c141.y);
  float4 glowSetup : packoffset(c142);
  float4 glowApply : packoffset(c143);
  float4 colorMatrixR : packoffset(c144);
  float4 colorMatrixG : packoffset(c145);
  float4 colorMatrixB : packoffset(c146);
  float4 colorTintBase : packoffset(c147);
  float4 colorTintDelta : packoffset(c148);
  float4 colorBias : packoffset(c149);
  float4 cinematicBlurBox : packoffset(c150);
  float4 cinematicBlurBox2 : packoffset(c151);
}

SamplerState codeTexture0_s : register(s0);
Texture2D<float4> codeTexture0 : register(t0);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position0,
    float4 v1: TEXCOORD0,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = codeTexture0.Sample(codeTexture0_s, v1.xy).xyzw;
  r1.xyzw = codeTexture0.Sample(codeTexture0_s, v1.zy).xyzw;
  r0.xyz = r1.xyz + r0.xyz;
  r1.xyzw = codeTexture0.Sample(codeTexture0_s, v1.xw).xyzw;
  r0.xyz = r1.xyz + r0.xyz;
  r1.xyzw = codeTexture0.Sample(codeTexture0_s, v1.zw).xyzw;
  r0.xyz = r1.xyz + r0.xyz;
  r0.xyz = float3(0.25, 0.25, 0.25) * r0.xyz;
  r0.xyz = r0.xyz * r0.xyz;
  r0.xyz = float3(4, 4, 4) * r0.xyz;
  r0.w = dot(r0.xyz, float3(0.212585449, 0.715194702, 0.0722198486));
  r1.xyzw = -postFxControl6.xxxy + r0.wwww;
  r2.xyzw = postFxControl6.zzzw + -postFxControl6.xxxy;
  r2.xyzw = float4(1, 1, 1, 1) / r2.xyzw;
  r1.xyzw = saturate(r2.xyzw * r1.xyzw);
  r2.xyzw = r1.zzzw * float4(-2, -2, -2, -2) + float4(3, 3, 3, 3);
  r1.xyzw = r1.xyzw * r1.xyzw;
  r1.xyzw = r2.xyzw * r1.xyzw;
  r0.xyzw = r1.xyzw * r0.xyzw;
  r0.xyzw = (r0.xyzw * postFxControl7.xyzw + postFxControl8.xyzw);

  // r0 = saturate(r0);
  // r0.xyzw = log2(r0.xyzw);
  // r0.xyzw = postFxControl9.xyzw * r0.xyzw;
  // r0.xyzw = exp2(r0.xyzw);
  r0 = renodx::math::SignPow(r0, postFxControl9);
  o0.xyzw = r0.xyzw * postFxControlA.xyzw + postFxControlB.xyzw;
  return;
}
