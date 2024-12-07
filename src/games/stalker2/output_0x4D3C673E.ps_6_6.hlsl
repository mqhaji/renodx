#include "./shared.h"
#include "./tonemapper.hlsl"

Texture2D<float4> ColorTexture : register(t0);

Texture2D<float4> BloomTexture : register(t1);

struct _SceneColorApplyParamaters {
  float data[4];
};
StructuredBuffer<_SceneColorApplyParamaters> SceneColorApplyParamaters : register(t2);

Texture3D<float4> LumBilateralGrid : register(t3);

Texture2D<float4> BlurredLogLum : register(t4);

Texture2D<float4> BloomDirtMaskTexture : register(t5);

Texture3D<float4> ColorGradingLUT : register(t6);

cbuffer _RootShaderParameters : register(b0) {
  float _RootShaderParameters_009x : packoffset(c009.x);
  float _RootShaderParameters_009y : packoffset(c009.y);
  float _RootShaderParameters_009z : packoffset(c009.z);
  float _RootShaderParameters_009w : packoffset(c009.w);
  float _RootShaderParameters_010x : packoffset(c010.x);
  float _RootShaderParameters_010y : packoffset(c010.y);
  float _RootShaderParameters_010z : packoffset(c010.z);
  float _RootShaderParameters_010w : packoffset(c010.w);
  float _RootShaderParameters_015x : packoffset(c015.x);
  float _RootShaderParameters_015y : packoffset(c015.y);
  float _RootShaderParameters_015z : packoffset(c015.z);
  float _RootShaderParameters_015w : packoffset(c015.w);
  float _RootShaderParameters_025y : packoffset(c025.y);
  float _RootShaderParameters_025z : packoffset(c025.z);
  float _RootShaderParameters_025w : packoffset(c025.w);
  float _RootShaderParameters_027x : packoffset(c027.x);
  float _RootShaderParameters_027y : packoffset(c027.y);
  float _RootShaderParameters_027z : packoffset(c027.z);
  float _RootShaderParameters_032x : packoffset(c032.x);
  float _RootShaderParameters_032y : packoffset(c032.y);
  float _RootShaderParameters_032z : packoffset(c032.z);
  float _RootShaderParameters_032w : packoffset(c032.w);
  float _RootShaderParameters_033z : packoffset(c033.z);
  float _RootShaderParameters_033w : packoffset(c033.w);
  float _RootShaderParameters_034x : packoffset(c034.x);
  float _RootShaderParameters_034y : packoffset(c034.y);
  float _RootShaderParameters_036x : packoffset(c036.x);
  float _RootShaderParameters_036y : packoffset(c036.y);
  float _RootShaderParameters_036z : packoffset(c036.z);
  float _RootShaderParameters_036w : packoffset(c036.w);
  float _RootShaderParameters_037x : packoffset(c037.x);
  float _RootShaderParameters_037y : packoffset(c037.y);
  float _RootShaderParameters_037z : packoffset(c037.z);
  float _RootShaderParameters_037w : packoffset(c037.w);
  float _RootShaderParameters_044x : packoffset(c044.x);
  float _RootShaderParameters_044y : packoffset(c044.y);
  float _RootShaderParameters_044z : packoffset(c044.z);
  float _RootShaderParameters_045x : packoffset(c045.x);
  float _RootShaderParameters_045y : packoffset(c045.y);
  float _RootShaderParameters_045z : packoffset(c045.z);
  float _RootShaderParameters_046x : packoffset(c046.x);
  float _RootShaderParameters_046y : packoffset(c046.y);
  float _RootShaderParameters_046z : packoffset(c046.z);
  float _RootShaderParameters_047x : packoffset(c047.x);
  float _RootShaderParameters_047y : packoffset(c047.y);
  float _RootShaderParameters_048x : packoffset(c048.x);
  float _RootShaderParameters_048y : packoffset(c048.y);
  float _RootShaderParameters_048z : packoffset(c048.z);
  float _RootShaderParameters_048w : packoffset(c048.w);
  float _RootShaderParameters_049x : packoffset(c049.x);
  float _RootShaderParameters_049y : packoffset(c049.y);
  float _RootShaderParameters_049z : packoffset(c049.z);
  float _RootShaderParameters_049w : packoffset(c049.w);
  float _RootShaderParameters_050y : packoffset(c050.y);
  uint _RootShaderParameters_050z : packoffset(c050.z);
  float _RootShaderParameters_050w : packoffset(c050.w);
  float _RootShaderParameters_051x : packoffset(c051.x);
};

cbuffer UniformBufferConstants_View : register(b1) {
  float UniformBufferConstants_View_140w : packoffset(c140.w);
};

SamplerState ColorSampler : register(s0);

SamplerState BloomSampler : register(s1);

SamplerState LumBilateralGridSampler : register(s2);

SamplerState BlurredLogLumSampler : register(s3);

SamplerState BloomDirtMaskSampler : register(s4);

SamplerState ColorGradingLUTSampler : register(s5);

float4 main(
    noperspective float2 TEXCOORD: TEXCOORD,
    noperspective float4 TEXCOORD_1: TEXCOORD1,
    noperspective float4 TEXCOORD_2: TEXCOORD2,
    noperspective float2 TEXCOORD_3: TEXCOORD3,
    noperspective float2 TEXCOORD_4: TEXCOORD4,
    noperspective float4 SV_Position: SV_Position)
    : SV_Target {
  float4 SV_Target;
  float3 post_lut;

  // texture _1 = ColorGradingLUT;
  // texture _2 = BloomDirtMaskTexture;
  // texture _3 = BlurredLogLum;
  // texture _4 = LumBilateralGrid;
  // texture _5 = SceneColorApplyParamaters;
  // texture _6 = BloomTexture;
  // texture _7 = ColorTexture;
  // SamplerState _8 = ColorGradingLUTSampler;
  // SamplerState _9 = BloomDirtMaskSampler;
  // SamplerState _10 = BlurredLogLumSampler;
  // SamplerState _11 = LumBilateralGridSampler;
  // SamplerState _12 = BloomSampler;
  // SamplerState _13 = ColorSampler;
  // cbuffer _14 = UniformBufferConstants_View;
  // cbuffer _15 = _RootShaderParameters;
  // _16 = _14;
  // _17 = _15;
  float _18 = TEXCOORD_4.x;
  float _19 = TEXCOORD_4.y;
  float _20 = TEXCOORD_3.x;
  float _21 = TEXCOORD_3.y;
  float _22 = TEXCOORD_2.z;
  float _23 = TEXCOORD_2.w;
  float _24 = TEXCOORD_1.x;
  float _25 = TEXCOORD_1.y;
  float _26 = TEXCOORD_1.z;
  float _27 = TEXCOORD_1.w;
  float _28 = TEXCOORD.x;
  float _29 = TEXCOORD.y;
  float _31 = _RootShaderParameters_046x;
  float _32 = _RootShaderParameters_046y;
  float _33 = _RootShaderParameters_046z;
  float _35 = _RootShaderParameters_048x;
  float _36 = _RootShaderParameters_048y;
  float _37 = _RootShaderParameters_048z;
  float _38 = _RootShaderParameters_048w;
  float _39 = _37 * _20;
  float _40 = _38 * _21;
  float _41 = _39 + _35;
  float _42 = _40 + _36;
  bool _43 = (_41 > 0.0f);
  bool _44 = (_42 > 0.0f);
  bool _45 = (_41 < 0.0f);
  bool _46 = (_42 < 0.0f);
  int _47 = int(_43);
  int _48 = int(_44);
  int _49 = int(_45);
  int _50 = int(_46);
  int _51 = _47 - _49;
  int _52 = _48 - _50;
  float _53 = float(_51);
  float _54 = float(_52);
  float _55 = abs(_41);
  float _56 = abs(_42);
  float _57 = _55 - _33;
  float _58 = _56 - _33;
  float _59 = saturate(_57);
  float _60 = saturate(_58);
  float _61 = _59 * _31;
  float _62 = _61 * _53;
  float _63 = _60 * _31;
  float _64 = _63 * _54;
  float _65 = _59 * _32;
  float _66 = _65 * _53;
  float _67 = _60 * _32;
  float _68 = _67 * _54;
  float _69 = _41 - _62;
  float _70 = _42 - _64;
  float _71 = _41 - _66;
  float _72 = _42 - _68;
  float _74 = _RootShaderParameters_049x;
  float _75 = _RootShaderParameters_049y;
  float _76 = _RootShaderParameters_049z;
  float _77 = _RootShaderParameters_049w;
  float _78 = _69 * _76;
  float _79 = _70 * _77;
  float _80 = _78 + _74;
  float _81 = _79 + _75;
  float _82 = _71 * _76;
  float _83 = _72 * _77;
  float _84 = _82 + _74;
  float _85 = _83 + _75;
  float _87 = _RootShaderParameters_009z;
  float _88 = _RootShaderParameters_009w;
  float _90 = _RootShaderParameters_010x;
  float _91 = _RootShaderParameters_010y;
  float _92 = _80 * _90;
  float _93 = _81 * _91;
  float _94 = _RootShaderParameters_010z;
  float _95 = _RootShaderParameters_010w;
  float _96 = _92 + _94;
  float _97 = _93 + _95;
  float _98 = _96 * _87;
  float _99 = _97 * _88;
  float _100 = _90 * _84;
  float _101 = _91 * _85;
  float _102 = _100 + _94;
  float _103 = _101 + _95;
  float _104 = _102 * _87;
  float _105 = _103 * _88;
  float _107 = _RootShaderParameters_015z;
  float _108 = _RootShaderParameters_015w;
  float _109 = _RootShaderParameters_015x;
  float _110 = _RootShaderParameters_015y;
  float _111 = max(_98, _109);
  float _112 = max(_99, _110);
  float _113 = min(_111, _107);
  float _114 = min(_112, _108);
  // _115 = _7;
  // _116 = _13;
  float4 _117 = ColorTexture.Sample(ColorSampler, float2(_113, _114));
  float _118 = _117.x;
  float _120 = _RootShaderParameters_015z;
  float _121 = _RootShaderParameters_015w;
  float _122 = _RootShaderParameters_015x;
  float _123 = _RootShaderParameters_015y;
  float _124 = max(_104, _122);
  float _125 = max(_105, _123);
  float _126 = min(_124, _120);
  float _127 = min(_125, _121);
  // _128 = _7;
  // _129 = _13;
  float4 _130 = ColorTexture.Sample(ColorSampler, float2(_126, _127));
  float _131 = _130.y;
  float _133 = _RootShaderParameters_015z;
  float _134 = _RootShaderParameters_015w;
  float _135 = _RootShaderParameters_015x;
  float _136 = _RootShaderParameters_015y;
  float _137 = max(_28, _135);
  float _138 = max(_29, _136);
  float _139 = min(_137, _133);
  float _140 = min(_138, _134);
  // _141 = _7;
  // _142 = _13;
  float4 _143 = ColorTexture.Sample(ColorSampler, float2(_139, _140));
  float _144 = _143.z;
  float _146 = UniformBufferConstants_View_140w;
  float _147 = _146 * _118;
  float _148 = _146 * _131;
  float _149 = _146 * _144;
  float _151 = _RootShaderParameters_025w;
  float _153 = _RootShaderParameters_027x;
  float _154 = _RootShaderParameters_027y;
  float _155 = _RootShaderParameters_027z;
  float _156 = dot(float3(_147, _148, _149), float3(_153, _154, _155));
  float _157 = max(_156, _151);
  float _158 = log2(_157);
  float _160 = _RootShaderParameters_032w;
  float _162 = _RootShaderParameters_033z;
  float _163 = _RootShaderParameters_033w;
  float _164 = _162 * _18;
  float _165 = _163 * _19;
  float _166 = _RootShaderParameters_025y;
  float _167 = _166 * _158;
  float _168 = _RootShaderParameters_025z;
  float _169 = _167 + _168;
  float _170 = _169 * 0.96875f;
  float _171 = _170 + 0.015625f;
  // _172 = _4;
  // _173 = _11;
  float4 _174 = LumBilateralGrid.Sample(LumBilateralGridSampler, float3(_164, _165, _171));
  float _175 = _174.x;
  float _176 = _174.y;
  float _177 = _175 / _176;
  // _178 = _3;
  // _179 = _10;
  float4 _180 = BlurredLogLum.Sample(BlurredLogLumSampler, float2(_18, _19));
  float _181 = _180.x;
  bool _182 = (_176 < 0.0010000000474974513f);
  float _183 = _182 ? _181 : _177;
  float _184 = _181 - _183;
  float _185 = _184 * _160;
  float _186 = log2(_24);
  float _187 = _183 + _186;
  float _188 = _187 + _185;
  float _190 = _RootShaderParameters_032z;
  float _191 = _RootShaderParameters_032y;
  float _192 = _RootShaderParameters_032x;
  float _193 = _186 + _158;
  float _194 = _193 - _188;
  float _195 = _188 - _25;
  bool _196 = (_195 > 0.0f);
  float _197 = _196 ? _192 : _191;
  float _208;
  float _568;
  float _569;
  float _570;
  if (_196) {
    float _200 = _RootShaderParameters_034x;
    float _201 = _195 - _200;
    float _202 = max(0.0f, _201);
    _208 = _202;
  } else {
    float _204 = _RootShaderParameters_034y;
    float _205 = _204 + _195;
    float _206 = min(0.0f, _205);
    _208 = _206;
  }
  float _209 = _208 * _197;
  float _210 = _194 * _190;
  float _211 = _188 - _193;
  float _212 = _211 + _210;
  float _213 = _212 - _208;
  float _214 = _213 + _209;
  float _215 = exp2(_214);
  float _216 = _215 * _147;
  float _217 = _215 * _148;
  float _218 = _215 * _149;
  float _219 = _215 * _146;
  float _221 = _RootShaderParameters_047y;
  float _222 = dot(float3(_216, _217, _218), float3(0.30000001192092896f, 0.5899999737739563f, 0.10999999940395355f));
  float _224 = _RootShaderParameters_009x;
  float _225 = _RootShaderParameters_009y;
  float _226 = _224 * _28;
  float _227 = _225 * _29;
  float _228 = floor(_226);
  float _229 = floor(_227);
  uint _230 = uint(_228);
  uint _231 = uint(_229);
  int _232 = _230 & 1;
  float _233 = float(_232);
  float _234 = _233 * 2.0f;
  float _235 = _234 + -1.0f;
  int _236 = _231 & 1;
  float _237 = float(_236);
  float _238 = _237 * 2.0f;
  float _239 = _238 + -1.0f;
  float _240 = _RootShaderParameters_009z;
  float _241 = _235 * _240;
  float _242 = _241 + _28;
  float _244 = _RootShaderParameters_015z;
  float _245 = _RootShaderParameters_015w;
  float _246 = _RootShaderParameters_015x;
  float _247 = _RootShaderParameters_015y;
  float _248 = max(_242, _246);
  float _249 = max(_29, _247);
  float _250 = min(_248, _244);
  float _251 = min(_249, _245);
  // _252 = _7;
  // _253 = _13;
  float4 _254 = ColorTexture.Sample(ColorSampler, float2(_250, _251));
  float _255 = _254.x;
  float _256 = _254.y;
  float _257 = _254.z;
  float _258 = _255 * _219;
  float _259 = _256 * _219;
  float _260 = _257 * _219;
  float _262 = _RootShaderParameters_009w;
  float _263 = _262 * _239;
  float _264 = _263 + _29;
  float _266 = _RootShaderParameters_015z;
  float _267 = _RootShaderParameters_015w;
  float _268 = _RootShaderParameters_015x;
  float _269 = _RootShaderParameters_015y;
  float _270 = max(_28, _268);
  float _271 = max(_264, _269);
  float _272 = min(_270, _266);
  float _273 = min(_271, _267);
  // _274 = _7;
  // _275 = _13;
  float4 _276 = ColorTexture.Sample(ColorSampler, float2(_272, _273));
  float _277 = _276.x;
  float _278 = _276.y;
  float _279 = _276.z;
  float _280 = _277 * _219;
  float _281 = _278 * _219;
  float _282 = _279 * _219;
  float _283 = dot(float3(_258, _259, _260), float3(0.30000001192092896f, 0.5899999737739563f, 0.10999999940395355f));
  float _284 = dot(float3(_280, _281, _282), float3(0.30000001192092896f, 0.5899999737739563f, 0.10999999940395355f));
  float _285 = ddx_fine(_216);
  float _286 = _285 * _235;
  float _287 = ddx_fine(_217);
  float _288 = _287 * _235;
  float _289 = ddx_fine(_218);
  float _290 = _289 * _235;
  float _291 = ddy_fine(_216);
  float _292 = _291 * _239;
  float _293 = ddy_fine(_217);
  float _294 = _293 * _239;
  float _295 = ddy_fine(_218);
  float _296 = _295 * _239;
  float _297 = ddx_fine(_222);
  float _298 = _297 * _235;
  float _299 = ddy_fine(_222);
  float _300 = _299 * _239;
  float _301 = _222 - _283;
  float _302 = _222 - _284;
  float _303 = abs(_301);
  float _304 = abs(_302);
  float _305 = abs(_298);
  float _306 = abs(_300);
  float _307 = max(_305, _306);
  float _308 = max(_303, _304);
  float _309 = max(_308, _307);
  float _310 = _309 * _24;
  float _311 = 1.0f - _310;
  float _312 = saturate(_311);
  float _313 = _221 * _312;
  float _314 = -0.0f - _313;
  float _315 = _216 * 4.0f;
  float _316 = _217 * 4.0f;
  float _317 = _218 * 4.0f;
  float _318 = _258 - _315;
  float _319 = _318 + _280;
  float _320 = _319 + _216;
  float _321 = _320 - _286;
  float _322 = _321 + _216;
  float _323 = _322 - _292;
  float _324 = _259 - _316;
  float _325 = _324 + _281;
  float _326 = _325 + _217;
  float _327 = _326 - _288;
  float _328 = _327 + _217;
  float _329 = _328 - _294;
  float _330 = _260 - _317;
  float _331 = _330 + _282;
  float _332 = _331 + _218;
  float _333 = _332 - _290;
  float _334 = _333 + _218;
  float _335 = _334 - _296;
  float _336 = _323 * _314;
  float _337 = _329 * _314;
  float _338 = _335 * _314;
  float _339 = _336 + _216;
  float _340 = _337 + _217;
  float _341 = _338 + _218;
  float _343 = _RootShaderParameters_044x;
  float _344 = _RootShaderParameters_044y;
  float _345 = _RootShaderParameters_044z;
  float _346 = _339 * _343;
  float _347 = _340 * _344;
  float _348 = _341 * _345;
  // _349 = _5;
  float4 _350 = SceneColorApplyParamaters[0].data[0 / 4];
  float _351 = _350.x;
  float _352 = _350.y;
  float _353 = _350.z;
  float _354 = _346 * _351;
  float _355 = _347 * _352;
  float _356 = _348 * _353;
  float _358 = _RootShaderParameters_036x;
  float _359 = _RootShaderParameters_036y;
  float _360 = _RootShaderParameters_036z;
  float _361 = _RootShaderParameters_036w;
  float _362 = _358 * _28;
  float _363 = _359 * _29;
  float _364 = _362 + _360;
  float _365 = _363 + _361;
  float _367 = _RootShaderParameters_037z;
  float _368 = _RootShaderParameters_037w;
  float _369 = _RootShaderParameters_037x;
  float _370 = _RootShaderParameters_037y;
  float _371 = max(_364, _369);
  float _372 = max(_365, _370);
  float _373 = min(_371, _367);
  float _374 = min(_372, _368);
  // _375 = _6;
  // _376 = _12;
  float4 _377 = BloomTexture.Sample(BloomSampler, float2(_373, _374));
  float _378 = _377.x;
  float _379 = _377.y;
  float _380 = _377.z;
  float _382 = UniformBufferConstants_View_140w;
  float _383 = _382 * _378;
  float _384 = _382 * _379;
  float _385 = _382 * _380;
  float _387 = _RootShaderParameters_048x;
  float _388 = _RootShaderParameters_048y;
  float _389 = _RootShaderParameters_048z;
  float _390 = _RootShaderParameters_048w;
  float _391 = _389 * _20;
  float _392 = _390 * _21;
  float _393 = _391 + _387;
  float _394 = _392 + _388;
  float _395 = _393 * 0.5f;
  float _396 = _394 * 0.5f;
  float _397 = _395 + 0.5f;
  float _398 = 0.5f - _396;
  // _399 = _2;
  // _400 = _9;
  float4 _401 = BloomDirtMaskTexture.Sample(BloomDirtMaskSampler, float2(_397, _398));
  float _402 = _401.x;
  float _403 = _401.y;
  float _404 = _401.z;
  float _406 = _RootShaderParameters_045x;
  float _407 = _RootShaderParameters_045y;
  float _408 = _RootShaderParameters_045z;
  float _409 = _406 * _402;
  float _410 = _407 * _403;
  float _411 = _408 * _404;
  float _412 = _409 + 1.0f;
  float _413 = _410 + 1.0f;
  float _414 = _411 + 1.0f;
  float _415 = _383 * _412;
  float _416 = _384 * _413;
  float _417 = _385 * _414;
  float _418 = _415 + _354;
  float _419 = _416 + _355;
  float _420 = _417 + _356;
  float _422 = _RootShaderParameters_047x;
  float _423 = _422 * _26;
  float _424 = _422 * _27;
  float _425 = dot(float2(_423, _424), float2(_423, _424));
  float _426 = _425 + 1.0f;
  float _427 = 1.0f / _426;
  float _428 = _427 * _427;
  float _429 = _24 * 0.009999999776482582f;
  float _430 = _429 * _428;
  float _431 = _430 * _418;
  float _432 = _430 * _419;
  float _433 = _430 * _420;
  float _434 = log2(_431);
  float _435 = log2(_432);
  float _436 = log2(_433);
  float _437 = _434 * 0.1593017578125f;
  float _438 = _435 * 0.1593017578125f;
  float _439 = _436 * 0.1593017578125f;
  float _440 = exp2(_437);
  float _441 = exp2(_438);
  float _442 = exp2(_439);
  float _443 = _440 * 18.8515625f;
  float _444 = _441 * 18.8515625f;
  float _445 = _442 * 18.8515625f;
  float _446 = _443 + 0.8359375f;
  float _447 = _444 + 0.8359375f;
  float _448 = _445 + 0.8359375f;
  float _449 = _440 * 18.6875f;
  float _450 = _441 * 18.6875f;
  float _451 = _442 * 18.6875f;
  float _452 = _449 + 1.0f;
  float _453 = _450 + 1.0f;
  float _454 = _451 + 1.0f;
  float _455 = 1.0f / _452;
  float _456 = 1.0f / _453;
  float _457 = 1.0f / _454;
  float _458 = _455 * _446;
  float _459 = _456 * _447;
  float _460 = _457 * _448;
  float _461 = log2(_458);
  float _462 = log2(_459);
  float _463 = log2(_460);
  float _464 = _461 * 78.84375f;
  float _465 = _462 * 78.84375f;
  float _466 = _463 * 78.84375f;
  float _467 = exp2(_464);
  float _468 = exp2(_465);
  float _469 = exp2(_466);
  float _470 = _467 * 0.96875f;
  float _471 = _468 * 0.96875f;
  float _472 = _469 * 0.96875f;
  float _473 = _470 + 0.015625f;
  float _474 = _471 + 0.015625f;
  float _475 = _472 + 0.015625f;
  // _476 = _1;
  // _477 = _8;
  float4 _478 = ColorGradingLUT.Sample(ColorGradingLUTSampler, float3(_473, _474, _475));
  post_lut = _478.rgb;
  // Code after sampling
  return float4(post_lut, 0.f);

  float _479 = _478.x;
  float _480 = _478.y;
  float _481 = _478.z;

  float _482 = _479 * 1.0499999523162842f;
  float _483 = _480 * 1.0499999523162842f;
  float _484 = _481 * 1.0499999523162842f;
  float _485 = _23 * 543.3099975585938f;
  float _486 = _485 + _22;
  float _487 = sin(_486);
  float _488 = _487 * 493013.0f;
  float _489 = frac(_488);
  float _490 = _489 * 0.00390625f;
  float _491 = _490 + -0.001953125f;
  float _492 = _491 + _482;
  float _493 = _491 + _483;
  float _494 = _491 + _484;
  uint _496 = _RootShaderParameters_050z;
  bool _497 = (_496 == 0);
  _568 = _492;
  _569 = _493;
  _570 = _494;

  // Never runs
  if (!_497) {
    float _499 = log2(_492);
    float _500 = log2(_493);
    float _501 = log2(_494);
    float _502 = _499 * 0.012683313339948654f;
    float _503 = _500 * 0.012683313339948654f;
    float _504 = _501 * 0.012683313339948654f;
    float _505 = exp2(_502);
    float _506 = exp2(_503);
    float _507 = exp2(_504);
    float _508 = _505 + -0.8359375f;
    float _509 = _506 + -0.8359375f;
    float _510 = _507 + -0.8359375f;
    float _511 = max(0.0f, _508);
    float _512 = max(0.0f, _509);
    float _513 = max(0.0f, _510);
    float _514 = _505 * 18.6875f;
    float _515 = _506 * 18.6875f;
    float _516 = _507 * 18.6875f;
    float _517 = 18.8515625f - _514;
    float _518 = 18.8515625f - _515;
    float _519 = 18.8515625f - _516;
    float _520 = _511 / _517;
    float _521 = _512 / _518;
    float _522 = _513 / _519;
    float _523 = log2(_520);
    float _524 = log2(_521);
    float _525 = log2(_522);
    float _526 = _523 * 6.277394771575928f;
    float _527 = _524 * 6.277394771575928f;
    float _528 = _525 * 6.277394771575928f;
    float _529 = exp2(_526);
    float _530 = exp2(_527);
    float _531 = exp2(_528);
    float _532 = _529 * 10000.0f;
    float _533 = _530 * 10000.0f;
    float _534 = _531 * 10000.0f;
    float _536 = _RootShaderParameters_050y;
    float _537 = _532 / _536;
    float _538 = _533 / _536;
    float _539 = _534 / _536;
    float _540 = max(6.103519990574569e-05f, _537);
    float _541 = max(6.103519990574569e-05f, _538);
    float _542 = max(6.103519990574569e-05f, _539);
    float _543 = max(_540, 0.0031306699384003878f);
    float _544 = max(_541, 0.0031306699384003878f);
    float _545 = max(_542, 0.0031306699384003878f);
    float _546 = log2(_543);
    float _547 = log2(_544);
    float _548 = log2(_545);
    float _549 = _546 * 0.4166666567325592f;
    float _550 = _547 * 0.4166666567325592f;
    float _551 = _548 * 0.4166666567325592f;
    float _552 = exp2(_549);
    float _553 = exp2(_550);
    float _554 = exp2(_551);
    float _555 = _552 * 1.0549999475479126f;
    float _556 = _553 * 1.0549999475479126f;
    float _557 = _554 * 1.0549999475479126f;
    float _558 = _555 + -0.054999999701976776f;
    float _559 = _556 + -0.054999999701976776f;
    float _560 = _557 + -0.054999999701976776f;
    float _561 = _540 * 12.920000076293945f;
    float _562 = _541 * 12.920000076293945f;
    float _563 = _542 * 12.920000076293945f;
    float _564 = min(_561, _558);
    float _565 = min(_562, _559);
    float _566 = min(_563, _560);
    _568 = _564;
    _569 = _565;
    _570 = _566;
  }
  float _572 = _RootShaderParameters_051x;
  float _574 = _RootShaderParameters_050w;
  if (injectedData.toneMapType > 0.f) {
    _572 = DEFAULT_CONTRAST;
    _574 = DEFAULT_BRIGHTNESS;
  }
  float _575 = _568 + -0.5f;
  float _576 = _575 + _574;
  float _577 = _569 + -0.5f;
  float _578 = _577 + _574;
  float _579 = _570 + -0.5f;
  float _580 = _579 + _574;
  float _581 = _576 * _572;
  float _582 = _578 * _572;
  float _583 = _580 * _572;
  float _584 = _581 + 0.5f;
  float _585 = _582 + 0.5f;
  float _586 = _583 + 0.5f;
  float _587 = saturate(_584);
  float _588 = saturate(_585);
  float _589 = saturate(_586);

  SV_Target.x = _587;
  SV_Target.y = _588;
  SV_Target.z = _589;
  SV_Target.w = 0.0f;
  return SV_Target;
}
