cbuffer _13_15 : register(b3, space0)
{
    float4 _15_m0[6] : packoffset(c0);
};

cbuffer _18_20 : register(b5, space0)
{
    float4 _20_m0[26] : packoffset(c0);
};

cbuffer _23_25 : register(b9, space1)
{
    float4 _25_m0[9] : packoffset(c0);
};

Texture2D<float4> _8 : register(t5, space1);
SamplerState _29[] : register(s0, space2);

static float2 TEXCOORD;
static float3 TEXCOORD_1;
static float4 SV_Target;

struct SPIRV_Cross_Input
{
    float2 TEXCOORD : TEXCOORD1;
    float3 TEXCOORD_1 : TEXCOORD2;
};

struct SPIRV_Cross_Output
{
    float4 SV_Target : SV_Target0;
};

void frag_main()
{
    float4 _68 = _8.Sample(_29[asuint(_15_m0[0u]).x + 0u], float2(TEXCOORD.x, TEXCOORD.y));
    float _70 = _68.x;
    float _74 = (_70 < 0.0039215688593685626983642578125f) ? 0.0f : 1.0f;
    float _85 = _70 * _70;
    float _86 = _85 * _85;
    SV_Target.x = ((_86 * TEXCOORD_1.x) + (_74 * _25_m0[3u].x)) * _20_m0[14u].z;
    SV_Target.y = ((_86 * TEXCOORD_1.y) + (_74 * _25_m0[3u].y)) * _20_m0[14u].z;
    SV_Target.z = ((_86 * TEXCOORD_1.z) + (_74 * _25_m0[3u].z)) * _20_m0[14u].z;
    SV_Target.w = _74;
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
