cbuffer _14_16 : register(b12, space1)
{
    float4 _16_m0[95] : packoffset(c0);
};

Texture2D<float4> _8 : register(t9, space1);
Texture2D<float4> _9 : register(t25, space1);
SamplerState _19 : register(s0, space1);
SamplerState _20 : register(s2, space1);

static float4 TEXCOORD;
static float4 SV_Target;

struct SPIRV_Cross_Input
{
    float4 TEXCOORD : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    float4 SV_Target : SV_Target0;
};

void frag_main()
{
    float _37;
    float _40;
    float _42;
    uint _44;
    _37 = 0.0f;
    _40 = 0.0f;
    _42 = 0.0f;
    _44 = 4294967295u;
    float _39;
    float _41;
    float _43;
    for (;;)
    {
        float _56 = (_16_m0[72u].y * float(int(_44))) + TEXCOORD.y;
        float4 _59 = _9.Sample(_20, float2(TEXCOORD.x - _16_m0[72u].x, _56));
        float4 _72 = _9.Sample(_20, float2(TEXCOORD.x, _56));
        float4 _84 = _9.Sample(_20, float2(_16_m0[72u].x + TEXCOORD.x, _56));
        _39 = (_84.x * 0.111111111938953399658203125f) + ((_72.x * 0.111111111938953399658203125f) + ((_59.x * 0.111111111938953399658203125f) + _37));
        _41 = (_84.y * 0.111111111938953399658203125f) + ((_72.y * 0.111111111938953399658203125f) + ((_59.y * 0.111111111938953399658203125f) + _40));
        _43 = (_84.z * 0.111111111938953399658203125f) + ((_72.z * 0.111111111938953399658203125f) + ((_59.z * 0.111111111938953399658203125f) + _42));
        uint _46 = _44 + 1u;
        if (_46 == 2u)
        {
            break;
        }
        else
        {
            _37 = _39;
            _40 = _41;
            _42 = _43;
            _44 = _46;
        }
    }
    float4 _96 = _8.Sample(_19, 0.0f.xx);
    float _98 = _96.x;
    float _115 = max(9.999999717180685365747194737196e-10f, dot(float3((_98 * _39) * _16_m0[8u].w, (_98 * _41) * _16_m0[8u].w, (_98 * _43) * _16_m0[8u].w), 0.333333313465118408203125f.xxx));
    float _125 = clamp((_115 - _16_m0[7u].x) * _16_m0[7u].z, 0.0f, 1.0f);
    SV_Target.x = (isnan(_39) || isinf(_39)) ? 0.0f : (_125 * max(_39, 0.0f));
    SV_Target.y = (isnan(_41) || isinf(_41)) ? 0.0f : (_125 * max(_41, 0.0f));
    SV_Target.z = (isnan(_43) || isinf(_43)) ? 0.0f : (_125 * max(_43, 0.0f));
    SV_Target.w = 0.0f;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    TEXCOORD = stage_input.TEXCOORD;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.SV_Target = SV_Target;
    return stage_output;
}
