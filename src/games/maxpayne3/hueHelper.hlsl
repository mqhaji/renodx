#include "./shared.h"

float max3(float3 color) {
    return max(color.r, max(color.g, color.b));
}

float min3(float3 color) {
    return min(color.r, min(color.g, color.b));
}

float sum_of_two_smallest(float3 color) {
    return color.r + color.g + color.b - max3(color);
}

// Applies the hue from correct_color to incorrect_color using two methods:
// - Positive correct_amount: Based on max channel of clamped color
// - Negative correct_amount: Based on avg channel of clamped color
float3 Hue(float3 incorrect_color, float3 correct_color, float correct_amount = 1.f) {
    float3 color;

    // If no correction is needed, return the original color
    if (correct_amount == 0) {
        return incorrect_color;

    // Standard hue correction path for positive correct_amount
    } else if (correct_amount > 0) {
        // Calculate the saturation of the original (unclamped) correct_color
        float saturation_correct = max3(correct_color) - min3(correct_color);

        // Clamp correct_color and calculate saturation for the clamped version
        float3 clamped_correct_color = saturate(correct_color);
        float saturation_clamped_correct = max3(clamped_correct_color) - min3(clamped_correct_color);

        // Compute the hue clipping percentage based on the difference in saturation
        float hue_clip_percentage = saturate((saturation_correct - saturation_clamped_correct) / max(saturation_correct, .0000001f));  // Prevent division by zero

        // Convert to OkLab color space for hue correction based on the clamped color
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

    // Alternate path for negative correct_amount, simulating SDR-like clipping behavior
    } else {
        // Calculate average channel values of the original (unclamped) correct_color
        float avg_correct = renodx::math::Average(correct_color);

        // Clamp correct_color and calculate average channel values for the clamped version
        float3 clamped_correct_color = saturate(correct_color);
        float avg_clamped_correct = renodx::math::Average(clamped_correct_color);

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
    return color;
}
