cbuffer _20_22 : register(b2, space0)
{
    float4 _22_m0[56] : packoffset(c0);
};

cbuffer _25_27 : register(b4, space0)
{
    float4 _27_m0[26] : packoffset(c0);
};

cbuffer _30_32 : register(b9, space1)
{
    float4 _32_m0[24] : packoffset(c0);
};

Texture2D<float4> _8 : register(t24, space1);
Texture2D<float4> _9 : register(t26, space0);
Texture2D<float4> _10 : register(t27, space0);
Texture2D<float4> _11 : register(t28, space0);
Texture2D<uint4> _15 : register(t30, space0);
Texture2D<float4> _16 : register(t31, space0);
SamplerState _35 : register(s0, space1);
SamplerState _36 : register(s1, space1);

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
    float _112 = TEXCOORD.x / TEXCOORD.w;
    float _113 = TEXCOORD.y / TEXCOORD.w;
    float4 _117 = _16.SampleLevel(_35, float2(_112, _113), 0.0f);
    float _120 = _117.x;
    float4 _145 = _9.Sample(_35, float2(_112, _113));
    float _147 = _145.x;
    float _148 = _145.y;
    float _149 = _145.z;
    float _160 = _32_m0[17u].z / ((1.0f - _120) + _32_m0[17u].w);
    float _161 = TEXCOORD_1.x / TEXCOORD_1.w;
    float _162 = TEXCOORD_1.y / TEXCOORD_1.w;
    float _163 = TEXCOORD_1.z / TEXCOORD_1.w;
    float _172 = _22_m0[15u].x + (_161 * _160);
    float _173 = _22_m0[15u].y + (_162 * _160);
    float _174 = _22_m0[15u].z + (_163 * _160);
    float4 _176 = _11.Sample(_35, float2(_112, _113));
    float _178 = _176.x;
    float _179 = _176.y;
    float _180 = _176.z;
    float _183 = (_179 * _179) * 512.0f;
    float4 _186 = _10.Sample(_35, float2(_112, _113));
    float _191 = _186.w;
    float _200 = frac(_191 * 7.984375f);
    float _201 = frac(_191 * 63.875f);
    float _212 = ((frac(_191 * 0.998046875f) + (-128.0f)) + (_186.x * 256.0f)) - (_200 * 0.125f);
    float _215 = ((_200 + (-128.0f)) + (_186.y * 256.0f)) - (_201 * 0.125f);
    float _217 = ((_186.z * 256.0f) + (-128.0f)) + _201;
    float _222 = rsqrt(dot(float3(_212, _215, _217), float3(_212, _215, _217)));
    float _223 = _212 * _222;
    float _224 = _215 * _222;
    float _225 = _217 * _222;
    float4 _227 = _8.Sample(_36, float2(_112, _113));
    float _229 = _227.x;
    float _230 = clamp(_178 * _178, 0.0f, 1.0f);
    float _233 = max(0.0f, _183 + (-500.0f));
    float _239 = ((_183 - _233) * 3.0f) + (_233 * 558.0f);
    float _240 = _32_m0[0u].x - _172;
    float _241 = _32_m0[0u].y - _173;
    float _242 = _32_m0[0u].z - _174;
    float _243 = dot(float3(_240, _241, _242), float3(_240, _241, _242));
    float _246 = rsqrt(_243);
    float _247 = _246 * _240;
    float _248 = _246 * _241;
    float _249 = _246 * _242;
    float _253 = rsqrt(dot(float3(_161, _162, _163), float3(_161, _162, _163)));
    float _254 = _253 * _161;
    float _255 = _253 * _162;
    float _256 = _253 * _163;
    float _261 = _247 - _254;
    float _262 = _248 - _255;
    float _263 = _249 - _256;
    float _267 = rsqrt(dot(float3(_261, _262, _263), float3(_261, _262, _263)));
    float _268 = _261 * _267;
    float _269 = _262 * _267;
    float _270 = _263 * _267;
    float _273 = clamp(1.0f - (_243 * _32_m0[4u].z), 0.0f, 1.0f);
    float _293 = (clamp((dot(float3(_247, _248, _249), float3((-0.0f) - _32_m0[1u].x, (-0.0f) - _32_m0[1u].y, (-0.0f) - _32_m0[1u].z)) * _32_m0[5u].w) + _32_m0[5u].z, 0.0f, 1.0f) * (_273 / ((_273 * (1.0f - _32_m0[7u].x)) + _32_m0[7u].x))) * float(dot(float4(_172, _173, _174, 1.0f), float4(_32_m0[6u])) >= 0.0f);
    if (_293 < 9.9999999747524270787835121154785e-07f)
    {
        discard_state = true;
    }
    float _299 = _229 * _229;
    float _460;
    if (_32_m0[8u].x > 0.0f)
    {
        float _315 = sqrt(((_241 * _241) + (_240 * _240)) + (_242 * _242));
        float _321 = min(_32_m0[8u].w * _160, _315);
        float _329 = _32_m0[9u].w * _160;
        float _345 = ((_321 * (_240 / _315)) + _172) + (_329 * _223);
        float _347 = ((_321 * (_241 / _315)) + _173) + (_329 * _224);
        float _349 = ((_321 * (_242 / _315)) + _174) + (_329 * _225);
        float _388 = mad(_349, _22_m0[10u].w, mad(_347, _22_m0[9u].w, _345 * _22_m0[8u].w)) + _22_m0[11u].w;
        uint4 _402 = asuint(_27_m0[23u]);
        float _411 = floor(float(_402.z) + (_27_m0[15u].x * _112)) * 0.3333333432674407958984375f;
        float _416 = frac(abs(_411));
        float _420 = floor(float(_402.w) + (_27_m0[15u].y * _113)) * 0.3333333432674407958984375f;
        float _424 = frac(abs(_420));
        uint _430 = uint(int((((_420 >= ((-0.0f) - _420)) ? _424 : ((-0.0f) - _424)) * 9.0f) + (((_411 >= ((-0.0f) - _411)) ? _416 : ((-0.0f) - _416)) * 3.0f)));
        float _444 = (_430 == 0u) ? 1.16666662693023681640625f : ((float(int((407331666u >> (((_430 << 2u) + 28u) & 28u)) & 15u)) * 0.11111111938953399658203125f) + 0.16666667163372039794921875f);
        float _449 = (((((mad(_349, _22_m0[10u].x, mad(_347, _22_m0[9u].x, _345 * _22_m0[8u].x)) + _22_m0[11u].x) + _388) * 0.5f) / _388) - _112) * 0.083333335816860198974609375f;
        float _451 = (((((((-0.0f) - _22_m0[11u].y) - mad(_349, _22_m0[10u].y, mad(_347, _22_m0[9u].y, _345 * _22_m0[8u].y))) + _388) * 0.5f) / _388) - _113) * 0.083333335816860198974609375f;
        float _452 = (((mad(_349, _22_m0[10u].z, mad(_347, _22_m0[9u].z, _345 * _22_m0[8u].z)) + _22_m0[11u].z) / _388) - _120) * 0.083333335816860198974609375f;
        float _459 = _32_m0[17u].w + 1.0f;
        float _542;
        float _544;
        float _546;
        float _548;
        uint _550;
        _542 = 1.0f;
        _544 = (_444 * _449) + _112;
        _546 = (_444 * _451) + _113;
        _548 = (_444 * _452) + _120;
        _550 = 0u;
        float _543;
        for (;;)
        {
            float4 _553 = _16.SampleLevel(_35, float2(_544, _546), 0.0f);
            float _555 = _553.x;
            float _568 = _544 + _449;
            float _569 = _546 + _451;
            float _570 = _548 + _452;
            float4 _571 = _16.SampleLevel(_35, float2(_568, _569), 0.0f);
            float _573 = _571.x;
            _543 = ((((_459 - _548) < ((_459 - _555) * _32_m0[10u].w)) ? (((float(_555 < _548) + (-1.0f)) * _32_m0[11u].z) + 1.0f) : 1.0f) * _542) * (((_459 - _570) < ((_459 - _573) * _32_m0[10u].w)) ? (((float(_573 < _570) + (-1.0f)) * _32_m0[11u].z) + 1.0f) : 1.0f);
            uint _551 = _550 + 2u;
            if (int(_551) < int(11u))
            {
                _542 = _543;
                _544 = _568 + _449;
                _546 = _569 + _451;
                _548 = _570 + _452;
                _550 = _551;
            }
            else
            {
                break;
            }
        }
        _460 = _543 * _299;
    }
    else
    {
        _460 = _299;
    }
    float _462 = _293 * ((float(uint(float(_15.Load(int3(uint2(uint(int(_27_m0[15u].x * _112)), uint(int(_27_m0[15u].y * _113))), 0u)).y)) & 8u) < 7.900000095367431640625f) ? 0.0f : 1.0f);
    float _469 = clamp(dot(float3(_223, _224, _225), float3(_247, _248, _249)), 0.0f, 1.0f);
    float _478 = 1.0f - clamp(dot(float3((-0.0f) - _254, (-0.0f) - _255, (-0.0f) - _256), float3(_223, _224, _225)), 0.0f, 1.0f);
    float _479 = 1.0f - clamp(dot(float3(_268, _269, _270), float3(_247, _248, _249)), 0.0f, 1.0f);
    float _480 = 1.0f - _180;
    precise float _481 = _478 * _478;
    precise float _482 = _481 * _481;
    precise float _485 = _479 * _479;
    precise float _486 = _485 * _485;
    float _508 = (1.0f - ((_480 + ((_478 * _180) * _482)) * _230)) * _469;
    float _511 = ((_230 * _32_m0[8u].z) * _469) * ((((_239 + 2.0f) * 0.125f) * exp2(log2(clamp(dot(float3(_223, _224, _225), float3(_268, _269, _270)) + 9.9999999392252902907785028219223e-09f, 0.0f, 1.0f)) * (_239 + 9.9999999392252902907785028219223e-09f))) * (_480 + ((_479 * _180) * _486)));
    float _514 = ((_460 - _299) * _32_m0[8u].y) + _299;
    SV_Target.x = ((((_32_m0[3u].w * _32_m0[3u].x) * _462) * _514) * _27_m0[14u].z) * (_511 + ((_147 * _147) * _508));
    SV_Target.y = ((((_32_m0[3u].w * _32_m0[3u].y) * _462) * _514) * _27_m0[14u].z) * (_511 + ((_148 * _148) * _508));
    SV_Target.z = ((((_32_m0[3u].w * _32_m0[3u].z) * _462) * _514) * _27_m0[14u].z) * (_511 + ((_149 * _149) * _508));
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
