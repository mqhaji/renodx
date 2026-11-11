// SPDX-License-Identifier: MIT
// Copyright and license notices: ../lighting/indirect/README.md#credits-and-license.

#include "brdf.hlsli"

// Run once on the selected device. The table is material-independent and
// immutable after this dispatch, so no integration work runs every frame.
RWTexture2D<float4> brdf_output : register(u0);

[numthreads(8, 8, 1)] void main(uint3 thread_id : SV_DispatchThreadID) {
  if (any(thread_id.xy >= SPECULAR_BRDF_SIZE)) return;
  const float no_v = max(float(thread_id.x) / float(SPECULAR_BRDF_SIZE - 1u), 1e-4);
  const float roughness = float(thread_id.y) / float(SPECULAR_BRDF_SIZE - 1u);
  const float3 view = float3(sqrt(saturate(1.0 - no_v * no_v)), 0.0, no_v);
  const float3 reflection = float3(-view.x, 0.0, view.z);
  const float cosine_radius = cos(GetSpecularConeRadius(roughness));
  const float alpha = roughness * roughness;
  const float alpha_squared = alpha * alpha;
  const float view_root = sqrt(alpha_squared + (1.0 - alpha_squared) * no_v * no_v);
  float4 sum = 0.0;
  [loop]
  for (uint index = 0u; index < SPECULAR_BRDF_SAMPLES; ++index) {
    const float2 sample = float2((float(index) + 0.5) / float(SPECULAR_BRDF_SAMPLES),
                                 float(reversebits(index)) * 2.3283064365386963e-10);
    const float3 half_vector = SampleSpecularVisibleNormal(view, max(alpha, 1e-6), sample);
    const float3 light = reflect(-view, half_vector);
    if (light.z <= 0.0) continue;
    // Under visible-normal sampling BRDF*cosine/PDF = Fresnel * G2/G1.
    const float light_root = sqrt(alpha_squared + (1.0 - alpha_squared) * light.z * light.z);
    const float masking = light.z * (no_v + view_root) / max(light.z * view_root + no_v * light_root, 1e-12);
    const float complement = 1.0 - saturate(dot(view, half_vector));
    const float square = complement * complement;
    const float fresnel = square * square * complement;
    const float2 weights = masking * float2(1.0 - fresnel, fresnel);
    sum.xy += weights;
    if (dot(light, reflection) >= cosine_radius) {
      sum.zw += weights;
    }
  }
  brdf_output[thread_id.xy] = sum / float(SPECULAR_BRDF_SAMPLES);
}
