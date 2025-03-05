Texture2D<float4> t0 : register(t14, space1);

Texture2D<float> t1 : register(t26, space1);

Texture2D<float> t2 : register(t27, space1);

RWTexture2D<float2> u0 : register(u3, space1);

cbuffer cb0 : register(b5) {
  float cb0_015x : packoffset(c015.x);
  float cb0_015y : packoffset(c015.y);
};

cbuffer cb1 : register(b12, space1) {
  float cb1_000z : packoffset(c000.z);
  float cb1_000w : packoffset(c000.w);
  float cb1_003x : packoffset(c003.x);
  float cb1_003y : packoffset(c003.y);
  float cb1_003z : packoffset(c003.z);
  float cb1_003w : packoffset(c003.w);
  float cb1_094x : packoffset(c094.x);
  float cb1_094z : packoffset(c094.z);
};

SamplerState s0 : register(s2, space1);

float2 main(
  noperspective float4 SV_Position : SV_Position,
  linear float4 TEXCOORD : TEXCOORD,
  linear float TEXCOORD_1 : TEXCOORD1
) : SV_Target {
  float2 SV_Target;
  float _13 = (cb0_015x) * (TEXCOORD.x);
  float _14 = (cb0_015y) * (TEXCOORD.y);
  uint _15 = uint(_13);
  uint _16 = uint(_14);
  float4 _17 = t0.Sample(s0, float2((TEXCOORD.x), (TEXCOORD.y)));
  float _19 = 1.0f - (_17.x);
  float _23 = _19 + (cb1_000w);
  float _24 = (cb1_000z) / _23;
  float _25 = t1.Sample(s0, float2((TEXCOORD.x), (TEXCOORD.y)));
  float _27 = t2.Sample(s0, float2((TEXCOORD.x), (TEXCOORD.y)));
  float _29 = saturate((_27.x));
  bool _30 = (_29 > 0.15000000596046448f);
  float _31 = float((bool)_30);
  float _32 = (_25.x) - _24;
  float _33 = _31 * _32;
  float _34 = _33 + _24;
  float _37 = _34 - (cb1_003x);
  float _39 = _37 * (cb1_003y);
  float _40 = saturate(_39);
  float _41 = 1.0f - _40;
  float _43 = _34 - (cb1_003z);
  float _45 = _43 * (cb1_003w);
  float _46 = saturate(_45);
  float _47 = _46 + _41;
  float _48 = saturate(_47);
  bool _49 = (_41 > 0.0f);
  float _50 = -0.0f - _41;
  float _51 = (_49 ? _50 : _48);
  float _54 = abs(_51);
  float _55 = log2(_54);
  float _56 = _55 * (cb1_094z);
  float _57 = exp2(_56);
  bool _58 = (_51 > 0.0f);
  bool _59 = (_51 < 0.0f);
  int _60 = (bool)(_58);
  int _61 = (bool)(_59);
  int _62 = _60 - _61;
  float _63 = float(_62);
  float _64 = _63 * _57;
  float _66 = _64 * (cb1_094x);
  int _67 = _15 | _16;
  int _68 = _67 & 1;
  bool _69 = (_68 == 0);
  if (_69) {
    int _71 = (uint)(_15) >> 1;
    int _72 = (uint)(_16) >> 1;
    u0[int2(_71, _72)] = float2(_34, _66);
  }
  SV_Target.x = _34;
  SV_Target.y = _66;
  return SV_Target;
}
