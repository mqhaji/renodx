cbuffer _13_15 : register(b12, space1)
{
    float4 _15_m0[95] : packoffset(c0);
};

Texture2D<float4> _8 : register(t23, space1);
SamplerState _18 : register(s0, space1);

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
    float _40 = TEXCOORD.x - _15_m0[72u].x;
    float _41 = TEXCOORD.y - _15_m0[72u].x;
    float4 _45 = _8.SampleLevel(_18, float2(_40, _41), 0.0f);
    float _49 = _45.z;
    bool _51 = _49 > (-1.0f);
    float _55 = _51 ? _49 : (-1.0f);
    float4 _56 = _8.SampleLevel(_18, float2(_40, TEXCOORD.y), 0.0f);
    float _60 = _56.z;
    bool _61 = _60 > _55;
    float _64 = _61 ? _60 : _55;
    float _65 = _15_m0[72u].x + TEXCOORD.y;
    float4 _66 = _8.SampleLevel(_18, float2(_40, _65), 0.0f);
    float _70 = _66.z;
    bool _71 = _70 > _64;
    float _74 = _71 ? _70 : _64;
    float4 _75 = _8.SampleLevel(_18, float2(TEXCOORD.x, _41), 0.0f);
    float _79 = _75.z;
    bool _80 = _79 > _74;
    float _83 = _80 ? _79 : _74;
    float4 _84 = _8.SampleLevel(_18, float2(TEXCOORD.x, TEXCOORD.y), 0.0f);
    float _88 = _84.z;
    bool _90 = _88 > _83;
    float _93 = _90 ? _88 : _83;
    float4 _94 = _8.SampleLevel(_18, float2(TEXCOORD.x, _65), 0.0f);
    float _98 = _94.z;
    bool _99 = _98 > _93;
    float _102 = _99 ? _98 : _93;
    float _103 = _15_m0[72u].x + TEXCOORD.x;
    float4 _104 = _8.SampleLevel(_18, float2(_103, _41), 0.0f);
    float _108 = _104.z;
    bool _109 = _108 > _102;
    float _112 = _109 ? _108 : _102;
    float4 _113 = _8.SampleLevel(_18, float2(_103, TEXCOORD.y), 0.0f);
    float _117 = _113.z;
    bool _118 = _117 > _112;
    float _121 = _118 ? _117 : _112;
    float4 _122 = _8.SampleLevel(_18, float2(_103, _65), 0.0f);
    float _126 = _122.z;
    bool _127 = _126 > _121;
    SV_Target.x = _127 ? _122.x : (_118 ? _113.x : (_109 ? _104.x : (_99 ? _94.x : (_90 ? _84.x : (_80 ? _75.x : (_71 ? _66.x : (_61 ? _56.x : (_51 ? _45.x : 0.0f))))))));
    SV_Target.y = _127 ? _122.y : (_118 ? _113.y : (_109 ? _104.y : (_99 ? _94.y : (_90 ? _84.y : (_80 ? _75.y : (_71 ? _66.y : (_61 ? _56.y : (_51 ? _45.y : 0.0f))))))));
    SV_Target.z = _127 ? _126 : _121;
    SV_Target.w = _84.w;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    TEXCOORD = stage_input.TEXCOORD;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.SV_Target = SV_Target;
    return stage_output;
}
