#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Fri Jul 26 00:52:42 2024

cbuffer cb_g_Bink : register(b5)
{

  struct
  {
    float4x3 YUVToRGB;
    float4 m_BinkSettings;
  } g_Bink : packoffset(c0);

}

SamplerState s_TrilinearClamp_s : register(s12);
Texture2D<float4> t_g_Ytex : register(t0);
Texture2D<float4> t_g_cRtex : register(t1);
Texture2D<float4> t_g_cBtex : register(t2);
Texture2D<float4> t_g_Atex : register(t3);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.x = t_g_Atex.Sample(s_TrilinearClamp_s, v1.xy).x;
  o0.w = g_Bink.m_BinkSettings.x * r0.x;
  r0.x = t_g_Ytex.Sample(s_TrilinearClamp_s, v1.xy).x;
  r0.y = t_g_cRtex.Sample(s_TrilinearClamp_s, v1.xy).x;
  r0.z = t_g_cBtex.Sample(s_TrilinearClamp_s, v1.xy).x;
  r0.w = 1;
  o0.x = dot(r0.xyzw, g_Bink.YUVToRGB._m00_m10_m20_m30);
  o0.y = dot(r0.xyzw, g_Bink.YUVToRGB._m01_m11_m21_m31);
  o0.z = dot(r0.xyzw, g_Bink.YUVToRGB._m02_m12_m22_m32);

  o0.rgb = saturate(o0.rgb);
  o0.rgb = injectedData.toneMapGammaCorrection
               ? pow(o0.rgb, 2.2f)
               : renodx::color::bt709::from::SRGB(o0.rgb);
  if (injectedData.toneMapType <= 1.f) {
    o0.rgb *= injectedData.toneMapGameNits / 80.f;
  } else {
    float scaling = injectedData.toneMapPeakNits / injectedData.toneMapGameNits;
    float videoPeak = 203.f * scaling;
    o0.rgb = renodx::tonemap::inverse::bt2446a::BT709(o0.rgb, 100.f / scaling, videoPeak);
    o0.rgb /= videoPeak;  // Normalize to 1.0
    o0.rgb *= injectedData.toneMapPeakNits / 80.f;
  }
  return;
}