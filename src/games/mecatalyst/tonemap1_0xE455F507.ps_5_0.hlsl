#include "./common.hlsl"

// ---- Created with 3Dmigoto v1.4.1 on Wed Jan  8 23:23:15 2025

cbuffer _Globals : register(b0) {
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

float3 applyVanillaTonemap(float3 untonemapped) {
  float4 r0, r1;
  float3 r2, r3;
  r0.rgb = untonemapped;

  r1.xyz = tonemapCoeffA.xzx / tonemapCoeffA.ywy;
  r1.xyz = r1.xyz * float3(-0.199999988, 0.229999989, 0.180000007) + float3(0.569999993, 0.00999999978, 0.0199999996);
  r0.w = r1.y * r1.x;
  r1.y = tonemapCoeffB.z * 0.200000003 + r0.w;
  r1.zw = float2(0.0199999996, 0.300000012) * r1.zz;
  r1.y = tonemapCoeffB.z * r1.y + r1.z;
  r2.x = tonemapCoeffB.z * 0.200000003 + r1.x;
  r2.x = tonemapCoeffB.z * r2.x + r1.w;
  r1.y = r1.y / r2.x;
  r1.y = -0.0666666627 + r1.y;
  r1.y = 1 / r1.y;
  r0.xyz = r1.yyy * r0.xyz;
  r2.xyz = r0.xyz * float3(0.200000003, 0.200000003, 0.200000003) + r0.www;
  r2.xyz = r0.xyz * r2.xyz + r1.zzz;
  r3.xyz = r0.xyz * float3(0.200000003, 0.200000003, 0.200000003) + r1.xxx;
  r0.xyz = r0.xyz * r3.xyz + r1.www;
  r0.xyz = r2.xyz / r0.xyz;
  r0.xyz = float3(-0.0666666627, -0.0666666627, -0.0666666627) + r0.xyz;
  r0.xyz = r0.xyz * r1.yyy;
  r0.xyz = r0.xyz / tonemapCoeffB.www;

  return r0.rgb;
}

float3 sampleLUT(Texture3D<float4> colorGradingTexture, float3 lutInputColor) {
  renodx::lut::Config lut_config = renodx::lut::config::Create(
      colorGradingTextureSampler_s,
      1.f,  // strength
      0.f,  // scaling
      renodx::lut::config::type::SRGB,
      renodx::lut::config::type::SRGB,
      32u);
  return renodx::lut::Sample(colorGradingTexture, lut_config, lutInputColor);  // outputs in linear
}

float3 applyRunnersVision(float3 inputColor, float2 v2) {
  float4 r0, r1;
  float3 r2;
  r0.rgb = inputColor;

  r1.xyz = float3(1, 1, 1) + -r0.xyz;
  r1.xyz = r1.xyz * float3(0.400000006, 0.400000006, 0.400000006) + r0.xyz;
  r2.xyz = runnersVisionColor.xyz + -r1.xyz;
  r1.xyz = preBlendAmount * r2.xyz + r1.xyz;
  r1.xyz = float3(1, 1, 1) + -r1.xyz;
  r1.xyz = r1.xyz / runnersVisionColor.xyz;
  r1.xyz = float3(1, 1, 1) + -r1.xyz;
  r1.xyz = runnersVisionColor.xyz * postAddAmount + r1.xyz;
  r1.yzw = runnersVisionColor.xyz * float3(0.400000006, 0.400000006, 0.400000006) + r1.xyz;
  r0.w = runnersVisionAlphaMaskTexture.Sample(runnersVisionAlphaMaskTextureSampler_s, v2.xy).x;
  r1.x = max(r0.w, r1.y);
  r1.xyz = r1.xzw + -r0.xyz;
  return r0.www * r1.xyz + r0.xyz;
}

void main(
    float4 v0: SV_Position0,
    float4 v1: TEXCOORD0,
    float2 v2: TEXCOORD1,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2, r3;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyz = lensDirtTexture.Sample(lensDirtTextureSampler_s, v2.xy).xyz;
  r0.rgb = pow(r0.rgb, lensDirtExponent.xyz);
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
  r1.xy = float2(-0.5, -0.5) + v2.xy;
  r1.xy = vignetteParams.xy * r1.xy * injectedData.fxVignette;  // vignette;
  r0.w = dot(r1.xy, r1.xy);
  r0.w = saturate(-r0.w * vignetteColor.w + 1);
  r0.w = pow(r0.w, vignetteParams.z);
  r0.xyz = r0.xyz * r0.www;

  float3 untonemapped = r0.rgb;

  // tonemap
  renodx::tonemap::config::DualToneMap dual_tone_map;
  float3 color_sdr, color_hdr;
  if (injectedData.toneMapType == 0) {
    r0.rgb = applyVanillaTonemap(untonemapped);
    color_sdr = r0.rgb;
  } else {
    float vanillaMidGray = renodx::color::y::from::BT709(applyVanillaTonemap(float3(0.18f, 0.18f, 0.18f)));
    dual_tone_map = applyUserTonemap(untonemapped, vanillaMidGray);
    color_sdr = dual_tone_map.color_sdr;
    color_hdr = dual_tone_map.color_hdr;
    r0.rgb = color_sdr;
  }
  // end tonemap

  // color grade
  float3 lutInputColor = r0.rgb;
  r0.rgb = renodx::color::srgb::Encode(sampleLUT(colorGradingTexture, lutInputColor));
  float3 lutOutputColor = saturate(r0.rgb);
  o0.rgb = max(0, applyRunnersVision(lutOutputColor, v2.xy));  // needed
  // end color grade

  if (injectedData.toneMapType != 0) {
    o0.rgb = UpgradeToneMap(color_hdr, color_sdr, renodx::color::srgb::DecodeSafe(o0.rgb), injectedData.colorGradeLUTStrength);

    if (injectedData.toneMapHueCorrection || injectedData.toneMapBlend) {
      float3 vanillaTM = applyVanillaTonemap(untonemapped);
      float3 correct_color = renodx::color::srgb::Encode(sampleLUT(colorGradingTexture, vanillaTM));
      correct_color = renodx::color::srgb::Decode(max(0, applyRunnersVision(correct_color, v2.xy)));
      correct_color = lerp(vanillaTM, correct_color, injectedData.colorGradeLUTStrength);

      o0.rgb = renodx::color::correct::HueOKLab(o0.rgb, correct_color, injectedData.toneMapHueCorrection);
      if (injectedData.toneMapBlend) {
        float3 negHDR = min(0, o0.rgb);
        o0.rgb = lerp(correct_color, max(0, o0.rgb), saturate(correct_color)) + negHDR;
      }
    }

    o0.rgb = renodx::color::srgb::EncodeSafe(o0.rgb);

  } else {
    o0.rgb = max(0, lerp(renodx::color::srgb::Encode(color_sdr), o0.rgb, injectedData.colorGradeLUTStrength));
  }

  o0.rgb = PostToneMapScale(o0.rgb);

  o0.w = saturate(dot(r0.xyz, float3(0.298999995, 0.587000012, 0.114)));  // clamp alpha
  return;
}
