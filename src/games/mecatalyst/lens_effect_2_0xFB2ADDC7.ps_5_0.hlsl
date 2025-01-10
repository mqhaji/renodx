#include "./common.hlsl"

// ---- Created with 3Dmigoto v1.4.1 on Wed Jan  8 23:27:23 2025

cbuffer cb0 : register(b0) {
  struct
  {
    float3 outerColor;
    float mixK;
    float3 innerColor;
    float mixM;
    float inputScale;
    float inputExp;
    float halfTextureScale;
    float distortionFactor;
    float maxOpacity;
    float dummy0;
    float dummy1;
    float dummy2;
  } planes[4] : packoffset(c0);

  float2 invScreenSize : packoffset(c16);
}

SamplerState mainTextureSampler_s : register(s0);
Texture2D<float4> mainTexture : register(t0);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position0,
    float2 v1: TEXCOORD0,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(1, -1) * planes[1].halfTextureScale;
  r0.z = dot(v1.xy, v1.xy);
  r0.w = sqrt(r0.z);
  r0.z = r0.z + -r0.w;
  r1.x = cmp(0 < r0.w);
  r1.yz = v1.xy / r0.ww;
  r1.xy = r1.xx ? r1.yz : 0;
  r1.z = planes[1].distortionFactor * r0.z + r0.w;
  r0.z = planes[0].distortionFactor * r0.z + r0.w;
  r2.xy = r1.xy * r0.zz;
  r1.xy = r1.xy * r1.zz;
  r0.xy = r1.xy * r0.xy + float2(0.5, 0.5);
  r0.xyz = mainTexture.Sample(mainTextureSampler_s, r0.xy).xyz;  // blue vignette

  r0.x = dot(float3(0.212599993, 0.715200007, 0.0722000003), r0.xyz);
  r0.x = max(0, r0.x);
  r0.x = planes[1].inputScale * r0.x;
  r0.x = log2(r0.x);
  r0.x = planes[1].inputExp * r0.x;
  r0.x = exp2(r0.x);
  r0.x = min(1, r0.x);
  r0.x = planes[1].maxOpacity * r0.x;
  r0.y = saturate(r0.w * planes[1].mixK + planes[1].mixM);
  r0.z = saturate(r0.w * planes[0].mixK + planes[0].mixM);
  r1.xyz = -planes[1].innerColor + planes[1].outerColor;
  r1.xyz = r0.yyy * r1.xyz + planes[1].innerColor;
  r0.xyw = r1.xyz * r0.xxx;
  r1.xy = float2(1, -1) * planes[0].halfTextureScale;
  r1.xy = r2.xy * r1.xy + float2(0.5, 0.5);
  r1.xyz = mainTexture.Sample(mainTextureSampler_s, r1.xy).xyz;  // red vignette

  r1.x = dot(float3(0.212599993, 0.715200007, 0.0722000003), r1.xyz);
  r1.x = max(0, r1.x);
  r1.x = planes[0].inputScale * r1.x;
  r1.x = log2(r1.x);
  r1.x = planes[0].inputExp * r1.x;
  r1.x = exp2(r1.x);
  r1.x = min(1, r1.x);
  r1.x = planes[0].maxOpacity * r1.x;
  r1.yzw = -planes[0].innerColor.xyz + planes[0].outerColor.xyz;
  r1.yzw = r0.zzz * r1.yzw + planes[0].innerColor.xyz;
  o0.xyz = r1.yzw * r1.xxx + r0.xyw;

  // scale lens effects with game brightness
  // not an ideal solution, transparencies will blend slightly differently
  o0.rgb = PostToneMapScale(o0.rgb);

  o0.w = 0;
  return;
}
