cbuffer _24_26 : register(b0, space1)
{
    float4 _26_m0[13] : packoffset(c0);
};

cbuffer _29_31 : register(b1, space1)
{
    float4 _31_m0[3] : packoffset(c0);
};

Texture2D<float4> _8 : register(t11, space1);
Buffer<uint4> _12 : register(t13, space1);
Texture2D<float4> _13 : register(t18, space1);
Texture2D<float4> _14 : register(t20, space1);
RWTexture2D<float4> _17 : register(u2, space1);
RWBuffer<uint> _20 : register(u7, space1);
SamplerState _34 : register(s0, space1);
SamplerState _35 : register(s2, space1);

static uint3 gl_WorkGroupID;
static uint3 gl_LocalInvocationID;
struct SPIRV_Cross_Input
{
    uint3 gl_WorkGroupID : SV_GroupID;
    uint3 gl_LocalInvocationID : SV_GroupThreadID;
};

void comp_main()
{
    uint4 _58 = _20[gl_WorkGroupID.x].xxxx;
    uint _59 = _58.x;
    uint _64 = (_59 & 65535u) + gl_LocalInvocationID.x;
    uint _65 = (_59 >> 16u) + gl_LocalInvocationID.y;
    float _75 = _31_m0[1u].z * (float(_64) + 0.5f);
    float _77 = _31_m0[1u].w * (float(_65) + 0.5f);
    float4 _81 = _13.SampleLevel(_34, float2(_75, _77), 0.0f);
    float _84 = _81.x;
    uint4 _88 = asuint(_26_m0[10u]);
    float _124 = _26_m0[9u].w * max(0.0f, max(_14.SampleLevel(_35, float2(_75, _77), 0.0f).y, _81.y));
    float _134 = (_84 < asfloat(_12.Load(4u).x)) ? min(max(_124, 0.0f), 13.0f) : _124;
    float _139 = clamp((_84 - _8.Load(int3(uint2(uint(min(int(uint(int(floor(floor(float(int(_88.z)) * _75) * 0.0416666679084300994873046875f)))), int(_88.x))), uint(min(int(uint(int(floor(floor(float(int(_88.w)) * _77) * 0.0416666679084300994873046875f)))), int(_88.y)))), 0u)).y) * 80.0f, 0.0f, 1.0f);
    float _145 = (_139 * _139) * (3.0f - (_139 * 2.0f));
    float _150 = min(0.3183098733425140380859375f / (_134 * _134), 0.636632025241851806640625f);
    _17[uint2(_64, _65)] = float4(_134, _145 * _150, (1.0f - _145) * _150, 1.0f);
}

[numthreads(8, 8, 1)]
void main(SPIRV_Cross_Input stage_input)
{
    gl_WorkGroupID = stage_input.gl_WorkGroupID;
    gl_LocalInvocationID = stage_input.gl_LocalInvocationID;
    comp_main();
}
