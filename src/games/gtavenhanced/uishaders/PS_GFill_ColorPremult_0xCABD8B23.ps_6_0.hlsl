#include "../shared.h"

Texture3D<float4> t0 : register(t7);

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

SamplerState s0 : register(s2, space1);

float4 main(
    noperspective float4 SV_Position: SV_Position,
    linear float4 COLOR: COLOR)
    : SV_Target {
  float4 SV_Target;
  float _14 = (cb1_008x) * (COLOR.x);
  float _15 = (cb1_008y) * (COLOR.y);
  float _16 = (cb1_008z) * (COLOR.z);
  float _17 = (cb1_008w) * (COLOR.w);
  float _23 = _14 + (cb1_007x);
  float _24 = _15 + (cb1_007y);
  float _25 = _16 + (cb1_007z);
  float _26 = _17 + (cb1_007w);
  bool _29 = (((uint)(cb1_010w)) == 0);
  float _36 = _23;
  float _37 = _24;
  float _38 = _25;
  float _65;
  if (!_29) {
    float4 _31 = t0.SampleLevel(s0, float3(_23, _24, _25), 0.0f);
    _36 = (_31.x);
    _37 = (_31.y);
    _38 = (_31.z);
  }
  bool _41 = ((cb1_009x) == 0.0f);
  float _42 = _36 * _26;
  float _43 = _37 * _26;
  float _44 = _38 * _26;
  float _45 = (_41 ? _36 : _42);
  float _46 = (_41 ? _37 : _43);
  float _47 = (_41 ? _38 : _44);
  float _50 = (cb0_012w)*_26;
  bool _51 = !((cb0_012w) == 1.0f);
  float _52 = _50 * _50;
  float _53 = (_51 ? _52 : _47);
  float _55 = (cb0_012x)*_26;
  bool _58 = (((uint)(cb1_010z)) == 0);
  _65 = _55;
  if (!_58) {
    float _60 = _55 * 3.1415927410125732f;
    float _61 = cos(_60);
    float _62 = _61 + -1.0f;
    float _63 = _62 * -0.5f;
    _65 = _63;
  }
  float _66 = saturate(_65);
  float _69 = (cb0_025x)*_45;
  float _70 = (cb0_025x)*_46;
  float _71 = (cb0_025x)*_53;
  SV_Target.x = _69;
  SV_Target.y = _70;
  SV_Target.z = _71;
  SV_Target.w = _66;

  SV_Target.rgb = renodx::draw::RenderIntermediatePass(renodx::color::srgb::Decode(max(0, SV_Target.rgb)));

  return SV_Target;
}
