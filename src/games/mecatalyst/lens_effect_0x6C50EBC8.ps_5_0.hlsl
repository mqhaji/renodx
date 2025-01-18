#include "./common.hlsl"

// ---- Created with 3Dmigoto v1.3.16 on Wed Sep 18 20:03:47 2024

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
  float4 r0, r1;

  r0.x = dot(v1.xy, v1.xy);
  r0.y = sqrt(r0.x);
  r0.x = r0.x + -r0.y;
  r0.x = planes[0].distortionFactor * r0.x + r0.y;
  r0.z = cmp(0 < r0.y);
  r1.xy = v1.xy / r0.yy;
  r0.y = saturate(r0.y * planes[0].mixK + planes[0].mixM);
  r0.zw = r0.zz ? r1.xy : 0;
  r0.xz = r0.zw * r0.xx;
  r1.xy = float2(1, -1) * planes[0].halfTextureScale;
  r0.xz = r0.xz * r1.xy + float2(0.5, 0.5);
  r0.xzw = mainTexture.Sample(mainTextureSampler_s, r0.xz).xyz;
  r0.x = dot(float3(0.212599993, 0.715200007, 0.0722000003), r0.xzw);
  r0.x = max(0, r0.x);
  r0.x = planes[0].inputScale * r0.x;
  r0.x = log2(r0.x);
  r0.x = planes[0].inputExp * r0.x;
  r0.x = exp2(r0.x);
  r0.x = min(1, r0.x);
  r0.x = planes[0].maxOpacity * r0.x;
  r1.xyz = -planes[0].innerColor.xyz + planes[0].outerColor.xyz;
  r0.yzw = r0.yyy * r1.xyz + planes[0].innerColor.xyz;
  o0.xyz = r0.yzw * r0.xxx;
  o0.w = 0;

  o0.rgb = lerp(0, o0.rgb, injectedData.fxLens);
  // scale lens effects with game brightness
  // not an ideal solution, transparencies will blend slightly differently
  o0.rgb = PostToneMapScale(o0.rgb);
  return;
}
