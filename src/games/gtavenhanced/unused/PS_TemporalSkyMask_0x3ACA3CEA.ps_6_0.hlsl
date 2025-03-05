Texture2D<float> t0 : register(t5, space1);

Texture2D<float4> t1 : register(t13, space1);

Texture2D<float2> t2 : register(t14, space1);

cbuffer cb0 : register(b8, space1) {
  float cb0_000x : packoffset(c000.x);
  float cb0_000y : packoffset(c000.y);
  float cb0_000z : packoffset(c000.z);
  float cb0_000w : packoffset(c000.w);
  float cb0_002z : packoffset(c002.z);
  float cb0_002w : packoffset(c002.w);
  float cb0_021w : packoffset(c021.w);
};

SamplerState s0 : register(s0, space1);

float4 main(
  noperspective float4 SV_Position : SV_Position,
  linear float4 TEXCOORD : TEXCOORD,
  linear float4 TEXCOORD_1 : TEXCOORD1
) : SV_Target {
  float4 SV_Target;
  float _11 = (cb0_000z) * (TEXCOORD.x);
  float _12 = (cb0_000w) * (TEXCOORD.y);
  int _13 = int(13);
  int _14 = int(14);
  float _18 = t0.Load(int3(_13, _14, 0));
  float _20 = (cb0_002w) + 1.0f;
  float _21 = _20 - (_18.x);
  float _22 = (cb0_002z) / _21;
  bool _23 = ((_18.x) == 0.0f);
  float _24 = float((bool)_23);
  uint _25 = _13 + -1;
  uint _26 = _14 + -1;
  float _27 = t0.Load(int3(_25, _26, 0));
  float _29 = _20 - (_27.x);
  float _30 = (cb0_002z) / _29;
  uint _31 = _14 + 1;
  float _32 = t0.Load(int3(_25, _31, 0));
  float _34 = _20 - (_32.x);
  float _35 = (cb0_002z) / _34;
  uint _36 = _13 + 1;
  float _37 = t0.Load(int3(_36, _26, 0));
  float _39 = _20 - (_37.x);
  float _40 = (cb0_002z) / _39;
  float _41 = t0.Load(int3(_36, _31, 0));
  float _43 = _20 - (_41.x);
  float _44 = (cb0_002z) / _43;
  float _45 = t0.Load(int3(_25, _14, 0));
  float _47 = _20 - (_45.x);
  float _48 = (cb0_002z) / _47;
  float _49 = t0.Load(int3(_13, _26, 0));
  float _51 = _20 - (_49.x);
  float _52 = (cb0_002z) / _51;
  float _53 = t0.Load(int3(_13, _31, 0));
  float _55 = _20 - (_53.x);
  float _56 = (cb0_002z) / _55;
  bool _57 = (_30 < _22);
  float _58 = (_57 ? _30 : _22);
  int _59 = int((bool)_57);
  bool _60 = (_35 < _58);
  float _61 = (_60 ? _35 : _58);
  int _62 = (_60 ? -1 : _59);
  int _63 = (_60 ? 1 : _59);
  bool _64 = (_40 < _61);
  float _65 = (_64 ? _40 : _61);
  int _66 = (_64 ? -1 : _63);
  bool _67 = (_44 < _65);
  float _68 = (_67 ? _44 : _65);
  bool _69 = _64 || _67;
  int _70 = (_69 ? 1 : _62);
  int _71 = (_67 ? 1 : _66);
  bool _72 = (_48 < _68);
  float _73 = (_72 ? _48 : _68);
  int _74 = (_72 ? -1 : _70);
  int _75 = (_72 ? 0 : _71);
  bool _76 = (_52 < _73);
  float _77 = (_76 ? _52 : _73);
  int _78 = (_76 ? -1 : _75);
  bool _79 = (_56 < _77);
  bool _80 = _76 || _79;
  int _81 = (_80 ? 0 : _74);
  int _82 = (_79 ? 1 : _78);
  uint _83 = _81 + _13;
  uint _84 = _82 + _14;
  float2 _85 = t2.Load(int3(_83, _84, 0));
  float _90 = (cb0_000x) * (_85.x);
  float _91 = (cb0_000y) * (_85.y);
  float _92 = _90 + (TEXCOORD.x);
  float _93 = _91 + (TEXCOORD.y);
  bool _94 = (_92 > 1.0f);
  bool _95 = (_93 > 1.0f);
  bool _96 = _94 || _95;
  bool _97 = (_92 < 0.0f);
  bool _98 = (_93 < 0.0f);
  bool _99 = _97 || _98;
  bool _100 = _96 || _99;
  float _112 = _24;
  if (!_100) {
    float4 _102 = t1.Sample(s0, float2(_92, _93));
    float _106 = (cb0_021w) * 0.3999999761581421f;
    float _107 = _106 + 0.5f;
    float _108 = (_102.x) - _24;
    float _109 = _107 * _108;
    float _110 = _109 + _24;
    _112 = _110;
  }
  SV_Target.x = _112;
  SV_Target.y = _112;
  SV_Target.z = _112;
  SV_Target.w = _112;
  return SV_Target;
}
