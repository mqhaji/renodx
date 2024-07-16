#ifndef SRC_COMMON_HLSL
#define SRC_COMMON_HLSL

#define FLT_MIN asfloat(0x00800000)  //1.175494351e-38f
#define FLT_MAX asfloat(0x7F7FFFFF)  //3.402823466e+38f

#ifndef ENABLE_LINEAR_SPACE_POST_PROCESS
#define ENABLE_LINEAR_SPACE_POST_PROCESS 1
#endif
// We changed LUT format to R16G16B16A16F so in their mixing shaders, we store them in linear space, for higher output quality and to keep output values beyond 1 (their input coordinates are still in gamma sRGB (and then need 2.2 gamma correction))
#ifndef ENABLE_LINEAR_COLOR_GRADING_LUT
#define ENABLE_LINEAR_COLOR_GRADING_LUT 1
#endif
// As many games, Prey rendered and tonemapped in linear space, though applied the sRGB gamma transfer function to apply the color grading LUT.
// Almost all TVs follow gamma 2.2 and most monitors also do, so to mantain the SDR look (and near black level), we need to linearize with gamma 2.2 and not sRGB.
// Disabling this will linearize with gamma sRGB, ignoring that the game would have been developed on (and for) gamma 2.2 displays.
// Note that if "ENABLE_LINEAR_SPACE_POST_PROCESS" is off, this simply determines how gamma is linearized for intermediary operations, while everything stays in sRGB gamma when stored in textures,
// so this would also go to determine how the final shader should linearize (if true, from 2.2, if false, from sRGB, thus causing raised blacks).
#ifndef ENABLE_GAMMA_CORRECTION
#define ENABLE_GAMMA_CORRECTION 1
#endif
// Necessary for HDR
#define ENABLE_LUT_EXTRAPOLATION 1
// It's better to leave the classic LUT interpolation (bilinear/trilinear), LUTs in Prey are very close to being neutral so tetrahedral interpolation just shifts their colors without gaining much
#define ENABLE_LUT_TETRAHEDRAL_INTERPOLATION 0
// 0 Vanilla SDR, 1 Luma HDR (Vanilla+ Hable/DICE mix), 2 Untonemapped
#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif
#define DELAY_HDR_TONEMAP 1
//TODOFT: delete? No, improve it
#define ENABLE_HDR_BOOST 0
// Especially needed if "ENABLE_LINEAR_SPACE_POST_PROCESS" is true
#define ENABLE_HDR_SUNSHAFTS 1
#define AUTO_HDR_VIDEOS 1
#define DELAY_DITHERING 1
// Do it higher than 8 bit for HDR
#define DITHERING_BIT_DEPTH 9u
// 0 None
// 1 Reduce saturation and increase brightness until luminance is >= 0
// 2 Clip negative colors (makes luminance >= 0)
// 3 Snap to black
#define INVALID_COLOR_GRADING_LUT_LUMINANCES_FIX_TYPE 1
// 0 SSDO (Vanilla, CryEngine)
// 1 GTAO (Luma)
#define SSAO_TYPE 0
// 0 Vanilla
// 1 High (best balance for 2024 GPUs)
// 2 Extreme (bad performance)
#define SSAO_QUALITY 1
#define ENABLE_SSAO_DENOISE 1
// Requires TAA enabled to not look terrible
#define ENABLE_SSAO_TEMPORAL 0
#define ENABLE_TAA_DEJITTER 1

//TODOFT2: fix SSAO/SSDO quality
//TODOFT: linearize the LUT mixing shader output too "ENABLE_LINEAR_COLOR_GRADING_LUT" is true? (MergeColorChartsPS layer0Sampler LayerBlendAmount)
//TODOFT: add "DEVELOPMENT" macro to skip defining all stuff???

//TODOFT: at the moment disabling motion bloom increases the quality of rendering as bloom is sourced from a R11G11B10F color texture and fully replaces the color buffer (even on pixel with no bloom)
#define ENABLE_MOTION_BLUR 1
//TODOFT2: fix extremely bright lights just clipping the whole scene to bloomed white (it's probably the mods I downloaded (no))
//TODOFT2: change how we blend in bloom to additive? There's a branch for it already
#define ENABLE_BLOOM 1
#define ENABLE_SSAO 1
// Disables all kinds of AA (SMAA, FXAA, TAA, ...)
#define ENABLE_AA 1
// Optional SMAA pass being run before TAA
#define ENABLE_SMAA 0
// Optional TAA pass being run after the optional SMAA pass
#define ENABLE_TAA 1
#define ENABLE_COLOR_GRADING_LUT 1
#define ENABLE_SUNSHAFTS 0
#define ENABLE_ARK_CUSTOM_POST_PROCESS 1
#define ENABLE_LENS_OPTICS 1
#define ENABLE_SHARPENING 1
#define ENABLE_CHROMATIC_ABERRATION 1
#define ENABLE_VIGNETTE 1
#define ENABLE_FILM_GRAIN 1
// Disabled as we are now in HDR (10 or 16 bits)
#define ENABLE_DITHERING 0
// This might also disable interfaces in the 3D scene, like compute screens
#define ENABLE_UI 1

// Test extra saturation to see if it passes through (HDR colors)
#define TEST_HIGH_SATURATION_GAMUT 0
#define TEST_TONEMAP_OUTPUT 0
// 0 None
// 1 Neutral LUT
// 2 Neutral LUT + bypass extrapolation
#define FORCE_NEUTRAL_COLOR_GRADING_LUT_TYPE 0
#define DRAW_LUT 1
#define TEST_LUT_EXTRAPOLATION 0
// Pixel scale
//TODOFT: lower before submitting
#define DRAW_LUT_TEXTURE_SCALE 22u
#define TEST_TINT 1
// Tests some alpha blends stuff
#define TEST_UI 1
#define TEST_MOTION_BLUR 0
//TODOFT1: test it
#define TEST_LENS_OPTICS 0
#define TEST_DITHERING 0
#define TEST_SMAA_EDGES 0

#define LUT_SIZE 32u
#define LUT_MAX (LUT_SIZE - 1u)

// SDR linear mid gray.
// This is based on the commonly used value, though perception space mid gray (0.5) in sRGB or Gamma 2.2 would theoretically be ~0.2155 in linear.
static const float MidGray = 0.18f;
static const float DefaultGamma = 2.2f;
static const float3 Rec709_Luminance = float3( 0.2126f, 0.7152f, 0.0722f );
static const float ITU_WhiteLevelNits = 203.0f;
static const float sRGB_WhiteLevelNits = 80.0f;

//TODOFT: move params and set them used defined and set good defaults
static const float GamePaperWhiteNits = sRGB_WhiteLevelNits;
static const float UIPaperWhiteNits = sRGB_WhiteLevelNits;
static const float PeakWhiteNits = 1050.0f;

#define PI 3.141592653589793238462643383279502884197
#define PI_X2 (PI * 2.0)
#define PI_X4 (PI_X4 * 4.0)

float GetLuminance( float3 color )
{
	return dot( color, Rec709_Luminance );
}

float average(float3 color)
{
	return (color.x + color.y + color.z) / 3.f;
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

float3 linear_to_gamma(float3 Color, float Gamma = DefaultGamma)
{
	return pow(Color, 1.f / Gamma);
}

float3 linear_to_gamma_mirrored(float3 Color, float Gamma = DefaultGamma)
{
	return linear_to_gamma(abs(Color), Gamma) * sign(Color);
}

// 1 component
float gamma_to_linear1(float Color, float Gamma = DefaultGamma)
{
	return pow(Color, Gamma);
}

float3 gamma_to_linear(float3 Color, float Gamma = DefaultGamma)
{
	return pow(Color, Gamma);
}

float3 gamma_to_linear_mirrored(float3 Color, float Gamma = DefaultGamma)
{
	return gamma_to_linear(abs(Color), Gamma) * sign(Color);
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

// The sRGB gamma formula already works beyond the 0-1 range but mirroring (and thus running the pow below 0 too makes it look better)
float3 gamma_sRGB_to_linear_mirrored(float3 Color)
{
	return gamma_sRGB_to_linear(abs(Color)) * sign(Color);
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

// LUMA FT: avoid using this, use "linear_to_sRGB_gamma_mirrored()" instead
float3 LinearToSRGB(float3 col)
{
#if 0 // LUMA FT: disabled saturate(), it's unnecessary
	col = saturate(col);
#endif
#if 1
  return linear_to_sRGB_gamma(col);
#else // CryEngine version (compiles to the same code)
	return (col < 0.0031308) ? 12.92 * col : 1.055 * pow(col, 1.0 / 2.4) - float3(0.055, 0.055, 0.055);
#endif
}

// Optimized gamma<->linear functions (don't use unless really necessary, they are not accurate)
float3 sqr_mirrored(float3 x)
{
	return (x * x) * sign(x); // LUMA FT: added mirroring to support negative colors
}
float3 sqrt_mirrored(float3 x)
{
	return sqrt(abs(x)) * sign(x); // LUMA FT: added mirroring to support negative colors
}

// Formulas that either uses 2.2 or sRGB gamma depending on a global definition
float3 game_gamma_to_linear_mirrored(float3 Color)
{
#if ENABLE_GAMMA_CORRECTION
	return gamma_to_linear_mirrored(Color);
#else
  return gamma_sRGB_to_linear_mirrored(Color);
#endif
}
float3 linear_to_game_gamma_mirrored(float3 Color)
{
#if ENABLE_GAMMA_CORRECTION
	return linear_to_gamma_mirrored(Color);
#else
  return linear_to_sRGB_gamma_mirrored(Color);
#endif
}

// LUMA FT: functions to convert an SDR color (optionally in gamma space) to an HDR one (optionally linear * paper white).
// This should be used for any color that writes on the color buffer (or back buffer) from tonemapping on.
float3 SDRToHDR(float3 Color, bool GammaSpace = true, bool UI = false)
{
#if ENABLE_LINEAR_SPACE_POST_PROCESS
  if (GammaSpace)
  {
    Color.rgb = game_gamma_to_linear_mirrored(Color.rgb);
  }
	const float paperWhite = (UI ? UIPaperWhiteNits : GamePaperWhiteNits) / sRGB_WhiteLevelNits;
  Color.rgb *= paperWhite;
#else // ENABLE_LINEAR_SPACE_POST_PROCESS
  if (!GammaSpace)
  {
    Color.rgb = linear_to_game_gamma_mirrored(Color.rgb);
  }
#endif // ENABLE_LINEAR_SPACE_POST_PROCESS
	return Color;
}
float4 SDRToHDR(float4 Color, bool GammaSpace = true, bool FixAlpha = false, bool UI = false)
{
#if ENABLE_LINEAR_SPACE_POST_PROCESS
  if (FixAlpha)
  {
    float HDRUIBlendPow = lerp(1.f, 1.f / DefaultGamma, 0.5f);
    Color.a = pow(abs(Color.a), HDRUIBlendPow) * sign(Color.a);
  }
#endif // ENABLE_LINEAR_SPACE_POST_PROCESS
	return float4(SDRToHDR(Color.rgb, GammaSpace, UI), Color.a);
}

// LUMA FT: added these functions to decode and re-encode the "back buffer" from any range to a range that roughly matched SDR linear space
float3 EncodeBackBufferFromLinearSDRRange(float3 color)
{
#if ENABLE_LINEAR_SPACE_POST_PROCESS
	const float paperWhite = GamePaperWhiteNits / sRGB_WhiteLevelNits;
	return color * paperWhite;
#else
	return linear_to_game_gamma_mirrored(color);
#endif
}
float3 DecodeBackBufferToLinearSDRRange(float3 color)
{
#if ENABLE_LINEAR_SPACE_POST_PROCESS
	const float paperWhite = GamePaperWhiteNits / sRGB_WhiteLevelNits;
	return color / paperWhite;
#else
	return game_gamma_to_linear_mirrored(color);
#endif
}

// Use gamma sRGB within 0-1 and gamma 2.2 beyond 0-1,
// this is because LUTs were baked with a gamma mismatch, but for extrapolation, we only replicate the gamma mismatch within the 0-1 range.
float3 ColorGradingLUTTransferFunctionIn(float3 col, bool gammaCorrection)
{
  if (gammaCorrection)
  {
    float3 reEncodedColor = linear_to_gamma_mirrored(col);
    float3 colorInExcess = reEncodedColor - saturate(reEncodedColor);
    return linear_to_sRGB_gamma(saturate(col)) + colorInExcess;
  }
  return linear_to_sRGB_gamma_mirrored(col);
}

float3 ColorGradingLUTTransferFunctionInInverted(float3 col, bool gammaCorrection)
{
  if (gammaCorrection)
  {
    float3 reEncodedColor = gamma_to_linear_mirrored(col);
    float3 colorInExcess = reEncodedColor - saturate(reEncodedColor);
    return gamma_sRGB_to_linear(saturate(col)) + colorInExcess;
  }
  return gamma_sRGB_to_linear_mirrored(col);
}

float3 ColorGradingLUTTransferFunctionOut(float3 col, bool gammaCorrection)
{
  if (gammaCorrection)
  {
    float3 reEncodedColor = gamma_sRGB_to_linear_mirrored(col);
    float3 colorInExcess = reEncodedColor - saturate(reEncodedColor);
    return gamma_to_linear(saturate(col)) + colorInExcess;
  }
  return gamma_sRGB_to_linear_mirrored(col);
}

// Correct gamma space LUT coordinates to return more accurate values for linear in/out LUTs
float3 AdjustLUTCoordinatesForLinearLUT(const float3 unclampedLUTCoordinatesGammaSpace, bool lutInputLinear = false, bool lutOutputLinear = false, const float3 lutMax3D = LUT_SIZE - 1u, bool gammaCorrection = false, bool lutExtrapolation = false, bool specifyLinearSpaceLUTCoordinates = false, float3 unclampedLUTCoordinatesLinearSpace = 0)
{
	if (!specifyLinearSpaceLUTCoordinates)
	{
		unclampedLUTCoordinatesLinearSpace = lutExtrapolation ? ColorGradingLUTTransferFunctionInInverted(unclampedLUTCoordinatesGammaSpace, gammaCorrection) : gamma_sRGB_to_linear(unclampedLUTCoordinatesGammaSpace);
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
  // Note: we use "sRGB" gamma independently of the "gammaCorrection" value, because LUT input coordinates are always in sRGB gamma (this is arguable, but it works this way and there's no need to change it).
  float3 previousLUTCoordinatesGammaSpace = floor(unclampedLUTCoordinatesGammaSpace * lutMax3D) / lutMax3D;
  float3 nextLUTCoordinatesGammaSpace = ceil(unclampedLUTCoordinatesGammaSpace * lutMax3D) / lutMax3D;
  float3 previousLUTCoordinatesLinearSpace = lutExtrapolation ? ColorGradingLUTTransferFunctionInInverted(previousLUTCoordinatesGammaSpace, gammaCorrection) : gamma_sRGB_to_linear(previousLUTCoordinatesGammaSpace);
  float3 nextLUTCoordinatesLinearSpace = lutExtrapolation ? ColorGradingLUTTransferFunctionInInverted(nextLUTCoordinatesGammaSpace, gammaCorrection) : gamma_sRGB_to_linear(nextLUTCoordinatesGammaSpace);
  // Every step size is different as it depends on where we are within the sRGB gamma to linear conversion.
  const float3 stepSize = nextLUTCoordinatesLinearSpace - previousLUTCoordinatesLinearSpace;
  // If "stepSize" is zero (due to the LUT pixel coords being exactly an integer), whether alpha is zero or one won't matter as "previousLUTCoordinatesGammaSpace" and "nextLUTCoordinatesGammaSpace" will be identical.
  const float3 blendAlpha = safeDivision(unclampedLUTCoordinatesLinearSpace - previousLUTCoordinatesLinearSpace, stepSize, 1);
  return lerp(previousLUTCoordinatesGammaSpace, nextLUTCoordinatesGammaSpace, blendAlpha);
}

// Color grading/charts tex lookup
float3 TexColorChart2D(Texture3D chartTex, SamplerState ssChart, float3 cImage, bool debugLutOutputLinear = false, bool testGammaCorrectionInput = false)
{
  float LUTCoordinatesScale = (LUT_SIZE - 1.f) / LUT_SIZE; // Alternatively "1-(1/LUT_SIZE)"
  float LUTCoordinatesOffset = 1.f / (2.f * LUT_SIZE); // Alternatively "(0.5/LUT_SIZE)"
  float3 LUTCoordinates = (cImage.rgb * LUTCoordinatesScale) + LUTCoordinatesOffset;
  return chartTex.SampleLevel(ssChart, LUTCoordinates, 0).rgb;

}

// LUT sample that allows to go beyond the 0-1 coordinates range through extrapolation.
// It finds the rate of change (acceleration) of the LUT color around the requested clamped coordinates, and guesses what color the sampling would have with the out of range coordinates.
// Extrapolating LUT by re-apply the rate of change has the benefit of consistency. If the LUT has the same color at (e.g.) uv 0.9 0.9 and 1.0 1.0, thus clipping to white or black, the extrapolation will also stay clipped.
// Additionally, if the LUT had inverted colors or highly fluctuating colors, extrapolation would work a lot better than a raw LUT out of range extraction with a luminance multiplier.
// 
// This function allows the LUT to be in linear or gamma space on input coordinates and output color separately.
// If gamma correction is enabled, there will be a gamma sRGB to 2.2 mismatch fix (encode with gamma sRGB and decode with gamma 2.2, the typical SDR gamma mismatch).
// As of now the function is hardcoded for a 2D LUT but adding support for 3D is trivial.
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
float3 SampleLUTWithExtrapolation(Texture3D lut, SamplerState samplerState, const float3 neutralLutColor, bool inputLinear = false, bool lutInputLinear = false, bool lutOutputLinear = false, bool outputLinear = false, bool gammaCorrectionInput = bool(ENABLE_GAMMA_CORRECTION), bool gammaCorrectionOutput = bool(ENABLE_GAMMA_CORRECTION), bool lutExtrapolation = bool(ENABLE_LUT_EXTRAPOLATION), uint lutSize = LUT_SIZE)
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

    // Change LUT input encoding to be scRGB/extrapolation friendly.
    // sRGB gamma doesn't really make sense within the 0-1 range (especially below 0), so it's not really compatible with scRGB colors (that go to negative values to represent colors beyond sRGB),
    // so, whether we use gamma 2.2 (instead of sRGB) beyond the 0-1 range doesn't make that much difference, as neither of the two choices are "correct" or great,
    // but, using 2.2 beyond 0-1 would theoretically be a little be closer to human perception and thus increase the quality of extrapolation.
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

    float3 clampedSample = TexColorChart2D(lut, samplerState, AdjustLUTCoordinatesForLinearLUT(clampedUV, lutInputLinear, lutOutputLinear, lutMax3D), lutSize); // Use "clampedUV" instead of "unclampedUV" as we don't know what kind of sampler was in use here
    float3 outputSample = clampedSample;
    bool gammaCorrected = false;

    if (lutExtrapolation && uvOutOfRange)
    {
      const float3 centeredUVEdge = clampedUV - (normalize(unclampedUV - clampedUV) * lutTexelRange);
      const float3 centeredUVCenter = clampedUV - (normalize(unclampedUV - clampedUV) * 0.5 * sqrt(2.0)); //TODOFT: take angle of LUT color

      float3 centeredSampleEdge = TexColorChart2D(lut, samplerState, AdjustLUTCoordinatesForLinearLUT(centeredUVEdge, lutInputLinear, lutOutputLinear, lutMax3D), lutOutputLinear);
      float3 centeredSampleCenter = TexColorChart2D(lut, samplerState, AdjustLUTCoordinatesForLinearLUT(centeredUVCenter, lutInputLinear, lutOutputLinear, lutMax3D), lutOutputLinear);

    if (lutOutputLinear)
    {
      centeredSampleEdge = linear_to_sRGB_gamma_mirrored(centeredSampleEdge);
      centeredSampleCenter = linear_to_sRGB_gamma_mirrored(centeredSampleCenter);
      clampedSample = linear_to_sRGB_gamma_mirrored(clampedSample);
      lutOutputLinear = false;
    }

      float3 centeredUV = lerp(centeredUVEdge, centeredUVCenter, 0.5);
      float3 centeredSample = lerp(centeredSampleEdge, centeredSampleCenter, 0.5);
#if 0 // Generate "extrapolationRatio" in 3D and starting from linear as opposed to gamma value //TODOFT2
      // "neutralLutColorLinear" is equivalent to "ColorGradingLUTTransferFunctionInInverted(unclampedUV, gammaCorrectionInput)"
    	const float3 distanceFromUnclampedToClamped3D = neutralLutColorLinear - gamma_sRGB_to_linear(clampedUV);
    	const float3 distanceFromClampedToCentered = gamma_sRGB_to_linear(clampedUV) - gamma_sRGB_to_linear(centeredUV);
    	const float3 extrapolationRatio = distanceFromClampedToCentered == 0.0 ? 0.0 : (distanceFromUnclampedToClamped3D / distanceFromClampedToCentered);
#elif 0
    	const float3 distanceFromUnclampedToClamped3D = unclampedUV - clampedUV;
    	const float3 distanceFromClampedToCentered = clampedUV - centeredUV;
    	const float3 extrapolationRatio = distanceFromClampedToCentered == 0.0 ? 0.0 : (distanceFromUnclampedToClamped3D / distanceFromClampedToCentered);
#else
      // We calculate the extrapolation ratio in gamma space as theoretically it's more accurate, linear space can't be used for extrapolation as it doesn't match human perception, if the LUT sampling coordinates are 1.1, we'd want to extrapolate 10% more color, but in linear space it would be a lot less than that.
    	const float distanceFromClampedToCentered = length(clampedUV - centeredUV);
    	const float extrapolationRatio = distanceFromClampedToCentered == 0.0 ? 0.0 : (distanceFromUnclampedToClamped / distanceFromClampedToCentered);
#endif
    	float3 extrapolatedSample;
      static const bool ignoreExtrapolationGammaCorrection = false; // Optionally set to true as a lower quality optimization
    	if (lutOutputLinear || ignoreExtrapolationGammaCorrection) //TODOFT: ignoreExtrapolationGammaCorrection
    	{
        // We appply sRGB gamma even beyond 0-1 as if the color comes from a linear LUT, it shouldn't already have any kind of gamma correction applied to it, and thus 
#if 1
    	  extrapolatedSample = lerp(linear_to_sRGB_gamma_mirrored(centeredSample), linear_to_sRGB_gamma_mirrored(clampedSample), 1.0 + extrapolationRatio);
        lutOutputLinear = false;
#elif 1
    	  extrapolatedSample = lerp(linear_to_gamma_mirrored(centeredSample), linear_to_gamma_mirrored(clampedSample), 1.0 + extrapolationRatio);
        gammaCorrected = true;
        lutOutputLinear = false;
#else
        //TODOFT: maybe lerping in linear space is actually better, what matters is the extrapolation ratio after all (???), it applies independently of the color encoding (no it doesn't, because we are lerping beyond their range).
    	  extrapolatedSample = lerp(centeredSample, clampedSample, 1.0 + extrapolationRatio);
#endif
    	}
    	else
    	{
#if 1
    	  extrapolatedSample = lerp(centeredSample, clampedSample, 1.0 + extrapolationRatio);
#else
    	  extrapolatedSample = lerp(gamma_sRGB_to_linear_mirrored(centeredSample), gamma_sRGB_to_linear_mirrored(clampedSample), 1.0 + extrapolationRatio);
        lutOutputLinear = true; // Turn this flag on to pretend the lut output was linear (after we linearized it due to extrapolation)
#endif
    	}
    	// We leave negative luminances values here in case they were generated (independently of "INVALID_COLOR_GRADING_LUT_LUMINANCES_FIX_TYPE"), everything works by channel in Prey, not much is done by luminance

    	outputSample = extrapolatedSample;
#if FORCE_NEUTRAL_COLOR_GRADING_LUT_TYPE == 2
      outputSample = neutralLutColorLinear;
      lutOutputLinear = true;
      gammaCorrected = false;
#endif // FORCE_NEUTRAL_COLOR_GRADING_LUT_TYPE == 2
#if TEST_LUT_EXTRAPOLATION
    	outputSample = 0; //TODOFT2: test LUT draw
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
        outputSample.xyz = linear_to_sRGB_gamma_mirrored(outputSample.xyz);
        if (gammaCorrectionOutput && !gammaCorrected)
        {
          outputSample.xyz = linear_to_sRGB_gamma_mirrored(ColorGradingLUTTransferFunctionOut(outputSample.xyz, true));
        }
    }
    else if (lutOutputLinear && outputLinear)
    {
        if (gammaCorrectionOutput && !gammaCorrected)
        {
          // Decoding sRGB with gamma 2.2 crushes blacks (which is what we want)
          outputSample.xyz = gamma_to_linear_mirrored(ColorGradingLUTTransferFunctionIn(outputSample.xyz, true));
        }
    }
    else if (!lutOutputLinear && !outputLinear)
    {
        if (gammaCorrectionOutput && !gammaCorrected)
        {
          // Encoding linear sRGB with gamma 2.2 raises blacks, so we do the opposite (encode linear 2.2 with sRGB)
          outputSample.xyz = ColorGradingLUTTransferFunctionIn(gamma_to_linear_mirrored(outputSample.xyz), true);
        }
    }
    return outputSample;
}

// Copied from "DrawLUTTexture()"
bool ShouldSkipPostProcess(float2 PixelPosition)
{
#if TEST_MOTION_BLUR || TEST_SMAA_EDGES
  return true;
#endif // TEST_MOTION_BLUR || TEST_SMAA_EDGES
#if DRAW_LUT
	const uint LUTMinPixel = 0;
	uint LUTMaxPixel = LUT_MAX;
	uint LUTSizeMultiplier = 1;
  uint PixelScale = DRAW_LUT_TEXTURE_SCALE;
#if ENABLE_LUT_EXTRAPOLATION
	LUTSizeMultiplier = 2;
	LUTMaxPixel += LUT_SIZE * (LUTSizeMultiplier - 1);
	PixelScale = round(pow(PixelScale, 1.f / LUTSizeMultiplier));
#endif // ENABLE_LUT_EXTRAPOLATION

	const uint LUTPixelSideSize = LUT_SIZE * LUTSizeMultiplier;
	const uint2 LUTPixelPosition2D = PixelPosition / PixelScale;
	const uint3 LUTPixelPosition3D = uint3(LUTPixelPosition2D.x % LUTPixelSideSize, LUTPixelPosition2D.y, LUTPixelPosition2D.x / LUTPixelSideSize);
	if (!any(LUTPixelPosition3D < LUTMinPixel) && !any(LUTPixelPosition3D > LUTMaxPixel))
	{
    return true;
  }
#endif // DRAW_LUT
  return false;
}

float3 min3(float3 _a, float3 _b, float3 _c) { return min(_a, min(_b, _c)); }
float3 max3(float3 _a, float3 _b, float3 _c) { return max(_a, max(_b, _c)); }

float3 NRand3(float2 seed, float tr = 1.0)
{
  return frac(sin(dot(seed.xy, float2(34.483, 89.637) * tr)) * float3(29156.4765, 38273.5639, 47843.7546));
}

void ApplyDithering(inout float3 color, float2 uv, bool gammaSpace = true, float paperWhite = 1.0, uint bitDepth = DITHERING_BIT_DEPTH, float time = 0, bool useTime = false)
{
  // LUMA FT: added in/out encoding
  color /= paperWhite;
  //TODO LUMA: use log10 gamma or HDR10 PQ, it should match human perception more accurately
  if (!gammaSpace)
  {
    color = linear_to_game_gamma_mirrored(color); // Just use the same gamma function we use across the code, to keep it simple
  }

  uint ditherRatio; // LUMA FT: added dither bith depth support, 8 bit might be too much for 16 bit HDR
  // Optimized (static) branches
  if (bitDepth == 8) { ditherRatio = 255; }
  else if (bitDepth == 10) { ditherRatio = 1023; }
  else { ditherRatio = uint(round(pow(2, bitDepth) - 1.0)); }

  float3 rndValue;
	// Apply dithering in sRGB space to minimize quantization artifacts
	// Use a triangular distribution which gives a more uniform noise by avoiding low-noise areas
  if (useTime)
  {
    const float tr = frac(time / 1337.7331) + 0.5; // LUMA FT: added "time" randomization to avoid dithering being fixed per pixel over time
    rndValue = NRand3(uv, tr) + NRand3(uv + 0.5789, tr) - 1.0;
  }
  else
  {
    rndValue = NRand3(uv) + NRand3(uv + 0.5789) - 1.0; // LUMA FT: fixed this from subtracting 0.5 to 1 so it's mirrored and doesn't just raise colors
  }
#if TEST_DITHERING
  color += rndValue;
#else
  color += rndValue / ditherRatio;
#endif

  if (!gammaSpace)
  {
    color = game_gamma_to_linear_mirrored(color);
  }
  color *= paperWhite;
}

#endif  // SRC_COMMON_HLSL