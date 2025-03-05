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
    float _40;
    float _43;
    float _45;
    uint _47;
    _40 = 0.0f;
    _43 = 0.0f;
    _45 = 0.0f;
    _47 = 0u;
    float _42;
    float _44;
    float _46;
    for (;;)
    {
        float _49 = float(int(_47));
        float _52 = 1.0f - (_49 * 0.010416666977107524871826171875f);
        float _54 = _52 * _52;
        float4 _83 = _8.Sample(_24[asuint(_15_m0[0u]).w + 0u], float2(((_20_m0[42u].x * _49) * _20_m0[72u].x) + TEXCOORD.x, ((_20_m0[42u].y * _49) * _20_m0[72u].y) + TEXCOORD.y));
        _42 = (_83.x * _54) + _40;
        _44 = (_83.y * _54) + _43;
        _46 = (_83.z * _54) + _45;
        uint _48 = _47 + 1u;
        if (_48 == 96u)
        {
            break;
        }
        else
        {
            _40 = _42;
            _43 = _44;
            _45 = _46;
            _47 = _48;
        }
    }
    float _125 = (((_20_m0[35u].w - _20_m0[35u].z) * 2.0f) * ((_20_m0[35u].y > 0.0f) ? abs(TEXCOORD.y + (-0.5f)) : abs(TEXCOORD.x + (-0.5f)))) + _20_m0[35u].z;
    float _126 = _125 * _125;
    SV_Target.x = ((_42 * 0.0416666679084300994873046875f) * _20_m0[40u].x) * _126;
    SV_Target.y = ((_44 * 0.0416666679084300994873046875f) * _20_m0[40u].y) * _126;
    SV_Target.z = ((_46 * 0.0416666679084300994873046875f) * _20_m0[40u].z) * _126;
    SV_Target.w = 1.0f;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    TEXCOORD = stage_input.TEXCOORD;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.SV_Target = SV_Target;
    return stage_output;
}
