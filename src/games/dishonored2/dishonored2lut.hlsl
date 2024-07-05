#ifndef SRC_GAMES_DISHONORED_DISHONORED2LUT_HLSL_
#define SRC_GAMES_DISHONORED_DISHONORED2LUT_HLSL_

// Color grading/charts tex lookup
float3 TexColorChart3D(Texture3D chartTex, SamplerState ssChart, float3 cImage, uint lutSize)
{
  float LUTCoordinatesScale = (lutSize - 1.f) / lutSize; // Alternatively "1-(1/LUT_SIZE)"
  float LUTCoordinatesOffset = 1.f / (2.f * lutSize); // Alternatively "(0.5/LUT_SIZE)"
  float3 LUTCoordinates = (cImage.rgb * LUTCoordinatesScale) + LUTCoordinatesOffset;
  return chartTex.SampleLevel(ssChart, LUTCoordinates, 0).rgb;
}


float3 ColorGradingLUTTransferFunctionIn(float3 color) {
  return color;
}

float3 ColorGradingLUTTransferFunctionInInverted(float3 color) {
  return color; 
}

float3 AdjustColorGradingLUTCoordinatesForLinearLUT(float3 UV, float3 lutLinear, float3 lutMax3D) {
  return UV;
}

float3 gamma_to_linear_mirrored(float3 color) {
  return color;
}
float3 linear_to_gamma_mirrored(float3 color) {
  return color;
}

float3 SampleLUTWithExtrapolation(Texture3D lut, SamplerState samplerState, const float3 neutralLutColor, bool inputLinear = false, bool lutLinear = false, bool outputLinear = false, bool lutExtrapolation = true, uint lutSize = 0)
{
    float3 lutTexelRange;
    float3 lutMax3D;
    if (lutSize == 0)
    {
        // LUT size in texels
        float lutWidth;
        float lutHeight;
        float lutDepth;
        lut.GetDimensions(lutWidth, lutHeight, lutDepth);
        lutMax3D = float3(lutWidth, lutHeight, lutDepth) - 1.0;
        // The uv distance between the center of one texel and the next one
        lutTexelRange = 1.0 / lutMax3D;
    }
    else
    {
        lutMax3D = float3(lutSize - 1u, lutSize - 1u, lutSize - 1u);
        lutTexelRange = 1.0 / lutSize;
    }

    float3 neutralLutColorGamma = neutralLutColor;
    float3 neutralLutColorLinear = neutralLutColor;

    if (inputLinear)
    {
        // FT: change LUT input encoding to be scRGB/extrapolation friendly
        neutralLutColorGamma = ColorGradingLUTTransferFunctionIn(neutralLutColorLinear);
    }
    else
    {
        // We could use "gamma_sRGB_to_linear_mirrored()" or "gamma_sRGB_to_linear()" for better performance but this might look better
        neutralLutColorLinear = ColorGradingLUTTransferFunctionInInverted(neutralLutColorGamma);
    }

    // LUT coordinates in 0-1 range, without acknowledging the lut size or lut max (like, the half texel around each edge)
    const float3 unclampedUV = neutralLutColorGamma;

    const float3 clampedUV = saturate(unclampedUV);
    const float distanceFromUnclampedToClamped = length(unclampedUV - clampedUV);
#if 1 //TODOFT1: pick best number and port it to Starfield Luma (or maybe compare rgb values directly)
    const bool uvOutOfRange = distanceFromUnclampedToClamped > 0.00001; // Some threshold is needed to avoid divisions by tiny numbers
#else
    const bool uvOutOfRange = distanceFromUnclampedToClamped > FLT_MIN; // Some threshold is needed to avoid divisions by tiny numbers
#endif

    const float3 clampedSample = TexColorChart3D(lut, samplerState, AdjustColorGradingLUTCoordinatesForLinearLUT(clampedUV, lutLinear, lutMax3D), lutSize); // Use "clampedUV" instead of "unclampedUV" as we don't know what kind of sampler was in use here
    float3 outputSample = clampedSample;

    if (lutExtrapolation && uvOutOfRange)
    {
        // Diagonal: Find the direction between the clamped and unclamped coordinates, flip it, and use it to determine where more centered texel for extrapolation is.
#if 0 //TODOFT1: improve the math
        const float3 centeredUV = clampedUV - (normalize(unclampedUV - clampedUV) * lutTexelRange);
#else // Go backwards by half or so, to be sure it's all good (this also fixes clipped LUTs, but can go beyond their max intent...)
        static float backwardsAmount = 0.25;
        float3 centeredUV = clampedUV - (normalize(unclampedUV - clampedUV) * backwardsAmount);
#endif

        const float3 centeredSample = TexColorChart3D(lut, samplerState, AdjustColorGradingLUTCoordinatesForLinearLUT(centeredUV, lutLinear, lutMax3D), lutSize);
#if 1 // Generate "extrapolationRatio" in 3D and starting from linear as opposed to gamma value
        const float3 distanceFromUnclampedToClamped3D = gamma_to_linear_mirrored(unclampedUV) - gamma_to_linear_mirrored(clampedUV); // We can't directly use "neutralLutColorLinear" here as it's linearized with sRGB (theoretically), and doesn't have the gamma correction baked in
        const float3 distanceFromClampedToCentered = gamma_to_linear_mirrored(clampedUV) - gamma_to_linear_mirrored(centeredUV);
        const float3 extrapolationRatio = distanceFromClampedToCentered == 0.0 ? 0.0 : (distanceFromUnclampedToClamped3D / distanceFromClampedToCentered);
#else
        const float distanceFromClampedToCentered = length(clampedUV - centeredUV);
        const float extrapolationRatio = distanceFromClampedToCentered == 0.0 ? 0.0 : (distanceFromUnclampedToClamped / distanceFromClampedToCentered);
#endif
        float3 extrapolatedSample;
        bool ignoreExtrapolationGammaCorrection = false;
#if 0
        ignoreExtrapolationGammaCorrection = true;
#endif
        if (lutLinear || ignoreExtrapolationGammaCorrection)
        {
            extrapolatedSample = lerp(centeredSample, clampedSample, 1.0 + extrapolationRatio);
        }
        else
        {
            //TODOFT: write optimized path to avoid back and forth gamma conversions in some paths?
            extrapolatedSample = linear_to_gamma_mirrored(lerp(gamma_to_linear_mirrored(centeredSample), gamma_to_linear_mirrored(clampedSample), 1.0 + extrapolationRatio));
        }
        // FT: we leave negative luminances values here in case they were generated, everything works by channel in Prey, not much is done by luminance

        outputSample = extrapolatedSample;
    }

    //TODOFT1: move LUT gamma correction (sRGB/2.2) from LUT mixing to Tonemapping (per pixel...)
    if (!lutLinear && outputLinear)
    {
        outputSample.xyz = gamma_to_linear_mirrored(outputSample.xyz);
    }
    else if (lutLinear && !outputLinear)
    {
        outputSample.xyz = linear_to_gamma_mirrored(outputSample.xyz);
    }
    else if (lutLinear && outputLinear)
    {
        // Nothing to do here
    }
    else if (!lutLinear && !outputLinear)
    {
        // Nothing to do here
    }
    return outputSample;
}

float3 satCorrection(float3 incorrectColor, float3 correctColor) {
  float3 correctLCh = okLChFromBT709(correctColor);
  float3 incorrectLCh = okLChFromBT709(incorrectColor);
  incorrectLCh[1] = correctLCh[1];
  float3 color = bt709FromOKLCh(incorrectLCh);
  color = mul(BT709_2_AP1_MAT, color);  // Convert to AP1
  color = max(0, color);                // Clamp to AP1
  color = mul(AP1_2_BT709_MAT, color);  // Convert BT709
  return color;
}

#endif  // SRC_GAMES_DISHONORED_DISHONORED2LUT_HLSL_