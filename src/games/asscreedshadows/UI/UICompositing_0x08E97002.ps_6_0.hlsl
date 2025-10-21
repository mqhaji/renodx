#include "../shared.h"

struct UICompositingParameters__Constants {
  float4 UICompositingParameters__Constants_000[4];
  float UICompositingParameters__Constants_064;
  float UICompositingParameters__Constants_068;
  float UICompositingParameters__Constants_072;
  float UICompositingParameters__Constants_076;
  int4 UICompositingParameters__Constants_080;
  int4 UICompositingParameters__Constants_096;
  int4 UICompositingParameters__Constants_112;
  int4 UICompositingParameters__Constants_128;
  int2 UICompositingParameters__Constants_144;
};

Texture2D<float4> UI_Texture : register(t0, space3);

Texture2D<float4> Scene_Texture : register(t1, space3);

Texture3D<float4> UI_LUT : register(t3, space3);

cbuffer cb0_space3 : register(b0, space3) {
  UICompositingParameters__Constants UICompositingParameters_cbuffer_000 : packoffset(c000.x);
};

SamplerState Sampler : register(s8, space98);

SamplerState UI_LUT_Sampler : register(s0, space4);

float4 main(
    noperspective float4 SV_Position: SV_Position,
    linear float2 TEXCOORD: TEXCOORD)
    : SV_Target {
  float4 SV_Target;
  float4 ui_tex = UI_Texture.Sample(Sampler, TEXCOORD.xy);
  float3 ui_srgb = ui_tex.rgb;
  float ui_alpha = ui_tex.w;

  float4 scene_tex = Scene_Texture.Sample(Sampler, TEXCOORD.xy);
  float3 scene_pq = scene_tex.rgb;
  scene_pq.rgb = min(scene_pq.rgb, UICompositingParameters_cbuffer_000.UICompositingParameters__Constants_076);
  float scene_alpha = scene_tex.w;

  float one_minus_ui_alpha = 1.f - ui_alpha;

#if 1
  float3 ui_pq = renodx::lut::Sample(UI_LUT, UI_LUT_Sampler, ui_srgb, 32u);
  float3 ui_gamma = renodx::color::gamma::Encode(renodx::color::pq::DecodeSafe(ui_pq, 100.f));
  float3 scene_gamma = renodx::color::gamma::Encode(renodx::color::pq::DecodeSafe(scene_pq, 100.f));

  float luma = renodx::color::y::from::BT2020(ui_gamma);
  float new_luma = lerp(
      luma,
      renodx::tonemap::Reinhard(luma, one_minus_ui_alpha),
      ui_alpha);
  scene_gamma *= renodx::math::DivideSafe(new_luma, luma, 1.f);

  SV_Target.rgb = (ui_gamma * ui_alpha) + (scene_gamma * one_minus_ui_alpha);
  SV_Target.rgb = renodx::color::gamma::DecodeSafe(SV_Target.rgb);
  SV_Target.rgb = renodx::color::pq::EncodeSafe(SV_Target.rgb, 100.f);
#else
  float ui_tonemap_factor = 1.f / max(ui_alpha, 1.0000000116860974e-07f);
  ui_srgb = ui_srgb * ui_tonemap_factor;
  float3 ui_pq = renodx::lut::Sample(UI_LUT, UI_LUT_Sampler, ui_srgb, 32u);
  SV_Target.rgb = (ui_pq * ui_alpha) + (scene_pq * one_minus_ui_alpha);
#endif
  SV_Target.w = ((scene_alpha * one_minus_ui_alpha) + ui_alpha);
  return SV_Target;
}
