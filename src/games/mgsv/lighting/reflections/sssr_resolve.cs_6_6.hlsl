#include "common.hlsli"
Texture2D<float4> input_light : register(t0);
Texture2D<float4> input_environment_weight : register(t1);
Texture2D<float4> normals : register(t2);
Texture2D<float4> materials : register(t3);
Texture2D<float> depth : register(t4);
Texture2D<float4> brdf_table : register(t5);
RWTexture2D<float4> output_light : register(u0);
RWTexture2D<float4> output_response : register(u1);

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
  if (any(id.xy >= reflection_size)) return;
  const int2 pixel = id.xy;
  const float4 normal = normals.Load(int3(pixel, 0));
  const float4 material = materials.Load(int3(pixel, 0));
  if (normal.a < 0.0) {
    output_light[pixel] = 0.0;
    if ((reflection_flags & 2u) == 0u) output_response[pixel] = 1.0;
    return;
  }
  // A qualified surface with no specular interface is a valid black result,
  // not missing data. Remove its saved fallback instead of preserving it.
  if (material.a <= 0.0) {
    output_light[pixel] = float4(0.0, 0.0, 0.0, 1.0);
    if ((reflection_flags & 2u) == 0u) output_response[pixel] = float4(0.0, 0.0, 0.0, 1.0);
    return;
  }
  const float3 view = normalize(-GetReflectionPosition(float2(pixel) + 0.5, depth.Load(int3(pixel, 0))));
  const SpecularResponse response = GetSpecularResponse(material.rgb, GetSpecularBRDFIntegral(brdf_table, abs(dot(normal.xyz, view)), normal.a));
  const float3 energy = response.energy * material.a;
  // NRD returns demodulated linear light in its documented YCoCg packing.
  // Reapply the original material response, not NRD's filtered guide roughness.
  const float3 light = REBLUR_BackEnd_UnpackRadianceAndNormHitDist(input_light.Load(int3(pixel, 0))).rgb;
  output_light[pixel] = float4(max(light, 0.0) * energy, 1.0);
  // Alpha preserves directional environment visibility from the same ray.
  // It is independent of the importance weight and is not a second AO factor.
  if ((reflection_flags & 2u) == 0u) output_response[pixel] = input_environment_weight.Load(int3(pixel, 0));
}
