Texture2D<float4> t0 : register(t0, space1);

Texture2D<float4> t1 : register(t2);

Texture3D<float4> t2 : register(t7);

cbuffer cb0 : register(b2) {
  float cb0_015x : packoffset(c015.x);
  float cb0_015y : packoffset(c015.y);
  float cb0_015z : packoffset(c015.z);
};

cbuffer cb1 : register(b3) {
  uint cb1_000x : packoffset(c000.x);
  uint cb1_001x : packoffset(c001.x);
};

cbuffer cb2 : register(b5) {
  float cb2_012x : packoffset(c012.x);
  float cb2_012y : packoffset(c012.y);
  float cb2_012z : packoffset(c012.z);
  float cb2_012w : packoffset(c012.w);
  float cb2_013y : packoffset(c013.y);
  float cb2_013z : packoffset(c013.z);
  float cb2_015z : packoffset(c015.z);
  float cb2_015w : packoffset(c015.w);
  float cb2_019y : packoffset(c019.y);
  float cb2_025y : packoffset(c025.y);
};

cbuffer cb3 : register(b6) {
  float cb3_000x : packoffset(c000.x);
  float cb3_000y : packoffset(c000.y);
  float cb3_000z : packoffset(c000.z);
  float cb3_001x : packoffset(c001.x);
  float cb3_001y : packoffset(c001.y);
  float cb3_001z : packoffset(c001.z);
  float cb3_043x : packoffset(c043.x);
  float cb3_043y : packoffset(c043.y);
  float cb3_043z : packoffset(c043.z);
  float cb3_043w : packoffset(c043.w);
  float cb3_044x : packoffset(c044.x);
  float cb3_044y : packoffset(c044.y);
  float cb3_044z : packoffset(c044.z);
  float cb3_044w : packoffset(c044.w);
  float cb3_045x : packoffset(c045.x);
  float cb3_045y : packoffset(c045.y);
  float cb3_045z : packoffset(c045.z);
  float cb3_046x : packoffset(c046.x);
  float cb3_046y : packoffset(c046.y);
  float cb3_046z : packoffset(c046.z);
  float cb3_046w : packoffset(c046.w);
  float cb3_047x : packoffset(c047.x);
  float cb3_047y : packoffset(c047.y);
  float cb3_047z : packoffset(c047.z);
  float cb3_047w : packoffset(c047.w);
  float cb3_048x : packoffset(c048.x);
  float cb3_048y : packoffset(c048.y);
  float cb3_048z : packoffset(c048.z);
  float cb3_048w : packoffset(c048.w);
  float cb3_049x : packoffset(c049.x);
  float cb3_049y : packoffset(c049.y);
  float cb3_049z : packoffset(c049.z);
  float cb3_050x : packoffset(c050.x);
  float cb3_051x : packoffset(c051.x);
  float cb3_051y : packoffset(c051.y);
  float cb3_051z : packoffset(c051.z);
  float cb3_051w : packoffset(c051.w);
  float cb3_052x : packoffset(c052.x);
  float cb3_052y : packoffset(c052.y);
  float cb3_052z : packoffset(c052.z);
  float cb3_053x : packoffset(c053.x);
  float cb3_053y : packoffset(c053.y);
  float cb3_053z : packoffset(c053.z);
  float cb3_053w : packoffset(c053.w);
  float cb3_054x : packoffset(c054.x);
  float cb3_054y : packoffset(c054.y);
  float cb3_054z : packoffset(c054.z);
  float cb3_054w : packoffset(c054.w);
  float cb3_055x : packoffset(c055.x);
  float cb3_055y : packoffset(c055.y);
  float cb3_055z : packoffset(c055.z);
  float cb3_055w : packoffset(c055.w);
  float cb3_056x : packoffset(c056.x);
  float cb3_056y : packoffset(c056.y);
  float cb3_056z : packoffset(c056.z);
  float cb3_056w : packoffset(c056.w);
  float cb3_057x : packoffset(c057.x);
  float cb3_057y : packoffset(c057.y);
  float cb3_057z : packoffset(c057.z);
  float cb3_057w : packoffset(c057.w);
  float cb3_058x : packoffset(c058.x);
  float cb3_058y : packoffset(c058.y);
  float cb3_058z : packoffset(c058.z);
};

cbuffer cb4 : register(b7) {
  uint cb4_007x : packoffset(c007.x);
};

cbuffer cb5 : register(b10, space1) {
  float cb5_000x : packoffset(c000.x);
};

SamplerState s0 : register(s2, space1);

SamplerState s1[] : register(s0, space2);

float4 main(
  float4 COLOR : COLOR,
  linear float2 TEXCOORD : TEXCOORD,
  linear float3 TEXCOORD_1 : TEXCOORD1,
  linear float4 TEXCOORD_6 : TEXCOORD6,
  noperspective float4 SV_Position : SV_Position,
  linear float4 SV_ClipDistance : SV_ClipDistance
) : SV_Target {
  float4 SV_Target;
  uint _27 = (cb1_000x) + 0;
  float4 _29 = t0.Sample(s1[_27], float2((TEXCOORD.x), (TEXCOORD.y)));
  float _34 = dot(float3((TEXCOORD_1.x), (TEXCOORD_1.y), (TEXCOORD_1.z)), float3((TEXCOORD_1.x), (TEXCOORD_1.y), (TEXCOORD_1.z)));
  float _35 = rsqrt(_34);
  float _36 = _35 * (TEXCOORD_1.x);
  float _37 = _35 * (TEXCOORD_1.y);
  float _38 = _35 * (TEXCOORD_1.z);
  float _47 = (_29.w) * (COLOR.w);
  float _48 = (cb2_012z) * (COLOR.x);
  float _49 = (cb2_012y) * (COLOR.y);
  float _51 = (_29.w) * (COLOR.z);
  float _52 = _51 * (cb5_000x);
  float _53 = _52 * (cb2_012w);
  float _54 = _53 * (cb2_013y);
  float _55 = (_29.x) * (_29.x);
  float _56 = (_29.y) * (_29.y);
  float _57 = (_29.z) * (_29.z);
  float _66 = -0.0f - (cb3_000x);
  float _67 = -0.0f - (cb3_000y);
  float _68 = -0.0f - (cb3_000z);
  float _69 = _48 * _48;
  float _70 = _49 * _49;
  float _71 = dot(float3(_36, _37, _38), float3(_66, _67, _68));
  float _72 = saturate(_71);
  float _73 = _72 * (cb3_001x);
  float _74 = _72 * (cb3_001y);
  float _75 = _72 * (cb3_001z);
  float _78 = (cb3_043w) + _38;
  float _81 = _78 * (cb3_044w);
  float _82 = max(0.0f, _81);
  float _87 = (cb3_047x) * _82;
  float _88 = (cb3_047y) * _82;
  float _89 = (cb3_047z) * _82;
  float _94 = _87 + (cb3_048x);
  float _95 = _88 + (cb3_048y);
  float _96 = _89 + (cb3_048z);
  float _97 = 1.0f - (cb2_013z);
  float _98 = _94 * _97;
  float _99 = _95 * _97;
  float _100 = _96 * _97;
  float _105 = (cb3_045x) * _82;
  float _106 = (cb3_045y) * _82;
  float _107 = (cb3_045z) * _82;
  float _112 = _105 + (cb3_046x);
  float _113 = _106 + (cb3_046y);
  float _114 = _107 + (cb3_046z);
  float _115 = _112 * (cb2_013z);
  float _116 = _113 * (cb2_013z);
  float _117 = _114 * (cb2_013z);
  float _118 = _115 + _98;
  float _119 = _116 + _99;
  float _120 = _117 + _100;
  float _121 = _118 * _70;
  float _122 = _119 * _70;
  float _123 = _120 * _70;
  float _127 = (cb3_043x) * _82;
  float _128 = (cb3_043y) * _82;
  float _129 = (cb3_043z) * _82;
  float _133 = _127 + (cb3_044x);
  float _134 = _128 + (cb3_044y);
  float _135 = _129 + (cb3_044z);
  float _139 = dot(float3((cb3_046w), (cb3_047w), (cb3_048w)), float3(_36, _37, _38));
  float _140 = saturate(_139);
  float _145 = (cb3_049x) * _140;
  float _146 = (cb3_049y) * _140;
  float _147 = (cb3_049z) * _140;
  float _148 = _133 + _145;
  float _149 = _134 + _146;
  float _150 = _135 + _147;
  float _151 = _148 * _69;
  float _152 = _149 * _69;
  float _153 = _150 * _69;
  float _154 = _73 + _54;
  float _155 = _154 + _121;
  float _156 = _155 + _151;
  float _157 = _55 * _156;
  float _158 = _74 + _54;
  float _159 = _158 + _122;
  float _160 = _159 + _152;
  float _161 = _56 * _160;
  float _162 = _75 + _54;
  float _163 = _162 + _123;
  float _164 = _163 + _153;
  float _165 = _57 * _164;
  float _167 = _47 * (cb2_012x);
  bool _170 = (((uint)(cb4_007x)) == 0);
  float _175 = (TEXCOORD_6.x) - (cb0_015x);
  float _176 = (TEXCOORD_6.y) - (cb0_015y);
  float _177 = (TEXCOORD_6.z) - (cb0_015z);
  float _178 = _175 * _175;
  float _179 = _176 * _176;
  float _180 = _178 + _179;
  float _181 = _177 * _177;
  float _182 = _180 + _181;
  float _183 = sqrt(_182);
  float _186 = _183 - (cb3_050x);
  float _187 = max(0.0f, _186);
  float _188 = _187 / _183;
  float _189 = _188 * _177;
  float _192 = (cb3_052z) * _189;
  float _193 = abs(_189);
  bool _194 = (_193 > 0.009999999776482582f);
  float _195 = _192 * -1.4426950216293335f;
  float _196 = exp2(_195);
  float _197 = 1.0f - _196;
  float _198 = _197 / _192;
  float _199 = (_194 ? _198 : 1.0f);
  float _202 = _199 * _187;
  float _203 = _202 * (cb3_051w);
  float _204 = min(1.0f, _203);
  float _205 = _204 * 1.4426950216293335f;
  float _206 = exp2(_205);
  float _207 = saturate(_206);
  float _208 = 1.0f - _207;
  float _210 = (cb3_052y) * _208;
  float _216 = dot(float3(_175, _176, _177), float3(_175, _176, _177));
  float _217 = rsqrt(_216);
  float _218 = _217 * _175;
  float _219 = _217 * _176;
  float _220 = _217 * _177;
  float _221 = dot(float3(_218, _219, _220), float3((cb3_054x), (cb3_054y), (cb3_054z)));
  float _222 = saturate(_221);
  float _223 = log2(_222);
  float _224 = _223 * (cb3_054w);
  float _225 = exp2(_224);
  float _231 = dot(float3(_218, _219, _220), float3((cb3_053x), (cb3_053y), (cb3_053z)));
  float _232 = saturate(_231);
  float _233 = log2(_232);
  float _234 = _233 * (cb3_053w);
  float _235 = exp2(_234);
  float _237 = 1.0f - _210;
  float _238 = (cb3_051y) * _237;
  float _241 = _187 - (cb3_052x);
  float _242 = max(0.0f, _241);
  float _243 = (cb3_051x) * 1.4426950216293335f;
  float _244 = _243 * _242;
  float _245 = exp2(_244);
  float _246 = 1.0f - _245;
  float _247 = _246 * _238;
  float _248 = _247 + _210;
  float _249 = saturate(_248);
  float _251 = _187 * -1.4426950216293335f;
  float _252 = _251 * (cb3_051z);
  float _253 = exp2(_252);
  float _254 = 1.0f - _253;
  float _263 = (cb3_058x) - (cb3_056x);
  float _264 = (cb3_058y) - (cb3_056y);
  float _265 = (cb3_058z) - (cb3_056z);
  float _266 = _263 * _225;
  float _267 = _264 * _225;
  float _268 = _265 * _225;
  float _269 = _266 + (cb3_056x);
  float _270 = _267 + (cb3_056y);
  float _271 = _268 + (cb3_056z);
  float _276 = (cb3_055x) - _269;
  float _277 = (cb3_055y) - _270;
  float _278 = (cb3_055z) - _271;
  float _279 = _276 * _235;
  float _280 = _277 * _235;
  float _281 = _278 * _235;
  float _286 = _269 - (cb3_057x);
  float _287 = _286 + _279;
  float _288 = _270 - (cb3_057y);
  float _289 = _288 + _280;
  float _290 = _271 - (cb3_057z);
  float _291 = _290 + _281;
  float _292 = _287 * _254;
  float _293 = _289 * _254;
  float _294 = _291 * _254;
  float _295 = _292 + (cb3_057x);
  float _296 = _293 + (cb3_057y);
  float _297 = _294 + (cb3_057z);
  float _301 = (cb3_055w) - _295;
  float _302 = (cb3_056w) - _296;
  float _303 = (cb3_057w) - _297;
  float _304 = _301 * _238;
  float _305 = _302 * _238;
  float _306 = _303 * _238;
  float _333;
  float _348;
  float _349;
  float _350;
  float _367;
  float _368;
  float _369;
  if (!_170) {
    float _308 = _304 + _295;
    float _309 = _305 + _296;
    float _310 = _306 + _297;
    bool _313 = ((cb2_019y) > 0.0f);
    _333 = 1.0f;
    do {
      if (_313) {
        float _317 = (cb2_015w) * (SV_Position.y);
        float _319 = (cb2_015z) * (SV_Position.x);
        uint _322 = (cb1_001x) + 0;
        float4 _324 = t1.Sample(s1[_322], float2(_319, _317));
        float _328 = (_324.x) + -1.0f;
        float _329 = (cb2_019y) * _328;
        float _330 = _329 + 1.0f;
        float _331 = saturate(_330);
        _333 = _331;
      }
      float _334 = _333 * _308;
      float _335 = _333 * _309;
      float _336 = _333 * _310;
      float _337 = _334 - _157;
      float _338 = _335 - _161;
      float _339 = _336 - _165;
      _348 = _337;
      _349 = _338;
      _350 = _339;
    } while (false);
  } else {
    float _341 = _295 - _157;
    float _342 = _341 + _304;
    float _343 = _296 - _161;
    float _344 = _343 + _305;
    float _345 = _297 - _165;
    float _346 = _345 + _306;
    _348 = _342;
    _349 = _344;
    _350 = _346;
  }
  float _351 = _350 * _249;
  float _352 = _349 * _249;
  float _353 = _348 * _249;
  float _354 = _353 + _157;
  float _355 = _352 + _161;
  float _356 = _351 + _165;
  int _359 = asint((cb2_025y));
  bool _360 = (_359 == 0);
  _367 = _354;
  _368 = _355;
  _369 = _356;
  if (!_360) {
    float4 _362 = t2.SampleLevel(s0, float3(_354, _355, _356), 0.0f);
    _367 = (_362.x);
    _368 = (_362.y);
    _369 = (_362.z);
  }
  float _370 = log2(_367);
  float _371 = log2(_368);
  float _372 = log2(_369);
  float _373 = _370 * 0.45454543828964233f;
  float _374 = _371 * 0.45454543828964233f;
  float _375 = _372 * 0.45454543828964233f;
  float _376 = exp2(_373);
  float _377 = exp2(_374);
  float _378 = exp2(_375);
  SV_Target.x = _376;
  SV_Target.y = _377;
  SV_Target.z = _378;
  SV_Target.w = _167;
  return SV_Target;
}
