#include "./shared.h"
#include "./hueHelper.hlsl"
#include "./DICE.hlsl"
#include "./frostbite.hlsl"


// ---- Created with 3Dmigoto v1.3.16 on Mon Sep 09 09:32:46 2024

cbuffer rage_globals : register(b5)
{
  float4 globalScalars : packoffset(c0);
  float4 globalScalars2 : packoffset(c1);
  float4 globalScalars3 : packoffset(c2);
  float4 globalScalars4 : packoffset(c3);
  float4 globalFogParams : packoffset(c4);
  float4 globalFogColor : packoffset(c5);
  float4 globalFogOffsetN : packoffset(c6);
  float4 globalFogColorN : packoffset(c7);
  float4 globalScreenSize : packoffset(c8);
  float4 globalDayNightEffects : packoffset(c9);
  float4 ColorCorrectTopAndPedReflectScale : packoffset(c10);
  float4 ColorCorrectBottomOffset : packoffset(c11);
  float4 ColorShift : packoffset(c12);
  float4 deSatContrastGamma : packoffset(c13);
  float4 deSatContrastGammaIFX : packoffset(c14);
  float4 colorize : packoffset(c15);
  float4 globalUmScalars : packoffset(c16);
  float4 gToneMapScalers : packoffset(c17);
  float4 gAmbientOcclusionEffect : packoffset(c18);
  float4 gWaterGlobals : packoffset(c19);
  float4 ColorCorrectTopScreenEdge : packoffset(c20);
  float4 ColorCorrectBottomOffsetScreenEdge : packoffset(c21);
  float4 gScreenSpaceTessFactors : packoffset(c22);
  float4 gTessFactors : packoffset(c23);
  float4 gFrustumPlaneEquation_Left : packoffset(c24);
  float4 gFrustumPlaneEquation_Right : packoffset(c25);
  float4 gFrustumPlaneEquation_Top : packoffset(c26);
  float4 gFrustumPlaneEquation_Bottom : packoffset(c27);
  float4 gTessParameters : packoffset(c28);
}

SamplerState BackBufferSampler_s : register(s0);
Texture2D<float4> BackBuffer : register(t0);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float2 v1 : TEXCOORD0,
  float2 w1 : TEXCOORD1,
  float2 v2 : TEXCOORD2,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2,r3,r4,r5,r6;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(0.5,0.5) * globalScreenSize.zw;
  r1.xy = -globalScreenSize.zw * float2(0.5,0.5) + v1.xy;
  r1.zw = globalScreenSize.zw * float2(0.5,0.5) + v1.xy;
  r0.z = BackBuffer.SampleLevel(BackBufferSampler_s, r1.xy, 0).w;
  r0.w = BackBuffer.SampleLevel(BackBufferSampler_s, r1.xw, 0).w;
  r1.x = BackBuffer.SampleLevel(BackBufferSampler_s, r1.zy, 0).w;
  r1.y = BackBuffer.SampleLevel(BackBufferSampler_s, r1.zw, 0).w;
  r2.xyzw = BackBuffer.SampleLevel(BackBufferSampler_s, v1.xy, 0).xyzw;
  r1.z = max(r0.z, r0.w);
  r1.x = 0.00260416674 + r1.x;
  r1.w = min(r0.z, r0.w);
  r3.x = max(r1.x, r1.y);
  r3.y = min(r1.x, r1.y);
  r1.z = max(r3.x, r1.z);
  r1.w = min(r3.y, r1.w);
  r3.x = 0.125 * r1.z;
  r3.y = min(r1.w, r2.w);
  r3.x = max(0.0500000007, r3.x);
  r3.z = max(r1.z, r2.w);
  r3.y = r3.z + -r3.y;
  r3.x = cmp(r3.y >= r3.x);
  if (r3.x != 0) {
    r3.xy = globalScreenSize.zw + globalScreenSize.zw;
    r0.w = -r1.x + r0.w;
    r0.z = r1.y + -r0.z;
    r1.x = r0.w + r0.z;
    r1.y = r0.w + -r0.z;
    r0.z = dot(r1.xy, r1.xy);
    r0.z = rsqrt(r0.z);
    r0.zw = r1.xy * r0.zz;
    r1.xy = -r0.zw * r0.xy + v1.xy;
    r4.xyzw = BackBuffer.SampleLevel(BackBufferSampler_s, r1.xy, 0).xyzw;
    r0.xy = r0.zw * r0.xy + v1.xy;
    r5.xyzw = BackBuffer.SampleLevel(BackBufferSampler_s, r0.xy, 0).xyzw;
    r0.x = min(abs(r0.z), abs(r0.w));
    r0.x = 8 * r0.x;
    r0.xy = r0.zw / r0.xx;
    r0.xy = max(float2(-2,-2), r0.xy);
    r0.xy = min(float2(2,2), r0.xy);
    r0.zw = -r0.xy * r3.xy + v1.xy;
    r6.xyzw = BackBuffer.SampleLevel(BackBufferSampler_s, r0.zw, 0).xyzw;
    r0.xy = r0.xy * r3.xy + v1.xy;
    r0.xyzw = BackBuffer.SampleLevel(BackBufferSampler_s, r0.xy, 0).xyzw;
    r3.xyzw = r5.xyzw + r4.xyzw;
    r0.xyzw = r6.xyzw + r0.xyzw;
    r4.xyzw = float4(0.25,0.25,0.25,0.25) * r3.xyzw;
    r0.xyzw = r0.xyzw * float4(0.25,0.25,0.25,0.25) + r4.xyzw;
    r1.x = cmp(r0.w < r1.w);
    r1.y = cmp(r1.z < r0.w);
    r1.x = (int)r1.y | (int)r1.x;
    r1.yzw = float3(0.5,0.5,0.5) * r3.xyz;
    o0.xyz = r1.xxx ? r1.yzw : r0.xyz;
    o0.w = r0.w;
  } else {
    o0.xyzw = r2.xyzw;
  }
  o0.xyzw = BackBuffer.SampleLevel(BackBufferSampler_s, v1.xy, 0).xyzw;



  float3 untonemapped = renodx::math::SafePow(o0.xyz, 2.2f);
  if (injectedData.toneMapType == 3) {
      untonemapped = Hue(untonemapped, injectedData.toneMapHueCorrection);
      o0.rgb = maxpayne3::tonemap::frostbite::BT709(untonemapped, injectedData.toneMapPeakNits / injectedData.toneMapGameNits);
      o0.rgb = renodx::math::SafePow(o0.rgb, 1.f / 2.2f);
  } else if (injectedData.toneMapType == 2) {
      untonemapped = Hue(untonemapped, injectedData.toneMapHueCorrection);

      const float paperWhite = injectedData.toneMapGameNits / renodx::color::srgb::REFERENCE_WHITE;
      const float peakWhite = injectedData.toneMapPeakNits / renodx::color::srgb::REFERENCE_WHITE;
      DICESettings config = DefaultDICESettings();
      config.Type = 3;
      config.ShoulderStart = 0.5;  // Vanilla just clips, don't tonemap below 1
      o0.rgb = DICETonemap(untonemapped * paperWhite, peakWhite, config) / paperWhite;
      o0.rgb = renodx::math::SafePow(o0.rgb, 1.f / 2.2f);
  } else if (injectedData.toneMapType == 0) {
      o0.rgb = saturate(o0.rgb);
  }

  return;
}