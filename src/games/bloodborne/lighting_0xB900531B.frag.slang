#define USE_AUX_PUSHCONSTANTS

#include "./shared.h"
// -----------------------------------------------------------
// SSBOs
// -----------------------------------------------------------

// Original GLSL:
// layout(set = 0, binding = 0, std430) readonly buffer ssbo_1_* { ... } ...
// These are all different "views" of the same buffer.
// Represent them as a single uint[] buffer and reinterpret as needed.

[[vk::binding(0,0)]]
StructuredBuffer<uint> ssbo_1;

// Original GLSL:
// layout(set = 0, binding = 1, std430) readonly buffer ssbo_2 { uint data[]; } ssbo_2_1;
[[vk::binding(1,0)]]
StructuredBuffer<uint> ssbo_2_1;

// -----------------------------------------------------------
// Push Constants
// -----------------------------------------------------------

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

// -----------------------------------------------------------
// Textures & Sampler
// -----------------------------------------------------------

[[vk::binding(2,0)]] Texture2D fs_img24;
[[vk::binding(3,0)]] Texture2D fs_img4;
[[vk::binding(4,0)]] Texture2D fs_img16;
[[vk::binding(5,0)]] Texture2D fs_img32;
[[vk::binding(6,0)]] Texture2D fs_img40;
[[vk::binding(7,0)]] Texture2D fs_img48;
[[vk::binding(8,0)]] SamplerState fs_sampsgpr_12;

// -----------------------------------------------------------
// Fragment IO
// -----------------------------------------------------------

struct PsInput
{
    [[vk::location(0)]] float4 attr0 : TEXCOORD0;
    [[vk::location(1)]] float4 attr1 : TEXCOORD1;
};

struct PsOutput
{
    float4 frag_color0 : SV_Target0;
};

// Decode an 8-bit index that was encoded as index / 255.0 in alpha.
// Works for both RGBA8_UNORM and RGBA16_FLOAT sampling.
inline uint DecodeIndex8FromAlpha(float a)
{
  // Make sure we’re in range and stabilize around integer boundaries.
  float clamped = saturate(a);              // [0,1]
  float scaled = clamped * 255.0;           // [0,255]
  float rounded = floor(scaled + 0.5);      // nearest int
  return (uint)clamp(rounded, 0.0, 255.0);  // safety
}

// -----------------------------------------------------------
// Main
// -----------------------------------------------------------

[shader("fragment")]
PsOutput main(PsInput IN)
{
    PsOutput OUT;

    uint buf0_off       = (push_data.buf_offsets0.x & 0xFFu);
    uint buf0_dword_off = buf0_off >> 2u;

    uint buf1_off       = (push_data.buf_offsets0.x >> 8u) & 0xFFu;
    uint buf1_dword_off = buf1_off >> 2u;

    float2 uv1 = IN.attr1.xy;
    float2 uv0 = IN.attr0.xy;

    // Sample textures using interpolated UVs
    float4 tex4   = fs_img4.Sample(fs_sampsgpr_12, uv1);
    float4 tex32  = fs_img32.Sample(fs_sampsgpr_12, uv1);
    float4 tex24  = fs_img24.Sample(fs_sampsgpr_12, uv1);
    float4 tex40  = fs_img40.Sample(fs_sampsgpr_12, uv1);
    float4 tex48  = fs_img48.Sample(fs_sampsgpr_12, uv1);

    float _148 = tex4.x;
    uint idxAlpha = DecodeIndex8FromAlpha(tex24.w);

    uint _167 = idxAlpha * 6u;

    // -----------------------------
    // Float data from ssbo_1 (asfloat)
    // -----------------------------
    uint idx202 = ((_167 + 2u) * 4u) + buf0_dword_off;
    float4 _202 = float4(
        asfloat(ssbo_1[idx202]),
        asfloat(ssbo_1[idx202 + 1u]),
        asfloat(ssbo_1[idx202 + 2u]),
        0.0);
    float4 _205 = _202;
    float  _206 = _205.x;

    uint idx228 = ((_167 + 3u) * 4u) + buf0_dword_off;
    float4 _228 = float4(
        asfloat(ssbo_1[idx228]),
        asfloat(ssbo_1[idx228 + 1u]),
        asfloat(ssbo_1[idx228 + 2u]),
        asfloat(ssbo_1[idx228 + 3u]));
    float4 _229 = _228;

    uint idx253 = ((_167 + 1u) * 4u) + buf0_dword_off;
    float4 _252 = float4(
        asfloat(ssbo_1[idx253]),
        asfloat(ssbo_1[idx253 + 1u]),
        asfloat(ssbo_1[idx253 + 2u]),
        asfloat(ssbo_1[idx253 + 3u]));
    float4 _253 = _252;

    uint idx277 = ((_167 + 4u) * 4u) + buf0_dword_off;
    float4 _276 = float4(
        asfloat(ssbo_1[idx277]),
        asfloat(ssbo_1[idx277 + 1u]),
        asfloat(ssbo_1[idx277 + 2u]),
        asfloat(ssbo_1[idx277 + 3u]));
    float4 _277 = _276;

    // -----------------------------
    // Remaining math using ssbo_2_1 (uint-backed)
    // -----------------------------
    float _350 = uv0.y;
    float _389 = uv0.x;

    float _409 =
        1.0 / fma(asfloat(ssbo_2_1[12u + buf1_dword_off]), _389,
        fma(asfloat(ssbo_2_1[13u + buf1_dword_off]), _350,
        fma(asfloat(ssbo_2_1[14u + buf1_dword_off]), _148,
             asfloat(ssbo_2_1[15u + buf1_dword_off]))));

    float _414 =
        fma(asfloat(ssbo_2_1[8u + buf1_dword_off]), _389,
        fma(asfloat(ssbo_2_1[9u + buf1_dword_off]), _350,
        fma(asfloat(ssbo_2_1[10u + buf1_dword_off]), _148,
             asfloat(ssbo_2_1[11u + buf1_dword_off])))) * _409;

    float _417 =
        fma(asfloat(ssbo_2_1[4u + buf1_dword_off]), _389,
        fma(asfloat(ssbo_2_1[5u + buf1_dword_off]), _350,
        fma(asfloat(ssbo_2_1[6u + buf1_dword_off]), _148,
             asfloat(ssbo_2_1[7u + buf1_dword_off])))) * _409;

    float _419 = asfloat(ssbo_2_1[42u + buf1_dword_off]) * _414;

    float _432 =
        fma(asfloat(ssbo_2_1[0u + buf1_dword_off]), _389,
        fma(asfloat(ssbo_2_1[1u + buf1_dword_off]), _350,
        fma(asfloat(ssbo_2_1[2u + buf1_dword_off]), _148,
             asfloat(ssbo_2_1[3u + buf1_dword_off])))) * _409;

    float _434 = asfloat(ssbo_2_1[34u + buf1_dword_off]) * _414;

    float _440 =
        fma(asfloat(ssbo_2_1[40u + buf1_dword_off]), _432,
        fma(asfloat(ssbo_2_1[41u + buf1_dword_off]), _417,
             _419));

    float _442 =
        fma(asfloat(ssbo_2_1[32u + buf1_dword_off]), _432,
        fma(asfloat(ssbo_2_1[33u + buf1_dword_off]), _417,
             _434));

    float _444 = asfloat(ssbo_2_1[38u + buf1_dword_off]) * _414;

    float _449 = tex32.y + tex40.y;
    float _450 = _440 * _440;
    float _451 = tex32.x + tex40.x;

    float _452 = tex48.y + _449;
    float _454 = tex32.z + tex40.z;
    float _455 = tex48.x + _451;

    float _458 = 0.586610019207000732421875 * _452;
    float _459 = tex48.z + _454;
    float _460 = sqrt(fma(_442, _442, _450)) - _205.y;

    float _464 = _460 * (-_205.z);
    float _469 = (-1.0) + fma(_459, 0.1144800037, fma(_455, 0.2989099919, _458));
    float _470 =
        fma(asfloat(ssbo_2_1[36u + buf1_dword_off]), _432,
        fma(asfloat(ssbo_2_1[37u + buf1_dword_off]), _417,
             _444))
        + _229.w;

    float _472 = _206 * (-exp2(_464));
    float _473 = fma(_253.w, _469, 1.0);
    float _474 = _470 - _229.y;

    float _475 = _206 + _472;
    float _476 = clamp(_475, 0.0, 1.0);

    float _483 = _229.z * _474;
    float _485 = fma(_277.w, _469, 1.0);

    float _486 = fma(_476, fma(_253.x, _473, -_455), _455);
    float _487 = fma(_476, fma(_253.y, _473, -_452), _452);
    float _488 = fma(_476, fma(_253.z, _473, -_459), _459);

    float _489 = _229.x * clamp(_483, 0.0, 1.0);
    float _490 = clamp(_489, 0.0, 1.0);

    float4 result_rgb = float4(
        fma(_490, fma(_277.x, _485, -_486), _486),
        fma(_490, fma(_277.y, _485, -_487), _487),
        fma(_490, fma(_277.z, _485, -_488), _488),
        0.0);

    float alpha = fs_img16.Sample(fs_sampsgpr_12, uv1).w;

    OUT.frag_color0 = float4(result_rgb.xyz, alpha);
    return OUT;
}
