#include "../shared.h"

Texture3D<float4> t0 : register(t7);

cbuffer cb0 : register(b4) {
  float cb0_012x : packoffset(c012.x);
  float cb0_012w : packoffset(c012.w);
  float cb0_025x : packoffset(c025.x);
};

cbuffer cb1 : register(b8, space1) {
  float cb1_006x : packoffset(c006.x);
  float cb1_006y : packoffset(c006.y);
  float cb1_006z : packoffset(c006.z);
  float cb1_006w : packoffset(c006.w);
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

float4 main()
    : SV_Target {
  float4 SV_Target;
  float _15 = (cb1_008x) * (cb1_006x);
  float _16 = (cb1_008y) * (cb1_006y);
  float _17 = (cb1_008z) * (cb1_006z);
  float _18 = (cb1_008w) * (cb1_006w);
  float _24 = _15 + (cb1_007x);
  float _25 = _16 + (cb1_007y);
  float _26 = _17 + (cb1_007z);
  float _27 = _18 + (cb1_007w);
  bool _30 = (((uint)(cb1_010w)) == 0);
  float _37 = _24;
  float _38 = _25;
  float _39 = _26;
  float _66;
  if (!_30) {
    float4 _32 = t0.SampleLevel(s0, float3(_24, _25, _26), 0.0f);
    _37 = (_32.x);
    _38 = (_32.y);
    _39 = (_32.z);
  }
  bool _42 = ((cb1_009x) == 0.0f);
  float _43 = _37 * _27;
  float _44 = _38 * _27;
  float _45 = _39 * _27;
  float _46 = (_42 ? _37 : _43);
  float _47 = (_42 ? _38 : _44);
  float _48 = (_42 ? _39 : _45);
  float _51 = (cb0_012w)*_27;
  bool _52 = !((cb0_012w) == 1.0f);
  float _53 = _51 * _51;
  float _54 = (_52 ? _53 : _48);
  float _56 = (cb0_012x)*_27;
  bool _59 = (((uint)(cb1_010z)) == 0);
  _66 = _56;
  if (!_59) {
    float _61 = _56 * 3.1415927410125732f;
    float _62 = cos(_61);
    float _63 = _62 + -1.0f;
    float _64 = _63 * -0.5f;
    _66 = _64;
  }
  float _67 = saturate(_66);
  float _70 = (cb0_025x)*_46;
  float _71 = (cb0_025x)*_47;
  float _72 = (cb0_025x)*_54;
  SV_Target.x = _70;
  SV_Target.y = _71;
  SV_Target.z = _72;
  SV_Target.w = _67;

  SV_Target.rgb = renodx::draw::RenderIntermediatePass(renodx::color::srgb::Decode(max(0, SV_Target.rgb)));

  return SV_Target;
}
