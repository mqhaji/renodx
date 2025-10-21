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

Texture3D<float4> t0_space5 : register(t0, space5);

cbuffer cb0_space3 : register(b0, space3) {
  UICompositingParameters__Constants UICompositingParameters_cbuffer_000 : packoffset(c000.x);
};

cbuffer cb0_space5 : register(b0, space5) {
  struct UICompositingCalibParameters__Constants {
    float2 UICompositingCalibParameters__Constants_000;
    float2 UICompositingCalibParameters__Constants_008;
  } UICompositingCalibParameters_cbuffer_000 : packoffset(c000.x);
};

SamplerState s8_space98 : register(s8, space98);

SamplerState s0_space4 : register(s0, space4);

float4 main(
  noperspective float4 SV_Position : SV_Position,
  linear float2 TEXCOORD : TEXCOORD
) : SV_Target {
  float4 SV_Target;
  float4 _11 = t0_space3.Sample(s8_space98, float2(TEXCOORD.x, TEXCOORD.y));
  float4 _16 = t1_space3.Sample(s8_space98, float2(TEXCOORD.x, TEXCOORD.y));
  float _21 = 1.0f - _11.w;
  float _28 = 1.0f / max(_11.w, 1.0000000116860974e-07f);
  float _32 = saturate(_28 * _11.x);
  float _33 = saturate(_28 * _11.y);
  float _34 = saturate(_28 * _11.z);
  float4 _41 = t3_space3.SampleLevel(s0_space4, float3(((_32 * 0.96875f) + 0.015625f), ((_33 * 0.96875f) + 0.015625f), ((_34 * 0.96875f) + 0.015625f)), 0.0f);
  float _128;
  float _129;
  float _130;
  if (((bool)(((bool)((bool)(TEXCOORD.x > UICompositingCalibParameters_cbuffer_000.UICompositingCalibParameters__Constants_000.x) && (bool)(TEXCOORD.y > UICompositingCalibParameters_cbuffer_000.UICompositingCalibParameters__Constants_000.y))) && (bool)(TEXCOORD.x < UICompositingCalibParameters_cbuffer_000.UICompositingCalibParameters__Constants_008.x))) && (bool)(TEXCOORD.y < UICompositingCalibParameters_cbuffer_000.UICompositingCalibParameters__Constants_008.y)) {
    float _93 = saturate(select((_32 <= 0.040449999272823334f), (_32 * 0.07739938050508499f), exp2(log2((_32 + 0.054999999701976776f) * 0.9478673338890076f) * 2.4000000953674316f)));
    float _94 = saturate(select((_33 <= 0.040449999272823334f), (_33 * 0.07739938050508499f), exp2(log2((_33 + 0.054999999701976776f) * 0.9478673338890076f) * 2.4000000953674316f)));
    float _95 = saturate(select((_34 <= 0.040449999272823334f), (_34 * 0.07739938050508499f), exp2(log2((_34 + 0.054999999701976776f) * 0.9478673338890076f) * 2.4000000953674316f)));
    float4 _123 = t0_space5.SampleLevel(s0_space4, float3(((saturate((log2(_93 / (0.6822916865348816f - (_93 * 0.6666666865348816f))) * 0.05000000074505806f) + 0.6236965656280518f) * 0.96875f) + 0.015625f), ((saturate((log2(_94 / (0.6822916865348816f - (_94 * 0.6666666865348816f))) * 0.05000000074505806f) + 0.6236965656280518f) * 0.96875f) + 0.015625f), ((saturate((log2(_95 / (0.6822916865348816f - (_95 * 0.6666666865348816f))) * 0.05000000074505806f) + 0.6236965656280518f) * 0.96875f) + 0.015625f)), 0.0f);
    _128 = _123.x;
    _129 = _123.y;
    _130 = _123.z;
  } else {
    _128 = ((_41.x * _11.w) + (min(_16.x, UICompositingParameters_cbuffer_000.UICompositingParameters__Constants_076) * _21));
    _129 = ((_41.y * _11.w) + (min(_16.y, UICompositingParameters_cbuffer_000.UICompositingParameters__Constants_076) * _21));
    _130 = ((_41.z * _11.w) + (min(_16.z, UICompositingParameters_cbuffer_000.UICompositingParameters__Constants_076) * _21));
  }
  SV_Target.x = _128;
  SV_Target.y = _129;
  SV_Target.z = _130;
  SV_Target.w = ((_16.w * _21) + _11.w);
  return SV_Target;
}
