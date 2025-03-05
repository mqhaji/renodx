#include "../common.hlsl"

Texture2D<float4> t0 : register(t5, space1);

cbuffer cb0 : register(b3) {
  uint cb0_000x : packoffset(c000.x);
};

cbuffer cb1 : register(b9, space1) {
  float cb1_002x : packoffset(c002.x);
};

SamplerState s0[] : register(s0, space2);

float4 main(
    noperspective float4 SV_Position: SV_Position,
    linear float4 COLOR: COLOR,
    linear float2 TEXCOORD: TEXCOORD)
    : SV_Target {
  float4 main_tex = t0.Sample(s0[asuint(cb0_000x)], float2(TEXCOORD.x, 1.0f - (TEXCOORD.y)));

  main_tex.rgb = renodx::color::gamma::Decode(main_tex.rgb, 2.2f);        // Linearize
  main_tex.rgb = renodx::color::gamma::Encode(main_tex.rgb, (cb1_002x));  // Apply gamma slider

  main_tex.rgb = UIScale(main_tex.rgb);

  return float4(main_tex.rgb, float((main_tex.w) > 0.0f));
}
