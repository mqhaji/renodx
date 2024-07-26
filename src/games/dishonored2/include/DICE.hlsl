#include "./Common.hlsl"

// Do the DICE compression in PQ (either on luminance or each color channel), this produces a curve that is closer to our "perception" and leaves more detail highlights without overly compressing them
#define DICE_PQ 1
// This might look more like classic SDR tonemappers and is closer to how modern TVs and Monitors play back colors (usually they clip each individual channel to the peak brightness value, though in their native panel color space, or current SDR/HDR mode color space)
#define DICE_PQ_BY_CHANNEL 1
// Modern HDR displays clip individual rgb channels beyond their "white" peak brightness,
// like, if the peak brightness is 700 nits, any r g b color beyond a value of 700/80 will be clipped (not acknowledged, it won't make a difference).
// Tonemapping by luminance, is generally more perception accurate but can then generate rgb colors "out of range". This setting fixes them up,
// though it's optional as it's working based on assumptions on how current displays work, which might not be true anymore in the future.
#define DICE_PQ_BY_CHANNEL_CORRECT_CHANNELS_BEYOND_PEAK_WHITE 1

// Aplies exponential ("Photographic") luminance/luma compression.
// The pow can modulate the curve without changing the values around the edges.
float rangeCompress(float X, float Max = FLT_MAX)
{
  // Branches are for static parameters optimizations
  if (Max == FLT_MAX) {
    // This does e^X. We expect X to be between 0 and 1.
    return 1.f - exp(-X);
  }
  const float lostRange = exp(-Max);
  const float restoreRangeScale = 1.f / (1.f - lostRange);
  return (1.f - exp(-X)) * restoreRangeScale;
}

// Refurbished DICE HDR tonemapper (per channel or luminance).
// Expects "InValue" to be >= "ShoulderStart" and "OutMaxValue" to be > "ShoulderStart".
float luminanceCompress(
  float InValue,
  float OutMaxValue,
  float ShoulderStart = 0.f,
  bool considerMaxValue = false,
  float InMaxValue = FLT_MAX)
{
  const float compressableValue = InValue - ShoulderStart;
  const float compressableRange = InMaxValue - ShoulderStart;
  const float compressedRange = OutMaxValue - ShoulderStart;
  const float possibleOutValue = ShoulderStart + compressedRange * rangeCompress(compressableValue / compressedRange, considerMaxValue ? (compressableRange / compressedRange) : FLT_MAX);
#if 1
  return possibleOutValue;
#else
  return (InValue <= ShoulderStart) ? InValue : possibleOutValue;
#endif
}

// Tonemapper inspired from DICE. Can work by luminance to maintain hue.
// Takes scRGB colors with a white point of 80 nits.
// "ShoulderStart" should be between 0 and 1. Determines where the highlights curve (shoulder) starts. Zero is a simple and good looking default for "DICE_PQ" off.
float3 DICETonemap(
  float3 Color,
  float PeakWhite,
#if DICE_PQ // Values between 0.25 and 0.5 are good with PQ DICE
  float ShoulderStart = 1.f / 4.f)
#else // With linear DICE this barely makes a difference, but 0.5 would be fine either way (we blend in the SDR tonemapper below mid gray anyway)
  float ShoulderStart = 0.f)
#endif // DICE_PQ
{
  const float sourceLuminance = GetLuminance(Color);

#if DICE_PQ
  static const float HDR10_MaxWhite = HDR10_MaxWhiteNits / sRGB_WhiteLevelNits;

  const float shoulderStartPQ = Linear_to_PQ((ShoulderStart * PeakWhite) / HDR10_MaxWhite).x;
#if !DICE_PQ_BY_CHANNEL // By luminance
  const float sourceLuminanceNormalized = sourceLuminance / HDR10_MaxWhite;
  const float sourceLuminancePQ = Linear_to_PQ(sourceLuminanceNormalized).x;

  if (sourceLuminancePQ > shoulderStartPQ) // Luminance below the shoulder (or below zero) don't need to be adjusted
  {
    const float peakWhitePQ = Linear_to_PQ(PeakWhite / HDR10_MaxWhite).x;

    const float compressedLuminancePQ = luminanceCompress(sourceLuminancePQ, peakWhitePQ, shoulderStartPQ);
    const float compressedLuminanceNormalized = PQ_to_Linear(compressedLuminancePQ).x;
    Color *= compressedLuminanceNormalized / sourceLuminanceNormalized;

#if DICE_PQ_BY_CHANNEL_CORRECT_CHANNELS_BEYOND_PEAK_WHITE
    float3 Color_BT2020 = BT709_To_BT2020(Color);
    if (any(Color_BT2020 > PeakWhite)) // Optional "optimization" branch
    {
      // The sum of these needs to be <= 1, both within 0 and 1.
      static const float DesaturateAlphaMultiplier = 0.75;
      static const float BrightnessReductionMultiplier = 0.125; //TODOFT1: expose these settings to the user, and make all the DICE_PQ etc branches function paramaters

      float colorLuminance = GetLuminance(Color_BT2020);
      float colorLuminanceInExcess = colorLuminance - PeakWhite;
      float maxColorInExcess = max3(Color_BT2020) - PeakWhite; // This is guaranteed to be >= "colorLuminanceInExcess"
      float brightnessReduction = saturate(safeDivision(PeakWhite, max3(Color_BT2020), 1)); // Fall back to one in case of division by zero
      float desaturateAlpha = saturate(safeDivision(maxColorInExcess, maxColorInExcess - colorLuminanceInExcess, 0)); // Fall back to zero in case of division by zero
      Color_BT2020 = lerp(Color_BT2020, colorLuminance, desaturateAlpha * DesaturateAlphaMultiplier);
      Color_BT2020 = lerp(Color_BT2020, Color_BT2020 * brightnessReduction, BrightnessReductionMultiplier); // Also reduce the brightness to partially maintain the hue, at the cost of brightness
      Color = BT2020_To_BT709(Color_BT2020);
    }
#endif // DICE_PQ_BY_CHANNEL_CORRECT_CHANNELS_BEYOND_PEAK_WHITE
  }
#else // By channel
  const float peakWhitePQ = Linear_to_PQ(PeakWhite / HDR10_MaxWhite).x;

  Color = BT709_To_BT2020(Color); // Tonemap in BT.2020 to more closely match the primaries of modern displays
  const float3 sourceColorNormalized = Color / HDR10_MaxWhite;
  const float3 sourceColorPQ = Linear_to_PQ(sourceColorNormalized);

  for (uint i = 0; i < 3; i++) //TODOFT: optimize? will the shader compile already convert this to float3?
  {
    if (sourceColorPQ[i] > shoulderStartPQ) // Colors below the shoulder (or below zero) don't need to be adjusted
    {
      const float compressedColorPQ = luminanceCompress(sourceColorPQ[i], peakWhitePQ, shoulderStartPQ);
      const float compressedColorNormalized = PQ_to_Linear(compressedColorPQ).x;
      Color[i] *= compressedColorNormalized / sourceColorNormalized[i];
    }
  }
  Color = BT2020_To_BT709(Color);
#endif // DICE_PQ_BY_CHANNEL
#else // DICE_PQ
  if (sourceLuminance > ShoulderStart) // Luminance below the shoulder (or below zero) don't need to be adjusted
  {
    const float compressedLuminance = luminanceCompress(sourceLuminance, PeakWhite, PeakWhite * ShoulderStart);
    Color *= compressedLuminance / sourceLuminance;
  }
#endif // DICE_PQ

  return Color;
}
