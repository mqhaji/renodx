shader type? (vert, frag, geom, tesc, tese, comp): cbuffer _14_16 : register(b0, space0)
{
    float4 _16_m0[19] : packoffset(c0);
};

cbuffer _19_21 : register(b12, space0)
{
    float4 _21_m0[15] : packoffset(c0);
};

Texture2D<float4> _8 : register(t1, space0);
Texture2D<float4> _9 : register(t11, space0);
SamplerState _24 : register(s0, space0);
SamplerState _25 : register(s2, space0);

static float4 TEXCOORD;
static float2 TEXCOORD_1;
static float4 TEXCOORD_2;
static float4 SV_Target;

struct SPIRV_Cross_Input
{
    float4 TEXCOORD : TEXCOORD0;
    float2 TEXCOORD_1 : TEXCOORD1;
    float4 TEXCOORD_2 : TEXCOORD2;
};

struct SPIRV_Cross_Output
{
    float4 SV_Target : SV_Target0;
};

void frag_main()
{
    float4 _62 = _8.Sample(_24, float2(TEXCOORD_1.x, TEXCOORD_1.y));
    float _68 = TEXCOORD_2.x / TEXCOORD_2.w;
    float _69 = TEXCOORD_2.y / TEXCOORD_2.w;
    float _99 = _62.x * TEXCOORD.x;
    float _100 = _62.y * TEXCOORD.y;
    float _101 = _62.z * TEXCOORD.z;
    float _103 = (_62.w * TEXCOORD.w) * min(1.0f, _9.Sample(_25, float2(dot(float3(_68, _69, 1.0f), float3(_21_m0[8u].xyz)), dot(float3(_68, _69, 1.0f), float3(_21_m0[9u].xyz)))).x);
    float _104 = dot(float3(_99, _100, _101), float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    float _124 = _21_m0[8u].w * _103;
    SV_Target.x = _124 * ((_16_m0[18u].x * (_104 - _99)) + _99);
    SV_Target.y = _124 * ((_16_m0[18u].x * (_104 - _100)) + _100);
    SV_Target.z = _124 * ((_16_m0[18u].x * (_104 - _101)) + _101);
    SV_Target.w = _103;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    TEXCOORD = stage_input.TEXCOORD;
    TEXCOORD_1 = stage_input.TEXCOORD_1;
    TEXCOORD_2 = stage_input.TEXCOORD_2;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.SV_Target = SV_Target;
    return stage_output;
}
