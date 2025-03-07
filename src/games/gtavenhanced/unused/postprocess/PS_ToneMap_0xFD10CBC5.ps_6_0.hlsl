Texture2D<float4> t0 : register(t9, space1);

cbuffer cb0 : register(b5) {
  float cb0_014w : packoffset(c014.w);
};

cbuffer cb1 : register(b9, space1) {
  float cb1_002x : packoffset(c002.x);
  float cb1_002y : packoffset(c002.y);
  float cb1_002z : packoffset(c002.z);
  float cb1_003x : packoffset(c003.x);
  float cb1_003y : packoffset(c003.y);
  float cb1_003z : packoffset(c003.z);
  float cb1_003w : packoffset(c003.w);
  float cb1_005x : packoffset(c005.x);
  float cb1_005y : packoffset(c005.y);
  float cb1_005z : packoffset(c005.z);
  float cb1_005w : packoffset(c005.w);
  float cb1_006x : packoffset(c006.x);
  float cb1_006y : packoffset(c006.y);
  float cb1_006z : packoffset(c006.z);
};

SamplerState s0 : register(s0, space1);

float4 main(
  noperspective float4 SV_Position : SV_Position,
  linear float4 COLOR : COLOR,
  linear float2 TEXCOORD : TEXCOORD
) : SV_Target {
  float4 SV_Target;
  float4 _7 = t0.Sample(s0, float2((TEXCOORD.x), (TEXCOORD.y)));
  float _13 = (cb0_014w) * (_7.x);
  float _14 = (cb0_014w) * (_7.y);
  float _15 = (cb0_014w) * (_7.z);
  float _25 = max(0.0f, _13);
  float _26 = max(0.0f, _14);
  float _27 = max(0.0f, _15);
  float _28 = (cb1_002x) * _25;
  float _29 = _26 * (cb1_002x);
  float _30 = _27 * (cb1_002x);
  float _31 = _28 + (cb1_003x);
  float _32 = _29 + (cb1_003x);
  float _33 = _30 + (cb1_003x);
  float _34 = _31 * _25;
  float _35 = _32 * _26;
  float _36 = _33 * _27;
  float _37 = _34 + (cb1_003y);
  float _38 = _35 + (cb1_003y);
  float _39 = _36 + (cb1_003y);
  float _40 = _28 + (cb1_002y);
  float _41 = _29 + (cb1_002y);
  float _42 = _30 + (cb1_002y);
  float _43 = _40 * _25;
  float _44 = _41 * _26;
  float _45 = _42 * _27;
  float _46 = _43 + (cb1_003z);
  float _47 = _44 + (cb1_003z);
  float _48 = _45 + (cb1_003z);
  float _49 = _37 / _46;
  float _50 = _38 / _47;
  float _51 = _39 / _48;
  float _52 = _49 - (cb1_003w);
  float _53 = _50 - (cb1_003w);
  float _54 = _51 - (cb1_003w);
  float _55 = _52 * (cb1_002z);
  float _56 = _53 * (cb1_002z);
  float _57 = _54 * (cb1_002z);
  float _58 = saturate(_55);
  float _59 = saturate(_56);
  float _60 = saturate(_57);
  float _65 = dot(float3(_58, _59, _60), float3((cb1_005x), (cb1_005y), (cb1_005z)));
  float _70 = (cb1_006x) * _65;
  float _71 = (cb1_006y) * _65;
  float _72 = (cb1_006z) * _65;
  float _74 = saturate((cb1_005w));
  float _75 = _70 - _58;
  float _76 = _71 - _59;
  float _77 = _72 - _60;
  float _78 = _75 * _74;
  float _79 = _76 * _74;
  float _80 = _77 * _74;
  float _81 = _78 + _58;
  float _82 = _79 + _59;
  float _83 = _80 + _60;
  float _84 = log2(_81);
  float _85 = log2(_82);
  float _86 = log2(_83);
  float _87 = _84 * 0.45454543828964233f;
  float _88 = _85 * 0.45454543828964233f;
  float _89 = _86 * 0.45454543828964233f;
  float _90 = exp2(_87);
  float _91 = exp2(_88);
  float _92 = exp2(_89);
  SV_Target.x = _90;
  SV_Target.y = _91;
  SV_Target.z = _92;
  SV_Target.w = 1.0f;
  return SV_Target;
}
