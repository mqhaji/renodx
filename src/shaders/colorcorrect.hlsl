#ifndef SRC_SHADERS_COLOR_CORRECT_HLSL_
#define SRC_SHADERS_COLOR_CORRECT_HLSL_

#include "./color.hlsl"

namespace renodx {
namespace color {
namespace correct {

float Gamma(float x, bool pow_to_srgb = false) {
  if (pow_to_srgb) {
    return renodx::color::bt709::from::SRGB(pow(x, 1.f / 2.2f));
  }  // srgb2pow
  return pow(renodx::color::srgb::from::BT709(x), 2.2f);
}

float GammaSafe(float x, bool pow_to_srgb = false) {
  if (pow_to_srgb) {
    return sign(x) * renodx::color::bt709::from::SRGB(pow(abs(x), 1.f / 2.2f));
  }
  return sign(x) * pow(renodx::color::srgb::from::BT709(abs(x)), 2.2f);
}

float3 Gamma(float3 color, bool pow_to_srgb = false) {
  return float3(
      Gamma(color.r, pow_to_srgb),
      Gamma(color.g, pow_to_srgb),
      Gamma(color.b, pow_to_srgb));
}

float3 GammaSafe(float3 color, bool pow2srgb = false) {
  float3 signs = sign(color);
  color = abs(color);
  color = float3(
      Gamma(color.r, pow2srgb),
      Gamma(color.g, pow2srgb),
      Gamma(color.b, pow2srgb));
  color *= signs;
  return color;
}

float avg3(float3 color) {
  return (color.r + color.g + color.b) / 3.0;
}

// Applies the hue from correct_color to incorrect_color using two methods:
// - Positive correct_amount: Standard hue correction by interpolating in Oklab space.
// - Negative correct_amount: Hue correction based on clamped color to replicate SDR-like clipping behavior.
float3 Hue(float3 incorrect_color, float3 correct_color, float correct_amount = 1.f) {
    float3 color;

    // If no correction is needed, return the original color.
    if (correct_amount == 0) {
        return incorrect_color;

    // Standard hue correction path for positive correct_amount
    } else if (correct_amount > 0) {
        // Convert colors to OkLab space for hue manipulation
        float3 correct_lab = renodx::color::oklab::from::BT709(correct_color);

        // Interpolate hue components (a, b in OkLab) based on correct_amount
        float3 incorrect_lab = renodx::color::oklab::from::BT709(incorrect_color);
        float3 new_lab = incorrect_lab;
        new_lab.yz = lerp(incorrect_lab.yz, correct_lab.yz, abs(correct_amount));

        // Restore original chrominance from incorrect_color in OkLCh space
        float3 incorrect_lch = renodx::color::oklch::from::OkLab(incorrect_lab);
        float3 new_lch = renodx::color::oklch::from::OkLab(new_lab);
        new_lch[1] = incorrect_lch[1];

        // Convert back to linear BT.709 space
        color = renodx::color::bt709::from::OkLCh(new_lch);

    // Alternate path for negative correct_amount, simulating SDR-like clipping behavior
    } else {
        // Calculate average channel values of the original (unclamped) correct_color
        float avg_correct = avg3(correct_color);

        // Clamp correct_color and calculate average channel values for the clamped version
        float3 clamped_correct_color = saturate(correct_color);
        float avg_clamped_correct = avg3(clamped_correct_color);

        // Compute the hue clipping percentage based on the difference in averages
        float hue_clip_percentage = saturate((avg_correct - avg_clamped_correct) / max(avg_correct, .0000001f));  // Prevent division by zero

        // Interpolate hue components (a, b in OkLab) based on correct_amount using clamped color
        float3 correct_lab = renodx::color::oklab::from::BT709(clamped_correct_color);
        float3 incorrect_lab = renodx::color::oklab::from::BT709(incorrect_color);
        float3 new_lab = incorrect_lab;

        // Apply hue correction based on clipping percentage and interpolate based on correct_amount
        new_lab.yz = lerp(incorrect_lab.yz, correct_lab.yz, hue_clip_percentage);
        new_lab.yz = lerp(incorrect_lab.yz, new_lab.yz, abs(correct_amount));

        // Restore original chrominance from incorrect_color in OkLCh space
        float3 incorrect_lch = renodx::color::oklch::from::OkLab(incorrect_lab);
        float3 new_lch = renodx::color::oklch::from::OkLab(new_lab);
        new_lch[1] = incorrect_lch[1];

        // Convert back to linear BT.709 space
        color = renodx::color::bt709::from::OkLCh(new_lch);
    }

    color = mul(BT709_TO_AP1_MAT, color);  // Convert to AP1
    color = max(0, color);                 // Clamp to AP1
    color = mul(AP1_TO_BT709_MAT, color);  // Convert BT709
    return color;
}

}  // namespace correct
}  // namespace color
}  // namespace renodx
#endif  // SRC_SHADERS_COLOR_CORRECT_HLSL_
