#ifndef SRC_COMMON_HLSL
#define SRC_COMMON_HLSL

// These should only ever be included through "Common.hlsl" and never individually
#include "./Math.hlsl"
#include "./Color.hlsl"
#include "./Settings.hlsl"

#define LUT_SIZE 16u
#define LUT_MAX (LUT_SIZE - 1u)

// The aspect ratio the game was developed against, in case some effects weren't scaling properly for other aspect ratios.
// For best results, we should consider the FOV Hor+ beyond 16:9 and Vert- below 16:9
// (so the 16:9 image is always visible, and the aspect ratio either extends the vertical or horizontal view).
static const float NativeAspectRatioWidth = 16.0;
static const float NativeAspectRatioHeight = 9.0;
static const float NativeAspectRatio = NativeAspectRatioWidth / NativeAspectRatioHeight;
// The vertical resolution that most likely was the most used by the game developers,
// we define this to scale up stuff that did not natively correctly scale by resolution.
// According to the developers, the game was mostly developed on 1080p displays, and some 1440p ones, so
// we are going for their middle point, but 1080 or 1440 would also work fine.
static const float BaseVerticalResolution = 1260.0;

// Exposure multiplier for sunshafts. It's useful to shift them towards a better range for float textures to avoid banding.
// This comes from vanilla values, it's not really meant to be changed.
static const float SunShaftsBrightnessMultiplier = 4.0;
// With "SUNSHAFTS_LOOK_TYPE" > 0 and "ENABLE_LENS_OPTICS_HDR", we apply exposure to sun shafts and lens optics as well.
// Given that exposure can deviate a lot from a value of 1, to the point where it would make lens optics effects look weird, we diminish its effect on them so it's less jarring, but still applies (which is visually nicer).
// The value should be between 0 and 1.
static const float SunShaftsAndLensOpticsExposureAlpha = 0.25; // Anything more than 0.25 can cause sun effects to be blinding if the exposure is too high (it's pretty high in some scenes)

// Formulas that either uses 2.2 or sRGB gamma depending on a global definition.
// Note that converting between linear and gamma space back and forth results in quality loss, especially over very high and very low values.
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
float3 SDRToHDR(float3 Color, bool InGammaSpace = true, bool UI = false)
{
  bool OutLinearSpace = bool(POST_PROCESS_SPACE_TYPE == 1) || (bool(POST_PROCESS_SPACE_TYPE >= 2) && !UI);
  if (OutLinearSpace)
  {
    if (InGammaSpace)
    {
      Color.rgb = game_gamma_to_linear_mirrored(Color.rgb);
    }
    const float paperWhite = (UI ? UIPaperWhiteNits : GamePaperWhiteNits) / sRGB_WhiteLevelNits;
    Color.rgb *= paperWhite;
  }
  else
  {
    if (!InGammaSpace)
    {
      Color.rgb = linear_to_game_gamma_mirrored(Color.rgb);
    }
  }
	return Color;
}
float4 SDRToHDR(float4 Color, bool InGammaSpace = true, bool FixAlpha = false, bool UI = false)
{
  bool OutLinearSpace = bool(POST_PROCESS_SPACE_TYPE == 1) || (bool(POST_PROCESS_SPACE_TYPE >= 2) && !UI);
  if (OutLinearSpace)
  {
    if (FixAlpha)
    {
      float HDRUIBlendPow = lerp(1.f, 1.f / DefaultGamma, 0.5f);
      Color.a = pow(abs(Color.a), HDRUIBlendPow) * sign(Color.a);
    }
  }
	return float4(SDRToHDR(Color.rgb, InGammaSpace, UI), Color.a);
}

// LUMA FT: added these functions to decode and re-encode the "back buffer" from any range to a range that roughly matched SDR linear space
float3 EncodeBackBufferFromLinearSDRRange(float3 color, bool UI = false)
{
  bool InLinearSpace = bool(POST_PROCESS_SPACE_TYPE == 1) || (bool(POST_PROCESS_SPACE_TYPE >= 2) && !UI);
  if (InLinearSpace)
  {
    const float paperWhite = GamePaperWhiteNits / sRGB_WhiteLevelNits;
    return color * paperWhite;
  }
  else
  {
  	return linear_to_game_gamma_mirrored(color);
  }
}
float3 DecodeBackBufferToLinearSDRRange(float3 color, bool UI = false)
{
  bool InLinearSpace = bool(POST_PROCESS_SPACE_TYPE == 1) || (bool(POST_PROCESS_SPACE_TYPE >= 2) && !UI);
  if (InLinearSpace)
  {
    const float paperWhite = GamePaperWhiteNits / sRGB_WhiteLevelNits;
    return color / paperWhite;
  }
  else
  {
  	return game_gamma_to_linear_mirrored(color);
  }
}

// Partially mirrors "DrawLUTTexture()".
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

	PixelPosition -= 0.5f;

	const uint LUTPixelSideSize = LUT_SIZE * LUTSizeMultiplier;
	const uint2 LUTPixelPosition2D = round(PixelPosition / PixelScale);
	const uint3 LUTPixelPosition3D = uint3(LUTPixelPosition2D.x % LUTPixelSideSize, LUTPixelPosition2D.y, LUTPixelPosition2D.x / LUTPixelSideSize);
	if (!any(LUTPixelPosition3D < LUTMinPixel) && !any(LUTPixelPosition3D > LUTMaxPixel))
	{
    return true;
  }
#endif // DRAW_LUT
  return false;
}

void ApplyDithering(inout float3 color, float2 uv, bool gammaSpace = true, float paperWhite = 1.0, uint bitDepth = DITHERING_BIT_DEPTH, float time = 0, bool useTime = false)
{
  // LUMA FT: added in/out encoding
  color /= paperWhite;
  float3 lastLinearColor = color;
  //TODO LUMA: use log10 gamma or HDR10 PQ, it should match human perception more accurately
  if (!gammaSpace)
  {
    color = linear_to_game_gamma_mirrored(color); // Just use the same gamma function we use across the code, to keep it simple
  }
  float3 lastGammaColor = color;

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
#else // TEST_DITHERING
  color += rndValue / ditherRatio;
#endif // TEST_DITHERING

  if (!gammaSpace)
  {
#if HIGH_QUALITY_POST_PROCESS_SPACE_CONVERSIONS
    color = lastLinearColor + (game_gamma_to_linear_mirrored(color) - game_gamma_to_linear_mirrored(lastGammaColor));
#else // HIGH_QUALITY_POST_PROCESS_SPACE_CONVERSIONS
    color = game_gamma_to_linear_mirrored(color);
#endif // HIGH_QUALITY_POST_PROCESS_SPACE_CONVERSIONS
  }
  color *= paperWhite;
}

// Fix up sharpening/blurring when done on HDR images in post processing. In SDR, the source color could only be between 0 and 1,
// so the halos (rings) that could result from rapidly changing colors were limited, but in HDR lights can go much brighter so the halos got noticeable with default settings.
// This should work with any "POST_PROCESS_SPACE_TYPE" setting.
float3 FixUpSharpeningOrBlurring(float3 postSharpeningColor, float3 preSharpeningColor)
{
#if ENABLE_SHARPENING
    // Either set it to 0.5, 0.75 or 1 to make results closer to SDR (this makes more sense when done in gamma space, but also works in linear space).
    // Lower values slightly diminish the effect of sharpening, but further avoid halos issues.
    static const float sharpeningMaxColorDifference = 0.5;
    postSharpeningColor.rgb = clamp(postSharpeningColor.rgb, preSharpeningColor - sharpeningMaxColorDifference, preSharpeningColor + sharpeningMaxColorDifference);
    
#if 0 // Not necessary until proven otherwise, the whole shader code base works in r g b individually so even if we had an invalid luminance, it'd be fine (it will likely be clipped on output anyway)
    postSharpeningColor.rgb = max(postSharpeningColor.rgb, min(preSharpeningColor.rgb, 0)); // Don't allow scRGB colors to go below the min we previously had
#endif
#endif // ENABLE_SHARPENING
  	return postSharpeningColor;
}

#endif  // SRC_COMMON_HLSL