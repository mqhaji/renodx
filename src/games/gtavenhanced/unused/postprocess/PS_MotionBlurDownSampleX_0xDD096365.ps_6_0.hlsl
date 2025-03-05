cbuffer _13_15 : register(b12, space1)
{
    float4 _15_m0[95] : packoffset(c0);
};

Texture2D<float4> _8 : register(t22, space1);
SamplerState _18 : register(s0, space1);

static float4 gl_FragCoord;
static float2 TEXCOORD;
static float4 SV_Target;

struct SPIRV_Cross_Input
{
    float2 TEXCOORD : TEXCOORD1;
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 SV_Target : SV_Target0;
};

void frag_main()
{
    uint _36 = uint(int(gl_FragCoord.x)) << 4u;
    float _48 = float(int(uint(int(gl_FragCoord.y)))) * _15_m0[72u].y;
    float4 _52 = _8.SampleLevel(_18, float2(float(int(_36)) * _15_m0[72u].x, _48), 0.0f);
    float _54 = _52.x;
    float _55 = _52.y;
    float _60 = sqrt((_54 * _54) + (_55 * _55));
    bool _62 = _60 > (-1.0f);
    float _66 = _62 ? _60 : (-1.0f);
    float4 _72 = _8.SampleLevel(_18, float2(_15_m0[72u].x * float(int(_36 | 1u)), _48), 0.0f);
    float _74 = _72.x;
    float _75 = _72.y;
    float _79 = sqrt((_74 * _74) + (_75 * _75));
    bool _80 = _79 > _66;
    float _83 = _80 ? _79 : _66;
    float4 _89 = _8.SampleLevel(_18, float2(_15_m0[72u].x * float(int(_36 | 2u)), _48), 0.0f);
    float _91 = _89.x;
    float _92 = _89.y;
    float _96 = sqrt((_91 * _91) + (_92 * _92));
    bool _97 = _96 > _83;
    float _100 = _97 ? _96 : _83;
    float4 _106 = _8.SampleLevel(_18, float2(_15_m0[72u].x * float(int(_36 | 3u)), _48), 0.0f);
    float _108 = _106.x;
    float _109 = _106.y;
    float _113 = sqrt((_108 * _108) + (_109 * _109));
    bool _114 = _113 > _100;
    float _117 = _114 ? _113 : _100;
    float4 _122 = _8.SampleLevel(_18, float2(_15_m0[72u].x * float(int(_36 | 4u)), _48), 0.0f);
    float _124 = _122.x;
    float _125 = _122.y;
    float _129 = sqrt((_124 * _124) + (_125 * _125));
    bool _130 = _129 > _117;
    float _133 = _130 ? _129 : _117;
    float4 _139 = _8.SampleLevel(_18, float2(_15_m0[72u].x * float(int(_36 | 5u)), _48), 0.0f);
    float _141 = _139.x;
    float _142 = _139.y;
    float _146 = sqrt((_141 * _141) + (_142 * _142));
    bool _147 = _146 > _133;
    float _150 = _147 ? _146 : _133;
    float4 _156 = _8.SampleLevel(_18, float2(_15_m0[72u].x * float(int(_36 | 6u)), _48), 0.0f);
    float _158 = _156.x;
    float _159 = _156.y;
    float _163 = sqrt((_158 * _158) + (_159 * _159));
    bool _164 = _163 > _150;
    float _167 = _164 ? _163 : _150;
    float4 _173 = _8.SampleLevel(_18, float2(_15_m0[72u].x * float(int(_36 | 7u)), _48), 0.0f);
    float _175 = _173.x;
    float _176 = _173.y;
    float _180 = sqrt((_175 * _175) + (_176 * _176));
    bool _181 = _180 > _167;
    float _184 = _181 ? _180 : _167;
    float4 _190 = _8.SampleLevel(_18, float2(_15_m0[72u].x * float(int(_36 | 8u)), _48), 0.0f);
    float _192 = _190.x;
    float _193 = _190.y;
    float _197 = sqrt((_192 * _192) + (_193 * _193));
    bool _198 = _197 > _184;
    float _201 = _198 ? _197 : _184;
    float4 _207 = _8.SampleLevel(_18, float2(_15_m0[72u].x * float(int(_36 | 9u)), _48), 0.0f);
    float _209 = _207.x;
    float _210 = _207.y;
    float _214 = sqrt((_209 * _209) + (_210 * _210));
    bool _215 = _214 > _201;
    float _218 = _215 ? _214 : _201;
    float4 _224 = _8.SampleLevel(_18, float2(_15_m0[72u].x * float(int(_36 | 10u)), _48), 0.0f);
    float _226 = _224.x;
    float _227 = _224.y;
    float _231 = sqrt((_226 * _226) + (_227 * _227));
    bool _232 = _231 > _218;
    float _235 = _232 ? _231 : _218;
    float4 _241 = _8.SampleLevel(_18, float2(_15_m0[72u].x * float(int(_36 | 11u)), _48), 0.0f);
    float _243 = _241.x;
    float _244 = _241.y;
    float _248 = sqrt((_243 * _243) + (_244 * _244));
    bool _249 = _248 > _235;
    float _252 = _249 ? _248 : _235;
    float4 _258 = _8.SampleLevel(_18, float2(_15_m0[72u].x * float(int(_36 | 12u)), _48), 0.0f);
    float _260 = _258.x;
    float _261 = _258.y;
    float _265 = sqrt((_260 * _260) + (_261 * _261));
    bool _266 = _265 > _252;
    float _269 = _266 ? _265 : _252;
    float4 _275 = _8.SampleLevel(_18, float2(_15_m0[72u].x * float(int(_36 | 13u)), _48), 0.0f);
    float _277 = _275.x;
    float _278 = _275.y;
    float _282 = sqrt((_277 * _277) + (_278 * _278));
    bool _283 = _282 > _269;
    float _286 = _283 ? _282 : _269;
    float4 _292 = _8.SampleLevel(_18, float2(_15_m0[72u].x * float(int(_36 | 14u)), _48), 0.0f);
    float _294 = _292.x;
    float _295 = _292.y;
    float _299 = sqrt((_294 * _294) + (_295 * _295));
    bool _300 = _299 > _286;
    float _303 = _300 ? _299 : _286;
    float4 _309 = _8.SampleLevel(_18, float2(_15_m0[72u].x * float(int(_36 | 15u)), _48), 0.0f);
    float _311 = _309.x;
    float _312 = _309.y;
    float _316 = sqrt((_311 * _311) + (_312 * _312));
    bool _317 = _316 > _303;
    SV_Target.x = _317 ? _311 : (_300 ? _294 : (_283 ? _277 : (_266 ? _260 : (_249 ? _243 : (_232 ? _226 : (_215 ? _209 : (_198 ? _192 : (_181 ? _175 : (_164 ? _158 : (_147 ? _141 : (_130 ? _124 : (_114 ? _108 : (_97 ? _91 : (_80 ? _74 : (_62 ? _54 : 0.0f)))))))))))))));
    SV_Target.y = _317 ? _312 : (_300 ? _295 : (_283 ? _278 : (_266 ? _261 : (_249 ? _244 : (_232 ? _227 : (_215 ? _210 : (_198 ? _193 : (_181 ? _176 : (_164 ? _159 : (_147 ? _142 : (_130 ? _125 : (_114 ? _109 : (_97 ? _92 : (_80 ? _75 : (_62 ? _55 : 0.0f)))))))))))))));
    SV_Target.z = _317 ? _316 : _303;
    SV_Target.w = min(min(min(min(min(min(min(min(min(min(min(min(min(min(min(min(100000.0f, _60), _79), _96), _113), _129), _146), _163), _180), _197), _214), _231), _248), _265), _282), _299), _316);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    TEXCOORD = stage_input.TEXCOORD;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.SV_Target = SV_Target;
    return stage_output;
}
