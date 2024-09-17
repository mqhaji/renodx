/////////////////////////////////////////
// Prey LUMA advanced settings
/////////////////////////////////////////

// Whether we store the post process buffers in linear space scRGB or gamma space (2.2 or sRGB) (like in the vanilla game, though now we use FP16 textures as opposed to UNORM8 ones).
// Note that converting between linear and gamma space back and forth results in quality loss, especially over very high and very low values, so this is best left on.
// 0 Gamma space (vanilla like)
// 1 Linear space
// 2 Linear space until UI (more specifically, until PostAAComposites, included), then gamma space (has the some of the advantage of both)
#ifndef POST_PROCESS_SPACE_TYPE
#define POST_PROCESS_SPACE_TYPE 2
#endif
// Higher qualy gamma<->linear conversions, it avoids the error generated from the conversion by restoring the change on the original color in an additive way.
// This has a relatively high performance cost for the visual gains it returns.
#define HIGH_QUALITY_POST_PROCESS_SPACE_CONVERSIONS 1
// The LUMA mod changed LUTs textures from UNORM 8 bit to FP 16 bit, so their sRGB (scRGB) values can theoretically go negative to conserve HDR colors
#define ENABLE_HDR_COLOR_GRADING_LUT 1
// We changed LUT format to R16G16B16A16F so in their mixing shaders, we store them in linear space, for higher output quality and to keep output values beyond 1 (their input coordinates are still in gamma sRGB (and then need 2.2 gamma correction)).
// This has a relatively small performance cost.
#ifndef ENABLE_LINEAR_COLOR_GRADING_LUT
#define ENABLE_LINEAR_COLOR_GRADING_LUT 1
#endif
// As many games, Prey rendered and tonemapped in linear space, though applied the sRGB gamma transfer function to apply the color grading LUT.
// Almost all TVs follow gamma 2.2 and most monitors also do, so to mantain the SDR look (and near black level), we need to linearize with gamma 2.2 and not sRGB.
// Disabling this will linearize with gamma sRGB, ignoring that the game would have been developed on (and for) gamma 2.2 displays.
// Note that if "POST_PROCESS_SPACE_TYPE" is 0, this simply determines how gamma is linearized for intermediary operations, while everything stays in sRGB gamma when stored in textures,
// so this would also go to determine how the final shader should linearize (if true, from 2.2, if false, from sRGB, thus causing raised blacks).
#ifndef ENABLE_GAMMA_CORRECTION
#define ENABLE_GAMMA_CORRECTION 0
#endif
// Necessary for HDR to work correctly
#ifndef ENABLE_LUT_EXTRAPOLATION
#define ENABLE_LUT_EXTRAPOLATION 1
#endif
// See "LUTExtrapolationSettings::extrapolationQuality"
#ifndef LUT_EXTRAPOLATION_QUALITY
#define LUT_EXTRAPOLATION_QUALITY 1
#endif
// It's better to leave the classic LUT interpolation (bilinear/trilinear),
// LUTs in Prey are very close to being neutral so tetrahedral interpolation just shifts their colors without gaining much, possibly actually losing quality.
// This is even less necessary while using "ENABLE_LINEAR_COLOR_GRADING_LUT".
#define ENABLE_LUT_TETRAHEDRAL_INTERPOLATION 0
// 0 Vanilla SDR (Hable), 1 Luma HDR (Vanilla+ Hable/DICE mix), 2 Untonemapped
#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE 1
#endif
//TODOFT: define DELAYED_HDR_TONEMAP as a consequence with the other conditions we need for it
// Better kept to true for sun shafts and lens optics and other post process effects
#if !defined(DELAY_HDR_TONEMAP) || (TONEMAP_TYPE != 1)
#undef DELAY_HDR_TONEMAP
#define DELAY_HDR_TONEMAP (TONEMAP_TYPE == 1 && 1)
#endif
// Sun shafts were drawn after tonemapping in the Vanilla game, thus they were completely SDR, Luma has implemented an HDR version of them which tries to retain the artistic direction.
#define PREEMPT_SUNSHAFTS (!DELAY_HDR_TONEMAP || 1)
// 0 Vanilla
// 1 Medium (Vanilla+)
// 2 High
// 3 Extreme (worst performance)
#define SUNSHAFTS_QUALITY 2
// 0 Raw Vanilla: raw sun shafts values, theoretically close to vanilla but in reality not always, it might not look good
// 1 Vanilla+: similar to vanilla but HDR and tweaked to be even closer (it's more "realistic" than SDR vanilla)
// 2 LUMA HDR: a bit dimmer but more realistic, so it works best with "ENABLE_LENS_OPTICS_HDR", which compensates for the lower brightness/area
#ifndef SUNSHAFTS_LOOK_TYPE
#define SUNSHAFTS_LOOK_TYPE 2
#endif
// Lens optics were clipped to 1 due to being rendered before tonemapping. As long as "DELAY_HDR_TONEMAP" is true, now these will also be tonemapped instead of clipped.
#ifndef ENABLE_LENS_OPTICS_HDR
#define ENABLE_LENS_OPTICS_HDR 1
#endif
#ifndef AUTO_HDR_VIDEOS
#define AUTO_HDR_VIDEOS 1
#endif
#define DELAY_DITHERING 1
// Do it higher than 8 bit for HDR
#define DITHERING_BIT_DEPTH 9u
// 0 SSDO (Vanilla, CryEngine)
// 1 GTAO (Luma)
#ifndef SSAO_TYPE
#define SSAO_TYPE 0
#endif
// 0 Vanilla
// 1 High (best balance for 2024 GPUs)
// 2 Extreme (bad performance)
#define SSAO_QUALITY 0
// Requires TAA enabled to not look terrible, but it still looks bad anyway
#define ENABLE_SSAO_TEMPORAL 0
// 0 Vanilla
// 1 High
#define BLOOM_QUALITY 1
// Disabled as we are now in HDR (10 or 16 bits)
#define ENABLE_DITHERING 0
// Disables development features if off
#ifndef DEVELOPMENT
#define DEVELOPMENT 0
#endif

//TODOFT2: fix GTAO
//TODOFT0: fix all shader compilation warnings
//TODOFT3: try to boost the chrominance on highlights

/////////////////////////////////////////
// Rendering features toggles (development)
/////////////////////////////////////////

#define ENABLE_MOTION_BLUR (!DEVELOPMENT || 1)
#define ENABLE_BLOOM (!DEVELOPMENT || 1)
#define ENABLE_SSAO (!DEVELOPMENT || 1)
// Needs to be enabled from SSAO to look good
#define ENABLE_SSAO_DENOISE (!DEVELOPMENT || 1)
#define ENABLE_TAA_DEJITTER (!DEVELOPMENT || 1)
// Disables all kinds of AA (SMAA, FXAA, TAA, ...) (disabling "ENABLE_SHARPENING" is also suggested if disabling AA)
#define ENABLE_AA (!DEVELOPMENT || 1)
// Optional SMAA pass being run before TAA
#define ENABLE_SMAA (!DEVELOPMENT || 1)
// Optional TAA pass being run after the optional SMAA pass
#define ENABLE_TAA (!DEVELOPMENT || 1)
#if !defined(ENABLE_COLOR_GRADING_LUT) || !DEVELOPMENT
#undef ENABLE_COLOR_GRADING_LUT
#define ENABLE_COLOR_GRADING_LUT (!DEVELOPMENT || 1)
#endif
// Note that this only disables the tonemap step sun shafts, not the secondary ones from lens optics
#define ENABLE_SUNSHAFTS (!DEVELOPMENT || 1)
#define ENABLE_ARK_CUSTOM_POST_PROCESS (!DEVELOPMENT || 1)
#define ENABLE_LENS_OPTICS (!DEVELOPMENT || 1)
// Disable this for a softer image
#define ENABLE_SHARPENING (!DEVELOPMENT || 1)
#define ENABLE_CHROMATIC_ABERRATION (!DEVELOPMENT || 1)
#define ENABLE_VIGNETTE (!DEVELOPMENT || 1)
// This is used for gameplay effects too, so it's best not disabled
#define ENABLE_FILM_GRAIN (!DEVELOPMENT || 1)
// This might also disable decals interfaces (like computer screens) in the 3D scene
#define ENABLE_UI (!DEVELOPMENT || 1)

/////////////////////////////////////////
// Debug toggles
/////////////////////////////////////////

// Test extra saturation to see if it passes through (HDR colors)
#define TEST_HIGH_SATURATION_GAMUT (DEVELOPMENT && 0)
#define TEST_TONEMAP_OUTPUT (DEVELOPMENT && 0)
// 0 None
// 1 Neutral LUT
// 2 Neutral LUT + bypass extrapolation
#if !defined(FORCE_NEUTRAL_COLOR_GRADING_LUT_TYPE) || !DEVELOPMENT
#undef FORCE_NEUTRAL_COLOR_GRADING_LUT_TYPE
#define FORCE_NEUTRAL_COLOR_GRADING_LUT_TYPE (DEVELOPMENT && 0)
#endif
#if !defined(DRAW_LUT) || !DEVELOPMENT
#undef DRAW_LUT
#define DRAW_LUT (DEVELOPMENT && 0)
#endif
// Debug LUT Pixel scale (this is rounded to the closest integer value for the size of the LUT)
// 10u is a good value for 2560 horizontal res. 22 for 5120 horizontal res or more.
#define DRAW_LUT_TEXTURE_SCALE 10u
#define TEST_LUT_EXTRAPOLATION (DEVELOPMENT && 0)
//TODOFT0: disable all
#define TEST_LUT (DEVELOPMENT && 1)
#define TEST_TINT (DEVELOPMENT && 1)
// Tests some alpha blends stuff
#define TEST_UI (DEVELOPMENT && 0)
#define TEST_MOTION_BLUR (DEVELOPMENT && 0)
// 0 None
// 1 Additive Bloom
// 2 Native Bloom
#define TEST_BLOOM_TYPE (DEVELOPMENT && 0)
#define TEST_SUN_SHAFTS (DEVELOPMENT && 0)
// 0 None
// 1 Show fixed color
// 2 Show only lens optics
#define TEST_LENS_OPTICS_TYPE (DEVELOPMENT && 0)
#define TEST_DITHERING (DEVELOPMENT && 0)
#define TEST_SMAA_EDGES (DEVELOPMENT && 0)
#define TEST_RESOLUTION_SCALING (DEVELOPMENT && 0)
#define TEST_EXPOSURE (DEVELOPMENT && 0)

/////////////////////////////////////////
// Prey LUMA user settings
/////////////////////////////////////////

//TODOFT0: expose and rename vars, and rename to "RenoDX" cbuffer? Or move it, and put them under "DEVELOPMENT"
// Registers 2, 4, 7, 8, 9, 10, 11 and 12 are 100% safe to be used for any post processing or late rendering passes.
// Register 2 is never used in the whole Prey code. Register 4, 7 and 8 are also seemengly never actively used by Prey.
// Register 3 seems to be used during post processing so it might not be safe.
// CryEngine pushes the registers that are used by each shader again for every draw, so it's generally safe to overridden them anyway (they are all reset between frames).
cbuffer LumaSettings : register(b2)
{
  struct
  {
    uint ForceSDR; //TODOFT0: Also properly add SDR support by making pw 1 and outputting sRGB (if ENABLE_GAMMA_CORRECTION is on, otherwise it's already correct)
    float PeakWhiteNits;
    float GamePaperWhiteNits;
    float UIPaperWhiteNits;
#if DEVELOPMENT || 1
    float DevSetting01;
    float DevSetting02;
    float DevSetting03;
    float DevSetting04;
    float DevSetting05;
    float DevSetting06;
#endif
  } LumaSettings : packoffset(c0);
}

#ifdef HDR_TONEMAP_PAPER_WHITE_BRIGHTNESS
static const float GamePaperWhiteNits = HDR_TONEMAP_PAPER_WHITE_BRIGHTNESS;
static const float UIPaperWhiteNits = HDR_TONEMAP_PAPER_WHITE_BRIGHTNESS;
#else // HDR_TONEMAP_PAPER_WHITE_BRIGHTNESS
static const float GamePaperWhiteNits = LumaSettings.ForceSDR ? ITU_WhiteLevelNits : (LumaSettings.GamePaperWhiteNits != 0 ? LumaSettings.GamePaperWhiteNits : ITU_WhiteLevelNits);
static const float UIPaperWhiteNits = LumaSettings.ForceSDR ? ITU_WhiteLevelNits : (LumaSettings.UIPaperWhiteNits != 0 ? LumaSettings.UIPaperWhiteNits : ITU_WhiteLevelNits);
#endif // HDR_TONEMAP_PAPER_WHITE_BRIGHTNESS
#ifdef HDR_TONEMAP_PEAK_BRIGHTNESS
static const float PeakWhiteNits = HDR_TONEMAP_PEAK_BRIGHTNESS;
#else // HDR_TONEMAP_PEAK_BRIGHTNESS
static const float PeakWhiteNits = LumaSettings.ForceSDR ? ITU_WhiteLevelNits : (LumaSettings.PeakWhiteNits != 0 ? LumaSettings.PeakWhiteNits : 1000.0);
#endif // HDR_TONEMAP_PEAK_BRIGHTNESS