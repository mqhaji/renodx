#include "../common.hlsl"

Texture2D<float4> t0 : register(t5, space1);

Texture3D<float4> t1 : register(t35);

cbuffer cb0 : register(b3) {
  uint cb0_000x : packoffset(c000.x);
};

cbuffer cb1 : register(b5) {
  float cb1_025y : packoffset(c025.y);
};

SamplerState s0 : register(s2, space1);

SamplerState s1[] : register(s0, space2);

float4 main(
    noperspective float4 SV_Position: SV_Position,
    linear float4 COLOR: COLOR,
    linear float2 TEXCOORD: TEXCOORD)
    : SV_Target {
  float4 SV_Target;
  uint _14 = (cb0_000x) + 0;
  float4 _16 = t0.Sample(s1[_14], float2((TEXCOORD.x), (TEXCOORD.y)));
  int _23 = asint((cb1_025y));
  bool _24 = (_23 == 0);
  float _31 = (_16.x);
  float _32 = (_16.y);
  float _33 = (_16.z);
  if (!_24) {
    float4 _26 = t1.SampleLevel(s0, float3((_16.x), (_16.y), (_16.z)), 0.0f);
    _31 = (_26.x);
    _32 = (_26.y);
    _33 = (_26.z);
  }
  float _34 = _31 * (COLOR.x);
  float _35 = _32 * (COLOR.y);
  float _36 = _33 * (COLOR.z);
  float _37 = (_16.w) * (COLOR.w);
  SV_Target.x = _34;
  SV_Target.y = _35;
  SV_Target.z = _36;
  SV_Target.w = _37;

  SV_Target.rgb = UIScale(SV_Target.rgb);

  return SV_Target;
}
