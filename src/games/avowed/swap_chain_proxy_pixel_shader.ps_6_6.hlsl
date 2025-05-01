#include "./common.hlsli"

Texture2D t0 : register(t0);
SamplerState s0 : register(s0);
float4 main(float4 vpos: SV_POSITION, float2 uv: TEXCOORD0)
    : SV_TARGET {
  float4 color = t0.Sample(s0, uv);

  float3 gamma_color = color.rgb;
  float3 linear_color = renodx::color::gamma::DecodeSafe(gamma_color, 2.2f);
  float3 bt2020_color = renodx::color::bt2020::from::BT709(linear_color);

  float3 pq_color = renodx::color::pq::EncodeSafe(bt2020_color, RENODX_GRAPHICS_WHITE_NITS);

  return float4(pq_color.rgb, color.a);
}
