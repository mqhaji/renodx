Texture2D<float4> _8 : register(t0, space0);
SamplerComparisonState _11 : register(s8, space0);

static float4 TEXCOORD_1;
static float2 TEXCOORD_2;
static float TEXCOORD_3;
static float4 SV_Target;

struct SPIRV_Cross_Input
{
    float4 TEXCOORD_1 : TEXCOORD0;
    float2 TEXCOORD_2 : TEXCOORD1;
    float TEXCOORD_3 : TEXCOORD2;
};

struct SPIRV_Cross_Output
{
    float4 SV_Target : SV_Target0;
};

void frag_main()
{
    float _57 = (TEXCOORD_2.x * 2.0f) + (-1.0f);
    float _59 = (TEXCOORD_2.y * 2.0f) + (-1.0f);
    float _67 = clamp(1.0f - sqrt((_59 * _59) + (_57 * _57)), 0.0f, 1.0f);
    float _75 = 1.0f - (((_67 * _67) * (_8.SampleCmpLevelZero(_11, float2(TEXCOORD_1.x / TEXCOORD_1.w, TEXCOORD_1.y / TEXCOORD_1.w), TEXCOORD_1.z / TEXCOORD_1.w).xxxx.x * TEXCOORD_3)) * (3.0f - (_67 * 2.0f)));
    SV_Target.x = _75;
    SV_Target.y = _75;
    SV_Target.z = _75;
    SV_Target.w = _75;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    TEXCOORD_1 = stage_input.TEXCOORD_1;
    TEXCOORD_2 = stage_input.TEXCOORD_2;
    TEXCOORD_3 = stage_input.TEXCOORD_3;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.SV_Target = SV_Target;
    return stage_output;
}
