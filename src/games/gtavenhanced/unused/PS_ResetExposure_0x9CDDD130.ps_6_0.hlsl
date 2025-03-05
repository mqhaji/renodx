Texture2D<float4> t0 : register(t8, space1);

cbuffer cb0 : register(b12, space1) {
  float cb0_077x : packoffset(c077.x);
  float cb0_077y : packoffset(c077.y);
  float cb0_077z : packoffset(c077.z);
  float cb0_077w : packoffset(c077.w);
  float cb0_078x : packoffset(c078.x);
  float cb0_078y : packoffset(c078.y);
  float cb0_078z : packoffset(c078.z);
};

SamplerState s0 : register(s0, space1);

float4 main() : SV_Target {
  float4 SV_Target;
  float4 _4 = t0.Sample(s0, float2(0.5f, 0.5f));
  float _9 = log2((_4.x));
  float _10 = _9 * (cb0_077y);
  float _11 = exp2(_10);
  float _12 = _11 * (cb0_077x);
  float _14 = (cb0_077z) + _12;
  float _16 = _14 + (cb0_077w);
  float _20 = max(_16, (cb0_078x));
  float _21 = min(_20, (cb0_078y));
  float _22 = abs(_21);
  float _24 = (cb0_078z) * _22;
  bool _25 = (_21 > 0.0f);
  bool _26 = (_21 < 0.0f);
  int _27 = (bool)(_25);
  int _28 = (bool)(_26);
  int _29 = _27 - _28;
  float _30 = float(_29);
  float _31 = _24 * _30;
  float _32 = _31 + _21;
  float _33 = max(_32, (cb0_078x));
  float _34 = min(_33, (cb0_078y));
  float _35 = exp2(_34);
  SV_Target.x = _35;
  SV_Target.y = _34;
  SV_Target.z = _34;
  SV_Target.w = (_4.x);
  return SV_Target;
}
