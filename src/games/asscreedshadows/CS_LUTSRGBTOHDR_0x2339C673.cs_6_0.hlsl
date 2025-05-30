#include "./shared.h"

cbuffer _14_16 : register(b0, space5) {
  float4 _16_m0[1] : packoffset(c0);
};

RWTexture3D<float4> _8 : register(u0, space5);
RWTexture3D<float4> _9 : register(u1, space5);

static uint3 gl_GlobalInvocationID;
struct SPIRV_Cross_Input {
  uint3 gl_GlobalInvocationID : SV_DispatchThreadID;
};

void comp_main() {
  float _41 = clamp(float(gl_GlobalInvocationID.x) * 0.0322580635547637939453125f, 0.0f, 1.0f);
  float _42 = clamp(float(gl_GlobalInvocationID.y) * 0.0322580635547637939453125f, 0.0f, 1.0f);
  float _43 = clamp(float(gl_GlobalInvocationID.z) * 0.0322580635547637939453125f, 0.0f, 1.0f);
#if RENODX_UI_GAMMA_CORRECTION  // 2.2 gamma
  float _71 = pow(_41, 2.2f);
  float _72 = pow(_42, 2.2f);
  float _73 = pow(_43, 2.2f);
  float _84 = _71;
  float _85 = _72;
  float _86 = _73;
#else  // sRGB gamma + contrast boost
  float _71 = (_41 <= 0.040449999272823333740234375f) ? (_41 * 0.077399380505084991455078125f) : exp2(log2((_41 + 0.054999999701976776123046875f) * 0.947867333889007568359375f) * 2.400000095367431640625f);
  float _72 = (_42 <= 0.040449999272823333740234375f) ? (_42 * 0.077399380505084991455078125f) : exp2(log2((_42 + 0.054999999701976776123046875f) * 0.947867333889007568359375f) * 2.400000095367431640625f);
  float _73 = (_43 <= 0.040449999272823333740234375f) ? (_43 * 0.077399380505084991455078125f) : exp2(log2((_43 + 0.054999999701976776123046875f) * 0.947867333889007568359375f) * 2.400000095367431640625f);
  float _84 = exp2(log2(_71) * _16_m0[0u].y);
  float _85 = exp2(log2(_72) * _16_m0[0u].y);
  float _86 = exp2(log2(_73) * _16_m0[0u].y);
#endif
  float _121 = exp2(log2(abs(mad(_73, 0.04331304132938385009765625f, mad(_72, 0.3292830288410186767578125f, _71 * 0.627403795719146728515625f)) * 9.9999997473787516355514526367188e-05f)) * 0.1593017578125f);
  float _138 = exp2(log2(abs(mad(_73, 0.0113622955977916717529296875f, mad(_72, 0.919540584087371826171875f, _71 * 0.0690973103046417236328125f)) * 9.9999997473787516355514526367188e-05f)) * 0.1593017578125f);
  float _151 = exp2(log2(abs(mad(_73, 0.89559519290924072265625f, mad(_72, 0.08801329135894775390625f, _71 * 0.01639144122600555419921875f)) * 9.9999997473787516355514526367188e-05f)) * 0.1593017578125f);
  float _160 = _16_m0[0u].x * 9.9999997473787516355514526367188e-05f;

  if (RENODX_GRAPHICS_WHITE_NITS != 0.f) {
    _160 *= (float(RENODX_GRAPHICS_WHITE_NITS) / 269.f);  // Scale UI Brightness
  }

  float _165 = exp2(log2(abs(_160 * mad(_86, 0.04331304132938385009765625f, mad(_85, 0.3292830288410186767578125f, _84 * 0.627403795719146728515625f)))) * 0.1593017578125f);
  float _178 = exp2(log2(abs(_160 * mad(_86, 0.0113622955977916717529296875f, mad(_85, 0.919540584087371826171875f, _84 * 0.0690973103046417236328125f)))) * 0.1593017578125f);
  float _191 = exp2(log2(abs(_160 * mad(_86, 0.89559519290924072265625f, mad(_85, 0.08801329135894775390625f, _84 * 0.01639144122600555419921875f)))) * 0.1593017578125f);
  _8[uint3(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y, gl_GlobalInvocationID.z)] = float4(exp2(log2(((_121 * 18.8515625f) + 0.8359375f) / ((_121 * 18.6875f) + 1.0f)) * 78.84375f), exp2(log2(((_138 * 18.8515625f) + 0.8359375f) / ((_138 * 18.6875f) + 1.0f)) * 78.84375f), exp2(log2(((_151 * 18.8515625f) + 0.8359375f) / ((_151 * 18.6875f) + 1.0f)) * 78.84375f), 1.0f);
  _9[uint3(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y, gl_GlobalInvocationID.z)] = float4(exp2(log2(((_165 * 18.8515625f) + 0.8359375f) / ((_165 * 18.6875f) + 1.0f)) * 78.84375f), exp2(log2(((_178 * 18.8515625f) + 0.8359375f) / ((_178 * 18.6875f) + 1.0f)) * 78.84375f), exp2(log2(((_191 * 18.8515625f) + 0.8359375f) / ((_191 * 18.6875f) + 1.0f)) * 78.84375f), 1.0f);
}

[numthreads(16, 16, 1)]
void main(SPIRV_Cross_Input stage_input) {
  gl_GlobalInvocationID = stage_input.gl_GlobalInvocationID;
  comp_main();
}
