// SPDX-License-Identifier: MIT
// Copyright and license notices: ../lighting/indirect/README.md#credits-and-license.

#ifndef MGSV_SPECULAR_BRDF_HLSLI_
#define MGSV_SPECULAR_BRDF_HLSLI_

#include "../lighting/indirect/trace/specular_occlusion.hlsli"

static const float SPECULAR_BRDF_PI = 3.14159265358979323846;
static const uint SPECULAR_BRDF_SIZE = 128u;
static const uint SPECULAR_BRDF_SAMPLES = 2048u;

struct SpecularMaterial {
  float3 f0;
  float roughness;  // Perceptual GGX roughness; alpha = roughness squared.
  float rate;       // Native reflection multiplier, separate from F0.
  bool valid;
};

// Match a linear-clamp lookup without depending on the native pass's sampler
// slots. Both the native Fresnel table and our integration table use this rule.
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

SpecularMaterial LoadSpecularMaterial(Texture2D<float4> palette, Texture2D<float4> fresnel, float4 packed) {
  SpecularMaterial result;
  result.f0 = 0.0;
  result.roughness = 0.0;
  result.rate = 0.0;
  result.valid = false;
  uint width, height, fresnel_width, fresnel_height;
  palette.GetDimensions(width, height);
  fresnel.GetDimensions(fresnel_width, fresnel_height);
  if (width != 256u || height != 2u || fresnel_width < 2u || fresnel_height < 2u || !all(isfinite(packed))) return result;
  const uint index = (uint)round(saturate(packed.b) * 255.0);
  const float4 tint = palette.Load(int3(index, 0, 0));
  const float4 parameters = palette.Load(int3(index, 1, 0));
  if (!all(isfinite(tint)) || !all(isfinite(parameters))) return result;
  // The palette stores a Fresnel control. Evaluate the native table at normal
  // incidence instead of assuming that control already equals reflectance.
  // Normal incidence is the last column. Clamp sampling repeats that column
  // in X, so only the two neighboring rows are needed for the same lookup.
  const float fresnel_y = saturate(parameters.r) * float(fresnel_height) - 0.5;
  const int row = int(floor(fresnel_y));
  const int last_row = int(fresnel_height) - 1;
  const int column = int(fresnel_width) - 1;
  const float normal_fresnel = lerp(fresnel.Load(int3(column, clamp(row, 0, last_row), 0)).r,
                                    fresnel.Load(int3(column, clamp(row + 1, 0, last_row), 0)).r,
                                    frac(fresnel_y));
  if (!isfinite(normal_fresnel)) return result;
  result.f0 = saturate(tint.rgb * normal_fresnel);
  // Native roughness drives a normalized Blinn-Phong power. Match its NDF peak
  // to GGX: alpha^2 = 2 / (power + 2). This is a conversion, not an exact fit
  // of the native tails or a change to the game's existing material shaders.
  const float native_roughness = saturate(packed.r);
  const float power = exp2(12.0 - 11.0 * native_roughness);
  result.roughness = pow(2.0 / (power + 2.0), 0.25);
  result.rate = saturate(packed.g);
  result.valid = true;
  return result;
}

// Current-frame F0 and mapped roughness, written by the shared trace.
// This guide is never accumulated. Negative alpha keeps missing data explicit.
float4 LoadSpecularMaterialGuide(Texture2D<float4> guide, int2 pixel) {
  const float4 material = guide.Load(int3(pixel, 0));
  return all(isfinite(material)) && all(material >= 0.0) && all(material <= 1.0)
             ? material
             : float4(0.0, 0.0, 0.0, -1.0);
}

// GGX with height-correlated Smith visibility, multiplied by N.L. Keep the
// Schlick coefficients separate: F0 * A + B also describes colored Fresnel.
float2 GetSpecularBRDFWeights(float3 normal, float3 view, float3 light, float alpha_squared) {
  const float no_v = dot(normal, view);
  const float no_l = dot(normal, light);
  const float3 half_sum = view + light;
  if (no_v <= 0.0 || no_l <= 0.0 || dot(half_sum, half_sum) < 1e-12) return 0.0;
  const float3 half_vector = normalize(half_sum);
  const float no_h = saturate(dot(normal, half_vector));
  const float vo_h = saturate(dot(view, half_vector));
  const float denominator = no_h * no_h * (alpha_squared - 1.0) + 1.0;
  const float distribution = alpha_squared / (SPECULAR_BRDF_PI * denominator * denominator);
  const float view_root = sqrt(alpha_squared + (1.0 - alpha_squared) * no_v * no_v);
  const float light_root = sqrt(alpha_squared + (1.0 - alpha_squared) * no_l * no_l);
  const float base = distribution * (0.5 * no_l) / max(no_l * view_root + no_v * light_root, 1e-12);
  const float complement = 1.0 - vo_h;
  const float square = complement * complement;
  const float fresnel = square * square * complement;
  return base * float2(1.0 - fresnel, fresnel);
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

// The LUT stores full-lobe Schlick coefficients in RG and finite-cone
// coefficients in BA. Table endpoints are samples at 0 and 1, not cell centers.
float4 GetSpecularBRDFIntegral(Texture2D<float4> table, float no_v, float roughness) {
  const float2 uv = (saturate(float2(no_v, roughness)) * float(SPECULAR_BRDF_SIZE - 1u) + 0.5) / float(SPECULAR_BRDF_SIZE);
  const float4 value = max(SampleSpecularTable(table, uv), 0.0);
  return float4(value.xy, min(value.zw, value.xy));
}

// Turquin's scaled-lobe multiple-scattering approximation. Apply the same
// compensation to the screen contribution and its environment complement.
float3 GetSpecularEnergy(float3 f0, float2 integral) {
  const float single_scattering = max(integral.x + integral.y, 1e-6);
  const float3 compensation = 1.0 + f0 * (rcp(single_scattering) - 1.0);
  return (f0 * integral.x + integral.y) * compensation;
}

// Integrate two Gauss points in an equal-solid-angle sector. The slice mapping
// is m = sign(theta) * (1-cos(theta)); its differential already includes the
// spherical Jacobian. The caller supplies the inverse slice PDF and averaging.
float2 GetSpecularSectorWeights(float4 interval, uint sector, float sector_offset, float3 axis, float3 slice,
                                float3 normal, float3 view, float alpha_squared) {
  // Coverage quantization shifts internal sector edges by the noise phase.
  // The two outer edges stay at the cone boundary, so no energy falls out.
  const float lower = max(0.0, float(sector) / 32.0 - sector_offset);
  const float upper = sector == 31u ? 1.0 : max(0.0, float(sector + 1u) / 32.0 - sector_offset);
  const float step = (upper - lower) * interval.w;
  const float midpoint = interval.x + (lower + upper) * (0.5 * interval.w);
  float2 sum = 0.0;
  [unroll]
  for (uint quadrature_index = 0u; quadrature_index < 2u; ++quadrature_index) {
    const float measure = midpoint + (quadrature_index == 0u ? -0.2886751345948129 : 0.2886751345948129) * step;
    const float cosine = clamp(1.0 - abs(measure), -1.0, 1.0);
    const float sine = sign(measure) * sqrt(saturate(1.0 - cosine * cosine));
    sum += GetSpecularBRDFWeights(normal, view, axis * cosine + slice * sine, alpha_squared);
  }
  return sum * (0.5 * step);
}

// A negative alpha marks missing data. Specular radiance is demodulated by
// integrated reflectance; its paired response stores the remaining fraction.
bool IsSpecularGIValid(float4 radiance, float4 response) {
  return all(isfinite(radiance)) && all(radiance.rgb >= 0.0) && radiance.a >= 0.0 && radiance.a <= 1.0
         && all(isfinite(response)) && all(response.rgb >= 0.0) && all(response.rgb <= 1.0)
         && response.a >= 0.0 && response.a <= 1.0;
}

// Keep lighting from unrelated materials out of specular history and filters.
// A small absolute tolerance admits dielectric quantization without making
// dark dielectrics interchangeable with bright or colored metals.
float GetSpecularMaterialWeight(float3 f0, float3 sample_f0) {
  const float3 difference = abs(f0 - sample_f0) / (0.02 + 0.25 * max(f0, sample_f0));
  return 1.0 - smoothstep(0.5, 1.0, max(difference.x, max(difference.y, difference.z)));
}

// Reduce one common history blend instead of clipping its two payloads
// independently. A paired white-light partition stays intact during rejection.
float GetSpecularHistoryTrust(float3 current, float3 history, float3 lower, float3 upper) {
  const float3 delta = history - current;
  const float3 distance = delta >= 0.0 ? max(upper - current, 0.0) : max(current - lower, 0.0);
  const float3 trust = abs(delta) < 1e-8 ? 1.0 : saturate(distance / max(abs(delta), 1e-8));
  return min(trust.x, min(trust.y, trust.z));
}

#endif  // MGSV_SPECULAR_BRDF_HLSLI_
