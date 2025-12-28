#define USE_AUX_PUSHCONSTANTS

#include "./shared.h"
// -----------------------------------------------------------------------------
// Resource bindings
// -----------------------------------------------------------------------------

[[vk::binding(0, 0)]]
Texture2D<float4> fs_img4;

[[vk::binding(1, 0)]]
Texture1DArray<float4> fs_img16;

[[vk::binding(2, 0)]]
SamplerState fs_sampsgpr_12;

// -----------------------------------------------------------------------------
// Push constants
// -----------------------------------------------------------------------------

struct AuxData
{
    float xoffset;
    float yoffset;
    float xscale;
    float yscale;

    uint4 ud_regs0;
    uint4 ud_regs1;
    uint4 ud_regs2;
    uint4 ud_regs3;

    uint4 buf_offsets0;
    uint4 buf_offsets1;
    uint2 buf_offsets2;
};

[[vk::push_constant]]
ConstantBuffer<AuxData> push_data;

// -----------------------------------------------------------------------------
// Fragment input/output
// -----------------------------------------------------------------------------

struct PsInput
{
    float4 fragCoord : SV_Position;
};

struct PsOutput
{
    float4 frag_color0 : SV_Target0;
};

// -----------------------------------------------------------------------------
// Main shader
// -----------------------------------------------------------------------------

[shader("fragment")]
PsOutput main(PsInput IN)
{
    PsOutput OUT;

    // Equivalent to:
    // texelFetch(fs_img4, ivec2(floor(gl_FragCoord.xy)), 0)
    int2 pixel = int2(IN.fragCoord.xy);
    float4 _77 = fs_img4.Load(int3(pixel, 0));

    // Sample LUTs (1D array)
    float r = fs_img16.Sample(fs_sampsgpr_12, float2(_77.z, 2.0)).x;
    float g = fs_img16.Sample(fs_sampsgpr_12, float2(_77.y, 1.0)).x;
    float b = fs_img16.Sample(fs_sampsgpr_12, float2(_77.x, 0.0)).x;
    float a = _77.w;

    // LUT output (unused because next line overrides)
    float4 lutResult = float4(r, g, b, a);

    // Final line in original GLSL overrides the LUT completely:
    OUT.frag_color0 = _77;

    return OUT;
}
