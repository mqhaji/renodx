// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:25 2026

cbuffer cPSSystem : register(b0)
{

  struct
  {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem : packoffset(c0);

}

SamplerState g_samplerPoint_Wrap_s : register(s8);
Texture2D<float4> inTexture : register(t0);


// 3Dmigoto declarations
#define cmp -


void main(
  float2 v0 : TEXCOORD0,
  out float4 o0 : SV_Target0,
  out float oDepth : SV_Depth)
{
  float4 r0;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = v0.xy;
  r0.xy = r0.xy;
  r0.z = -r0.y;
  r0.xy = float2(0.5,0.5) * r0.xz;
  r0.xy = float2(0.5,0.5) + r0.xy;
  r0.zw = g_psSystem.m_renderBuffer.zw * g_psSystem.m_renderInfo.xy;
  r0.xy = r0.xy * r0.zw;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = float2(0,0) + r0.xy;
  r0.x = inTexture.Sample(g_samplerPoint_Wrap_s, r0.xy).x;
  r0.x = r0.x;
  r0.x = r0.x;
  oDepth = r0.x;
  o0.xyzw = float4(0,0,0,1);
  return;
}