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

Texture3D<float2> t8 : register(t8);

Texture2D<float4> t9 : register(t9);

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
  float cb2_009x : packoffset(c009.x);
  float cb2_009y : packoffset(c009.y);
  float cb2_009z : packoffset(c009.z);
  float cb2_010x : packoffset(c010.x);
  float cb2_010y : packoffset(c010.y);
  float cb2_010z : packoffset(c010.z);
  float cb2_011x : packoffset(c011.x);
  float cb2_011y : packoffset(c011.y);
  float cb2_011z : packoffset(c011.z);
  float cb2_011w : packoffset(c011.w);
  float cb2_012x : packoffset(c012.x);
  float cb2_012y : packoffset(c012.y);
  float cb2_012z : packoffset(c012.z);
  float cb2_012w : packoffset(c012.w);
  float cb2_013x : packoffset(c013.x);
  float cb2_013y : packoffset(c013.y);
  float cb2_013z : packoffset(c013.z);
  float cb2_013w : packoffset(c013.w);
  float cb2_014x : packoffset(c014.x);
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
  float _24 = t2.SampleLevel(s4_space2, float2(TEXCOORD0_centroid.x, TEXCOORD0_centroid.y), 0.0f);
  float _29 = cb2_015x * TEXCOORD0_centroid.x;
  float _30 = cb2_015y * TEXCOORD0_centroid.y;
  float _33 = _29 + cb2_015z;
  float _34 = _30 + cb2_015w;
  float4 _35 = t7.SampleLevel(s0_space2, float2(_33, _34), 0.0f);
  float _39 = saturate(_35.x);
  float _40 = saturate(_35.z);
  float _43 = cb2_026x * _40;
  float _44 = _39 * 6.283199787139893f;
  float _45 = cos(_44);
  float _46 = sin(_44);
  float _47 = _43 * _45;
  float _48 = _46 * _43;
  float _49 = 1.0f - _35.y;
  float _50 = saturate(_49);
  float _51 = _47 * _50;
  float _52 = _48 * _50;
  float _53 = _51 + TEXCOORD0_centroid.x;
  float _54 = _52 + TEXCOORD0_centroid.y;
  float4 _55 = t1.SampleLevel(s4_space2, float2(_53, _54), 0.0f);
  float _59 = max(_55.x, 0.0f);
  float _60 = max(_55.y, 0.0f);
  float _61 = max(_55.z, 0.0f);
  float _62 = min(_59, 65000.0f);
  float _63 = min(_60, 65000.0f);
  float _64 = min(_61, 65000.0f);
  float4 _65 = t4.SampleLevel(s2_space2, float2(_53, _54), 0.0f);
  float _70 = max(_65.x, 0.0f);
  float _71 = max(_65.y, 0.0f);
  float _72 = max(_65.z, 0.0f);
  float _73 = max(_65.w, 0.0f);
  float _74 = min(_70, 5000.0f);
  float _75 = min(_71, 5000.0f);
  float _76 = min(_72, 5000.0f);
  float _77 = min(_73, 5000.0f);
  float _80 = _24.x * cb0_028z;
  float _81 = _80 + cb0_028x;
  float _82 = cb2_027w / _81;
  float _83 = 1.0f - _82;
  float _84 = abs(_83);
  float _86 = cb2_027y * _84;
  float _88 = _86 - cb2_027z;
  float _89 = saturate(_88);
  float _90 = max(_89, _77);
  float _91 = saturate(_90);
  float _95 = cb2_013x * _53;
  float _96 = cb2_013y * _54;
  float _99 = _95 + cb2_013z;
  float _100 = _96 + cb2_013w;
  float _103 = dot(float2(_99, _100), float2(_99, _100));
  float _104 = abs(_103);
  float _105 = log2(_104);
  float _106 = _105 * cb2_014x;
  float _107 = exp2(_106);
  float _108 = saturate(_107);
  float _112 = cb2_011x * _53;
  float _113 = cb2_011y * _54;
  float _116 = _112 + cb2_011z;
  float _117 = _113 + cb2_011w;
  float _118 = _116 * _108;
  float _119 = _117 * _108;
  float _120 = _118 + _53;
  float _121 = _119 + _54;
  float _125 = cb2_012x * _53;
  float _126 = cb2_012y * _54;
  float _129 = _125 + cb2_012z;
  float _130 = _126 + cb2_012w;
  float _131 = _129 * _108;
  float _132 = _130 * _108;
  float _133 = _131 + _53;
  float _134 = _132 + _54;
  float4 _135 = t1.SampleLevel(s2_space2, float2(_120, _121), 0.0f);
  float _139 = max(_135.x, 0.0f);
  float _140 = max(_135.y, 0.0f);
  float _141 = max(_135.z, 0.0f);
  float _142 = min(_139, 65000.0f);
  float _143 = min(_140, 65000.0f);
  float _144 = min(_141, 65000.0f);
  float4 _145 = t1.SampleLevel(s2_space2, float2(_133, _134), 0.0f);
  float _149 = max(_145.x, 0.0f);
  float _150 = max(_145.y, 0.0f);
  float _151 = max(_145.z, 0.0f);
  float _152 = min(_149, 65000.0f);
  float _153 = min(_150, 65000.0f);
  float _154 = min(_151, 65000.0f);
  float4 _155 = t4.SampleLevel(s2_space2, float2(_120, _121), 0.0f);
  float _159 = max(_155.x, 0.0f);
  float _160 = max(_155.y, 0.0f);
  float _161 = max(_155.z, 0.0f);
  float _162 = min(_159, 5000.0f);
  float _163 = min(_160, 5000.0f);
  float _164 = min(_161, 5000.0f);
  float4 _165 = t4.SampleLevel(s2_space2, float2(_133, _134), 0.0f);
  float _169 = max(_165.x, 0.0f);
  float _170 = max(_165.y, 0.0f);
  float _171 = max(_165.z, 0.0f);
  float _172 = min(_169, 5000.0f);
  float _173 = min(_170, 5000.0f);
  float _174 = min(_171, 5000.0f);
  float _179 = 1.0f - cb2_009x;
  float _180 = 1.0f - cb2_009y;
  float _181 = 1.0f - cb2_009z;
  float _186 = _179 - cb2_010x;
  float _187 = _180 - cb2_010y;
  float _188 = _181 - cb2_010z;
  float _189 = saturate(_186);
  float _190 = saturate(_187);
  float _191 = saturate(_188);
  float _192 = _189 * _62;
  float _193 = _190 * _63;
  float _194 = _191 * _64;
  float _195 = cb2_009x * _142;
  float _196 = cb2_009y * _143;
  float _197 = cb2_009z * _144;
  float _198 = _195 + _192;
  float _199 = _196 + _193;
  float _200 = _197 + _194;
  float _201 = cb2_010x * _152;
  float _202 = cb2_010y * _153;
  float _203 = cb2_010z * _154;
  float _204 = _198 + _201;
  float _205 = _199 + _202;
  float _206 = _200 + _203;
  float _207 = _189 * _74;
  float _208 = _190 * _75;
  float _209 = _191 * _76;
  float _210 = cb2_009x * _162;
  float _211 = cb2_009y * _163;
  float _212 = cb2_009z * _164;
  float _213 = cb2_010x * _172;
  float _214 = cb2_010y * _173;
  float _215 = cb2_010z * _174;
  float4 _216 = t5.SampleLevel(s2_space2, float2(TEXCOORD0_centroid.x, TEXCOORD0_centroid.y), 0.0f);
  float _220 = _207 - _204;
  float _221 = _220 + _210;
  float _222 = _221 + _213;
  float _223 = _208 - _205;
  float _224 = _223 + _211;
  float _225 = _224 + _214;
  float _226 = _209 - _206;
  float _227 = _226 + _212;
  float _228 = _227 + _215;
  float _229 = _222 * _91;
  float _230 = _225 * _91;
  float _231 = _228 * _91;
  float _232 = _229 + _204;
  float _233 = _230 + _205;
  float _234 = _231 + _206;
  float _235 = dot(float3(_232, _233, _234), float3(0.2125999927520752f, 0.7152000069618225f, 0.0722000002861023f));
  float _239 = t0[0].SExposureData_020;
  float _241 = t0[0].SExposureData_004;
  float _243 = cb2_018x * 0.5f;
  float _244 = _243 * cb2_018y;
  float _245 = _241.x - _244;
  float _246 = cb2_018y * cb2_018x;
  float _247 = 1.0f / _246;
  float _248 = _245 * _247;
  float _249 = _235 / _239.x;
  float _250 = _249 * 5464.01611328125f;
  float _251 = _250 + 9.99999993922529e-09f;
  float _252 = log2(_251);
  float _253 = _252 - _245;
  float _254 = _253 * _247;
  float _255 = saturate(_254);
  float2 _256 = t8.SampleLevel(s2_space2, float3(TEXCOORD0_centroid.x, TEXCOORD0_centroid.y, _255), 0.0f);
  float _259 = max(_256.y, 1.0000000116860974e-07f);
  float _260 = _256.x / _259;
  float _261 = _260 + _248;
  float _262 = _261 / _247;
  float _263 = _262 - _241.x;
  float _264 = -0.0f - _263;
  float _266 = _264 - cb2_027x;
  float _267 = max(0.0f, _266);
  float _269 = cb2_026z * _267;
  float _270 = _263 - cb2_027x;
  float _271 = max(0.0f, _270);
  float _273 = cb2_026w * _271;
  bool _274 = (_263 < 0.0f);
  float _275 = select(_274, _269, _273);
  float _276 = exp2(_275);
  float _277 = _276 * _232;
  float _278 = _276 * _233;
  float _279 = _276 * _234;
  float _284 = cb2_024y * _216.x;
  float _285 = cb2_024z * _216.y;
  float _286 = cb2_024w * _216.z;
  float _287 = _284 + _277;
  float _288 = _285 + _278;
  float _289 = _286 + _279;
  float _294 = _287 * cb2_025x;
  float _295 = _288 * cb2_025y;
  float _296 = _289 * cb2_025z;
  float _297 = dot(float3(_294, _295, _296), float3(0.2125999927520752f, 0.7152000069618225f, 0.0722000002861023f));
  float _298 = t0[0].SExposureData_012;
  float _300 = _297 * 5464.01611328125f;
  float _301 = _300 * _298.x;
  float _302 = _301 + 9.99999993922529e-09f;
  float _303 = log2(_302);
  float _304 = _303 + 16.929765701293945f;
  float _305 = _304 * 0.05734497308731079f;
  float _306 = saturate(_305);
  float _307 = _306 * _306;
  float _308 = _306 * 2.0f;
  float _309 = 3.0f - _308;
  float _310 = _307 * _309;
  float _311 = _295 * 0.8450999855995178f;
  float _312 = _296 * 0.14589999616146088f;
  float _313 = _311 + _312;
  float _314 = _313 * 2.4890189170837402f;
  float _315 = _313 * 0.3754962384700775f;
  float _316 = _313 * 2.811495304107666f;
  float _317 = _313 * 5.519708156585693f;
  float _318 = _297 - _314;
  float _319 = _310 * _318;
  float _320 = _319 + _314;
  float _321 = _310 * 0.5f;
  float _322 = _321 + 0.5f;
  float _323 = _322 * _318;
  float _324 = _323 + _314;
  float _325 = _294 - _315;
  float _326 = _295 - _316;
  float _327 = _296 - _317;
  float _328 = _322 * _325;
  float _329 = _322 * _326;
  float _330 = _322 * _327;
  float _331 = _328 + _315;
  float _332 = _329 + _316;
  float _333 = _330 + _317;
  float _334 = 1.0f / _324;
  float _335 = _320 * _334;
  float _336 = _335 * _331;
  float _337 = _335 * _332;
  float _338 = _335 * _333;
  float _342 = cb2_020x * TEXCOORD0_centroid.x;
  float _343 = cb2_020y * TEXCOORD0_centroid.y;
  float _346 = _342 + cb2_020z;
  float _347 = _343 + cb2_020w;
  float _350 = dot(float2(_346, _347), float2(_346, _347));
  float _351 = 1.0f - _350;
  float _352 = saturate(_351);
  float _353 = log2(_352);
  float _354 = _353 * cb2_021w;
  float _355 = exp2(_354);
  float _359 = _336 - cb2_021x;
  float _360 = _337 - cb2_021y;
  float _361 = _338 - cb2_021z;
  float _362 = _359 * _355;
  float _363 = _360 * _355;
  float _364 = _361 * _355;
  float _365 = _362 + cb2_021x;
  float _366 = _363 + cb2_021y;
  float _367 = _364 + cb2_021z;
  float _368 = t0[0].SExposureData_000;
  float _370 = max(_239.x, 0.0010000000474974513f);
  float _371 = 1.0f / _370;
  float _372 = _371 * _368.x;
  bool _375 = ((uint)(cb2_069y) == 0);
  float _381;
  float _382;
  float _383;
  float _437;
  float _438;
  float _439;
  float _470;
  float _471;
  float _472;
  float _622;
  float _659;
  float _660;
  float _661;
  float _690;
  float _691;
  float _692;
  float _773;
  float _774;
  float _775;
  float _781;
  float _782;
  float _783;
  float _797;
  float _798;
  float _799;
  float _824;
  float _836;
  float _864;
  float _876;
  float _888;
  float _889;
  float _890;
  float _917;
  float _918;
  float _919;
  if (!_375) {
    float _377 = _372 * _365;
    float _378 = _372 * _366;
    float _379 = _372 * _367;
    _381 = _377;
    _382 = _378;
    _383 = _379;
  } else {
    _381 = _365;
    _382 = _366;
    _383 = _367;
  }
  float _384 = _381 * 0.6130970120429993f;
  float _385 = mad(0.33952298760414124f, _382, _384);
  float _386 = mad(0.04737899824976921f, _383, _385);
  float _387 = _381 * 0.07019399851560593f;
  float _388 = mad(0.9163540005683899f, _382, _387);
  float _389 = mad(0.013451999984681606f, _383, _388);
  float _390 = _381 * 0.02061600051820278f;
  float _391 = mad(0.10956999659538269f, _382, _390);
  float _392 = mad(0.8698149919509888f, _383, _391);
  float _393 = log2(_386);
  float _394 = log2(_389);
  float _395 = log2(_392);
  float _396 = _393 * 0.04211956635117531f;
  float _397 = _394 * 0.04211956635117531f;
  float _398 = _395 * 0.04211956635117531f;
  float _399 = _396 + 0.6252607107162476f;
  float _400 = _397 + 0.6252607107162476f;
  float _401 = _398 + 0.6252607107162476f;
  float4 _402 = t6.SampleLevel(s2_space2, float3(_399, _400, _401), 0.0f);
  bool _408 = ((int)(uint)(cb1_018w) > (int)-1);
  if (_408 && RENODX_TONE_MAP_TYPE == 0.f) {
    float _412 = cb2_017x * _402.x;
    float _413 = cb2_017x * _402.y;
    float _414 = cb2_017x * _402.z;
    float _416 = _412 + cb2_017y;
    float _417 = _413 + cb2_017y;
    float _418 = _414 + cb2_017y;
    float _419 = exp2(_416);
    float _420 = exp2(_417);
    float _421 = exp2(_418);
    float _422 = _419 + 1.0f;
    float _423 = _420 + 1.0f;
    float _424 = _421 + 1.0f;
    float _425 = 1.0f / _422;
    float _426 = 1.0f / _423;
    float _427 = 1.0f / _424;
    float _429 = cb2_017z * _425;
    float _430 = cb2_017z * _426;
    float _431 = cb2_017z * _427;
    float _433 = _429 + cb2_017w;
    float _434 = _430 + cb2_017w;
    float _435 = _431 + cb2_017w;
    _437 = _433;
    _438 = _434;
    _439 = _435;
  } else {
    _437 = _402.x;
    _438 = _402.y;
    _439 = _402.z;
  }
  float _440 = _437 * 23.0f;
  float _441 = _440 + -14.473931312561035f;
  float _442 = exp2(_441);
  float _443 = _438 * 23.0f;
  float _444 = _443 + -14.473931312561035f;
  float _445 = exp2(_444);
  float _446 = _439 * 23.0f;
  float _447 = _446 + -14.473931312561035f;
  float _448 = exp2(_447);
  float _455 = cb2_016x - _442;
  float _456 = cb2_016y - _445;
  float _457 = cb2_016z - _448;
  float _458 = _455 * cb2_016w;
  float _459 = _456 * cb2_016w;
  float _460 = _457 * cb2_016w;
  float _461 = _458 + _442;
  float _462 = _459 + _445;
  float _463 = _460 + _448;
  if (_408 && RENODX_TONE_MAP_TYPE == 0.f) {
    float _466 = cb2_024x * _461;
    float _467 = cb2_024x * _462;
    float _468 = cb2_024x * _463;
    _470 = _466;
    _471 = _467;
    _472 = _468;
  } else {
    _470 = _461;
    _471 = _462;
    _472 = _463;
  }
  float _475 = _470 * 0.9708889722824097f;
  float _476 = mad(0.026962999254465103f, _471, _475);
  float _477 = mad(0.002148000057786703f, _472, _476);
  float _478 = _470 * 0.01088900025933981f;
  float _479 = mad(0.9869629740715027f, _471, _478);
  float _480 = mad(0.002148000057786703f, _472, _479);
  float _481 = mad(0.026962999254465103f, _471, _478);
  float _482 = mad(0.9621480107307434f, _472, _481);
  float _483 = max(_477, 0.0f);
  float _484 = max(_480, 0.0f);
  float _485 = max(_482, 0.0f);
  float _486 = min(_483, cb2_095y);
  float _487 = min(_484, cb2_095y);
  float _488 = min(_485, cb2_095y);
  bool _491 = ((uint)(cb2_095x) == 0);
  bool _494 = ((uint)(cb2_094w) == 0);
  bool _496 = ((uint)(cb2_094z) == 0);
  bool _498 = ((uint)(cb2_094y) != 0);
  bool _500 = ((uint)(cb2_094x) == 0);
  bool _502 = ((uint)(cb2_069z) != 0);
  float _549 = asfloat((uint)(cb2_075y));
  float _550 = asfloat((uint)(cb2_075z));
  float _551 = asfloat((uint)(cb2_075w));
  float _552 = asfloat((uint)(cb2_074z));
  float _553 = asfloat((uint)(cb2_074w));
  float _554 = asfloat((uint)(cb2_075x));
  float _555 = asfloat((uint)(cb2_073w));
  float _556 = asfloat((uint)(cb2_074x));
  float _557 = asfloat((uint)(cb2_074y));
  float _558 = asfloat((uint)(cb2_077x));
  float _559 = asfloat((uint)(cb2_077y));
  float _560 = asfloat((uint)(cb2_079x));
  float _561 = asfloat((uint)(cb2_079y));
  float _562 = asfloat((uint)(cb2_079z));
  float _563 = asfloat((uint)(cb2_078y));
  float _564 = asfloat((uint)(cb2_078z));
  float _565 = asfloat((uint)(cb2_078w));
  float _566 = asfloat((uint)(cb2_077z));
  float _567 = asfloat((uint)(cb2_077w));
  float _568 = asfloat((uint)(cb2_078x));
  float _569 = asfloat((uint)(cb2_072y));
  float _570 = asfloat((uint)(cb2_072z));
  float _571 = asfloat((uint)(cb2_072w));
  float _572 = asfloat((uint)(cb2_071x));
  float _573 = asfloat((uint)(cb2_071y));
  float _574 = asfloat((uint)(cb2_076x));
  float _575 = asfloat((uint)(cb2_070w));
  float _576 = asfloat((uint)(cb2_070x));
  float _577 = asfloat((uint)(cb2_070y));
  float _578 = asfloat((uint)(cb2_070z));
  float _579 = asfloat((uint)(cb2_073x));
  float _580 = asfloat((uint)(cb2_073y));
  float _581 = asfloat((uint)(cb2_073z));
  float _582 = asfloat((uint)(cb2_071z));
  float _583 = asfloat((uint)(cb2_071w));
  float _584 = asfloat((uint)(cb2_072x));
  float _585 = max(_487, _488);
  float _586 = max(_486, _585);
  float _587 = 1.0f / _586;
  float _588 = _587 * _486;
  float _589 = _587 * _487;
  float _590 = _587 * _488;
  float _591 = abs(_588);
  float _592 = log2(_591);
  float _593 = _592 * _576;
  float _594 = exp2(_593);
  float _595 = abs(_589);
  float _596 = log2(_595);
  float _597 = _596 * _577;
  float _598 = exp2(_597);
  float _599 = abs(_590);
  float _600 = log2(_599);
  float _601 = _600 * _578;
  float _602 = exp2(_601);
  if (_498) {
    float _605 = asfloat((uint)(cb2_076w));
    float _607 = asfloat((uint)(cb2_076z));
    float _609 = asfloat((uint)(cb2_076y));
    float _610 = _607 * _487;
    float _611 = _609 * _486;
    float _612 = _605 * _488;
    float _613 = _611 + _612;
    float _614 = _613 + _610;
    _622 = _614;
  } else {
    float _616 = _583 * _487;
    float _617 = _582 * _486;
    float _618 = _584 * _488;
    float _619 = _616 + _617;
    float _620 = _619 + _618;
    _622 = _620;
  }
  float _623 = abs(_622);
  float _624 = log2(_623);
  float _625 = _624 * _575;
  float _626 = exp2(_625);
  float _627 = log2(_626);
  float _628 = _627 * _574;
  float _629 = exp2(_628);
  float _630 = select(_502, _629, _626);
  float _631 = _630 * _572;
  float _632 = _631 + _573;
  float _633 = 1.0f / _632;
  float _634 = _633 * _626;
  if (_498) {
    if (!_500) {
      float _637 = _594 * _566;
      float _638 = _598 * _567;
      float _639 = _602 * _568;
      float _640 = _638 + _637;
      float _641 = _640 + _639;
      float _642 = _598 * _564;
      float _643 = _594 * _563;
      float _644 = _602 * _565;
      float _645 = _642 + _643;
      float _646 = _645 + _644;
      float _647 = _602 * _562;
      float _648 = _598 * _561;
      float _649 = _594 * _560;
      float _650 = _648 + _649;
      float _651 = _650 + _647;
      float _652 = max(_646, _651);
      float _653 = max(_641, _652);
      float _654 = 1.0f / _653;
      float _655 = _654 * _641;
      float _656 = _654 * _646;
      float _657 = _654 * _651;
      _659 = _655;
      _660 = _656;
      _661 = _657;
    } else {
      _659 = _594;
      _660 = _598;
      _661 = _602;
    }
    float _662 = _659 * _559;
    float _663 = exp2(_662);
    float _664 = _663 * _558;
    float _665 = saturate(_664);
    float _666 = _659 * _558;
    float _667 = _659 - _666;
    float _668 = saturate(_667);
    float _669 = max(_558, _668);
    float _670 = min(_669, _665);
    float _671 = _660 * _559;
    float _672 = exp2(_671);
    float _673 = _672 * _558;
    float _674 = saturate(_673);
    float _675 = _660 * _558;
    float _676 = _660 - _675;
    float _677 = saturate(_676);
    float _678 = max(_558, _677);
    float _679 = min(_678, _674);
    float _680 = _661 * _559;
    float _681 = exp2(_680);
    float _682 = _681 * _558;
    float _683 = saturate(_682);
    float _684 = _661 * _558;
    float _685 = _661 - _684;
    float _686 = saturate(_685);
    float _687 = max(_558, _686);
    float _688 = min(_687, _683);
    _690 = _670;
    _691 = _679;
    _692 = _688;
  } else {
    _690 = _594;
    _691 = _598;
    _692 = _602;
  }
  float _693 = _690 * _582;
  float _694 = _691 * _583;
  float _695 = _694 + _693;
  float _696 = _692 * _584;
  float _697 = _695 + _696;
  float _698 = 1.0f / _697;
  float _699 = _698 * _634;
  float _700 = saturate(_699);
  float _701 = _700 * _690;
  float _702 = saturate(_701);
  float _703 = _700 * _691;
  float _704 = saturate(_703);
  float _705 = _700 * _692;
  float _706 = saturate(_705);
  float _707 = _702 * _569;
  float _708 = _569 - _707;
  float _709 = _704 * _570;
  float _710 = _570 - _709;
  float _711 = _706 * _571;
  float _712 = _571 - _711;
  float _713 = _706 * _584;
  float _714 = _702 * _582;
  float _715 = _704 * _583;
  float _716 = _634 - _714;
  float _717 = _716 - _715;
  float _718 = _717 - _713;
  float _719 = saturate(_718);
  float _720 = _710 * _583;
  float _721 = _708 * _582;
  float _722 = _712 * _584;
  float _723 = _720 + _721;
  float _724 = _723 + _722;
  float _725 = 1.0f / _724;
  float _726 = _725 * _719;
  float _727 = _726 * _708;
  float _728 = _727 + _702;
  float _729 = saturate(_728);
  float _730 = _726 * _710;
  float _731 = _730 + _704;
  float _732 = saturate(_731);
  float _733 = _726 * _712;
  float _734 = _733 + _706;
  float _735 = saturate(_734);
  float _736 = _735 * _584;
  float _737 = _729 * _582;
  float _738 = _732 * _583;
  float _739 = _634 - _737;
  float _740 = _739 - _738;
  float _741 = _740 - _736;
  float _742 = saturate(_741);
  float _743 = _742 * _579;
  float _744 = _743 + _729;
  float _745 = saturate(_744);
  float _746 = _742 * _580;
  float _747 = _746 + _732;
  float _748 = saturate(_747);
  float _749 = _742 * _581;
  float _750 = _749 + _735;
  float _751 = saturate(_750);
  if (!_496) {
    float _753 = _745 * _555;
    float _754 = _748 * _556;
    float _755 = _751 * _557;
    float _756 = _754 + _753;
    float _757 = _756 + _755;
    float _758 = _748 * _553;
    float _759 = _745 * _552;
    float _760 = _751 * _554;
    float _761 = _758 + _759;
    float _762 = _761 + _760;
    float _763 = _751 * _551;
    float _764 = _748 * _550;
    float _765 = _745 * _549;
    float _766 = _764 + _765;
    float _767 = _766 + _763;
    if (!_494) {
      float _769 = saturate(_757);
      float _770 = saturate(_762);
      float _771 = saturate(_767);
      _773 = _771;
      _774 = _770;
      _775 = _769;
    } else {
      _773 = _767;
      _774 = _762;
      _775 = _757;
    }
  } else {
    _773 = _751;
    _774 = _748;
    _775 = _745;
  }
  if (!_491) {
    float _777 = _775 * _555;
    float _778 = _774 * _555;
    float _779 = _773 * _555;
    _781 = _779;
    _782 = _778;
    _783 = _777;
  } else {
    _781 = _773;
    _782 = _774;
    _783 = _775;
  }
  if (_408) {
    float _787 = cb1_018z * 9.999999747378752e-05f;
    float _788 = _787 * _783;
    float _789 = _787 * _782;
    float _790 = _787 * _781;
    float _792 = 5000.0f / cb1_018y;
    float _793 = _788 * _792;
    float _794 = _789 * _792;
    float _795 = _790 * _792;
    _797 = _793;
    _798 = _794;
    _799 = _795;
  } else {
    _797 = _783;
    _798 = _782;
    _799 = _781;
  }
  float _800 = _797 * 1.6047500371932983f;
  float _801 = mad(-0.5310800075531006f, _798, _800);
  float _802 = mad(-0.07366999983787537f, _799, _801);
  float _803 = _797 * -0.10208000242710114f;
  float _804 = mad(1.1081299781799316f, _798, _803);
  float _805 = mad(-0.006049999967217445f, _799, _804);
  float _806 = _797 * -0.0032599999103695154f;
  float _807 = mad(-0.07275000214576721f, _798, _806);
  float _808 = mad(1.0760200023651123f, _799, _807);
  if (_408) {
    // float _810 = max(_802, 0.0f);
    // float _811 = max(_805, 0.0f);
    // float _812 = max(_808, 0.0f);
    // bool _813 = !(_810 >= 0.0030399328097701073f);
    // if (!_813) {
    //   float _815 = abs(_810);
    //   float _816 = log2(_815);
    //   float _817 = _816 * 0.4166666567325592f;
    //   float _818 = exp2(_817);
    //   float _819 = _818 * 1.0549999475479126f;
    //   float _820 = _819 + -0.054999999701976776f;
    //   _824 = _820;
    // } else {
    //   float _822 = _810 * 12.923210144042969f;
    //   _824 = _822;
    // }
    // bool _825 = !(_811 >= 0.0030399328097701073f);
    // if (!_825) {
    //   float _827 = abs(_811);
    //   float _828 = log2(_827);
    //   float _829 = _828 * 0.4166666567325592f;
    //   float _830 = exp2(_829);
    //   float _831 = _830 * 1.0549999475479126f;
    //   float _832 = _831 + -0.054999999701976776f;
    //   _836 = _832;
    // } else {
    //   float _834 = _811 * 12.923210144042969f;
    //   _836 = _834;
    // }
    // bool _837 = !(_812 >= 0.0030399328097701073f);
    // if (!_837) {
    //   float _839 = abs(_812);
    //   float _840 = log2(_839);
    //   float _841 = _840 * 0.4166666567325592f;
    //   float _842 = exp2(_841);
    //   float _843 = _842 * 1.0549999475479126f;
    //   float _844 = _843 + -0.054999999701976776f;
    //   _917 = _824;
    //   _918 = _836;
    //   _919 = _844;
    // } else {
    //   float _846 = _812 * 12.923210144042969f;
    //   _917 = _824;
    //   _918 = _836;
    //   _919 = _846;
    // }
    _917 = renodx::color::srgb::EncodeSafe(_802);
    _918 = renodx::color::srgb::EncodeSafe(_805);
    _919 = renodx::color::srgb::EncodeSafe(_808);

  } else {
    float _848 = saturate(_802);
    float _849 = saturate(_805);
    float _850 = saturate(_808);
    bool _851 = ((uint)(cb1_018w) == -2);
    if (!_851) {
      bool _853 = !(_848 >= 0.0030399328097701073f);
      if (!_853) {
        float _855 = abs(_848);
        float _856 = log2(_855);
        float _857 = _856 * 0.4166666567325592f;
        float _858 = exp2(_857);
        float _859 = _858 * 1.0549999475479126f;
        float _860 = _859 + -0.054999999701976776f;
        _864 = _860;
      } else {
        float _862 = _848 * 12.923210144042969f;
        _864 = _862;
      }
      bool _865 = !(_849 >= 0.0030399328097701073f);
      if (!_865) {
        float _867 = abs(_849);
        float _868 = log2(_867);
        float _869 = _868 * 0.4166666567325592f;
        float _870 = exp2(_869);
        float _871 = _870 * 1.0549999475479126f;
        float _872 = _871 + -0.054999999701976776f;
        _876 = _872;
      } else {
        float _874 = _849 * 12.923210144042969f;
        _876 = _874;
      }
      bool _877 = !(_850 >= 0.0030399328097701073f);
      if (!_877) {
        float _879 = abs(_850);
        float _880 = log2(_879);
        float _881 = _880 * 0.4166666567325592f;
        float _882 = exp2(_881);
        float _883 = _882 * 1.0549999475479126f;
        float _884 = _883 + -0.054999999701976776f;
        _888 = _864;
        _889 = _876;
        _890 = _884;
      } else {
        float _886 = _850 * 12.923210144042969f;
        _888 = _864;
        _889 = _876;
        _890 = _886;
      }
    } else {
      _888 = _848;
      _889 = _849;
      _890 = _850;
    }
    float _895 = abs(_888);
    float _896 = abs(_889);
    float _897 = abs(_890);
    float _898 = log2(_895);
    float _899 = log2(_896);
    float _900 = log2(_897);
    float _901 = _898 * cb2_000z;
    float _902 = _899 * cb2_000z;
    float _903 = _900 * cb2_000z;
    float _904 = exp2(_901);
    float _905 = exp2(_902);
    float _906 = exp2(_903);
    float _907 = _904 * cb2_000y;
    float _908 = _905 * cb2_000y;
    float _909 = _906 * cb2_000y;
    float _910 = _907 + cb2_000x;
    float _911 = _908 + cb2_000x;
    float _912 = _909 + cb2_000x;
    float _913 = saturate(_910);
    float _914 = saturate(_911);
    float _915 = saturate(_912);
    _917 = _913;
    _918 = _914;
    _919 = _915;
  }
  float _923 = cb2_023x * TEXCOORD0_centroid.x;
  float _924 = cb2_023y * TEXCOORD0_centroid.y;
  float _927 = _923 + cb2_023z;
  float _928 = _924 + cb2_023w;
  float4 _931 = t9.SampleLevel(s0_space2, float2(_927, _928), 0.0f);
  float _933 = _931.x + -0.5f;
  float _934 = _933 * cb2_022x;
  float _935 = _934 + 0.5f;
  float _936 = _935 * 2.0f;
  float _937 = _936 * _917;
  float _938 = _936 * _918;
  float _939 = _936 * _919;
  float _943 = float((uint)(cb2_019z));
  float _944 = float((uint)(cb2_019w));
  float _945 = _943 + SV_Position.x;
  float _946 = _944 + SV_Position.y;
  uint _947 = uint(_945);
  uint _948 = uint(_946);
  uint _951 = cb2_019x + -1u;
  uint _952 = cb2_019y + -1u;
  int _953 = _947 & _951;
  int _954 = _948 & _952;
  float4 _955 = t3.Load(int3(_953, _954, 0));
  float _959 = _955.x * 2.0f;
  float _960 = _955.y * 2.0f;
  float _961 = _955.z * 2.0f;
  float _962 = _959 + -1.0f;
  float _963 = _960 + -1.0f;
  float _964 = _961 + -1.0f;
  float _965 = _962 * cb2_025w;
  float _966 = _963 * cb2_025w;
  float _967 = _964 * cb2_025w;
  float _968 = _965 + _937;
  float _969 = _966 + _938;
  float _970 = _967 + _939;
  float _971 = dot(float3(_968, _969, _970), float3(0.2125999927520752f, 0.7152000069618225f, 0.0722000002861023f));
  SV_Target.x = _968;
  SV_Target.y = _969;
  SV_Target.z = _970;
  SV_Target.w = _971;
  SV_Target_1.x = _971;
  SV_Target_1.y = 0.0f;
  SV_Target_1.z = 0.0f;
  SV_Target_1.w = 0.0f;
  OutputSignature output_signature = { SV_Target, SV_Target_1 };
  return output_signature;
}
