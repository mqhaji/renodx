// ---- Created with 3Dmigoto v1.3.16 on Sun Sep 22 01:43:31 2024

#include "../tonemap.hlsl"

SamplerState SamplerLowResCapture_SMP_s : register(s5);
SamplerState SamplerFrameBuffer_SMP_s : register(s6);
SamplerState SamplerDistortion_SMP_s : register(s7);
SamplerState SamplerBloomMap0_SMP_s : register(s8);
SamplerState SamplerQuarterSizeBlur_SMP_s : register(s9);
SamplerState SamplerNoise_SMP_s : register(s12);
SamplerState SamplerToneMapCurve_SMP_s : register(s14);
SamplerState SamplerOverlay_SMP_s : register(s15);
Texture2D<float4> SamplerLowResCapture_TEX : register(t5);
Texture2D<float4> SamplerFrameBuffer_TEX : register(t6);
Texture2D<float4> SamplerDistortion_TEX : register(t7);
Texture2D<float4> SamplerBloomMap0_TEX : register(t8);
Texture2D<float4> SamplerQuarterSizeBlur_TEX : register(t9);
Texture2D<float4> SamplerNoise_TEX : register(t12);
Texture2D<float4> SamplerToneMapCurve_TEX : register(t14);
Texture2D<float4> SamplerOverlay_TEX : register(t15);


void main(
  float4 v0 : TEXCOORD0,
  float4 v1 : TEXCOORD1,
  float4 v2 : SV_Position0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2,r3,r4;
  uint4 bitmask, uiDest;
  float4 fDest;

  GetSceneColorAndTexCoord(
      SamplerDistortion_TEX, SamplerDistortion_SMP_s, SamplerFrameBuffer_TEX,
      SamplerFrameBuffer_SMP_s, v0, r2.rgb, r0.xy);

  r1.rgb = ApplyMotionBlurType1(
      r2.rgb, r0.xy, SamplerQuarterSizeBlur_TEX,
      SamplerQuarterSizeBlur_SMP_s);

  r1.rgb = ApplyBloomType1(r1.rgb, r0.xy, SamplerBloomMap0_TEX, SamplerBloomMap0_SMP_s);

  r0.rgb = ApplyDizzyEffect(r1.rgb, r0.xy, SamplerLowResCapture_TEX, SamplerLowResCapture_SMP_s);

  const float untonemapped_lum = renodx::color::luma::from::BT601(r0.rgb);  // save for reuse

  float3 outputColor = ApplyToneMapVignette(
      r0.xyz, untonemapped_lum, v1, v2, SamplerToneMapCurve_TEX,
      SamplerToneMapCurve_SMP_s);

  r0.xyz = EncodeGamma(outputColor);
  r0.rgb = ApplyFilmGrain(r0.rgb, SamplerNoise_TEX, SamplerNoise_SMP_s, v1);

  r0.xyz = ApplyBloodOverlay(r0.xyz, v0, SamplerOverlay_TEX, SamplerOverlay_SMP_s);

  o0 = FinalizeToneMapOutput(r0.rgb);
  return;
}