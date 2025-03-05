#include "../shared.h"

Texture2D<float4> t0 : register(t15, space1);

cbuffer cb0 : register(b3) {
  uint cb0_000w : packoffset(c000.w);
};

cbuffer cb1 : register(b12, space1) {
  float cb1_007w : packoffset(c007.w);
};

SamplerState s0[] : register(s0, space2);

float4 main(
    noperspective float4 SV_Position: SV_Position,
    linear float2 TEXCOORD: TEXCOORD)
    : SV_Target {
  float3 bloom_tex = t0.Sample(s0[asuint(cb0_000w)], TEXCOORD.xy).rgb;
  float3 output_color = bloom_tex * cb1_007w;

  output_color *= CUSTOM_BLOOM;  // Custom bloom

  return float4(output_color, 0.f);
}
