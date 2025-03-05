Texture2D<float4> t0 : register(t0, space1);

Texture2D<float4> t1 : register(t1, space1);

Texture2D<float4> t2 : register(t2, space1);

Texture3D<float4> t3 : register(t4, space1);

cbuffer cb0 : register(b3, space1) {
  float cb0_001x : packoffset(c001.x);
};

SamplerState s0 : register(s2, space1);

float4 main(
    noperspective float4 SV_Position: SV_Position,
    linear float4 COLOR: COLOR,
    linear float4 TEXCOORD: TEXCOORD)
    : SV_Target {
  float4 SV_Target;
  float4 _15 = t0.Sample(s0, TEXCOORD.xy);
  float4 _17 = t1.Sample(s0, TEXCOORD.zw);
  float4 _19 = t2.Sample(s0, TEXCOORD.zw);
  float _21 = dot(float4(1.16412353515625f, 1.595794677734375f, 0.0f, -0.8706550598144531f), float4((_15.x), (_17.x), (_19.x), 1.0f));
  float _22 = dot(float4(1.16412353515625f, -0.8134765625f, -0.391448974609375f, 0.5297050476074219f), float4((_15.x), (_17.x), (_19.x), 1.0f));
  float _23 = dot(float4(1.16412353515625f, 0.0f, 2.017822265625f, -1.0816688537597656f), float4((_15.x), (_17.x), (_19.x), 1.0f));
  float _24 = _21 * (COLOR.x);
  float _25 = _22 * (COLOR.y);
  float _26 = _23 * (COLOR.z);
  bool _29 = ((cb0_001x) > 0.0f);
  float _36 = _24;
  float _37 = _25;
  float _38 = _26;
  if (_29) {
    float4 _31 = t3.SampleLevel(s0, float3(_24, _25, _26), 0.0f);
    _36 = (_31.x);
    _37 = (_31.y);
    _38 = (_31.z);
  }
  SV_Target.x = _36;
  SV_Target.y = _37;
  SV_Target.z = _38;
  SV_Target.w = (COLOR.w);

  return SV_Target;
}
