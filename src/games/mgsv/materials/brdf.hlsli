// SPDX-License-Identifier: MIT
// Copyright and license notices: ../lighting/indirect/README.md#credits-and-license.

// Specular BRDF shared by the reflection passes: material parameters from the
// native palette, GGX visible-normal sampling, and the energy-compensated response.
#ifndef MGSV_SPECULAR_BRDF_HLSLI_
#define MGSV_SPECULAR_BRDF_HLSLI_

static const float SPECULAR_BRDF_PI = 3.14159265358979323846;
static const uint SPECULAR_BRDF_SIZE = 128u;
static const uint SPECULAR_BRDF_SAMPLES = 2048u;
static const float SPECULAR_MIN_ALPHA = 1e-4;

struct SpecularMaterial {
  float3 f0;
  float roughness;  // Perceptual GGX roughness; alpha = roughness squared.
  float rate;       // Native reflection multiplier, separate from F0.
  bool valid;
};

// Read the integration table with linear-clamp filtering without depending on
// the caller's sampler bindings. All three energy channels use the same weights.
float4 SampleSpecularTable(Texture2D<float4> table, float2 uv) {
  uint width, height;
  table.GetDimensions(width, height);
  const float2 position = saturate(uv) * float2(width, height) - 0.5;
  const int2 base = int2(floor(position));
  const float2 fraction = frac(position);
  const int2 last = int2(width, height) - 1;
  const float4 a = table.Load(int3(clamp(base, 0, last), 0));
  const float4 b = table.Load(int3(clamp(base + int2(1, 0), 0, last), 0));
  const float4 c = table.Load(int3(clamp(base + int2(0, 1), 0, last), 0));
  const float4 d = table.Load(int3(clamp(base + 1, 0, last), 0));
  return lerp(lerp(a, b, fraction.x), lerp(c, d, fraction.x), fraction.y);
}

SpecularMaterial LoadSpecularMaterial(Texture2D<float4> palette, float4 packed) {
  SpecularMaterial result;
  result.f0 = 0.0;
  result.roughness = 0.0;
  result.rate = 0.0;
  result.valid = false;
  uint width, height;
  palette.GetDimensions(width, height);
  if (width != 256u || height != 2u || !all(isfinite(packed))) return result;
  const uint index = (uint)round(saturate(packed.b) * 255.0);
  const float4 tint = palette.Load(int3(index, 0, 0));
  const float4 parameters = palette.Load(int3(index, 1, 0));
  if (!all(isfinite(tint)) || !all(isfinite(parameters))) return result;
  // The native shader annotations name row 1 red as F0. The game's Fresnel table
  // is Schlick on an endpoint grid, and reading it as a texture adds 8-bit and
  // interpolation error. Use the authored reflectance directly instead.
  result.f0 = saturate(tint.rgb * parameters.r);
  // The captured native power table matches this exponent, including row 1
  // alpha (ANISO_ROUGHNESS), which changes lobe width and gain even though no
  // tangent direction is supplied. Normalize that power lobe, then match its
  // peak to GGX: alpha^2 = 2 / (power + 2). This keeps the native width but not
  // the native gain. The two distributions have different tails, so this is an
  // isotropic approximation, not recovered physical surface data. See README.md
  // for the evidence and the limits on special materials.
  const float native_roughness = max(saturate(packed.r), saturate(parameters.a));
  const float power = exp2(12.0 - 11.0 * native_roughness);
  result.roughness = pow(2.0 / (power + 2.0), 0.25);
  // Zero reflectance has no interface in this model. In particular, a black
  // native specular tint must not acquire Schlick's white grazing response.
  result.rate = any(result.f0 > 0.0) ? saturate(packed.g) : 0.0;
  result.valid = true;
  return result;
}

// Heitz (2018), visible GGX normals. The caller supplies an orthonormal tangent
// frame with N = (0,0,1). This sampler is also used by the integration reference.
float3 SampleSpecularVisibleNormal(float3 view, float alpha, float2 sample) {
  const float3 stretched = normalize(float3(alpha * view.xy, view.z));
  const float length_squared = dot(stretched.xy, stretched.xy);
  const float3 tangent = length_squared > 0.0 ? float3(-stretched.y, stretched.x, 0.0) * rsqrt(length_squared) : float3(1.0, 0.0, 0.0);
  const float3 bitangent = cross(stretched, tangent);
  const float radius = sqrt(sample.x);
  const float angle = 2.0 * SPECULAR_BRDF_PI * sample.y;
  float2 disk = radius * float2(cos(angle), sin(angle));
  const float visible = 0.5 * (1.0 + stretched.z);
  disk.y = lerp(sqrt(max(0.0, 1.0 - disk.x * disk.x)), disk.y, visible);
  const float3 normal = disk.x * tangent + disk.y * bitangent + sqrt(max(0.0, 1.0 - dot(disk, disk))) * stretched;
  return normalize(float3(alpha * normal.xy, max(normal.z, 0.0)));
}

// RG stores single-scattering Schlick coefficients; B stores the cosine-weighted
// average energy for F=1. Endpoints are samples at 0 and 1, not cell centers.
float3 GetSpecularBRDFIntegral(Texture2D<float4> table, float no_v, float roughness) {
  const float2 uv = (saturate(float2(no_v, roughness)) * float(SPECULAR_BRDF_SIZE - 1u) + 0.5) / float(SPECULAR_BRDF_SIZE);
  float3 integral = max(SampleSpecularTable(table, uv).xyz, 0.0);
  // FP16 rounding can put the unit-reflectance integral just above one.
  // Correct that integration error, without clamping stochastic ray weights.
  integral.xy /= max(integral.x + integral.y, 1.0);
  integral.z = saturate(integral.z);
  return integral;
}

struct SpecularResponse {
  float3 energy;
  float3 multiple_scattering;  // Color factor for the reciprocal compensation lobe.
};

// Kulla/Conty (2017): add a reciprocal lobe instead of scaling the single lobe
// by a view-dependent factor. Trace and resolve must use the same integral.
SpecularResponse GetSpecularResponse(float3 f0, float3 integral) {
  SpecularResponse response;
  const float3 average_fresnel = (1.0 + 20.0 * f0) / 21.0;
  response.multiple_scattering = average_fresnel * average_fresnel * integral.z
                                 / max(1.0 - average_fresnel * (1.0 - integral.z), 1e-6);
  response.energy = f0 * integral.x + integral.y
                    + response.multiple_scattering * (1.0 - integral.x - integral.y);
  return response;
}

#endif  // MGSV_SPECULAR_BRDF_HLSLI_
