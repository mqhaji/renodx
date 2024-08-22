#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Fri Jul 26 00:52:44 2024

cbuffer cb_g_ColorBalance : register(b5)
{

  struct
  {
    float4 m_PreLutScale;
    float4 m_PreLutOffset;
    float4 m_DepthControl;
  } g_ColorBalance : packoffset(c0);

}

SamplerState s_PointClamp_s : register(s10);
SamplerState s_TrilinearClamp_s : register(s12);
Texture2D<float4> t_FrameBuffer : register(t0);
Texture3D<float4> t_ColorBalance3DTexture : register(t1);


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

  r0.xyz = t_FrameBuffer.Sample(s_PointClamp_s, v1.xy).xyz;
  r0.xyz = r0.xyz * g_ColorBalance.m_PreLutScale.xyz + g_ColorBalance.m_PreLutOffset.xyz;
  r0.xyz = t_ColorBalance3DTexture.Sample(s_TrilinearClamp_s, r0.xyz).xyz;
  o0.xyz = r0.xyz;

  float3 signs = sign(o0.rgb);
  o0.rgb = abs(o0.rgb);
  o0.rgb = (injectedData.toneMapGammaCorrection
                ? pow(o0.rgb, 2.2f)
                : renodx::color::bt709::from::SRGB(o0.rgb));
  o0.rgb *= signs;
  o0.rgb *= injectedData.toneMapGameNits / 80.f;
  return;
}