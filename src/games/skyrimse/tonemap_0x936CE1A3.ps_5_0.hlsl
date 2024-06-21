#include "../../shaders/tonemap.hlsl"
#include "../../shaders/color.hlsl"
#include "../../shaders/filmgrain.hlsl"
#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Sat May 18 02:41:57 2024
Texture2D<float4> t2 : register(t2);  // adaptation

Texture2D<float4> t1 : register(t1);  // color

Texture2D<float4> t0 : register(t0);  // bloom

SamplerState s2_s : register(s2); // adaptation

SamplerState s1_s : register(s1); // color

SamplerState s0_s : register(s0); // bloom

cbuffer cb2 : register(b2)
{
  float4 cb2[5];
}

cbuffer cb12 : register(b12)
{
  float4 cb12[45];
}




// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_POSITION0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2,r3;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = cb12[43].xy * v1.xy;
  r0.xy = max(float2(0,0), r0.xy);
  r1.x = min(cb12[44].z, r0.x);
  r1.y = min(cb12[43].y, r0.y);
  r0.xyz = t1.Sample(s1_s, r1.xy).xyz;
  r0.w = cmp(0.5 < cb2[0].x);
  if (r0.w != 0) {
      r1.xyz = t0.Sample(s0_s, r1.xy).xyz;
  } else {
      r1.xyz = t0.Sample(s0_s, v1.xy).xyz;
  }


  r2.xy = t2.Sample(s2_s, v1.xy).xy;  // midgray

  // Eye adaptation
  float lum = dot(float3(0.212500006,0.715399981,0.0720999986), r0.xyz);
  lum = max(9.99999975e-006, lum);
  float lumAdjusted = lum * r2.y / r2.x;
  
  //r0.xyz = r0.xyz * lumAdjusted / lum;  // original autoexposure code
  r0.xyz = lerp(r0.xyz, r0.xyz * lumAdjusted / lum, injectedData.fxAutoExposure);  // adjustable auto exposure

  const float3 untonemapped = r0.xyz;

  float lumMapped = lumAdjusted * (lumAdjusted * cb2[2].y + 1.0) / (lumAdjusted + 1.0);

  // Tone mapping
  if (injectedData.toneMapType == 0) { // Vanilla Extended Reinhard tone mapping: x/(x+1)
    r0.xyz = r0.xyz * lumMapped / lumAdjusted;
    o0.xyz = r0.xyz;
  }
  else {
    
    r0.xyz = untonemapped;

    float vanillaMidGray =  injectedData.midGray; // .175?

    float renoDRTContrast = 1.f;
    float renoDRTFlare = injectedData.renoDRTFlare;
    float renoDRTShadows = 1.f;
    float renoDRTDechroma = injectedData.colorGradeDechroma;
    float renoDRTSaturation = 1.f;
    float renoDRTHighlights = 1.f;

    ToneMapParams tmParams = buildToneMapParams(
      injectedData.toneMapType,
      injectedData.toneMapPeakNits,
      injectedData.toneMapGameNits,
      injectedData.toneMapGammaCorrection,
      injectedData.colorGradeExposure,
      injectedData.colorGradeHighlights,
      injectedData.colorGradeShadows,
      injectedData.colorGradeContrast,
      injectedData.colorGradeSaturation,
      vanillaMidGray,
      vanillaMidGray * 100.f,
      renoDRTHighlights,
      renoDRTShadows,
      renoDRTContrast,
      renoDRTSaturation,
      renoDRTDechroma,
      renoDRTFlare
    );

    r0.xyz = toneMap(untonemapped, tmParams);

  }


  // bloom application
  r0.xyz = (r1.xyz * saturate(cb2[2].x - lumMapped)) * injectedData.fxBloom + r0.xyz;

  // Color grading
  r1.x = dot(r0.xyz, float3(0.212500006,0.715399981,0.0720999986));
  r0.w = 1;
  float3 outputColor = r0.xyz; // before scene filter
  r0.xyzw = -r1.xxxx + r0.xyzw;
  r0.xyzw = cb2[3].xxxx * r0.xyzw + r1.xxxx;
  r1.xyzw = r1.xxxx * cb2[4].xyzw + -r0.xyzw;
  r0.xyzw = cb2[4].wwww * r1.xyzw + r0.xyzw;
  r0.xyzw = cb2[3].wwww * r0.xyzw + -r2.xxxx;
  r0.xyzw = cb2[3].zzzz * r0.xyzw + r2.xxxx;
  o0.xyz = lerp(outputColor, o0.xyz, injectedData.fxSceneFilter);

  // gamma 2.2 conversion
  // r0.xyz = max(0, r0.xyz);
  // r0.xyz = log2(r0.xyz);
  // r0.xyz = cb12[42].xxx * r0.xyz;
  // o0.xyz = exp2(r0.xyz);
  o0.xyz = sign(r0.xyz) * pow(abs(r0.xyz), cb12[42].xxx);
  
  if (injectedData.fxFilmGrain) {
    float3 grainedColor = computeFilmGrain(
      o0.rgb,
      v1.xy,
      frac(injectedData.elapsedTime / 1000.f),
      injectedData.fxFilmGrain * 0.03f,
      1.f
    );
    o0.xyz = grainedColor;
  }


  o0.rgb = injectedData.toneMapGammaCorrection ? sign(o0.rgb) * pow(abs(o0.rgb), 2.2f) : linearFromSRGB(o0.rgb);
  o0.rgb *= injectedData.toneMapGameNits/80.f;

  o0.rgb = mul(BT709_2_BT2020_MAT, o0.rgb); // Convert to BT2020
  o0.rgb = max(0, o0.rgb);                  // Clamp to BT2020
  o0.rgb = mul(BT2020_2_BT709_MAT, o0.rgb); // Convert BT709

  o0.w = r0.w;
  // o0.xyz = cb12[42].xxx/80.f; // test

  return;
}