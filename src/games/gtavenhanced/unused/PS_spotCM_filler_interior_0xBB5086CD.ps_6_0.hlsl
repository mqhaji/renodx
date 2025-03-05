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
    float _111 = TEXCOORD.x / TEXCOORD.w;
    float _112 = TEXCOORD.y / TEXCOORD.w;
    float4 _116 = _16.SampleLevel(_35, float2(_111, _112), 0.0f);
    float _119 = _116.x;
    float4 _144 = _9.Sample(_35, float2(_111, _112));
    float _146 = _144.x;
    float _147 = _144.y;
    float _148 = _144.z;
    float _156 = _32_m0[17u].z / ((1.0f - _119) + _32_m0[17u].w);
    float _157 = TEXCOORD_1.x / TEXCOORD_1.w;
    float _158 = TEXCOORD_1.y / TEXCOORD_1.w;
    float _159 = TEXCOORD_1.z / TEXCOORD_1.w;
    float _168 = _22_m0[15u].x + (_157 * _156);
    float _169 = _22_m0[15u].y + (_158 * _156);
    float _170 = _22_m0[15u].z + (_159 * _156);
    float4 _172 = _11.Sample(_35, float2(_111, _112));
    float _174 = _172.x;
    float _175 = _172.z;
    float4 _178 = _10.Sample(_35, float2(_111, _112));
    float _183 = _178.w;
    float _192 = frac(_183 * 7.984375f);
    float _193 = frac(_183 * 63.875f);
    float _204 = ((frac(_183 * 0.998046875f) + (-128.0f)) + (_178.x * 256.0f)) - (_192 * 0.125f);
    float _207 = ((_192 + (-128.0f)) + (_178.y * 256.0f)) - (_193 * 0.125f);
    float _209 = ((_178.z * 256.0f) + (-128.0f)) + _193;
    float _214 = rsqrt(dot(float3(_204, _207, _209), float3(_204, _207, _209)));
    float _215 = _204 * _214;
    float _216 = _207 * _214;
    float _217 = _209 * _214;
    float4 _219 = _8.Sample(_36, float2(_111, _112));
    float _221 = _219.x;
    float _223 = _32_m0[0u].x - _168;
    float _224 = _32_m0[0u].y - _169;
    float _225 = _32_m0[0u].z - _170;
    float _226 = dot(float3(_223, _224, _225), float3(_223, _224, _225));
    float _229 = rsqrt(_226);
    float _230 = _229 * _223;
    float _231 = _229 * _224;
    float _232 = _229 * _225;
    float _236 = rsqrt(dot(float3(_157, _158, _159), float3(_157, _158, _159)));
    float _246 = clamp(1.0f - (_226 * _32_m0[4u].z), 0.0f, 1.0f);
    float _266 = (clamp((dot(float3(_230, _231, _232), float3((-0.0f) - _32_m0[1u].x, (-0.0f) - _32_m0[1u].y, (-0.0f) - _32_m0[1u].z)) * _32_m0[5u].w) + _32_m0[5u].z, 0.0f, 1.0f) * (_246 / ((_246 * (1.0f - _32_m0[7u].x)) + _32_m0[7u].x))) * float(dot(float4(_168, _169, _170, 1.0f), float4(_32_m0[6u])) >= 0.0f);
    if (_266 < 9.9999999747524270787835121154785e-07f)
    {
        discard_state = true;
    }
    float _272 = _221 * _221;
    float _434;
    if (_32_m0[8u].x > 0.0f)
    {
        float _288 = sqrt(((_224 * _224) + (_223 * _223)) + (_225 * _225));
        float _294 = min(_32_m0[8u].w * _156, _288);
        float _302 = _32_m0[9u].w * _156;
        float _318 = ((_294 * (_223 / _288)) + _168) + (_302 * _215);
        float _320 = ((_294 * (_224 / _288)) + _169) + (_302 * _216);
        float _322 = ((_294 * (_225 / _288)) + _170) + (_302 * _217);
        float _361 = mad(_322, _22_m0[10u].w, mad(_320, _22_m0[9u].w, _318 * _22_m0[8u].w)) + _22_m0[11u].w;
        uint4 _375 = asuint(_27_m0[23u]);
        float _384 = floor(float(_375.z) + (_27_m0[15u].x * _111)) * 0.3333333432674407958984375f;
        float _389 = frac(abs(_384));
        float _394 = floor(float(_375.w) + (_27_m0[15u].y * _112)) * 0.3333333432674407958984375f;
        float _398 = frac(abs(_394));
        uint _404 = uint(int((((_394 >= ((-0.0f) - _394)) ? _398 : ((-0.0f) - _398)) * 9.0f) + (((_384 >= ((-0.0f) - _384)) ? _389 : ((-0.0f) - _389)) * 3.0f)));
        float _418 = (_404 == 0u) ? 1.16666662693023681640625f : ((float(int((407331666u >> (((_404 << 2u) + 28u) & 28u)) & 15u)) * 0.11111111938953399658203125f) + 0.16666667163372039794921875f);
        float _423 = (((((mad(_322, _22_m0[10u].x, mad(_320, _22_m0[9u].x, _318 * _22_m0[8u].x)) + _22_m0[11u].x) + _361) * 0.5f) / _361) - _111) * 0.083333335816860198974609375f;
        float _425 = (((((((-0.0f) - _22_m0[11u].y) - mad(_322, _22_m0[10u].y, mad(_320, _22_m0[9u].y, _318 * _22_m0[8u].y))) + _361) * 0.5f) / _361) - _112) * 0.083333335816860198974609375f;
        float _426 = (((mad(_322, _22_m0[10u].z, mad(_320, _22_m0[9u].z, _318 * _22_m0[8u].z)) + _22_m0[11u].z) / _361) - _119) * 0.083333335816860198974609375f;
        float _433 = _32_m0[17u].w + 1.0f;
        float _488;
        float _490;
        float _492;
        float _494;
        uint _496;
        _488 = 1.0f;
        _490 = (_418 * _423) + _111;
        _492 = (_418 * _425) + _112;
        _494 = (_418 * _426) + _119;
        _496 = 0u;
        float _489;
        for (;;)
        {
            float4 _499 = _16.SampleLevel(_35, float2(_490, _492), 0.0f);
            float _501 = _499.x;
            float _514 = _490 + _423;
            float _515 = _492 + _425;
            float _516 = _494 + _426;
            float4 _517 = _16.SampleLevel(_35, float2(_514, _515), 0.0f);
            float _519 = _517.x;
            _489 = ((((_433 - _494) < ((_433 - _501) * _32_m0[10u].w)) ? (((float(_501 < _494) + (-1.0f)) * _32_m0[11u].z) + 1.0f) : 1.0f) * _488) * (((_433 - _516) < ((_433 - _519) * _32_m0[10u].w)) ? (((float(_519 < _516) + (-1.0f)) * _32_m0[11u].z) + 1.0f) : 1.0f);
            uint _497 = _496 + 2u;
            if (int(_497) < int(11u))
            {
                _488 = _489;
                _490 = _514 + _423;
                _492 = _515 + _425;
                _494 = _516 + _426;
                _496 = _497;
            }
            else
            {
                break;
            }
        }
        _434 = _489 * _272;
    }
    else
    {
        _434 = _272;
    }
    float _436 = _266 * ((float(uint(float(_15.Load(int3(uint2(uint(int(_27_m0[15u].x * _111)), uint(int(_27_m0[15u].y * _112))), 0u)).y)) & 8u) < 7.900000095367431640625f) ? 0.0f : 1.0f);
    float _445 = 1.0f - clamp(dot(float3((-0.0f) - (_157 * _236), (-0.0f) - (_158 * _236), (-0.0f) - (_159 * _236)), float3(_215, _216, _217)), 0.0f, 1.0f);
    precise float _447 = _445 * _445;
    precise float _448 = _447 * _447;
    float _454 = (1.0f - (((1.0f - _175) + ((_445 * _175) * _448)) * clamp(_174 * _174, 0.0f, 1.0f))) * clamp(dot(float3(_215, _216, _217), float3(_230, _231, _232)), 0.0f, 1.0f);
    float _457 = ((_434 - _272) * _32_m0[8u].y) + _272;
    SV_Target.x = (((((_146 * _146) * (_32_m0[3u].w * _32_m0[3u].x)) * _436) * _457) * _27_m0[14u].z) * _454;
    SV_Target.y = (((((_147 * _147) * (_32_m0[3u].w * _32_m0[3u].y)) * _436) * _457) * _27_m0[14u].z) * _454;
    SV_Target.z = (((((_148 * _148) * (_32_m0[3u].w * _32_m0[3u].z)) * _436) * _457) * _27_m0[14u].z) * _454;
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
