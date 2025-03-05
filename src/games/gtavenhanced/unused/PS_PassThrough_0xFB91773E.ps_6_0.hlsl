Texture2D<float4> t0 : register(t15, space1);

cbuffer cb0 : register(b3) {
  uint cb0_000w : packoffset(c000.w);
};

cbuffer cb1 : register(b5) {
  float cb1_014w : packoffset(c014.w);
};

SamplerState s0[] : register(s0, space2);

float4 main(
  noperspective float4 SV_Position : SV_Position,
  linear float2 TEXCOORD : TEXCOORD
) : SV_Target {
  float4 SV_Target;
  uint _8 = (cb0_000w) + 0;
  float4 _10 = t0.Sample(s0[_8], float2((TEXCOORD.x), (TEXCOORD.y)));
  float _17 = (cb1_014w) * (_10.x);
  float _18 = (cb1_014w) * (_10.y);
  float _19 = (cb1_014w) * (_10.z);
  SV_Target.x = _17;
  SV_Target.y = _18;
  SV_Target.z = _19;
  SV_Target.w = (_10.w);
  return SV_Target;
}
