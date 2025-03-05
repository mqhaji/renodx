cbuffer _13_15 : register(b9, space1)
{
    float4 _15_m0[9] : packoffset(c0);
};

Texture2D<float4> _8 : register(t9, space1);
SamplerState _18 : register(s0, space1);

static float4 COLOR;
static float2 TEXCOORD;
static float4 SV_Target;

struct SPIRV_Cross_Input
{
    float4 COLOR : TEXCOORD1;
    float2 TEXCOORD : TEXCOORD2;
};

struct SPIRV_Cross_Output
{
    float4 SV_Target : SV_Target0;
};

void frag_main()
{
    float4 _52 = _8.SampleLevel(_18, float2(TEXCOORD.x, TEXCOORD.y), _15_m0[3u].x);
    SV_Target.x = (_52.x * COLOR.x) * _15_m0[2u].x;
    SV_Target.y = (_52.y * COLOR.y) * _15_m0[2u].y;
    SV_Target.z = (_52.z * COLOR.z) * _15_m0[2u].z;
    SV_Target.w = (_52.w * COLOR.w) * _15_m0[2u].w;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    COLOR = stage_input.COLOR;
    TEXCOORD = stage_input.TEXCOORD;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.SV_Target = SV_Target;
    return stage_output;
}
