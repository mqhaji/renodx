static const float _37[3] = { 0.0f, 1.3846199512481689453125f, 3.230770111083984375f };
static const float _43[3] = { 0.22702999413013458251953125f, 0.316219985485076904296875f, 0.070270001888275146484375f };

cbuffer _13_15 : register(b3, space0)
{
    float4 _15_m0[6] : packoffset(c0);
};

cbuffer _18_20 : register(b12, space1)
{
    float4 _20_m0[95] : packoffset(c0);
};

Texture2D<float4> _8 : register(t15, space1);
SamplerState _24[] : register(s0, space2);

static float2 TEXCOORD;
static float4 SV_Target;

struct SPIRV_Cross_Input
{
    float2 TEXCOORD : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    float4 SV_Target : SV_Target0;
};

void frag_main()
{
    float4 _65 = _8.Sample(_24[asuint(_15_m0[0u]).w + 0u], float2(TEXCOORD.x, TEXCOORD.y));
    float _75;
    float _77;
    float _79;
    float _81;
    uint _83;
    _75 = _65.x * 0.22702999413013458251953125f;
    _77 = _65.y * 0.22702999413013458251953125f;
    _79 = _65.z * 0.22702999413013458251953125f;
    _81 = _65.w * 0.22702999413013458251953125f;
    _83 = 1u;
    float _76;
    float _78;
    float _80;
    float _82;
    for (;;)
    {
        float4 _102 = _8.Sample(_24[asuint(_15_m0[0u]).w + 0u], float2(TEXCOORD.x, (_20_m0[71u].y * _37[_83]) + TEXCOORD.y));
        float4 _131 = _8.Sample(_24[asuint(_15_m0[0u]).w + 0u], float2(TEXCOORD.x, TEXCOORD.y - (_20_m0[71u].y * _37[_83])));
        _76 = ((_102.x * _43[_83]) + _75) + (_131.x * _43[_83]);
        _78 = ((_102.y * _43[_83]) + _77) + (_131.y * _43[_83]);
        _80 = ((_102.z * _43[_83]) + _79) + (_131.z * _43[_83]);
        _82 = ((_102.w * _43[_83]) + _81) + (_131.w * _43[_83]);
        uint _84 = _83 + 1u;
        if (_84 == 3u)
        {
            break;
        }
        else
        {
            _75 = _76;
            _77 = _78;
            _79 = _80;
            _81 = _82;
            _83 = _84;
        }
    }
    SV_Target.x = _76;
    SV_Target.y = _78;
    SV_Target.z = _80;
    SV_Target.w = _82;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    TEXCOORD = stage_input.TEXCOORD;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.SV_Target = SV_Target;
    return stage_output;
}
