Texture2D<float4> t0 : register(t15, space1);

cbuffer cb0 : register(b3) {
  uint cb0_000w : packoffset(c000.w);
};

cbuffer cb1 : register(b12, space1) {
  float cb1_072x : packoffset(c072.x);
  float cb1_072y : packoffset(c072.y);
};

SamplerState s0[] : register(s0, space2);

float4 main(
  noperspective float4 SV_Position : SV_Position,
  linear float2 TEXCOORD : TEXCOORD
) : SV_Target {
  float4 SV_Target;
  uint _8 = (cb0_000w) + 0;
  float _13 = (cb1_072x) * 0.949999988079071f;
  float _14 = (cb1_072y) * 0.949999988079071f;
  float _15 = (TEXCOORD.x) - _13;
  float _16 = (TEXCOORD.y) - _14;
  float4 _17 = t0.Sample(s0[_8], float2(_15, _16));
  uint _23 = (cb0_000w) + 0;
  float _28 = (cb1_072x) * 0.949999988079071f;
  float _29 = (cb1_072y) * 0.949999988079071f;
  float _30 = (TEXCOORD.x) - _28;
  float _31 = _29 + (TEXCOORD.y);
  float4 _32 = t0.Sample(s0[_23], float2(_30, _31));
  uint _38 = (cb0_000w) + 0;
  float _43 = (cb1_072x) * 0.949999988079071f;
  float _44 = (cb1_072y) * 0.949999988079071f;
  float _45 = _43 + (TEXCOORD.x);
  float _46 = _44 + (TEXCOORD.y);
  float4 _47 = t0.Sample(s0[_38], float2(_45, _46));
  uint _53 = (cb0_000w) + 0;
  float _58 = (cb1_072x) * 0.949999988079071f;
  float _59 = (cb1_072y) * 0.949999988079071f;
  float _60 = _58 + (TEXCOORD.x);
  float _61 = (TEXCOORD.y) - _59;
  float4 _62 = t0.Sample(s0[_53], float2(_60, _61));
  float _66 = min((_47.x), (_62.x));
  float _67 = min((_47.y), (_62.y));
  float _68 = min((_47.z), (_62.z));
  float _69 = min((_17.x), (_32.x));
  float _70 = min((_17.y), (_32.y));
  float _71 = min((_17.z), (_32.z));
  float _72 = min(_69, _66);
  float _73 = min(_70, _67);
  float _74 = min(_71, _68);
  SV_Target.x = _72;
  SV_Target.y = _73;
  SV_Target.z = _74;
  SV_Target.w = 1.0f;
  return SV_Target;
}
