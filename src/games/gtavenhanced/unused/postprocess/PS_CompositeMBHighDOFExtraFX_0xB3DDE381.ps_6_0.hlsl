cbuffer _31_33 : register(b2, space0)
{
    float4 _33_m0[56] : packoffset(c0);
};

cbuffer _36_38 : register(b3, space0)
{
    float4 _38_m0[6] : packoffset(c0);
};

cbuffer _41_43 : register(b5, space0)
{
    float4 _43_m0[26] : packoffset(c0);
};

cbuffer _46_48 : register(b12, space1)
{
    float4 _48_m0[95] : packoffset(c0);
};

Texture2DArray<float4> _8 : register(t1, space0);
Texture2D<float4> _11 : register(t11, space1);
Texture2D<float4> _12 : register(t14, space1);
Texture2D<float4> _13 : register(t15, space1);
Texture2D<float4> _14 : register(t17, space1);
Texture2D<uint4> _18 : register(t18, space1);
Texture2D<float4> _19 : register(t19, space1);
Texture2D<float4> _20 : register(t20, space1);
Texture2D<float4> _21 : register(t22, space1);
Texture2D<float4> _22 : register(t23, space1);
Texture2D<float4> _23 : register(t25, space1);
Texture2D<float4> _24 : register(t28, space1);
Texture2D<float4> _25 : register(t29, space1);
Texture2D<float4> _26 : register(t30, space1);
Texture2D<float4> _27 : register(t31, space1);
SamplerState _51 : register(s0, space1);
SamplerState _52 : register(s1, space1);
SamplerState _53 : register(s2, space1);
SamplerState _54 : register(s3, space1);
SamplerState _57[] : register(s0, space2);
SamplerState _58 : register(s6, space1);
SamplerState _59 : register(s8, space1);

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
    float4 _108 = _11.Sample(_51, float2(TEXCOORD.x, TEXCOORD.y));
    float _120 = _48_m0[0u].z / ((1.0f - _108.x) + _48_m0[0u].w);
    float _136 = min(max(1.0f - clamp(((_48_m0[72u].z / _48_m0[72u].w) * 0.20000000298023223876953125f) + (-0.4000000059604644775390625f), 0.0f, 1.0f), 0.5f), 1.0f);
    float _145 = TEXCOORD.x + (-0.5f);
    float _147 = TEXCOORD.y + (-0.5f);
    float _148 = (_48_m0[72u].x / _48_m0[72u].y) * _145;
    float _149 = dot(float2(_148, _147), float2(_148, _147));
    float _157 = ((_136 * _149) * ((sqrt(_149) * _48_m0[69u].y) + _48_m0[69u].x)) + 1.0f;
    float _158 = _157 * _145;
    float _159 = _157 * _147;
    float _160 = _158 + 0.5f;
    float _161 = _159 + 0.5f;
    float _167 = _160 * _43_m0[15u].x;
    float _168 = _161 * _43_m0[15u].y;
    float _171 = floor(_167 + (-0.5f));
    float _172 = floor(_168 + (-0.5f));
    float _173 = _171 + 0.5f;
    float _174 = _172 + 0.5f;
    float _175 = _167 - _173;
    float _176 = _168 - _174;
    float _177 = _175 * _175;
    float _178 = _176 * _176;
    float _179 = _177 * _175;
    float _180 = _178 * _176;
    float _185 = _177 - ((_179 + _175) * 0.5f);
    float _186 = _178 - ((_180 + _176) * 0.5f);
    float _200 = (_175 * 0.5f) * (_177 - _175);
    float _202 = (_176 * 0.5f) * (_178 - _176);
    float _204 = (1.0f - _200) - _185;
    float _207 = (1.0f - _202) - _186;
    float _219 = (((_204 - (((_179 * 1.5f) - (_177 * 2.5f)) + 1.0f)) / _204) + _173) / _43_m0[15u].x;
    float _220 = (((_207 - (((_180 * 1.5f) - (_178 * 2.5f)) + 1.0f)) / _207) + _174) / _43_m0[15u].y;
    float _223 = _204 * _186;
    float _224 = _207 * _185;
    float _225 = _204 * _207;
    float _226 = _207 * _200;
    float _227 = _204 * _202;
    float _231 = (((_223 + _224) + _225) + _226) + _227;
    float4 _241 = _13.SampleLevel(_57[asuint(_38_m0[1u]).x + 0u], float2(_219, (_172 + (-0.5f)) / _43_m0[15u].y), 0.0f);
    float4 _255 = _13.SampleLevel(_57[asuint(_38_m0[1u]).x + 0u], float2((_171 + (-0.5f)) / _43_m0[15u].x, _220), 0.0f);
    float4 _271 = _13.SampleLevel(_57[asuint(_38_m0[1u]).x + 0u], float2(_219, _220), 0.0f);
    float4 _287 = _13.SampleLevel(_57[asuint(_38_m0[1u]).x + 0u], float2((_171 + 2.5f) / _43_m0[15u].x, _220), 0.0f);
    float4 _303 = _13.SampleLevel(_57[asuint(_38_m0[1u]).x + 0u], float2(_219, (_172 + 2.5f) / _43_m0[15u].y), 0.0f);
    float _313 = max(0.0f, (((((_255.y * _224) + (_241.y * _223)) + (_271.y * _225)) + (_287.y * _226)) + (_303.y * _227)) / _231);
    float _314 = max(0.0f, (((((_255.z * _224) + (_241.z * _223)) + (_271.z * _225)) + (_287.z * _226)) + (_303.z * _227)) / _231);
    float _324 = (_48_m0[72u].x / _48_m0[72u].y) * _158;
    float _325 = dot(float2(_324, _159), float2(_324, _159));
    float _333 = ((_136 * _325) * ((sqrt(_325) * _48_m0[69u].w) + _48_m0[69u].z)) + 1.0f;
    float4 _346 = _13.Sample(_57[asuint(_38_m0[0u]).w + 0u], float2((_333 * _158) + 0.5f, (_333 * _159) + 0.5f));
    float _353 = _43_m0[14u].w * _346.x;
    float _360;
    float _362;
    float _364;
    if (_48_m0[85u].x > 0.0f)
    {
        _360 = _353;
        _362 = _313;
        _364 = _314;
    }
    else
    {
        float _376 = _48_m0[72u].y * 0.5f;
        float _377 = _160 - _48_m0[72u].x;
        float _378 = _161 - _376;
        float4 _380 = _12.Sample(_53, float2(_377, _378));
        float _385 = _48_m0[72u].x * 0.5f;
        float _386 = _385 + _160;
        float _387 = _161 - _48_m0[72u].y;
        float4 _388 = _12.Sample(_53, float2(_386, _387));
        float _393 = _160 - _385;
        float _394 = _48_m0[72u].y + _161;
        float4 _395 = _12.Sample(_53, float2(_393, _394));
        float _400 = _48_m0[72u].x + _160;
        float _401 = _376 + _161;
        float4 _402 = _12.Sample(_53, float2(_400, _401));
        float4 _407 = _12.Sample(_53, float2(_160, _161));
        float4 _413 = _19.Sample(_52, float2(_377, _378));
        float4 _418 = _19.Sample(_52, float2(_386, _387));
        float4 _423 = _19.Sample(_52, float2(_393, _394));
        float4 _428 = _19.Sample(_52, float2(_400, _401));
        float4 _433 = _19.Sample(_52, float2(_160, _161));
        float _435 = _433.x;
        float _436 = _433.y;
        float _437 = _433.z;
        float _439 = (_435 + _407.x) * 0.5f;
        float _441 = (_436 + _407.y) * 0.5f;
        float _443 = (_437 + _407.z) * 0.5f;
        bool _476 = _48_m0[4u].x != 0.0f;
        float _496 = max(clamp(1.0f - ((_120 - _48_m0[3u].x) * _48_m0[3u].y), 0.0f, 1.0f), clamp((_120 - _48_m0[3u].z) * _48_m0[3u].w, 0.0f, 1.0f));
        float _497 = _496 * 2.0040080547332763671875f;
        float _505 = clamp(1.0f - _497, 0.0f, 1.0f);
        float _508 = 1.0f - _505;
        float _509 = min(clamp(2.0020039081573486328125f - _497, 0.0f, 1.0f), _508);
        float _510 = _508 - _509;
        float _511 = min(clamp(999.99993896484375f - (_496 * 999.99993896484375f), 0.0f, 1.0f), _510);
        float _512 = _510 - _511;
        _360 = (((_509 * _435) + (_505 * _353)) + (_511 * (_476 ? _435 : _439))) + (_512 * (_476 ? _435 : (((((((((_388.x + _380.x) + _395.x) + _402.x) + _413.x) + _418.x) + _423.x) + _428.x) + _439) * 0.111111111938953399658203125f)));
        _362 = (((_509 * _436) + (_505 * _313)) + (_511 * (_476 ? _436 : _441))) + (_512 * (_476 ? _436 : (((((((((_388.y + _380.y) + _395.y) + _402.y) + _413.y) + _418.y) + _423.y) + _428.y) + _441) * 0.111111111938953399658203125f)));
        _364 = (((_509 * _437) + (_505 * _314)) + (_511 * (_476 ? _437 : _443))) + (_512 * (_476 ? _437 : (((((((((_388.z + _380.z) + _395.z) + _402.z) + _413.z) + _418.z) + _423.z) + _428.z) + _443) * 0.111111111938953399658203125f)));
    }
    uint _370 = uint(int(_48_m0[17u].z));
    float _776;
    float _779;
    float _782;
    if (_370 == 1u)
    {
        float _554 = ((float(_18.Load(int3(uint2(uint(int(_43_m0[15u].x * _160)), uint(int(_43_m0[15u].y * _161))), 0u)).y) * 0.0039215688593685626983642578125f) > _48_m0[56u].x) ? 0.0f : 1.0f;
        float _558 = (_160 * 2.0f) + (-1.0f);
        float _560 = 1.0f - (_161 * 2.0f);
        float _597 = _33_m0[15u].x + (dot(float3(_558, _560, 1.0f), float3(_48_m0[21u].xyz)) * _120);
        float _598 = _33_m0[15u].y + (dot(float3(_558, _560, 1.0f), float3(_48_m0[22u].xyz)) * _120);
        float _599 = _33_m0[15u].z + (dot(float3(_558, _560, 1.0f), float3(_48_m0[23u].xyz)) * _120);
        float _627 = dot(float4(_597, _598, _599, 1.0f), float4(_48_m0[20u]));
        float _631 = (_627 == 0.0f) ? 9.9999997473787516355514526367188e-06f : _627;
        float _637 = (_558 - (dot(float4(_597, _598, _599, 1.0f), float4(_48_m0[18u])) / _631)) * 40.0f;
        float _639 = (_560 - (dot(float4(_597, _598, _599, 1.0f), float4(_48_m0[19u])) / _631)) * (-22.5f);
        float _641 = dot(float2(_637, _639), float2(_637, _639));
        bool _644 = _641 > 1.0f;
        float _645 = rsqrt(_641);
        float _652 = (_48_m0[16u].x * 0.012500000186264514923095703125f) * (_644 ? (_645 * _637) : _637);
        float _655 = (_48_m0[16u].x * 0.02222222276031970977783203125f) * (_644 ? (_639 * _645) : _639);
        float _656 = _554 * _360;
        float _657 = _554 * _362;
        float _658 = _554 * _364;
        float _660 = _652 * _48_m0[17u].y;
        float _661 = _655 * _48_m0[17u].y;
        float4 _672 = _24.Sample(_58, float2((_360 * 8.0f) + (_160 * 58.16400146484375f), (_362 * 8.0f) + (_161 * 47.130001068115234375f)));
        float _676 = (_672.x + (-0.5f)) * 0.5f;
        float _681;
        float _683;
        float _685;
        float _687;
        if (int(uint(int(_48_m0[17u].x))) > int(1u))
        {
            float _726;
            float _727;
            float _728;
            float _729;
            uint _730;
            _726 = _656;
            _727 = _657;
            _728 = _658;
            _729 = _554;
            _730 = 1u;
            float _682;
            float _684;
            float _686;
            float _688;
            for (;;)
            {
                float _733 = float(int(_730)) + _676;
                float _746 = round(((_660 * _733) + _160) * _43_m0[15u].x) + 0.5f;
                float _747 = round(((_661 * _733) + _161) * _43_m0[15u].y) + 0.5f;
                float _761 = ((float(_18.Load(int3(uint2(uint(int(_746)), uint(int(_747))), 0u)).y) * 0.0039215688593685626983642578125f) > _48_m0[56u].x) ? 0.0f : 1.0f;
                float4 _763 = _19.SampleLevel(_52, float2(_746 / _43_m0[15u].x, _747 / _43_m0[15u].y), 0.0f);
                _682 = (_763.x * _761) + _726;
                _684 = (_763.y * _761) + _727;
                _686 = (_763.z * _761) + _728;
                _688 = _761 + _729;
                uint _731 = _730 + 1u;
                if (int(_731) < int(uint(int(_48_m0[17u].x))))
                {
                    _726 = _682;
                    _727 = _684;
                    _728 = _686;
                    _729 = _688;
                    _730 = _731;
                }
                else
                {
                    break;
                }
            }
            _681 = _682;
            _683 = _684;
            _685 = _686;
            _687 = _688;
        }
        else
        {
            _681 = _656;
            _683 = _657;
            _685 = _658;
            _687 = _554;
        }
        float _689 = max(_687, 0.100000001490116119384765625f);
        float _700 = clamp(dot(float2(_652, _655), float2(_652, _655)) * 100000.0f, 0.0f, 1.0f) * _554;
        _776 = (_700 * ((_681 / _689) - _360)) + _360;
        _779 = (_700 * ((_683 / _689) - _362)) + _362;
        _782 = (_700 * ((_685 / _689) - _364)) + _364;
    }
    else
    {
        float frontier_phi_10_4_ladder;
        float frontier_phi_10_4_ladder_1;
        float frontier_phi_10_4_ladder_2;
        if (_370 == 2u)
        {
            float4 _718 = _22.SampleLevel(_51, float2(_48_m0[88u].x * _160, _48_m0[88u].y * _161), 0.0f);
            float _720 = _718.z;
            float _880;
            float _882;
            float _884;
            float _886;
            if ((_720 >= 1.0f) && (_718.w < 2.0f))
            {
                float4 _827 = _21.SampleLevel(_51, float2(_160, _161), 0.0f);
                float _834 = _48_m0[16u].x * _827.x;
                float _835 = _48_m0[16u].x * _827.y;
                float _841 = min(sqrt((_834 * _834) + (_835 * _835)), _48_m0[16u].z);
                float _844 = min(_720, 2.0f);
                uint _849 = uint(int(min(2.0f, _844 + 1.0f)));
                float _854 = _48_m0[72u].x * (_844 * (_718.x / _720));
                float _855 = _48_m0[72u].y * (_844 * (_718.y / _720));
                float _857 = float(int(_849)) + (-0.5f);
                float _858 = _857 / _844;
                float _876 = ((((float(uint(gl_FragCoord.y) & 1u) * 2.0f) + (-1.0f)) * ((float(uint(gl_FragCoord.x) & 1u) * 2.0f) + (-1.0f))) * _48_m0[16u].w) * clamp((_844 + (-2.0f)) * 0.5f, 0.0f, 1.0f);
                float _877 = _876 + 0.5f;
                float _878 = 0.5f - _876;
                float _1302;
                float _1304;
                float _1306;
                float _1308;
                if (int(_849) > int(0u))
                {
                    float _1507;
                    float _1508;
                    float _1509;
                    float _1510;
                    uint _1511;
                    _1507 = 0.0f;
                    _1508 = 0.0f;
                    _1509 = 0.0f;
                    _1510 = 0.0f;
                    _1511 = 0u;
                    float _1303;
                    float _1305;
                    float _1307;
                    float _1309;
                    for (;;)
                    {
                        float _1513 = float(int(_1511));
                        float _1514 = _877 + _1513;
                        float _1515 = _1514 / _857;
                        float _1518 = (_854 * _1515) + _160;
                        float _1519 = (_855 * _1515) + _161;
                        float4 _1521 = _21.SampleLevel(_51, float2(_1518, _1519), 0.0f);
                        float _1528 = _48_m0[16u].x * _1521.x;
                        float _1529 = _48_m0[16u].x * _1521.y;
                        float _1535 = min(sqrt((_1528 * _1528) + (_1529 * _1529)), _48_m0[16u].z);
                        float _1546 = _48_m0[0u].z / ((1.0f - _11.SampleLevel(_51, float2(_1518, _1519), 0.0f).x) + _48_m0[0u].w);
                        float _1552 = _858 * _841;
                        float _1553 = _1546 - _120;
                        float _1559 = max(_1514 + (-1.0f), 0.0f);
                        float4 _1569 = _19.SampleLevel(_52, float2(_1518, _1519), 0.0f);
                        float _1574 = _1513 + _878;
                        float _1575 = _1574 / _857;
                        float _1578 = _160 - (_854 * _1575);
                        float _1579 = _161 - (_855 * _1575);
                        float4 _1580 = _21.SampleLevel(_51, float2(_1578, _1579), 0.0f);
                        float _1584 = _48_m0[16u].x * _1580.x;
                        float _1585 = _48_m0[16u].x * _1580.y;
                        float _1590 = min(sqrt((_1584 * _1584) + (_1585 * _1585)), _48_m0[16u].z);
                        float _1596 = _48_m0[0u].z / ((1.0f - _11.SampleLevel(_51, float2(_1578, _1579), 0.0f).x) + _48_m0[0u].w);
                        float _1602 = _1596 - _120;
                        float _1608 = max(_1574 + (-1.0f), 0.0f);
                        float _1616 = dot(float2(clamp(_1602 + 0.5f, 0.0f, 1.0f), clamp(0.5f - _1602, 0.0f, 1.0f)), float2(clamp(_1552 - _1608, 0.0f, 1.0f), clamp((_1590 * _858) - _1608, 0.0f, 1.0f))) * (1.0f - clamp((1.0f - _1590) * 8.0f, 0.0f, 1.0f));
                        float4 _1617 = _19.SampleLevel(_52, float2(_1578, _1579), 0.0f);
                        bool _1622 = _1546 > _1596;
                        bool _1623 = _1590 > _1535;
                        float _1625 = (_1623 && _1622) ? _1616 : (dot(float2(clamp(_1553 + 0.5f, 0.0f, 1.0f), clamp(0.5f - _1553, 0.0f, 1.0f)), float2(clamp(_1552 - _1559, 0.0f, 1.0f), clamp((_1535 * _858) - _1559, 0.0f, 1.0f))) * (1.0f - clamp((1.0f - _1535) * 8.0f, 0.0f, 1.0f)));
                        float _1627 = (_1623 || _1622) ? _1616 : _1625;
                        _1303 = ((_1625 * _1569.x) + _1507) + (_1617.x * _1627);
                        _1305 = ((_1625 * _1569.y) + _1508) + (_1617.y * _1627);
                        _1307 = ((_1625 * _1569.z) + _1509) + (_1617.z * _1627);
                        _1309 = (_1625 + _1510) + _1627;
                        uint _1512 = _1511 + 1u;
                        if (_1512 == _849)
                        {
                            break;
                        }
                        else
                        {
                            _1507 = _1303;
                            _1508 = _1305;
                            _1509 = _1307;
                            _1510 = _1309;
                            _1511 = _1512;
                        }
                    }
                    _1302 = _1303;
                    _1304 = _1305;
                    _1306 = _1307;
                    _1308 = _1309;
                }
                else
                {
                    _1302 = 0.0f;
                    _1304 = 0.0f;
                    _1306 = 0.0f;
                    _1308 = 0.0f;
                }
                float _1311 = float(int(_849 << 1u));
                _880 = _1302 / _1311;
                _882 = _1304 / _1311;
                _884 = _1306 / _1311;
                _886 = _1308 / _1311;
            }
            else
            {
                _880 = 0.0f;
                _882 = 0.0f;
                _884 = 0.0f;
                _886 = 0.0f;
            }
            float _888 = 1.0f - _886;
            float4 _896 = _20.SampleLevel(_52, float2(_160, _161), 0.0f);
            float _902 = 1.0f - _896.w;
            frontier_phi_10_4_ladder = (_902 * ((_888 * _364) + _884)) + _896.z;
            frontier_phi_10_4_ladder_1 = (_902 * ((_888 * _362) + _882)) + _896.y;
            frontier_phi_10_4_ladder_2 = (_902 * ((_888 * _360) + _880)) + _896.x;
        }
        else
        {
            float frontier_phi_10_4_ladder_8_ladder;
            float frontier_phi_10_4_ladder_8_ladder_1;
            float frontier_phi_10_4_ladder_8_ladder_2;
            if (_370 == 3u)
            {
                float4 _913 = _22.SampleLevel(_51, float2(_48_m0[88u].x * _160, _48_m0[88u].y * _161), 0.0f);
                float _915 = _913.z;
                float _1378;
                float _1380;
                float _1382;
                float _1384;
                if ((_915 >= 1.0f) && (_913.w < 2.0f))
                {
                    float4 _1315 = _21.SampleLevel(_51, float2(_160, _161), 0.0f);
                    float4 _1327 = _19.Load(int3(uint2(uint(int(_43_m0[15u].x * _160)), uint(int(_43_m0[15u].y * _161))), 0u));
                    float _1329 = _1327.w;
                    float _1333 = _48_m0[16u].x * _1315.x;
                    float _1334 = _48_m0[16u].x * _1315.y;
                    float _1340 = min(sqrt((_1333 * _1333) + (_1334 * _1334)), _48_m0[16u].z);
                    float _1343 = min(_915, 2.0f);
                    uint _1348 = uint(int(min(2.0f, _1343 + 1.0f)));
                    float _1353 = _48_m0[72u].x * (_1343 * (_913.x / _915));
                    float _1354 = _48_m0[72u].y * (_1343 * (_913.y / _915));
                    float _1356 = float(int(_1348)) + (-0.5f);
                    float _1357 = _1356 / _1343;
                    float _1374 = ((((float(uint(gl_FragCoord.y) & 1u) * 2.0f) + (-1.0f)) * ((float(uint(gl_FragCoord.x) & 1u) * 2.0f) + (-1.0f))) * _48_m0[16u].w) * clamp((_1343 + (-2.0f)) * 0.5f, 0.0f, 1.0f);
                    float _1375 = _1374 + 0.5f;
                    float _1376 = 0.5f - _1374;
                    float _1639;
                    float _1641;
                    float _1643;
                    float _1645;
                    if (int(_1348) > int(0u))
                    {
                        float _1649;
                        float _1650;
                        float _1651;
                        float _1652;
                        uint _1653;
                        _1649 = 0.0f;
                        _1650 = 0.0f;
                        _1651 = 0.0f;
                        _1652 = 0.0f;
                        _1653 = 0u;
                        float _1640;
                        float _1642;
                        float _1644;
                        float _1646;
                        for (;;)
                        {
                            float _1655 = float(int(_1653));
                            float _1656 = _1375 + _1655;
                            float _1657 = _1656 / _1356;
                            float _1660 = (_1353 * _1657) + _160;
                            float _1661 = (_1354 * _1657) + _161;
                            float4 _1663 = _21.SampleLevel(_51, float2(_1660, _1661), 0.0f);
                            float _1670 = _48_m0[16u].x * _1663.x;
                            float _1671 = _48_m0[16u].x * _1663.y;
                            float _1677 = min(sqrt((_1670 * _1670) + (_1671 * _1671)), _48_m0[16u].z);
                            float4 _1686 = _19.Load(int3(uint2(uint(int(_43_m0[15u].x * _1660)), uint(int(_43_m0[15u].y * _1661))), 0u));
                            float _1688 = _1686.w;
                            float _1694 = _1357 * _1340;
                            float _1695 = _1688 - _1329;
                            float _1701 = max(_1656 + (-1.0f), 0.0f);
                            float4 _1711 = _19.SampleLevel(_52, float2(_1660, _1661), 0.0f);
                            float _1716 = _1655 + _1376;
                            float _1717 = _1716 / _1356;
                            float _1720 = _160 - (_1353 * _1717);
                            float _1721 = _161 - (_1354 * _1717);
                            float4 _1722 = _21.SampleLevel(_51, float2(_1720, _1721), 0.0f);
                            float _1726 = _48_m0[16u].x * _1722.x;
                            float _1727 = _48_m0[16u].x * _1722.y;
                            float _1732 = min(sqrt((_1726 * _1726) + (_1727 * _1727)), _48_m0[16u].z);
                            float4 _1737 = _19.Load(int3(uint2(uint(int(_43_m0[15u].x * _1720)), uint(int(_43_m0[15u].y * _1721))), 0u));
                            float _1739 = _1737.w;
                            float _1745 = _1739 - _1329;
                            float _1751 = max(_1716 + (-1.0f), 0.0f);
                            float _1759 = dot(float2(clamp(_1745 + 0.5f, 0.0f, 1.0f), clamp(0.5f - _1745, 0.0f, 1.0f)), float2(clamp(_1694 - _1751, 0.0f, 1.0f), clamp((_1732 * _1357) - _1751, 0.0f, 1.0f))) * (1.0f - clamp((1.0f - _1732) * 8.0f, 0.0f, 1.0f));
                            float4 _1760 = _19.SampleLevel(_52, float2(_1720, _1721), 0.0f);
                            bool _1765 = _1688 > _1739;
                            bool _1766 = _1732 > _1677;
                            float _1768 = (_1766 && _1765) ? _1759 : (dot(float2(clamp(_1695 + 0.5f, 0.0f, 1.0f), clamp(0.5f - _1695, 0.0f, 1.0f)), float2(clamp(_1694 - _1701, 0.0f, 1.0f), clamp((_1677 * _1357) - _1701, 0.0f, 1.0f))) * (1.0f - clamp((1.0f - _1677) * 8.0f, 0.0f, 1.0f)));
                            float _1770 = (_1766 || _1765) ? _1759 : _1768;
                            _1640 = ((_1768 * _1711.x) + _1649) + (_1760.x * _1770);
                            _1642 = ((_1768 * _1711.y) + _1650) + (_1760.y * _1770);
                            _1644 = ((_1768 * _1711.z) + _1651) + (_1760.z * _1770);
                            _1646 = (_1768 + _1652) + _1770;
                            uint _1654 = _1653 + 1u;
                            if (_1654 == _1348)
                            {
                                break;
                            }
                            else
                            {
                                _1649 = _1640;
                                _1650 = _1642;
                                _1651 = _1644;
                                _1652 = _1646;
                                _1653 = _1654;
                            }
                        }
                        _1639 = _1640;
                        _1641 = _1642;
                        _1643 = _1644;
                        _1645 = _1646;
                    }
                    else
                    {
                        _1639 = 0.0f;
                        _1641 = 0.0f;
                        _1643 = 0.0f;
                        _1645 = 0.0f;
                    }
                    float _1648 = float(int(_1348 << 1u));
                    _1378 = _1639 / _1648;
                    _1380 = _1641 / _1648;
                    _1382 = _1643 / _1648;
                    _1384 = _1645 / _1648;
                }
                else
                {
                    _1378 = 0.0f;
                    _1380 = 0.0f;
                    _1382 = 0.0f;
                    _1384 = 0.0f;
                }
                float _1386 = 1.0f - _1384;
                float4 _1394 = _20.SampleLevel(_52, float2(_160, _161), 0.0f);
                float _1400 = 1.0f - _1394.w;
                frontier_phi_10_4_ladder_8_ladder = (_1400 * ((_1386 * _364) + _1382)) + _1394.z;
                frontier_phi_10_4_ladder_8_ladder_1 = (_1400 * ((_1386 * _362) + _1380)) + _1394.y;
                frontier_phi_10_4_ladder_8_ladder_2 = (_1400 * ((_1386 * _360) + _1378)) + _1394.x;
            }
            else
            {
                frontier_phi_10_4_ladder_8_ladder = _364;
                frontier_phi_10_4_ladder_8_ladder_1 = _362;
                frontier_phi_10_4_ladder_8_ladder_2 = _360;
            }
            frontier_phi_10_4_ladder = frontier_phi_10_4_ladder_8_ladder;
            frontier_phi_10_4_ladder_1 = frontier_phi_10_4_ladder_8_ladder_1;
            frontier_phi_10_4_ladder_2 = frontier_phi_10_4_ladder_8_ladder_2;
        }
        _776 = frontier_phi_10_4_ladder_2;
        _779 = frontier_phi_10_4_ladder_1;
        _782 = frontier_phi_10_4_ladder;
    }
    float4 _786 = _26.Sample(_53, float2(TEXCOORD.x, TEXCOORD.y));
    float _801 = (_48_m0[64u].x * (_786.x - _776)) + _776;
    float _802 = (_48_m0[64u].x * (_786.y - _779)) + _779;
    float _803 = (_48_m0[64u].x * (_786.z - _782)) + _782;
    bool _808 = _48_m0[7u].y < 0.0f;
    float _809 = _808 ? 1.0f : TEXCOORD.w;
    float4 _811 = _23.Sample(_53, float2(_160, _161));
    float _816 = _811.x * _809;
    float _817 = _811.y * _809;
    float _818 = _811.z * _809;
    float _939;
    float _940;
    float _941;
    if (_48_m0[75u].z > 0.0f)
    {
        float4 _921 = _27.Sample(_53, float2(_160, _161));
        float _923 = _921.x;
        float _924 = _923 * _923;
        float _925 = _924 * _924;
        float _926 = _925 * _925;
        _939 = (_926 * _48_m0[46u].x) + _801;
        _940 = (_926 * _48_m0[46u].y) + _802;
        _941 = (_926 * _48_m0[46u].z) + _803;
    }
    else
    {
        _939 = _801;
        _940 = _802;
        _941 = _803;
    }
    float4 _943 = _25.Sample(_54, float2(TEXCOORD.x, TEXCOORD.y));
    float _972 = ((((_48_m0[34u].z + (-1.0f)) + ((_48_m0[34u].w - _48_m0[34u].z) * clamp((TEXCOORD.z - _48_m0[34u].x) * _48_m0[34u].y, 0.0f, 1.0f))) * _48_m0[35u].x) + 1.0f) * _48_m0[36u].w;
    float _976 = (_972 * _943.x) + _939;
    float _977 = (_972 * _943.y) + _940;
    float _978 = (_972 * _943.z) + _941;
    float _989 = abs(_48_m0[7u].y);
    precise float _1003 = _976 + (_808 ? (((_43_m0[14u].w * _816) - _976) * _989) : ((_816 * 0.25f) * _48_m0[7u].y));
    precise float _1005 = _977 + (_808 ? (((_43_m0[14u].w * _817) - _977) * _989) : ((_817 * 0.25f) * _48_m0[7u].y));
    precise float _1007 = _978 + (_808 ? (((_43_m0[14u].w * _818) - _978) * _989) : ((_818 * 0.25f) * _48_m0[7u].y));
    float _1014 = TEXCOORD.x + (-0.5f);
    float _1015 = TEXCOORD.y + (-0.5f);
    float _1026 = clamp(clamp(exp2(log2(1.0f - dot(float2(_1014, _1015), float2(_1014, _1015))) * _48_m0[57u].y) + _48_m0[57u].x, 0.0f, 1.0f) * _48_m0[57u].z, 0.0f, 1.0f);
    float _1055 = clamp((_48_m0[14u].x * TEXCOORD_1) + _48_m0[14u].y, 0.0f, 1.0f);
    float _1078 = ((_48_m0[12u].x - _48_m0[10u].x) * _1055) + _48_m0[10u].x;
    float _1079 = ((_48_m0[12u].y - _48_m0[10u].y) * _1055) + _48_m0[10u].y;
    float _1081 = ((_48_m0[12u].w - _48_m0[10u].w) * _1055) + _48_m0[10u].w;
    float _1100 = ((_48_m0[13u].x - _48_m0[11u].x) * _1055) + _48_m0[11u].x;
    float _1101 = ((_48_m0[13u].y - _48_m0[11u].y) * _1055) + _48_m0[11u].y;
    float _1102 = ((_48_m0[13u].z - _48_m0[11u].z) * _1055) + _48_m0[11u].z;
    float _1103 = _1102 * _1078;
    float _1104 = (((_48_m0[12u].z - _48_m0[10u].z) * _1055) + _48_m0[10u].z) * _1079;
    float _1107 = _1100 * _1081;
    float _1111 = _1101 * _1081;
    float _1114 = _1100 / _1101;
    float _1116 = 1.0f / (((((_1103 + _1104) * _1102) + _1107) / (((_1103 + _1079) * _1102) + _1111)) - _1114);
    float _1120 = max(0.0f, min((((1.0f - _48_m0[58u].x) * _1026) + _48_m0[58u].x) * _1003, 65504.0f) * TEXCOORD.z);
    float _1121 = max(0.0f, min((((1.0f - _48_m0[58u].y) * _1026) + _48_m0[58u].y) * _1005, 65504.0f) * TEXCOORD.z);
    float _1122 = max(0.0f, min((((1.0f - _48_m0[58u].z) * _1026) + _48_m0[58u].z) * _1007, 65504.0f) * TEXCOORD.z);
    float _1123 = _1120 * _1078;
    float _1124 = _1121 * _1078;
    float _1125 = _1122 * _1078;
    float _1153 = clamp((((((_1123 + _1104) * _1120) + _1107) / (((_1123 + _1079) * _1120) + _1111)) - _1114) * _1116, 0.0f, 1.0f);
    float _1154 = clamp((((((_1124 + _1104) * _1121) + _1107) / (((_1124 + _1079) * _1121) + _1111)) - _1114) * _1116, 0.0f, 1.0f);
    float _1155 = clamp((((((_1125 + _1104) * _1122) + _1107) / (((_1125 + _1079) * _1122) + _1111)) - _1114) * _1116, 0.0f, 1.0f);
    float _1156 = dot(float3(_1153, _1154, _1155), float3(0.2125000059604644775390625f, 0.7153999805450439453125f, 0.07209999859333038330078125f));
    float _1172 = (_48_m0[67u].x * (_1153 - _1156)) + _1156;
    float _1173 = (_48_m0[67u].x * (_1154 - _1156)) + _1156;
    float _1174 = (_48_m0[67u].x * (_1155 - _1156)) + _1156;
    float _1180 = clamp(_1156 / _48_m0[66u].w, 0.0f, 1.0f);
    float _1199 = (((_48_m0[65u].x - _48_m0[66u].x) * _1180) + _48_m0[66u].x) * _1172;
    float _1200 = (((_48_m0[65u].y - _48_m0[66u].y) * _1180) + _48_m0[66u].y) * _1173;
    float _1201 = (((_48_m0[65u].z - _48_m0[66u].z) * _1180) + _48_m0[66u].z) * _1174;
    float _1208 = clamp(((_1156 + (-1.0f)) + _48_m0[65u].w) / max(0.00999999977648258209228515625f, _48_m0[65u].w), 0.0f, 1.0f);
    float _1255 = (1.0f - (((sin((_48_m0[63u].w + TEXCOORD.y) * _48_m0[63u].y) * 0.5f) + 0.5f) * _48_m0[63u].x)) - (((sin(((_48_m0[63u].w * 0.5f) + TEXCOORD.y) * _48_m0[63u].z) * 0.5f) + 0.5f) * _48_m0[63u].x);
    float _1280 = (_14.Sample(_59, float2(frac(((TEXCOORD.x * 1.60000002384185791015625f) * _48_m0[15u].w) + _48_m0[15u].x), frac(((TEXCOORD.y * 0.89999997615814208984375f) * _48_m0[15u].w) + _48_m0[15u].y))).w + (-0.5f)) * _48_m0[15u].z;
    float _1287 = clamp(max(0.0f, _1280 + (_1255 * exp2(log2(abs(clamp(((_1172 - _1199) * _1208) + _1199, 0.0f, 1.0f))) * _48_m0[67u].y))), 0.0f, 1.0f);
    float _1288 = clamp(max(0.0f, _1280 + (_1255 * exp2(log2(abs(clamp(((_1173 - _1200) * _1208) + _1200, 0.0f, 1.0f))) * _48_m0[67u].y))), 0.0f, 1.0f);
    float _1289 = clamp(max(0.0f, _1280 + (_1255 * exp2(log2(abs(clamp(((_1174 - _1201) * _1208) + _1201, 0.0f, 1.0f))) * _48_m0[67u].y))), 0.0f, 1.0f);
    float _1404;
    float _1406;
    float _1408;
    if (asuint(_48_m0[89u].x) == 0u)
    {
        _1404 = _1287;
        _1406 = _1288;
        _1408 = _1289;
    }
    else
    {
        bool _1420 = asuint(_48_m0[92u].w) != 0u;
        float _1422 = max(_1287, max(_1288, _1289));
        float _1490 = (_8.Load(int4(uint3(uint(gl_FragCoord.x) & 63u, uint(gl_FragCoord.y) & 63u, asuint(_43_m0[22u]).y & 31u), 0u)).x * 2.0f) + (-1.0f);
        float _1496 = float(int(uint(_1490 > 0.0f) - uint(_1490 < 0.0f)));
        float _1500 = 1.0f - sqrt(1.0f - abs(_1490));
        _1404 = ((_1500 * (((_48_m0[91u].x - _48_m0[90u].x) * exp2(log2(clamp(((_1420 ? _1287 : _1422) - _48_m0[93u].x) * _48_m0[92u].x, 0.0f, 1.0f)) * _48_m0[93u].w)) + _48_m0[90u].x)) * _1496) + _1287;
        _1406 = ((_1500 * (((_48_m0[91u].y - _48_m0[90u].y) * exp2(log2(clamp(((_1420 ? _1288 : _1422) - _48_m0[93u].y) * _48_m0[92u].y, 0.0f, 1.0f)) * _48_m0[93u].w)) + _48_m0[90u].y)) * _1496) + _1288;
        _1408 = ((_1500 * (((_48_m0[91u].z - _48_m0[90u].z) * exp2(log2(clamp(((_1420 ? _1289 : _1422) - _48_m0[93u].z) * _48_m0[92u].z, 0.0f, 1.0f)) * _48_m0[93u].w)) + _48_m0[90u].z)) * _1496) + _1289;
    }
    SV_Target.x = _1404;
    SV_Target.y = _1406;
    SV_Target.z = _1408;
    SV_Target.w = dot(float3(_1287, _1288, _1289), float3(0.2989999949932098388671875f, 0.58700001239776611328125f, 0.114000000059604644775390625f));
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
