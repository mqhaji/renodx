Texture2D<float4> t0 : register(t7, space1);

struct _t1 {
  float data[12];
};
StructuredBuffer<_t1> t1 : register(t13, space1);

Texture2D<float2> t2 : register(t18, space1);

RWTexture2D<float4> u0 : register(u2, space1);

RWBuffer<uint> u1 : register(u7, space1);

cbuffer cb0 : register(b1, space1) {
  float cb0_001z : packoffset(c001.z);
  float cb0_001w : packoffset(c001.w);
};

SamplerState s0 : register(s0, space1);

[numthreads(8, 8, 1)]
void main(
 uint3 SV_DispatchThreadID : SV_DispatchThreadID,
 uint3 SV_GroupID : SV_GroupID,
 uint3 SV_GroupThreadID : SV_GroupThreadID,
 uint SV_GroupIndex : SV_GroupIndex
) {
  uint _11 = u1.Load(((uint)(SV_GroupID.x)));
  int _13 = (_11.x) & 65535;
  int _14 = (uint)(((int)(_11.x))) >> 16;
  uint _15 = _13 + (SV_GroupThreadID.x);
  uint _16 = _14 + (SV_GroupThreadID.y);
  float _17 = float((uint)_15);
  float _18 = float((uint)_16);
  float _22 = _17 + 0.5f;
  float _23 = (cb0_001z) * _22;
  float _24 = _18 + 0.5f;
  float _25 = (cb0_001w) * _24;
  float4 _26 = t2.GatherRed(s0, float2(_23, _25));
  float4 _31 = t0.GatherRed(s0, float2(_23, _25));
  float4 _36 = t0.GatherGreen(s0, float2(_23, _25));
  float4 _41 = t0.GatherBlue(s0, float2(_23, _25));
  _t1 _46 = t1.Load(0);
  bool _48 = ((_26.x) <= (_46.data[0]));
  float _49 = (_48 ? 1.0f : 0.0f);
  bool _50 = ((_26.y) <= (_46.data[0]));
  float _51 = (_50 ? 1.0f : 0.0f);
  bool _52 = ((_26.z) <= (_46.data[0]));
  float _53 = (_52 ? 1.0f : 0.0f);
  bool _54 = ((_26.w) <= (_46.data[0]));
  float _55 = (_54 ? 1.0f : 0.0f);
  float _56 = 1.0f - _49;
  float _57 = 1.0f - _51;
  float _58 = 1.0f - _53;
  float _59 = 1.0f - _55;
  float _60 = _49 + _51;
  float _61 = _60 + _53;
  float _62 = _61 + _55;
  float _63 = 4.0f - _62;
  bool _64 = (_62 > _63);
  float _65 = (_64 ? _49 : _56);
  float _66 = (_64 ? _51 : _57);
  float _67 = (_64 ? _53 : _58);
  float _68 = (_64 ? _55 : _59);
  float _69 = dot(float4(_65, _66, _67, _68), float4(1.0f, 1.0f, 1.0f, 1.0f));
  float _70 = _65 * (_31.x);
  float _71 = _65 * (_36.x);
  float _72 = _65 * (_41.x);
  float _73 = _66 * (_31.y);
  float _74 = _66 * (_36.y);
  float _75 = _66 * (_41.y);
  float _76 = _70 + _73;
  float _77 = _71 + _74;
  float _78 = _72 + _75;
  float _79 = _67 * (_31.z);
  float _80 = _67 * (_36.z);
  float _81 = _67 * (_41.z);
  float _82 = _76 + _79;
  float _83 = _77 + _80;
  float _84 = _78 + _81;
  float _85 = _68 * (_31.w);
  float _86 = _68 * (_36.w);
  float _87 = _68 * (_41.w);
  float _88 = _82 + _85;
  float _89 = _83 + _86;
  float _90 = _84 + _87;
  float _91 = _88 / _69;
  float _92 = _89 / _69;
  float _93 = _90 / _69;
  u0[int2(_15, _16)] = float4(_91, _92, _93, 1.0f);
}
