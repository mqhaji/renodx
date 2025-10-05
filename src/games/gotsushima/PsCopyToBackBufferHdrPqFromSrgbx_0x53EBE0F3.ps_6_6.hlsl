#include "./shared.h"

cbuffer RootSrtCbv : register(b14, space0) {
  int bufferIndex : packoffset(c0);
  int unknown1 : packoffset(c0);
}

// Texture2D<float4> ResourceDescriptorHeap[] : register(t0, space0); // don't declare
ByteAddressBuffer bufferT1 : register(t1, space1);

SamplerState _22 : register(s12, space0);
SamplerState _23 : register(s1, space0);

static float2 TEXCOORD;
static float4 SV_Target;

struct SPIRV_Cross_Input {
  float4 SV_Position : SV_Position;  // missing from decomp
  float2 TEXCOORD : TEXCOORD0;
};

struct SPIRV_Cross_Output {
  float4 SV_Target : SV_Target0;
};

float3 ApplyPQApproximation(float3 bt2020_color) {
  float3 bt2020_sqrt = sqrt(bt2020_color);
  return (((((((bt2020_sqrt * 0.16258220374584197998046875f) + 8.06964778900146484375f) * bt2020_sqrt) + 3.5209205150604248046875f) * bt2020_sqrt) + 0.000487781013362109661102294921875f) / ((((bt2020_sqrt * 7.96455097198486328125f) + 10.5308475494384765625f) * bt2020_sqrt) + 1.0f));
}

namespace rec709 {
static const float REFERENCE_WHITE = 100.f;

float OETF(float channel) {
  return (channel < 0.018f)
             ? (channel * 4.5f)
             : (1.099f * pow(channel, 0.45f) - 0.099f);
}
float InverseOETF(float channel) {
  return (channel < 0.018f)
             ? (channel / 4.5f)
             : pow((channel + 0.099f) / 1.099f, 1 / 0.45f);
}

}  // namespace rec709

void frag_main() {
  SV_Target.x = 0.0f;
  SV_Target.y = 0.0f;
  SV_Target.z = 0.0f;
  SV_Target.w = 0.0f;
  // uint2 _49 = _19_m0[0u];

  float2 _64;

  uint _50 = bufferIndex;  // _49.x;
  uint index32 = _50 + 32u;
  uint _56 = (_50 + 64u);
  _64 = bufferT1.Load<float2>(_56);

  SV_Target.x = 0.0f;
  SV_Target.y = 0.0f;
  SV_Target.z = 0.0f;
  SV_Target.w = 0.0f;

  uint index96 = _50 + 96u;

  uint value00 = bufferT1.Load<uint>(_50);

  Texture2D<float4> res0 = ResourceDescriptorHeap[value00];

  // float4 _82 = _9[bufferT1.Load(_50).x].Sample(_22, float2(TEXCOORD.x, TEXCOORD.y));

  float4 _82 = res0.Sample(_22, float2(TEXCOORD.x, TEXCOORD.y));
  float3 srgbInputColor = _82.rgb;

#if 1  // controlled by brightness slider
  float3 scaled_color = (((((((((bufferT1.Load<float>(_50 + 112u) * _82.rgb) + bufferT1.Load<float>(_50 + 108u))
                               * _82.rgb)
                              + bufferT1.Load<float>(_50 + 104u))
                             * _82.rgb)
                            + bufferT1.Load<float>(_50 + 100u))
                           * _82.rgb)
                          + bufferT1.Load<float>(index96))
                         * _82.rgb)
                        / ((((((bufferT1.Load<float>(_50 + 124u) * _82.rgb)
                               + bufferT1.Load<float>(_50 + 120u))
                              * _82.rgb)
                             + bufferT1.Load<float>(_50 + 116u))
                            * _82.rgb)
                           + 1.0f);
#else
  float3 scaled_color = renodx::color::srgb::DecodeSafe(srgbInputColor) * (10.f / 3.f);
  scaled_color = renodx::color::correct::GammaSafe(scaled_color, false, 2.4f);
  SV_Target.rgb = renodx::color::bt2020::from::BT709(scaled_color);
  SV_Target.rgb = renodx::color::pq::EncodeSafe(SV_Target.rgb, 100.f);
  SV_Target.w = 1.0f;
  return;
#endif

#if 0
  scaled_color = renodx::math::SignPow(scaled_color, 1.f / 2.4f);
  scaled_color = renodx::math::SignPow(scaled_color, 2.2f);
  // scaled_color.r = rec709::InverseOETF(scaled_color.r);
  // scaled_color.g = rec709::InverseOETF(scaled_color.g);
  // scaled_color.b = rec709::InverseOETF(scaled_color.b);
  // scaled_color = renodx::color::correct::GammaSafe(scaled_color);
#endif
  float3 bt2020_color = mul(
      float3x3(
          0.627403736114501953125f, 0.329281747341156005859375f, 0.043313510715961456298828125f,
          0.06909702718257904052734375f, 0.9195404052734375f, 0.01136207766830921173095703125f,
          0.01639144308865070343017578125f, 0.088013507425785064697265625f, 0.895595014095306396484375f),
      scaled_color);

  float3 pq_color;
#if 0
  pq_color = ApplyPQApproximation(bt2020_color);
#else
  pq_color = renodx::color::pq::EncodeSafe(bt2020_color, RENODX_DIFFUSE_WHITE_NITS);
#endif
  uint resIndex = bufferT1.Load<uint>(index32);
  Texture2D<float> res1 = (Texture2D<float>)ResourceDescriptorHeap[resIndex];
  float resSample = res1.SampleLevel(_23, float2((_64.x * TEXCOORD.x) + 0.5f, (_64.y * TEXCOORD.y) + 0.5f), 0.0f).x;
  float _260 = (resSample * 0.0009775171f) + (-0.0004887586f);

  SV_Target.xyz = pq_color + _260;
  SV_Target.w = 1.0f;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input) {
  TEXCOORD = stage_input.TEXCOORD;
  frag_main();
  SPIRV_Cross_Output stage_output;
  stage_output.SV_Target = SV_Target;
  return stage_output;
}
