#include "./tonemap.hlsl"

SamplerState SamplerLowResCapture_SMP_s : register(s5);
SamplerState SamplerFrameBuffer_SMP_s : register(s6);
SamplerState SamplerDistortion_SMP_s : register(s7);
SamplerState SamplerBloomMap0_SMP_s : register(s8);
SamplerState SamplerQuarterSizeBlur_SMP_s : register(s9);
SamplerState SamplerColourLUT_SMP_s : register(s10);
SamplerState SamplerNoise_SMP_s : register(s12);
SamplerState SamplerToneMapCurve_SMP_s : register(s14);
Texture2D<float4> SamplerLowResCapture_TEX : register(t5);
Texture2D<float4> SamplerFrameBuffer_TEX : register(t6);
Texture2D<float4> SamplerDistortion_TEX : register(t7);
Texture2D<float4> SamplerBloomMap0_TEX : register(t8);
Texture2D<float4> SamplerQuarterSizeBlur_TEX : register(t9);
Texture3D<float4> SamplerColourLUT_TEX : register(t10);
Texture2D<float4> SamplerNoise_TEX : register(t12);
Texture2D<float4> SamplerToneMapCurve_TEX : register(t14);

// 3Dmigoto declarations
#define cmp -

float3 renderPostFX(float4 v0, float4 v1) {
  float4 r0, r1, r2, r3;

  // motion blur?
  r0.xyzw = SamplerDistortion_TEX.Sample(SamplerDistortion_SMP_s, v0.xy).xyzw;
  r0.xy = r0.xy + -r0.zw;
  r0.zw = rp_parameter_ps[1].xy * r0.xy;
  r0.xy = r0.xy * rp_parameter_ps[1].xy + v0.xy;
  r0.xy = min(ScreenResolution[1].xy, r0.xy);
  r1.x = rp_parameter_ps[9].y + rp_parameter_ps[0].z;
  r1.x = cmp(0 < r1.x);
  if (r1.x != 0) {
    r1.x = 1 + rp_parameter_ps[0].z;
    r1.xy = r0.zw * r1.xx + v0.xy;
    r2.x = 1 + -rp_parameter_ps[0].z;
    r1.zw = r0.zw * r2.xx + v0.xy;
    r0.zw = v0.xy * float2(2, 2) + float2(-1, -1);
    r2.x = dot(r0.zw, r0.zw);
    r2.x = cmp(9.99999975e-005 < r2.x);
    r3.xy = r0.zw * rp_parameter_ps[9].yy + r1.xy;
    r3.zw = -r0.zw * rp_parameter_ps[9].yy + r1.zw;
    r1.xyzw = r2.xxxx ? r3.xyzw : r1.xyzw;
    r2.x = SamplerFrameBuffer_TEX.SampleLevel(SamplerFrameBuffer_SMP_s, r1.xy, 0).x;
    r2.y = SamplerFrameBuffer_TEX.SampleLevel(SamplerFrameBuffer_SMP_s, r0.xy, 0).y;
    r2.z = SamplerFrameBuffer_TEX.SampleLevel(SamplerFrameBuffer_SMP_s, r1.zw, 0).z;
  } else {
    r2.xyz = SamplerFrameBuffer_TEX.SampleLevel(SamplerFrameBuffer_SMP_s, r0.xy, 0).xyz;
  }
  r1.xyz = HDR_EncodeScale.www * r2.xyz;
  r2.xyzw = SamplerQuarterSizeBlur_TEX.Sample(SamplerQuarterSizeBlur_SMP_s, r0.xy).xyzw;
  r2.xyz = r2.xyz * r2.xyz;
  r2.xyz = r2.xyz * r2.xyz;
  r2.xyz = HDR_EncodeScale2.zzz * r2.xyz;
  r0.z = sqrt(r2.w);
  r0.z = rp_parameter_ps[3].x * r0.z;
  r2.xyz = r2.xyz * float3(4, 4, 4) + -r1.xyz;
  r1.xyz = r0.zzz * r2.xyz + r1.xyz;

  // bloom
  r2.xyz = SamplerBloomMap0_TEX.Sample(SamplerBloomMap0_SMP_s, r0.xy).xyz;
  r2.xyz = r2.xyz * r2.xyz;
  r1.xyz = r2.xyz * HDR_EncodeScale2.zzz + r1.xyz;

  // damage dizzy effect
  r0.xyzw = SamplerLowResCapture_TEX.Sample(SamplerLowResCapture_SMP_s, r0.xy).xyzw;
  r0.xyz = r0.xyz * r0.www;
  r0.xyz = HDR_EncodeScale2.zzz * r0.xyz;
  r0.xyz = r0.xyz * float3(8, 8, 8) + -r1.xyz;
  r0.xyz = rp_parameter_ps[10].www * r0.xyz + r1.xyz;
  // end damage dizzy

  return r0.xyz;
}

float3 applyVanillaTonemap(float3 untonemapped, float luminance) {
  float4 r0, r1, r2, r3, r4;

  // Luminance calculation
  r0.xyz = untonemapped;
  r0.w = luminance;
  r1.x = log2(r0.w);
  r1.x = r1.x * 0.693147182 + 12;
  r1.x = saturate(0.0625 * r1.x);
  r1.y = 0.25;

  // Sample tone map curve
  r1.x = SamplerToneMapCurve_TEX.SampleLevel(SamplerToneMapCurve_SMP_s, r1.xy, 0).x;
  r1.y = -r1.x * r1.x + 1;
  r2.xyz = max(float3(0, 0, 0), r0.xyz);
  r1.z = max(9.99999975e-005, r0.w);
  r2.xyz = r2.xyz / r1.zzz;
  r1.y = max(9.99999975e-006, r1.y);
  r2.xyz = log2(r2.xyz);
  r1.yzw = r2.xyz * r1.yyy;
  r1.yzw = exp2(r1.yzw);
  r2.xyz = r1.yzw * r1.xxx;
  r2.w = sqrt(r1.x);

  // Apply tone mapping based on debug parameters
  r3.x = cmp(ToneMappingDebugParams.y < r2.w);
  r2.w = cmp(r2.w < ToneMappingDebugParams.x);
  r4.xyzw = r2.wwww ? float4(0, 0, 1, 1) : 0;
  r3.xyzw = r3.xxxx ? float4(1, 0, 0, 1) : r4.xyzw;
  r2.w = ToneMappingDebugParams.z * r3.w;
  r1.xyz = -r1.yzw * r1.xxx + r3.xyz;
  r1.xyz = r2.www * r1.xyz + r2.xyz;
  r0.xyz = log2(r0.xyz);
  r0.xyz = r0.xyz * float3(0.693147182, 0.693147182, 0.693147182) + float3(12, 12, 12);

  // Saturate and sample the tone map curve for each channel
  r2.xyz = saturate(float3(0.0625, 0.0625, 0.0625) * r0.xyz);
  r2.w = 0.25;
  r0.x = SamplerToneMapCurve_TEX.SampleLevel(SamplerToneMapCurve_SMP_s, r2.xw, 0).x;
  r0.y = SamplerToneMapCurve_TEX.SampleLevel(SamplerToneMapCurve_SMP_s, r2.yw, 0).x;
  r0.z = SamplerToneMapCurve_TEX.SampleLevel(SamplerToneMapCurve_SMP_s, r2.zw, 0).x;

  // Adjust colors based on luminance
  r1.w = dot(float3(0.298999995, 0.587000012, 0.114), r0.xyz);
  r1.w = sqrt(r1.w);
  r2.x = cmp(ToneMappingDebugParams.y < r1.w);
  r1.w = cmp(r1.w < ToneMappingDebugParams.x);
  r3.xyzw = r1.wwww ? float4(0, 0, 1, 1) : 0;
  r2.xyzw = r2.xxxx ? float4(1, 0, 0, 1) : r3.xyzw;
  r1.w = ToneMappingDebugParams.z * r2.w;
  r2.xyz = r2.xyz + -r0.xyz;
  r0.xyz = r1.www * r2.xyz + r0.xyz;
  r0.xyz = r0.xyz + -r1.xyz;
  r0.xyz = ToneMappingDebugParams.www * r0.xyz + r1.xyz;

  return r0.xyz;
}

// debug stuff
// maybe has vignette?
float3 applyPostTMDebug(float3 tonemapped, float4 sv_position, float4 texcoord, float luminance) {
  float4 r0, r1, r2;
  r0.xyz = tonemapped;
  r0.w = luminance;

  // Light meter debug and vignette processing
  r1.xy = (uint2)sv_position.xy;
  uint4 uiDest;
  uiDest.xy = (uint2)r1.xy / int2(5, 5);  // Compute vignette grid
  r1.xy = uiDest.xy;
  r1.x = (int)r1.x + (int)r1.y;  // Sum the grid coordinates
  r1.x = (int)r1.x & 1;          // Create a checkerboard pattern for the vignette

  // Apply LightMeterDebugParams threshold checks
  r1.y = cmp(LightMeterDebugParams.y < r0.w);        // Check if brightness exceeds the threshold
  r0.w = cmp(r0.w < LightMeterDebugParams.x);        // Check if brightness is below a threshold
  r2.xyzw = r0.wwww ? float4(0, 0, 1, 1) : 0;        // Set blue if brightness is too low
  r2.xyzw = r1.yyyy ? float4(1, 0, 0, 1) : r2.xyzw;  // Set red if brightness is too high
  r0.w = LightMeterDebugParams.w * r2.w;             // Apply light meter multiplier

  // Final color adjustment based on debug/vignette
  r1.yzw = r2.xyz + -r0.xyz;
  r1.yzw = r0.www * r1.yzw + r0.xyz;
  r0.xyz = r1.xxx ? r1.yzw : r0.xyz;
  r0.xyz = texcoord.zzz * r0.xyz;  // Use v1 (texcoord) for final adjustment

  return r0.rgb;  // Return the modified tonemapped color
}

// Function to apply the LUT based on input color
float3 applyLUT(float3 lutInputColor, float lutStrength = 1.f) {
  float4 r0, r1;
  float3 lutOutputColor;

  // Apply the LUT
  r1.xyz = sign(lutInputColor) * sqrt(abs(lutInputColor));                   // Take square root of the input color and preserve the sign
  r1.xyz = rp_parameter_ps[2].zzz + r1.xyz;                                  // Apply an offset from rp_parameter_ps
  r1.xyz = SamplerColourLUT_TEX.Sample(SamplerColourLUT_SMP_s, r1.xyz).xyz;  // Sample the LUT using the adjusted color
  r0.w = rp_parameter_ps[2].y * rp_parameter_ps[2].x;                        // Calculate a scaling factor

  // Apply adjustments to the LUT output
  r1.xyz = r1.xyz * r1.xyz + -lutInputColor;
  lutOutputColor = r0.w * r1.xyz + lutInputColor;

  lutOutputColor = lerp(lutInputColor, lutOutputColor, lutStrength);
  return lutOutputColor;  // Return the adjusted color
}

float3 dualTonemap(float3 inputColor, float4 sv_position, float4 texcoord, float luminance) {
  float3 untonemapped = inputColor;

  untonemapped = applyPostTMDebug(untonemapped, sv_position, texcoord, luminance);

  const float paperWhite = injectedData.toneMapGameNits / renodx::color::srgb::REFERENCE_WHITE;
  const float peakWhite = injectedData.toneMapPeakNits / renodx::color::srgb::REFERENCE_WHITE;
  const float highlightsShoulderStart = paperWhite;

  float3 hdrTonemap = renodx::tonemap::dice::BT709(untonemapped * paperWhite, peakWhite, highlightsShoulderStart) / paperWhite;
  float3 sdrTonemap = renodx::tonemap::dice::BT709(untonemapped * paperWhite, paperWhite) / paperWhite;

  float3 sdrLUTOutput = applyLUT(sdrTonemap);

  float3 tonemapped;
  if (injectedData.toneMapType == 1) {
    tonemapped = renodx::tonemap::UpgradeToneMap(untonemapped, sdrTonemap, sdrLUTOutput, injectedData.colorGradeLUTStrength);
  } else {
    tonemapped = renodx::tonemap::UpgradeToneMap(hdrTonemap, sdrTonemap, sdrLUTOutput, injectedData.colorGradeLUTStrength);
  }

  return tonemapped;
}

void main(
    float4 v0: TEXCOORD0,
    float4 v1: TEXCOORD1,
    float4 v2: SV_Position0,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2, r3, r4;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyz = renderPostFX(v0, v1);

  float3 untonemapped = r0.xyz;
  const float untonemappedLum = renodx::color::luma::from::BT601(untonemapped);  // save for reuse
  float3 outputColor = r0.xyz;
  if (injectedData.toneMapType == 0) {  // tonemap
    r0.xyz = applyVanillaTonemap(untonemapped, untonemappedLum);
    r0.xyz = applyPostTMDebug(r0.rgb, v2, v1, untonemappedLum);
    r0.xyz = applyLUT(r0.rgb, injectedData.colorGradeLUTStrength);

    outputColor = r0.xyz;
  } else if (injectedData.toneMapType == 2) {
    const float3 vanillaMidGray = applyVanillaTonemap(float3(0.18, 0.18, 0.18), untonemappedLum);
    const float exposureScale = renodx::color::y::from::BT709(vanillaMidGray) / 0.18f;
    const float3 sdrTM = applyVanillaTonemap(untonemapped, untonemappedLum);
    float3 sdrColor = applyPostTMDebug(sdrTM, v2, v1, untonemappedLum);
    sdrColor = applyLUT(sdrColor, injectedData.colorGradeLUTStrength);
    sdrColor = renodx::color::grade::UserColorGrading(
        sdrColor,
        injectedData.colorGradeExposure,    // Exposure
        1.f,                                // Highlights, only applies to hdrColor
        injectedData.colorGradeShadows,     // Shadows
        injectedData.colorGradeContrast,    // Contrast
        injectedData.colorGradeSaturation,  // Saturation
        injectedData.colorGradeBlowout);    // Blowout

    float3 hdrColor = renodx::color::grade::UserColorGrading(
        untonemapped,
        injectedData.colorGradeExposure,            // Exposure
        injectedData.colorGradeHighlights * 1.12f,  // Highlights
        injectedData.colorGradeShadows * 0.4f,      // Shadows
        injectedData.colorGradeContrast * 0.66f,    // Contrast
        injectedData.colorGradeSaturation,          // Saturation
        injectedData.colorGradeBlowout,             // Blowout
        injectedData.toneMapHueCorrection,          // Hue Correction
        sdrTM);                                     // Vanilla Tonemapper
    hdrColor *= exposureScale;                      // Scale Mid-gray

    hdrColor = dualTonemap(hdrColor, v2, v1, untonemappedLum);

    // blend HDR with SDR
    float3 negHDR = min(0, hdrColor);  // save WCG
    float3 blendedColor = lerp(saturate(sdrColor), max(0, hdrColor), saturate(sdrColor));
    blendedColor += negHDR;  // add back WCG

    outputColor = blendedColor;
  } else {
    const float3 vanillaMidGray = applyVanillaTonemap(float3(0.18, 0.18, 0.18), untonemappedLum);
    const float exposureScale = renodx::color::y::from::BT709(vanillaMidGray) / 0.18f;
    const float3 sdrTM = applyVanillaTonemap(untonemapped, untonemappedLum);

    untonemapped = renodx::color::grade::UserColorGrading(
        untonemapped,
        injectedData.colorGradeExposure,    // Exposure
        injectedData.colorGradeHighlights,  // Highlights
        injectedData.colorGradeShadows,     // Shadows
        injectedData.colorGradeContrast,    // Contrast
        injectedData.colorGradeSaturation,  // Saturation
        injectedData.colorGradeBlowout,     // Blowout
        injectedData.toneMapHueCorrection,  // Hue Correction
        sdrTM);                             // Vanilla Tonemapper
    untonemapped *= exposureScale;          // Scale Mid-gray

    outputColor = dualTonemap(untonemapped, v2, v1, untonemappedLum);
  }

  // ignore user gamma, force 2.2
  r0.xyz = renodx::color::gamma::EncodeSafe(outputColor, 2.2f);  //  r0.xyz = pow(r0.xyz, OutputGamma.xxx);

  // film grain
  r0.rgb = applyFilmGrain(r0.rgb, SamplerNoise_TEX, SamplerNoise_SMP_s, v1);

  r0.xyz = (r0.xyz * rp_parameter_ps[0].xxx + rp_parameter_ps[0].yyy);  // r0.xyz = saturate(r0.xyz * rp_parameter_ps[0].xxx + rp_parameter_ps[0].yyy);
  o0.w = dot(r0.xyz, float3(0.298999995, 0.587000012, 0.114));
  o0.xyz = r0.xyz;

  return;
}
