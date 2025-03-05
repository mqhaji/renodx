cbuffer _20_22 : register(b0, space1)
{
    float4 _22_m0[13] : packoffset(c0);
};

cbuffer _25_27 : register(b1, space1)
{
    float4 _27_m0[3] : packoffset(c0);
};

Texture2D<float4> _8 : register(t7, space1);
Texture2D<float4> _9 : register(t12, space1);
RWTexture2D<float4> _12 : register(u2, space1);
RWBuffer<uint> _16 : register(u7, space1);
SamplerState _30 : register(s0, space1);
SamplerState _31 : register(s2, space1);

static uint3 gl_WorkGroupID;
static uint3 gl_LocalInvocationID;
struct SPIRV_Cross_Input
{
    uint3 gl_WorkGroupID : SV_GroupID;
    uint3 gl_LocalInvocationID : SV_GroupThreadID;
};

void comp_main()
{
    uint4 _52 = _16[gl_WorkGroupID.x].xxxx;
    uint _53 = _52.x;
    uint _58 = (_53 & 65535u) + gl_LocalInvocationID.x;
    uint _59 = (_53 >> 16u) + gl_LocalInvocationID.y;
    float _69 = _27_m0[1u].z * (float(_58) + 0.5f);
    float _71 = _27_m0[1u].w * (float(_59) + 0.5f);
    float4 _75 = _8.SampleLevel(_31, float2(_69, _71), 0.0f);
    float _78 = _75.x;
    float _79 = _75.y;
    float _80 = _75.z;
    float4 _82 = _9.SampleLevel(_30, float2(_69, _71), 0.0f);
    float _84 = _82.x;
    float _85 = _82.y;
    float _86 = _82.z;
    float _87 = _85 * _78;
    float _88 = _85 * _79;
    float _89 = _85 * _80;
    float _90 = _86 * _78;
    float _91 = _86 * _79;
    float _92 = _86 * _80;
    float _93 = _84 * 0.3333333432674407958984375f;
    float _96;
    float _98;
    float _100;
    float _102;
    float _104;
    float _106;
    float _108;
    float _110;
    float _113;
    float _114;
    float _115;
    float _116;
    float _117;
    float _118;
    float _119;
    float _120;
    uint _121;
    float _95 = _87;
    float _97 = _88;
    float _99 = _89;
    float _101 = _85;
    float _103 = _90;
    float _105 = _91;
    float _107 = _92;
    float _109 = _86;
    uint _111 = 0u;
    for (;;)
    {
        _113 = _95;
        _114 = _97;
        _115 = _99;
        _116 = _101;
        _117 = _103;
        _118 = _105;
        _119 = _107;
        _120 = _109;
        _121 = 0u;
        float _145;
        float _146;
        float _126;
        float _128;
        float _129;
        bool _133;
        for (;;)
        {
            _126 = (float(int(_121)) * 0.3333333432674407958984375f) + (-1.0f);
            _128 = float(int(_111)) * 0.3333333432674407958984375f;
            _129 = _128 + (-1.0f);
            _133 = (_126 * _126) > (_129 * _129);
            float frontier_phi_5_pred;
            float frontier_phi_5_pred_1;
            if (_133)
            {
                frontier_phi_5_pred = (_129 / _126) * 0.785398185253143310546875f;
                frontier_phi_5_pred_1 = _126;
            }
            else
            {
                frontier_phi_5_pred = (_129 != 0.0f) ? (1.57079637050628662109375f - ((_126 / (_128 + (-0.99999988079071044921875f))) * 0.785398185253143310546875f)) : 0.0f;
                frontier_phi_5_pred_1 = _129;
            }
            _145 = frontier_phi_5_pred;
            _146 = frontier_phi_5_pred_1;
            float _162 = _146 * _84;
            float _166 = (((_162 * cos(_145)) * _22_m0[11u].y) * _22_m0[12u].z) * _27_m0[0u].x;
            float _169 = ((_162 * sin(_145)) * _22_m0[11u].y) * _27_m0[0u].y;
            float _170 = _166 + _69;
            float _171 = _169 + _71;
            float4 _173 = _8.SampleLevel(_31, float2(_170, _171), 0.0f);
            float _175 = _173.x;
            float _176 = _173.y;
            float _177 = _173.z;
            float _184 = _27_m0[0u].z * _166;
            float _185 = _27_m0[0u].w * _169;
            float _196 = clamp(((_9.SampleLevel(_30, float2(_170, _171), 0.0f).x - (_22_m0[11u].x * sqrt((_184 * _184) + (_185 * _185)))) / _93) + 1.0f, 0.0f, 1.0f);
            _96 = ((_175 * _85) * _196) + _113;
            _98 = ((_176 * _85) * _196) + _114;
            _100 = ((_177 * _85) * _196) + _115;
            _102 = (_196 * _85) + _116;
            _104 = ((_175 * _86) * _196) + _117;
            _106 = ((_176 * _86) * _196) + _118;
            _108 = ((_177 * _86) * _196) + _119;
            _110 = (_196 * _86) + _120;
            uint _122 = _121 + 1u;
            if (_122 == 7u)
            {
                break;
            }
            else
            {
                _113 = _96;
                _114 = _98;
                _115 = _100;
                _116 = _102;
                _117 = _104;
                _118 = _106;
                _119 = _108;
                _120 = _110;
                _121 = _122;
                continue;
            }
        }
        uint _112 = _111 + 1u;
        if (_112 == 7u)
        {
            break;
        }
        else
        {
            _95 = _96;
            _97 = _98;
            _99 = _100;
            _101 = _102;
            _103 = _104;
            _105 = _106;
            _107 = _108;
            _109 = _110;
            _111 = _112;
            continue;
        }
    }
    float _214 = max(_110, 9.9999997473787516355514526367188e-06f);
    float _219 = max(_102, 9.9999997473787516355514526367188e-06f);
    float _220 = _96 / _219;
    float _221 = _98 / _219;
    float _222 = _100 / _219;
    float _231 = clamp((0.0408163256943225860595703125f / min(0.3183098733425140380859375f / (_84 * _84), 0.636632025241851806640625f)) * _110, 0.0f, 1.0f);
    _12[uint2(_58, _59)] = float4((_231 * ((_104 / _214) - _220)) + _220, (_231 * ((_106 / _214) - _221)) + _221, (_231 * ((_108 / _214) - _222)) + _222, 1.0f);
}

[numthreads(8, 8, 1)]
void main(SPIRV_Cross_Input stage_input)
{
    gl_WorkGroupID = stage_input.gl_WorkGroupID;
    gl_LocalInvocationID = stage_input.gl_LocalInvocationID;
    comp_main();
}
