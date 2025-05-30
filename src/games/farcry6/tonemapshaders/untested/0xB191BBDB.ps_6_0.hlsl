#include "../../common.hlsli"
struct SExposureData {
  float SExposureData_000;
  float SExposureData_004;
  float SExposureData_008;
  float SExposureData_012;
  float SExposureData_016;
  float SExposureData_020;
};

StructuredBuffer<SExposureData> t0 : register(t0);

Texture2D<float4> t1 : register(t1);

Texture2D<float> t2 : register(t2);

Texture2D<float4> t3 : register(t3);

Texture2D<float4> t4 : register(t4);

Texture2D<float4> t5 : register(t5);

Texture3D<float4> t6 : register(t6);

Texture2D<float4> t7 : register(t7);

Texture2D<float4> t8 : register(t8);

Texture2D<float4> t9 : register(t9);

Texture3D<float2> t10 : register(t10);

Texture2D<float4> t11 : register(t11);

cbuffer cb0 : register(b0) {
  float cb0_028x : packoffset(c028.x);
  float cb0_028z : packoffset(c028.z);
};

cbuffer cb1 : register(b1) {
  float cb1_018y : packoffset(c018.y);
  float cb1_018z : packoffset(c018.z);
  uint cb1_018w : packoffset(c018.w);
};

cbuffer cb2 : register(b2) {
  float cb2_000x : packoffset(c000.x);
  float cb2_000y : packoffset(c000.y);
  float cb2_000z : packoffset(c000.z);
  float cb2_001x : packoffset(c001.x);
  float cb2_001y : packoffset(c001.y);
  float cb2_001z : packoffset(c001.z);
  float cb2_002x : packoffset(c002.x);
  float cb2_002y : packoffset(c002.y);
  float cb2_002z : packoffset(c002.z);
  float cb2_002w : packoffset(c002.w);
  float cb2_005x : packoffset(c005.x);
  float cb2_006x : packoffset(c006.x);
  float cb2_006y : packoffset(c006.y);
  float cb2_006z : packoffset(c006.z);
  float cb2_006w : packoffset(c006.w);
  float cb2_007x : packoffset(c007.x);
  float cb2_007y : packoffset(c007.y);
  float cb2_007z : packoffset(c007.z);
  float cb2_007w : packoffset(c007.w);
  float cb2_008x : packoffset(c008.x);
  float cb2_008y : packoffset(c008.y);
  float cb2_008z : packoffset(c008.z);
  float cb2_008w : packoffset(c008.w);
  float cb2_015x : packoffset(c015.x);
  float cb2_015y : packoffset(c015.y);
  float cb2_015z : packoffset(c015.z);
  float cb2_015w : packoffset(c015.w);
  float cb2_016x : packoffset(c016.x);
  float cb2_016y : packoffset(c016.y);
  float cb2_016z : packoffset(c016.z);
  float cb2_016w : packoffset(c016.w);
  float cb2_017x : packoffset(c017.x);
  float cb2_017y : packoffset(c017.y);
  float cb2_017z : packoffset(c017.z);
  float cb2_017w : packoffset(c017.w);
  float cb2_018x : packoffset(c018.x);
  float cb2_018y : packoffset(c018.y);
  uint cb2_019x : packoffset(c019.x);
  uint cb2_019y : packoffset(c019.y);
  uint cb2_019z : packoffset(c019.z);
  uint cb2_019w : packoffset(c019.w);
  float cb2_020x : packoffset(c020.x);
  float cb2_020y : packoffset(c020.y);
  float cb2_020z : packoffset(c020.z);
  float cb2_020w : packoffset(c020.w);
  float cb2_021x : packoffset(c021.x);
  float cb2_021y : packoffset(c021.y);
  float cb2_021z : packoffset(c021.z);
  float cb2_021w : packoffset(c021.w);
  float cb2_022x : packoffset(c022.x);
  float cb2_023x : packoffset(c023.x);
  float cb2_023y : packoffset(c023.y);
  float cb2_023z : packoffset(c023.z);
  float cb2_023w : packoffset(c023.w);
  float cb2_024x : packoffset(c024.x);
  float cb2_024y : packoffset(c024.y);
  float cb2_024z : packoffset(c024.z);
  float cb2_024w : packoffset(c024.w);
  float cb2_025x : packoffset(c025.x);
  float cb2_025y : packoffset(c025.y);
  float cb2_025z : packoffset(c025.z);
  float cb2_025w : packoffset(c025.w);
  float cb2_026x : packoffset(c026.x);
  float cb2_026z : packoffset(c026.z);
  float cb2_026w : packoffset(c026.w);
  float cb2_027x : packoffset(c027.x);
  float cb2_027y : packoffset(c027.y);
  float cb2_027z : packoffset(c027.z);
  float cb2_027w : packoffset(c027.w);
  uint cb2_069y : packoffset(c069.y);
  uint cb2_069z : packoffset(c069.z);
  uint cb2_070x : packoffset(c070.x);
  uint cb2_070y : packoffset(c070.y);
  uint cb2_070z : packoffset(c070.z);
  uint cb2_070w : packoffset(c070.w);
  uint cb2_071x : packoffset(c071.x);
  uint cb2_071y : packoffset(c071.y);
  uint cb2_071z : packoffset(c071.z);
  uint cb2_071w : packoffset(c071.w);
  uint cb2_072x : packoffset(c072.x);
  uint cb2_072y : packoffset(c072.y);
  uint cb2_072z : packoffset(c072.z);
  uint cb2_072w : packoffset(c072.w);
  uint cb2_073x : packoffset(c073.x);
  uint cb2_073y : packoffset(c073.y);
  uint cb2_073z : packoffset(c073.z);
  uint cb2_073w : packoffset(c073.w);
  uint cb2_074x : packoffset(c074.x);
  uint cb2_074y : packoffset(c074.y);
  uint cb2_074z : packoffset(c074.z);
  uint cb2_074w : packoffset(c074.w);
  uint cb2_075x : packoffset(c075.x);
  uint cb2_075y : packoffset(c075.y);
  uint cb2_075z : packoffset(c075.z);
  uint cb2_075w : packoffset(c075.w);
  uint cb2_076x : packoffset(c076.x);
  uint cb2_076y : packoffset(c076.y);
  uint cb2_076z : packoffset(c076.z);
  uint cb2_076w : packoffset(c076.w);
  uint cb2_077x : packoffset(c077.x);
  uint cb2_077y : packoffset(c077.y);
  uint cb2_077z : packoffset(c077.z);
  uint cb2_077w : packoffset(c077.w);
  uint cb2_078x : packoffset(c078.x);
  uint cb2_078y : packoffset(c078.y);
  uint cb2_078z : packoffset(c078.z);
  uint cb2_078w : packoffset(c078.w);
  uint cb2_079x : packoffset(c079.x);
  uint cb2_079y : packoffset(c079.y);
  uint cb2_079z : packoffset(c079.z);
  uint cb2_094x : packoffset(c094.x);
  uint cb2_094y : packoffset(c094.y);
  uint cb2_094z : packoffset(c094.z);
  uint cb2_094w : packoffset(c094.w);
  uint cb2_095x : packoffset(c095.x);
  float cb2_095y : packoffset(c095.y);
};

SamplerState s0_space2 : register(s0, space2);

SamplerState s2_space2 : register(s2, space2);

SamplerState s4_space2 : register(s4, space2);

struct OutputSignature {
  float4 SV_Target : SV_Target;
  float4 SV_Target_1 : SV_Target1;
};

OutputSignature main(
  linear float2 TEXCOORD0_centroid : TEXCOORD0_centroid,
  noperspective float4 SV_Position : SV_Position,
  nointerpolation uint SV_IsFrontFace : SV_IsFrontFace
) {
  float4 SV_Target;
  float4 SV_Target_1;
  float _26 = t2.SampleLevel(s4_space2, float2(TEXCOORD0_centroid.x, TEXCOORD0_centroid.y), 0.0f);
  float4 _28 = t8.SampleLevel(s2_space2, float2(TEXCOORD0_centroid.x, TEXCOORD0_centroid.y), 0.0f);
  float _32 = _28.x * 6.283199787139893f;
  float _33 = cos(_32);
  float _34 = sin(_32);
  float _35 = _33 * _28.z;
  float _36 = _34 * _28.z;
  float _37 = _35 + TEXCOORD0_centroid.x;
  float _38 = _36 + TEXCOORD0_centroid.y;
  float _39 = _37 * 10.0f;
  float _40 = 10.0f - _39;
  float _41 = min(_39, _40);
  float _42 = saturate(_41);
  float _43 = _42 * _35;
  float _44 = _38 * 10.0f;
  float _45 = 10.0f - _44;
  float _46 = min(_44, _45);
  float _47 = saturate(_46);
  float _48 = _47 * _36;
  float _49 = _43 + TEXCOORD0_centroid.x;
  float _50 = _48 + TEXCOORD0_centroid.y;
  float4 _51 = t8.SampleLevel(s2_space2, float2(_49, _50), 0.0f);
  float _53 = _51.w * _43;
  float _54 = _51.w * _48;
  float _55 = 1.0f - _28.y;
  float _56 = saturate(_55);
  float _57 = _53 * _56;
  float _58 = _54 * _56;
  float _62 = cb2_015x * TEXCOORD0_centroid.x;
  float _63 = cb2_015y * TEXCOORD0_centroid.y;
  float _66 = _62 + cb2_015z;
  float _67 = _63 + cb2_015w;
  float4 _68 = t9.SampleLevel(s0_space2, float2(_66, _67), 0.0f);
  float _72 = saturate(_68.x);
  float _73 = saturate(_68.z);
  float _76 = cb2_026x * _73;
  float _77 = _72 * 6.283199787139893f;
  float _78 = cos(_77);
  float _79 = sin(_77);
  float _80 = _76 * _78;
  float _81 = _79 * _76;
  float _82 = 1.0f - _68.y;
  float _83 = saturate(_82);
  float _84 = _80 * _83;
  float _85 = _81 * _83;
  float _86 = _57 + TEXCOORD0_centroid.x;
  float _87 = _86 + _84;
  float _88 = _58 + TEXCOORD0_centroid.y;
  float _89 = _88 + _85;
  float4 _90 = t8.SampleLevel(s2_space2, float2(_87, _89), 0.0f);
  bool _92 = (_90.y > 0.0f);
  float _93 = select(_92, TEXCOORD0_centroid.x, _87);
  float _94 = select(_92, TEXCOORD0_centroid.y, _89);
  float4 _95 = t1.SampleLevel(s4_space2, float2(_93, _94), 0.0f);
  float _99 = max(_95.x, 0.0f);
  float _100 = max(_95.y, 0.0f);
  float _101 = max(_95.z, 0.0f);
  float _102 = min(_99, 65000.0f);
  float _103 = min(_100, 65000.0f);
  float _104 = min(_101, 65000.0f);
  float4 _105 = t4.SampleLevel(s2_space2, float2(_93, _94), 0.0f);
  float _110 = max(_105.x, 0.0f);
  float _111 = max(_105.y, 0.0f);
  float _112 = max(_105.z, 0.0f);
  float _113 = max(_105.w, 0.0f);
  float _114 = min(_110, 5000.0f);
  float _115 = min(_111, 5000.0f);
  float _116 = min(_112, 5000.0f);
  float _117 = min(_113, 5000.0f);
  float _120 = _26.x * cb0_028z;
  float _121 = _120 + cb0_028x;
  float _122 = cb2_027w / _121;
  float _123 = 1.0f - _122;
  float _124 = abs(_123);
  float _126 = cb2_027y * _124;
  float _128 = _126 - cb2_027z;
  float _129 = saturate(_128);
  float _130 = max(_129, _117);
  float _131 = saturate(_130);
  float _135 = cb2_006x * _93;
  float _136 = cb2_006y * _94;
  float _139 = _135 + cb2_006z;
  float _140 = _136 + cb2_006w;
  float _144 = cb2_007x * _93;
  float _145 = cb2_007y * _94;
  float _148 = _144 + cb2_007z;
  float _149 = _145 + cb2_007w;
  float _153 = cb2_008x * _93;
  float _154 = cb2_008y * _94;
  float _157 = _153 + cb2_008z;
  float _158 = _154 + cb2_008w;
  float4 _159 = t1.SampleLevel(s2_space2, float2(_139, _140), 0.0f);
  float _161 = max(_159.x, 0.0f);
  float _162 = min(_161, 65000.0f);
  float4 _163 = t1.SampleLevel(s2_space2, float2(_148, _149), 0.0f);
  float _165 = max(_163.y, 0.0f);
  float _166 = min(_165, 65000.0f);
  float4 _167 = t1.SampleLevel(s2_space2, float2(_157, _158), 0.0f);
  float _169 = max(_167.z, 0.0f);
  float _170 = min(_169, 65000.0f);
  float4 _171 = t4.SampleLevel(s2_space2, float2(_139, _140), 0.0f);
  float _173 = max(_171.x, 0.0f);
  float _174 = min(_173, 5000.0f);
  float4 _175 = t4.SampleLevel(s2_space2, float2(_148, _149), 0.0f);
  float _177 = max(_175.y, 0.0f);
  float _178 = min(_177, 5000.0f);
  float4 _179 = t4.SampleLevel(s2_space2, float2(_157, _158), 0.0f);
  float _181 = max(_179.z, 0.0f);
  float _182 = min(_181, 5000.0f);
  float4 _183 = t7.SampleLevel(s2_space2, float2(TEXCOORD0_centroid.x, TEXCOORD0_centroid.y), 0.0f);
  float _189 = cb2_005x * _183.x;
  float _190 = cb2_005x * _183.y;
  float _191 = cb2_005x * _183.z;
  float _192 = _162 - _102;
  float _193 = _166 - _103;
  float _194 = _170 - _104;
  float _195 = _189 * _192;
  float _196 = _190 * _193;
  float _197 = _191 * _194;
  float _198 = _195 + _102;
  float _199 = _196 + _103;
  float _200 = _197 + _104;
  float _201 = _174 - _114;
  float _202 = _178 - _115;
  float _203 = _182 - _116;
  float _204 = _189 * _201;
  float _205 = _190 * _202;
  float _206 = _191 * _203;
  float _207 = _204 + _114;
  float _208 = _205 + _115;
  float _209 = _206 + _116;
  float4 _210 = t5.SampleLevel(s2_space2, float2(TEXCOORD0_centroid.x, TEXCOORD0_centroid.y), 0.0f);
  float _214 = _207 - _198;
  float _215 = _208 - _199;
  float _216 = _209 - _200;
  float _217 = _214 * _131;
  float _218 = _215 * _131;
  float _219 = _216 * _131;
  float _220 = _217 + _198;
  float _221 = _218 + _199;
  float _222 = _219 + _200;
  float _223 = dot(float3(_220, _221, _222), float3(0.2125999927520752f, 0.7152000069618225f, 0.0722000002861023f));
  float _227 = t0[0].SExposureData_020;
  float _229 = t0[0].SExposureData_004;
  float _231 = cb2_018x * 0.5f;
  float _232 = _231 * cb2_018y;
  float _233 = _229.x - _232;
  float _234 = cb2_018y * cb2_018x;
  float _235 = 1.0f / _234;
  float _236 = _233 * _235;
  float _237 = _223 / _227.x;
  float _238 = _237 * 5464.01611328125f;
  float _239 = _238 + 9.99999993922529e-09f;
  float _240 = log2(_239);
  float _241 = _240 - _233;
  float _242 = _241 * _235;
  float _243 = saturate(_242);
  float2 _244 = t10.SampleLevel(s2_space2, float3(TEXCOORD0_centroid.x, TEXCOORD0_centroid.y, _243), 0.0f);
  float _247 = max(_244.y, 1.0000000116860974e-07f);
  float _248 = _244.x / _247;
  float _249 = _248 + _236;
  float _250 = _249 / _235;
  float _251 = _250 - _229.x;
  float _252 = -0.0f - _251;
  float _254 = _252 - cb2_027x;
  float _255 = max(0.0f, _254);
  float _257 = cb2_026z * _255;
  float _258 = _251 - cb2_027x;
  float _259 = max(0.0f, _258);
  float _261 = cb2_026w * _259;
  bool _262 = (_251 < 0.0f);
  float _263 = select(_262, _257, _261);
  float _264 = exp2(_263);
  float _265 = _264 * _220;
  float _266 = _264 * _221;
  float _267 = _264 * _222;
  float _272 = cb2_024y * _210.x;
  float _273 = cb2_024z * _210.y;
  float _274 = cb2_024w * _210.z;
  float _275 = _272 + _265;
  float _276 = _273 + _266;
  float _277 = _274 + _267;
  float _282 = _275 * cb2_025x;
  float _283 = _276 * cb2_025y;
  float _284 = _277 * cb2_025z;
  float _285 = dot(float3(_282, _283, _284), float3(0.2125999927520752f, 0.7152000069618225f, 0.0722000002861023f));
  float _286 = t0[0].SExposureData_012;
  float _288 = _285 * 5464.01611328125f;
  float _289 = _288 * _286.x;
  float _290 = _289 + 9.99999993922529e-09f;
  float _291 = log2(_290);
  float _292 = _291 + 16.929765701293945f;
  float _293 = _292 * 0.05734497308731079f;
  float _294 = saturate(_293);
  float _295 = _294 * _294;
  float _296 = _294 * 2.0f;
  float _297 = 3.0f - _296;
  float _298 = _295 * _297;
  float _299 = _283 * 0.8450999855995178f;
  float _300 = _284 * 0.14589999616146088f;
  float _301 = _299 + _300;
  float _302 = _301 * 2.4890189170837402f;
  float _303 = _301 * 0.3754962384700775f;
  float _304 = _301 * 2.811495304107666f;
  float _305 = _301 * 5.519708156585693f;
  float _306 = _285 - _302;
  float _307 = _298 * _306;
  float _308 = _307 + _302;
  float _309 = _298 * 0.5f;
  float _310 = _309 + 0.5f;
  float _311 = _310 * _306;
  float _312 = _311 + _302;
  float _313 = _282 - _303;
  float _314 = _283 - _304;
  float _315 = _284 - _305;
  float _316 = _310 * _313;
  float _317 = _310 * _314;
  float _318 = _310 * _315;
  float _319 = _316 + _303;
  float _320 = _317 + _304;
  float _321 = _318 + _305;
  float _322 = 1.0f / _312;
  float _323 = _308 * _322;
  float _324 = _323 * _319;
  float _325 = _323 * _320;
  float _326 = _323 * _321;
  float _330 = cb2_020x * TEXCOORD0_centroid.x;
  float _331 = cb2_020y * TEXCOORD0_centroid.y;
  float _334 = _330 + cb2_020z;
  float _335 = _331 + cb2_020w;
  float _338 = dot(float2(_334, _335), float2(_334, _335));
  float _339 = 1.0f - _338;
  float _340 = saturate(_339);
  float _341 = log2(_340);
  float _342 = _341 * cb2_021w;
  float _343 = exp2(_342);
  float _347 = _324 - cb2_021x;
  float _348 = _325 - cb2_021y;
  float _349 = _326 - cb2_021z;
  float _350 = _347 * _343;
  float _351 = _348 * _343;
  float _352 = _349 * _343;
  float _353 = _350 + cb2_021x;
  float _354 = _351 + cb2_021y;
  float _355 = _352 + cb2_021z;
  float _356 = t0[0].SExposureData_000;
  float _358 = max(_227.x, 0.0010000000474974513f);
  float _359 = 1.0f / _358;
  float _360 = _359 * _356.x;
  bool _363 = ((uint)(cb2_069y) == 0);
  float _369;
  float _370;
  float _371;
  float _425;
  float _426;
  float _427;
  float _503;
  float _504;
  float _505;
  float _655;
  float _692;
  float _693;
  float _694;
  float _723;
  float _724;
  float _725;
  float _806;
  float _807;
  float _808;
  float _814;
  float _815;
  float _816;
  float _830;
  float _831;
  float _832;
  float _857;
  float _869;
  float _897;
  float _909;
  float _921;
  float _922;
  float _923;
  float _950;
  float _951;
  float _952;
  if (!_363) {
    float _365 = _360 * _353;
    float _366 = _360 * _354;
    float _367 = _360 * _355;
    _369 = _365;
    _370 = _366;
    _371 = _367;
  } else {
    _369 = _353;
    _370 = _354;
    _371 = _355;
  }
  float _372 = _369 * 0.6130970120429993f;
  float _373 = mad(0.33952298760414124f, _370, _372);
  float _374 = mad(0.04737899824976921f, _371, _373);
  float _375 = _369 * 0.07019399851560593f;
  float _376 = mad(0.9163540005683899f, _370, _375);
  float _377 = mad(0.013451999984681606f, _371, _376);
  float _378 = _369 * 0.02061600051820278f;
  float _379 = mad(0.10956999659538269f, _370, _378);
  float _380 = mad(0.8698149919509888f, _371, _379);
  float _381 = log2(_374);
  float _382 = log2(_377);
  float _383 = log2(_380);
  float _384 = _381 * 0.04211956635117531f;
  float _385 = _382 * 0.04211956635117531f;
  float _386 = _383 * 0.04211956635117531f;
  float _387 = _384 + 0.6252607107162476f;
  float _388 = _385 + 0.6252607107162476f;
  float _389 = _386 + 0.6252607107162476f;
  float4 _390 = t6.SampleLevel(s2_space2, float3(_387, _388, _389), 0.0f);
  bool _396 = ((int)(uint)(cb1_018w) > (int)-1);
  if (_396 && RENODX_TONE_MAP_TYPE == 0.f) {
    float _400 = cb2_017x * _390.x;
    float _401 = cb2_017x * _390.y;
    float _402 = cb2_017x * _390.z;
    float _404 = _400 + cb2_017y;
    float _405 = _401 + cb2_017y;
    float _406 = _402 + cb2_017y;
    float _407 = exp2(_404);
    float _408 = exp2(_405);
    float _409 = exp2(_406);
    float _410 = _407 + 1.0f;
    float _411 = _408 + 1.0f;
    float _412 = _409 + 1.0f;
    float _413 = 1.0f / _410;
    float _414 = 1.0f / _411;
    float _415 = 1.0f / _412;
    float _417 = cb2_017z * _413;
    float _418 = cb2_017z * _414;
    float _419 = cb2_017z * _415;
    float _421 = _417 + cb2_017w;
    float _422 = _418 + cb2_017w;
    float _423 = _419 + cb2_017w;
    _425 = _421;
    _426 = _422;
    _427 = _423;
  } else {
    _425 = _390.x;
    _426 = _390.y;
    _427 = _390.z;
  }
  float _428 = _425 * 23.0f;
  float _429 = _428 + -14.473931312561035f;
  float _430 = exp2(_429);
  float _431 = _426 * 23.0f;
  float _432 = _431 + -14.473931312561035f;
  float _433 = exp2(_432);
  float _434 = _427 * 23.0f;
  float _435 = _434 + -14.473931312561035f;
  float _436 = exp2(_435);
  float _437 = dot(float3(_430, _433, _436), float3(0.2125999927520752f, 0.7152000069618225f, 0.0722000002861023f));
  float _442 = dot(float3(_430, _433, _436), float3(_430, _433, _436));
  float _443 = rsqrt(_442);
  float _444 = _443 * _430;
  float _445 = _443 * _433;
  float _446 = _443 * _436;
  float _447 = cb2_001x - _444;
  float _448 = cb2_001y - _445;
  float _449 = cb2_001z - _446;
  float _450 = dot(float3(_447, _448, _449), float3(_447, _448, _449));
  float _453 = cb2_002z * _450;
  float _455 = _453 + cb2_002w;
  float _456 = saturate(_455);
  float _458 = cb2_002x * _456;
  float _459 = _437 - _430;
  float _460 = _437 - _433;
  float _461 = _437 - _436;
  float _462 = _458 * _459;
  float _463 = _458 * _460;
  float _464 = _458 * _461;
  float _465 = _462 + _430;
  float _466 = _463 + _433;
  float _467 = _464 + _436;
  float _469 = cb2_002y * _456;
  float _470 = 0.10000000149011612f - _465;
  float _471 = 0.10000000149011612f - _466;
  float _472 = 0.10000000149011612f - _467;
  float _473 = _470 * _469;
  float _474 = _471 * _469;
  float _475 = _472 * _469;
  float _476 = _473 + _465;
  float _477 = _474 + _466;
  float _478 = _475 + _467;
  float _479 = saturate(_476);
  float _480 = saturate(_477);
  float _481 = saturate(_478);
  float _488 = cb2_016x - _479;
  float _489 = cb2_016y - _480;
  float _490 = cb2_016z - _481;
  float _491 = _488 * cb2_016w;
  float _492 = _489 * cb2_016w;
  float _493 = _490 * cb2_016w;
  float _494 = _491 + _479;
  float _495 = _492 + _480;
  float _496 = _493 + _481;
  if (_396 && RENODX_TONE_MAP_TYPE == 0.f) {
    float _499 = cb2_024x * _494;
    float _500 = cb2_024x * _495;
    float _501 = cb2_024x * _496;
    _503 = _499;
    _504 = _500;
    _505 = _501;
  } else {
    _503 = _494;
    _504 = _495;
    _505 = _496;
  }
  float _508 = _503 * 0.9708889722824097f;
  float _509 = mad(0.026962999254465103f, _504, _508);
  float _510 = mad(0.002148000057786703f, _505, _509);
  float _511 = _503 * 0.01088900025933981f;
  float _512 = mad(0.9869629740715027f, _504, _511);
  float _513 = mad(0.002148000057786703f, _505, _512);
  float _514 = mad(0.026962999254465103f, _504, _511);
  float _515 = mad(0.9621480107307434f, _505, _514);
  float _516 = max(_510, 0.0f);
  float _517 = max(_513, 0.0f);
  float _518 = max(_515, 0.0f);
  float _519 = min(_516, cb2_095y);
  float _520 = min(_517, cb2_095y);
  float _521 = min(_518, cb2_095y);
  bool _524 = ((uint)(cb2_095x) == 0);
  bool _527 = ((uint)(cb2_094w) == 0);
  bool _529 = ((uint)(cb2_094z) == 0);
  bool _531 = ((uint)(cb2_094y) != 0);
  bool _533 = ((uint)(cb2_094x) == 0);
  bool _535 = ((uint)(cb2_069z) != 0);
  float _582 = asfloat((uint)(cb2_075y));
  float _583 = asfloat((uint)(cb2_075z));
  float _584 = asfloat((uint)(cb2_075w));
  float _585 = asfloat((uint)(cb2_074z));
  float _586 = asfloat((uint)(cb2_074w));
  float _587 = asfloat((uint)(cb2_075x));
  float _588 = asfloat((uint)(cb2_073w));
  float _589 = asfloat((uint)(cb2_074x));
  float _590 = asfloat((uint)(cb2_074y));
  float _591 = asfloat((uint)(cb2_077x));
  float _592 = asfloat((uint)(cb2_077y));
  float _593 = asfloat((uint)(cb2_079x));
  float _594 = asfloat((uint)(cb2_079y));
  float _595 = asfloat((uint)(cb2_079z));
  float _596 = asfloat((uint)(cb2_078y));
  float _597 = asfloat((uint)(cb2_078z));
  float _598 = asfloat((uint)(cb2_078w));
  float _599 = asfloat((uint)(cb2_077z));
  float _600 = asfloat((uint)(cb2_077w));
  float _601 = asfloat((uint)(cb2_078x));
  float _602 = asfloat((uint)(cb2_072y));
  float _603 = asfloat((uint)(cb2_072z));
  float _604 = asfloat((uint)(cb2_072w));
  float _605 = asfloat((uint)(cb2_071x));
  float _606 = asfloat((uint)(cb2_071y));
  float _607 = asfloat((uint)(cb2_076x));
  float _608 = asfloat((uint)(cb2_070w));
  float _609 = asfloat((uint)(cb2_070x));
  float _610 = asfloat((uint)(cb2_070y));
  float _611 = asfloat((uint)(cb2_070z));
  float _612 = asfloat((uint)(cb2_073x));
  float _613 = asfloat((uint)(cb2_073y));
  float _614 = asfloat((uint)(cb2_073z));
  float _615 = asfloat((uint)(cb2_071z));
  float _616 = asfloat((uint)(cb2_071w));
  float _617 = asfloat((uint)(cb2_072x));
  float _618 = max(_520, _521);
  float _619 = max(_519, _618);
  float _620 = 1.0f / _619;
  float _621 = _620 * _519;
  float _622 = _620 * _520;
  float _623 = _620 * _521;
  float _624 = abs(_621);
  float _625 = log2(_624);
  float _626 = _625 * _609;
  float _627 = exp2(_626);
  float _628 = abs(_622);
  float _629 = log2(_628);
  float _630 = _629 * _610;
  float _631 = exp2(_630);
  float _632 = abs(_623);
  float _633 = log2(_632);
  float _634 = _633 * _611;
  float _635 = exp2(_634);
  if (_531) {
    float _638 = asfloat((uint)(cb2_076w));
    float _640 = asfloat((uint)(cb2_076z));
    float _642 = asfloat((uint)(cb2_076y));
    float _643 = _640 * _520;
    float _644 = _642 * _519;
    float _645 = _638 * _521;
    float _646 = _644 + _645;
    float _647 = _646 + _643;
    _655 = _647;
  } else {
    float _649 = _616 * _520;
    float _650 = _615 * _519;
    float _651 = _617 * _521;
    float _652 = _649 + _650;
    float _653 = _652 + _651;
    _655 = _653;
  }
  float _656 = abs(_655);
  float _657 = log2(_656);
  float _658 = _657 * _608;
  float _659 = exp2(_658);
  float _660 = log2(_659);
  float _661 = _660 * _607;
  float _662 = exp2(_661);
  float _663 = select(_535, _662, _659);
  float _664 = _663 * _605;
  float _665 = _664 + _606;
  float _666 = 1.0f / _665;
  float _667 = _666 * _659;
  if (_531) {
    if (!_533) {
      float _670 = _627 * _599;
      float _671 = _631 * _600;
      float _672 = _635 * _601;
      float _673 = _671 + _670;
      float _674 = _673 + _672;
      float _675 = _631 * _597;
      float _676 = _627 * _596;
      float _677 = _635 * _598;
      float _678 = _675 + _676;
      float _679 = _678 + _677;
      float _680 = _635 * _595;
      float _681 = _631 * _594;
      float _682 = _627 * _593;
      float _683 = _681 + _682;
      float _684 = _683 + _680;
      float _685 = max(_679, _684);
      float _686 = max(_674, _685);
      float _687 = 1.0f / _686;
      float _688 = _687 * _674;
      float _689 = _687 * _679;
      float _690 = _687 * _684;
      _692 = _688;
      _693 = _689;
      _694 = _690;
    } else {
      _692 = _627;
      _693 = _631;
      _694 = _635;
    }
    float _695 = _692 * _592;
    float _696 = exp2(_695);
    float _697 = _696 * _591;
    float _698 = saturate(_697);
    float _699 = _692 * _591;
    float _700 = _692 - _699;
    float _701 = saturate(_700);
    float _702 = max(_591, _701);
    float _703 = min(_702, _698);
    float _704 = _693 * _592;
    float _705 = exp2(_704);
    float _706 = _705 * _591;
    float _707 = saturate(_706);
    float _708 = _693 * _591;
    float _709 = _693 - _708;
    float _710 = saturate(_709);
    float _711 = max(_591, _710);
    float _712 = min(_711, _707);
    float _713 = _694 * _592;
    float _714 = exp2(_713);
    float _715 = _714 * _591;
    float _716 = saturate(_715);
    float _717 = _694 * _591;
    float _718 = _694 - _717;
    float _719 = saturate(_718);
    float _720 = max(_591, _719);
    float _721 = min(_720, _716);
    _723 = _703;
    _724 = _712;
    _725 = _721;
  } else {
    _723 = _627;
    _724 = _631;
    _725 = _635;
  }
  float _726 = _723 * _615;
  float _727 = _724 * _616;
  float _728 = _727 + _726;
  float _729 = _725 * _617;
  float _730 = _728 + _729;
  float _731 = 1.0f / _730;
  float _732 = _731 * _667;
  float _733 = saturate(_732);
  float _734 = _733 * _723;
  float _735 = saturate(_734);
  float _736 = _733 * _724;
  float _737 = saturate(_736);
  float _738 = _733 * _725;
  float _739 = saturate(_738);
  float _740 = _735 * _602;
  float _741 = _602 - _740;
  float _742 = _737 * _603;
  float _743 = _603 - _742;
  float _744 = _739 * _604;
  float _745 = _604 - _744;
  float _746 = _739 * _617;
  float _747 = _735 * _615;
  float _748 = _737 * _616;
  float _749 = _667 - _747;
  float _750 = _749 - _748;
  float _751 = _750 - _746;
  float _752 = saturate(_751);
  float _753 = _743 * _616;
  float _754 = _741 * _615;
  float _755 = _745 * _617;
  float _756 = _753 + _754;
  float _757 = _756 + _755;
  float _758 = 1.0f / _757;
  float _759 = _758 * _752;
  float _760 = _759 * _741;
  float _761 = _760 + _735;
  float _762 = saturate(_761);
  float _763 = _759 * _743;
  float _764 = _763 + _737;
  float _765 = saturate(_764);
  float _766 = _759 * _745;
  float _767 = _766 + _739;
  float _768 = saturate(_767);
  float _769 = _768 * _617;
  float _770 = _762 * _615;
  float _771 = _765 * _616;
  float _772 = _667 - _770;
  float _773 = _772 - _771;
  float _774 = _773 - _769;
  float _775 = saturate(_774);
  float _776 = _775 * _612;
  float _777 = _776 + _762;
  float _778 = saturate(_777);
  float _779 = _775 * _613;
  float _780 = _779 + _765;
  float _781 = saturate(_780);
  float _782 = _775 * _614;
  float _783 = _782 + _768;
  float _784 = saturate(_783);
  if (!_529) {
    float _786 = _778 * _588;
    float _787 = _781 * _589;
    float _788 = _784 * _590;
    float _789 = _787 + _786;
    float _790 = _789 + _788;
    float _791 = _781 * _586;
    float _792 = _778 * _585;
    float _793 = _784 * _587;
    float _794 = _791 + _792;
    float _795 = _794 + _793;
    float _796 = _784 * _584;
    float _797 = _781 * _583;
    float _798 = _778 * _582;
    float _799 = _797 + _798;
    float _800 = _799 + _796;
    if (!_527) {
      float _802 = saturate(_790);
      float _803 = saturate(_795);
      float _804 = saturate(_800);
      _806 = _804;
      _807 = _803;
      _808 = _802;
    } else {
      _806 = _800;
      _807 = _795;
      _808 = _790;
    }
  } else {
    _806 = _784;
    _807 = _781;
    _808 = _778;
  }
  if (!_524) {
    float _810 = _808 * _588;
    float _811 = _807 * _588;
    float _812 = _806 * _588;
    _814 = _812;
    _815 = _811;
    _816 = _810;
  } else {
    _814 = _806;
    _815 = _807;
    _816 = _808;
  }
  if (_396) {
    float _820 = cb1_018z * 9.999999747378752e-05f;
    float _821 = _820 * _816;
    float _822 = _820 * _815;
    float _823 = _820 * _814;
    float _825 = 5000.0f / cb1_018y;
    float _826 = _821 * _825;
    float _827 = _822 * _825;
    float _828 = _823 * _825;
    _830 = _826;
    _831 = _827;
    _832 = _828;
  } else {
    _830 = _816;
    _831 = _815;
    _832 = _814;
  }
  float _833 = _830 * 1.6047500371932983f;
  float _834 = mad(-0.5310800075531006f, _831, _833);
  float _835 = mad(-0.07366999983787537f, _832, _834);
  float _836 = _830 * -0.10208000242710114f;
  float _837 = mad(1.1081299781799316f, _831, _836);
  float _838 = mad(-0.006049999967217445f, _832, _837);
  float _839 = _830 * -0.0032599999103695154f;
  float _840 = mad(-0.07275000214576721f, _831, _839);
  float _841 = mad(1.0760200023651123f, _832, _840);
  if (_396) {
    // float _843 = max(_835, 0.0f);
    // float _844 = max(_838, 0.0f);
    // float _845 = max(_841, 0.0f);
    // bool _846 = !(_843 >= 0.0030399328097701073f);
    // if (!_846) {
    //   float _848 = abs(_843);
    //   float _849 = log2(_848);
    //   float _850 = _849 * 0.4166666567325592f;
    //   float _851 = exp2(_850);
    //   float _852 = _851 * 1.0549999475479126f;
    //   float _853 = _852 + -0.054999999701976776f;
    //   _857 = _853;
    // } else {
    //   float _855 = _843 * 12.923210144042969f;
    //   _857 = _855;
    // }
    // bool _858 = !(_844 >= 0.0030399328097701073f);
    // if (!_858) {
    //   float _860 = abs(_844);
    //   float _861 = log2(_860);
    //   float _862 = _861 * 0.4166666567325592f;
    //   float _863 = exp2(_862);
    //   float _864 = _863 * 1.0549999475479126f;
    //   float _865 = _864 + -0.054999999701976776f;
    //   _869 = _865;
    // } else {
    //   float _867 = _844 * 12.923210144042969f;
    //   _869 = _867;
    // }
    // bool _870 = !(_845 >= 0.0030399328097701073f);
    // if (!_870) {
    //   float _872 = abs(_845);
    //   float _873 = log2(_872);
    //   float _874 = _873 * 0.4166666567325592f;
    //   float _875 = exp2(_874);
    //   float _876 = _875 * 1.0549999475479126f;
    //   float _877 = _876 + -0.054999999701976776f;
    //   _950 = _857;
    //   _951 = _869;
    //   _952 = _877;
    // } else {
    //   float _879 = _845 * 12.923210144042969f;
    //   _950 = _857;
    //   _951 = _869;
    //   _952 = _879;
    // }
    _950 = renodx::color::srgb::EncodeSafe(_835);
    _951 = renodx::color::srgb::EncodeSafe(_838);
    _952 = renodx::color::srgb::EncodeSafe(_841);

  } else {
    float _881 = saturate(_835);
    float _882 = saturate(_838);
    float _883 = saturate(_841);
    bool _884 = ((uint)(cb1_018w) == -2);
    if (!_884) {
      bool _886 = !(_881 >= 0.0030399328097701073f);
      if (!_886) {
        float _888 = abs(_881);
        float _889 = log2(_888);
        float _890 = _889 * 0.4166666567325592f;
        float _891 = exp2(_890);
        float _892 = _891 * 1.0549999475479126f;
        float _893 = _892 + -0.054999999701976776f;
        _897 = _893;
      } else {
        float _895 = _881 * 12.923210144042969f;
        _897 = _895;
      }
      bool _898 = !(_882 >= 0.0030399328097701073f);
      if (!_898) {
        float _900 = abs(_882);
        float _901 = log2(_900);
        float _902 = _901 * 0.4166666567325592f;
        float _903 = exp2(_902);
        float _904 = _903 * 1.0549999475479126f;
        float _905 = _904 + -0.054999999701976776f;
        _909 = _905;
      } else {
        float _907 = _882 * 12.923210144042969f;
        _909 = _907;
      }
      bool _910 = !(_883 >= 0.0030399328097701073f);
      if (!_910) {
        float _912 = abs(_883);
        float _913 = log2(_912);
        float _914 = _913 * 0.4166666567325592f;
        float _915 = exp2(_914);
        float _916 = _915 * 1.0549999475479126f;
        float _917 = _916 + -0.054999999701976776f;
        _921 = _897;
        _922 = _909;
        _923 = _917;
      } else {
        float _919 = _883 * 12.923210144042969f;
        _921 = _897;
        _922 = _909;
        _923 = _919;
      }
    } else {
      _921 = _881;
      _922 = _882;
      _923 = _883;
    }
    float _928 = abs(_921);
    float _929 = abs(_922);
    float _930 = abs(_923);
    float _931 = log2(_928);
    float _932 = log2(_929);
    float _933 = log2(_930);
    float _934 = _931 * cb2_000z;
    float _935 = _932 * cb2_000z;
    float _936 = _933 * cb2_000z;
    float _937 = exp2(_934);
    float _938 = exp2(_935);
    float _939 = exp2(_936);
    float _940 = _937 * cb2_000y;
    float _941 = _938 * cb2_000y;
    float _942 = _939 * cb2_000y;
    float _943 = _940 + cb2_000x;
    float _944 = _941 + cb2_000x;
    float _945 = _942 + cb2_000x;
    float _946 = saturate(_943);
    float _947 = saturate(_944);
    float _948 = saturate(_945);
    _950 = _946;
    _951 = _947;
    _952 = _948;
  }
  float _956 = cb2_023x * TEXCOORD0_centroid.x;
  float _957 = cb2_023y * TEXCOORD0_centroid.y;
  float _960 = _956 + cb2_023z;
  float _961 = _957 + cb2_023w;
  float4 _964 = t11.SampleLevel(s0_space2, float2(_960, _961), 0.0f);
  float _966 = _964.x + -0.5f;
  float _967 = _966 * cb2_022x;
  float _968 = _967 + 0.5f;
  float _969 = _968 * 2.0f;
  float _970 = _969 * _950;
  float _971 = _969 * _951;
  float _972 = _969 * _952;
  float _976 = float((uint)(cb2_019z));
  float _977 = float((uint)(cb2_019w));
  float _978 = _976 + SV_Position.x;
  float _979 = _977 + SV_Position.y;
  uint _980 = uint(_978);
  uint _981 = uint(_979);
  uint _984 = cb2_019x + -1u;
  uint _985 = cb2_019y + -1u;
  int _986 = _980 & _984;
  int _987 = _981 & _985;
  float4 _988 = t3.Load(int3(_986, _987, 0));
  float _992 = _988.x * 2.0f;
  float _993 = _988.y * 2.0f;
  float _994 = _988.z * 2.0f;
  float _995 = _992 + -1.0f;
  float _996 = _993 + -1.0f;
  float _997 = _994 + -1.0f;
  float _998 = _995 * cb2_025w;
  float _999 = _996 * cb2_025w;
  float _1000 = _997 * cb2_025w;
  float _1001 = _998 + _970;
  float _1002 = _999 + _971;
  float _1003 = _1000 + _972;
  float _1004 = dot(float3(_1001, _1002, _1003), float3(0.2125999927520752f, 0.7152000069618225f, 0.0722000002861023f));
  SV_Target.x = _1001;
  SV_Target.y = _1002;
  SV_Target.z = _1003;
  SV_Target.w = _1004;
  SV_Target_1.x = _1004;
  SV_Target_1.y = 0.0f;
  SV_Target_1.z = 0.0f;
  SV_Target_1.w = 0.0f;
  OutputSignature output_signature = { SV_Target, SV_Target_1 };
  return output_signature;
}
