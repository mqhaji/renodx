#include "./shared.h"

// ---- Created with 3Dmigoto v1.4.1 on Sat Oct 18 15:11:07 2025

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

SamplerState colorMapSampler_s : register(s0);
Texture2D<float4> colorMapSampler : register(t0);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position0,
    float4 v1: TEXCOORD0,
    float2 v2: TEXCOORD1,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2, r3, r4, r5, r6, r7, r8;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = colorMapSampler.SampleLevel(colorMapSampler_s, v2.xy, 0).xyzw;

  // o0 = r0;
  // return;

  r1.xyzw = renderTargetSize.zwzw * float4(0, 1, 1, 0) + v2.xyxy;
  r2.xyzw = colorMapSampler.SampleLevel(colorMapSampler_s, r1.xy, 0).xyzw;
  r1.xyzw = colorMapSampler.SampleLevel(colorMapSampler_s, r1.zw, 0).xyzw;
  r3.xyzw = renderTargetSize.zwzw * float4(0, -1, -1, 0) + v2.xyxy;
  r4.xyzw = colorMapSampler.SampleLevel(colorMapSampler_s, r3.xy, 0).xyzw;
  r3.xyzw = colorMapSampler.SampleLevel(colorMapSampler_s, r3.zw, 0).xyzw;
  r1.x = max(r2.w, r0.w);
  r1.y = min(r2.w, r0.w);
  r1.x = max(r1.w, r1.x);
  r1.y = min(r1.w, r1.y);
  r1.z = max(r4.w, r3.w);
  r2.x = min(r4.w, r3.w);
  r1.x = max(r1.z, r1.x);
  r1.y = min(r2.x, r1.y);
  r1.z = 0.165999994 * r1.x;
  r1.x = r1.x + -r1.y;
  r1.y = max(0.0833000019, r1.z);
  r1.y = cmp(r1.x >= r1.y);
  if (r1.y != 0) {
    r1.yz = -renderTargetSize.zw + v2.xy;
    r5.xyzw = colorMapSampler.SampleLevel(colorMapSampler_s, r1.yz, 0).xyzw;
    r1.yz = renderTargetSize.zw + v2.xy;
    r6.xyzw = colorMapSampler.SampleLevel(colorMapSampler_s, r1.yz, 0).xyzw;
    r7.xyzw = renderTargetSize.zwzw * float4(1, -1, -1, 1) + v2.xyxy;
    r8.xyzw = colorMapSampler.SampleLevel(colorMapSampler_s, r7.xy, 0).xyzw;
    r7.xyzw = colorMapSampler.SampleLevel(colorMapSampler_s, r7.zw, 0).xyzw;
    r1.y = r4.w + r2.w;
    r1.z = r3.w + r1.w;
    r1.x = 1 / r1.x;
    r2.x = r1.y + r1.z;
    r1.y = r0.w * -2 + r1.y;
    r1.z = r0.w * -2 + r1.z;
    r2.y = r8.w + r6.w;
    r2.z = r8.w + r5.w;
    r3.x = r1.w * -2 + r2.y;
    r2.z = r4.w * -2 + r2.z;
    r3.y = r7.w + r5.w;
    r3.z = r7.w + r6.w;
    r1.y = abs(r1.y) * 2 + abs(r3.x);
    r1.z = abs(r1.z) * 2 + abs(r2.z);
    r2.z = r3.w * -2 + r3.y;
    r3.x = r2.w * -2 + r3.z;
    r1.y = abs(r2.z) + r1.y;
    r1.z = abs(r3.x) + r1.z;
    r2.y = r3.y + r2.y;
    r1.y = cmp(r1.y >= r1.z);
    r1.z = r2.x * 2 + r2.y;
    r2.x = r1.y ? r4.w : r3.w;
    r1.w = r1.y ? r2.w : r1.w;
    r2.y = r1.y ? renderTargetSize.w : renderTargetSize.z;
    r1.z = r1.z * 0.0833333358 + -r0.w;
    r2.z = r2.x + -r0.w;
    r2.w = r1.w + -r0.w;
    r2.x = r2.x + r0.w;
    r1.w = r1.w + r0.w;
    r3.x = cmp(abs(r2.z) >= abs(r2.w));
    r2.z = max(abs(r2.z), abs(r2.w));
    r2.y = r3.x ? -r2.y : r2.y;
    r1.x = saturate(abs(r1.z) * r1.x);
    r1.z = r1.y ? renderTargetSize.z : 0;
    r2.w = r1.y ? 0 : renderTargetSize.w;
    r3.yz = r2.yy * float2(0.5, 0.5) + v2.xy;
    r3.y = r1.y ? v2.x : r3.y;
    r3.z = r1.y ? r3.z : v2.y;
    r4.x = r3.y + -r1.z;
    r4.y = r3.z + -r2.w;
    r5.x = r3.y + r1.z;
    r5.y = r3.z + r2.w;
    r3.y = r1.x * -2 + 3;
    r6.xyzw = colorMapSampler.SampleLevel(colorMapSampler_s, r4.xy, 0).xyzw;
    r1.x = r1.x * r1.x;
    r7.xyzw = colorMapSampler.SampleLevel(colorMapSampler_s, r5.xy, 0).xyzw;
    r1.w = r3.x ? r2.x : r1.w;
    r2.x = 0.25 * r2.z;
    r2.z = -r1.w * 0.5 + r0.w;
    r1.x = r3.y * r1.x;
    r2.z = cmp(r2.z < 0);
    r3.y = -r1.w * 0.5 + r6.w;
    r3.x = -r1.w * 0.5 + r7.w;
    r4.zw = cmp(abs(r3.yx) >= r2.xx);
    r5.z = -r1.z * 1.5 + r4.x;
    r5.z = r4.z ? r4.x : r5.z;
    r4.x = -r2.w * 1.5 + r4.y;
    r5.w = r4.z ? r4.y : r4.x;
    r4.xy = ~(int2)r4.zw;
    r4.x = (int)r4.y | (int)r4.x;
    r4.y = r1.z * 1.5 + r5.x;
    r6.x = r4.w ? r5.x : r4.y;
    r4.y = r2.w * 1.5 + r5.y;
    r6.y = r4.w ? r5.y : r4.y;
    if (r4.x != 0) {
      if (r4.z == 0) {
        r7.xyzw = colorMapSampler.SampleLevel(colorMapSampler_s, r5.zw, 0).wxyz;
      } else {
        r7.x = r3.y;
      }
      if (r4.w == 0) {
        r3.xyzw = colorMapSampler.SampleLevel(colorMapSampler_s, r6.xy, 0).wxyz;
      }
      r4.x = -r1.w * 0.5 + r7.x;
      r3.y = r4.z ? r7.x : r4.x;
      r4.x = -r1.w * 0.5 + r3.x;
      r3.x = r4.w ? r3.x : r4.x;
      r4.xy = cmp(abs(r3.yx) >= r2.xx);
      r4.z = -r1.z * 2 + r5.z;
      r5.z = r4.x ? r5.z : r4.z;
      r4.z = -r2.w * 2 + r5.w;
      r5.w = r4.x ? r5.w : r4.z;
      r4.zw = ~(int2)r4.xy;
      r4.z = (int)r4.w | (int)r4.z;
      r4.w = r1.z * 2 + r6.x;
      r6.x = r4.y ? r6.x : r4.w;
      r4.w = r2.w * 2 + r6.y;
      r6.y = r4.y ? r6.y : r4.w;
      if (r4.z != 0) {
        if (r4.x == 0) {
          r7.xyzw = colorMapSampler.SampleLevel(colorMapSampler_s, r5.zw, 0).wxyz;
        } else {
          r7.x = r3.y;
        }
        if (r4.y == 0) {
          r3.xyzw = colorMapSampler.SampleLevel(colorMapSampler_s, r6.xy, 0).wxyz;
        }
        r4.z = -r1.w * 0.5 + r7.x;
        r3.y = r4.x ? r7.x : r4.z;
        r4.x = -r1.w * 0.5 + r3.x;
        r3.x = r4.y ? r3.x : r4.x;
        r4.xy = cmp(abs(r3.yx) >= r2.xx);
        r4.z = -r1.z * 4 + r5.z;
        r5.z = r4.x ? r5.z : r4.z;
        r4.z = -r2.w * 4 + r5.w;
        r5.w = r4.x ? r5.w : r4.z;
        r4.zw = ~(int2)r4.xy;
        r4.z = (int)r4.w | (int)r4.z;
        r4.w = r1.z * 4 + r6.x;
        r6.x = r4.y ? r6.x : r4.w;
        r4.w = r2.w * 4 + r6.y;
        r6.y = r4.y ? r6.y : r4.w;
        if (r4.z != 0) {
          if (r4.x == 0) {
            r7.xyzw = colorMapSampler.SampleLevel(colorMapSampler_s, r5.zw, 0).wxyz;
          } else {
            r7.x = r3.y;
          }
          if (r4.y == 0) {
            r3.xyzw = colorMapSampler.SampleLevel(colorMapSampler_s, r6.xy, 0).wxyz;
          }
          r3.z = -r1.w * 0.5 + r7.x;
          r3.y = r4.x ? r7.x : r3.z;
          r1.w = -r1.w * 0.5 + r3.x;
          r3.x = r4.y ? r3.x : r1.w;
          r3.zw = cmp(abs(r3.yx) >= r2.xx);
          r1.w = -r1.z * 12 + r5.z;
          r5.z = r3.z ? r5.z : r1.w;
          r1.w = -r2.w * 12 + r5.w;
          r5.w = r3.z ? r5.w : r1.w;
          r1.z = r1.z * 12 + r6.x;
          r6.x = r3.w ? r6.x : r1.z;
          r1.z = r2.w * 12 + r6.y;
          r6.y = r3.w ? r6.y : r1.z;
        }
      }
    }
    r1.z = v2.x + -r5.z;
    r1.w = -v2.x + r6.x;
    r2.x = v2.y + -r5.w;
    r1.z = r1.y ? r1.z : r2.x;
    r2.x = -v2.y + r6.y;
    r1.w = r1.y ? r1.w : r2.x;
    r2.xw = cmp(r3.yx < float2(0, 0));
    r3.x = r1.w + r1.z;
    r2.xz = cmp((int2)r2.xw != (int2)r2.zz);
    r2.w = 1 / r3.x;
    r3.x = cmp(r1.z < r1.w);
    r1.z = min(r1.z, r1.w);
    r1.w = r3.x ? r2.x : r2.z;
    r1.x = r1.x * r1.x;
    r1.z = r1.z * -r2.w + 0.5;
    r1.x = 0.75 * r1.x;
    r1.z = (int)r1.z & (int)r1.w;
    r1.x = max(r1.z, r1.x);
    r1.xz = r1.xx * r2.yy + v2.xy;
    r2.x = r1.y ? v2.x : r1.x;
    r2.y = r1.y ? r1.z : v2.y;
    r1.xyzw = colorMapSampler.SampleLevel(colorMapSampler_s, r2.xy, 0).xyzw;
    r0.xyz = r1.xyz;
  }
  o0.xyzw = r0.xyzw;
  return;
}
