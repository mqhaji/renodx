cbuffer _14_16 : register(b0, space0)
{
    float4 _16_m0[19] : packoffset(c0);
};

cbuffer _19_21 : register(b12, space0)
{
    float4 _21_m0[15] : packoffset(c0);
};

cbuffer _24_26 : register(b13, space0)
{
    float4 _26_m0[7] : packoffset(c0);
};

Texture2D<float4> _8 : register(t1, space0);
Texture2D<float4> _9 : register(t7, space0);
SamplerState _29 : register(s0, space0);
SamplerState _30 : register(s3, space0);

static float4 TEXCOORD;
static float2 TEXCOORD_1;
static float4 TEXCOORD_2;
static float4 SV_Target;

struct SPIRV_Cross_Input
{
    float4 TEXCOORD : TEXCOORD0;
    float2 TEXCOORD_1 : TEXCOORD1;
    float4 TEXCOORD_2 : TEXCOORD2;
};

struct SPIRV_Cross_Output
{
    float4 SV_Target : SV_Target0;
};

void frag_main()
{
    float4 _61 = _8.Sample(_29, float2(TEXCOORD_1.x, TEXCOORD_1.y));
    float _67 = _61.x * TEXCOORD.x;
    float _68 = _61.y * TEXCOORD.y;
    float _69 = _61.z * TEXCOORD.z;
    float _70 = _61.w * TEXCOORD.w;
    float _71 = dot(float3(_67, _68, _69), float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    float _92 = ((_16_m0[18u].x * (_71 - _67)) + _67) * _70;
    float _93 = ((_16_m0[18u].x * (_71 - _68)) + _68) * _70;
    float _94 = ((_16_m0[18u].x * (_71 - _69)) + _69) * _70;
    float _124;
    float _126;
    float _128;
    float _130;
    if (abs(_21_m0[5u].w - _21_m0[6u].x) > 0.0f)
    {
        float4 _116 = _9.SampleLevel(_30, float2(_26_m0[3u].z * TEXCOORD_1.x, _26_m0[3u].z * TEXCOORD_1.y), 0.0f);
        float _118 = _116.x;
        float _163;
        if (_26_m0[4u].y > 0.0f)
        {
            _163 = clamp(1.0f - (_26_m0[5u].y * (max(_21_m0[5u].w, _21_m0[6u].x) - _118)), 0.0f, 1.0f);
        }
        else
        {
            _163 = 0.0f;
        }
        float _166 = (_26_m0[4u].z * _163) + 1.0f;
        float _175 = min(_21_m0[5u].w, _21_m0[6u].x);
        float _176 = max(_21_m0[5u].w, _21_m0[6u].x);
        float _177 = _176 - _175;
        float _184 = min(min(_26_m0[3u].w * 0.5f, 0.5f - abs(0.5f - _176)), _177);
        float _201 = min(min(_26_m0[4u].x * 0.5f, 0.5f - abs(0.5f - _175)), _177);
        _124 = _166 * _92;
        _126 = _166 * _93;
        _128 = _166 * _94;
        _130 = min((_201 <= 0.0f) ? ((_118 < _175) ? 0.0f : 1.0f) : clamp(1.0f - (((_175 - _118) + _201) / (_201 * 2.0f)), 0.0f, 1.0f), (_184 <= 0.0f) ? ((_176 < _118) ? 0.0f : 1.0f) : clamp(1.0f - (((_118 - _176) + _184) / (_184 * 2.0f)), 0.0f, 1.0f));
    }
    else
    {
        _124 = _92;
        _126 = _93;
        _128 = _94;
        _130 = 0.0f;
    }
    SV_Target.x = (_130 * _124) * _21_m0[8u].w;
    SV_Target.y = (_130 * _126) * _21_m0[8u].w;
    SV_Target.z = (_130 * _128) * _21_m0[8u].w;
    SV_Target.w = _130 * _70;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    TEXCOORD = stage_input.TEXCOORD;
    TEXCOORD_1 = stage_input.TEXCOORD_1;
    TEXCOORD_2 = stage_input.TEXCOORD_2;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.SV_Target = SV_Target;
    return stage_output;
}
