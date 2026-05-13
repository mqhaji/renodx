#include "../shared.h"

namespace Uncharted2 {

float Derivative(
    float x,
    float a, float b, float c,
    float d, float e, float f) {
  float num = -a * b * (c - 1.0) * x * x
              + 2.0 * a * d * (f - e) * x
              + b * d * (c * f - e);

  float den = x * (a * x + b) + d * f;
  den = den * den;

  return num / den;
}

float FindSecondDerivativeRoot(float a, float b, float c, float d, float e, float f) {
  // Coefficients of the numerator of f''(x):
  // num(x) = A3 x^3 + A2 x^2 + A1 x + A0

  float A3 = a * a * b * (c - 1.0f);
  float A2 = 3.0f * a * a * d * (e - f);
  float A1 = 3.0f * a * b * d * (e - c * f);
  float A0 = a * d * d * (f * f - e * f) + b * b * d * (e - c * f);

  // If A3 = 0, curve is degenerate → no inflection
  if (abs(A3) < 1e-12f)
    return 0.f;

  // Normalize to monic cubic: x^3 + ax^2 + bx + c = 0
  float invA3 = 1.0f / A3;
  float an = A2 * invA3;
  float bn = A1 * invA3;
  float cn = A0 * invA3;

  // Depressed cubic t^3 + p t + q = 0  with x = t - a/3
  float an_3 = an / 3.0f;
  float p = bn - an * an_3;
  float q = 2.0f * an * an * an / 27.0f - an * bn / 3.0f + cn;

  float half_q = 0.5f * q;
  float Delta = half_q * half_q + (p / 3.0f) * (p / 3.0f) * (p / 3.0f);

  // Real root output
  float t;

  if (Delta >= 0.f) {
    float sqrtD = sqrt(Delta);
    float u = (-half_q + sqrtD);
    float v = (-half_q - sqrtD);

    // Use signed cube root
    float u_c = renodx::math::SignPow(u, 1.0f / 3.0f);
    float v_c = renodx::math::SignPow(v, 1.0f / 3.0f);
    t = u_c + v_c;
  } else {
    // 3 real roots → trig branch
    float m = 2.0f * sqrt(-p / 3.0f);
    float angle = acos((-half_q) / sqrt(-(p * p * p) / 27.0f));
    t = m * cos(angle / 3.0f);
  }

  float x = t - an_3;

  // Only meaningful inflection is positive
  return max(x, 0.f);
}

namespace Config {

struct Uncharted2ExtendedConfig {
  float pivot_point;
  float white_precompute;
  float coeffs[6];  // A,B,C,D,E,F
};

Uncharted2ExtendedConfig CreateUncharted2ExtendedConfig(
    float pivot_point,
    float coeffs[6], float white_precompute) {
  Uncharted2ExtendedConfig cfg;
  cfg.pivot_point = pivot_point;
  cfg.white_precompute = white_precompute;
  cfg.coeffs = coeffs;

  return cfg;
}

Uncharted2ExtendedConfig CreateUncharted2ExtendedConfig(float coeffs[6], float white_precompute) {
  float pivot_point = FindSecondDerivativeRoot(coeffs[0], coeffs[1], coeffs[2], coeffs[3], coeffs[4], coeffs[5]);
  // pivot_point = (pivot_point + FindSecondDerivativeRoot(coeffs[0], coeffs[1], coeffs[2], coeffs[3], coeffs[4], coeffs[5])) / 2.f;

  return CreateUncharted2ExtendedConfig(pivot_point, coeffs, white_precompute);
}

}  // Config

#define APPLY_EXTENDED_GENERATOR(T)                                                            \
  T ApplyExtended(                                                                             \
      T x,                                                                                     \
      T base,                                                                                  \
      float pivot_point,                                                                       \
      float white_precompute,                                                                  \
      float A, float B, float C, float D, float E, float F) {                                  \
    float pivot_x = pivot_point;                                                               \
    float pivot_y = renodx::tonemap::ApplyCurve(pivot_x, A, B, C, D, E, F) * white_precompute; \
    float slope = Derivative(pivot_x, A, B, C, D, E, F) * white_precompute;                    \
    T offset = pivot_y - slope * pivot_x;                                                      \
                                                                                               \
    T extended = slope * x + offset; /* match slope */                                         \
                                                                                               \
    return lerp(base, extended, step(pivot_x, x));                                             \
  }

APPLY_EXTENDED_GENERATOR(float)
APPLY_EXTENDED_GENERATOR(float3)
#undef APPLY_EXTENDED_GENERATOR

float ApplyExtended(float x, float base, Config::Uncharted2ExtendedConfig uc2_config) {
  return ApplyExtended(
      x, base, uc2_config.pivot_point, uc2_config.white_precompute,
      uc2_config.coeffs[0], uc2_config.coeffs[1], uc2_config.coeffs[2],
      uc2_config.coeffs[3], uc2_config.coeffs[4], uc2_config.coeffs[5]);
}

float3 ApplyExtended(float3 x, float3 base, Config::Uncharted2ExtendedConfig uc2_config) {
  return ApplyExtended(
      x, base, uc2_config.pivot_point, uc2_config.white_precompute,
      uc2_config.coeffs[0], uc2_config.coeffs[1], uc2_config.coeffs[2],
      uc2_config.coeffs[3], uc2_config.coeffs[4], uc2_config.coeffs[5]);
}

float ApplyExtended(float x, Config::Uncharted2ExtendedConfig uc2_config) {
  float base =
      renodx::tonemap::ApplyCurve(x, uc2_config.coeffs[0], uc2_config.coeffs[1], uc2_config.coeffs[2],
                                  uc2_config.coeffs[3], uc2_config.coeffs[4], uc2_config.coeffs[5])
      * uc2_config.white_precompute;

  return ApplyExtended(x, base, uc2_config);
}

float3 ApplyExtended(float3 x, Config::Uncharted2ExtendedConfig uc2_config) {
  float3 base =
      renodx::tonemap::ApplyCurve(x, uc2_config.coeffs[0], uc2_config.coeffs[1], uc2_config.coeffs[2],
                                  uc2_config.coeffs[3], uc2_config.coeffs[4], uc2_config.coeffs[5])
      * uc2_config.white_precompute;
  return ApplyExtended(x, base, uc2_config);
}
}  // Uncharted2
