#ifndef SRC_GAMES_DISHONORED2_DISHONORED2LUT_HLSL_
#define SRC_GAMES_DISHONORED2_DISHONORED2LUT_HLSL_

#define FLT_MIN asfloat(0x00800000)  //1.175494351e-38f
#define FLT_MAX asfloat(0x7F7FFFFF)  //3.402823466e+38f

// Color grading/charts tex lookup
float3 TexColorChart3D(Texture3D chartTex, SamplerState ssChart, float3 cImage, uint lutSize)
{
  float LUTCoordinatesScale = (lutSize - 1.f) / lutSize; // Alternatively "1-(1/LUT_SIZE)"
  float LUTCoordinatesOffset = 1.f / (2.f * lutSize); // Alternatively "(0.5/LUT_SIZE)"
  float3 LUTCoordinates = (cImage.rgb * LUTCoordinatesScale) + LUTCoordinatesOffset;
  return chartTex.SampleLevel(ssChart, LUTCoordinates, 0).rgb;
}

// Returns 0, 1 or FLT_MAX if "dividend" is 0
float safeDivision(float quotient, float dividend, int fallbackMode = 0)
{
	if (dividend == 0.f) {
        if (fallbackMode == 0)
          return 0;
        if (fallbackMode == 1)
          return sign(quotient);
        return FLT_MAX * sign(quotient);
    }
    return quotient / dividend;
}

// Returns 0, 1 or FLT_MAX if "dividend" is 0
float3 safeDivision(float3 quotient, float3 dividend, int fallbackMode = 0)
{
    return float3(safeDivision(quotient.x, dividend.x, fallbackMode), safeDivision(quotient.y, dividend.y, fallbackMode), safeDivision(quotient.z, dividend.z, fallbackMode));
}
float3 linear_to_gamma(float3 Color, float Gamma = 2.2f)
{
	return pow(Color, 1.f / Gamma);
}

float3 linear_to_gamma_mirrored(float3 Color, float Gamma = 2.2f)
{
	return linear_to_gamma(abs(Color), Gamma) * sign(Color);
}

float3 gamma_to_linear(float3 Color, float Gamma = 2.2f)
{
	return pow(Color, Gamma);
}

float3 gamma_to_linear_mirrored(float3 Color, float Gamma = 2.2f)
{
	return gamma_to_linear(abs(Color), Gamma) * sign(Color);
}

float linear_to_sRGB_gamma(float channel)
{
	if (channel <= 0.0031308f)
	{
		channel = channel * 12.92f;
	}
	else
	{
		channel = 1.055f * pow(channel, 1.f / 2.4f) - 0.055f;
	}
	return channel;
}

float3 linear_to_sRGB_gamma(float3 Color)
{
	return float3(linear_to_sRGB_gamma(Color.r), linear_to_sRGB_gamma(Color.g), linear_to_sRGB_gamma(Color.b));
}

// The sRGB gamma formula already works beyond the 0-1 range but mirroring (and thus running the pow below 0 too makes it look better)
float3 linear_to_sRGB_gamma_mirrored(float3 Color)
{
	return linear_to_sRGB_gamma(abs(Color)) * sign(Color);
}

float gamma_sRGB_to_linear(float channel)
{
	if (channel <= 0.04045f)
	{
		channel = channel / 12.92f;
	}
	else
	{
		channel = pow((channel + 0.055f) / 1.055f, 2.4f);
	}
	return channel;
}

float3 gamma_sRGB_to_linear(float3 Color)
{
	return float3(gamma_sRGB_to_linear(Color.r), gamma_sRGB_to_linear(Color.g), gamma_sRGB_to_linear(Color.b));
}

// Use gamma sRGB within 0-1 and gamma 2.2 beyond 0-1,
// this is because LUTs were baked with a gamma mismatch, but for extrapolation, we only replicate the gamma mismatch within the 0-1 range.
float3 ColorGradingLUTTransferFunctionIn(float3 col)
{
  float3 reEncodedColor = linear_to_gamma_mirrored(col);
  float3 colorInExcess = reEncodedColor - saturate(reEncodedColor);
  return linear_to_sRGB_gamma(saturate(col)) + colorInExcess;
}

float3 ColorGradingLUTTransferFunctionInInverted(float3 col)
{
  float3 reEncodedColor = gamma_to_linear_mirrored(col);
  float3 colorInExcess = reEncodedColor - saturate(reEncodedColor);
  return gamma_sRGB_to_linear(saturate(col)) + colorInExcess;
}

// Correct gamma space LUT coordinates to return more accurate values for linear in/out LUTs
float3 AdjustColorGradingLUTCoordinatesForLinearLUT (const float3 unclampedLUTCoordinatesGammaSpace, bool lutInputLinear = false, bool lutOutputLinear = false, const float3 lutMax3D = 32 - 1u, bool allowGammaCorrection = true, bool lutExtrapolation = false, bool specifyLinearSpace = false, float3 unclampedLUTCoordinatesLinearSpace = 0)
{
    if (!specifyLinearSpace)
    {
        unclampedLUTCoordinatesLinearSpace = lutExtrapolation ? ColorGradingLUTTransferFunctionInInverted(unclampedLUTCoordinatesGammaSpace) : gamma_sRGB_to_linear(unclampedLUTCoordinatesGammaSpace);
    }
  if (lutInputLinear)
  {
    // The "!lutOutputLinear" case would need coordinate adjustments to sample properly, but "linear in gamma out" LUTs don't really exist as they make no sense
    return unclampedLUTCoordinatesLinearSpace;
  }
    if (!lutOutputLinear)
    {
        return unclampedLUTCoordinatesGammaSpace;
    }
  // if (!lutInputLinear && lutOutputLinear)
  // Note: we do this even for values beyond the 0-1 range (and apply gamma there) as we have LUT extrapolation to help with that later.
  // Note: we use "sRGB" gamma independently of the "allowGammaCorrection" value, because LUT input coordinates are always in sRGB gamma (this is arguable, but it works this way and there's no need to change it).
  float3 previousLUTCoordinatesGammaSpace = floor(unclampedLUTCoordinatesGammaSpace * lutMax3D) / lutMax3D;
  float3 nextLUTCoordinatesGammaSpace = ceil(unclampedLUTCoordinatesGammaSpace * lutMax3D) / lutMax3D;
  float3 previousLUTCoordinatesLinearSpace = lutExtrapolation ? ColorGradingLUTTransferFunctionInInverted(previousLUTCoordinatesGammaSpace) : gamma_sRGB_to_linear(previousLUTCoordinatesGammaSpace);
  float3 nextLUTCoordinatesLinearSpace = lutExtrapolation ? ColorGradingLUTTransferFunctionInInverted(nextLUTCoordinatesGammaSpace) : gamma_sRGB_to_linear(nextLUTCoordinatesGammaSpace);
  // Every step size is different as it depends on where we are within the sRGB gamma to linear conversion.
  const float3 stepSize = nextLUTCoordinatesLinearSpace - previousLUTCoordinatesLinearSpace;
  // If "stepSize" is zero (due to the LUT pixel coords being exactly an integer), whether alpha is zero or one won't matter as "previousLUTCoordinatesGammaSpace" and "nextLUTCoordinatesGammaSpace" will be identical.
  const float3 blendAlpha = safeDivision(unclampedLUTCoordinatesLinearSpace - previousLUTCoordinatesLinearSpace, stepSize, 1);
  return lerp(previousLUTCoordinatesGammaSpace, nextLUTCoordinatesGammaSpace, blendAlpha); // new "clampedUV"
}


// Sample that allows to go beyond the 0-1 coordinates range through extrapolation.
// It finds the rate of change (acceleration) of the LUT color around the requested clamped coordinates, and guesses what color the sampling would have with the out of range coordinates.
// Extrapolating LUT by re-apply the rate of change has the benefit of consistency. If the LUT has the same color at (e.g.) uv 0.9 0.9 and 1.0 1.0, thus clipping to white or black, the extrapolation will also stay clipped.
// Additionally, if the LUT had inverted colors or highly fluctuating colors, extrapolation would work a lot better than a raw LUT out of range extraction with a luminance multiplier.
// 
// Note that this function might return "invalid colors", they could have negative values etc etc, so make sure to clamp them after if you need to.
float3 SampleLUTWithExtrapolation(Texture3D lut, SamplerState samplerState, const float3 neutralLutColor, bool inputLinear = true, bool lutLinear = true, bool outputLinear = true, bool gamaCorrection = true, bool lutExtrapolation = true, uint lutSize = 32)
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
        lutTexelRange = 1.0 / lutMax3D;
    }

    float3 neutralLutColorGamma = neutralLutColor;
    float3 neutralLutColorLinear = neutralLutColor;

    if (inputLinear)
    {
        // LUMA FT: change LUT input encoding to be scRGB/extrapolation friendly
    	neutralLutColorGamma = ColorGradingLUTTransferFunctionIn(neutralLutColorLinear);
    }
    else
    {
        // We could use "gamma_sRGB_to_linear_mirrored()" or "gamma_sRGB_to_linear()" for better performance but this might look better
    	neutralLutColorLinear = ColorGradingLUTTransferFunctionInInverted(neutralLutColorGamma);
    }

    // LUT coordinates in 0-1 range, without acknowleding the lut size or lut max (like, the half texel around each edge)
    const float3 unclampedUV = neutralLutColorGamma;

    const float3 clampedUV = saturate(unclampedUV);
    const float distanceFromUnclampedToClamped = length(unclampedUV - clampedUV);
    const bool uvOutOfRange = distanceFromUnclampedToClamped > FLT_MIN; // Some threshold is needed to avoid divisions by tiny numbers

    const float3 clampedSample = TexColorChart3D(lut, samplerState, AdjustColorGradingLUTCoordinatesForLinearLUT(clampedUV, lutLinear, lutMax3D), lutSize); // Use "clampedUV" instead of "unclampedUV" as we don't know what kind of sampler was in use here
    float3 outputSample = clampedSample;

    if (lutExtrapolation && uvOutOfRange)
    {
    	// Diagonal: Find the direction between the clamped and unclamped coordinates, flip it, and use it to determine where more centered texel for extrapolation is.
#if 0 //TODOFT1: improve the math
    	const float3 centeredUV = clampedUV - (normalize(unclampedUV - clampedUV) * lutTexelRange);
#elif 0
    	const float3 centeredUV = clampedUV - ((unclampedUV - clampedUV) * lutTexelRange);
#else // Go backwards by half or so, to be sure it's all good (this also fixes clipped LUTs, but can go beyond their max intent...)
      const float3 centeredUV = clampedUV - (normalize(unclampedUV - 0.5) * 0.5);
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
    	// LUMA FT: we leave negative luminances values here in case they were generated (independently of "FIX_INVALID_COLOR_GRADING_LUT_LUMINANCES"), everything works by channel in Prey, not much is done by luminance

        outputSample = extrapolatedSample;
        //outputSample = 0; //TODOFT1
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

#endif  // SRC_GAMES_DISHONORED2_DISHONORED2LUT_HLSL_