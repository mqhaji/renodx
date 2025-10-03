shader type? (vert, frag, geom, tesc, tese, comp): cbuffer _14_16 : register(b0, space0)
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
    float _90 = mad(_69, _21_m0[2u].x, mad(_68, _21_m0[1u].x, _67 * _21_m0[0u].x));
    float _93 = mad(_69, _21_m0[2u].y, mad(_68, _21_m0[1u].y, _67 * _21_m0[0u].y));
    float _96 = mad(_69, _21_m0[2u].z, mad(_68, _21_m0[1u].z, _67 * _21_m0[0u].z));
    float _97 = dot(float3(_90, _93, _96), float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    float _117 = ((_16_m0[18u].x * (_97 - _90)) + _90) * _70;
    float _118 = ((_16_m0[18u].x * (_97 - _93)) + _93) * _70;
    float _119 = ((_16_m0[18u].x * (_97 - _96)) + _96) * _70;
    float _148;
    float _150;
    float _152;
    float _154;
    if (abs(_21_m0[5u].w - _21_m0[6u].x) > 0.0f)
    {
        float4 _140 = _9.SampleLevel(_30, float2(_26_m0[3u].z * TEXCOORD_1.x, _26_m0[3u].z * TEXCOORD_1.y), 0.0f);
        float _142 = _140.x;
        float _187;
        if (_26_m0[4u].y > 0.0f)
        {
            _187 = clamp(1.0f - (_26_m0[5u].y * (max(_21_m0[5u].w, _21_m0[6u].x) - _142)), 0.0f, 1.0f);
        }
        else
        {
            _187 = 0.0f;
        }
        float _190 = (_26_m0[4u].z * _187) + 1.0f;
        float _199 = min(_21_m0[5u].w, _21_m0[6u].x);
        float _200 = max(_21_m0[5u].w, _21_m0[6u].x);
        float _201 = _200 - _199;
        float _208 = min(min(_26_m0[3u].w * 0.5f, 0.5f - abs(0.5f - _200)), _201);
        float _225 = min(min(_26_m0[4u].x * 0.5f, 0.5f - abs(0.5f - _199)), _201);
        _148 = _190 * _117;
        _150 = _190 * _118;
        _152 = _190 * _119;
        _154 = min((_225 <= 0.0f) ? ((_142 < _199) ? 0.0f : 1.0f) : clamp(1.0f - (((_199 - _142) + _225) / (_225 * 2.0f)), 0.0f, 1.0f), (_208 <= 0.0f) ? ((_200 < _142) ? 0.0f : 1.0f) : clamp(1.0f - (((_142 - _200) + _208) / (_208 * 2.0f)), 0.0f, 1.0f));
    }
    else
    {
        _148 = _117;
        _150 = _118;
        _152 = _119;
        _154 = 0.0f;
    }
    SV_Target.x = (_154 * _148) * _21_m0[8u].w;
    SV_Target.y = (_154 * _150) * _21_m0[8u].w;
    SV_Target.z = (_154 * _152) * _21_m0[8u].w;
    SV_Target.w = _154 * _70;
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
