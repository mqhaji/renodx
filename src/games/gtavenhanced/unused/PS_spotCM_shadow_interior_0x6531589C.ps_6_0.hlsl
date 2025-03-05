cbuffer _24_26 : register(b2, space0)
{
    float4 _26_m0[56] : packoffset(c0);
};

cbuffer _29_31 : register(b4, space0)
{
    float4 _31_m0[26] : packoffset(c0);
};

cbuffer _34_36 : register(b8, space0)
{
    float4 _36_m0[53] : packoffset(c0);
};

cbuffer _39_41 : register(b9, space1)
{
    float4 _41_m0[24] : packoffset(c0);
};

Texture2D<float4> _8 : register(t5, space0);
TextureCube<float4> _11 : register(t20, space0);
Texture2D<float4> _12 : register(t24, space1);
Texture2D<float4> _13 : register(t26, space0);
Texture2D<float4> _14 : register(t27, space0);
Texture2D<float4> _15 : register(t28, space0);
Texture2D<uint4> _19 : register(t30, space0);
Texture2D<float4> _20 : register(t31, space0);
SamplerState _44 : register(s0, space1);
SamplerState _45 : register(s1, space1);
SamplerComparisonState _46 : register(s5, space1);
SamplerComparisonState _47 : register(s6, space1);

static float4 TEXCOORD;
static float4 TEXCOORD_1;
static float4 SV_Target;

struct SPIRV_Cross_Input
{
    float4 TEXCOORD : TEXCOORD1;
    float4 TEXCOORD_1 : TEXCOORD2;
};

struct SPIRV_Cross_Output
{
    float4 SV_Target : SV_Target0;
};

static bool discard_state;

void discard_exit()
{
    if (discard_state)
    {
        discard;
    }
}

void frag_main()
{
    discard_state = false;
    float _118 = TEXCOORD.x / TEXCOORD.w;
    float _119 = TEXCOORD.y / TEXCOORD.w;
    float _123 = (_118 * 2.0f) + (-1.0f);
    float _125 = 1.0f - (_119 * 2.0f);
    float4 _130 = _20.Sample(_44, float2(_118, _119));
    float _141 = _41_m0[17u].z / ((1.0f - _130.x) + _41_m0[17u].w);
    float _148 = dot(float3(_123, _125, 1.0f), float3(_41_m0[18u].xyz));
    float _158 = dot(float3(_123, _125, 1.0f), float3(_41_m0[19u].xyz));
    float _167 = dot(float3(_123, _125, 1.0f), float3(_41_m0[20u].xyz));
    float4 _173 = _20.SampleLevel(_44, float2(_118, _119), 0.0f);
    float _175 = _173.x;
    float4 _199 = _13.Sample(_44, float2(_118, _119));
    float _201 = _199.x;
    float _202 = _199.y;
    float _203 = _199.z;
    float _213 = _41_m0[17u].z / ((1.0f - _175) + _41_m0[17u].w);
    float _222 = _26_m0[15u].x + (_213 * _148);
    float _223 = _26_m0[15u].y + (_213 * _158);
    float _224 = _26_m0[15u].z + (_213 * _167);
    float4 _226 = _15.Sample(_44, float2(_118, _119));
    float _228 = _226.x;
    float _229 = _226.y;
    float _230 = _226.z;
    float _233 = (_229 * _229) * 512.0f;
    float4 _236 = _14.Sample(_44, float2(_118, _119));
    float _241 = _236.w;
    float _250 = frac(_241 * 7.984375f);
    float _251 = frac(_241 * 63.875f);
    float _262 = ((frac(_241 * 0.998046875f) + (-128.0f)) + (_236.x * 256.0f)) - (_250 * 0.125f);
    float _265 = ((_250 + (-128.0f)) + (_236.y * 256.0f)) - (_251 * 0.125f);
    float _267 = ((_236.z * 256.0f) + (-128.0f)) + _251;
    float _271 = rsqrt(dot(float3(_262, _265, _267), float3(_262, _265, _267)));
    float _272 = _262 * _271;
    float _273 = _265 * _271;
    float _274 = _267 * _271;
    float4 _276 = _12.Sample(_45, float2(_118, _119));
    float _278 = _276.x;
    float _279 = clamp(_228 * _228, 0.0f, 1.0f);
    float _282 = max(0.0f, _233 + (-500.0f));
    float _288 = ((_233 - _282) * 3.0f) + (_282 * 558.0f);
    float _289 = _41_m0[0u].x - _222;
    float _290 = _41_m0[0u].y - _223;
    float _291 = _41_m0[0u].z - _224;
    float _292 = dot(float3(_289, _290, _291), float3(_289, _290, _291));
    float _295 = rsqrt(_292);
    float _296 = _295 * _289;
    float _297 = _295 * _290;
    float _298 = _295 * _291;
    float _302 = rsqrt(dot(float3(_148, _158, _167), float3(_148, _158, _167)));
    float _303 = _302 * _148;
    float _304 = _302 * _158;
    float _305 = _302 * _167;
    float _310 = _296 - _303;
    float _311 = _297 - _304;
    float _312 = _298 - _305;
    float _316 = rsqrt(dot(float3(_310, _311, _312), float3(_310, _311, _312)));
    float _317 = _310 * _316;
    float _318 = _311 * _316;
    float _319 = _312 * _316;
    float _322 = clamp(1.0f - (_292 * _41_m0[4u].z), 0.0f, 1.0f);
    float _342 = (clamp((dot(float3(_296, _297, _298), float3((-0.0f) - _41_m0[1u].x, (-0.0f) - _41_m0[1u].y, (-0.0f) - _41_m0[1u].z)) * _41_m0[5u].w) + _41_m0[5u].z, 0.0f, 1.0f) * (_322 / ((_322 * (1.0f - _41_m0[7u].x)) + _41_m0[7u].x))) * float(dot(float4(_222, _223, _224, 1.0f), float4(_41_m0[6u])) >= 0.0f);
    if (_342 < 9.9999999747524270787835121154785e-07f)
    {
        discard_state = true;
    }
    float _374 = _36_m0[23u].x + (_148 * _141);
    float _375 = _36_m0[23u].y + (_158 * _141);
    float _376 = _36_m0[23u].z + (_167 * _141);
    float _377 = dot(float3(_374, _375, _376), float3(_36_m0[20u].xyz));
    float _380 = dot(float3(_374, _375, _376), float3(_36_m0[21u].xyz));
    float _383 = dot(float3(_374, _375, _376), float3(_36_m0[22u].xyz));
    float _386 = (-0.0f) - _383;
    float _400 = float(asuint(_31_m0[22u]).y & 63u) * 5.588237762451171875f;
    float _413 = frac(frac(((_400 + (_31_m0[15u].x * _118)) * 0.067110560834407806396484375f) + ((_400 + (_31_m0[15u].y * _119)) * 0.005837149918079376220703125f)) * 52.98291778564453125f) * 6.283185482025146484375f;
    float _415 = sin(_413);
    float _416 = cos(_413);
    float _422 = ((-0.0f) - _415) - _416;
    float _423 = _415 - _416;
    float _612;
    if (_36_m0[20u].w == 2.0f)
    {
        float _431 = sqrt(((_380 * _380) + (_377 * _377)) + (_383 * _383));
        float _432 = ((-0.0f) - _377) / _431;
        float _433 = ((-0.0f) - _380) / _431;
        float _434 = _386 / _431;
        float _435 = _36_m0[22u].w * _431;
        float _436 = abs(_432);
        float _437 = abs(_433);
        float _438 = abs(_434);
        bool _459 = (_438 < _436) || (_438 < _437);
        bool _462 = (_437 < _436) || (_437 < _438);
        float _467 = _459 ? (_462 ? 0.0f : ((-0.0f) - float(int(uint(_433 > 0.0f) - uint(_433 < 0.0f))))) : ((-0.0f) - float(int(uint(_434 > 0.0f) - uint(_434 < 0.0f))));
        float _469 = (_459 && _462) ? ((-0.0f) - float(int(uint(_432 > 0.0f) - uint(_432 < 0.0f)))) : 0.0f;
        float _470 = _469 * _433;
        float _473 = (_467 * _434) - (_469 * _432);
        float _475 = (-0.0f) - (_433 * _467);
        float _479 = rsqrt(dot(float3(_470, _473, _475), float3(_470, _473, _475)));
        float _480 = _470 * _479;
        float _481 = _473 * _479;
        float _482 = _479 * _475;
        float _492 = _36_m0[52u].z * 3.0f;
        float _493 = _492 * ((_481 * _434) - (_482 * _433));
        float _494 = _492 * ((_482 * _432) - (_480 * _434));
        float _495 = _492 * ((_480 * _433) - (_481 * _432));
        float _496 = _492 * _480;
        float _497 = _492 * _481;
        float _498 = _492 * _482;
        float _499 = _422 * 0.4709331095218658447265625f;
        float _501 = _423 * 0.4709331095218658447265625f;
        float _502 = _416 * 0.333000004291534423828125f;
        float _504 = _415 * (-0.333000004291534423828125f);
        _612 = (_11.SampleCmpLevelZero(_46, float3(((_493 * _499) + _432) + (_496 * _501), ((_494 * _499) + _433) + (_497 * _501), ((_495 * _499) + _434) + (_498 * _501)), _435).xxxx.x + _11.SampleCmpLevelZero(_46, float3(((_493 * _415) + _432) + (_496 * _416), ((_494 * _415) + _433) + (_497 * _416), ((_495 * _415) + _434) + (_498 * _416)), _435).xxxx.x) + _11.SampleCmpLevelZero(_46, float3(((_493 * _502) + _432) + (_496 * _504), ((_494 * _502) + _433) + (_497 * _504), ((_495 * _502) + _434) + (_498 * _504)), _435).xxxx.x;
    }
    else
    {
        float _567 = sqrt(((_374 * _374) + (_375 * _375)) + (_376 * _376)) * _36_m0[22u].w;
        float _571 = ((_377 / _386) * 0.5f) + 0.5f;
        float _572 = 0.5f - ((_380 / _386) * 0.5f);
        float _574 = _36_m0[52u].z * 2.0f;
        float _575 = _36_m0[52u].w * 2.0f;
        _612 = (_8.SampleCmpLevelZero(_47, float2(((_422 * 0.49497473239898681640625f) * _574) + _571, ((_423 * 0.49497473239898681640625f) * _575) + _572), _567).xxxx.x + _8.SampleCmpLevelZero(_47, float2((_574 * _415) + _571, (_575 * _416) + _572), _567).xxxx.x) + _8.SampleCmpLevelZero(_47, float2(((_416 * 0.4000000059604644775390625f) * _574) + _571, ((_415 * (-0.4000000059604644775390625f)) * _575) + _572), _567).xxxx.x;
    }
    float _613 = _612 * 0.3333333432674407958984375f;
    float _773;
    if (_41_m0[8u].x > 0.0f)
    {
        float _630 = sqrt(((_290 * _290) + (_289 * _289)) + (_291 * _291));
        float _636 = min(_41_m0[8u].w * _213, _630);
        float _644 = _41_m0[9u].w * _213;
        float _660 = ((_636 * (_289 / _630)) + _222) + (_644 * _272);
        float _662 = ((_636 * (_290 / _630)) + _223) + (_644 * _273);
        float _664 = ((_636 * (_291 / _630)) + _224) + (_644 * _274);
        float _703 = mad(_664, _26_m0[10u].w, mad(_662, _26_m0[9u].w, _660 * _26_m0[8u].w)) + _26_m0[11u].w;
        uint4 _715 = asuint(_31_m0[23u]);
        float _724 = floor(float(_715.z) + (_31_m0[15u].x * _118)) * 0.3333333432674407958984375f;
        float _728 = frac(abs(_724));
        float _732 = floor(float(_715.w) + (_31_m0[15u].y * _119)) * 0.3333333432674407958984375f;
        float _736 = frac(abs(_732));
        uint _742 = uint(int((((_732 >= ((-0.0f) - _732)) ? _736 : ((-0.0f) - _736)) * 9.0f) + (((_724 >= ((-0.0f) - _724)) ? _728 : ((-0.0f) - _728)) * 3.0f)));
        float _757 = (_742 == 0u) ? 1.16666662693023681640625f : ((float(int((407331666u >> (((_742 << 2u) + 28u) & 28u)) & 15u)) * 0.11111111938953399658203125f) + 0.16666667163372039794921875f);
        float _762 = (((((mad(_664, _26_m0[10u].x, mad(_662, _26_m0[9u].x, _660 * _26_m0[8u].x)) + _26_m0[11u].x) + _703) * 0.5f) / _703) - _118) * 0.083333335816860198974609375f;
        float _764 = (((((((-0.0f) - _26_m0[11u].y) - mad(_664, _26_m0[10u].y, mad(_662, _26_m0[9u].y, _660 * _26_m0[8u].y))) + _703) * 0.5f) / _703) - _119) * 0.083333335816860198974609375f;
        float _765 = (((mad(_664, _26_m0[10u].z, mad(_662, _26_m0[9u].z, _660 * _26_m0[8u].z)) + _26_m0[11u].z) / _703) - _175) * 0.083333335816860198974609375f;
        float _772 = _41_m0[17u].w + 1.0f;
        float _855;
        float _857;
        float _859;
        float _861;
        uint _863;
        _855 = 1.0f;
        _857 = (_757 * _762) + _118;
        _859 = (_757 * _764) + _119;
        _861 = (_757 * _765) + _175;
        _863 = 0u;
        float _856;
        for (;;)
        {
            float4 _866 = _20.SampleLevel(_44, float2(_857, _859), 0.0f);
            float _868 = _866.x;
            float _880 = _857 + _762;
            float _881 = _859 + _764;
            float _882 = _861 + _765;
            float4 _883 = _20.SampleLevel(_44, float2(_880, _881), 0.0f);
            float _885 = _883.x;
            _856 = ((((_772 - _861) < ((_772 - _868) * _41_m0[10u].w)) ? (((float(_868 < _861) + (-1.0f)) * _41_m0[11u].z) + 1.0f) : 1.0f) * _855) * (((_772 - _882) < ((_772 - _885) * _41_m0[10u].w)) ? (((float(_885 < _882) + (-1.0f)) * _41_m0[11u].z) + 1.0f) : 1.0f);
            uint _864 = _863 + 2u;
            if (int(_864) < int(11u))
            {
                _855 = _856;
                _857 = _880 + _762;
                _859 = _881 + _764;
                _861 = _882 + _765;
                _863 = _864;
            }
            else
            {
                break;
            }
        }
        _773 = _856 * _613;
    }
    else
    {
        _773 = _613;
    }
    float _775 = _342 * ((float(uint(float(_19.Load(int3(uint2(uint(int(_31_m0[15u].x * _118)), uint(int(_31_m0[15u].y * _119))), 0u)).y)) & 8u) < 7.900000095367431640625f) ? 0.0f : 1.0f);
    float _782 = clamp(dot(float3(_272, _273, _274), float3(_296, _297, _298)), 0.0f, 1.0f);
    float _791 = 1.0f - clamp(dot(float3((-0.0f) - _303, (-0.0f) - _304, (-0.0f) - _305), float3(_272, _273, _274)), 0.0f, 1.0f);
    float _792 = 1.0f - clamp(dot(float3(_317, _318, _319), float3(_296, _297, _298)), 0.0f, 1.0f);
    float _793 = 1.0f - _230;
    precise float _794 = _791 * _791;
    precise float _795 = _794 * _794;
    precise float _798 = _792 * _792;
    precise float _799 = _798 * _798;
    float _820 = (1.0f - ((_793 + ((_791 * _230) * _795)) * _279)) * _782;
    float _823 = ((_279 * _41_m0[8u].z) * _782) * ((((_288 + 2.0f) * 0.125f) * exp2(log2(clamp(dot(float3(_272, _273, _274), float3(_317, _318, _319)) + 9.9999999392252902907785028219223e-09f, 0.0f, 1.0f)) * (_288 + 9.9999999392252902907785028219223e-09f))) * (_793 + ((_792 * _230) * _799)));
    float _824 = _278 * _278;
    float _827 = ((_773 - _824) * _41_m0[8u].y) + _824;
    SV_Target.x = ((((_41_m0[3u].w * _41_m0[3u].x) * _775) * _827) * _31_m0[14u].z) * (_823 + ((_201 * _201) * _820));
    SV_Target.y = ((((_41_m0[3u].w * _41_m0[3u].y) * _775) * _827) * _31_m0[14u].z) * (_823 + ((_202 * _202) * _820));
    SV_Target.z = ((((_41_m0[3u].w * _41_m0[3u].z) * _775) * _827) * _31_m0[14u].z) * (_823 + ((_203 * _203) * _820));
    SV_Target.w = 1.0f;
    discard_exit();
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
