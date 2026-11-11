// SPDX-License-Identifier: MIT
// Copyright and license notices: ../README.md#credits-and-license.

#ifndef MGSV_SPECULAR_OCCLUSION_HLSLI_
#define MGSV_SPECULAR_OCCLUSION_HLSLI_

static const float SPECULAR_PI = 3.14159265358979323846;

// Native environment lighting also shades normals facing away from the view.
// Choose the visible side for occlusion; reversing the whole normal preserves
// its reflection direction. Never use the diffuse normal fold for reflections.
float GetSpecularSurfaceSide(float3 normal, float3 view_direction) {
  return dot(normal, view_direction) < 0.0 ? -1.0 : 1.0;
}

// Approximate the directions a rough surface reflects with a cone: smooth
// surfaces use a narrow cone, rough surfaces a wide one. The roughness mapping
// follows GGX; it has not been fitted to MGSV's native reflection model.
float GetSpecularConeRadius(float roughness) {
  const float alpha = roughness * roughness;
  return max(acos(exp2(-3.321928094887362 * alpha * alpha)), 0.05);
}

// R is the sampled visible angular area; G is the same area without blockers.
// Dividing R by G gives visibility after filtering. B stores native roughness;
// A stores sampling confidence. Zero areas are valid; negative A means missing.
bool IsSpecularSampleValid(float4 value) {
  return all(isfinite(value)) && value.a >= 0.0 && value.a <= 1.0
         && all(value.xy >= 0.0) && value.x <= value.y
         && value.z >= 0.0 && value.z <= 1.0;
}

// cos(3r/4), cos(r/4), log2(1-cos(r)) for the reflection cone.
// Half-angle identities avoid reconstructing angles for each filter tap.
float3 GetSpecularLobeParameters(float roughness) {
  const float alpha = roughness * roughness;
  const float height = max(1.0 - exp2(-3.321928094887362 * alpha * alpha), 1.0 - cos(0.05));
  const float half_cosine = sqrt(1.0 - 0.5 * height);
  const float quarter_cosine = sqrt(0.5 + 0.5 * half_cosine);
  return float3((2.0 * half_cosine - 1.0) * quarter_cosine, quarter_cosine, log2(height));
}

// Similar depth and normals are not enough for reflections. Avoid mixing
// samples that reflect in different directions or have different roughness.
float GetSpecularLobeWeight(float3 reflection, float3 parameters, float3 sample_reflection, float sample_roughness) {
  const float3 sample_parameters = GetSpecularLobeParameters(sample_roughness);
  const float2 limits = max(parameters.xy, sample_parameters.xy);
  const float direction_weight = smoothstep(limits.x, limits.y, clamp(dot(reflection, sample_reflection), -1.0, 1.0));
  const float area_ratio = abs(parameters.z - sample_parameters.z);
  return direction_weight * (1.0 - smoothstep(0.321928094887362, 1.0, area_ratio));
}

// Search directions favor the surface normal. This weight compensates for
// that uneven sampling when adding specular angular areas.
float GetSpecularSliceWeight(float normal_length, float cosine_azimuth) {
  const float projection_length = saturate(normal_length);
  const float stretch = (projection_length + (projection_length - projection_length * projection_length) * 0.15) * (SPECULAR_PI * 0.5);
  const float complement = (1.0 - projection_length) * (1.0 - 0.15 * projection_length) * (SPECULAR_PI * 0.5);
  if (stretch < 1e-4) return SPECULAR_PI;
  if (stretch > SPECULAR_PI * 0.5 - 1e-5) return 2.0 / max(cosine_azimuth, 1e-7);
  const float sine_azimuth = sqrt(saturate(1.0 - cosine_azimuth * cosine_azimuth));
  const float difference = stretch * cosine_azimuth * cosine_azimuth / (1.0 + sine_azimuth);
  const float sinc = difference < 1e-4 ? 1.0 : sin(difference) / difference;
  // Complementary small angles avoid WARP's loss of relative accuracy near pi/2.
  const float sine_sum = (stretch < SPECULAR_PI * 0.25 ? sin(stretch * (1.0 + sine_azimuth))
                                                       : sin(2.0 * complement + difference));
  const float cosine = (stretch < SPECULAR_PI * 0.25 ? cos(stretch * sine_azimuth) : sin(complement + difference));
  const float numerator = sqrt(max(sine_sum * sinc * stretch / (1.0 + sine_azimuth), 0.0));
  return SPECULAR_PI * numerator / max(stretch * cosine, 1e-12);
}

// Estimate whether enough search lines cross the reflection cone. Low coverage
// reduces confidence so deferred composition can retain native occlusion.
float GetSpecularSliceSupport(float normal_length, float reflection_cosine, float radius, float slices) {
  const float transverse = sqrt(saturate(1.0 - reflection_cosine * reflection_cosine));
  const float sine_support = min(sin(radius) / max(transverse, 1e-7), 1.0);
  float stretch = saturate(normal_length);
  stretch = (stretch + (stretch - stretch * stretch) * 0.15) * (SPECULAR_PI * 0.5);
  const float probability = (stretch < 1e-4 ? asin(sine_support)
                                            : asin(clamp(sin(stretch * sine_support) / sin(stretch), -1.0, 1.0)))
                            * (2.0 / SPECULAR_PI);
  return smoothstep(0.5, 2.0, slices * probability);
}

float GetSpecularAngleMeasure(float angle) {
  const float half_sine = sin(angle * 0.5);
  return sign(angle) * 2.0 * half_sine * half_sine;
}

// Find the part of this search slice inside the reflection cone and above the
// selected surface side. Return its angular-area endpoints, inverse area, and area.
float4 GetSpecularSliceInterval(float2 normal_projection, float2 reflection_projection, float cosine_radius) {
  const float amplitude = length(reflection_projection);
  if (amplitude <= cosine_radius || normal_projection.x <= 0.0) return 0.0;
  const float normal_angle = atan2(normal_projection.y, normal_projection.x);
  float center = atan2(reflection_projection.y, reflection_projection.x);
  center += (2.0 * SPECULAR_PI) * floor((normal_angle - center + SPECULAR_PI) / (2.0 * SPECULAR_PI));
  const float width = acos(clamp(cosine_radius / amplitude, -1.0, 1.0));
  const float lo = max(center - width, normal_angle - SPECULAR_PI * 0.5);
  const float hi = min(center + width, normal_angle + SPECULAR_PI * 0.5);
  if (hi <= lo) return 0.0;
  const float lower = GetSpecularAngleMeasure(lo);
  const float upper = GetSpecularAngleMeasure(hi);
  const float measure = (lo < 0.0 && hi > 0.0 ? upper - lower
                                              : 2.0 * abs(sin((lo + hi) * 0.5)) * sin((hi - lo) * 0.5));
  // Below this size, FP32 slab cosines cannot resolve the interval reliably.
  if (!(measure > 1e-7) || !(upper > lower)) return 0.0;
  return float4(lower, upper, rcp(measure), measure);
}

// Angle in [0, pi/2] for nonnegative x and y. Half-angle reduction bounds the Taylor
// argument by sqrt(2)-1 for a degree-fifteen polynomial.
float GetPositiveArcAngle(float y, float x) {
  const float ratio = min(y, x) / max(max(y, x), 1e-20);
  const float reduced = ratio / (1.0 + sqrt(1.0 + ratio * ratio));
  const float square = reduced * reduced;
  const float polynomial = 1.0 + square * (-1.0 / 3.0 + square * (1.0 / 5.0 + square * (-1.0 / 7.0 + square * (1.0 / 9.0 + square * (-1.0 / 11.0 + square * (1.0 / 13.0 - square / 15.0))))));
  const float angle = 2.0 * reduced * polynomial;
  return y > x ? SPECULAR_PI * 0.5 - angle : angle;
}

// Solid angle of a reflection cone above the selected surface side. Its axis faces
// the hemisphere, so the area lies between half and all of the cone's area.
// Express the clipped result with cap height and positive angle differences
// to retain precision for narrow cones and nearly tangent intersections.
float GetSpecularOpenArea(float radius, float normal_reflection) {
  const float cosine = cos(radius);
  const float sine = sin(radius);
  const float half_sine = sin(radius * 0.5);
  const float height = 2.0 * half_sine * half_sine;
  const float alignment = saturate(normal_reflection);
  if (alignment >= sine) return 2.0 * SPECULAR_PI * height;

  const float determinant = sqrt(max((sine - alignment) * (sine + alignment), 0.0));
  const float angle = GetPositiveArcAngle(cosine * alignment, determinant);
  const float difference = GetPositiveArcAngle(height * alignment * determinant,
                                               determinant * determinant + cosine * alignment * alignment);
  return height * (SPECULAR_PI + 2.0 * angle) + 2.0 * difference;
}

#endif  // MGSV_SPECULAR_OCCLUSION_HLSLI_
