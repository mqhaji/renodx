Texture2D<float4> t0 : register(t7, space1);

Texture2D<float4> t1 : register(t12, space1);

RWTexture2D<float4> u0 : register(u2, space1);

RWBuffer<uint> u1 : register(u7, space1);

cbuffer cb0 : register(b1, space1) {
  float cb0_001x : packoffset(c001.x);
  float cb0_001y : packoffset(c001.y);
  float cb0_001z : packoffset(c001.z);
  float cb0_001w : packoffset(c001.w);
};

SamplerState s0 : register(s0, space1);

SamplerState s1 : register(s2, space1);

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
  float4 _26 = t0.SampleLevel(s1, float2(_23, _25), 0.0f);
  float4 _30 = t1.SampleLevel(s0, float2(_23, _25), 0.0f);
  bool _32 = !((_30.x) <= 2.0f);
  float _100 = (_26.x);
  float _101 = (_26.y);
  float _102 = (_26.z);
  if (_32) {
    float _36 = _23 - (cb0_001x);
    float _37 = _25 - (cb0_001y);
    float4 _38 = t0.SampleLevel(s1, float2(_36, _37), 0.0f);
    float4 _42 = t0.SampleLevel(s1, float2(_23, _37), 0.0f);
    float _46 = (cb0_001x) + _23;
    float4 _47 = t0.SampleLevel(s1, float2(_46, _37), 0.0f);
    float4 _51 = t0.SampleLevel(s1, float2(_36, _25), 0.0f);
    float4 _55 = t0.SampleLevel(s1, float2(_46, _25), 0.0f);
    float _59 = (cb0_001y) + _25;
    float4 _60 = t0.SampleLevel(s1, float2(_36, _59), 0.0f);
    float4 _64 = t0.SampleLevel(s1, float2(_23, _59), 0.0f);
    float4 _68 = t0.SampleLevel(s1, float2(_46, _59), 0.0f);
    float _72 = (_38.x) + (_26.x);
    float _73 = _72 + (_42.x);
    float _74 = _73 + (_47.x);
    float _75 = _74 + (_51.x);
    float _76 = _75 + (_55.x);
    float _77 = _76 + (_60.x);
    float _78 = _77 + (_64.x);
    float _79 = _78 + (_68.x);
    float _80 = (_38.y) + (_26.y);
    float _81 = _80 + (_42.y);
    float _82 = _81 + (_47.y);
    float _83 = _82 + (_51.y);
    float _84 = _83 + (_55.y);
    float _85 = _84 + (_60.y);
    float _86 = _85 + (_64.y);
    float _87 = _86 + (_68.y);
    float _88 = (_38.z) + (_26.z);
    float _89 = _88 + (_42.z);
    float _90 = _89 + (_47.z);
    float _91 = _90 + (_51.z);
    float _92 = _91 + (_55.z);
    float _93 = _92 + (_60.z);
    float _94 = _93 + (_64.z);
    float _95 = _94 + (_68.z);
    float _96 = _79 * 0.1111111119389534f;
    float _97 = _87 * 0.1111111119389534f;
    float _98 = _95 * 0.1111111119389534f;
    _100 = _96;
    _101 = _97;
    _102 = _98;
  }
  u0[int2(_15, _16)] = float4(_100, _101, _102, 1.0f);
}
