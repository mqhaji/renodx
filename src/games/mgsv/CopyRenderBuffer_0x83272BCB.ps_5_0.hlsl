// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:28 2026

cbuffer cPSObject : register(b5)
{

  struct
  {
    float4x4 m_viewWorld;
    float4x4 m_world;
    float4 m_useWeightCount;
    float4 m_localParam[4];
  } g_psObject : packoffset(c0);

}

SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inImage : register(t0);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = v1.xy;
  r0.xy = r0.xy;
  r0.z = -r0.y;
  r0.xy = float2(0.5,0.5) * r0.xz;
  r0.xy = float2(0.5,0.5) + r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.zw = g_psObject.m_localParam[3].xy;
  r1.xy = g_psObject.m_localParam[3].zw;
  r0.xy = r1.xy * r0.xy;
  r0.xy = r0.zw + r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xyzw = inImage.Sample(g_samplerLinear_Clamp_s, r0.xy).xyzw;
  r0.xyzw = r0.xyzw;
  o0.xyzw = r0.xyzw;

  o0 = max(0, o0);
  return;
}