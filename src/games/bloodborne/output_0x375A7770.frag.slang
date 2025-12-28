#define USE_AUX_PUSHCONSTANTS

#include "./shared.h"
// -----------------------------------------------------------------------------
// Resources
// -----------------------------------------------------------------------------

[[vk::binding(0, 0)]]
StructuredBuffer<uint> ssbo_1_1;  // ssbo_1.data[]

[[vk::binding(1, 0)]]
Texture2D fs_img24;

[[vk::binding(2, 0)]]
Texture2D fs_img4;

[[vk::binding(3, 0)]]
Texture2D fs_img16;

[[vk::binding(4, 0)]]
Texture2D fs_img32;

[[vk::binding(5, 0)]]
SamplerState fs_sampsgpr_44;

[[vk::binding(6, 0)]]
SamplerState fs_sampsgpr_12;

[[vk::binding(7, 0)]]
SamplerState fs_sampsgpr_40;

[[vk::binding(8, 0)]]
SamplerState fs_sampsgpr_48;

// -----------------------------------------------------------------------------
// Push constants
// -----------------------------------------------------------------------------

struct AuxData {
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

struct PsInput {
  // Matches the original locations:
  [[vk::location(0)]]
  float4 attr0 : TEXCOORD0;  // fs_in_attr0_p
  [[vk::location(1)]]
  float4 attr1 : TEXCOORD1;  // fs_in_attr1_p
  [[vk::location(3)]]
  float4 attr2 : TEXCOORD2;  // fs_in_attr2_p
  [[vk::location(4)]]
  float4 attr3 : TEXCOORD3;  // fs_in_attr3_p
};

struct PsOutput {
  float4 frag_color0 : SV_Target0;
};

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

[shader("fragment")]
PsOutput main(PsInput IN) {
  PsOutput OUT;

  // buf0_dword_off = (buf_offsets0.x[0:8] >> 2)
  uint buf0_dword_off = (push_data.buf_offsets0.x & 0xFFu) >> 2u;

  // Instead of barycentrics+pervertexEXT, just use interpolated UVs:
  float2 uv2 = IN.attr2.xy;
  float2 uv0 = IN.attr0.xy;
  float2 uv1 = IN.attr1.xy;
  float2 uv3 = IN.attr3.xy;

  // vec4 _121 = texture(fs_img24, uv2);
  float4 _121 = fs_img24.Sample(fs_sampsgpr_44, uv2);
  float _122 = _121.x;
  float _123 = _121.y;
  float _124 = _121.z;

  // vec4 _153 = textureLod(fs_img4, uv0, 0.0);
  float4 _153 = fs_img4.SampleLevel(fs_sampsgpr_12, uv0, 0.0);
  float3 untonemapped = _153.rgb;

  float _154 = _153.x;
  float _155 = _153.y;
  float _156 = _153.z;

  // vec4 _186 = textureLod(fs_img16, uv1, 0.0);
  float4 _186 = fs_img16.SampleLevel(fs_sampsgpr_40, uv1, 0.0);

  // vec4 _218 = textureLod(fs_img32, uv3, 0.0);
  float4 _218 = fs_img32.SampleLevel(fs_sampsgpr_48, uv3, 0.0);
  float _219 = _218.x;

  uint _221 = 752u + buf0_dword_off;
  uint _225 = 753u + buf0_dword_off;
  uint _229 = 754u + buf0_dword_off;
  uint _233 = 755u + buf0_dword_off;
  uint _237 = 756u + buf0_dword_off;

  float _242 = fma(_124, asfloat(ssbo_1_1[_221]), asfloat(ssbo_1_1[_225]));

  uint _244 = 616u + buf0_dword_off;
  uint _248 = 617u + buf0_dword_off;

  float _253 = fma(_123, asfloat(ssbo_1_1[_221]), asfloat(ssbo_1_1[_225]));
  float _255 = 0.333333343f * _242;

  uint _257 = 744u + buf0_dword_off;
  uint _261 = 745u + buf0_dword_off;
  uint _265 = 746u + buf0_dword_off;
  uint _269 = 747u + buf0_dword_off;

  float _274 = fma(asfloat(ssbo_1_1[_221]), _122, asfloat(ssbo_1_1[_225]));
  float _276 = fma(_274, 0.333333343f, fma(_253, 0.333333343f, _255));

  uint _278 = 832u + buf0_dword_off;
  uint _282 = 833u + buf0_dword_off;

  float _285 = _274 - _276;
  float _286 = _253 - _276;
  float _287 = _242 - _276;

  float _296 = fma(fma(asfloat(ssbo_1_1[_237]), _285, _276),
                   asfloat(ssbo_1_1[_229]),
                   asfloat(ssbo_1_1[_233]));
  float _299 = fma(fma(asfloat(ssbo_1_1[_237]), _286, _276),
                   asfloat(ssbo_1_1[_229]),
                   asfloat(ssbo_1_1[_233]));
  float _302 = fma(asfloat(ssbo_1_1[_229]),
                   fma(asfloat(ssbo_1_1[_237]), _287, _276),
                   asfloat(ssbo_1_1[_233]));

  float _304 = asfloat(ssbo_1_1[_244]) * _154;
  float _306 = asfloat(ssbo_1_1[_244]) * _155;
  float _308 = asfloat(ssbo_1_1[_244]) * _156;

  float _309 = _296 * _304;
  float _310 = _299 * _306;
  float _311 = _302 * _308;

  uint _313 = 748u + buf0_dword_off;

  float _318 = fma(_124, asfloat(ssbo_1_1[_257]), asfloat(ssbo_1_1[_261]));
  float _325 = fma(_123, asfloat(ssbo_1_1[_257]), asfloat(ssbo_1_1[_261]));
  float _326 = 0.333333343f * _318;

  float _329 = (-max(6.103515625e-05f, _309)) * asfloat(ssbo_1_1[_278]);
  float _332 = (-max(6.103515625e-05f, _310)) * asfloat(ssbo_1_1[_278]);
  float _335 = (-max(6.103515625e-05f, _311)) * asfloat(ssbo_1_1[_278]);

  float _338 = fma(asfloat(ssbo_1_1[_257]), _122, asfloat(ssbo_1_1[_261]));

  float _341 = 1.44269502f * _329;
  float _342 = 1.44269502f * _332;
  float _343 = 1.44269502f * _335;

  float _344 = fma(_338, 0.333333343f, fma(_325, 0.333333343f, _326));

  float _348 = _338 - _344;
  float _349 = _325 - _344;
  float _350 = _318 - _344;

  float _352 = 1.0f - exp2(_341);
  float _353 = 1.0f - exp2(_342);
  float _354 = 1.0f - exp2(_343);

  float _365 = (-1.0f) + _352;
  float _366 = (-1.0f) + _353;
  float _367 = (-1.0f) + _354;

  float _378 = fma(asfloat(ssbo_1_1[_282]), _365, 1.0f);
  float _380 = fma(asfloat(ssbo_1_1[_282]), _366, 1.0f);
  float _382 = fma(asfloat(ssbo_1_1[_282]), _367, 1.0f);

  float _386 = _352 * _378;
  float _387 = _353 * _380;
  float _388 = _354 * _382;

  uint _406 = 808u + buf0_dword_off;
  uint _410 = 809u + buf0_dword_off;
  uint _414 = 810u + buf0_dword_off;

  float _418 = asfloat(ssbo_1_1[_248]) * fma(_186.x, _296, fma(clamp(fma(asfloat(ssbo_1_1[_313]), _348, _344), 0.0f, 1.0f), asfloat(ssbo_1_1[_265]), asfloat(ssbo_1_1[_269])));
  float _419 = clamp(_418, 0.0f, 1.0f);

  float _421 = asfloat(ssbo_1_1[_248]) * fma(_186.y, _299, fma(clamp(fma(asfloat(ssbo_1_1[_313]), _349, _344), 0.0f, 1.0f), asfloat(ssbo_1_1[_265]), asfloat(ssbo_1_1[_269])));
  float _422 = clamp(_421, 0.0f, 1.0f);

  float _424 = asfloat(ssbo_1_1[_248]) * fma(_186.z, _302, fma(asfloat(ssbo_1_1[_265]), clamp(fma(asfloat(ssbo_1_1[_313]), _350, _344), 0.0f, 1.0f), asfloat(ssbo_1_1[_269])));
  float _425 = clamp(_424, 0.0f, 1.0f);

  float _426 = _378 * _386;
  float _427 = clamp(_426, 0.0f, 1.0f);

  float _428 = _380 * _387;
  float _429 = clamp(_428, 0.0f, 1.0f);

  float _430 = _382 * _388;
  float _431 = clamp(_430, 0.0f, 1.0f);

  float _432 = _427 + _419;
  float _433 = _429 + _422;
  float _434 = _431 + _425;

  float _442 = fma(-_427, _419, _432) + 2.384185791015625e-07f;
  float _444 = fma(-_429, _422, _433) + 2.384185791015625e-07f;
  float _446 = fma(-_431, _425, _434) + 2.384185791015625e-07f;

  float _452 = asfloat(ssbo_1_1[623u + buf0_dword_off]) * _153.w;

  float _454 = asfloat(ssbo_1_1[_406]) * log2(clamp(_442, 0.0f, 1.0f));
  float _456 = asfloat(ssbo_1_1[_406]) * log2(clamp(_444, 0.0f, 1.0f));
  float _458 = asfloat(ssbo_1_1[_406]) * log2(clamp(_446, 0.0f, 1.0f));

  float _476 = asfloat(ssbo_1_1[_414]) + fma(asfloat(ssbo_1_1[_410]), _219, exp2(_454));
  float _478 = asfloat(ssbo_1_1[_414]) + fma(asfloat(ssbo_1_1[_410]), _219, exp2(_456));
  float _480 = asfloat(ssbo_1_1[_414]) + fma(asfloat(ssbo_1_1[_410]), _219, exp2(_458));

  OUT.frag_color0.x = _480;
  OUT.frag_color0.y = _478;
  OUT.frag_color0.z = _476;

  OUT.frag_color0.w = clamp(
      fma(_154, asfloat(ssbo_1_1[620u + buf0_dword_off]),
          fma(asfloat(ssbo_1_1[621u + buf0_dword_off]), _155,
              fma(asfloat(ssbo_1_1[622u + buf0_dword_off]), _156, _452))),
      0.0f, 1.0f);

  // Swizzle back
  OUT.frag_color0.rgb = OUT.frag_color0.bgr;
  float3 tonemapped_sdr = renodx::color::srgb::DecodeSafe(OUT.frag_color0.rgb);
  float3 output = tonemapped_sdr;
  if (RENODX_TONE_MAP_TYPE) {
    output.rgb = renodx::draw::ToneMapPass(untonemapped, output);
  }
  OUT.frag_color0.rgb = renodx::draw::RenderIntermediatePass(output);
  return OUT;
}
