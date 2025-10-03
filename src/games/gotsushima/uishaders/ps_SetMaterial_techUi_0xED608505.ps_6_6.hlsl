shader type? (vert, frag, geom, tesc, tese, comp): cbuffer _13_15 : register(b0, space0)
{
    float4 _15_m0[19] : packoffset(c0);
};

cbuffer _18_20 : register(b12, space0)
{
    float4 _20_m0[15] : packoffset(c0);
};

cbuffer _23_25 : register(b13, space0)
{
    float4 _25_m0[7] : packoffset(c0);
};

Texture2D<float4> _8 : register(t1, space0);
SamplerState _28 : register(s0, space0);

static float4 TEXCOORD;
static float2 TEXCOORD_1;
static float4 SV_Target;

struct SPIRV_Cross_Input
{
    float4 TEXCOORD : TEXCOORD0;
    float2 TEXCOORD_1 : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    float4 SV_Target : SV_Target0;
};

void frag_main()
{
    float4 _58 = _8.Sample(_28, float2(TEXCOORD_1.x, TEXCOORD_1.y));
    float _64 = ddx_coarse(TEXCOORD_1.x);
    float _65 = ddy_coarse(TEXCOORD_1.x);
    float _71 = ddx_coarse(TEXCOORD_1.y);
    float _72 = ddy_coarse(TEXCOORD_1.y);
    float _83 = _20_m0[11u].x * sqrt((_65 * _65) + (_64 * _64));
    float _84 = _20_m0[11u].y * sqrt((_72 * _72) + (_71 * _71));
    float _91 = max(0.0f, _25_m0[4u].w * 0.5f);
    float _95 = _20_m0[7u].w + _91;
    float _96 = _95 * _83;
    float _97 = _95 * _84;
    float _101 = _96 - TEXCOORD_1.x;
    float _102 = _97 - TEXCOORD_1.y;
    float _103 = TEXCOORD_1.x - (1.0f - _96);
    float _104 = TEXCOORD_1.y - (1.0f - _97);
    float _124 = _58.x * TEXCOORD.x;
    float _125 = _58.y * TEXCOORD.y;
    float _126 = _58.z * TEXCOORD.z;
    float _128 = dot(float3(_124, _125, _126), float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    float _148 = clamp(1.0f - ((min(max(min(abs(_101), abs(_103)) / _83, max(_102, _104) / _84), max(min(abs(_102), abs(_104)) / _84, max(_101, _103) / _83)) - _91) / _20_m0[7u].w), 0.0f, 1.0f) * (_58.w * TEXCOORD.w);
    float _153 = _148 * _20_m0[8u].w;
    SV_Target.x = _153 * ((_15_m0[18u].x * (_128 - _124)) + _124);
    SV_Target.y = _153 * ((_15_m0[18u].x * (_128 - _125)) + _125);
    SV_Target.z = _153 * ((_15_m0[18u].x * (_128 - _126)) + _126);
    SV_Target.w = _148;
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
