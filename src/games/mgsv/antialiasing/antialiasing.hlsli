#include "../common.hlsli"

namespace renodx_game {
namespace antialiasing {

float GetSceneScaleInverse() {
  return renodx::math::SafeDivision(RENODX_GRAPHICS_WHITE_NITS, RENODX_DIFFUSE_WHITE_NITS, 1.f);
}

float GetSceneScale() {
  return renodx::math::SafeDivision(RENODX_DIFFUSE_WHITE_NITS, RENODX_GRAPHICS_WHITE_NITS, 1.f);
}

float3 DecodeScene(float3 color) {
  return RENODX_GAMMA_CORRECTION
             ? renodx::color::gamma::DecodeSafe(color)
             : renodx::color::srgb::DecodeSafe(color);
}

float DecodeScene(float color) {
  return RENODX_GAMMA_CORRECTION
             ? renodx::color::gamma::DecodeSafe(color)
             : renodx::color::srgb::DecodeSafe(color);
}

float3 EncodeScene(float3 color) {
  return RENODX_GAMMA_CORRECTION
             ? renodx::color::gamma::EncodeSafe(color)
             : renodx::color::srgb::EncodeSafe(color);
}

float EncodeScene(float color) {
  return RENODX_GAMMA_CORRECTION
             ? renodx::color::gamma::EncodeSafe(color)
             : renodx::color::srgb::EncodeSafe(color);
}

float3 InverseSceneScale(float3 color) {
  color = DecodeScene(color);
  color *= GetSceneScaleInverse();
  return EncodeScene(color);
}

float InverseSceneScale(float color) {
  color = DecodeScene(color);
  color *= GetSceneScaleInverse();
  return EncodeScene(color);
}

float4 InverseSceneScale(float4 color) {
  return float4(InverseSceneScale(color.rgb), color.a);
}

float3 ApplySceneScale(float3 color) {
  color = DecodeScene(color);
  color *= GetSceneScale();
  return EncodeScene(color);
}

float4 ApplySceneScale(float4 color) {
  return float4(ApplySceneScale(color.rgb), color.a);
}

float4 InverseSceneScaleGather(float4 color) {
  return float4(
      InverseSceneScale(color.x),
      InverseSceneScale(color.y),
      InverseSceneScale(color.z),
      InverseSceneScale(color.w));
}

float4 SampleLevelScene(
    Texture2D<float4> texture_input,
    SamplerState sampler_input,
    float2 texcoord,
    float lod) {
  return InverseSceneScale(texture_input.SampleLevel(sampler_input, texcoord, lod));
}

float4 SampleLevelScene(
    Texture2D<float4> texture_input,
    SamplerState sampler_input,
    float2 texcoord,
    float lod,
    int2 offset) {
  return InverseSceneScale(texture_input.SampleLevel(sampler_input, texcoord, lod, offset));
}

float4 GatherScene(
    Texture2D<float4> texture_input,
    SamplerState sampler_input,
    float2 texcoord) {
  return InverseSceneScaleGather(texture_input.Gather(sampler_input, texcoord));
}

float4 GatherScene(
    Texture2D<float4> texture_input,
    SamplerState sampler_input,
    float2 texcoord,
    int2 offset) {
  return InverseSceneScaleGather(texture_input.Gather(sampler_input, texcoord, offset));
}

}  // namespace antialiasing
}  // namespace renodx_game
