cbuffer _15_17 : register(b12, space1)
{
    float4 _17_m0[95] : packoffset(c0);
};

Texture2D<float4> _8 : register(t21, space1);
Texture2D<float4> _9 : register(t23, space1);
Texture2D<float4> _10 : register(t24, space1);
SamplerState _20 : register(s0, space1);

static float4 TEXCOORD;
static float TEXCOORD_1;
static float4 SV_Target;

struct SPIRV_Cross_Input
{
    float4 TEXCOORD : TEXCOORD1;
    float TEXCOORD_1 : TEXCOORD2;
};

struct SPIRV_Cross_Output
{
    float4 SV_Target : SV_Target0;
};

void frag_main()
{
    float4 _49 = _9.SampleLevel(_20, float2(_17_m0[88u].x * TEXCOORD.x, _17_m0[88u].y * TEXCOORD.y), 0.0f);
    float _54 = _49.z * 0.5f;
    float _95;
    float _97;
    float _99;
    float _101;
    if ((_54 >= 1.0f) && ((_49.w * 0.5f) < 1000.0f))
    {
        float4 _68 = _10.SampleLevel(_20, float2(TEXCOORD.x, TEXCOORD.y), 0.0f);
        float _70 = _68.z;
        float _71 = _68.w;
        float _75 = min(_54, 1000.0f);
        uint _82 = uint(int(min(4.0f, _75 + 2.0f)));
        float _88 = _17_m0[72u].x * (_75 * ((_49.x * 0.5f) / _54));
        float _89 = _17_m0[72u].y * (_75 * ((_49.y * 0.5f) / _54));
        float _91 = float(int(_82)) + (-0.5f);
        float _93 = _91 / _75;
        float _110;
        float _112;
        float _114;
        float _116;
        if (int(_82) > int(0u))
        {
            float _120;
            float _121;
            float _122;
            float _123;
            uint _124;
            _120 = 0.0f;
            _121 = 0.0f;
            _122 = 0.0f;
            _123 = 0.0f;
            _124 = 0u;
            float _111;
            float _113;
            float _115;
            float _117;
            for (;;)
            {
                float _126 = float(int(_124));
                float _128 = (_126 + 0.5f) / _91;
                float _129 = _88 * _128;
                float _130 = _89 * _128;
                float _131 = _129 + TEXCOORD.x;
                float _132 = _130 + TEXCOORD.y;
                float4 _134 = _10.SampleLevel(_20, float2(_131, _132), 0.0f);
                float _136 = _134.z;
                float _137 = _134.w;
                float _145 = _136 - _70;
                float _151 = max(_126 + (-0.5f), 0.0f);
                float _154 = clamp((_93 * _71) - _151, 0.0f, 1.0f);
                float4 _161 = _8.SampleLevel(_20, float2(_131, _132), 0.0f);
                float _166 = TEXCOORD.x - _129;
                float _167 = TEXCOORD.y - _130;
                float4 _168 = _10.SampleLevel(_20, float2(_166, _167), 0.0f);
                float _170 = _168.z;
                float _171 = _168.w;
                float _177 = _170 - _70;
                float _187 = dot(float2(clamp(_177 + 0.5f, 0.0f, 1.0f), clamp(0.5f - _177, 0.0f, 1.0f)), float2(_154, clamp((_171 * _93) - _151, 0.0f, 1.0f))) * (1.0f - clamp(8.0f - (_171 * 4.0f), 0.0f, 1.0f));
                float4 _188 = _8.SampleLevel(_20, float2(_166, _167), 0.0f);
                bool _193 = _136 > _170;
                bool _194 = _171 > _137;
                float _196 = (_193 && _194) ? _187 : (dot(float2(clamp(_145 + 0.5f, 0.0f, 1.0f), clamp(0.5f - _145, 0.0f, 1.0f)), float2(_154, clamp((_137 * _93) - _151, 0.0f, 1.0f))) * (1.0f - clamp(8.0f - (_137 * 4.0f), 0.0f, 1.0f)));
                float _198 = (_193 || _194) ? _187 : _196;
                _111 = ((_196 * _161.x) + _120) + (_188.x * _198);
                _113 = ((_196 * _161.y) + _121) + (_188.y * _198);
                _115 = ((_196 * _161.z) + _122) + (_188.z * _198);
                _117 = (_196 + _123) + _198;
                uint _125 = _124 + 1u;
                if (_125 == _82)
                {
                    break;
                }
                else
                {
                    _120 = _111;
                    _121 = _113;
                    _122 = _115;
                    _123 = _117;
                    _124 = _125;
                }
            }
            _110 = _111;
            _112 = _113;
            _114 = _115;
            _116 = _117;
        }
        else
        {
            _110 = 0.0f;
            _112 = 0.0f;
            _114 = 0.0f;
            _116 = 0.0f;
        }
        float _119 = float(int(_82 << 1u));
        _95 = _110 / _119;
        _97 = _112 / _119;
        _99 = _114 / _119;
        _101 = _116 / _119;
    }
    else
    {
        _95 = 0.0f;
        _97 = 0.0f;
        _99 = 0.0f;
        _101 = 0.0f;
    }
    SV_Target.x = _95;
    SV_Target.y = _97;
    SV_Target.z = _99;
    SV_Target.w = _101;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    TEXCOORD = stage_input.TEXCOORD;
    TEXCOORD_1 = stage_input.TEXCOORD_1;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.SV_Target = SV_Target;
    return stage_output;
}
