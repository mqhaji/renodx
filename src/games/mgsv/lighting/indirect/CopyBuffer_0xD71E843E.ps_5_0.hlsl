// Port of the game's CopyBuffer, which copies diffuse light for the two
// skin-scattering passes and marks skin in alpha. The mod adds target 1: FP32
// source radiance that the GT-VBGI trace reads.
#include "source_radiance.hlsli"

cbuffer cPSSystem : register(b0) {
  struct {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem : packoffset(c0);
}

SamplerState g_samplerPoint_Clamp_s : register(s9);
Texture2D<float4> inLightBuffer : register(t0);
Texture2D<float4> inMaterialIDBuffer : register(t1);
Texture2D<float4> source_albedo : register(t18);
Texture2D<float4> source_normal : register(t19);
Texture2D<float> source_depth : register(t20);
Texture2D<float4> clean_material : register(t21);
Texture2D<uint4> material_owner : register(t22);

void main(float4 position : SV_Position0, out float4 native_copy : SV_Target0,
          out float4 source_radiance : SV_Target1) {
  // 1. Native output: the diffuse copy, with alpha 1 for skin materials
  // (IDs 10 and 110). Both scattering passes read it unchanged.
  const float2 uv = position.xy * g_psSystem.m_renderBuffer.zw;
  const int material_id = (int)(255.0 * inMaterialIDBuffer.Sample(g_samplerPoint_Clamp_s, uv).z);
  native_copy = float4(inLightBuffer.Sample(g_samplerPoint_Clamp_s, uv).rgb,
                       material_id == 10 || material_id == 110 ? 1.0 : 0.0);

  // 2. Added output: source radiance computed once per pixel (see
  // source_radiance.hlsli), so the GT-VBGI trace does not repeat the
  // cube-material lookup for every sample.
  uint width, height;
  source_albedo.GetDimensions(width, height);
  const int2 pixel = int2(position.xy);
  source_radiance = LoadSourceRadiance(inLightBuffer, source_albedo, inMaterialIDBuffer,
                                       clean_material, material_owner, pixel, uint2(width, height),
                                       source_normal.Load(int3(pixel, 0)), source_depth.Load(int3(pixel, 0)), true);
}
