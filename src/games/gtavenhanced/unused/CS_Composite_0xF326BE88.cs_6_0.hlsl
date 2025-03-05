cbuffer _22_24 : register(b0, space1)
{
    float4 _24_m0[13] : packoffset(c0);
};

cbuffer _27_29 : register(b1, space1)
{
    float4 _29_m0[3] : packoffset(c0);
};

Texture2D<float4> _8 : register(t7, space1);
Texture2D<float4> _9 : register(t9, space1);
Texture2D<float4> _10 : register(t19, space1);
Texture2D<float4> _11 : register(t20, space1);
RWTexture2D<float4> _14 : register(u2, space1);
RWBuffer<uint> _18 : register(u7, space1);
SamplerState _32 : register(s0, space1);
SamplerState _33 : register(s2, space1);

static uint3 gl_WorkGroupID;
static uint3 gl_LocalInvocationID;
struct SPIRV_Cross_Input
{
    uint3 gl_WorkGroupID : SV_GroupID;
    uint3 gl_LocalInvocationID : SV_GroupThreadID;
};

void comp_main()
{
    uint4 _56 = _18[gl_WorkGroupID.x].xxxx;
    uint _57 = _56.x;
    uint _64 = ((_57 << 1u) & 131070u) + gl_LocalInvocationID.x;
    uint _65 = ((_57 >> 16u) << 1u) + gl_LocalInvocationID.y;
    float _75 = _29_m0[1u].z * (float(_64) + 0.5f);
    float _77 = _29_m0[1u].w * (float(_65) + 0.5f);
    float4 _81 = _9.SampleLevel(_33, float2(_75, _77), 0.0f);
    float _84 = _81.x;
    float _85 = _81.y;
    float _86 = _81.z;
    float4 _88 = _8.SampleLevel(_33, float2(_75, _77), 0.0f);
    float _122 = clamp(clamp(((_24_m0[9u].w * max(_11.SampleLevel(_33, float2(_75, _77), 0.0f).y, abs(_10.SampleLevel(_32, float2(_75, _77), 0.0f).y))) + (-0.5f)) / min(max(_24_m0[12u].w * 2.0f, 0.5f), 2.0f), 0.0f, 1.0f), 0.0f, 1.0f);
    float _127 = (_122 * _122) * (3.0f - (_122 * 2.0f));
    _14[uint2(_64, _65)] = float4((_127 * (_88.x - _84)) + _84, (_127 * (_88.y - _85)) + _85, (_127 * (_88.z - _86)) + _86, 1.0f);
}

[numthreads(16, 16, 1)]
void main(SPIRV_Cross_Input stage_input)
{
    gl_WorkGroupID = stage_input.gl_WorkGroupID;
    gl_LocalInvocationID = stage_input.gl_LocalInvocationID;
    comp_main();
}
