#include "./shared.h"

Texture2D<float4> t0 : register(t0);
Texture2D<float4> t1 : register(t1);
struct _t2 {
  float data[4];
};
StructuredBuffer<_t2> t2 : register(t2);
Texture2D<float4> t3 : register(t3);
Texture3D<float4> t4 : register(t4);

cbuffer _cb0 : register(b0) { float4 cb0[47] : packoffset(c0); };
cbuffer _cb1 : register(b1) { float4 cb1[331] : packoffset(c0); };

SamplerState s0 : register(s0);
SamplerState s1 : register(s1);
SamplerState s2 : register(s2);
SamplerState s3 : register(s3);

float4 main(noperspective float2 TEXCOORD
            : TEXCOORD, noperspective float4 TEXCOORD_1
            : TEXCOORD1, noperspective float4 TEXCOORD_2
            : TEXCOORD2, noperspective float2 TEXCOORD_3
            : TEXCOORD3, noperspective float2 TEXCOORD_4
            : TEXCOORD4, noperspective float4 SV_Position
            : SV_Position)
    : SV_Target {
  float4 SV_Target;
  // texture _1 = t4;
  // texture _2 = t3;
  // texture _3 = t2;
  // texture _4 = t1;
  // texture _5 = t0;
  // SamplerState _6 = s3;
  // SamplerState _7 = s2;
  // SamplerState _8 = s1;
  // SamplerState _9 = s0;
  // cbuffer _10 = cb1; // index=1
  // cbuffer _11 = cb0; // index=0
  // _12 = _10;
  // _13 = _11;
  float _14 = TEXCOORD_3.x;
  float _15 = TEXCOORD_3.y;
  float _16 = TEXCOORD_2.z;
  float _17 = TEXCOORD_2.w;
  float _18 = TEXCOORD_1.x;
  float _19 = TEXCOORD_1.z;
  float _20 = TEXCOORD_1.w;
  float _21 = TEXCOORD.x;
  float _22 = TEXCOORD.y;
  float4 _23 = cb0[15u];
  float _24 = _23.z;
  float _25 = _23.w;
  float _26 = _23.x;
  float _27 = _23.y;
  float _28 = max(_21, _26);
  float _29 = max(_22, _27);
  float _30 = min(_28, _24);
  float _31 = min(_29, _25);
  // _32 = _5;
  // _33 = _9;
  float4 _34 = t0.Sample(s0, float2(_30, _31));
  float _35 = _34.x;
  float _36 = _34.y;
  float _37 = _34.z;
  float4 _38 = cb1[136u];
  float _39 = _38.w;
  float _40 = _39 * _35;
  float _41 = _39 * _36;
  float _42 = _39 * _37;
  float4 _43 = cb0[43u];
  float _44 = _43.y;
  float _45 = dot(
      float3(_40, _41, _42),
      float3(0.30000001192092896f, 0.5899999737739563f, 0.10999999940395355f));
  float4 _46 = cb0[9u];
  float _47 = _46.x;
  float _48 = _46.y;
  float _49 = _47 * _21;
  float _50 = _48 * _22;
  float _51 = floor(_49);
  float _52 = floor(_50);
  uint _53 = uint(_51);
  uint _54 = uint(_52);
  int _55 = _53 & 1;
  float _56 = float(_55);
  float _57 = _56 * 2.0f;
  float _58 = _57 + -1.0f;
  int _59 = _54 & 1;
  float _60 = float(_59);
  float _61 = _60 * 2.0f;
  float _62 = _61 + -1.0f;
  float _63 = _46.z;
  float _64 = _58 * _63;
  float _65 = _64 + _21;
  float4 _66 = cb0[15u];
  float _67 = _66.z;
  float _68 = _66.w;
  float _69 = _66.x;
  float _70 = _66.y;
  float _71 = max(_65, _69);
  float _72 = max(_22, _70);
  float _73 = min(_71, _67);
  float _74 = min(_72, _68);
  // _75 = _5;
  // _76 = _9;
  float4 _77 = t0.Sample(s0, float2(_73, _74));
  float _78 = _77.x;
  float _79 = _77.y;
  float _80 = _77.z;
  float _81 = _78 * _39;
  float _82 = _79 * _39;
  float _83 = _80 * _39;
  float4 _84 = cb0[9u];
  float _85 = _84.w;
  float _86 = _85 * _62;
  float _87 = _86 + _22;
  float4 _88 = cb0[15u];
  float _89 = _88.z;
  float _90 = _88.w;
  float _91 = _88.x;
  float _92 = _88.y;
  float _93 = max(_21, _91);
  float _94 = max(_87, _92);
  float _95 = min(_93, _89);
  float _96 = min(_94, _90);
  // _97 = _5;
  // _98 = _9;
  float4 _99 = t0.Sample(s0, float2(_95, _96));
  float _100 = _99.x;
  float _101 = _99.y;
  float _102 = _99.z;
  float _103 = _100 * _39;
  float _104 = _101 * _39;
  float _105 = _102 * _39;
  float _106 = dot(
      float3(_81, _82, _83),
      float3(0.30000001192092896f, 0.5899999737739563f, 0.10999999940395355f));
  float _107 = dot(
      float3(_103, _104, _105),
      float3(0.30000001192092896f, 0.5899999737739563f, 0.10999999940395355f));
  float _108 = ddx_fine(_40);
  float _109 = _108 * _58;
  float _110 = ddx_fine(_41);
  float _111 = _110 * _58;
  float _112 = ddx_fine(_42);
  float _113 = _112 * _58;
  float _114 = ddy_fine(_40);
  float _115 = _114 * _62;
  float _116 = ddy_fine(_41);
  float _117 = _116 * _62;
  float _118 = ddy_fine(_42);
  float _119 = _118 * _62;
  float _120 = ddx_fine(_45);
  float _121 = _120 * _58;
  float _122 = ddy_fine(_45);
  float _123 = _122 * _62;
  float _124 = _45 - _106;
  float _125 = _45 - _107;
  float _126 = abs(_124);
  float _127 = abs(_125);
  float _128 = abs(_121);
  float _129 = abs(_123);
  float _130 = max(_128, _129);
  float _131 = max(_126, _127);
  float _132 = max(_131, _130);
  float _133 = _132 * _18;
  float _134 = 1.0f - _133;
  float _135 = saturate(_134);
  float _136 = _44 * _135;
  float _137 = -0.0f - _136;
  float _138 = _40 * 4.0f;
  float _139 = _41 * 4.0f;
  float _140 = _42 * 4.0f;
  float _141 = _81 - _138;
  float _142 = _141 + _103;
  float _143 = _142 + _40;
  float _144 = _143 - _109;
  float _145 = _144 + _40;
  float _146 = _145 - _115;
  float _147 = _82 - _139;
  float _148 = _147 + _104;
  float _149 = _148 + _41;
  float _150 = _149 - _111;
  float _151 = _150 + _41;
  float _152 = _151 - _117;
  float _153 = _83 - _140;
  float _154 = _153 + _105;
  float _155 = _154 + _42;
  float _156 = _155 - _113;
  float _157 = _156 + _42;
  float _158 = _157 - _119;
  float _159 = _146 * _137;
  float _160 = _152 * _137;
  float _161 = _158 * _137;
  float _162 = _159 + _40;
  float _163 = _160 + _41;
  float _164 = _161 + _42;
  float4 _165 = cb0[40u];
  float _166 = _165.x;
  float _167 = _165.y;
  float _168 = _165.z;
  float _169 = _162 * _166;
  float _170 = _163 * _167;
  float _171 = _164 * _168;
  // _172 = _3;
  float4 _173 = t2[0].data[0 / 4];
  float _174 = _173.x;
  float _175 = _173.y;
  float _176 = _173.z;
  float _177 = _169 * _174;
  float _178 = _170 * _175;
  float _179 = _171 * _176;
  float4 _180 = cb0[32u];
  float _181 = _180.x;
  float _182 = _180.y;
  float _183 = _180.z;
  float _184 = _180.w;
  float _185 = _181 * _21;
  float _186 = _182 * _22;
  float _187 = _185 + _183;
  float _188 = _186 + _184;
  float4 _189 = cb0[33u];
  float _190 = _189.z;
  float _191 = _189.w;
  float _192 = _189.x;
  float _193 = _189.y;
  float _194 = max(_187, _192);
  float _195 = max(_188, _193);
  float _196 = min(_194, _190);
  float _197 = min(_195, _191);
  // _198 = _4;
  // _199 = _8;
  float4 _200 = t1.Sample(s1, float2(_196, _197));
  float _201 = _200.x;
  float _202 = _200.y;
  float _203 = _200.z;
  float4 _204 = cb1[136u];
  float _205 = _204.w;
  float _206 = _205 * _201;
  float _207 = _205 * _202;
  float _208 = _205 * _203;
  float4 _209 = cb0[44u];
  float _210 = _209.x;
  float _211 = _209.y;
  float _212 = _209.z;
  float _213 = _209.w;
  float _214 = _212 * _14;
  float _215 = _213 * _15;
  float _216 = _214 + _210;
  float _217 = _215 + _211;
  float _218 = _216 * 0.5f;
  float _219 = _217 * 0.5f;
  float _220 = _218 + 0.5f;
  float _221 = 0.5f - _219;
  // _222 = _2;
  // _223 = _7;
  float4 _224 = t3.Sample(s2, float2(_220, _221));
  float _225 = _224.x;
  float _226 = _224.y;
  float _227 = _224.z;
  float4 _228 = cb0[41u];
  float _229 = _228.x;
  float _230 = _228.y;
  float _231 = _228.z;
  float _232 = _229 * _225;
  float _233 = _230 * _226;
  float _234 = _231 * _227;
  float _235 = _232 + 1.0f;
  float _236 = _233 + 1.0f;
  float _237 = _234 + 1.0f;
  float _238 = _206 * _235;
  float _239 = _207 * _236;
  float _240 = _208 * _237;
  float _241 = _238 + _177;
  float _242 = _239 + _178;
  float _243 = _240 + _179;
  float4 _244 = cb0[43u];
  float _245 = _244.x;
  float _246 = _245 * _19;
  float _247 = _245 * _20;
  float _248 = dot(float2(_246, _247), float2(_246, _247));
  float _249 = _248 + 1.0f;
  float _250 = 1.0f / _249;
  float _251 = _250 * _250;
  float _252 = _251 * _18;
  float _253 = _252 * _241;
  float _254 = _252 * _242;
  float _255 = _252 * _243;

  float3 untonemapped = float3(_253, _254, _255);

  float _256 = _253 + 0.002667719265446067f;
  float _257 = _254 + 0.002667719265446067f;
  float _258 = _255 + 0.002667719265446067f;
  float _259 = log2(_256);
  float _260 = log2(_257);
  float _261 = log2(_258);
  float _262 = _259 * 0.0714285746216774f;
  float _263 = _260 * 0.0714285746216774f;
  float _264 = _261 * 0.0714285746216774f;
  float _265 = _262 + 0.6107269525527954f;
  float _266 = _263 + 0.6107269525527954f;
  float _267 = _264 + 0.6107269525527954f;
  float _268 = saturate(_265);
  float _269 = saturate(_266);
  float _270 = saturate(_267);
  float _271 = _268 * 0.96875f;
  float _272 = _269 * 0.96875f;
  float _273 = _270 * 0.96875f;
  float _274 = _271 + 0.015625f;
  float _275 = _272 + 0.015625f;
  float _276 = _273 + 0.015625f;
  // _277 = _1;
  // _278 = _6;
  float4 _279 = t4.Sample(s3, float3(_274, _275, _276));
  float _280 = _279.x;
  float _281 = _279.y;
  float _282 = _279.z;
  float _283 = _280 * 1.0499999523162842f;
  float _284 = _281 * 1.0499999523162842f;
  float _285 = _282 * 1.0499999523162842f;

  float3 post_lut = float3(_283, _284, _285);


  // Custom
  float3 lut_input = renodx::color::pq::from::BT2020(untonemapped, 100.f);
  float3 sampled = renodx::lut::Sample(t4, s3, lut_input);
  post_lut = renodx::color::bt2020::from::PQ(sampled, 100.f);

  float _286 = _17 * 543.3099975585938f;
  float _287 = _286 + _16;
  float _288 = sin(_287);
  float _289 = _288 * 493013.0f;
  float _290 = frac(_289);
  float _291 = _290 * 0.00390625f;
  float _292 = _291 + -0.001953125f;
  float _293 = _292 + _283;
  float _294 = _292 + _284;
  float _295 = _292 + _285;
  int4 _296 = cb0[46u];
  int _297 = _296.z;
  bool _298 = (_297 == 0);
  float _369;
  _369 = _293;
  float _370;
  _370 = _294;
  float _371;
  _371 = _295;

  _298 = false; // disable custom PQ

  if (!_298) {
    float _300 = log2(_293);
    float _301 = log2(_294);
    float _302 = log2(_295);
    float _303 = _300 * 0.012683313339948654f;
    float _304 = _301 * 0.012683313339948654f;
    float _305 = _302 * 0.012683313339948654f;
    float _306 = exp2(_303);
    float _307 = exp2(_304);
    float _308 = exp2(_305);
    float _309 = _306 + -0.8359375f;
    float _310 = _307 + -0.8359375f;
    float _311 = _308 + -0.8359375f;
    float _312 = max(0.0f, _309);
    float _313 = max(0.0f, _310);
    float _314 = max(0.0f, _311);
    float _315 = _306 * 18.6875f;
    float _316 = _307 * 18.6875f;
    float _317 = _308 * 18.6875f;
    float _318 = 18.8515625f - _315;
    float _319 = 18.8515625f - _316;
    float _320 = 18.8515625f - _317;
    float _321 = _312 / _318;
    float _322 = _313 / _319;
    float _323 = _314 / _320;
    float _324 = log2(_321);
    float _325 = log2(_322);
    float _326 = log2(_323);
    float _327 = _324 * 6.277394771575928f;
    float _328 = _325 * 6.277394771575928f;
    float _329 = _326 * 6.277394771575928f;
    float _330 = exp2(_327);
    float _331 = exp2(_328);
    float _332 = exp2(_329);
    float _333 = _330 * 10000.0f;
    float _334 = _331 * 10000.0f;
    float _335 = _332 * 10000.0f;
    float4 _336 = cb0[46u];
    float _337 = _336.y;
    float _338 = _333 / _337;
    float _339 = _334 / _337;
    float _340 = _335 / _337;
    float _341 = max(6.103519990574569e-05f, _338);
    float _342 = max(6.103519990574569e-05f, _339);
    float _343 = max(6.103519990574569e-05f, _340);
    float _344 = max(_341, 0.0031306699384003878f);
    float _345 = max(_342, 0.0031306699384003878f);
    float _346 = max(_343, 0.0031306699384003878f);
    float _347 = log2(_344);
    float _348 = log2(_345);
    float _349 = log2(_346);
    float _350 = _347 * 0.4166666567325592f;
    float _351 = _348 * 0.4166666567325592f;
    float _352 = _349 * 0.4166666567325592f;
    float _353 = exp2(_350);
    float _354 = exp2(_351);
    float _355 = exp2(_352);
    float _356 = _353 * 1.0549999475479126f;
    float _357 = _354 * 1.0549999475479126f;
    float _358 = _355 * 1.0549999475479126f;
    float _359 = _356 + -0.054999999701976776f;
    float _360 = _357 + -0.054999999701976776f;
    float _361 = _358 + -0.054999999701976776f;
    float _362 = _341 * 12.920000076293945f;
    float _363 = _342 * 12.920000076293945f;
    float _364 = _343 * 12.920000076293945f;
    float _365 = min(_362, _359);
    float _366 = min(_363, _360);
    float _367 = min(_364, _361);
    _369 = _365;
    _370 = _366;
    _371 = _367;
  }
  SV_Target.x = _369;
  SV_Target.y = _370;
  SV_Target.z = _371;
  SV_Target.w = 0.0f;

  SV_Target.rgb = post_lut;
  SV_Target.rgb = renodx::color::bt2020::from::BT709(SV_Target.rgb);
  SV_Target.rgb = renodx::color::pq::from::BT2020(SV_Target.rgb * (203.f / 10000.f));

  return SV_Target;
}
