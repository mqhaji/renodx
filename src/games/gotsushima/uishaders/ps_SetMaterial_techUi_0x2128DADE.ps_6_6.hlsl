cbuffer _13_15 : register(b0, space0)
{
    float4 _15_m0[19] : packoffset(c0);
};

cbuffer _18_20 : register(b12, space0)
{
    float4 _20_m0[15] : packoffset(c0);
};

Texture2D<float4> _8 : register(t1, space0);
SamplerState _23 : register(s0, space0);

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
    float4 _53 = _8.Sample(_23, float2(TEXCOORD_1.x, TEXCOORD_1.y));
    float _59 = _53.x * TEXCOORD.x;
    float _60 = _53.y * TEXCOORD.y;
    float _61 = _53.z * TEXCOORD.z;
    float _62 = _53.w * TEXCOORD.w;
    float _63 = dot(float3(_59, _60, _61), float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    float _88 = _20_m0[8u].w * _62;
    SV_Target.x = _88 * ((_15_m0[18u].x * (_63 - _59)) + _59);
    SV_Target.y = _88 * ((_15_m0[18u].x * (_63 - _60)) + _60);
    SV_Target.z = _88 * ((_15_m0[18u].x * (_63 - _61)) + _61);
    SV_Target.w = _62;
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
