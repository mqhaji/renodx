cbuffer _20_22 : register(b2, space0) {
  float4 _22_m0[56] : packoffset(c0);
};

cbuffer _25_27 : register(b3, space0) {
  float4 _27_m0[6] : packoffset(c0);
};

cbuffer _30_32 : register(b5, space0) {
  float4 _32_m0[26] : packoffset(c0);
};

cbuffer _35_37 : register(b6, space0) {
  float4 _37_m0[60] : packoffset(c0);
};

cbuffer _40_42 : register(b7, space0) {
  float4 _42_m0[11] : packoffset(c0);
};

cbuffer _45_47 : register(b10, space1) {
  float4 _47_m0[3] : packoffset(c0);
};

Texture2D<float4> _8 : register(t0, space1);
Texture2D<float4> _9 : register(t1, space0);
Texture2D<float4> _10 : register(t2, space0);
Texture3D<float4> _13 : register(t7, space0);
Texture2D<float4> _14 : register(t14, space1);
Texture2D<float4> _15 : register(t15, space1);
SamplerState _50 : register(s2, space1);
SamplerState _53[] : register(s0, space2);

static float4 gl_FragCoord;
static float4 COLOR;
static float2 TEXCOORD;
static float3 TEXCOORD_1;
static float3 TEXCOORD_3;
static float3 TEXCOORD_4;
static float3 TEXCOORD_5;
static float4 TEXCOORD_6;
static float4 SV_Target;

struct SPIRV_Cross_Input {
  centroid float4 COLOR : TEXCOORD0;
  float2 TEXCOORD : TEXCOORD1;
  float3 TEXCOORD_1 : TEXCOORD2;
  float3 TEXCOORD_3 : TEXCOORD3;
  float3 TEXCOORD_4 : TEXCOORD4;
  float3 TEXCOORD_5 : TEXCOORD5;
  float4 TEXCOORD_6 : TEXCOORD6;
  float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output {
  float4 SV_Target : SV_Target0;
};

uint2 spvTextureSize(Texture2D<float4> Tex, uint Level, out uint Param) {
  uint2 ret;
  Tex.GetDimensions(Level, ret.x, ret.y, Param);
  return ret;
}

void frag_main() {
  float4 _148 = _8.Sample(_53[asuint(_27_m0[0u]).x + 0u], float2(TEXCOORD.x, TEXCOORD.y));
  float _150 = _148.x;
  float _151 = _148.y;
  float _152 = _148.z;
  float4 _155 = _14.Sample(_53[asuint(_27_m0[3u]).w + 0u], float2(TEXCOORD.x, TEXCOORD.y));
  float _165 = (_155.x * 2.0f) + (-1.0f);
  float _167 = (_155.y * 2.0f) + (-1.0f);
  float _175 = sqrt(abs(1.0f - dot(float2(_165, _167), float2(_165, _167))));
  float _176 = max(0.001000000047497451305389404296875f, _47_m0[1u].w);
  float _178 = _176 * _165;
  float _179 = _176 * _167;
  float _190 = ((_179 * TEXCOORD_5.x) + (_175 * TEXCOORD_1.x)) + (_178 * TEXCOORD_4.x);
  float _192 = ((_179 * TEXCOORD_5.y) + (_175 * TEXCOORD_1.y)) + (_178 * TEXCOORD_4.y);
  float _194 = ((_179 * TEXCOORD_5.z) + (_175 * TEXCOORD_1.z)) + (_178 * TEXCOORD_4.z);
  float _198 = rsqrt(dot(float3(_190, _192, _194), float3(_190, _192, _194)));
  float _199 = _190 * _198;
  float _200 = _192 * _198;
  float _201 = _194 * _198;
  float4 _206 = _15.Sample(_53[asuint(_27_m0[1u]).w + 0u], float2(TEXCOORD.x, TEXCOORD.y));
  float _208 = _206.x;
  float _209 = _206.y;
  float _227 = _47_m0[0u].y * _206.w;
  float _234 = _32_m0[12u].z * COLOR.x;
  float _235 = _32_m0[12u].y * COLOR.y;
  float _240 = _150 * _150;
  float _241 = _151 * _151;
  float _242 = _152 * _152;
  float _253 = (-0.0f) - _37_m0[0u].x;
  float _255 = (-0.0f) - _37_m0[0u].y;
  float _256 = (-0.0f) - _37_m0[0u].z;
  float _263 = _22_m0[15u].x - TEXCOORD_6.x;
  float _264 = _22_m0[15u].y - TEXCOORD_6.y;
  float _265 = _22_m0[15u].z - TEXCOORD_6.z;
  float _269 = rsqrt(dot(float3(_263, _264, _265), float3(_263, _264, _265)));
  float _270 = _263 * _269;
  float _271 = _264 * _269;
  float _272 = _265 * _269;
  float _273 = _270 - _37_m0[0u].x;
  float _274 = _271 - _37_m0[0u].y;
  float _275 = _272 - _37_m0[0u].z;
  float _279 = rsqrt(dot(float3(_273, _274, _275), float3(_273, _274, _275)));
  float _280 = _273 * _279;
  float _281 = _274 * _279;
  float _282 = _275 * _279;
  float _283 = clamp(_47_m0[0u].z * dot(float3(_208 * _208, _209 * _209, _206.z), float3(_47_m0[1u].xyz)), 0.0f, 1.0f);
  float _284 = _234 * _234;
  float _285 = _235 * _235;
  float _288 = max(0.0f, _227 + (-500.0f));
  float _294 = ((_227 - _288) * 3.0f) + (_288 * 558.0f);
  float _298 = clamp(dot(float3(_199, _200, _201), float3(_253, _255, _256)), 0.0f, 1.0f);
  float _307 = 1.0f - clamp(dot(float3(_270, _271, _272), float3(_199, _200, _201)), 0.0f, 1.0f);
  float _308 = 1.0f - clamp(dot(float3(_280, _281, _282), float3(_253, _255, _256)), 0.0f, 1.0f);
  float _309 = 1.0f - _47_m0[0u].x;
  precise float _310 = _307 * _307;
  precise float _311 = _310 * _310;
  precise float _314 = _308 * _308;
  precise float _315 = _314 * _314;
  float _321 = (_309 + ((_307 * _47_m0[0u].x) * _311)) * _283;
  float _322 = 1.0f - _321;
  float _338 = (_298 * _283) * ((((_294 + 2.0f) * 0.125f) * exp2(log2(clamp(dot(float3(_199, _200, _201), float3(_280, _281, _282)) + 9.9999999392252902907785028219223e-09f, 0.0f, 1.0f)) * (_294 + 9.9999999392252902907785028219223e-09f))) * (_309 + ((_308 * _47_m0[0u].x) * _315)));
  float _339 = _322 * _298;
  float _359 = max(0.0f, (_37_m0[43u].w + _201) * _37_m0[44u].w);
  float _378 = 1.0f - _32_m0[13u].z;
  float _427 = clamp(dot(float3(_37_m0[46u].w, _37_m0[47u].w, _37_m0[48u].w), float3(_199, _200, _201)), 0.0f, 1.0f);
  float _456 = (-0.0f) - _270;
  float _457 = (-0.0f) - _271;
  float _458 = (-0.0f) - _272;
  uint _463 = asuint(_27_m0[1u]).x + 0u;
  float _472 = dot(float3(_456, _457, _458), float3(_199, _200, _201)) * 2.0f;
  float _476 = _456 - (_472 * _199);
  float _478 = _458 - (_472 * _201);
  float _482 = 1.0f - clamp(_294 * 0.000666666659526526927947998046875f, 0.0f, 1.0f);
  float _483 = _42_m0[6u].z + (-5.0f);
  float _485 = _482 * _42_m0[6u].z;
  float _493 = (_485 < _483) ? (_485 + (-5.0f)) : (((_482 * _482) * 5.0f) + _483);
  float _501 = abs(_478) + 1.0f;
  float _509 = (_478 > 0.0f) ? (0.75f - ((_476 * (-0.25f)) / _501)) : (0.25f - ((_476 * 0.25f) / _501));
  precise float _510 = 0.5f - (((_457 - (_472 * _200)) * 0.5f) / _501);
  float _541;
  float _542;
  float _543;
  if (_493 < 1.0f) {
    float4 _513 = _9.SampleLevel(_53[_463], float2(_509, _510), _493);
    _541 = _513.x;
    _542 = _513.y;
    _543 = _513.z;
  } else {
    float _519 = max(_493 + (-1.0f), 0.0f);
    uint _522_dummy_parameter;
    uint2 _522 = spvTextureSize(_9, uint(_519), _522_dummy_parameter);
    float4 _536 = _9.SampleLevel(_53[_463], float2((0.25f / float(int(uint(int(float(int(_522.x))))))) + _509, (0.25f / float(int(uint(int(float(int(_522.y))))))) + _510), _519);
    _541 = _536.x;
    _542 = _536.y;
    _543 = _536.z;
  }
  float _549 = (clamp(_294 * 0.001776198972947895526885986328125f, 0.0f, 1.0f) * 0.68169009685516357421875f) + 0.3183098733425140380859375f;
  float _551 = max(_284, _285) * _321;
  float _558 = (((_322 * _240) * (((((_37_m0[43u].x * _359) + _37_m0[44u].x) + (_37_m0[49u].x * _427)) * _284) + (((((_37_m0[45u].x * _359) + _37_m0[46u].x) * _32_m0[13u].z) + (((_37_m0[47u].x * _359) + _37_m0[48u].x) * _378)) * _285))) + ((_338 + (_339 * _240)) * _37_m0[1u].x)) + ((_551 * _541) * _549);
  float _559 = (((_322 * _241) * (((((_37_m0[43u].y * _359) + _37_m0[44u].y) + (_37_m0[49u].y * _427)) * _284) + (((((_37_m0[45u].y * _359) + _37_m0[46u].y) * _32_m0[13u].z) + (((_37_m0[47u].y * _359) + _37_m0[48u].y) * _378)) * _285))) + ((_338 + (_339 * _241)) * _37_m0[1u].y)) + ((_551 * _542) * _549);
  float _560 = (((_322 * _242) * (((((_37_m0[43u].z * _359) + _37_m0[44u].z) + (_37_m0[49u].z * _427)) * _284) + (((((_37_m0[45u].z * _359) + _37_m0[46u].z) * _32_m0[13u].z) + (((_37_m0[47u].z * _359) + _37_m0[48u].z) * _378)) * _285))) + ((_338 + (_339 * _242)) * _37_m0[1u].z)) + ((_551 * _543) * _549);
  float _576 = TEXCOORD_6.x - _22_m0[15u].x;
  float _577 = TEXCOORD_6.y - _22_m0[15u].y;
  float _578 = TEXCOORD_6.z - _22_m0[15u].z;
  float _584 = sqrt(((_576 * _576) + (_577 * _577)) + (_578 * _578));
  float _590 = max(0.0f, _584 - _37_m0[50u].x);
  float _592 = (_590 / _584) * _578;
  float _597 = _37_m0[52u].z * _592;
  float _620 = _37_m0[52u].y * (1.0f - clamp(exp2(min(1.0f, (((abs(_592) > 0.00999999977648258209228515625f) ? ((1.0f - exp2(_597 * (-1.44269502162933349609375f))) / _597) : 1.0f) * _590) * _37_m0[51u].w) * 1.44269502162933349609375f), 0.0f, 1.0f));
  float _631 = rsqrt(dot(float3(_576, _577, _578), float3(_576, _577, _578)));
  float _632 = _631 * _576;
  float _633 = _631 * _577;
  float _634 = _631 * _578;
  float _641 = exp2(log2(clamp(dot(float3(_632, _633, _634), float3(_37_m0[54u].xyz)), 0.0f, 1.0f)) * _37_m0[54u].w);
  float _655 = exp2(log2(clamp(dot(float3(_632, _633, _634), float3(_37_m0[53u].xyz)), 0.0f, 1.0f)) * _37_m0[53u].w);
  float _658 = _37_m0[51u].y * (1.0f - _620);
  float _669 = clamp(((1.0f - exp2((_37_m0[51u].x * 1.44269502162933349609375f) * max(0.0f, _590 - _37_m0[52u].x))) * _658) + _620, 0.0f, 1.0f);
  float _674 = 1.0f - exp2((_590 * (-1.44269502162933349609375f)) * _37_m0[51u].z);
  float _692 = ((_37_m0[58u].x - _37_m0[56u].x) * _641) + _37_m0[56u].x;
  float _693 = ((_37_m0[58u].y - _37_m0[56u].y) * _641) + _37_m0[56u].y;
  float _694 = ((_37_m0[58u].z - _37_m0[56u].z) * _641) + _37_m0[56u].z;
  float _722 = (((_692 - _37_m0[57u].x) + ((_37_m0[55u].x - _692) * _655)) * _674) + _37_m0[57u].x;
  float _723 = (((_693 - _37_m0[57u].y) + ((_37_m0[55u].y - _693) * _655)) * _674) + _37_m0[57u].y;
  float _724 = (((_694 - _37_m0[57u].z) + ((_37_m0[55u].z - _694) * _655)) * _674) + _37_m0[57u].z;
  float _731 = (_37_m0[55u].w - _722) * _658;
  float _732 = (_37_m0[56u].w - _723) * _658;
  float _733 = (_37_m0[57u].w - _724) * _658;
  float _748;
  float _750;
  float _752;
  if (asuint(_42_m0[7u]).x == 0u) {
    _748 = (_722 - _558) + _731;
    _750 = (_723 - _559) + _732;
    _752 = (_724 - _560) + _733;
  } else {
    float _790;
    if (_32_m0[19u].y > 0.0f) {
      _790 = clamp((_32_m0[19u].y * (_10.Sample(_53[asuint(_27_m0[1u]).x + 0u], float2(_32_m0[15u].z * gl_FragCoord.x, _32_m0[15u].w * gl_FragCoord.y)).x + (-1.0f))) + 1.0f, 0.0f, 1.0f);
    } else {
      _790 = 1.0f;
    }
    _748 = (_790 * (_731 + _722)) - _558;
    _750 = (_790 * (_732 + _723)) - _559;
    _752 = (_790 * (_733 + _724)) - _560;
  }
  precise float _754 = _752 * _669;
  precise float _755 = _750 * _669;
  precise float _756 = _748 * _669;
  float _757 = _756 + _558;
  float _758 = _755 + _559;
  float _759 = _754 + _560;
  float _794;
  float _796;
  float _798;
  if (asuint(_32_m0[25u].y) == 0u) {
    _794 = _757;
    _796 = _758;
    _798 = _759;
  } else {
    float4 _817 = _13.SampleLevel(_50, float3(_757, _758, _759), 0.0f);
    _794 = _817.x;
    _796 = _817.y;
    _798 = _817.z;
  }
  SV_Target.x = exp2(log2(_794) * 0.454545438289642333984375f);
  SV_Target.y = exp2(log2(_796) * 0.454545438289642333984375f);
  SV_Target.z = exp2(log2(_798) * 0.454545438289642333984375f);
  SV_Target.w = (_148.w * COLOR.w) * _32_m0[12u].x;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input) {
  gl_FragCoord = stage_input.gl_FragCoord;
  gl_FragCoord.w = 1.0 / gl_FragCoord.w;
  COLOR = stage_input.COLOR;
  TEXCOORD = stage_input.TEXCOORD;
  TEXCOORD_1 = stage_input.TEXCOORD_1;
  TEXCOORD_3 = stage_input.TEXCOORD_3;
  TEXCOORD_4 = stage_input.TEXCOORD_4;
  TEXCOORD_5 = stage_input.TEXCOORD_5;
  TEXCOORD_6 = stage_input.TEXCOORD_6;
  frag_main();
  SPIRV_Cross_Output stage_output;
  stage_output.SV_Target = SV_Target;
  return stage_output;
}
