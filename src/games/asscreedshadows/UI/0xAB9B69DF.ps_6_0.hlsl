struct UICompositingParameters__Constants {
  float4 UICompositingParameters__Constants_000[4];
  float UICompositingParameters__Constants_064;
  float UICompositingParameters__Constants_068;
  float UICompositingParameters__Constants_072;
  float UICompositingParameters__Constants_076;
  int4 UICompositingParameters__Constants_080;
  int4 UICompositingParameters__Constants_096;
  int4 UICompositingParameters__Constants_112;
  int4 UICompositingParameters__Constants_128;
  int2 UICompositingParameters__Constants_144;
};


Texture2D<float4> t0_space3 : register(t0, space3);

Texture2D<float4> t1_space3 : register(t1, space3);

Texture3D<float4> t3_space3 : register(t3, space3);

cbuffer cb0_space3 : register(b0, space3) {
  UICompositingParameters__Constants UICompositingParameters_cbuffer_000 : packoffset(c000.x);
};

SamplerState s8_space98 : register(s8, space98);

SamplerState s0_space4 : register(s0, space4);

struct OutputSignature {
  float4 SV_Target : SV_Target;
  float4 SV_Target_1 : SV_Target1;
};

OutputSignature main(
  noperspective float4 SV_Position : SV_Position,
  linear float2 TEXCOORD : TEXCOORD
) {
  float4 SV_Target;
  float4 SV_Target_1;
  float4 _9 = t0_space3.Sample(s8_space98, float2(TEXCOORD.x, TEXCOORD.y));
  float4 _14 = t1_space3.Sample(s8_space98, float2(TEXCOORD.x, TEXCOORD.y));
  float _19 = 1.0f - _9.w;
  float _22 = min(_14.x, UICompositingParameters_cbuffer_000.UICompositingParameters__Constants_076);
  float _23 = min(_14.y, UICompositingParameters_cbuffer_000.UICompositingParameters__Constants_076);
  float _24 = min(_14.z, UICompositingParameters_cbuffer_000.UICompositingParameters__Constants_076);
  float _26 = 1.0f / max(_9.w, 1.0000000116860974e-07f);
  float4 _39 = t3_space3.SampleLevel(s0_space4, float3(((saturate(_26 * _9.x) * 0.96875f) + 0.015625f), ((saturate(_26 * _9.y) * 0.96875f) + 0.015625f), ((saturate(_26 * _9.z) * 0.96875f) + 0.015625f)), 0.0f);
  SV_Target.x = ((_39.x * _9.w) + (_22 * _19));
  SV_Target.y = ((_39.y * _9.w) + (_23 * _19));
  SV_Target.z = ((_39.z * _9.w) + (_24 * _19));
  SV_Target.w = ((_14.w * _19) + _9.w);
  SV_Target_1.x = _22;
  SV_Target_1.y = _23;
  SV_Target_1.z = _24;
  SV_Target_1.w = _14.w;
  OutputSignature output_signature = { SV_Target, SV_Target_1 };
  return output_signature;
}
