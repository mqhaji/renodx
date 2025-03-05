cbuffer _24_26 : register(b3, space0)
{
    float4 _26_m0[6] : packoffset(c0);
};

cbuffer _29_31 : register(b5, space0)
{
    float4 _31_m0[26] : packoffset(c0);
};

cbuffer _34_36 : register(b12, space1)
{
    float4 _36_m0[95] : packoffset(c0);
};

Texture2DArray<float4> _8 : register(t1, space0);
Texture2D<float4> _11 : register(t11, space1);
Texture2D<float4> _12 : register(t14, space1);
Texture2D<float4> _13 : register(t15, space1);
Texture2D<float4> _14 : register(t17, space1);
Texture2D<float4> _15 : register(t19, space1);
Texture2D<float4> _16 : register(t25, space1);
Texture2D<float4> _17 : register(t29, space1);
Texture2D<float4> _18 : register(t30, space1);
Texture2D<float4> _19 : register(t31, space1);
SamplerState _39 : register(s0, space1);
SamplerState _40 : register(s1, space1);
SamplerState _41 : register(s2, space1);
SamplerState _42 : register(s3, space1);
SamplerState _45[] : register(s0, space2);
SamplerState _46 : register(s8, space1);

static float4 gl_FragCoord;
static float4 TEXCOORD;
static float TEXCOORD_1;
static float4 SV_Target;

struct SPIRV_Cross_Input
{
    float4 TEXCOORD : TEXCOORD1;
    float TEXCOORD_1 : TEXCOORD2;
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 SV_Target : SV_Target0;
};

void frag_main()
{
    float4 _89 = _11.Sample(_39, float2(TEXCOORD.x, TEXCOORD.y));
    float _101 = _36_m0[0u].z / ((1.0f - _89.x) + _36_m0[0u].w);
    float _117 = min(max(1.0f - clamp(((_36_m0[72u].z / _36_m0[72u].w) * 0.20000000298023223876953125f) + (-0.4000000059604644775390625f), 0.0f, 1.0f), 0.5f), 1.0f);
    float _126 = TEXCOORD.x + (-0.5f);
    float _128 = TEXCOORD.y + (-0.5f);
    float _129 = (_36_m0[72u].x / _36_m0[72u].y) * _126;
    float _130 = dot(float2(_129, _128), float2(_129, _128));
    float _138 = ((_117 * _130) * ((sqrt(_130) * _36_m0[69u].y) + _36_m0[69u].x)) + 1.0f;
    float _139 = _138 * _126;
    float _140 = _138 * _128;
    float _141 = _139 + 0.5f;
    float _142 = _140 + 0.5f;
    float _148 = _141 * _31_m0[15u].x;
    float _149 = _142 * _31_m0[15u].y;
    float _152 = floor(_148 + (-0.5f));
    float _153 = floor(_149 + (-0.5f));
    float _154 = _152 + 0.5f;
    float _155 = _153 + 0.5f;
    float _156 = _148 - _154;
    float _157 = _149 - _155;
    float _158 = _156 * _156;
    float _159 = _157 * _157;
    float _160 = _158 * _156;
    float _161 = _159 * _157;
    float _166 = _158 - ((_160 + _156) * 0.5f);
    float _167 = _159 - ((_161 + _157) * 0.5f);
    float _181 = (_156 * 0.5f) * (_158 - _156);
    float _183 = (_157 * 0.5f) * (_159 - _157);
    float _185 = (1.0f - _181) - _166;
    float _188 = (1.0f - _183) - _167;
    float _200 = (((_185 - (((_160 * 1.5f) - (_158 * 2.5f)) + 1.0f)) / _185) + _154) / _31_m0[15u].x;
    float _201 = (((_188 - (((_161 * 1.5f) - (_159 * 2.5f)) + 1.0f)) / _188) + _155) / _31_m0[15u].y;
    float _204 = _185 * _167;
    float _205 = _188 * _166;
    float _206 = _185 * _188;
    float _207 = _188 * _181;
    float _208 = _185 * _183;
    float _212 = (((_204 + _205) + _206) + _207) + _208;
    float4 _222 = _13.SampleLevel(_45[asuint(_26_m0[1u]).x + 0u], float2(_200, (_153 + (-0.5f)) / _31_m0[15u].y), 0.0f);
    float4 _236 = _13.SampleLevel(_45[asuint(_26_m0[1u]).x + 0u], float2((_152 + (-0.5f)) / _31_m0[15u].x, _201), 0.0f);
    float4 _252 = _13.SampleLevel(_45[asuint(_26_m0[1u]).x + 0u], float2(_200, _201), 0.0f);
    float4 _268 = _13.SampleLevel(_45[asuint(_26_m0[1u]).x + 0u], float2((_152 + 2.5f) / _31_m0[15u].x, _201), 0.0f);
    float4 _284 = _13.SampleLevel(_45[asuint(_26_m0[1u]).x + 0u], float2(_200, (_153 + 2.5f) / _31_m0[15u].y), 0.0f);
    float _294 = max(0.0f, (((((_236.y * _205) + (_222.y * _204)) + (_252.y * _206)) + (_268.y * _207)) + (_284.y * _208)) / _212);
    float _295 = max(0.0f, (((((_236.z * _205) + (_222.z * _204)) + (_252.z * _206)) + (_268.z * _207)) + (_284.z * _208)) / _212);
    float _305 = (_36_m0[72u].x / _36_m0[72u].y) * _139;
    float _306 = dot(float2(_305, _140), float2(_305, _140));
    float _314 = ((_117 * _306) * ((sqrt(_306) * _36_m0[69u].w) + _36_m0[69u].z)) + 1.0f;
    float4 _327 = _13.Sample(_45[asuint(_26_m0[0u]).w + 0u], float2((_314 * _139) + 0.5f, (_314 * _140) + 0.5f));
    float _334 = _31_m0[14u].w * _327.x;
    float _341;
    float _343;
    float _345;
    if (_36_m0[85u].x > 0.0f)
    {
        _341 = _334;
        _343 = _294;
        _345 = _295;
    }
    else
    {
        float _390 = _36_m0[72u].y * 0.5f;
        float _391 = _141 - _36_m0[72u].x;
        float _392 = _142 - _390;
        float4 _394 = _12.Sample(_41, float2(_391, _392));
        float _399 = _36_m0[72u].x * 0.5f;
        float _400 = _399 + _141;
        float _401 = _142 - _36_m0[72u].y;
        float4 _402 = _12.Sample(_41, float2(_400, _401));
        float _407 = _141 - _399;
        float _408 = _36_m0[72u].y + _142;
        float4 _409 = _12.Sample(_41, float2(_407, _408));
        float _414 = _36_m0[72u].x + _141;
        float _415 = _390 + _142;
        float4 _416 = _12.Sample(_41, float2(_414, _415));
        float4 _421 = _12.Sample(_41, float2(_141, _142));
        float4 _427 = _15.Sample(_40, float2(_391, _392));
        float4 _432 = _15.Sample(_40, float2(_400, _401));
        float4 _437 = _15.Sample(_40, float2(_407, _408));
        float4 _442 = _15.Sample(_40, float2(_414, _415));
        float4 _447 = _15.Sample(_40, float2(_141, _142));
        float _449 = _447.x;
        float _450 = _447.y;
        float _451 = _447.z;
        float _453 = (_449 + _421.x) * 0.5f;
        float _455 = (_450 + _421.y) * 0.5f;
        float _457 = (_451 + _421.z) * 0.5f;
        bool _490 = _36_m0[4u].x != 0.0f;
        float _510 = max(clamp(1.0f - ((_101 - _36_m0[3u].x) * _36_m0[3u].y), 0.0f, 1.0f), clamp((_101 - _36_m0[3u].z) * _36_m0[3u].w, 0.0f, 1.0f));
        float _511 = _510 * 2.0040080547332763671875f;
        float _519 = clamp(1.0f - _511, 0.0f, 1.0f);
        float _522 = 1.0f - _519;
        float _523 = min(clamp(2.0020039081573486328125f - _511, 0.0f, 1.0f), _522);
        float _524 = _522 - _523;
        float _525 = min(clamp(999.99993896484375f - (_510 * 999.99993896484375f), 0.0f, 1.0f), _524);
        float _526 = _524 - _525;
        _341 = (((_523 * _449) + (_519 * _334)) + (_525 * (_490 ? _449 : _453))) + (_526 * (_490 ? _449 : (((((((((_402.x + _394.x) + _409.x) + _416.x) + _427.x) + _432.x) + _437.x) + _442.x) + _453) * 0.111111111938953399658203125f)));
        _343 = (((_523 * _450) + (_519 * _294)) + (_525 * (_490 ? _450 : _455))) + (_526 * (_490 ? _450 : (((((((((_402.y + _394.y) + _409.y) + _416.y) + _427.y) + _432.y) + _437.y) + _442.y) + _455) * 0.111111111938953399658203125f)));
        _345 = (((_523 * _451) + (_519 * _295)) + (_525 * (_490 ? _451 : _457))) + (_526 * (_490 ? _451 : (((((((((_402.z + _394.z) + _409.z) + _416.z) + _427.z) + _432.z) + _437.z) + _442.z) + _457) * 0.111111111938953399658203125f)));
    }
    float4 _348 = _18.Sample(_41, float2(TEXCOORD.x, TEXCOORD.y));
    float _363 = (_36_m0[64u].x * (_348.x - _341)) + _341;
    float _364 = (_36_m0[64u].x * (_348.y - _343)) + _343;
    float _365 = (_36_m0[64u].x * (_348.z - _345)) + _345;
    bool _370 = _36_m0[7u].y < 0.0f;
    float _371 = _370 ? 1.0f : TEXCOORD.w;
    float4 _373 = _16.Sample(_41, float2(_141, _142));
    float _378 = _373.x * _371;
    float _379 = _373.y * _371;
    float _380 = _373.z * _371;
    float _564;
    float _565;
    float _566;
    if (_36_m0[75u].z > 0.0f)
    {
        float4 _546 = _19.Sample(_41, float2(_141, _142));
        float _548 = _546.x;
        float _549 = _548 * _548;
        float _550 = _549 * _549;
        float _551 = _550 * _550;
        _564 = (_551 * _36_m0[46u].x) + _363;
        _565 = (_551 * _36_m0[46u].y) + _364;
        _566 = (_551 * _36_m0[46u].z) + _365;
    }
    else
    {
        _564 = _363;
        _565 = _364;
        _566 = _365;
    }
    float4 _568 = _17.Sample(_42, float2(TEXCOORD.x, TEXCOORD.y));
    float _598 = ((((_36_m0[34u].z + (-1.0f)) + ((_36_m0[34u].w - _36_m0[34u].z) * clamp((TEXCOORD.z - _36_m0[34u].x) * _36_m0[34u].y, 0.0f, 1.0f))) * _36_m0[35u].x) + 1.0f) * _36_m0[36u].w;
    float _602 = (_598 * _568.x) + _564;
    float _603 = (_598 * _568.y) + _565;
    float _604 = (_598 * _568.z) + _566;
    float _615 = abs(_36_m0[7u].y);
    precise float _629 = _602 + (_370 ? (((_31_m0[14u].w * _378) - _602) * _615) : ((_378 * 0.25f) * _36_m0[7u].y));
    precise float _631 = _603 + (_370 ? (((_31_m0[14u].w * _379) - _603) * _615) : ((_379 * 0.25f) * _36_m0[7u].y));
    precise float _633 = _604 + (_370 ? (((_31_m0[14u].w * _380) - _604) * _615) : ((_380 * 0.25f) * _36_m0[7u].y));
    float _640 = TEXCOORD.x + (-0.5f);
    float _641 = TEXCOORD.y + (-0.5f);
    float _652 = clamp(clamp(exp2(log2(1.0f - dot(float2(_640, _641), float2(_640, _641))) * _36_m0[57u].y) + _36_m0[57u].x, 0.0f, 1.0f) * _36_m0[57u].z, 0.0f, 1.0f);
    float _681 = clamp((_36_m0[14u].x * TEXCOORD_1) + _36_m0[14u].y, 0.0f, 1.0f);
    float _704 = ((_36_m0[12u].x - _36_m0[10u].x) * _681) + _36_m0[10u].x;
    float _705 = ((_36_m0[12u].y - _36_m0[10u].y) * _681) + _36_m0[10u].y;
    float _707 = ((_36_m0[12u].w - _36_m0[10u].w) * _681) + _36_m0[10u].w;
    float _726 = ((_36_m0[13u].x - _36_m0[11u].x) * _681) + _36_m0[11u].x;
    float _727 = ((_36_m0[13u].y - _36_m0[11u].y) * _681) + _36_m0[11u].y;
    float _728 = ((_36_m0[13u].z - _36_m0[11u].z) * _681) + _36_m0[11u].z;
    float _729 = _728 * _704;
    float _730 = (((_36_m0[12u].z - _36_m0[10u].z) * _681) + _36_m0[10u].z) * _705;
    float _733 = _726 * _707;
    float _737 = _727 * _707;
    float _740 = _726 / _727;
    float _742 = 1.0f / (((((_729 + _730) * _728) + _733) / (((_729 + _705) * _728) + _737)) - _740);
    float _746 = max(0.0f, min((((1.0f - _36_m0[58u].x) * _652) + _36_m0[58u].x) * _629, 65504.0f) * TEXCOORD.z);
    float _747 = max(0.0f, min((((1.0f - _36_m0[58u].y) * _652) + _36_m0[58u].y) * _631, 65504.0f) * TEXCOORD.z);
    float _748 = max(0.0f, min((((1.0f - _36_m0[58u].z) * _652) + _36_m0[58u].z) * _633, 65504.0f) * TEXCOORD.z);
    float _749 = _746 * _704;
    float _750 = _747 * _704;
    float _751 = _748 * _704;
    float _779 = clamp((((((_749 + _730) * _746) + _733) / (((_749 + _705) * _746) + _737)) - _740) * _742, 0.0f, 1.0f);
    float _780 = clamp((((((_750 + _730) * _747) + _733) / (((_750 + _705) * _747) + _737)) - _740) * _742, 0.0f, 1.0f);
    float _781 = clamp((((((_751 + _730) * _748) + _733) / (((_751 + _705) * _748) + _737)) - _740) * _742, 0.0f, 1.0f);
    float _782 = dot(float3(_779, _780, _781), float3(0.2125000059604644775390625f, 0.7153999805450439453125f, 0.07209999859333038330078125f));
    float _799 = (_36_m0[67u].x * (_779 - _782)) + _782;
    float _800 = (_36_m0[67u].x * (_780 - _782)) + _782;
    float _801 = (_36_m0[67u].x * (_781 - _782)) + _782;
    float _807 = clamp(_782 / _36_m0[66u].w, 0.0f, 1.0f);
    float _826 = (((_36_m0[65u].x - _36_m0[66u].x) * _807) + _36_m0[66u].x) * _799;
    float _827 = (((_36_m0[65u].y - _36_m0[66u].y) * _807) + _36_m0[66u].y) * _800;
    float _828 = (((_36_m0[65u].z - _36_m0[66u].z) * _807) + _36_m0[66u].z) * _801;
    float _835 = clamp(((_782 + (-1.0f)) + _36_m0[65u].w) / max(0.00999999977648258209228515625f, _36_m0[65u].w), 0.0f, 1.0f);
    float _882 = (1.0f - (((sin((_36_m0[63u].w + TEXCOORD.y) * _36_m0[63u].y) * 0.5f) + 0.5f) * _36_m0[63u].x)) - (((sin(((_36_m0[63u].w * 0.5f) + TEXCOORD.y) * _36_m0[63u].z) * 0.5f) + 0.5f) * _36_m0[63u].x);
    float _907 = (_14.Sample(_46, float2(frac(((TEXCOORD.x * 1.60000002384185791015625f) * _36_m0[15u].w) + _36_m0[15u].x), frac(((TEXCOORD.y * 0.89999997615814208984375f) * _36_m0[15u].w) + _36_m0[15u].y))).w + (-0.5f)) * _36_m0[15u].z;
    float _914 = clamp(max(0.0f, _907 + (_882 * exp2(log2(abs(clamp(((_799 - _826) * _835) + _826, 0.0f, 1.0f))) * _36_m0[67u].y))), 0.0f, 1.0f);
    float _915 = clamp(max(0.0f, _907 + (_882 * exp2(log2(abs(clamp(((_800 - _827) * _835) + _827, 0.0f, 1.0f))) * _36_m0[67u].y))), 0.0f, 1.0f);
    float _916 = clamp(max(0.0f, _907 + (_882 * exp2(log2(abs(clamp(((_801 - _828) * _835) + _828, 0.0f, 1.0f))) * _36_m0[67u].y))), 0.0f, 1.0f);
    float _929;
    float _931;
    float _933;
    if (asuint(_36_m0[89u].x) == 0u)
    {
        _929 = _914;
        _931 = _915;
        _933 = _916;
    }
    else
    {
        bool _945 = asuint(_36_m0[92u].w) != 0u;
        float _947 = max(_914, max(_915, _916));
        float _1017 = (_8.Load(int4(uint3(uint(gl_FragCoord.x) & 63u, uint(gl_FragCoord.y) & 63u, asuint(_31_m0[22u]).y & 31u), 0u)).x * 2.0f) + (-1.0f);
        float _1023 = float(int(uint(_1017 > 0.0f) - uint(_1017 < 0.0f)));
        float _1027 = 1.0f - sqrt(1.0f - abs(_1017));
        _929 = ((_1027 * (((_36_m0[91u].x - _36_m0[90u].x) * exp2(log2(clamp(((_945 ? _914 : _947) - _36_m0[93u].x) * _36_m0[92u].x, 0.0f, 1.0f)) * _36_m0[93u].w)) + _36_m0[90u].x)) * _1023) + _914;
        _931 = ((_1027 * (((_36_m0[91u].y - _36_m0[90u].y) * exp2(log2(clamp(((_945 ? _915 : _947) - _36_m0[93u].y) * _36_m0[92u].y, 0.0f, 1.0f)) * _36_m0[93u].w)) + _36_m0[90u].y)) * _1023) + _915;
        _933 = ((_1027 * (((_36_m0[91u].z - _36_m0[90u].z) * exp2(log2(clamp(((_945 ? _916 : _947) - _36_m0[93u].z) * _36_m0[92u].z, 0.0f, 1.0f)) * _36_m0[93u].w)) + _36_m0[90u].z)) * _1023) + _916;
    }
    SV_Target.x = _929;
    SV_Target.y = _931;
    SV_Target.z = _933;
    SV_Target.w = dot(float3(_914, _915, _916), float3(0.2989999949932098388671875f, 0.58700001239776611328125f, 0.114000000059604644775390625f));
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    TEXCOORD = stage_input.TEXCOORD;
    TEXCOORD_1 = stage_input.TEXCOORD_1;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.SV_Target = SV_Target;
    return stage_output;
}
