cbuffer _13_15 : register(b3, space0)
{
    float4 _15_m0[6] : packoffset(c0);
};

Texture2D<float4> _8 : register(t15, space1);
SamplerState _19[] : register(s0, space2);

static float2 TEXCOORD;
static float4 SV_Target;

struct SPIRV_Cross_Input
{
    float2 TEXCOORD : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    float4 SV_Target : SV_Target0;
};

void frag_main()
{
    float4 _48 = _8.Sample(_19[asuint(_15_m0[0u]).w + 0u], float2(TEXCOORD.x, TEXCOORD.y));
    SV_Target.x = _48.x;
    SV_Target.y = _48.y;
    SV_Target.z = _48.z;
    SV_Target.w = _48.w;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    TEXCOORD = stage_input.TEXCOORD;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.SV_Target = SV_Target;
    return stage_output;
}
