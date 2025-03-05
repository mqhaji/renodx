#include "../common.hlsl"

Texture2D<float4> t0 : register(t5, space1);

cbuffer cb0 : register(b3) {
  uint cb0_000x : packoffset(c000.x);
};

cbuffer cb1 : register(b9, space1) {
  float cb1_002x : packoffset(c002.x);
  float cb1_002y : packoffset(c002.y);
  float cb1_002z : packoffset(c002.z);
  float cb1_002w : packoffset(c002.w);
  float cb1_003x : packoffset(c003.x);
};

SamplerState s0[] : register(s0, space2);

float4 main(
    noperspective float4 SV_Position: SV_Position,
    linear float4 COLOR: COLOR,
    linear float2 TEXCOORD: TEXCOORD)
    : SV_Target {
  float4 SV_Target;
  float4 _16 = t0.SampleLevel(s0[asuint(cb0_000x)], TEXCOORD.xy, (cb1_003x));
  float _21 = (_16.x) * (COLOR.x);
  float _22 = (_16.y) * (COLOR.y);
  float _23 = (_16.z) * (COLOR.z);
  float _24 = (_16.w) * (COLOR.w);
  float _30 = _21 * (cb1_002x);
  float _31 = _22 * (cb1_002y);
  float _32 = _23 * (cb1_002z);
  float _33 = _24 * (cb1_002w);
  SV_Target.x = _30;
  SV_Target.y = _31;
  SV_Target.z = _32;
  SV_Target.w = _33;
 
  SV_Target.rgb = UIScale(SV_Target.rgb);

  return SV_Target;
}
