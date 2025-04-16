cbuffer _17_19 : register(b14, space0) {
  float4 _19_m0[1] : packoffset(c0);
};

cbuffer _22_24 : register(b0, space0) {
  float4 _24_m0[19] : packoffset(c0);
};

Texture2D<float4> _9[] : register(t0, space0);
Buffer<uint4> _13 : register(t1, space1);
SamplerState _27 : register(s0, space0);
SamplerState _28 : register(s1, space0);

static float3 TEXCOORD_3;
static float2 TEXCOORD;
static float4 COLOR;
static float4 TEXCOORD_1;
static float3 TEXCOORD_2;
static float4 SV_Target;

struct SPIRV_Cross_Input {
  float3 TEXCOORD_3 : TEXCOORD0;
  float2 TEXCOORD : TEXCOORD1;
  float4 COLOR : TEXCOORD2;
  nointerpolation float4 TEXCOORD_1 : TEXCOORD3;
  float3 TEXCOORD_2 : TEXCOORD4;
};

struct SPIRV_Cross_Output {
  float4 SV_Target : SV_Target0;
};

void frag_main() {
  SV_Target.x = 0.0f;
  SV_Target.y = 0.0f;
  SV_Target.z = 0.0f;
  SV_Target.w = 0.0f;
  uint4 _86 = asuint(_19_m0[0u]);
  uint _87 = _86.x;
  float _98 = asfloat(_13.Load((_87 + 296u) >> 2u).x);
  float _104 = asfloat(_13.Load((_87 + 300u) >> 2u).x);
  uint _107 = (_87 + 304u) >> 2u;
  float4 _120 = asfloat(uint4(_13.Load(_107).x, _13.Load(_107 + 1u).x, _13.Load(_107 + 2u).x, _13.Load(_107 + 3u).x));
  uint _127 = (_87 + 320u) >> 2u;
  float2 _135 = asfloat(uint2(_13.Load(_127).x, _13.Load(_127 + 1u).x));
  uint _140 = (_87 + 328u) >> 2u;
  float4 _153 = asfloat(uint4(_13.Load(_140).x, _13.Load(_140 + 1u).x, _13.Load(_140 + 2u).x, _13.Load(_140 + 3u).x));
  float _154 = _153.x;
  float _155 = _153.y;
  float _156 = _153.z;
  float _163 = asfloat(_13.Load((_87 + 344u) >> 2u).x);
  float _171 = clamp(_135.x * TEXCOORD_3.x, 0.0f, 1.0f);
  float _173 = min(_98, _104);
  float _174 = max(_98, _104);
  float _175 = _174 - _173;
  float _182 = min(min(0.0500000007450580596923828125f, 0.5f - abs(0.5f - _174)), _175);
  float _199 = min(min(0.0500000007450580596923828125f, 0.5f - abs(0.5f - _173)), _175);
  float4 _225 = _9[_13.Load((_87 + 96u) >> 2u).x].Sample(_28, float2((_171 * _120.x) + _120.z, (clamp(_135.y * TEXCOORD_3.y, 0.0f, 1.0f) * _120.y) + _120.w));
  float4 _235 = _9[_13.Load(_87 >> 2u).x].Sample(_27, float2(TEXCOORD.x, TEXCOORD.y));
  float4 _244 = _9[_13.Load((_87 + 32u) >> 2u).x].Sample(_27, float2(TEXCOORD.x, TEXCOORD.y));
  float _262;
  if (TEXCOORD_1.w > 0.5f) {
    _262 = _9[_13.Load((_87 + 64u) >> 2u).x].Sample(_27, float2(TEXCOORD.x, TEXCOORD.y)).w;
  } else {
    _262 = _235.w;
  }
  float _263 = ddx_coarse(TEXCOORD.x);
  float _264 = ddy_coarse(TEXCOORD.x);
  float _269 = ddx_coarse(TEXCOORD.y);
  float _270 = ddy_coarse(TEXCOORD.y);
  float _280 = (TEXCOORD_1.z > 0.0f) ? ((sqrt(sqrt((_270 * _270) + (_269 * _269)) * sqrt((_264 * _264) + (_263 * _263))) * 0.5f) / TEXCOORD_1.z) : 0.0f;
  float _284 = clamp(0.5f - _280, 0.0f, 1.0f);
  float _288 = clamp((_262 - _284) / (clamp(_280 + 0.5f, 0.0f, 1.0f) - _284), 0.0f, 1.0f);
  float _293 = (_288 * _288) * (3.0f - (_288 * 2.0f));
  float _296 = ((1.0f - _293) * TEXCOORD_1.y) + _293;
  float _300;
  float _302;
  float _304;
  float _306;
  if (_163 < 0.0039215688593685626983642578125f) {
    _300 = COLOR.x;
    _302 = COLOR.y;
    _304 = COLOR.z;
    _306 = _296 * COLOR.w;
  } else {
    float _337 = _163 * (-0.4000000059604644775390625f);
    float _344 = clamp(_337 + 0.4000000059604644775390625f, 0.0f, 1.0f);
    float _348 = clamp((_262 - _344) / (clamp(_337 + 0.60000002384185791015625f, 0.0f, 1.0f) - _344), 0.0f, 1.0f);
    float _353 = ((_348 * _348) * _153.w) * (3.0f - (_348 * 2.0f));
    _300 = (_296 * (COLOR.x - _154)) + _154;
    _302 = (_296 * (COLOR.y - _155)) + _155;
    _304 = (_296 * (COLOR.z - _156)) + _156;
    _306 = ((COLOR.w - _353) * _296) + _353;
  }
  SV_Target.x = _24_m0[16u].z * ((((_244.x * COLOR.x) - _300) * TEXCOORD_1.x) + _300);
  SV_Target.y = _24_m0[16u].z * ((((_244.y * COLOR.y) - _302) * TEXCOORD_1.x) + _302);
  SV_Target.z = ((((_244.z * COLOR.z) - _304) * TEXCOORD_1.x) + _304) * _24_m0[16u].z;
  SV_Target.w = ((min((_199 <= 0.0f) ? ((_171 < _173) ? 0.0f : 1.0f) : clamp(1.0f - (((_173 - _171) + _199) / (_199 * 2.0f)), 0.0f, 1.0f), (_182 <= 0.0f) ? ((_174 < _171) ? 0.0f : 1.0f) : clamp(1.0f - (((_171 - _174) + _182) / (_182 * 2.0f)), 0.0f, 1.0f)) * max(TEXCOORD_2.z, clamp(min(TEXCOORD_2.x, TEXCOORD_2.y), 0.0f, 1.0f))) * _225.w) * ((((_244.w * COLOR.w) - _306) * TEXCOORD_1.x) + _306);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input) {
  TEXCOORD_3 = stage_input.TEXCOORD_3;
  TEXCOORD = stage_input.TEXCOORD;
  COLOR = stage_input.COLOR;
  TEXCOORD_1 = stage_input.TEXCOORD_1;
  TEXCOORD_2 = stage_input.TEXCOORD_2;
  frag_main();
  SPIRV_Cross_Output stage_output;
  stage_output.SV_Target = SV_Target;
  return stage_output;
}
