// ---- Created with 3Dmigoto v1.4.1 on Wed Jan  8 23:25:53 2025

cbuffer viewConstants : register(b2) {
  float3 time : packoffset(c0);
  float1 vc_pad0_ : packoffset(c0.w);
  float4 screenSize : packoffset(c1);
  float4x4 viewMatrix : packoffset(c2);
  float4x4 projMatrix : packoffset(c6);
  float4x4 viewProjMatrix : packoffset(c10);
  float4x4 crViewProjMatrix : packoffset(c14);
  float4x4 prevViewProjMatrix : packoffset(c18);
  float4x4 crPrevViewProjMatrix : packoffset(c22);
  float4x3 normalBasisTransforms[6] : packoffset(c26);
  float4 projectionKxKyKzKw : packoffset(c44);
  float3 cameraPos : packoffset(c45);
  float1 vc_pad8_ : packoffset(c45.w);
  float3 prevCameraPos : packoffset(c46);
  float1 vc_pad9_ : packoffset(c46.w);
  float3 transparentStartAndSlopeAndClamp : packoffset(c47);
  float1 vc_pad10_ : packoffset(c47.w);
  float4 transparentCurve : packoffset(c48);
  float4 exposureMultipliers : packoffset(c49);
  float4 fogParams : packoffset(c50);
  float4 fogForwardScatteringParamsLuminanceScaleFogEnable : packoffset(c51);
  float4 fogForwardScatteringColorPresence : packoffset(c52);
  float4 fogForwardScatteringSunDir : packoffset(c53);
  float4 fogCoefficients : packoffset(c54);
  float4 fogColorCoefficients : packoffset(c55);
  float4 fogColor : packoffset(c56);
  float4 fogStartDistance : packoffset(c57);
  float4 fogHeightFogCoefficients : packoffset(c58);
  float4 fogMiscParam : packoffset(c59);
  float4 fogEnabledModeSkyModeUseLight2 : packoffset(c60);
  float4 fogSkyGradientUVRanges : packoffset(c61);
  float4 mieGMaxDistanceTransTexDepthMieCoef : packoffset(c62);
  float3 light0Dir : packoffset(c63);
  float1 vc_pad11_ : packoffset(c63.w);
  float3 light1Dir : packoffset(c64);
  float1 vc_pad12_ : packoffset(c64.w);
  float3 rayleighScatteringCoefficient : packoffset(c65);
  float1 vc_pad13_ : packoffset(c65.w);
  float3 rayleighPolarizationFilter : packoffset(c66);
  float1 vc_pad14_ : packoffset(c66.w);
  float3 miePolarizationFilter : packoffset(c67);
  float1 vc_pad15_ : packoffset(c67.w);
  float4 heightFogColorMulMinTransmittance : packoffset(c68);
  float3 heightFogColorAdd : packoffset(c69);
  float vc_pad16_ : packoffset(c69.w);
}

cbuffer externalConstants : register(b1) {
  float3 external_Color : packoffset(c0);
  float vc_pad0 : packoffset(c0.w);
  float external_OverrideExposureFactor : packoffset(c1);
  float3 vc_pad1 : packoffset(c1.y);
}

SamplerState sampler0_s : register(s0);
Texture2D<float4> texture_Texture2 : register(t1);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position0,
    float4 v1: TEXCOORD0,
    out float4 o0: SV_Target0,
    out float4 o1: SV_Target1) {
  float4 r0, r1;

  o1.xyzw = float4(0, 0, 0, 0);
  r0.xyz = texture_Texture2.Sample(sampler0_s, v1.xy).xyz;
  r0.xyz = external_Color.xyz * r0.xyz;
  r0.xyz = v1.zzz * r0.xyz;
  r0.w = saturate(external_OverrideExposureFactor);
  r1.x = 1 + -exposureMultipliers.w;
  r0.w = r0.w * r1.x + exposureMultipliers.w;
  r0.xyz = r0.xyz * r0.www;
  o0.xyz = exposureMultipliers.zzz * r0.xyz;  // lens flare
  o0.w = 0;
  return;
}
