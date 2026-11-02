// SPDX-License-Identifier: MIT
// Copyright and license notices: README.md#credits-and-license.

#ifndef MGSV_SPECULAR_OCCLUSION_HLSLI_
#define MGSV_SPECULAR_OCCLUSION_HLSLI_

#include "directional_visibility.hlsli"

static const float GTSO_PI = 3.14159265358979323846;

// Angle in [0, pi] for nonnegative y. Half-angle reduction bounds the Taylor
// argument by sqrt(2)-1 for a degree-fifteen polynomial.
float PositiveArcAngle(float y, float x) {
  const float absolute_x = abs(x);
  const float ratio = min(y, absolute_x) / max(max(y, absolute_x), 1e-20);
  const float reduced = ratio / (1.0 + sqrt(1.0 + ratio * ratio));
  const float square = reduced * reduced;
  const float polynomial = 1.0 + square * (-1.0 / 3.0 + square * (1.0 / 5.0 + square * (-1.0 / 7.0 + square * (1.0 / 9.0 + square * (-1.0 / 11.0 + square * (1.0 / 13.0 - square / 15.0))))));
  const float angle = 2.0 * reduced * polynomial;
  const float quadrant = (y > absolute_x ? GTSO_PI * 0.5 - angle : angle);
  return x < 0.0 ? GTSO_PI - quadrant : quadrant;
}

// Solid angle shared by two spherical caps. These cone apertures are at most
// pi/2; containment and separation avoid the singular endpoint expressions.
float SphericalCapIntersection(float radius_a, float radius_b, float separation) {
  const float2 cosine = cos(float2(radius_a, radius_b));
  const float2 half_sine = sin(float2(radius_a, radius_b) * 0.5);
  const float2 cap_height = 2.0 * half_sine * half_sine;
  const float smaller_area = 2.0 * GTSO_PI * min(cap_height.x, cap_height.y);
  if (separation <= abs(radius_a - radius_b)) return smaller_area;
  if (separation >= radius_a + radius_b) return 0.0;
  if (min(radius_a, radius_b) >= GTSO_PI * 0.5 - 1e-6) return max(2.0 * (GTSO_PI - separation), 0.0);

  const float distance_height = 2.0 * sin(separation * 0.5) * sin(separation * 0.5);
  const float cosine_difference = 2.0 * sin((radius_a + radius_b) * 0.5) * sin((radius_a - radius_b) * 0.5);
  const float4 quarter_sides = 0.25 * float4(radius_a + radius_b + separation, -radius_a + radius_b + separation, radius_a - radius_b + separation, radius_a + radius_b - separation);
  // The spherical triangle determinant factors into four nonnegative sines.
  // The angle calculation also retains nearly tangent narrow lenses.
  const float4 side_sines = sin(2.0 * quarter_sides);
  const float determinant = 2.0 * sqrt(max(side_sines.x * side_sines.y * side_sines.z * side_sines.w, 0.0));
  const float angle_a = PositiveArcAngle(determinant, cosine_difference + cosine.x * distance_height);
  const float angle_b = PositiveArcAngle(determinant, -cosine_difference + cosine.y * distance_height);
  // The determinant also gives the spherical excess. A positive half-angle
  // denominator avoids cancellation at the wide-cone limit and four tan calls.
  const float half_cosine = cos(separation * 0.5);
  const float excess = 2.0 * PositiveArcAngle(determinant, cosine.x + cosine.y + 2.0 * half_cosine * half_cosine);
  return clamp(2.0 * (angle_a * cap_height.x + angle_b * cap_height.y - excess), 0.0, smaller_area);
}

// Jimenez et al., Section 6 and Appendix B. Return visibility and confidence.
// Native roughness is interpreted as perceptual roughness for this model:
// GGX alpha = roughness^2. This is not a fitted model of MGSV's native BRDF.
float2 SpecularVisibility(float4 directional_visibility, float3 normal, float3 view, float roughness) {
  const float normal_squared = dot(normal, normal);
  const float view_squared = dot(view, view);
  if (!ValidDirectionalVisibility(directional_visibility) || !all(isfinite(normal)) || !all(isfinite(view))
      || !isfinite(roughness) || roughness < 0.0 || roughness > 1.0
      || normal_squared <= 1e-10 || view_squared <= 1e-10) return float2(1.0, 0.0);

  normal *= rsqrt(normal_squared);
  view *= rsqrt(view_squared);
  const float3 bent_normal = BentNormalDirection(directional_visibility, normal);
  float confidence = smoothstep(0.0, 0.2, dot(normal, view)) * smoothstep(0.0, 0.2, dot(normal, bent_normal));
  if (directional_visibility.a > 1e-6 && dot(directional_visibility.rgb, directional_visibility.rgb) <= 1e-10) {
    confidence = 0.0;
  }
  if (confidence <= 0.0) return float2(1.0, 0.0);
  const float visibility = directional_visibility.a;
  if (visibility <= 0.0) return float2(0.0, confidence);
  if (visibility >= 1.0) return float2(1.0, confidence);

  const float3 reflection = reflect(-view, normal);
  const float visibility_radius = acos(sqrt(1.0 - visibility));
  const float alpha = roughness * roughness;
  const float specular_radius = max(acos(exp2(-3.321928094887362 * alpha * alpha)), 0.05);
  const float visible_area = SphericalCapIntersection(visibility_radius, specular_radius,
                                                      acos(clamp(dot(bent_normal, reflection), -1.0, 1.0)));
  // Normalize by the reflection cone above the actual surface, preserving
  // fully open grazing reflections rather than darkening them a second time.
  const float open_area = SphericalCapIntersection(GTSO_PI * 0.5, specular_radius,
                                                   acos(clamp(dot(normal, reflection), -1.0, 1.0)));
  return float2(saturate(visible_area / max(open_area, 1e-6)), confidence);
}

#endif  // MGSV_SPECULAR_OCCLUSION_HLSLI_
