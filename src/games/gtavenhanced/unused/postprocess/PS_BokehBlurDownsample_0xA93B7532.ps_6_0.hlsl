Texture2D<float4> t0 : register(t16, space1);

SamplerState s0 : register(s2, space1);

float2 main(
  noperspective float4 SV_Position : SV_Position,
  linear float4 TEXCOORD : TEXCOORD
) : SV_Target {
  float2 SV_Target;
  float4 _5 = t0.Sample(s0, float2((TEXCOORD.x), (TEXCOORD.y)));
  bool _8 = ((_5.y) < 0.0f);
  float _9 = -0.0f - (_5.y);
  float _10 = (_8 ? _9 : 0.0f);
  SV_Target.x = (_5.x);
  SV_Target.y = _10;
  return SV_Target;
}
