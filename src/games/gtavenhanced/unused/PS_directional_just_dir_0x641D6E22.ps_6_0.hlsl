cbuffer _15_17 : register(b4, space0)
{
    float4 _17_m0[26] : packoffset(c0);
};

cbuffer _20_22 : register(b10, space1)
{
    float4 _22_m0[24] : packoffset(c0);
};

Texture2D<float4> _8 : register(t12, space0);
Texture2D<float4> _9 : register(t13, space0);
Texture2D<float4> _10 : register(t14, space0);
SamplerState _25 : register(s0, space1);

static float4 TEXCOORD;
static float4 TEXCOORD_1;
static float4 SV_Target;

struct SPIRV_Cross_Input
{
    float4 TEXCOORD : TEXCOORD1;
    float4 TEXCOORD_1 : TEXCOORD2;
};

struct SPIRV_Cross_Output
{
    float4 SV_Target : SV_Target0;
};

void frag_main()
{
    float _55 = TEXCOORD.x / TEXCOORD.w;
    float _56 = TEXCOORD.y / TEXCOORD.w;
    float4 _60 = _8.Sample(_25, float2(_55, _56));
    float _63 = _60.x;
    float _64 = _60.y;
    float _65 = _60.z;
    float _69 = TEXCOORD_1.x / TEXCOORD_1.w;
    float _70 = TEXCOORD_1.y / TEXCOORD_1.w;
    float _71 = TEXCOORD_1.z / TEXCOORD_1.w;
    float4 _73 = _10.Sample(_25, float2(_55, _56));
    float _75 = _73.x;
    float _76 = _73.y;
    float _77 = _73.z;
    float _80 = (_76 * _76) * 512.0f;
    float4 _83 = _9.Sample(_25, float2(_55, _56));
    float _88 = _83.w;
    float _97 = frac(_88 * 7.984375f);
    float _98 = frac(_88 * 63.875f);
    float _109 = ((frac(_88 * 0.998046875f) + (-128.0f)) + (_83.x * 256.0f)) - (_97 * 0.125f);
    float _112 = ((_97 + (-128.0f)) + (_83.y * 256.0f)) - (_98 * 0.125f);
    float _114 = ((_83.z * 256.0f) + (-128.0f)) + _98;
    float _119 = rsqrt(dot(float3(_109, _112, _114), float3(_109, _112, _114)));
    float _120 = _109 * _119;
    float _121 = _112 * _119;
    float _122 = _114 * _119;
    float _124 = clamp(_75 * _75, 0.0f, 1.0f);
    float _127 = max(0.0f, _80 + (-500.0f));
    float _133 = ((_80 - _127) * 3.0f) + (_127 * 558.0f);
    float _145 = (-0.0f) - _22_m0[1u].x;
    float _147 = (-0.0f) - _22_m0[1u].y;
    float _148 = (-0.0f) - _22_m0[1u].z;
    float _152 = rsqrt(dot(float3(_69, _70, _71), float3(_69, _70, _71)));
    float _153 = _152 * _69;
    float _154 = _152 * _70;
    float _155 = _152 * _71;
    float _159 = _145 - _153;
    float _160 = _147 - _154;
    float _161 = _148 - _155;
    float _165 = rsqrt(dot(float3(_159, _160, _161), float3(_159, _160, _161)));
    float _166 = _159 * _165;
    float _167 = _160 * _165;
    float _168 = _161 * _165;
    float _172 = clamp(dot(float3(_120, _121, _122), float3(_145, _147, _148)), 0.0f, 1.0f);
    float _181 = 1.0f - clamp(dot(float3((-0.0f) - _153, (-0.0f) - _154, (-0.0f) - _155), float3(_120, _121, _122)), 0.0f, 1.0f);
    float _182 = 1.0f - clamp(dot(float3(_166, _167, _168), float3(_145, _147, _148)), 0.0f, 1.0f);
    float _183 = 1.0f - _77;
    precise float _184 = _181 * _181;
    precise float _185 = _184 * _184;
    precise float _188 = _182 * _182;
    precise float _189 = _188 * _188;
    float _212 = (_172 * _124) * ((((_133 + 2.0f) * 0.125f) * exp2(log2(clamp(dot(float3(_120, _121, _122), float3(_166, _167, _168)) + 9.9999999392252902907785028219223e-09f, 0.0f, 1.0f)) * (_133 + 9.9999999392252902907785028219223e-09f))) * (_183 + ((_182 * _77) * _189)));
    float _213 = (1.0f - ((_183 + ((_181 * _77) * _185)) * _124)) * _172;
    SV_Target.x = (_17_m0[14u].z * _22_m0[3u].x) * (_212 + ((_63 * _63) * _213));
    SV_Target.y = (_17_m0[14u].z * _22_m0[3u].y) * (_212 + ((_64 * _64) * _213));
    SV_Target.z = (_17_m0[14u].z * _22_m0[3u].z) * (_212 + ((_65 * _65) * _213));
    SV_Target.w = 1.0f;
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
