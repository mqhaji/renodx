#include "./shared.h"

cbuffer _17_19 : register(b14, space0) {
  float4 _19_m0[1] : packoffset(c0);
};

// Texture2D<float4> ResourceDescriptorHeap[] : register(t0, space0); // Texture2D<float4> _9[] : register(t0, space0);
ByteAddressBuffer _13 : register(t1, space1);
SamplerState _22 : register(s11, space0);

static float4 SV_POSITION;
static float2 TEXCOORD;
static float4 SV_Target;

struct SPIRV_Cross_Input {
  float4 SV_POSITION : SV_POSITION;  // missing from decomp
  float2 TEXCOORD : TEXCOORD;
};

struct SPIRV_Cross_Output {
  float4 SV_Target : SV_Target0;
};

void frag_main() {
  SV_Target = float4(0.0f, 0.0f, 0.0f, 0.0f);

  uint baseOffset = asuint(_19_m0[0u].x);
  uint descriptorIndex = _13.Load<uint>(baseOffset);
  float scale = _13.Load<float>(baseOffset + 88u);

  Texture2D<float4> inputTexture = ResourceDescriptorHeap[descriptorIndex];
  float4 sampled = inputTexture.Sample(_22, TEXCOORD);

  float r = sampled.x * scale;
  float g = sampled.y * scale;
  float b = sampled.z * scale;

#if 1
  SV_Target = float4(r, g, b, 1.0f);
  SV_Target.rgb = renodx::color::srgb::DecodeSafe(SV_Target.rgb) * (10.f / 3.f);
  SV_Target.rgb = renodx::color::srgb::EncodeSafe(SV_Target.rgb);

  // SV_Target.rgb = renodx::color::correct::GammaSafe(SV_Target.rgb, false, 2.4f);
  // SV_Target.rgb = renodx::color::bt2020::from::BT709(SV_Target.rgb);
  // SV_Target.rgb = renodx::color::pq::EncodeSafe(SV_Target.rgb, 100.f);

  return;
#endif

  float coeff112 = _13.Load<float>(baseOffset + 112u);
  float coeff108 = _13.Load<float>(baseOffset + 108u);
  float coeff104 = _13.Load<float>(baseOffset + 104u);
  float coeff100 = _13.Load<float>(baseOffset + 100u);
  float coeff96 = _13.Load<float>(baseOffset + 96u);
  float coeff124 = _13.Load<float>(baseOffset + 124u);
  float coeff120 = _13.Load<float>(baseOffset + 120u);
  float coeff116 = _13.Load<float>(baseOffset + 116u);

  float numeratorR = (r * coeff112) + coeff108;
  numeratorR = (numeratorR * r) + coeff104;
  numeratorR = (numeratorR * r) + coeff100;
  numeratorR = (numeratorR * r) + coeff96;
  numeratorR *= r;

  float numeratorG = (g * coeff112) + coeff108;
  numeratorG = (numeratorG * g) + coeff104;
  numeratorG = (numeratorG * g) + coeff100;
  numeratorG = (numeratorG * g) + coeff96;
  numeratorG *= g;

  float numeratorB = (b * coeff112) + coeff108;
  numeratorB = (numeratorB * b) + coeff104;
  numeratorB = (numeratorB * b) + coeff100;
  numeratorB = (numeratorB * b) + coeff96;
  numeratorB *= b;

  float denominatorR = (coeff124 * r) + coeff120;
  denominatorR = (denominatorR * r) + coeff116;
  denominatorR = (denominatorR * r) + 1.0f;

  float denominatorG = (coeff124 * g) + coeff120;
  denominatorG = (denominatorG * g) + coeff116;
  denominatorG = (denominatorG * g) + 1.0f;

  float denominatorB = (coeff124 * b) + coeff120;
  denominatorB = (denominatorB * b) + coeff116;
  denominatorB = (denominatorB * b) + 1.0f;

  float encodedR = numeratorR / denominatorR;
  float encodedG = numeratorG / denominatorG;
  float encodedB = numeratorB / denominatorB;

  float sqrtR = sqrt(mad(encodedB, 0.043313510715961456298828125f, mad(encodedG, 0.329281747341156005859375f, encodedR * 0.627403736114501953125f)));
  float sqrtG = sqrt(mad(encodedB, 0.01136207766830921173095703125f, mad(encodedG, 0.9195404052734375f, encodedR * 0.06909702718257904052734375f)));
  float sqrtB = sqrt(mad(encodedB, 0.895595014095306396484375f, mad(encodedG, 0.088013507425785064697265625f, encodedR * 0.01639144308865070343017578125f)));

  float pqNumeratorR = (((((sqrtR * 0.16258220374584197998046875f) + 8.06964778900146484375f) * sqrtR) + 3.5209205150604248046875f) * sqrtR) + 0.000487781013362109661102294921875f;
  float pqNumeratorG = (((((sqrtG * 0.16258220374584197998046875f) + 8.06964778900146484375f) * sqrtG) + 3.5209205150604248046875f) * sqrtG) + 0.000487781013362109661102294921875f;
  float pqNumeratorB = (((((sqrtB * 0.16258220374584197998046875f) + 8.06964778900146484375f) * sqrtB) + 3.5209205150604248046875f) * sqrtB) + 0.000487781013362109661102294921875f;

  float pqDenominatorR = (((sqrtR * 7.96455097198486328125f) + 10.5308475494384765625f) * sqrtR) + 1.0f;
  float pqDenominatorG = (((sqrtG * 7.96455097198486328125f) + 10.5308475494384765625f) * sqrtG) + 1.0f;
  float pqDenominatorB = (((sqrtB * 7.96455097198486328125f) + 10.5308475494384765625f) * sqrtB) + 1.0f;

  SV_Target.x = pqNumeratorR / pqDenominatorR;
  SV_Target.y = pqNumeratorG / pqDenominatorG;
  SV_Target.z = pqNumeratorB / pqDenominatorB;
  SV_Target.w = 1.0f;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input) {
  TEXCOORD = stage_input.TEXCOORD;
  frag_main();
  SPIRV_Cross_Output stage_output;
  stage_output.SV_Target = SV_Target;
  return stage_output;
}
