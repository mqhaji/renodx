Texture2D<float4> t0 : register(t7, space1);

struct _t1 {
  float data[12];
};
StructuredBuffer<_t1> t1 : register(t13, space1);

Texture2D<float2> t2 : register(t18, space1);

RWTexture2D<float4> u0 : register(u1, space1);

RWTexture2D<float4> u1 : register(u2, space1);

RWTexture2D<float4> u2 : register(u3, space1);

RWBuffer<uint> u3 : register(u7, space1);

cbuffer cb0 : register(b0, space1) {
  uint cb0_006x : packoffset(c006.x);
};

cbuffer cb1 : register(b1, space1) {
  float cb1_001z : packoffset(c001.z);
  float cb1_001w : packoffset(c001.w);
};

SamplerState s0 : register(s0, space1);

[numthreads(8, 8, 1)]
void main(
    uint3 SV_DispatchThreadID: SV_DispatchThreadID,
    uint3 SV_GroupID: SV_GroupID,
    uint3 SV_GroupThreadID: SV_GroupThreadID,
    uint SV_GroupIndex: SV_GroupIndex) {
  uint _16 = ((uint)(cb0_006x)) << 1;
  uint _17 = _16 + (SV_GroupID.x);
  uint _18 = u3.Load(_17);
  int _20 = (_18.x) & 65535;
  int _21 = (uint)(((int)(_18.x))) >> 16;
  uint _22 = _20 + (SV_GroupThreadID.x);
  uint _23 = _21 + (SV_GroupThreadID.y);
  float _24 = float((uint)_22);
  float _25 = float((uint)_23);
  float _29 = _24 + 0.5f;
  float _30 = (cb1_001z)*_29;
  float _31 = _25 + 0.5f;
  float _32 = (cb1_001w)*_31;
  float4 _33 = t2.GatherRed(s0, float2(_30, _32));
  float4 _38 = t0.GatherRed(s0, float2(_30, _32));
  float4 _43 = t0.GatherGreen(s0, float2(_30, _32));
  float4 _48 = t0.GatherBlue(s0, float2(_30, _32));
  _t1 _53 = t1.Load(0);
  bool _55 = ((_33.x) <= (_53.data[0]));
  float _56 = (_55 ? 1.0f : 0.0f);
  bool _57 = ((_33.y) <= (_53.data[0]));
  float _58 = (_57 ? 1.0f : 0.0f);
  bool _59 = ((_33.z) <= (_53.data[0]));
  float _60 = (_59 ? 1.0f : 0.0f);
  bool _61 = ((_33.w) <= (_53.data[0]));
  float _62 = (_61 ? 1.0f : 0.0f);
  float _63 = 1.0f - _56;
  float _64 = 1.0f - _58;
  float _65 = 1.0f - _60;
  float _66 = 1.0f - _62;
  float _67 = _56 + _58;
  float _68 = _67 + _60;
  float _69 = _68 + _62;
  float _70 = 4.0f - _69;
  bool _71 = (_69 > _70);
  float _72 = (_71 ? _56 : _63);
  float _73 = (_71 ? _58 : _64);
  float _74 = (_71 ? _60 : _65);
  float _75 = (_71 ? _62 : _66);
  float _76 = dot(float4(_72, _73, _74, _75), float4(1.0f, 1.0f, 1.0f, 1.0f));
  float _77 = _72 * (_38.x);
  float _78 = _72 * (_43.x);
  float _79 = _72 * (_48.x);
  float _80 = _73 * (_38.y);
  float _81 = _73 * (_43.y);
  float _82 = _73 * (_48.y);
  float _83 = _77 + _80;
  float _84 = _78 + _81;
  float _85 = _79 + _82;
  float _86 = _74 * (_38.z);
  float _87 = _74 * (_43.z);
  float _88 = _74 * (_48.z);
  float _89 = _83 + _86;
  float _90 = _84 + _87;
  float _91 = _85 + _88;
  float _92 = _75 * (_38.w);
  float _93 = _75 * (_43.w);
  float _94 = _75 * (_48.w);
  float _95 = _89 + _92;
  float _96 = _90 + _93;
  float _97 = _91 + _94;
  float _98 = _95 / _76;
  float _99 = _96 / _76;
  float _100 = _97 / _76;
  u1[int2(_22, _23)] = float4(_98, _99, _100, 1.0f);
  u2[int2(_22, _23)] = float4(_98, _99, _100, 1.0f);
  u0[int2(_22, _23)] = float4(0.0f, 0.0f, 0.0f, 0.0f);
}
