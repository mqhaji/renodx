cbuffer _13_15 : register(b0, space5)
{
    float4 _15_m0[10] : packoffset(c0);
};

RWTexture3D<float4> _8 : register(u0, space5);

static uint3 gl_GlobalInvocationID;
struct SPIRV_Cross_Input
{
    uint3 gl_GlobalInvocationID : SV_DispatchThreadID;
};

void comp_main()
{
    float _33 = float(gl_GlobalInvocationID.x) * 0.0322580635547637939453125f;
    float _35 = float(gl_GlobalInvocationID.y) * 0.0322580635547637939453125f;
    float _36 = float(gl_GlobalInvocationID.z) * 0.0322580635547637939453125f;
    uint4 _42 = asuint(_15_m0[8u]);
    float _173;
    float _174;
    float _175;
    if (_42.x == 0u)    // HDR
    {
        float _49 = clamp(_33, 0.0f, 1.0f);
        float _50 = clamp(_35, 0.0f, 1.0f);
        float _51 = clamp(_36, 0.0f, 1.0f);
#if 1
        float _84 = pow(_49, 2.2f);
        float _85 = pow(_50, 2.2f);
        float _86 = pow(_51, 2.2f);
#else
        float _84 = _15_m0[8u].w * ((_49 <= 0.040449999272823333740234375f) ? (_49 * 0.077399380505084991455078125f) : exp2(log2((_49 + 0.054999999701976776123046875f) * 0.947867333889007568359375f) * 2.400000095367431640625f));
        float _85 = _15_m0[8u].w * ((_50 <= 0.040449999272823333740234375f) ? (_50 * 0.077399380505084991455078125f) : exp2(log2((_50 + 0.054999999701976776123046875f) * 0.947867333889007568359375f) * 2.400000095367431640625f));
        float _86 = _15_m0[8u].w * ((_51 <= 0.040449999272823333740234375f) ? (_51 * 0.077399380505084991455078125f) : exp2(log2((_51 + 0.054999999701976776123046875f) * 0.947867333889007568359375f) * 2.400000095367431640625f));
#endif
        _173 = mad(_86, 0.047379434108734130859375f, mad(_85, 0.339523255825042724609375f, _84 * 0.6130974292755126953125f));
        _174 = mad(_86, 0.013452420942485332489013671875f, mad(_85, 0.916354238986968994140625f, _84 * 0.07019375264644622802734375f));
        _175 = mad(_86, 0.869814455509185791015625f, mad(_85, 0.1095697581768035888671875f, _84 * 0.02061559818685054779052734375f));
    }
    else
    {
        float _109 = exp2(log2(abs(_33)) * 0.0126833133399486541748046875f);
        float _110 = _109 + (-0.8359375f);
        float _123 = exp2(log2(abs(((_110 < 0.0f) ? 0.0f : _110) / (18.8515625f - (_109 * 18.6875f)))) * 6.277394771575927734375f);
        float _127 = exp2(log2(abs(_35)) * 0.0126833133399486541748046875f);
        float _128 = _127 + (-0.8359375f);
        float _138 = exp2(log2(abs(((_128 < 0.0f) ? 0.0f : _128) / (18.8515625f - (_127 * 18.6875f)))) * 6.277394771575927734375f) * 10000.0f;
        float _143 = exp2(log2(abs(_36)) * 0.0126833133399486541748046875f);
        float _144 = _143 + (-0.8359375f);
        float _154 = exp2(log2(abs(((_144 < 0.0f) ? 0.0f : _144) / (18.8515625f - (_143 * 18.6875f)))) * 6.277394771575927734375f) * 10000.0f;
        _173 = mad(_154, 0.0055058780126273632049560546875f, mad(_138, 0.01959916017949581146240234375f, _123 * 9748.951171875f));
        _174 = mad(_154, 0.0022850031964480876922607421875f, mad(_138, 0.99553549289703369140625f, _123 * 21.795501708984375f));
        _175 = mad(_154, 0.9706707000732421875f, mad(_138, 0.0245320089161396026611328125f, _123 * 47.972553253173828125f));
    }
    float _250;
    float _254;
    float _258;
    switch (_42.y)
    {
        case 2u:
        {
            float _179 = abs(_173 * 0.00999999977648258209228515625f);
            float _183 = _15_m0[8u].w * 0.00999999977648258209228515625f;
            float _196 = (_183 - _15_m0[5u].y) * _15_m0[5u].z;
            float _197 = _196 / _15_m0[5u].x;
            float _198 = _196 + _15_m0[5u].y;
            float _703;
            if (_179 < _15_m0[5u].y)
            {
                float _697;
                float _699;
                if ((_15_m0[5u].y - _15_m0[6u].x) > 9.9999997473787516355514526367188e-06f)
                {
                    float _840;
                    float _841;
                    _840 = _15_m0[6u].x;
                    _841 = _15_m0[5u].y;
                    float _698;
                    float _700;
                    for (;;)
                    {
                        float _843 = (_841 + _840) * 0.5f;
                        float _847 = _843 / _15_m0[5u].y;
                        float _854 = clamp(_847, 0.0f, 1.0f);
                        float _858 = (_854 * _854) * (3.0f - (_854 * 2.0f));
                        bool _863 = (((1.0f - _858) * ((exp2(log2(abs(_847)) * _15_m0[5u].w) * _15_m0[5u].y) + _15_m0[6u].x)) + (_858 * (((_843 - _15_m0[5u].y) * _15_m0[5u].x) + _15_m0[5u].y))) > _179;
                        _698 = _863 ? _840 : _843;
                        _700 = _863 ? _843 : _841;
                        if ((_700 - _698) > 9.9999997473787516355514526367188e-06f)
                        {
                            _840 = _698;
                            _841 = _700;
                        }
                        else
                        {
                            break;
                        }
                    }
                    _697 = _698;
                    _699 = _700;
                }
                else
                {
                    _697 = _15_m0[6u].x;
                    _699 = _15_m0[5u].y;
                }
                _703 = (_699 + _697) * 0.5f;
            }
            else
            {
                float _302 = _197 + _15_m0[5u].y;
                _703 = (_179 > _302) ? (_302 + (((_179 - _198) * _183) / (max(_183 - _179, 9.9999997473787516355514526367188e-06f) * ((_15_m0[5u].x * _183) / (_183 - _198))))) : (((_179 - _15_m0[5u].y) / _15_m0[5u].x) + _15_m0[5u].y);
            }
            float _705 = abs(_174 * 0.00999999977648258209228515625f);
            float _956;
            if (_705 < _15_m0[5u].y)
            {
                float _950;
                float _952;
                if ((_15_m0[5u].y - _15_m0[6u].x) > 9.9999997473787516355514526367188e-06f)
                {
                    float _978;
                    float _979;
                    _978 = _15_m0[6u].x;
                    _979 = _15_m0[5u].y;
                    float _951;
                    float _953;
                    for (;;)
                    {
                        float _981 = (_979 + _978) * 0.5f;
                        float _985 = _981 / _15_m0[5u].y;
                        float _992 = clamp(_985, 0.0f, 1.0f);
                        float _996 = (_992 * _992) * (3.0f - (_992 * 2.0f));
                        bool _1001 = (((1.0f - _996) * ((exp2(log2(abs(_985)) * _15_m0[5u].w) * _15_m0[5u].y) + _15_m0[6u].x)) + (_996 * (((_981 - _15_m0[5u].y) * _15_m0[5u].x) + _15_m0[5u].y))) > _705;
                        _951 = _1001 ? _978 : _981;
                        _953 = _1001 ? _981 : _979;
                        if ((_953 - _951) > 9.9999997473787516355514526367188e-06f)
                        {
                            _978 = _951;
                            _979 = _953;
                        }
                        else
                        {
                            break;
                        }
                    }
                    _950 = _951;
                    _952 = _953;
                }
                else
                {
                    _950 = _15_m0[6u].x;
                    _952 = _15_m0[5u].y;
                }
                _956 = (_952 + _950) * 0.5f;
            }
            else
            {
                float _868 = _197 + _15_m0[5u].y;
                _956 = (_705 > _868) ? (_868 + (((_705 - _198) * _183) / (max(_183 - _705, 9.9999997473787516355514526367188e-06f) * ((_15_m0[5u].x * _183) / (_183 - _198))))) : (((_705 - _15_m0[5u].y) / _15_m0[5u].x) + _15_m0[5u].y);
            }
            float _958 = abs(_175 * 0.00999999977648258209228515625f);
            float _1089;
            if (_958 < _15_m0[5u].y)
            {
                float _1083;
                float _1085;
                if ((_15_m0[5u].y - _15_m0[6u].x) > 9.9999997473787516355514526367188e-06f)
                {
                    float _1103;
                    float _1104;
                    _1103 = _15_m0[6u].x;
                    _1104 = _15_m0[5u].y;
                    float _1084;
                    float _1086;
                    for (;;)
                    {
                        float _1106 = (_1104 + _1103) * 0.5f;
                        float _1110 = _1106 / _15_m0[5u].y;
                        float _1117 = clamp(_1110, 0.0f, 1.0f);
                        float _1121 = (_1117 * _1117) * (3.0f - (_1117 * 2.0f));
                        bool _1126 = (((1.0f - _1121) * ((exp2(log2(abs(_1110)) * _15_m0[5u].w) * _15_m0[5u].y) + _15_m0[6u].x)) + (_1121 * (((_1106 - _15_m0[5u].y) * _15_m0[5u].x) + _15_m0[5u].y))) > _958;
                        _1084 = _1126 ? _1103 : _1106;
                        _1086 = _1126 ? _1106 : _1104;
                        if ((_1086 - _1084) > 9.9999997473787516355514526367188e-06f)
                        {
                            _1103 = _1084;
                            _1104 = _1086;
                        }
                        else
                        {
                            break;
                        }
                    }
                    _1083 = _1084;
                    _1085 = _1086;
                }
                else
                {
                    _1083 = _15_m0[6u].x;
                    _1085 = _15_m0[5u].y;
                }
                _1089 = (_1085 + _1083) * 0.5f;
            }
            else
            {
                float _1006 = _197 + _15_m0[5u].y;
                _1089 = (_958 > _1006) ? (_1006 + (((_958 - _198) * _183) / (max(_183 - _958, 9.9999997473787516355514526367188e-06f) * ((_15_m0[5u].x * _183) / (_183 - _198))))) : (((_958 - _15_m0[5u].y) / _15_m0[5u].x) + _15_m0[5u].y);
            }
            _250 = _703 * 100.0f;
            _254 = _956 * 100.0f;
            _258 = _1089 * 100.0f;
            break;
        }
        case 3u:
        {
            float _203 = abs(_15_m0[8u].w);
            float _213 = abs(_173 / _203);
            float _214 = log2(abs(_15_m0[7u].x));
            float _217 = _15_m0[7u].w * _15_m0[7u].z;
            float _219 = exp2(_217 * _214);
            float _220 = log2(_203);
            float _222 = exp2(_220 * _15_m0[7u].z);
            float _224 = exp2(_217 * _220);
            float _226 = (_224 - _219) * _15_m0[7u].y;
            float _229 = ((_222 * _15_m0[7u].y) - exp2(_214 * _15_m0[7u].z)) / _226;
            float _234 = ((_224 * _219) - ((_219 * _15_m0[7u].y) * _222)) / _226;
            float _235 = 1.0f / _15_m0[7u].z;
            float _237 = exp2(_220 * _235);
            float _318;
            float _320;
            uint _322;
            _318 = 0.0f;
            _320 = _237;
            _322 = 0u;
            float _319;
            float _321;
            for (;;)
            {
                float _325 = (_320 + _318) * 0.5f;
                bool _334 = (_325 / ((exp2(log2(abs(_325)) * _15_m0[7u].w) * _229) + _234)) > _213;
                _319 = _334 ? _318 : _325;
                _321 = _334 ? _325 : _320;
                uint _323 = _322 + 1u;
                if (_323 == 32u)
                {
                    break;
                }
                else
                {
                    _318 = _319;
                    _320 = _321;
                    _322 = _323;
                }
            }
            float _714 = abs(_174 / _203);
            float _884;
            float _886;
            uint _888;
            _884 = 0.0f;
            _886 = _237;
            _888 = 0u;
            float _885;
            float _887;
            for (;;)
            {
                float _891 = (_886 + _884) * 0.5f;
                bool _899 = (_891 / ((exp2(log2(abs(_891)) * _15_m0[7u].w) * _229) + _234)) > _714;
                _885 = _899 ? _884 : _891;
                _887 = _899 ? _891 : _886;
                uint _889 = _888 + 1u;
                if (_889 == 32u)
                {
                    break;
                }
                else
                {
                    _884 = _885;
                    _886 = _887;
                    _888 = _889;
                }
            }
            float _967 = abs(_175 / _203);
            float _1022;
            float _1024;
            uint _1026;
            _1022 = 0.0f;
            _1024 = _237;
            _1026 = 0u;
            float _1023;
            float _1025;
            for (;;)
            {
                float _1029 = (_1024 + _1022) * 0.5f;
                bool _1037 = (_1029 / ((exp2(log2(abs(_1029)) * _15_m0[7u].w) * _229) + _234)) > _967;
                _1023 = _1037 ? _1022 : _1029;
                _1025 = _1037 ? _1029 : _1024;
                uint _1027 = _1026 + 1u;
                if (_1027 == 32u)
                {
                    break;
                }
                else
                {
                    _1022 = _1023;
                    _1024 = _1025;
                    _1026 = _1027;
                }
            }
            _250 = exp2(log2(abs((_319 + _321) * 0.5f)) * _235) * _203;
            _254 = exp2(log2(abs((_885 + _887) * 0.5f)) * _235) * _203;
            _258 = exp2(log2(abs((_1023 + _1025) * 0.5f)) * _235) * _203;
            break;
        }
        case 4u:
        {
            _250 = _173 / (1.0f - (_173 / _15_m0[8u].w));
            _254 = _174 / (1.0f - (_174 / _15_m0[8u].w));
            _258 = _175 / (1.0f - (_175 / _15_m0[8u].w));
            break;
        }
        case 1u:
        {
            float _280 = abs(_173 * 0.00999999977648258209228515625f);
            float _284 = _15_m0[8u].w * 0.00999999977648258209228515625f;
            float _295 = (_284 - _15_m0[5u].y) * _15_m0[5u].z;
            float _296 = _295 / _15_m0[5u].x;
            float _297 = _295 + _15_m0[5u].y;
            float _836;
            if (_280 < _15_m0[5u].y)
            {
                float _830;
                float _832;
                if ((_15_m0[5u].y - _15_m0[6u].x) > 9.9999997473787516355514526367188e-06f)
                {
                    float _906;
                    float _907;
                    _906 = _15_m0[6u].x;
                    _907 = _15_m0[5u].y;
                    float _831;
                    float _833;
                    for (;;)
                    {
                        float _909 = (_907 + _906) * 0.5f;
                        float _913 = _909 / _15_m0[5u].y;
                        float _919 = clamp(_913, 0.0f, 1.0f);
                        float _923 = (_919 * _919) * (3.0f - (_919 * 2.0f));
                        bool _928 = (((1.0f - _923) * ((exp2(log2(_913) * _15_m0[5u].w) * _15_m0[5u].y) + _15_m0[6u].x)) + (_923 * (((_909 - _15_m0[5u].y) * _15_m0[5u].x) + _15_m0[5u].y))) > _280;
                        _831 = _928 ? _906 : _909;
                        _833 = _928 ? _909 : _907;
                        if ((_833 - _831) > 9.9999997473787516355514526367188e-06f)
                        {
                            _906 = _831;
                            _907 = _833;
                        }
                        else
                        {
                            break;
                        }
                    }
                    _830 = _831;
                    _832 = _833;
                }
                else
                {
                    _830 = _15_m0[6u].x;
                    _832 = _15_m0[5u].y;
                }
                _836 = (_832 + _830) * 0.5f;
            }
            else
            {
                float _679 = _296 + _15_m0[5u].y;
                float _681 = _284 - _297;
                _836 = (_280 > _679) ? (_679 - ((((_15_m0[8u].w * 0.006931471638381481170654296875f) * _681) * log2(max((_284 - _280) / _681, 9.9999997473787516355514526367188e-06f))) / (_15_m0[5u].x * _284))) : (((_280 - _15_m0[5u].y) / _15_m0[5u].x) + _15_m0[5u].y);
            }
            float _838 = abs(_174 * 0.00999999977648258209228515625f);
            float _974;
            if (_838 < _15_m0[5u].y)
            {
                float _968;
                float _970;
                if ((_15_m0[5u].y - _15_m0[6u].x) > 9.9999997473787516355514526367188e-06f)
                {
                    float _1039;
                    float _1040;
                    _1039 = _15_m0[6u].x;
                    _1040 = _15_m0[5u].y;
                    float _969;
                    float _971;
                    for (;;)
                    {
                        float _1042 = (_1040 + _1039) * 0.5f;
                        float _1046 = _1042 / _15_m0[5u].y;
                        float _1052 = clamp(_1046, 0.0f, 1.0f);
                        float _1056 = (_1052 * _1052) * (3.0f - (_1052 * 2.0f));
                        bool _1061 = (((1.0f - _1056) * ((exp2(log2(_1046) * _15_m0[5u].w) * _15_m0[5u].y) + _15_m0[6u].x)) + (_1056 * (((_1042 - _15_m0[5u].y) * _15_m0[5u].x) + _15_m0[5u].y))) > _838;
                        _969 = _1061 ? _1039 : _1042;
                        _971 = _1061 ? _1042 : _1040;
                        if ((_971 - _969) > 9.9999997473787516355514526367188e-06f)
                        {
                            _1039 = _969;
                            _1040 = _971;
                        }
                        else
                        {
                            break;
                        }
                    }
                    _968 = _969;
                    _970 = _971;
                }
                else
                {
                    _968 = _15_m0[6u].x;
                    _970 = _15_m0[5u].y;
                }
                _974 = (_970 + _968) * 0.5f;
            }
            else
            {
                float _933 = _296 + _15_m0[5u].y;
                float _935 = _284 - _297;
                _974 = (_838 > _933) ? (_933 - ((((_15_m0[8u].w * 0.006931471638381481170654296875f) * _935) * log2(max((_284 - _838) / _935, 9.9999997473787516355514526367188e-06f))) / (_15_m0[5u].x * _284))) : (((_838 - _15_m0[5u].y) / _15_m0[5u].x) + _15_m0[5u].y);
            }
            float _976 = abs(_175 * 0.00999999977648258209228515625f);
            float _1102;
            if (_976 < _15_m0[5u].y)
            {
                float _1096;
                float _1098;
                if ((_15_m0[5u].y - _15_m0[6u].x) > 9.9999997473787516355514526367188e-06f)
                {
                    float _1129;
                    float _1130;
                    _1129 = _15_m0[6u].x;
                    _1130 = _15_m0[5u].y;
                    float _1097;
                    float _1099;
                    for (;;)
                    {
                        float _1132 = (_1130 + _1129) * 0.5f;
                        float _1136 = _1132 / _15_m0[5u].y;
                        float _1142 = clamp(_1136, 0.0f, 1.0f);
                        float _1146 = (_1142 * _1142) * (3.0f - (_1142 * 2.0f));
                        bool _1151 = (((1.0f - _1146) * ((exp2(log2(_1136) * _15_m0[5u].w) * _15_m0[5u].y) + _15_m0[6u].x)) + (_1146 * (((_1132 - _15_m0[5u].y) * _15_m0[5u].x) + _15_m0[5u].y))) > _976;
                        _1097 = _1151 ? _1129 : _1132;
                        _1099 = _1151 ? _1132 : _1130;
                        if ((_1099 - _1097) > 9.9999997473787516355514526367188e-06f)
                        {
                            _1129 = _1097;
                            _1130 = _1099;
                        }
                        else
                        {
                            break;
                        }
                    }
                    _1096 = _1097;
                    _1098 = _1099;
                }
                else
                {
                    _1096 = _15_m0[6u].x;
                    _1098 = _15_m0[5u].y;
                }
                _1102 = (_1098 + _1096) * 0.5f;
            }
            else
            {
                float _1066 = _296 + _15_m0[5u].y;
                float _1068 = _284 - _297;
                _1102 = (_976 > _1066) ? (_1066 - ((((_15_m0[8u].w * 0.006931471638381481170654296875f) * _1068) * log2(max((_284 - _976) / _1068, 9.9999997473787516355514526367188e-06f))) / (_15_m0[5u].x * _284))) : (((_976 - _15_m0[5u].y) / _15_m0[5u].x) + _15_m0[5u].y);
            }
            _250 = _836 * 100.0f;
            _254 = _974 * 100.0f;
            _258 = _1102 * 100.0f;
            break;
        }
        default:
        {
            _250 = 0.0f;
            _254 = 0.0f;
            _258 = 0.0f;
            break;
        }
        case 5u:
        case 6u:
        {
            _250 = _173;
            _254 = _174;
            _258 = _175;
            break;
        }
    }
    float _272 = _15_m0[3u].z * (_250 / _15_m0[8u].z);
    float _273 = _15_m0[3u].z * (_254 / _15_m0[8u].z);
    float _274 = _15_m0[3u].z * (_258 / _15_m0[8u].z);
    uint4 _277 = asuint(_15_m0[3u]);
    float _538;
    float _541;
    float _544;
    switch (_277.y)
    {
        case 2u:
        {
            float _338 = abs(_272 * 0.00999999977648258209228515625f);
            float _340 = _15_m0[3u].w * 0.00999999977648258209228515625f;
            float _351 = (_340 - _15_m0[0u].y) * _15_m0[0u].z;
            float _352 = _351 / _15_m0[0u].x;
            float _353 = _338 - _15_m0[0u].y;
            bool _356 = _15_m0[0u].y > 9.9999997473787516355514526367188e-06f;
            float _357 = _338 / _15_m0[0u].y;
            float _367 = _340 - (_351 + _15_m0[0u].y);
            float _368 = (_15_m0[0u].x * _340) / _367;
            float _375 = clamp(_357, 0.0f, 1.0f);
            float _381 = (_375 * _375) * (3.0f - (_375 * 2.0f));
            float _383 = _352 + _15_m0[0u].y;
            float _385 = float(_338 > _383);
            float _395 = abs(_273 * 0.00999999977648258209228515625f);
            float _396 = _395 - _15_m0[0u].y;
            float _399 = _395 / _15_m0[0u].y;
            float _413 = clamp(_399, 0.0f, 1.0f);
            float _417 = (_413 * _413) * (3.0f - (_413 * 2.0f));
            float _420 = float(_395 > _383);
            float _429 = abs(_274 * 0.00999999977648258209228515625f);
            float _430 = _429 - _15_m0[0u].y;
            float _433 = _429 / _15_m0[0u].y;
            float _447 = clamp(_433, 0.0f, 1.0f);
            float _451 = (_447 * _447) * (3.0f - (_447 * 2.0f));
            float _454 = float(_429 > _383);
            _538 = ((((_381 - _385) * ((_353 * _15_m0[0u].x) + _15_m0[0u].y)) + ((_340 - (_367 / (((_368 * (_353 - _352)) / _340) + 1.0f))) * _385)) + ((1.0f - _381) * (_356 ? ((exp2(log2(abs(_357)) * _15_m0[0u].w) * _15_m0[0u].y) + _15_m0[1u].x) : _15_m0[1u].x))) * 100.0f;
            _541 = ((((_417 - _420) * ((_396 * _15_m0[0u].x) + _15_m0[0u].y)) + ((_340 - (_367 / (((_368 * (_396 - _352)) / _340) + 1.0f))) * _420)) + ((1.0f - _417) * (_356 ? ((exp2(log2(abs(_399)) * _15_m0[0u].w) * _15_m0[0u].y) + _15_m0[1u].x) : _15_m0[1u].x))) * 100.0f;
            _544 = ((((_451 - _454) * ((_430 * _15_m0[0u].x) + _15_m0[0u].y)) + ((_340 - (_367 / (((_368 * (_430 - _352)) / _340) + 1.0f))) * _454)) + ((1.0f - _451) * (_356 ? ((exp2(log2(abs(_433)) * _15_m0[0u].w) * _15_m0[0u].y) + _15_m0[1u].x) : _15_m0[1u].x))) * 100.0f;
            break;
        }
        case 3u:
        {
            float _463 = abs(_15_m0[3u].w);
            float _473 = log2(abs(_15_m0[2u].x));
            float _476 = _15_m0[2u].w * _15_m0[2u].z;
            float _478 = exp2(_476 * _473);
            float _479 = log2(_463);
            float _481 = exp2(_479 * _15_m0[2u].z);
            float _483 = exp2(_476 * _479);
            float _485 = (_483 - _478) * _15_m0[2u].y;
            float _488 = ((_481 * _15_m0[2u].y) - exp2(_473 * _15_m0[2u].z)) / _485;
            float _493 = ((_483 * _478) - ((_478 * _15_m0[2u].y) * _481)) / _485;
            float _496 = exp2(log2(abs(_272 / _463)) * _15_m0[2u].z);
            float _508 = exp2(log2(abs(_273 / _463)) * _15_m0[2u].z);
            float _520 = exp2(log2(abs(_274 / _463)) * _15_m0[2u].z);
            _538 = _15_m0[3u].w * (_496 / ((exp2(log2(_496) * _15_m0[2u].w) * _488) + _493));
            _541 = _15_m0[3u].w * (_508 / ((exp2(log2(_508) * _15_m0[2u].w) * _488) + _493));
            _544 = _15_m0[3u].w * (_520 / ((exp2(log2(_520) * _15_m0[2u].w) * _488) + _493));
            break;
        }
        case 4u:
        {
            _538 = _272 / ((_272 / _15_m0[3u].w) + 1.0f);
            _541 = _273 / ((_273 / _15_m0[3u].w) + 1.0f);
            _544 = _274 / ((_274 / _15_m0[3u].w) + 1.0f);
            break;
        }
        case 6u:
        {
            _538 = min(_272, _15_m0[3u].w);
            _541 = min(_273, _15_m0[3u].w);
            _544 = min(_274, _15_m0[3u].w);
            break;
        }
        case 1u:
        {
            float _551 = abs(_272 * 0.00999999977648258209228515625f);
            float _553 = _15_m0[3u].w * 0.00999999977648258209228515625f;
            float _564 = (_553 - _15_m0[0u].y) * _15_m0[0u].z;
            float _565 = _564 / _15_m0[0u].x;
            float _566 = _551 - _15_m0[0u].y;
            bool _569 = _15_m0[0u].y > 9.9999997473787516355514526367188e-06f;
            float _570 = _551 / _15_m0[0u].y;
            float _580 = _553 - (_564 + _15_m0[0u].y);
            float _581 = (_15_m0[0u].x * _553) / _580;
            float _592 = clamp(_570, 0.0f, 1.0f);
            float _596 = (_592 * _592) * (3.0f - (_592 * 2.0f));
            float _598 = _565 + _15_m0[0u].y;
            float _600 = float(_551 > _598);
            float _608 = abs(_273 * 0.00999999977648258209228515625f);
            float _609 = _608 - _15_m0[0u].y;
            float _612 = _608 / _15_m0[0u].y;
            float _628 = clamp(_612, 0.0f, 1.0f);
            float _632 = (_628 * _628) * (3.0f - (_628 * 2.0f));
            float _635 = float(_608 > _598);
            float _643 = abs(_274 * 0.00999999977648258209228515625f);
            float _644 = _643 - _15_m0[0u].y;
            float _647 = _643 / _15_m0[0u].y;
            float _663 = clamp(_647, 0.0f, 1.0f);
            float _667 = (_663 * _663) * (3.0f - (_663 * 2.0f));
            float _670 = float(_643 > _598);
            _538 = ((((_596 - _600) * ((_566 * _15_m0[0u].x) + _15_m0[0u].y)) + ((_553 - (exp2((((-0.0f) - ((_566 - _565) * _581)) / _553) * 1.44269502162933349609375f) * _580)) * _600)) + ((1.0f - _596) * (_569 ? ((exp2(log2(abs(_570)) * _15_m0[0u].w) * _15_m0[0u].y) + _15_m0[1u].x) : _15_m0[1u].x))) * 100.0f;
            _541 = ((((_632 - _635) * ((_609 * _15_m0[0u].x) + _15_m0[0u].y)) + ((_553 - (exp2((((-0.0f) - ((_609 - _565) * _581)) / _553) * 1.44269502162933349609375f) * _580)) * _635)) + ((1.0f - _632) * (_569 ? ((exp2(log2(abs(_612)) * _15_m0[0u].w) * _15_m0[0u].y) + _15_m0[1u].x) : _15_m0[1u].x))) * 100.0f;
            _544 = ((((_667 - _670) * ((_644 * _15_m0[0u].x) + _15_m0[0u].y)) + ((_553 - (exp2((((-0.0f) - ((_644 - _565) * _581)) / _553) * 1.44269502162933349609375f) * _580)) * _670)) + ((1.0f - _667) * (_569 ? ((exp2(log2(abs(_647)) * _15_m0[0u].w) * _15_m0[0u].y) + _15_m0[1u].x) : _15_m0[1u].x))) * 100.0f;
            break;
        }
        default:
        {
            _538 = 0.0f;
            _541 = 0.0f;
            _544 = 0.0f;
            break;
        }
        case 5u:
        {
            _538 = _272;
            _541 = _273;
            _544 = _274;
            break;
        }
    }
    float _901;
    float _902;
    float _903;
    if (_277.x == 0u)
    {
        float _737 = clamp(mad(_544, -0.0832588374614715576171875f, mad(_541, -0.621792018413543701171875f, _538 * 1.705050945281982421875f)) / _15_m0[3u].w, 0.0f, 1.0f);
        float _738 = clamp(mad(_544, -0.010548345744609832763671875f, mad(_541, 1.14080417156219482421875f, _538 * (-0.1302564144134521484375f))) / _15_m0[3u].w, 0.0f, 1.0f);
        float _739 = clamp(mad(_544, 1.15297257900238037109375f, mad(_541, -0.12896890938282012939453125f, _538 * (-0.0240033753216266632080078125f))) / _15_m0[3u].w, 0.0f, 1.0f);
        _901 = (_737 <= 0.003130800090730190277099609375f) ? (_737 * 12.9200000762939453125f) : ((exp2(log2(_737) * 0.4166666567325592041015625f) * 1.05499994754791259765625f) + (-0.054999999701976776123046875f));
        _902 = (_738 <= 0.003130800090730190277099609375f) ? (_738 * 12.9200000762939453125f) : ((exp2(log2(_738) * 0.4166666567325592041015625f) * 1.05499994754791259765625f) + (-0.054999999701976776123046875f));
        _903 = (_739 <= 0.003130800090730190277099609375f) ? (_739 * 12.9200000762939453125f) : ((exp2(log2(_739) * 0.4166666567325592041015625f) * 1.05499994754791259765625f) + (-0.054999999701976776123046875f));
    }
    else
    {
        float _793 = exp2(log2(abs(mad(_544, -0.00577151775360107421875f, mad(_541, -0.0200532414019107818603515625f, _538 * 1.02582454681396484375f)) * 9.9999997473787516355514526367188e-05f)) * 0.1593017578125f);
        float _808 = exp2(log2(abs(mad(_544, -0.00235216878354549407958984375f, mad(_541, 1.0045864582061767578125f, _538 * (-0.0022343560121953487396240234375f))) * 9.9999997473787516355514526367188e-05f)) * 0.1593017578125f);
        float _821 = exp2(log2(abs(mad(_544, 1.030303478240966796875f, mad(_541, -0.025290064513683319091796875f, _538 * (-0.0050133676268160343170166015625f))) * 9.9999997473787516355514526367188e-05f)) * 0.1593017578125f);
        _901 = exp2(log2(((_793 * 18.8515625f) + 0.8359375f) / ((_793 * 18.6875f) + 1.0f)) * 78.84375f);
        _902 = exp2(log2(((_808 * 18.8515625f) + 0.8359375f) / ((_808 * 18.6875f) + 1.0f)) * 78.84375f);
        _903 = exp2(log2(((_821 * 18.8515625f) + 0.8359375f) / ((_821 * 18.6875f) + 1.0f)) * 78.84375f);
    }
    _8[uint3(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y, gl_GlobalInvocationID.z)] = float4(_901, _902, _903, 1.0f);
}

[numthreads(16, 16, 1)]
void main(SPIRV_Cross_Input stage_input)
{
    gl_GlobalInvocationID = stage_input.gl_GlobalInvocationID;
    comp_main();
}
