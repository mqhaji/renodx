#define USE_AUX_PUSHCONSTANTS

#include "./shared.h"
// -----------------------------------------------------------
// Resources
// -----------------------------------------------------------
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

// -----------------------------------------------------------
// Buffers
// -----------------------------------------------------------
struct SSBO1 {
  uint data[];
};
[[vk::binding(0, 0)]]
StructuredBuffer<SSBO1> ssbo_1_1;

// -----------------------------------------------------------
// Push constants
// -----------------------------------------------------------
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

// -----------------------------------------------------------
// Fragment Inputs (replacing pervertexEXT arrays)
// -----------------------------------------------------------
struct PsInput {
  [[vk::location(0)]]
  float4 attr0 : TEXCOORD0;
  [[vk::location(1)]]
  float4 attr1 : TEXCOORD1;
  [[vk::location(3)]]
  float4 attr2 : TEXCOORD2;
  [[vk::location(4)]]
  float4 attr3 : TEXCOORD3;
};

struct PsOutput {
  float4 frag_color0 : SV_Target0;
};

// -----------------------------------------------------------
// Main Shader
// -----------------------------------------------------------
[shader("fragment")]
PsOutput main(PsInput IN) {
  PsOutput OUT;

  uint buf0_dword_off =
      (push_data.buf_offsets0.x & 0xFFu) >> 2u;

  //
  // Instead of reconstructing barycentrics, we use interpolated attrN.xy directly.
  //

  float2 uv2 = IN.attr2.xy;
  float2 uv0 = IN.attr0.xy;
  float2 uv1 = IN.attr1.xy;
  float2 uv3 = IN.attr3.xy;

  // luminance?
  float4 s24 = fs_img24.Sample(fs_sampsgpr_44, uv2);

  float _122 = s24.x;
  float _123 = s24.y;
  float _124 = s24.z;

  float4 s4 = fs_img4.SampleLevel(fs_sampsgpr_12, uv0, 0.0);
  float3 untonemapped = s4.rgb;

  float _154 = s4.x;
  float _155 = s4.y;
  float _156 = s4.z;

  // bloom
  float4 s16 = fs_img16.SampleLevel(fs_sampsgpr_40, uv1, 0.0);
  float4 s32 = fs_img32.SampleLevel(fs_sampsgpr_48, uv3, 0.0);
  float _219 = s32.x;

  //
  // All SSBO math stays unchanged
  //

  uint _221 = 752u + buf0_dword_off;
  uint _225 = 753u + buf0_dword_off;
  uint _229 = 754u + buf0_dword_off;
  uint _233 = 755u + buf0_dword_off;
  uint _237 = 756u + buf0_dword_off;

  float f221 = asfloat(ssbo_1_1[0].data[_221]);
  float f225 = asfloat(ssbo_1_1[0].data[_225]);
  float f229 = asfloat(ssbo_1_1[0].data[_229]);
  float f233 = asfloat(ssbo_1_1[0].data[_233]);
  float f237 = asfloat(ssbo_1_1[0].data[_237]);

  float _242 = fma(_124, f221, f225);

  uint _244 = 616u + buf0_dword_off;
  float f244 = asfloat(ssbo_1_1[0].data[_244]);

  float _253 = fma(_123, f221, f225);
  float _255 = 0.333333343f * _242;

  uint _257 = 744u + buf0_dword_off;
  uint _261 = 745u + buf0_dword_off;
  uint _265 = 746u + buf0_dword_off;
  uint _269 = 747u + buf0_dword_off;

  float f257 = asfloat(ssbo_1_1[0].data[_257]);
  float f261 = asfloat(ssbo_1_1[0].data[_261]);
  float f265 = asfloat(ssbo_1_1[0].data[_265]);
  float f269 = asfloat(ssbo_1_1[0].data[_269]);

  float _274 = fma(f221, _122, f225);
  float _276 = fma(_274, 0.333333343f, fma(_253, 0.333333343f, _255));

  uint _278 = 832u + buf0_dword_off;
  float f278 = asfloat(ssbo_1_1[0].data[_278]);

  float _285 = _242 - _276;
  float _286 = _253 - _276;
  float _289 = _274 - _276;

  float _294 = fma(fma(f237, _285, _276), f229, f233);
  float _296 = f244 * _156;

  float _301 = fma(fma(f237, _286, _276), f229, f233);
  float _303 = f244 * _155;

  float _304 = _294 * _296;

  uint _306 = 748u + buf0_dword_off;
  float f306 = asfloat(ssbo_1_1[0].data[_306]);

  float _311 = fma(_124, f257, f261);
  float _314 = fma(f229, fma(f237, _289, _276), f233);
  float _316 = f244 * _154;

  float _317 = _301 * _303;

  float _322 = fma(_123, f257, f261);
  float _323 = 0.333333343f * _311;

  float _324 = _314 * _316;

  float _328 = (-max(6.103515625e-05f, _304)) * f278;
  float _336 = (-max(6.103515625e-05f, _317)) * f278;
  float _342 = (-max(6.103515625e-05f, _324)) * f278;

  float _338 = 1.44269502f * _328;
  float _343 = 1.44269502f * _336;
  float _346 = 1.44269502f * _342;

  float _339 = fma(_311, 0.333333343f, fma(_322, 0.333333343f, _323));

  float _349 = 1.0f - exp2(_338);
  float _403 = 1.0f - exp2(_343);
  float _413 = 1.0f - exp2(_346);

  uint _282 = 833u + buf0_dword_off;
  float f282 = asfloat(ssbo_1_1[0].data[_282]);

  float _405 = (-1.0f) + _349;
  float _414 = (-1.0f) + _403;
  float _424 = (-1.0f) + _413;

  float _416 = fma(f282, _405, 1.0f);
  float _426 = fma(f282, _414, 1.0f);
  float _436 = fma(f282, _424, 1.0f);

  float _427 = _349 * _416;
  float _445 = _403 * _426;
  float _444 = _413 * _436;

  float _434 = clamp(f244 * (s16.z * _294), 0.0f, 1.0f);
  float _439 = clamp(_416 * _427, 0.0f, 1.0f);
  float _443 = clamp(f244 * (s16.y * _301), 0.0f, 1.0f);
  float _446 = clamp(_426 * _445, 0.0f, 1.0f);
  float _450 = clamp(f244 * (s16.x * _314), 0.0f, 1.0f);
  float _452 = clamp(_436 * _444, 0.0f, 1.0f);

  float _447 = _439 + _434;
  float _453 = _446 + _443;

  float _455 = fma(-_439, _434, _447);
  float _486 = fma(-_446, _443, _453);
  float _484 = _452 + _450;
  float _497 = fma(-_452, _450, _484);

  uint _473 = 808u + buf0_dword_off;
  float f473 = asfloat(ssbo_1_1[0].data[_473]);

  uint _477 = 809u + buf0_dword_off;
  float f477 = asfloat(ssbo_1_1[0].data[_477]);

  uint _481 = 810u + buf0_dword_off;
  float f481 = asfloat(ssbo_1_1[0].data[_481]);

  float _511 =
      fma(asfloat(ssbo_1_1[0].data[840u + buf0_dword_off]), _497,
          fma(asfloat(ssbo_1_1[0].data[841u + buf0_dword_off]), _486,
              fma(asfloat(ssbo_1_1[0].data[842u + buf0_dword_off]), _455,
                  asfloat(ssbo_1_1[0].data[843u + buf0_dword_off]))))
      + 2.38418579e-07f;

  float _513 =
      fma(asfloat(ssbo_1_1[0].data[844u + buf0_dword_off]), _497,
          fma(asfloat(ssbo_1_1[0].data[845u + buf0_dword_off]), _486,
              fma(asfloat(ssbo_1_1[0].data[846u + buf0_dword_off]), _455,
                  asfloat(ssbo_1_1[0].data[847u + buf0_dword_off]))))
      + 2.38418579e-07f;

  float _515 =
      fma(asfloat(ssbo_1_1[0].data[848u + buf0_dword_off]), _497,
          fma(asfloat(ssbo_1_1[0].data[849u + buf0_dword_off]), _486,
              fma(asfloat(ssbo_1_1[0].data[850u + buf0_dword_off]), _455,
                  asfloat(ssbo_1_1[0].data[851u + buf0_dword_off]))))
      + 2.38418579e-07f;

  float _521 = asfloat(ssbo_1_1[0].data[623u + buf0_dword_off]) * s4.w;

  float _545 = f481 + fma(f477, _219, exp2(f473 * log2(clamp(_511, 0.0f, 1.0f))));
  float _547 = f481 + fma(f477, _219, exp2(f473 * log2(clamp(_513, 0.0f, 1.0f))));
  float _549 = f481 + fma(f477, _219, exp2(f473 * log2(clamp(_515, 0.0f, 1.0f))));

  // Swizzles
  OUT.frag_color0 = float4(_549, _547, _545,
                           clamp(
                               fma(_154, asfloat(ssbo_1_1[0].data[620u + buf0_dword_off]),
                                   fma(asfloat(ssbo_1_1[0].data[621u + buf0_dword_off]), _155,
                                       fma(asfloat(ssbo_1_1[0].data[622u + buf0_dword_off]), _156, _521))),
                               0.0f, 1.0f));

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
