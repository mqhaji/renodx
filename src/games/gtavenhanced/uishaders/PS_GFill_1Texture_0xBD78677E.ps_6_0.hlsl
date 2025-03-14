#include "../shared.h"

Texture2D<float4> t0 : register(t5, space1);

Texture3D<float4> t1 : register(t7);

cbuffer cb0 : register(b4) {
  float cb0_012x : packoffset(c012.x);
  float cb0_012w : packoffset(c012.w);
  float cb0_025x : packoffset(c025.x);
};

cbuffer cb1 : register(b8, space1) {
  float cb1_007x : packoffset(c007.x);
  float cb1_007y : packoffset(c007.y);
  float cb1_007z : packoffset(c007.z);
  float cb1_007w : packoffset(c007.w);
  float cb1_008x : packoffset(c008.x);
  float cb1_008y : packoffset(c008.y);
  float cb1_008z : packoffset(c008.z);
  float cb1_008w : packoffset(c008.w);
  float cb1_009x : packoffset(c009.x);
  uint cb1_010z : packoffset(c010.z);
  uint cb1_010w : packoffset(c010.w);
};

cbuffer cb2 : register(b254) {
  uint cb2_000x : packoffset(c000.x);
};

SamplerState s0 : register(s2, space1);

SamplerState s1[] : register(s0, space2);

float4 main(
    noperspective float4 SV_Position: SV_Position,
    linear float4 COLOR: COLOR,
    linear float4 COLOR_1: COLOR1,
    linear float2 TEXCOORD: TEXCOORD)
    : SV_Target {
  float4 SV_Target;
  uint _12 = (cb2_000x) + 0;
  float4 _14 = t0.Sample(s1[_12], float2((TEXCOORD.x), (TEXCOORD.y)));
  float _24 = (cb1_008x) * (_14.x);
  float _25 = (cb1_008y) * (_14.y);
  float _26 = (cb1_008z) * (_14.z);
  float _27 = (cb1_008w) * (_14.w);
  float _33 = _24 + (cb1_007x);
  float _34 = _25 + (cb1_007y);
  float _35 = _26 + (cb1_007z);
  float _36 = _27 + (cb1_007w);
  float _37 = _36 * (COLOR_1.w);
  bool _40 = (((uint)(cb1_010w)) == 0);
  float _47 = _33;
  float _48 = _34;
  float _49 = _35;
  float _75;
  if (!_40) {
    float4 _42 = t1.SampleLevel(s0, float3(_33, _34, _35), 0.0f);
    _47 = (_42.x);
    _48 = (_42.y);
    _49 = (_42.z);
  }
  bool _52 = ((cb1_009x) == 0.0f);
  float _53 = _47 * _37;
  float _54 = _48 * _37;
  float _55 = _49 * _37;
  float _56 = (_52 ? _47 : _53);
  float _57 = (_52 ? _48 : _54);
  float _58 = (_52 ? _49 : _55);
  float _61 = (cb0_012w)*_37;
  bool _62 = !((cb0_012w) == 1.0f);
  float _63 = _61 * _61;
  float _64 = (_62 ? _63 : _58);
  float _66 = (cb0_012x)*_37;
  bool _68 = (((uint)(cb1_010z)) == 0);
  _75 = _66;
  if (!_68) {
    float _70 = _66 * 3.1415927410125732f;
    float _71 = cos(_70);
    float _72 = _71 + -1.0f;
    float _73 = _72 * -0.5f;
    _75 = _73;
  }
  float _76 = saturate(_75);
  float _79 = (cb0_025x)*_56;
  float _80 = (cb0_025x)*_57;
  float _81 = (cb0_025x)*_64;
  SV_Target.x = _79;
  SV_Target.y = _80;
  SV_Target.z = _81;
  SV_Target.w = _76;

  SV_Target.rgb = renodx::draw::RenderIntermediatePass(renodx::color::srgb::Decode(max(0, SV_Target.rgb)));

  return SV_Target;
}
