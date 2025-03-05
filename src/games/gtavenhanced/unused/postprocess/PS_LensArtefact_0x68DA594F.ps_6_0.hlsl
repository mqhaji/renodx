cbuffer _13_15 : register(b3, space0)
{
    float4 _15_m0[6] : packoffset(c0);
};

cbuffer _18_20 : register(b12, space1)
{
    float4 _20_m0[95] : packoffset(c0);
};

Texture2D<float4> _8 : register(t15, space1);
SamplerState _24[] : register(s0, space2);

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
    float _77 = (((_20_m0[35u].w - _20_m0[35u].z) * 2.0f) * ((_20_m0[35u].y > 0.0f) ? abs(TEXCOORD.y + (-0.5f)) : abs(TEXCOORD.x + (-0.5f)))) + _20_m0[35u].z;
    float4 _91 = _8.Sample(_24[asuint(_15_m0[0u]).w + 0u], float2((1.0f - _20_m0[34u].z) + ((TEXCOORD.x - _20_m0[34u].z) * _20_m0[34u].x), (1.0f - _20_m0[34u].w) + ((TEXCOORD.y - _20_m0[34u].w) * _20_m0[34u].y)));
    float _112 = _20_m0[35u].x * clamp(_77 * _77, 0.0f, 1.0f);
    SV_Target.x = (_20_m0[36u].x * min(_91.x, 65504.0f)) * _112;
    SV_Target.y = (_20_m0[36u].y * min(_91.y, 65504.0f)) * _112;
    SV_Target.z = (_20_m0[36u].z * min(_91.z, 65504.0f)) * _112;
    SV_Target.w = 0.0f;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    TEXCOORD = stage_input.TEXCOORD;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.SV_Target = SV_Target;
    return stage_output;
}
