#include "./Common.hlsl"


#define LUT_3D 1
#if LUT_3D
#define LUT_TEXTURE_TYPE Texture3D
#else
#define LUT_TEXTURE_TYPE Texture2D
#endif

// Replace these with whatever encoding your LUTs use (or use any you prefer in case your LUTs were linear).
// If you don't care about LUT gamma correction, you can set the "CORRECTED" versions of these defines to the same encoding as non corrected ones.
#define LINEAR_TO_GAMMA(x) linear_to_sRGB_gamma(x)
#define GAMMA_TO_LINEAR(x) gamma_sRGB_to_linear(x)
#define LINEAR_TO_GAMMA_MIRRORED(x) linear_to_sRGB_gamma_mirrored(x)
#define GAMMA_TO_LINEAR_MIRRORED(x) gamma_sRGB_to_linear_mirrored(x)
#define LINEAR_TO_GAMMA_CORRECTED(x) linear_to_gamma(x)
#define GAMMA_TO_LINEAR_CORRECTED(x) gamma_to_linear(x)
#define LINEAR_TO_GAMMA_CORRECTED_MIRRORED(x) linear_to_gamma_mirrored(x)
#define GAMMA_TO_LINEAR_CORRECTED_MIRRORED(x) gamma_to_linear_mirrored(x)

void FixColorGradingLUTNegativeLuminance(inout float3 col)
{
#if INVALID_COLOR_GRADING_LUT_LUMINANCES_FIX_TYPE > 0
  float luminance = GetLuminance(col.xyz);
  if (luminance < -FLT_MIN)
  {
#if INVALID_COLOR_GRADING_LUT_LUMINANCES_FIX_TYPE == 1
    // Make the color more "SDR" (less saturated, and thus less beyond Rec.709) until the luminance is not negative anymore (negative luminance means the color was beyond Rec.709 to begin with, unless all components were negative).
    // This is preferrable to simply clipping all negative colors or snapping to black, because it keeps some HDR colors, even if overall it's still "black", luminance wise.
    // This should work even in case "positiveLuminance" was <= 0, as it will simply make the color black.
    float3 positiveColor = max(col.xyz, 0.0);
    float3 negativeColor = min(col.xyz, 0.0);
    float positiveLuminance = GetLuminance(positiveColor);
    float negativeLuminance = GetLuminance(negativeColor);
    float negativePositiveLuminanceRatio = positiveLuminance / -negativeLuminance;
    negativeColor.xyz *= negativePositiveLuminanceRatio;
    col.xyz = positiveColor + negativeColor;
#elif INVALID_COLOR_GRADING_LUT_LUMINANCES_FIX_TYPE == 2
    col.xyz = max(col.xyz, 0.0);
#else // INVALID_COLOR_GRADING_LUT_LUMINANCES_FIX_TYPE >= 3
    col.xyz = 0.0;
#endif // INVALID_COLOR_GRADING_LUT_LUMINANCES_FIX_TYPE
  }
#endif // INVALID_COLOR_GRADING_LUT_LUMINANCES_FIX_TYPE
}



// Use gamma sRGB within 0-1 and gamma 2.2 beyond 0-1,
// this is because LUTs were baked with a gamma mismatch, but for extrapolation, we only replicate the gamma mismatch within the 0-1 range.
float3 ColorGradingLUTTransferFunctionIn(float3 col, bool gammaCorrection)
{
  if (gammaCorrection)
  {
    float3 reEncodedColor = LINEAR_TO_GAMMA_CORRECTED_MIRRORED(col);
    float3 colorInExcess = reEncodedColor - saturate(reEncodedColor);
    return LINEAR_TO_GAMMA(saturate(col)) + colorInExcess;
  }
  return LINEAR_TO_GAMMA_MIRRORED(col);
}

float3 ColorGradingLUTTransferFunctionInInverted(float3 col, bool gammaCorrection)
{
  if (gammaCorrection)
  {
    float3 reEncodedColor = GAMMA_TO_LINEAR_CORRECTED_MIRRORED(col);
    float3 colorInExcess = reEncodedColor - saturate(reEncodedColor);
    return GAMMA_TO_LINEAR(saturate(col)) + colorInExcess;
  }
  return GAMMA_TO_LINEAR_MIRRORED(col);
}

float3 ColorGradingLUTTransferFunctionOut(float3 col, bool gammaCorrection)
{
  if (gammaCorrection)
  {
    float3 reEncodedColor = GAMMA_TO_LINEAR_MIRRORED(col);
    float3 colorInExcess = reEncodedColor - saturate(reEncodedColor);
    return GAMMA_TO_LINEAR_CORRECTED(saturate(col)) + colorInExcess;
  }
  return GAMMA_TO_LINEAR_MIRRORED(col);
}

// Correct gamma space LUT coordinates to return more accurate values for linear in/out LUTs
float3 AdjustLUTCoordinatesForLinearLUT(const float3 unclampedLUTCoordinatesGammaSpace, bool lutInputLinear = false, bool lutOutputLinear = false, const float3 lutMax3D = LUT_SIZE - 1u, bool gammaCorrection = false, bool lutExtrapolation = false, bool specifyLinearSpaceLUTCoordinates = false, float3 unclampedLUTCoordinatesLinearSpace = 0)
{
	if (!specifyLinearSpaceLUTCoordinates)
	{
		unclampedLUTCoordinatesLinearSpace = lutExtrapolation ? ColorGradingLUTTransferFunctionInInverted(unclampedLUTCoordinatesGammaSpace, gammaCorrection) : GAMMA_TO_LINEAR(unclampedLUTCoordinatesGammaSpace);
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
#if 0 // Low quality version with no linear input correction
  return unclampedLUTCoordinatesGammaSpace;
#endif
  // Note: we do the LUT input coordinates correction even for values beyond the 0-1 range (and apply gamma there), as it's more correct this way, especially if we run LUT extrapolation with these values.
  // Note: we use "sRGB" gamma independently of the "gammaCorrection" value (as opposed to 2.2), because LUT input coordinates are always in sRGB gamma (this is arguable, but it works this way and there's no need to change it).
  float3 previousLUTCoordinatesGammaSpace = floor(unclampedLUTCoordinatesGammaSpace * lutMax3D) / lutMax3D;
  float3 nextLUTCoordinatesGammaSpace = ceil(unclampedLUTCoordinatesGammaSpace * lutMax3D) / lutMax3D;
  float3 previousLUTCoordinatesLinearSpace = lutExtrapolation ? ColorGradingLUTTransferFunctionInInverted(previousLUTCoordinatesGammaSpace, gammaCorrection) : GAMMA_TO_LINEAR(previousLUTCoordinatesGammaSpace);
  float3 nextLUTCoordinatesLinearSpace = lutExtrapolation ? ColorGradingLUTTransferFunctionInInverted(nextLUTCoordinatesGammaSpace, gammaCorrection) : GAMMA_TO_LINEAR(nextLUTCoordinatesGammaSpace);
  // Every step size is different as it depends on where we are within the sRGB gamma to linear conversion.
  const float3 stepSize = nextLUTCoordinatesLinearSpace - previousLUTCoordinatesLinearSpace;
  // If "stepSize" is zero (due to the LUT pixel coords being exactly an integer), whether alpha is zero or one won't matter as "previousLUTCoordinatesGammaSpace" and "nextLUTCoordinatesGammaSpace" will be identical.
  const float3 blendAlpha = safeDivision(unclampedLUTCoordinatesLinearSpace - previousLUTCoordinatesLinearSpace, stepSize, 1);
  return lerp(previousLUTCoordinatesGammaSpace, nextLUTCoordinatesGammaSpace, blendAlpha);
}

// Color grading/charts tex lookup. Called "TexColorChart2D()" in Vanilla code.
float3 SampleLUT(LUT_TEXTURE_TYPE lut, SamplerState samplerState, float3 cImage, uint lutSize = LUT_SIZE, bool debugLutOutputLinear = false, bool testGammaCorrectionInput = false)
{
#if FORCE_NEUTRAL_COLOR_GRADING_LUT_TYPE > 0
  return debugLutOutputLinear ? ColorGradingLUTTransferFunctionInInverted(cImage, testGammaCorrectionInput) : cImage; // Do not saturate() on purpose
#endif // FORCE_NEUTRAL_COLOR_GRADING_LUT_TYPE > 0

	const uint chartDimUint = lutSize;
	const float chartDim	= (float)chartDimUint;
	const float chartDimSqr	= chartDim * chartDim;
	const float chartMax	= chartDim - 1.0;
	const uint chartMaxUint = chartDimUint - 1u;

#if !ENABLE_LUT_TETRAHEDRAL_INTERPOLATION

#if LUT_3D
	const float3 scale = float3(chartMax, chartMax, chartMax) / chartDim;
	const float3 bias = float3(0.5, 0.5, 0.5) / chartDim;
  
	float3 lookup = saturate(cImage) * scale + bias;
  
 	return lut.Sample(samplerState, lookup).rgb;
#else // LUT_3D
	const float3 scale = float3(chartMax, chartMax, chartMax) / chartDim;
	const float3 bias = float3(0.5, 0.5, 0.0) / chartDim;

	float3 lookup = saturate(cImage) * scale + bias;
	
	// convert input color into 2d color chart lookup address 
	float slice = lookup.z * chartDim;	
	float sliceFrac = frac(slice);	
	float sliceIdx = slice - sliceFrac;
	
	lookup.x = (lookup.x + sliceIdx) / chartDim;
	
 	// lookup adjacent slices
 	float3 col0 = lut.Sample(samplerState, lookup.xy).rgb;
 	lookup.x += 1.0 / chartDim;
 	float3 col1 = lut.Sample(samplerState, lookup.xy).rgb;

	// linearly blend between slices
	return lerp(col0, col1, sliceFrac); // LUMA FT: changed to be a lerp (easier to read)
#endif // LUT_3D

#else // LUMA FT: added tetrahedral LUT interpolation (from Lilium) (note that this ignores the sampler) //TODOFT: to finish it... It's not working (and LUT_3D)

  const float lutTexelOffsetY = 0.5f /  chartDim;
  const float lutTexelOffsetX = 0.5f / chartDimSqr;

  // We need to clip the input coordinates as LUT texture samples below are not clamped.
  const float3 coords = saturate(cImage) * chartDimSqr; // Pixel coords 

  // floorCoords are on [0,chartMaxUint]
  float3 floorBaseCoords = floor(coords);
  float3 floorNextCoords = min(floorBaseCoords + 1.f, chartMaxUint);
  // baseInd and nextInd are on [0,1]
  float2 baseInd;
  float2 nextInd;

  baseInd.y = floorBaseCoords.g / chartDim + lutTexelOffsetY;
  nextInd.y = floorNextCoords.g / chartDim + lutTexelOffsetY;

  baseInd.x = (floorBaseCoords.b * chartDim / chartDimSqr) + (floorBaseCoords.r / chartDimSqr) + lutTexelOffsetX;
  nextInd.x = (floorNextCoords.b * chartDim / chartDimSqr) + (floorNextCoords.r / chartDimSqr) + lutTexelOffsetX;

  // indV2 and indV3 are on [0,chartMaxUint]
  float3 indV2;
  float3 indV3;

  // fract is on [0,1]
  float3 fract = frac(coords);

  const float3 v1 = lut.Sample(samplerState, baseInd.xy).rgb;
  //const float3 v1 = lut.Load(uint3(baseInd.xy, 0)).rgb;
  const float3 v4 = lut.Sample(samplerState, nextInd.xy).rgb;
  //const float3 v4 = lut.Load(uint3(nextInd.xy, 0)).rgb;

  float3 f1, f2, f3, f4;

  [flatten]
  if (fract.r >= fract.g)
  {
    [flatten]
    if (fract.g >= fract.b)  // R > G > B
    {
      indV2 = float3(1.f, 0.f, 0.f);
      indV3 = float3(1.f, 1.f, 0.f);

      f1 = 1.f - fract.r;
      f4 = fract.b;

      f2 = fract.r - fract.g;
      f3 = fract.g - fract.b;
    }
    else [flatten] if (fract.r >= fract.b)  // R > B > G
    {
      indV2 = float3(1.f, 0.f, 0.f);
      indV3 = float3(1.f, 0.f, 1.f);

      f1 = 1.f - fract.r;
      f4 = fract.g;

      f2 = fract.r - fract.b;
      f3 = fract.b - fract.g;
    }
    else  // B > R > G
    {
      indV2 = float3(0.f, 0.f, 1.f);
      indV3 = float3(1.f, 0.f, 1.f);

      f1 = 1.f - fract.b;
      f4 = fract.g;

      f2 = fract.b - fract.r;
      f3 = fract.r - fract.g;
    }
  }
  else
  {
    [flatten]
    if (fract.g <= fract.b)  // B > G > R
    {
      indV2 = float3(0.f, 0.f, 1.f);
      indV3 = float3(0.f, 1.f, 1.f);

      f1 = 1.f - fract.b;
      f4 = fract.r;

      f2 = fract.b - fract.g;
      f3 = fract.g - fract.r;
    }
    else [flatten] if (fract.r >= fract.b)  // G > R > B
    {
      indV2 = float3(0.f, 1.f, 0.f);
      indV3 = float3(1.f, 1.f, 0.f);

      f1 = 1.f - fract.g;
      f4 = fract.b;

      f2 = fract.g - fract.r;
      f3 = fract.r - fract.b;
    }
    else  // G > B > R
    {
      indV2 = float3(0.f, 1.f, 0.f);
      indV3 = float3(0.f, 1.f, 1.f);

      f1 = 1.f - fract.g;
      f4 = fract.r;

      f2 = fract.g - fract.b;
      f3 = fract.b - fract.r;
    }
  }

  indV2 = min(floorBaseCoords + indV2, chartMax);
  indV3 = min(floorBaseCoords + indV3, chartMax);

  float2 indV2Coords;
  float2 indV3Coords;

  indV2Coords.y = indV2.g / chartDim + lutTexelOffsetY;
  indV3Coords.y = indV3.g / chartDim + lutTexelOffsetY;

  indV2Coords.x = (indV2.b * chartDim / chartDimSqr) + (indV2.r / chartDimSqr) + lutTexelOffsetX;
  indV3Coords.x = (indV3.b * chartDim / chartDimSqr) + (indV3.r / chartDimSqr) + lutTexelOffsetX;

  const float3 v2 = lut.Sample(samplerState, indV2Coords.xy).rgb;
  //const float3 v2 = lut.Load(uint3(indV2Coords.xy, 0)).rgb;
  const float3 v3 = lut.Sample(samplerState, indV3Coords.xy).rgb;
  //const float3 v3 = lut.Load(uint3(indV3Coords.xy, 0)).rgb;

  return (f1 * v1) + (f2 * v2) + (f3 * v3) + (f4 * v4);

#endif // !ENABLE_LUT_TETRAHEDRAL_INTERPOLATION
}

// LUT sample that allows to go beyond the 0-1 coordinates range through extrapolation.
// It finds the rate of change (acceleration) of the LUT color around the requested clamped coordinates, and guesses what color the sampling would have with the out of range coordinates.
// Extrapolating LUT by re-apply the rate of change has the benefit of consistency. If the LUT has the same color at (e.g.) uv 0.9 0.9 and 1.0 1.0, thus clipping to white or black, the extrapolation will also stay clipped.
// Additionally, if the LUT had inverted colors or highly fluctuating colors, extrapolation would work a lot better than a raw LUT out of range extraction with a luminance multiplier.
// 
// This function allows the LUT to be in linear or gamma space on input coordinates and output color separately.
// If gamma correction is enabled, there will be a gamma sRGB to 2.2 mismatch fix (encode with gamma sRGB and decode with gamma 2.2, the typical SDR gamma mismatch).
// LUTs are expected to be of equal size on each axis (once unwrapped from 2D to 3D).
// 
// Parameters:
// -neutralLutColor: the input color, or in other words, LUT coordinates (theoretically the input color would match a "neutral" LUT, so it's called like that)
// -inputLinear: whether the "neutralLutColor" was in linear space or gamma space
// -lutInputLinear: whether the LUT input coordinates are in linear or gamma space (most of the times they are in gamma space)
// -lutOutputLinear: whether the LUT output coordinates are in linear or gamma space (most of the times they are in gamma space) (pre-baking gamma correction in the LUT isn't suggested due to the smallish number of samples it has)
// -outputLinear: whether we expect this function to output linear or gamma space (gamma correction might be applied nonetheless)
// -gammaCorrectionInput: whether we apply gamma correction on the LUT input coordinates (in the range beyond 0-1, input coordinates within 0-1 should never be corrected)
// -gammaCorrectionOutput: whether we apply gamma correction on the LUT output color
// 
// Note that this function might return "invalid colors", they could have negative values etc etc, so make sure to clamp them after if you need to.
float3 SampleLUTWithExtrapolation(LUT_TEXTURE_TYPE lut, SamplerState samplerState, const float3 neutralLutColor, bool inputLinear = false, bool lutInputLinear = false, bool lutOutputLinear = false, bool outputLinear = false, bool gammaCorrectionInput = bool(ENABLE_GAMMA_CORRECTION), bool gammaCorrectionOutput = bool(ENABLE_GAMMA_CORRECTION), bool lutExtrapolation = bool(ENABLE_LUT_EXTRAPOLATION), uint lutSize = LUT_SIZE)
{
    float3 lutTexelRange;
    float3 lutMax3D;
    if (lutSize == 0)
    {
    	// LUT size in texels
    	float lutWidth;
    	float lutHeight;
#if LUT_3D
    	float lutDepth;
    	lut.GetDimensions(lutWidth, lutHeight, lutDepth);
    	const float3 lutSize3D = float3(lutWidth, lutHeight, lutDepth);
#else
    	lut.GetDimensions(lutWidth, lutHeight);
    	lutWidth = sqrt(lutWidth); // 2D LUTs usually expand horizontally, and they do so in Prey
    	const float3 lutSize3D = float3(lutWidth, lutWidth, lutHeight);
#endif
      lutSize = lutWidth;
    	lutMax3D = lutSize3D - 1.0;
    	// The uv distance between the center of one texel and the next one
    	lutTexelRange = 1.0 / lutMax3D;
    }
    else
    {
    	lutMax3D = lutSize - 1u;
    	lutTexelRange = 1.0 / lutMax3D;
    }

    float3 neutralLutColorGamma = neutralLutColor;
    float3 neutralLutColorLinear = neutralLutColor;

    //TODOFT2: test "gammaCorrectionInput" on/off and merge it with "gammaCorrectionOutput"! It looks better without this!!!
    // Change LUT input encoding to be scRGB/extrapolation friendly.
    // sRGB gamma doesn't really make sense beyond the 0-1 range (especially below 0), so it's not really compatible with scRGB colors (that go to negative values to represent colors beyond sRGB),
    // so, whether we use gamma 2.2 (instead of sRGB) beyond the 0-1 range doesn't make that much difference, as neither of the two choices are "correct" or great,
    // but, using 2.2 beyond 0-1 would theoretically be a little be closer to human perception and thus increase the quality of extrapolation.
    // We still need to apply gamma correction on output anyway, this doesn't really influence that, it just makes the extrapolation more perception friendly.
    if (inputLinear)
    {
      neutralLutColorGamma = ColorGradingLUTTransferFunctionIn(neutralLutColorLinear, gammaCorrectionInput);
    }
    else
    {
    	neutralLutColorLinear = ColorGradingLUTTransferFunctionInInverted(neutralLutColorGamma, gammaCorrectionInput);
    }

    // LUT coordinates in 0-1 range, without acknowleding the lut size or lut max (like, the half texel around each edge)
    const float3 unclampedUV = neutralLutColorGamma;

    const float3 clampedUV = saturate(unclampedUV);
    const float distanceFromUnclampedToClamped = length(unclampedUV - clampedUV);
    const bool uvOutOfRange = distanceFromUnclampedToClamped > FLT_MIN; // Some threshold is needed to avoid divisions by tiny numbers

    float3 clampedSample = SampleLUT(lut, samplerState, AdjustLUTCoordinatesForLinearLUT(clampedUV, lutInputLinear, lutOutputLinear, lutMax3D), lutSize, lutOutputLinear); // Use "clampedUV" instead of "unclampedUV" as we don't know what kind of sampler was in use here
    float3 outputSample = clampedSample;
    bool gammaCorrected = false;

    if (lutExtrapolation && uvOutOfRange)
    {
      //TODO LUMA: to further improve the LUT extrapolation, we could make it run the extrapolation math with log10 encoding instead of sRGB gamma,
      //so it would match human perception more closely.
      //We could also make it run in OKLAB, and extract and re-apply the hue, saturation and luminance change to extrapolate colors.

      //TODOFT2: improve LUT extrapolation by doing two samples to extract the new color.
      //check all extrapolation code from SF Luma.
      //Try to do "if LUT sample > 1, then / 2 and * 2 after extrapolation", or try LUT extrapolation done before tonemapping to fix highlights.
      //Or find some other way to smooth uneven gradients beyond 1... maybe by blending in a neutral lut or something.
#if LUT_EXTRAPOLATION_QUALITY >= 1
    	// Find the direction between the clamped and unclamped coordinates, flip it, and use it to determine where more centered texel for extrapolation is.
      const float3 centeringNormal = normalize(unclampedUV - clampedUV); // This should always be valid as "unclampedUV" and guaranteed to be different from "clampedUV".
      static const float lutDiagonal = 1.0;
      float centeringAngle = acos(dot(centeringNormal, lutDiagonal));
      //TODOFT: just pick the biggest 2 components from r g b and compute angle against 1 1?
      float centeringAngle1 = acos(dot(centeringNormal, float3(1, 1, 0)));
      float centeringAngle2 = acos(dot(centeringNormal, float3(1, 0, 1)));
      float centeringAngle3 = acos(dot(centeringNormal, float3(0, 1, 1)));
      // Mirror if it's facing the other way (> 90 deg away)
      if (centeringAngle > PI)
      {
        centeringAngle = PI_X2 - centeringAngle;
      }
      if (centeringAngle1 > PI)
      {
        centeringAngle1 = PI_X2 - centeringAngle;
      }
      if (centeringAngle2 > PI)
      {
        centeringAngle2 = PI_X2 - centeringAngle;
      }
      if (centeringAngle3 > PI)
      {
        centeringAngle3 = PI_X2 - centeringAngle;
      }
      centeringAngle = min(centeringAngle, min(centeringAngle1, min(centeringAngle2, centeringAngle3)));
      const float centeringAngleAlpha = saturate(((PI / 4.0) - centeringAngle) / (PI / 4.0));
      static const float lutDiagonalLength = sqrt(3.0);
#if 0
      const float lutBackwardsDiagonalMultiplier = lerp(1.0, lutDiagonalLength, centeringAngleAlpha);
#else
      static const float lutBackwardsDiagonalMultiplier = lerp(1.0, lutDiagonalLength, 0.5);
#endif
    	const float3 centeredUVEdge = clampedUV - (centeringNormal * lutTexelRange * lutBackwardsDiagonalMultiplier);
    	static const float backwardsAmount = 0.5; //TODOFT: define 0.25? 0.5?
    	const float3 centeredUVCenter = clampedUV - (centeringNormal * backwardsAmount * lutBackwardsDiagonalMultiplier);

    	float3 centeredSampleEdge = SampleLUT(lut, samplerState, AdjustLUTCoordinatesForLinearLUT(centeredUVEdge, lutInputLinear, lutOutputLinear, lutMax3D), lutSize, lutOutputLinear);
    	float3 centeredSampleCenter = SampleLUT(lut, samplerState, AdjustLUTCoordinatesForLinearLUT(centeredUVCenter, lutInputLinear, lutOutputLinear, lutMax3D), lutSize, lutOutputLinear);

      // LUT extrapolation lerping is best run in gamma space, to be closer to perception.
      // We appply sRGB gamma even beyond 0-1 as if the color comes from a linear LUT, it shouldn't already have any kind of gamma correction applied to it (gamma correction runs later).
      if (lutOutputLinear)
      {
        centeredSampleEdge = LINEAR_TO_GAMMA_MIRRORED(centeredSampleEdge);
        centeredSampleCenter = LINEAR_TO_GAMMA_MIRRORED(centeredSampleCenter);
        clampedSample = LINEAR_TO_GAMMA_MIRRORED(clampedSample);
        lutOutputLinear = false;
      }

#if 1
      float3 centeredCenterToCenteredEdgeOffset = centeredUVEdge - centeredUVCenter;
      float3 centeredEdgeToEdgeOffset = clampedUV - centeredUVEdge;
      float3 distanceFromUnclampedToClamped3D = unclampedUV - clampedUV;
#if 0 // 3D never works (???)
      float3 centeredCenterToCenteredEdgeVelocity = centeredCenterToCenteredEdgeOffset != 0 ? ((centeredSampleEdge - centeredSampleCenter) / centeredCenterToCenteredEdgeOffset) : 0;
      float3 centeredEdgeToEdgeVelocity = centeredEdgeToEdgeOffset != 0 ? ((clampedSample - centeredSampleEdge) / centeredEdgeToEdgeOffset) : 0;
      float3 centeredCenterToEdgeAcceleration = (centeredEdgeToEdgeOffset - centeredCenterToCenteredEdgeOffset) != 0 ? ((centeredEdgeToEdgeVelocity - centeredCenterToCenteredEdgeVelocity) / (centeredEdgeToEdgeOffset - centeredCenterToCenteredEdgeOffset)) : 0;
      float3 time = distanceFromUnclampedToClamped3D;
#else
      float3 centeredCenterToCenteredEdgeVelocity = (centeredSampleEdge - centeredSampleCenter) / length(centeredCenterToCenteredEdgeOffset);
      float3 centeredEdgeToEdgeVelocity = (clampedSample - centeredSampleEdge) / length(centeredEdgeToEdgeOffset);
      float3 centeredCenterToEdgeAcceleration = (centeredEdgeToEdgeVelocity - centeredCenterToCenteredEdgeVelocity) / ((length(centeredEdgeToEdgeOffset) - length(centeredCenterToCenteredEdgeOffset)) / 1);
      float time = distanceFromUnclampedToClamped;
#endif
      
#if 0
      float3 extrapolatedSample = clampedSample + (lerp(centeredCenterToCenteredEdgeVelocity, centeredEdgeToEdgeVelocity, 0.5) * distanceFromUnclampedToClamped);
#else
      float3 velocity = centeredEdgeToEdgeVelocity;
      float3 acceleration = centeredCenterToEdgeAcceleration;
      float3 extrapolatedOffset = (velocity * time) + (0.5 * acceleration * time * time);
      float3 extrapolatedSample = clampedSample + extrapolatedOffset;
#endif

#else // Old
      // We average between the acceleration at the edge and the acceleration at the center (not really, this isn't either velocity nor acceleration)
    	float3 centeredUV = lerp(centeredUVCenter, centeredUVEdge, 0.5);
    	float3 centeredSample = lerp(centeredSampleCenter, centeredSampleEdge, 0.5);
#endif

#else // LUT_EXTRAPOLATION_QUALITY

#if 0
    	static const float backwardsAmount = 1.0; // ... value 3 is a good value for Prey because...
    	const float3 centeredUV = clampedUV - (normalize(unclampedUV - clampedUV) * lutTexelRange * backwardsAmount);
#else // Go backwards by half or so, to be sure it's all good (this also fixes clipped LUTs, but can go beyond their max intent...)
    	static const float backwardsAmount = 0.25 * sqrt(3.0);
    	const float3 centeredUV = clampedUV - (normalize(unclampedUV - clampedUV) * backwardsAmount);
#endif
    	float3 centeredSample = SampleLUT(lut, samplerState, AdjustLUTCoordinatesForLinearLUT(centeredUV, lutInputLinear, lutOutputLinear, lutMax3D), lutSize, lutOutputLinear);

      // LUT extrapolation lerping is best run in gamma space, to be closer to perception.
      // We appply sRGB gamma even beyond 0-1 as if the color comes from a linear LUT, it shouldn't already have any kind of gamma correction applied to it (gamma correction runs later).
    	if (lutOutputLinear)
    	{
        centeredSample = LINEAR_TO_GAMMA_MIRRORED(centeredSample);
        clampedSample = LINEAR_TO_GAMMA_MIRRORED(clampedSample);
        lutOutputLinear = false;
      }

      // We calculate the extrapolation ratio in gamma space as theoretically it's more accurate, linear space can't be used for extrapolation as it doesn't match human perception,
      // e.g. if the LUT sampling coordinates are 1.1, we'd want to extrapolate 10% more color, but in linear space it would be a lot less than that, thus the peak brightness would be compressed a lot more than it should.
    	const float distanceFromClampedToCentered = length(clampedUV - centeredUV);
    	const float extrapolationRatio = distanceFromClampedToCentered == 0.0 ? 0.0 : (distanceFromUnclampedToClamped / distanceFromClampedToCentered);

      float3 extrapolatedSample = lerp(centeredSample, clampedSample, 1.0 + extrapolationRatio);
      
	    float fLuminance = GetLuminance(extrapolatedSample);
	    //extrapolatedSample = lerp(extrapolatedSample, fLuminance.xxx, saturate(extrapolationRatio / 6.0));

#endif // LUT_EXTRAPOLATION_QUALITY

#if 0 // We can optionally leave or fix negative luminances colors here in case they were generated by the extrapolation, everything works by channel in Prey, not much is done by luminance, so this isn't needed until proven otherwise
      if (lutOutputLinear)
    	{
        FixColorGradingLUTNegativeLuminance(extrapolatedSample);
      }
      else
      {
        LINEAR_TO_GAMMA_MIRRORED(FixColorGradingLUTNegativeLuminance(GAMMA_TO_LINEAR_MIRRORED(extrapolatedSample)));
      }
#endif

    	outputSample = extrapolatedSample;
#if FORCE_NEUTRAL_COLOR_GRADING_LUT_TYPE == 2
      outputSample = neutralLutColorLinear;
      lutOutputLinear = true;
      gammaCorrected = false;
#endif // FORCE_NEUTRAL_COLOR_GRADING_LUT_TYPE == 2
#if TEST_LUT_EXTRAPOLATION
    	outputSample = 0;
#endif // TEST_LUT_EXTRAPOLATION
    }

    // We do gamma correction after sampling the LUT because baking the gamma mismatch correction into the LUT makes it lose a lot of quality
    if (!lutOutputLinear && outputLinear)
    {
        outputSample.xyz = ColorGradingLUTTransferFunctionOut(outputSample.xyz, gammaCorrectionOutput && !gammaCorrected);
    }
    else if (lutOutputLinear && !outputLinear)
    {
        // Note: we could probably optimize the gamma correction branch here by making a single function that applies the sRGB encode 2.2 decode gamma mismatch (we need to linearize with gamma 2.2 instead of sRGB to fix the gamma mismatch), but this branch isn't used at the moment.
        outputSample.xyz = LINEAR_TO_GAMMA_MIRRORED(outputSample.xyz);
        if (gammaCorrectionOutput && !gammaCorrected)
        {
          outputSample.xyz = LINEAR_TO_GAMMA_MIRRORED(ColorGradingLUTTransferFunctionOut(outputSample.xyz, true));
        }
    }
    else if (lutOutputLinear && outputLinear)
    {
        if (gammaCorrectionOutput && !gammaCorrected)
        {
          // Decoding sRGB with gamma 2.2 crushes blacks (which is what we want). We only correct the 0-1 range.
          outputSample.xyz = GAMMA_TO_LINEAR_CORRECTED_MIRRORED(ColorGradingLUTTransferFunctionIn(outputSample.xyz, true));
        }
    }
    else if (!lutOutputLinear && !outputLinear)
    {
        if (gammaCorrectionOutput && !gammaCorrected)
        {
          // Encoding linear sRGB with gamma 2.2 raises blacks (which is the opposite of what we want), so we do the opposite (encode linear 2.2 with sRGB). We only correct the 0-1 range.
          outputSample.xyz = ColorGradingLUTTransferFunctionIn(GAMMA_TO_LINEAR_CORRECTED_MIRRORED(outputSample.xyz), true);
        }
    }
    return outputSample;
}

// Note that this function expects "LUT_SIZE" to be divisible by 2. If your LUT is (e.g.) 15x instead of 16x, move some math to be floating point and round to the closest pixel.
// "PixelPosition" is expected to be centered around texles center, so the first pixel would be 0.5 0.5, not 0 0.
// This partially mirrors "ShouldSkipPostProcess()".
float3 DrawLUTTexture(LUT_TEXTURE_TYPE lut, SamplerState samplerState, float2 PixelPosition, inout bool DrawnLUT)
{
	const uint LUTMinPixel = 0; // Extra offset from the top left
	uint LUTMaxPixel = LUT_MAX; // Bottom (right) limit
	uint LUTSizeMultiplier = 1;
  uint PixelScale = DRAW_LUT_TEXTURE_SCALE;
#if ENABLE_LUT_EXTRAPOLATION
	LUTSizeMultiplier = 2; // This will end up multiplying the number of shown cube slices as well
	// Shift the LUT coordinates generation to account for 50% of extra area beyond 1 and 50% below 0,
	// so "LUTPixelPosition3D" would represent the LUT from -0.5 to 1.5 before being normalized.
	// The bottom and top 25% squares (cube sections) will be completely outside of the valid cube range and be completely extrapolated,
	// while for the middle 50% squares, only their outer half would be extrapolated.
	LUTMaxPixel += LUT_SIZE * (LUTSizeMultiplier - 1);
	PixelScale = round(pow(PixelScale, 1.f / LUTSizeMultiplier));
#endif // ENABLE_LUT_EXTRAPOLATION

	PixelPosition -= 0.5f;

	const uint LUTPixelSideSize = LUT_SIZE * LUTSizeMultiplier; // LUT pixel size (one dimension) on screen (with extrapolated pixels too)
	const uint2 LUTPixelPosition2D = round(PixelPosition / PixelScale); // Round to avoid the color accidentally snapping to the lower integer
	const uint3 LUTPixelPosition3D = uint3(LUTPixelPosition2D.x % LUTPixelSideSize, LUTPixelPosition2D.y, LUTPixelPosition2D.x / LUTPixelSideSize);
	if (!any(LUTPixelPosition3D < LUTMinPixel) && !any(LUTPixelPosition3D > LUTMaxPixel))
	{
    // Note that the LUT sampling function will still use bilinear sampling, we are just manually centering the LUT coordinates to match the center of texels.
		static const bool NearestNeighbor = false;

		DrawnLUT = true;

		// The color the neutral LUT would have, in sRGB gamma space
    float3 LUTCoordinates;

    if (NearestNeighbor)
    {
      LUTCoordinates = LUTPixelPosition3D / float(LUTMaxPixel);
    }
    else
    {
		  const float2 LUTPixelPosition2DFloat = PixelPosition / (float)PixelScale;
		  float3 LUTPixelPosition3DFloat = float3(fmod(LUTPixelPosition2DFloat.x, LUTPixelSideSize), LUTPixelPosition2DFloat.y, (uint)(LUTPixelPosition2DFloat.x / LUTPixelSideSize));
      LUTCoordinates = LUTPixelPosition3DFloat / float(LUTMaxPixel);
    }
    LUTCoordinates *= LUTSizeMultiplier;
    LUTCoordinates -= (LUTSizeMultiplier - 1.f) / 2.f;
#if ENABLE_LUT_EXTRAPOLATION && TEST_LUT_EXTRAPOLATION
    if (any(LUTCoordinates < -FLT_MIN) || any(LUTCoordinates > 1.f + FLT_MIN))
    {
		  return 0;
    }
#endif // ENABLE_LUT_EXTRAPOLATION && TEST_LUT_EXTRAPOLATION

  #if 1 // We might not want gamma correction on the debug LUT, gamma correction comes after extrapolation and isn't directly a part of the LUT, so it shouldn't affect its "raw" visualization
    bool gammaCorrectionInput = bool(ENABLE_GAMMA_CORRECTION);
    bool gammaCorrectionOutput = bool(ENABLE_LINEAR_SPACE_POST_PROCESS) && bool(ENABLE_GAMMA_CORRECTION);
  #else
    bool gammaCorrectionInput = false;
    bool gammaCorrectionOutput = false;
  #endif
		const float3 LUTColor = SampleLUTWithExtrapolation(lut, samplerState, LUTCoordinates, false, false, bool(ENABLE_LINEAR_COLOR_GRADING_LUT), bool(ENABLE_LINEAR_SPACE_POST_PROCESS), gammaCorrectionInput, gammaCorrectionOutput);
		return LUTColor;
	}
	return 0;
}