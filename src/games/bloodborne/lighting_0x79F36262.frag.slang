#define USE_AUX_PUSHCONSTANTS

#include "./shared.h"
// -----------------------------------------------------------
// Types (kept from original, though unused here)
// -----------------------------------------------------------
struct full_result_i32x2 {
  int _m0;
  int _m1;
};

struct full_result_u32x2 {
  uint _m0;
  uint _m1;
};

struct frexp_result_f32 {
  float _m0;
  int _m1;
};

// -----------------------------------------------------------
// Buffers
// -----------------------------------------------------------

struct SSBO1_F {
  float data[];
};

struct SSBO2_U {
  uint data[];
};

struct SSBO3_U {
  uint data[];
};

// Set = 0, binding = 0,1,2 as in original GLSL
[[vk::binding(0, 0)]]
StructuredBuffer<SSBO1_F> ssbo_1_3;
[[vk::binding(1, 0)]]
StructuredBuffer<SSBO2_U> ssbo_2_1;
[[vk::binding(2, 0)]]
StructuredBuffer<SSBO3_U> ssbo_3_1;

// -----------------------------------------------------------
// Textures & Samplers (set = 0, bindings as in original)
// -----------------------------------------------------------

[[vk::binding(3, 0)]]
Texture2D fs_img32;
[[vk::binding(4, 0)]]
Texture2D fs_img4;
[[vk::binding(5, 0)]]
Texture2D fs_img24;
[[vk::binding(6, 0)]]
Texture2D fs_img16;
[[vk::binding(7, 0)]]
Texture2D fs_img40;

[[vk::binding(8, 0)]]
SamplerState fs_sampsgpr_12;
[[vk::binding(9, 0)]]
SamplerState fs_sampsgpr_48;

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
// Fragment input/output
//   Replaces pervertexEXT arrays + gl_BaryCoordEXT with
//   simple interpolated attributes (uv, pos) from vertex shader.
// -----------------------------------------------------------

struct PsInput {
  // Was: fs_in_attr0_p[3] / fs_in_attr1_p[3] with barycentrics
  // Now: single interpolated values
  [[vk::location(0)]]
  float4 attr0 : TEXCOORD0;  // used for _382, _373
  [[vk::location(1)]]
  float4 attr1 : TEXCOORD1;  // used for _157, _163
};

struct PsOutput {
  float4 frag_color0 : SV_Target0;
  float4 frag_color1 : SV_Target1;
};

float inversesqrt(float x) {
  return 1.0 / sqrt(x);
}

// Decode an 8-bit index that was encoded as index / 255.0 in alpha.
// Works for both RGBA8_UNORM and RGBA16_FLOAT sampling.
inline uint DecodeIndex8FromAlpha(float a) {
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
PsOutput main(PsInput IN) {
  PsOutput OUT;

  // Decode buffer offsets (same as original)
  uint buf0_off = (push_data.buf_offsets0.x >> 0u) & 0xFFu;
  uint buf0_dword_off = buf0_off >> 2u;
  uint buf1_dword_off = ((push_data.buf_offsets0.x >> 8u) & 0xFFu) >> 2u;
  uint buf2_dword_off = ((push_data.buf_offsets0.x >> 16u) & 0xFFu) >> 2u;

  // --------------------------------------------------------------------
  // Replace barycentric reconstruction:
  //   _157, _163 were computed from fs_in_attr1_p[] + gl_BaryCoordEXT
  //   _382, _373 were computed from fs_in_attr0_p[] + gl_BaryCoordEXT
  // Now: just use interpolated attributes directly.
  // --------------------------------------------------------------------
  float _157 = IN.attr1.x;
  float _163 = IN.attr1.y;

  float _382 = IN.attr0.x;
  float _373 = IN.attr0.y;

  float2 uv = float2(_157, _163);

  float4 _168 = fs_img32.Sample(fs_sampsgpr_12, uv);
  float _169 = _168.x;
  float _170 = _168.y;
  float _171 = _168.z;

  float4 _177 = fs_img4.Sample(fs_sampsgpr_12, uv);
  float4 _185 = fs_img24.Sample(fs_sampsgpr_12, uv);
  float _187 = _185.y;
  float4 _193 = fs_img16.Sample(fs_sampsgpr_12, uv);
  float _194 = _193.x;

  uint _197 = DecodeIndex8FromAlpha(_168.w);
  uint _199 = _197 * 18u;

  float4 _206 = fs_img40.Sample(fs_sampsgpr_48, uv);
  float _208 = _206.z;
  float _209 = _206.w;

  uint _212 = ((_199 + 17u) * 4u) + buf0_dword_off;
  float4 _229 = float4(
      ssbo_1_3[0].data[_212],
      ssbo_1_3[0].data[_212 + 1u],
      ssbo_1_3[0].data[_212 + 2u],
      ssbo_1_3[0].data[_212 + 3u]);
  float4 _232 = _229;
  float _236 = _232.w;

  uint _309 = 35u + buf1_dword_off;
  uint _325 = 39u + buf1_dword_off;
  uint _341 = 43u + buf1_dword_off;

  float _397 = fma(2.0, _177.z, -1.0);

  float _405 = 1.0 / fma(asfloat(ssbo_2_1[0].data[12u + buf1_dword_off]), _382, fma(asfloat(ssbo_2_1[0].data[13u + buf1_dword_off]), _373, fma(asfloat(ssbo_2_1[0].data[14u + buf1_dword_off]), _194, asfloat(ssbo_2_1[0].data[15u + buf1_dword_off]))));

  float _409 = fma(2.0, _177.y, -1.0);
  float _410 = _397 * _397;

  float _415 = fma(
                   asfloat(ssbo_2_1[0].data[8u + buf1_dword_off]), _382,
                   fma(asfloat(ssbo_2_1[0].data[9u + buf1_dword_off]), _373,
                       fma(asfloat(ssbo_2_1[0].data[10u + buf1_dword_off]), _194,
                           asfloat(ssbo_2_1[0].data[11u + buf1_dword_off]))))
               * _405;

  float _416 = fma(2.0, _177.x, -1.0);

  float _420 = fma(
                   asfloat(ssbo_2_1[0].data[4u + buf1_dword_off]), _382,
                   fma(asfloat(ssbo_2_1[0].data[5u + buf1_dword_off]), _373,
                       fma(asfloat(ssbo_2_1[0].data[6u + buf1_dword_off]), _194,
                           asfloat(ssbo_2_1[0].data[7u + buf1_dword_off]))))
               * _405;

  float _431 = 1.0 - asfloat(ssbo_3_1[0].data[76u + buf2_dword_off]);
  float _433 = 1.0 - asfloat(ssbo_3_1[0].data[77u + buf2_dword_off]);
  float _435 = 1.0 - asfloat(ssbo_3_1[0].data[78u + buf2_dword_off]);

  float _437 = fma(
                   asfloat(ssbo_2_1[0].data[0u + buf1_dword_off]), _382,
                   fma(asfloat(ssbo_2_1[0].data[1u + buf1_dword_off]), _373,
                       fma(asfloat(ssbo_2_1[0].data[2u + buf1_dword_off]), _194,
                           asfloat(ssbo_2_1[0].data[3u + buf1_dword_off]))))
               * _405;

  float _444 = inversesqrt(fma(_416, _416, fma(_409, _409, _410)));
  float _451 = _416 * _444;
  float _452 = _409 * _444;
  float _453 = _397 * _444;

  float _455 = asfloat(ssbo_2_1[0].data[_309]) - fma(asfloat(ssbo_2_1[0].data[32u + buf1_dword_off]), _437, fma(asfloat(ssbo_2_1[0].data[33u + buf1_dword_off]), _420, fma(asfloat(ssbo_2_1[0].data[34u + buf1_dword_off]), _415, asfloat(ssbo_2_1[0].data[_309]))));

  float _457 = asfloat(ssbo_2_1[0].data[_325]) - fma(asfloat(ssbo_2_1[0].data[36u + buf1_dword_off]), _437, fma(asfloat(ssbo_2_1[0].data[37u + buf1_dword_off]), _420, fma(asfloat(ssbo_2_1[0].data[38u + buf1_dword_off]), _415, asfloat(ssbo_2_1[0].data[_325]))));

  float _459 = asfloat(ssbo_2_1[0].data[_341]) - fma(asfloat(ssbo_2_1[0].data[40u + buf1_dword_off]), _437, fma(asfloat(ssbo_2_1[0].data[41u + buf1_dword_off]), _420, fma(asfloat(ssbo_2_1[0].data[42u + buf1_dword_off]), _415, asfloat(ssbo_2_1[0].data[_341]))));

  float _460 = (-1.0) + _206.x;
  float _461 = 1.0 - _431;
  float _462 = 1.0 - _433;
  float _463 = 1.0 - _435;
  float _464 = fma(_232.x, _460, 1.0);
  float _466 = 10.0 * _187;

  float _470 = fma(_209, _463, _435);
  float _472 = (-1.0) + _464;

  uint _475 = (_197 * 72u) + buf0_dword_off;
  float4 _492 = float4(
      ssbo_1_3[0].data[_475],
      ssbo_1_3[0].data[_475 + 1u],
      ssbo_1_3[0].data[_475 + 2u],
      ssbo_1_3[0].data[_475 + 3u]);
  float4 _493 = _492;
  float _494 = _493.x;
  float _495 = _493.y;
  float _496 = _493.z;
  float _498 = _493.w;

  uint _500 = ((_199 + 1u) * 4u) + buf0_dword_off;
  float4 _517 = float4(
      ssbo_1_3[0].data[_500],
      ssbo_1_3[0].data[_500 + 1u],
      ssbo_1_3[0].data[_500 + 2u],
      ssbo_1_3[0].data[_500 + 3u]);
  float4 _518 = _517;
  float _519 = _518.x;
  float _520 = _518.y;
  float _521 = _518.z;
  float _522 = _518.w;

  uint _524 = ((_199 + 4u) * 4u) + buf0_dword_off;
  float4 _537 = float4(
      ssbo_1_3[0].data[_524],
      ssbo_1_3[0].data[_524 + 1u],
      ssbo_1_3[0].data[_524 + 2u],
      0.0);
  float4 _538 = _537;
  float _539 = _538.x;
  float _540 = _538.y;
  float _541 = _538.z;

  uint _543 = ((_199 + 5u) * 4u) + buf0_dword_off;
  float4 _556 = float4(
      ssbo_1_3[0].data[_543],
      ssbo_1_3[0].data[_543 + 1u],
      ssbo_1_3[0].data[_543 + 2u],
      0.0);
  float4 _557 = _556;
  float _558 = _557.x;
  float _559 = _557.y;
  float _560 = _557.z;

  float _561 = _169 * _169;
  float _562 = _170 * _170;
  float _563 = _171 * _171;

  float _565 = 13.0 * _185.x;
  float _566 = _208 * fma(_209, _461, _431);
  float _567 = _208 * fma(_209, _462, _433);
  float _568 = _208 * _470;

  float _569 = fma(_232.y, _472, 1.0);
  float _570 = fma(_232.z, _472, 1.0);
  float _571 = fma(_236, _472, 1.0);

  bool _572 = _466 > 0.0;

  uint _575;
  if (_572) {
    float _573 = _453 * _496;
    _575 = asuint(_573);
  } else {
    _575 = asuint(_236);
  }

  uint _608;
  uint _609;
  uint _610;

  if (_572) {
    float _582 = 1.0 / fma(2.0, _466, 1.0);
    float _583 = max(0.0,
                     fma(_187, 10.0,
                         fma(_494, _451,
                             fma(_495, _452, asfloat(_575)))))
                 * _582;

    float _584 = _539 * _583;
    float _585 = _540 * _583;
    float _586 = _541 * _583;
    float _587 = _464 * _584;
    float _588 = _464 * _585;
    float _589 = _464 * _586;

    float _590 = _453 * _521;

    float _595 = max(0.0,
                     fma(_187, 10.0,
                         fma(_519, _451,
                             fma(_520, _452, _590))))
                 * _582;

    float _596 = _558 * _595;
    float _597 = _559 * _595;
    float _598 = _560 * _595;
    float _599 = _569 * _596;
    float _600 = _569 * _597;
    float _601 = _569 * _598;

    _608 = asuint(fma(_589, _568, _601));
    _609 = asuint(fma(_588, _567, _600));
    _610 = asuint(fma(_587, _566, _599));
  } else {
    _608 = asuint(_470);
    _609 = _575;
    _610 = asuint(_187);
  }

  uint _614;
  if (!_572) {
    float _612 = _558 * _569;
    _614 = asuint(_612);
  } else {
    _614 = _610;
  }

  uint _654;
  uint _655;
  uint _656;

  if (!_572) {
    float _616 = _559 * _569;
    float _617 = _560 * _569;

    float _618 = _453 * _496;
    float _621 = clamp(fma(_494, _451, fma(_495, _452, _618)), 0.0, 1.0);

    float _622 = _453 * _521;
    float _625 = clamp(fma(_519, _451, fma(_520, _452, _622)), 0.0, 1.0);

    float _638 = _539 * _464;
    float _639 = _540 * _464;
    float _640 = _541 * _464;

    float _641 = fma(-_561, _621, _621) * _638;
    float _642 = fma(-_562, _621, _621) * _639;
    float _643 = fma(-_563, _621, _621) * _640;

    float _645 = asfloat(_614) * fma(-_561, _625, _625);
    float _646 = _616 * fma(-_562, _625, _625);
    float _647 = _617 * fma(-_563, _625, _625);

    _654 = asuint(fma(_643, _568, _647));
    _655 = asuint(fma(_642, _567, _646));
    _656 = asuint(fma(_641, _566, _645));
  } else {
    _654 = _608;
    _655 = _609;
    _656 = _614;
  }

  float _657 = _459 * _459;
  float _660 = inversesqrt(fma(_455, _455, fma(_457, _457, _657)));
  float _661 = _455 * _660;
  float _662 = _457 * _660;
  float _663 = _459 * _660;

  float _664 = _453 * _496;
  float _667 = _453 * _521;

  float _670 = fma(_455, _660, _494);
  float _671 = fma(_457, _660, _495);
  float _672 = fma(_459, _660, _496);
  float _673 = _672 * _672;

  float _676 = inversesqrt(fma(_670, _670, fma(_671, _671, _673)));
  float _677 = _670 * _676;
  float _678 = _671 * _676;
  float _680 = _672 * _676;

  float _681 = fma(_455, _660, _519);
  float _682 = fma(_457, _660, _520);
  float _683 = fma(_459, _660, _521);
  float _684 = _683 * _683;

  float _687 = inversesqrt(fma(_681, _681, fma(_682, _682, _684)));
  float _688 = _681 * _687;
  float _689 = _682 * _687;
  float _690 = _683 * _687;

  float _691 = _453 * _680;
  float _695 = _453 * _690;
  float _699 = _663 * _680;
  float _703 = _663 * _690;

  uint _710;
  if (_572) {
    float _708 = _496 * (-_663);
    _710 = asuint(_708);
  } else {
    _710 = asuint(_496);
  }

  uint _767;
  uint _768;
  uint _769;
  uint _770;
  uint _771;

  if (_572) {
    float _717 = _453 * _663;
    float _720 = 1.0 + fma(_661, _451, fma(_662, _452, _717));
    float _723 = 0.5 * max(0.0, _720);
    float _726 = 32.0 * log2(clamp(fma(_494, -_661, fma(_495, -_662, asfloat(_710))), 0.0, 1.0));
    float _728 = _723 * _723;
    float _729 = exp2(_726) * _728;

    float _730 = _539 * _729;
    float _731 = _540 * _729;
    float _732 = _541 * _729;
    float _733 = _466 * _730;
    float _734 = _466 * _731;
    float _735 = _466 * _732;

    float _737 = _521 * (-_663);
    float _744 = 32.0 * log2(clamp(fma(_519, -_661, fma(_520, -_662, _737)), 0.0, 1.0));
    float _746 = _728 * exp2(_744);

    float _747 = _558 * _746;
    float _748 = _559 * _746;
    float _749 = _560 * _746;

    float _750 = _466 * _747;
    float _751 = _466 * _748;
    float _752 = _466 * _749;

    float _756 = fma(_734, _464, asfloat(_655));
    float _759 = fma(_735, _464, asfloat(_654));

    _767 = asuint(fma(_751, _569, _756));
    _768 = asuint(fma(_752, _569, _759));
    _769 = asuint(fma(_750, _569, fma(_733, _464, asfloat(_656))));
    _770 = asuint(_759);
    _771 = asuint(_756);
  } else {
    _767 = asuint(_678);
    _768 = asuint(_466);
    _769 = _656;
    _770 = _654;
    _771 = _655;
  }

  uint _773;
  uint _774;

  if (!_572) {
    _773 = _771;
    _774 = _770;
  } else {
    _773 = _767;
    _774 = _768;
  }

  float _777 = exp2(_565);
  float _778 = 1.0 - clamp(fma(_661, _688, fma(_662, _689, _703)), 0.0, 1.0);

  uint _781 = ((_199 + 8u) * 4u) + buf0_dword_off;
  float4 _794 = float4(
      ssbo_1_3[0].data[_781],
      ssbo_1_3[0].data[_781 + 1u],
      ssbo_1_3[0].data[_781 + 2u],
      0.0);
  float4 _795 = _794;

  uint _800 = ((_199 + 9u) * 4u) + buf0_dword_off;
  float4 _813 = float4(
      ssbo_1_3[0].data[_800],
      ssbo_1_3[0].data[_800 + 1u],
      ssbo_1_3[0].data[_800 + 2u],
      0.0);
  float4 _814 = _813;

  float _820 = 1.0 - clamp(fma(_661, _677, fma(_662, _678, _699)), 0.0, 1.0);

  uint _824 = ((_199 + 16u) * 4u) + buf0_dword_off;
  float4 _833 = float4(
      ssbo_1_3[0].data[_824],
      ssbo_1_3[0].data[_824 + 1u],
      0.0,
      0.0);
  float4 _834 = _833;

  float _842 = _498 * _777;
  float _844 = _522 * _777;
  float _846 = _842 * (1.0 / fma(_777, 0.01000213623046875, _498));
  float _847 = _844 * (1.0 / fma(_777, 0.01000213623046875, _522));
  float _848 = max(1.0, _846);
  float _849 = max(1.0, _847);

  float _852 = fma(_848, 0.125, 0.25);
  float _853 = fma(0.125, _849, 0.25);

  float _854 = _848 * log2(clamp(fma(_677, _451, fma(_678, _452, _691)), 0.0, 1.0));
  float _855 = _849 * log2(clamp(fma(_688, _451, fma(_689, _452, _695)), 0.0, 1.0));

  float _857 = 5.0 * log2(_778);
  float _858 = exp2(_854);
  float _859 = exp2(_855);

  float _860 = 5.0 * log2(_820);
  float _861 = exp2(_857);
  float _862 = exp2(_860);

  float _863 = _795.x * _852;
  float _864 = _795.y * _852;
  float _865 = _795.z * _852;

  float _866 = _814.x * _853;
  float _867 = _814.y * _853;
  float _868 = _814.z * _853;

  float _869 = _464 * _863;
  float _870 = _464 * _864;
  float _871 = _464 * _865;

  float _872 = _569 * _866;
  float _873 = _569 * _867;
  float _874 = _569 * _868;

  float _879 = fma(_451, _494, fma(_452, _495, _664)) * 16.0;
  float _880 = clamp(_879, 0.0, 1.0);

  float _881 = fma(_451, _519, fma(_452, _520, _667)) * 16.0;
  float _882 = clamp(_881, 0.0, 1.0);

  float _883 = _858 * _869;
  float _884 = _858 * _870;
  float _885 = _858 * _871;

  float _886 = _859 * _872;
  float _887 = _859 * _873;
  float _888 = _859 * _874;

  float _898 = _880 * _883;
  float _899 = _880 * _884;
  float _900 = _880 * _885;

  float _901 = _882 * _886;
  float _902 = _882 * _887;
  float _903 = _882 * _888;

  float _910 = _566 * _898;
  float _912 = _567 * _899;
  float _913 = _568 * _900;

  float _914 = fma(-_861, _561, fma(_169, _169, _861)) * _901;
  float _915 = fma(-_861, _562, fma(_170, _170, _861)) * _902;
  float _916 = fma(-_861, _563, fma(_171, _171, _861)) * _903;

  float _917 = fma(_910, fma(-_862, _561, fma(_169, _169, _862)), _914);
  float _919 = fma(_912, fma(-_862, _562, fma(_170, _170, _862)), _915);
  float _921 = fma(_913, fma(-_862, _563, fma(_171, _171, _862)), _916);

  bool _923 = 0u < asuint(_834.x);

  uint _925;
  if (_923) {
    _925 = _199 + 2u;
  } else {
    _925 = asuint(_910);
  }

  uint _1069;
  uint _1070;
  uint _1071;
  uint _1072;
  uint _1073;
  uint _1074;

  if (_923) {
    uint _929 = (_925 * 4u) + buf0_dword_off;
    float4 _946 = float4(
        ssbo_1_3[0].data[_929],
        ssbo_1_3[0].data[_929 + 1u],
        ssbo_1_3[0].data[_929 + 2u],
        ssbo_1_3[0].data[_929 + 3u]);
    float4 _947 = _946;
    float _948 = _947.x;
    float _949 = _947.y;
    float _950 = _947.z;
    float _951 = _947.w;

    uint _953 = ((_199 + 6u) * 4u) + buf0_dword_off;
    float4 _966 = float4(
        ssbo_1_3[0].data[_953],
        ssbo_1_3[0].data[_953 + 1u],
        ssbo_1_3[0].data[_953 + 2u],
        0.0);
    float4 _967 = _966;

    uint _972 = ((_199 + 10u) * 4u) + buf0_dword_off;
    float4 _985 = float4(
        ssbo_1_3[0].data[_972],
        ssbo_1_3[0].data[_972 + 1u],
        ssbo_1_3[0].data[_972 + 2u],
        0.0);
    float4 _986 = _985;

    float _990 = _570 * _967.x;
    float _991 = _570 * _967.y;
    float _992 = _570 * _967.z;

    float _993 = _453 * _950;
    float _996 = fma(0.5, fma(_948, _451, fma(_949, _452, _993)), 0.5);
    float _997 = _996 * _996;
    float _998 = clamp(_997, 0.0, 1.0);

    float _1014 = fma(_455, _660, _948);
    float _1015 = fma(_457, _660, _949);
    float _1016 = fma(_459, _660, _950);
    float _1017 = _1016 * _1016;

    float _1020 = inversesqrt(fma(_1014, _1014, fma(_1015, _1015, _1017)));
    float _1021 = _1014 * _1020;
    float _1022 = _1015 * _1020;
    float _1023 = _1016 * _1020;

    float _1024 = _453 * _1023;

    float _1028 = _777 * _951;
    float _1031 = _1028 * (1.0 / fma(_777, 0.01000213623046875, _951));
    float _1032 = max(1.0, _1031);

    float _1034 = _1032 * log2(clamp(fma(_1021, _451, fma(_1022, _452, _1024)), 0.0, 1.0));
    float _1035 = exp2(_1034);

    float _1036 = _663 * _1023;
    float _1040 = 1.0 - clamp(fma(_661, _1021, fma(_662, _1022, _1036)), 0.0, 1.0);

    float _1042 = 5.0 * log2(_1040);
    float _1043 = exp2(_1042);

    float _1053 = fma(0.125, _1032, 0.25);
    float _1054 = _986.x * _1053;
    float _1055 = _986.y * _1053;
    float _1056 = _986.z * _1053;

    float _1057 = _570 * _1054;
    float _1058 = _570 * _1055;
    float _1059 = _570 * _1056;

    float _1060 = _1035 * _1057;
    float _1061 = _1035 * _1058;
    float _1062 = _1035 * _1059;

    _1069 = asuint(fma(_991, fma(-_562, _998, _998), asfloat(_773)));
    _1070 = asuint(fma(_1061, fma(-_1043, _562, fma(_170, _170, _1043)), _919));
    _1071 = asuint(fma(_1062, fma(-_1043, _563, fma(_171, _171, _1043)), _921));
    _1072 = asuint(fma(_1060, fma(-_1043, _561, fma(_169, _169, _1043)), _917));
    _1073 = asuint(fma(_992, fma(-_563, _998, _998), asfloat(_774)));
    _1074 = asuint(fma(_990, fma(-_561, _998, _998), asfloat(_769)));
  } else {
    _1069 = _773;
    _1070 = asuint(_919);
    _1071 = asuint(_921);
    _1072 = asuint(_917);
    _1073 = _774;
    _1074 = _769;
  }

  uint _1223;
  uint _1224;
  uint _1225;
  uint _1226;
  uint _1227;
  uint _1228;

  if (0u < asuint(_834.y)) {
    uint _1080 = ((_199 + 3u) * 4u) + buf0_dword_off;
    float4 _1097 = float4(
        ssbo_1_3[0].data[_1080],
        ssbo_1_3[0].data[_1080 + 1u],
        ssbo_1_3[0].data[_1080 + 2u],
        ssbo_1_3[0].data[_1080 + 3u]);
    float4 _1098 = _1097;
    float _1099 = _1098.x;
    float _1100 = _1098.y;
    float _1101 = _1098.z;
    float _1102 = _1098.w;

    uint _1104 = ((_199 + 7u) * 4u) + buf0_dword_off;
    float4 _1117 = float4(
        ssbo_1_3[0].data[_1104],
        ssbo_1_3[0].data[_1104 + 1u],
        ssbo_1_3[0].data[_1104 + 2u],
        0.0);
    float4 _1118 = _1117;

    uint _1123 = ((_199 + 11u) * 4u) + buf0_dword_off;
    float4 _1136 = float4(
        ssbo_1_3[0].data[_1123],
        ssbo_1_3[0].data[_1123 + 1u],
        ssbo_1_3[0].data[_1123 + 2u],
        0.0);
    float4 _1137 = _1136;

    float _1141 = fma(_455, _660, _1099);
    float _1142 = _571 * _1118.x;
    float _1143 = _571 * _1118.y;
    float _1144 = _571 * _1118.z;

    float _1145 = _453 * _1101;
    float _1148 = fma(0.5, fma(_1099, _451, fma(_1100, _452, _1145)), 0.5);
    float _1149 = _1148 * _1148;
    float _1150 = clamp(_1149, 0.0, 1.0);

    float _1166 = fma(_457, _660, _1100);
    float _1167 = fma(_459, _660, _1101);
    float _1168 = _1167 * _1167;

    float _1171 = inversesqrt(fma(_1141, _1141, fma(_1166, _1166, _1168)));
    float _1172 = _1141 * _1171;
    float _1173 = _1166 * _1171;
    float _1174 = _1167 * _1171;

    float _1175 = _453 * _1174;

    float _1179 = _777 * _1102;
    float _1182 = _1179 * (1.0 / fma(_777, 0.01000213623046875, _1102));
    float _1183 = max(1.0, _1182);

    float _1185 = _1183 * log2(clamp(fma(_1172, _451, fma(_1173, _452, _1175)), 0.0, 1.0));
    float _1186 = exp2(_1185);

    float _1187 = _663 * _1174;
    float _1191 = 1.0 - clamp(fma(_661, _1172, fma(_662, _1173, _1187)), 0.0, 1.0);

    float _1193 = 5.0 * log2(_1191);
    float _1194 = exp2(_1193);

    float _1204 = fma(0.125, _1183, 0.25);
    float _1205 = _1137.x * _1204;
    float _1206 = _1137.y * _1204;
    float _1207 = _1137.z * _1204;

    float _1208 = _571 * _1205;
    float _1209 = _571 * _1206;
    float _1210 = _571 * _1207;

    float _1211 = _1186 * _1208;
    float _1212 = _1186 * _1209;
    float _1213 = _1186 * _1210;

    _1223 = asuint(fma(_1144, fma(-_563, _1150, _1150), asfloat(_1073)));
    _1224 = asuint(fma(_1142, fma(-_561, _1150, _1150), asfloat(_1074)));
    _1225 = asuint(fma(_1143, fma(-_562, _1150, _1150), asfloat(_1069)));
    _1226 = asuint(fma(_1213, fma(-_1194, _563, fma(_171, _171, _1194)), asfloat(_1071)));
    _1227 = asuint(fma(_1211, fma(-_1194, _561, fma(_169, _169, _1194)), asfloat(_1072)));
    _1228 = asuint(fma(_1212, fma(-_1194, _562, fma(_170, _170, _1194)), asfloat(_1070)));
  } else {
    _1223 = _1073;
    _1224 = _1074;
    _1225 = _1069;
    _1226 = _1071;
    _1227 = _1072;
    _1228 = _1070;
  }

  OUT.frag_color1.x = asfloat(_1227);
  OUT.frag_color1.y = asfloat(_1228);
  OUT.frag_color1.z = asfloat(_1226);
  OUT.frag_color1.w = 1.0;

  OUT.frag_color0.x = asfloat(_1224);
  OUT.frag_color0.y = asfloat(_1225);
  OUT.frag_color0.z = asfloat(_1223);
  OUT.frag_color0.w = 1.0;

  return OUT;
}
