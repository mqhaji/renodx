#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Wed Sep 18 18:31:15 2024

cbuffer _Globals : register(b0)
{
  float2 invPixelSize : packoffset(c0);
  float preBlendAmount : packoffset(c0.z);
  float postAddAmount : packoffset(c0.w);
  float4 parametricTonemapParams : packoffset(c1);
  float4 parametricTonemapToeCoeffs : packoffset(c2);
  float4 parametricTonemapShoulderCoeffs : packoffset(c3);
  float3 filmGrainColorScale : packoffset(c4);
  float4 filmGrainTextureScaleAndOffset : packoffset(c5);
  float4 color : packoffset(c6);
  float4 colorMatrix0 : packoffset(c7);
  float4 colorMatrix1 : packoffset(c8);
  float4 colorMatrix2 : packoffset(c9);
  float4 ironsightsDofParams : packoffset(c10);
  float4 filmicLensDistortParams : packoffset(c11);
  float4 colorScale : packoffset(c12);
  float4 runnersVisionColor : packoffset(c13);
  float3 depthScaleFactors : packoffset(c14);
  float4 dofParams : packoffset(c15);
  float4 dofParams2 : packoffset(c16);
  float4 dofDebugParams : packoffset(c17);
  float3 bloomScale : packoffset(c18);
  float3 lensDirtExponent : packoffset(c19);
  float3 lensDirtFactor : packoffset(c20);
  float3 lensDirtBias : packoffset(c21);
  float4 tonemapCoeffA : packoffset(c22);
  float4 tonemapCoeffB : packoffset(c23);
  float3 luminanceVector : packoffset(c24);
  float3 vignetteParams : packoffset(c25);
  float4 vignetteColor : packoffset(c26);
  float4 chromostereopsisParams : packoffset(c27);
  float4 distortionScaleOffset : packoffset(c28);
  float3 maxClampColor : packoffset(c29);
  float fftBloomSpikeDampingScale : packoffset(c29.w);
  float4 fftKernelSampleScales : packoffset(c30);
}

SamplerState mainTextureSampler_s : register(s0);
SamplerState colorGradingTextureSampler_s : register(s1);
SamplerState distortionTextureSampler_s : register(s2);
SamplerState tonemapBloomTextureSampler_s : register(s3);
SamplerState runnersVisionAlphaMaskTextureSampler_s : register(s4);
SamplerState lensDirtTextureSampler_s : register(s5);
Texture2D<float4> mainTexture : register(t0);
Texture3D<float4> colorGradingTexture : register(t1);
Texture2D<float4> distortionTexture : register(t2);
Texture2D<float4> tonemapBloomTexture : register(t3);
Texture2D<float4> runnersVisionAlphaMaskTexture : register(t4);
Texture2D<float4> lensDirtTexture : register(t5);


// 3Dmigoto declarations
#define cmp -

void main(
  float4 v0 : SV_Position0,
  float4 v1 : TEXCOORD0,
  float2 v2 : TEXCOORD1,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2,r3;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyz = lensDirtTexture.Sample(lensDirtTextureSampler_s, v2.xy).xyz;
  r0.xyz = log2(r0.xyz);
  r0.xyz = lensDirtExponent.xyz * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  r0.xyz = r0.xyz * lensDirtFactor.xyz + lensDirtBias.xyz;
  r1.xyz = distortionTexture.Sample(distortionTextureSampler_s, v2.xy).xyz;
  r1.xy = r1.xy * distortionScaleOffset.xy + distortionScaleOffset.zw;
  r1.xy = v2.xy + r1.xy;
  r2.xyz = mainTexture.Sample(mainTextureSampler_s, r1.xy).xyz;
  r1.xyw = tonemapBloomTexture.Sample(tonemapBloomTextureSampler_s, r1.xy).xyz;
  r3.xyz = r1.xyw + -r2.xyz;
  r2.xyz = r1.zzz * r3.xyz + r2.xyz;

  r1.xyz = bloomScale.xyz * r1.xyw * injectedData.fxBloom;  // scale bloom
  
  r0.xyz = r1.xyz * r0.xyz + r2.xyz;
  r0.xyz = colorScale.xyz * r0.xyz;
  r1.xy = float2(-0.5,-0.5) + v2.xy;
  
  r1.xy = vignetteParams.xy * r1.xy * injectedData.fxVignette;  // vignette
  
  r0.w = dot(r1.xy, r1.xy);
  r0.w = saturate(-r0.w * vignetteColor.w + 1);
  r0.w = log2(r0.w);
  r0.w = vignetteParams.z * r0.w;
  r0.w = exp2(r0.w);
  r0.xyz = r0.xyz * r0.www;

  float3 untonemapped = r0.xyz;
  
  // tonemap run 1 (midgray)
  r0.xyz = (0.18, 0.18, 0.18);  // find midgray
  r1.xyz = tonemapCoeffA.xzx / tonemapCoeffA.ywy;
  r1.xyz = r1.xyz * float3(-0.199999988,0.229999989,0.180000007) + float3(0.569999993,0.00999999978,0.0199999996);
  r0.w = r1.y * r1.x;
  r1.y = tonemapCoeffB.z * 0.200000003 + r0.w;
  r1.zw = float2(0.0199999996,0.300000012) * r1.zz;
  r1.y = tonemapCoeffB.z * r1.y + r1.z;
  r2.x = tonemapCoeffB.z * 0.200000003 + r1.x;
  r2.x = tonemapCoeffB.z * r2.x + r1.w;
  r1.y = r1.y / r2.x;
  r1.y = -0.0666666627 + r1.y;
  r1.y = 1 / r1.y;
  r0.xyz = r1.yyy * r0.xyz;
  r2.xyz = r0.xyz * float3(0.200000003,0.200000003,0.200000003) + r0.www;
  r2.xyz = r0.xyz * r2.xyz + r1.zzz;
  r3.xyz = r0.xyz * float3(0.200000003,0.200000003,0.200000003) + r1.xxx;
  r0.xyz = r0.xyz * r3.xyz + r1.www;
  r0.xyz = r2.xyz / r0.xyz;
  r0.xyz = float3(-0.0666666627,-0.0666666627,-0.0666666627) + r0.xyz;
  r0.xyz = r0.xyz * r1.yyy;
  r0.xyz = r0.xyz / tonemapCoeffB.www;
  float vanillaMidGray = renodx::color::y::from::BT709(r0.xyz); // midgray

  // tonemap run 2 (vanillaColor)
  r0.xyz = untonemapped;
  r1.xyz = tonemapCoeffA.xzx / tonemapCoeffA.ywy;
  r1.xyz = r1.xyz * float3(-0.199999988,0.229999989,0.180000007) + float3(0.569999993,0.00999999978,0.0199999996);
  r0.w = r1.y * r1.x;
  r1.y = tonemapCoeffB.z * 0.200000003 + r0.w;
  r1.zw = float2(0.0199999996,0.300000012) * r1.zz;
  r1.y = tonemapCoeffB.z * r1.y + r1.z;
  r2.x = tonemapCoeffB.z * 0.200000003 + r1.x;
  r2.x = tonemapCoeffB.z * r2.x + r1.w;
  r1.y = r1.y / r2.x;
  r1.y = -0.0666666627 + r1.y;
  r1.y = 1 / r1.y;
  r0.xyz = r1.yyy * r0.xyz;
  r2.xyz = r0.xyz * float3(0.200000003,0.200000003,0.200000003) + r0.www;
  r2.xyz = r0.xyz * r2.xyz + r1.zzz;
  r3.xyz = r0.xyz * float3(0.200000003,0.200000003,0.200000003) + r1.xxx;
  r0.xyz = r0.xyz * r3.xyz + r1.www;
  r0.xyz = r2.xyz / r0.xyz;
  r0.xyz = float3(-0.0666666627,-0.0666666627,-0.0666666627) + r0.xyz;
  r0.xyz = r0.xyz * r1.yyy;
  r0.xyz = r0.xyz / tonemapCoeffB.www;
  float3 vanillaTM = r0.xyz;  // vanilla tonemap

  r0.xyz = renodx::color::srgb::from::BT709(r0.xyz);
  float3 lutInputColor = r0.xyz;
  r0.xyz = renodx::lut::Sample(colorGradingTexture, colorGradingTextureSampler_s, r0.xyz, 32);
  r0.xyz = lerp(lutInputColor, r0.xyz, injectedData.colorGradeLUTStrength);
  float3 vanillaColor = r0.xyz;

  if (injectedData.toneMapType) {
    float renoDRTContrast = 1.f;
    float renoDRTFlare = 0.02f;
    float renoDRTShadows = 1.f;
    float renoDRTDechroma = 0.f;
    float renoDRTSaturation = 1.05f;
    float renoDRTHighlights = 1.14f;
    float3 hdrColor;
    hdrColor = renodx::tonemap::config::Apply(
        untonemapped,
        renodx::tonemap::config::Create(
            injectedData.toneMapType, // tm type
            pow(injectedData.toneMapPeakNits/80.f, 1.f / 2.2f) * 80.f,  // correct for sRGB LUT
            pow(injectedData.toneMapGameNits/80.f, 1.f / 2.2f) * 80.f,
            0.f,
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
            renoDRTFlare,
            injectedData.toneMapHueCorrection,
            vanillaTM),
        renodx::lut::config::Create(
            colorGradingTextureSampler_s,
            injectedData.colorGradeLUTStrength,
            0.f,  // Lut scaling not needed
            renodx::lut::config::type::SRGB,
            renodx::lut::config::type::LINEAR,
            32.f),
        colorGradingTexture);

    r0.xyz = hdrColor;
    if (injectedData.toneMapBlend) {
      hdrColor = renodx::math::SafePow(hdrColor, 2.2f);
      vanillaColor = renodx::math::SafePow(vanillaColor, 2.2f);
      vanillaColor = renodx::color::grade::UserColorGrading(
          vanillaColor,
          injectedData.colorGradeExposure,              // exposure
          1.f,                                          // highlights, controlled by hdrColor
          injectedData.colorGradeShadows,               // shadows
          injectedData.colorGradeContrast,              // contrast
          injectedData.colorGradeSaturation,            // saturation
          injectedData.colorGradeBlowout);              // dechroma

      // blend HDR with SDR
      float3 negHDR = min(0, hdrColor); // save WCG
      float3 blendedColor = lerp(saturate(vanillaColor), max(0, hdrColor), saturate(vanillaColor));
      blendedColor += negHDR; // add back WCG

      r0.xyz = renodx::math::SafePow(blendedColor, 1.f/2.2f);
    }
  }
  r1.xyz = float3(1,1,1) + -r0.xyz;
  r1.xyz = r1.xyz * float3(0.400000006,0.400000006,0.400000006) + r0.xyz;
  r2.xyz = runnersVisionColor.xyz + -r1.xyz;
  r1.xyz = preBlendAmount * r2.xyz + r1.xyz;
  r1.xyz = float3(1,1,1) + -r1.xyz;
  r1.xyz = r1.xyz / runnersVisionColor.xyz;
  r1.xyz = float3(1,1,1) + -r1.xyz;
  r1.xyz = runnersVisionColor.xyz * postAddAmount + r1.xyz;
  r1.yzw = runnersVisionColor.xyz * float3(0.400000006,0.400000006,0.400000006) + r1.xyz;
  r0.w = runnersVisionAlphaMaskTexture.Sample(runnersVisionAlphaMaskTextureSampler_s, v2.xy).x;
  r1.x = max(r0.w, r1.y);
  r1.xyz = r1.xzw + -r0.xyz;
  o0.xyz = r0.www * r1.xyz + r0.xyz;
  o0.w = dot(r0.xyz, float3(0.298999995,0.587000012,0.114));

  o0.xyz = renodx::math::SafePow(o0.xyz, 2.2f);
  o0.xyz *= injectedData.toneMapGameNits / injectedData.toneMapUINits;
  o0.xyz = renodx::math::SafePow(o0.xyz, 1.f / 2.2f);
  return;
}