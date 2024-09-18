#ifndef SRC_COMMON_HLSL
#define SRC_COMMON_HLSL

// Silence pow(x, n) issue complaining about negative pow possibly failing
#pragma warning( disable : 3571 )
// Silence for loop issue where multiple int i declarations overlap each other (because hlsl doesn't have stack/scope like c++ thus variables don't pop after their scope dies)
#pragma warning( disable : 3078 )

// These should only ever be included through "Common.hlsl" and never individually
#include "./Math.hlsl"
#include "./Color.hlsl"

/////////////////////////////////////////
// Prey LUMA settings
/////////////////////////////////////////

#ifndef ENABLE_LINEAR_SPACE_POST_PROCESS
#define ENABLE_LINEAR_SPACE_POST_PROCESS 1
#endif
// The LUMA mod changed LUTs textures from UNORM 8 bit to FP 16 bit, so their sRGB (scRGB) values can theoretically go negative to conserve HDR colors
#define ENABLE_HDR_COLOR_GRADING_LUT 1
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
#ifndef ENABLE_LUT_EXTRAPOLATION
#define ENABLE_LUT_EXTRAPOLATION 1
#endif
#ifndef LUT_EXTRAPOLATION_QUALITY
//TODOFT: add more qualities?
#define LUT_EXTRAPOLATION_QUALITY 0
#endif
// It's better to leave the classic LUT interpolation (bilinear/trilinear), LUTs in Prey are very close to being neutral so tetrahedral interpolation just shifts their colors without gaining much
#define ENABLE_LUT_TETRAHEDRAL_INTERPOLATION 0
// 0 Vanilla SDR, 1 Luma HDR (Vanilla+ Hable/DICE mix), 2 Untonemapped
#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif
#ifndef DELAY_HDR_TONEMAP
#define DELAY_HDR_TONEMAP 1
#endif
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

//TODOFT2: fix "SpotTexArray"?
//TODOFT2: fix GTAO
//TODOFT2: add highlights saturation slider (desaturate is realistic, saturate is cooler)? Or try new RenoDX tonemapper
//TODOFT1: keep LUT in gamma space for performance?
//TODOFT: add "DEVELOPMENT" macro to skip defining all stuff???
//TODOFT: remove CryEngine/Arkane references from source

/////////////////////////////////////////
// Rendering features toggles
/////////////////////////////////////////

//TODOFT: at the moment disabling motion bloom increases the quality of rendering as bloom is sourced from a R11G11B10F color texture and fully replaces the color buffer (even on pixel with no bloom)
#define ENABLE_MOTION_BLUR 1
#define ENABLE_BLOOM 1
#define ENABLE_SSAO 1
// Needs to be enabled from SSAO to look good
#define ENABLE_SSAO_DENOISE 1
// Requires TAA enabled to not look terrible
#define ENABLE_SSAO_TEMPORAL 0
#define ENABLE_TAA_DEJITTER 1
// Disables all kinds of AA (SMAA, FXAA, TAA, ...)
#define ENABLE_AA 1
// Optional SMAA pass being run before TAA
#define ENABLE_SMAA 0
// Optional TAA pass being run after the optional SMAA pass
#define ENABLE_TAA 1
#ifndef ENABLE_COLOR_GRADING_LUT
#define ENABLE_COLOR_GRADING_LUT 1
#endif
#define ENABLE_SUNSHAFTS 1
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

/////////////////////////////////////////
// Debug toggles
/////////////////////////////////////////

// Test extra saturation to see if it passes through (HDR colors)
#define TEST_HIGH_SATURATION_GAMUT 0
#define TEST_TONEMAP_OUTPUT 0
// 0 None
// 1 Neutral LUT
// 2 Neutral LUT + bypass extrapolation
#ifndef FORCE_NEUTRAL_COLOR_GRADING_LUT_TYPE
#define FORCE_NEUTRAL_COLOR_GRADING_LUT_TYPE 0
#endif
#define DRAW_LUT 1
#define TEST_LUT_EXTRAPOLATION 0
// Pixel scale
//TODOFT: lower from 22 or 10 before submitting
#define DRAW_LUT_TEXTURE_SCALE 10u
#define TEST_TINT 0
// Tests some alpha blends stuff
#define TEST_UI 0
#define TEST_MOTION_BLUR 0
// 0 None
// 1 Additive Bloom
// 2 Native Bloom
#define TEST_BLOOM_TYPE 0
//TODOFT1: test it
#define TEST_LENS_OPTICS 0
#define TEST_DITHERING 0
#define TEST_SMAA_EDGES 0

#define LUT_SIZE 32u
#define LUT_MAX (LUT_SIZE - 1u)

//TODOFT: move params and set them used defined and set good defaults
static const float GamePaperWhiteNits = ITU_WhiteLevelNits;
static const float UIPaperWhiteNits = ITU_WhiteLevelNits;
static const float PeakWhiteNits = 1000.0f;

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

// Luma per pass or per frame data
cbuffer LumaData : register(b8)
{
  struct
  {
    // If true, DLSS SR or other upscalers have already run before the game's original upscaling pass,
    // and thus we need to work in full resolution space and not rendering resolution space.
    uint PostEarlyUpscaling;
    uint DummyPadding; // GPU has "32 32 32 32 | break" bits alignment on memory, so to not break the "float2" below, we need this (because we are using a unified struct).
    // Camera jitters in UV space (rendering resolution) (not in projection matrix space, so they don't need to be divided by the rendering resolution). You might need to multiply this by 0.5 and invert the horizontal axis before using it.
    float2 CameraJitters;
    // Previous frame's camera jitters in UV space (relative to its own resolution).
    float2 PreviousCameraJitters;
    float2 RenderResolutionScale;
    // This can be used instead of "CV_ScreenSize" in passes where "CV_ScreenSize" would have been
    // replaced with 1 because DLSS SR upscaled the image earlier in the rendering.
    float2 PreviousRenderResolutionScale;
    row_major float4x4 ViewProjectionMatrix;
    row_major float4x4 PreviousViewProjectionMatrix;
    // Same as the one on "PostAA" "AA" but fixed to include jitters as well
    row_major float4x4 ReprojectionMatrix;
  } LumaData : packoffset(c0);
}

// AdvancedAutoHDR pass to generate some HDR brightess out of an SDR signal.
// This is hue conserving and only really affects highlights.
// "SDRColor" is meant to be in "SDR range", as in, a value of 1 matching SDR white (something between 80, 100, 203, 300 nits, or whatever else)
// https://github.com/Filoppi/PumboAutoHDR
float3 PumboAutoHDR(float3 SDRColor, float _PeakWhiteNits, float _PaperWhiteNits, float ShoulderPow = 2.75)
{
	const float SDRRatio = max(GetLuminance(SDRColor), 0.f);
	// Limit AutoHDR brightness, it won't look good beyond a certain level.
	// The paper white multiplier is applied later so we account for that.
	const float AutoHDRMaxWhite = min(_PeakWhiteNits, PeakWhiteNits) / _PaperWhiteNits;
	const float AutoHDRShoulderRatio = 1.f - max(1.f - SDRRatio, 0.f);
	const float AutoHDRExtraRatio = pow(AutoHDRShoulderRatio, ShoulderPow) * (AutoHDRMaxWhite - 1.f);
	const float AutoHDRTotalRatio = SDRRatio + AutoHDRExtraRatio;
	return SDRColor * safeDivision(AutoHDRTotalRatio, SDRRatio, 1);
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

// Partially mirrors "DrawLUTTexture()".
// PassType:
//  0 Generic
//  1 TAA
bool ShouldSkipPostProcess(float2 PixelPosition, uint PassType = 0)
{
#if TEST_MOTION_BLUR_TYPE || TEST_SMAA_EDGES
  return true;
#endif // TEST_MOTION_BLUR_TYPE || TEST_SMAA_EDGES
#if TEST_TAA_TYPE
  if (PassType != 1) { return true; }
#endif // TEST_TAA_TYPE
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