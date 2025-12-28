// ---------------------------------------------------------------------------
// SSBOs
// ---------------------------------------------------------------------------
[[vk::binding(0, 0)]]
StructuredBuffer<uint> ssbo_1_1;   // ssbo_1.data[]

[[vk::binding(1, 0)]]
StructuredBuffer<uint> ssbo_2_1;   // ssbo_2.data[]

[[vk::binding(2, 0)]]
StructuredBuffer<uint> ssbo_3_1;   // ssbo_3.data[]

// ---------------------------------------------------------------------------
// Textures & samplers
// ---------------------------------------------------------------------------
[[vk::binding(3, 0)]]
Texture2D<float4> fs_img4;

[[vk::binding(4, 0)]]
SamplerState fs_sampsgpr_12;

// ---------------------------------------------------------------------------
// Push constants
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Fragment IO
// ---------------------------------------------------------------------------
struct PsInput
{
    // Interpolated UV instead of barycentrics+pervertexEXT
    [[vk::location(0)]]
    float4 attr0 : TEXCOORD0;
};

struct PsOutput
{
    float4 frag_color0 : SV_Target0;
};

float f1(uint idx) { return asfloat(ssbo_1_1[idx]); }
float f2(uint idx) { return asfloat(ssbo_2_1[idx]); }
float f3(uint idx) { return asfloat(ssbo_3_1[idx]); }

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
[shader("fragment")]
PsOutput main(PsInput IN)
{
    PsOutput OUT;

    // Derive offsets (bitfieldExtract equivalents)
    uint b0_0_7   =  push_data.buf_offsets0.x        & 0xFFu;
    uint b0_8_15  = (push_data.buf_offsets0.x >> 8)  & 0xFFu;
    uint b0_16_23 = (push_data.buf_offsets0.x >> 16) & 0xFFu;

    uint buf0_dword_off = b0_0_7   >> 2;
    uint buf1_dword_off = b0_8_15  >> 2;
    uint buf2_dword_off = b0_16_23 >> 2; // used with ssbo_3

    uint _103 = 6u  + buf0_dword_off;
    uint _106 = 4u  + buf1_dword_off;
    uint _109 = 5u  + buf1_dword_off;
    uint _118 = 2u  + buf0_dword_off;
    uint _129 = 10u + buf0_dword_off;
    uint _141 = 14u + buf0_dword_off;
    uint _152 = 18u + buf0_dword_off;
    uint _177 = 22u + buf0_dword_off;
    uint _201 = 26u + buf0_dword_off;
    uint _219 = 30u + buf0_dword_off;
    uint _246 = 34u + buf0_dword_off;
    uint _273 = 38u + buf0_dword_off;
    uint _300 = 42u + buf0_dword_off;
    uint _327 = 46u + buf0_dword_off;
    uint _354 = 50u + buf0_dword_off;

    // Use interpolated UV instead of barycentrics
    float2 uv = IN.attr0.xy;
    float _185 = uv.x;
    float _191 = uv.y;

    // Sample positions and taps (same as original, just Slang-ified)
    float2 sampleUV;

    sampleUV = float2(
        fma(f2(_106), f1(4u  + buf0_dword_off), _185),
        fma(f2(_109), f1(5u  + buf0_dword_off), _191));
    float4 _232 = fs_img4.SampleLevel(fs_sampsgpr_12, sampleUV, 0.0);

    sampleUV = float2(
        fma(f2(_106), f1(0u  + buf0_dword_off), _185),
        fma(f2(_109), f1(1u  + buf0_dword_off), _191));
    float4 _259 = fs_img4.SampleLevel(fs_sampsgpr_12, sampleUV, 0.0);

    sampleUV = float2(
        fma(f2(_106), f1(8u  + buf0_dword_off), _185),
        fma(f2(_109), f1(9u  + buf0_dword_off), _191));
    float4 _286 = fs_img4.SampleLevel(fs_sampsgpr_12, sampleUV, 0.0);

    sampleUV = float2(
        fma(f2(_106), f1(12u + buf0_dword_off), _185),
        fma(f2(_109), f1(13u + buf0_dword_off), _191));
    float4 _313 = fs_img4.SampleLevel(fs_sampsgpr_12, sampleUV, 0.0);

    sampleUV = float2(
        fma(f2(_106), f1(16u + buf0_dword_off), _185),
        fma(f2(_109), f1(17u + buf0_dword_off), _191));
    float4 _340 = fs_img4.SampleLevel(fs_sampsgpr_12, sampleUV, 0.0);

    sampleUV = float2(
        fma(f2(_106), f1(20u + buf0_dword_off), _185),
        fma(f2(_109), f1(21u + buf0_dword_off), _191));
    float4 _367 = fs_img4.SampleLevel(fs_sampsgpr_12, sampleUV, 0.0);

    sampleUV = float2(
        fma(f2(_106), f1(24u + buf0_dword_off), _185),
        fma(f2(_109), f1(25u + buf0_dword_off), _191));
    float4 _382 = fs_img4.SampleLevel(fs_sampsgpr_12, sampleUV, 0.0);

    sampleUV = float2(
        fma(f2(_106), f1(28u + buf0_dword_off), _185),
        fma(f2(_109), f1(29u + buf0_dword_off), _191));
    float4 _397 = fs_img4.SampleLevel(fs_sampsgpr_12, sampleUV, 0.0);

    sampleUV = float2(
        fma(f2(_106), f1(32u + buf0_dword_off), _185),
        fma(f2(_109), f1(33u + buf0_dword_off), _191));
    float4 _412 = fs_img4.SampleLevel(fs_sampsgpr_12, sampleUV, 0.0);

    sampleUV = float2(
        fma(f2(_106), f1(36u + buf0_dword_off), _185),
        fma(f2(_109), f1(37u + buf0_dword_off), _191));
    float4 _427 = fs_img4.SampleLevel(fs_sampsgpr_12, sampleUV, 0.0);

    sampleUV = float2(
        fma(f2(_106), f1(40u + buf0_dword_off), _185),
        fma(f2(_109), f1(41u + buf0_dword_off), _191));
    float4 _442 = fs_img4.SampleLevel(fs_sampsgpr_12, sampleUV, 0.0);

    sampleUV = float2(
        fma(f2(_106), f1(44u + buf0_dword_off), _185),
        fma(f2(_109), f1(45u + buf0_dword_off), _191));
    float4 _457 = fs_img4.SampleLevel(fs_sampsgpr_12, sampleUV, 0.0);

    sampleUV = float2(
        fma(f2(_106), f1(48u + buf0_dword_off), _185),
        fma(f2(_109), f1(49u + buf0_dword_off), _191));
    float4 _466 = fs_img4.SampleLevel(fs_sampsgpr_12, sampleUV, 0.0);

    // Initial weighted contribution from _232
    float _472 = f1(_103) * _232.w;
    float _474 = f1(_103) * _232.x;
    float _476 = f1(_103) * _232.y;
    float _478 = f1(_103) * _232.z;

    // Big weighted sums over neighborhood taps (preserved exactly)
    float _563 =
        fma(f1(_327), _457.w,
        fma(f1(_300), _442.w,
        fma(f1(_273), _427.w,
        fma(f1(_246), _412.w,
        fma(f1(_219), _397.w,
        fma(f1(_201), _382.w,
        fma(f1(_177), _367.w,
        fma(f1(_152), _340.w,
        fma(f1(_141), _313.w,
        fma(f1(_129), _286.w,
        fma(f1(_118), _259.w, _472)))))))))));

    float _565 =
        fma(f1(_327), _457.x,
        fma(f1(_300), _442.x,
        fma(f1(_273), _427.x,
        fma(f1(_246), _412.x,
        fma(f1(_219), _397.x,
        fma(f1(_201), _382.x,
        fma(f1(_177), _367.x,
        fma(f1(_152), _340.x,
        fma(f1(_141), _313.x,
        fma(f1(_129), _286.x,
        fma(f1(_118), _259.x, _474)))))))))));

    float _567 =
        fma(f1(_327), _457.y,
        fma(f1(_300), _442.y,
        fma(f1(_273), _427.y,
        fma(f1(_246), _412.y,
        fma(f1(_219), _397.y,
        fma(f1(_201), _382.y,
        fma(f1(_177), _367.y,
        fma(f1(_152), _340.y,
        fma(f1(_141), _313.y,
        fma(f1(_129), _286.y,
        fma(f1(_118), _259.y, _476)))))))))));

    float _569 =
        fma(f1(_327), _457.z,
        fma(f1(_300), _442.z,
        fma(f1(_273), _427.z,
        fma(f1(_246), _412.z,
        fma(f1(_219), _397.z,
        fma(f1(_201), _382.z,
        fma(f1(_177), _367.z,
        fma(f1(_152), _340.z,
        fma(f1(_141), _313.z,
        fma(f1(_129), _286.z,
        fma(f1(_118), _259.z, _478)))))))))));

    // Alpha uses ssbo_3 and another weight from ssbo_1
    float scaleA = f3(1u + buf2_dword_off);
    float _579 = scaleA * fma(f1(_354), _466.w, _563);

    OUT.frag_color0.x = fma(f1(_354), _466.z, _569);
    OUT.frag_color0.y = fma(f1(_354), _466.y, _567);
    OUT.frag_color0.z = fma(f1(_354), _466.x, _565);
    OUT.frag_color0.w = saturate(_579);

    return OUT;
}
