#include "../../materials/brdf.hlsli"
// Builds the GGX response table that the reflection trace and resolve share,
// once per backend. Rows are roughness and columns are N.V. RG holds the Schlick
// single-scattering terms (weights of 1 - F and F); B holds the row's
// cosine-weighted average for unit Fresnel, used for multiple scattering.
RWTexture2D<float4> output_brdf : register(u0);
groupshared float2 integrated_response[SPECULAR_BRDF_SIZE];
groupshared float average_terms[64];

// The host dispatches 16 x 16 groups of 8 x 8 threads. Each of the first 128
// groups integrates one full roughness row, two N.V columns per thread. Keeping
// a row in one group lets it compute the row's average in the same dispatch.
[numthreads(8, 8, 1)] void main(uint3 group : SV_GroupID, uint lane : SV_GroupIndex) {
  const uint row = group.x + group.y * (SPECULAR_BRDF_SIZE / 8u);
  if (row >= SPECULAR_BRDF_SIZE) return;
  const float roughness = float(row) / float(SPECULAR_BRDF_SIZE - 1u);
  const float alpha = max(roughness * roughness, SPECULAR_MIN_ALPHA);
  const float a2 = alpha * alpha;
  for (uint column = lane; column < SPECULAR_BRDF_SIZE; column += 64u) {
    const float no_v = float(column) / float(SPECULAR_BRDF_SIZE - 1u);
    const float3 view = float3(sqrt(saturate(1.0 - no_v * no_v)), 0.0, no_v);
    const float view_root = sqrt(a2 + (1.0 - a2) * no_v * no_v);
    float2 sum = 0.0;
    for (uint i = 0u; i < SPECULAR_BRDF_SAMPLES; ++i) {
      const float2 sample = float2((float(i) + 0.5) / float(SPECULAR_BRDF_SAMPLES), reversebits(i) * 2.3283064365386963e-10);
      const float3 half_vector = SampleSpecularVisibleNormal(view, alpha, sample);
      const float3 light = reflect(-view, half_vector);
      if (light.z <= 0.0) continue;
      const float light_root = sqrt(a2 + (1.0 - a2) * light.z * light.z);
      const float masking = light.z * (no_v + view_root) / max(light.z * view_root + no_v * light_root, 1e-20);
      const float fresnel = pow(1.0 - saturate(dot(view, half_vector)), 5.0);
      sum += masking * float2(1.0 - fresnel, fresnel);
    }
    const float2 integral = sum / float(SPECULAR_BRDF_SAMPLES);
    integrated_response[column] = integral / max(integral.x + integral.y, 1.0);
  }
  GroupMemoryBarrierWithGroupSync();

  // Integrate 2*NoV*E(NoV) exactly between this row's linear samples. Using
  // the same row keeps the compensation and its directional integrals paired.
  const float step = 1.0 / float(SPECULAR_BRDF_SIZE - 1u);
  float average = 0.0;
  for (uint column = lane; column + 1u < SPECULAR_BRDF_SIZE; column += 64u) {
    const float2 left = integrated_response[column];
    const float2 right = integrated_response[column + 1u];
    const float no_v = float(column) * step;
    average += step * ((no_v + step / 3.0) * (left.x + left.y) + (no_v + 2.0 * step / 3.0) * (right.x + right.y));
  }
  average_terms[lane] = average;
  GroupMemoryBarrierWithGroupSync();
  for (uint stride = 32u; stride > 0u; stride >>= 1u) {
    if (lane < stride) average_terms[lane] += average_terms[lane + stride];
    GroupMemoryBarrierWithGroupSync();
  }
  for (uint column = lane; column < SPECULAR_BRDF_SIZE; column += 64u) {
    output_brdf[uint2(column, row)] = float4(integrated_response[column], saturate(average_terms[0]), 0.0);
  }
}
