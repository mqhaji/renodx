#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Mon Jun 03 19:56:59 2024

cbuffer PerViewCB : register(b1)
{
  float4 cb_alwaystweak : packoffset(c0);
  float4 cb_viewrandom : packoffset(c1);
  float4x4 cb_viewprojectionmatrix : packoffset(c2);
  float4x4 cb_viewmatrix : packoffset(c6);
  float4 cb_subpixeloffset : packoffset(c10);
  float4x4 cb_projectionmatrix : packoffset(c11);
  float4x4 cb_previousviewprojectionmatrix : packoffset(c15);
  float4x4 cb_previousviewmatrix : packoffset(c19);
  float4x4 cb_previousprojectionmatrix : packoffset(c23);
  float4 cb_mousecursorposition : packoffset(c27);
  float4 cb_mousebuttonsdown : packoffset(c28);
  float4 cb_jittervectors : packoffset(c29);
  float4x4 cb_inverseviewprojectionmatrix : packoffset(c30);
  float4x4 cb_inverseviewmatrix : packoffset(c34);
  float4x4 cb_inverseprojectionmatrix : packoffset(c38);
  float4 cb_globalviewinfos : packoffset(c42);
  float3 cb_wscamforwarddir : packoffset(c43);
  uint cb_alwaysone : packoffset(c43.w);
  float3 cb_wscamupdir : packoffset(c44);
  uint cb_usecompressedhdrbuffers : packoffset(c44.w);
  float3 cb_wscampos : packoffset(c45);
  float cb_time : packoffset(c45.w);
  float3 cb_wscamleftdir : packoffset(c46);
  float cb_systime : packoffset(c46.w);
  float2 cb_jitterrelativetopreviousframe : packoffset(c47);
  float2 cb_worldtime : packoffset(c47.z);
  float2 cb_shadowmapatlasslicedimensions : packoffset(c48);
  float2 cb_resolutionscale : packoffset(c48.z);
  float2 cb_parallelshadowmapslicedimensions : packoffset(c49);
  float cb_framenumber : packoffset(c49.z);
  uint cb_alwayszero : packoffset(c49.w);
}

SamplerState smp_bilinearsampler_s : register(s0);
Texture2D<float4> ro_identity_bufferin : register(t0);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : INTERP0,
  out float4 o0 : SV_TARGET0)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = cb_resolutionscale.xy * v0.xy;
  ro_identity_bufferin.GetDimensions(0, fDest.x, fDest.y, fDest.z);
  r0.zw = fDest.xy;
  r1.xy = r0.xy * r0.zw + float2(-0.5,-0.5);
  r1.xy = floor(r1.xy);
  r2.xyzw = float4(0.5,0.5,-0.5,-0.5) + r1.xyxy;
  r1.xy = float2(2.5,2.5) + r1.xy;
  r0.xy = r0.xy * r0.zw + -r2.xy;
  r0.zw = float2(1,1) / r0.zw;
  r1.zw = r0.xy * r0.xy;
  r3.xy = r1.zw * r0.xy;
  r3.zw = float2(2.5,2.5) * r1.zw;
  r3.xy = r3.xy * float2(1.5,1.5) + -r3.zw;
  r3.xy = float2(1,1) + r3.xy;
  r3.zw = r1.zw * r0.xy + r0.xy;
  r0.xy = r1.zw * r0.xy + -r1.zw;
  r1.zw = -r3.zw * float2(0.5,0.5) + r1.zw;
  r3.zw = float2(1,1) + -r1.zw;
  r3.zw = r3.zw + -r3.xy;
  r3.zw = -r0.xy * float2(0.5,0.5) + r3.zw;
  r0.xy = float2(0.5,0.5) * r0.xy;
  r3.xy = r3.xy + r3.zw;
  r3.zw = r3.zw / r3.xy;
  r2.xy = r3.zw + r2.xy;
  r4.xyzw = r2.zyxw * r0.zwzw;
  r2.xy = r1.xy * r0.zw;
  r5.xyzw = ro_identity_bufferin.SampleLevel(smp_bilinearsampler_s, r4.zw, 0).xyzw;
  r5.xyzw = r5.xyzw * r3.xxxx;
  r5.xyzw = r5.xyzw * r1.wwww;
  r6.xyzw = ro_identity_bufferin.SampleLevel(smp_bilinearsampler_s, r4.xw, 0).xyzw;
  r6.xyzw = r6.xyzw * r1.zzzz;
  r5.xyzw = r6.xyzw * r1.wwww + r5.xyzw;
  r2.zw = r4.wy;
  r6.xyzw = ro_identity_bufferin.SampleLevel(smp_bilinearsampler_s, r2.xz, 0).xyzw;
  r7.xyzw = ro_identity_bufferin.SampleLevel(smp_bilinearsampler_s, r2.xw, 0).xyzw;
  r7.xyzw = r7.xyzw * r0.xxxx;
  r6.xyzw = r6.xyzw * r0.xxxx;
  r5.xyzw = r6.xyzw * r1.wwww + r5.xyzw;
  r6.xyzw = ro_identity_bufferin.SampleLevel(smp_bilinearsampler_s, r4.xy, 0).xyzw;
  r8.xyzw = ro_identity_bufferin.SampleLevel(smp_bilinearsampler_s, r4.zy, 0).xyzw;
  r8.xyzw = r8.xyzw * r3.xxxx;
  r6.xyzw = r6.xyzw * r1.zzzz;
  r5.xyzw = r6.xyzw * r3.yyyy + r5.xyzw;
  r5.xyzw = r8.xyzw * r3.yyyy + r5.xyzw;
  r5.xyzw = r7.xyzw * r3.yyyy + r5.xyzw;
  r4.y = r2.y;
  r2.xyzw = ro_identity_bufferin.SampleLevel(smp_bilinearsampler_s, r2.xy, 0).xyzw;
  r2.xyzw = r2.xyzw * r0.xxxx;
  r6.xyzw = ro_identity_bufferin.SampleLevel(smp_bilinearsampler_s, r4.xy, 0).xyzw;
  r4.xyzw = ro_identity_bufferin.SampleLevel(smp_bilinearsampler_s, r4.zy, 0).xyzw;
  r3.xyzw = r4.xyzw * r3.xxxx;
  r1.xyzw = r6.xyzw * r1.zzzz;
  r1.xyzw = r1.xyzw * r0.yyyy + r5.xyzw;
  r1.xyzw = r3.xyzw * r0.yyyy + r1.xyzw;
  o0.xyzw = r2.xyzw * r0.yyyy + r1.xyzw;

  float3 signs = sign(o0.rgb);
  o0.rgb = abs(o0.rgb);
  o0.rgb = renodx::color::srgb::from::BT709(o0.rgb);
  o0.rgb *= signs;

  o0.rgb = renodx::math::SafePow(o0.rgb, 2.2f);
  o0.rgb *= injectedData.toneMapGameNits / injectedData.toneMapUINits;
  o0.rgb = renodx::math::SafePow(o0.rgb, 1.f / 2.2f);


  // if (injectedData.toneMapGammaCorrection) {
  //   float3 signs = sign(o0.rgb);
  //   o0.rgb = abs(o0.rgb);
  //   o0.rgb = renodx::color::srgb::from::BT709(o0.rgb);
  //   o0.rgb = pow(o0.rgb, 2.2f);
  //   o0.rgb *= signs;
  // }
  // o0.rgb *= injectedData.toneMapGameNits / 80.f;

  return;
}