// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:21 2026

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

SamplerState g_samplerLinear_Wrap_s : register(s10);
Texture2D<float4> inTexture : register(t8);


// 3Dmigoto declarations
#define cmp -


void main(
  float2 v0 : TEXCOORD0,
  float4 v1 : COLOR0,
  float4 v2 : COLOR1,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2,r3,r4;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.x = v2.w;
  r0.yz = v0.xy;
  r0.x = r0.x;
  r0.yz = r0.yz;
  r0.w = inTexture.Sample(g_samplerLinear_Wrap_s, v0.xy).w;
  r0.x = max(r0.w, r0.x);
  r1.xyz = float3(1,1,1) * r0.xxx;
  r1.xyz = float3(0,0,0) + r1.xyz;
  r2.xy = ddx_coarse(v0.xy);
  r2.zw = ddy_coarse(v0.xy);
  r3.xy = g_psObject.m_localParam[2].xy;
  r3.zw = g_psObject.m_localParam[2].zw;
  r4.xy = -r2.zw;
  r4.xy = float2(1.20000005,1.20000005) * r4.xy;
  r4.xy = max(r4.xy, r3.xy);
  r4.xy = min(r4.xy, r3.zw);
  r4.zw = -r2.xy;
  r4.zw = float2(1.20000005,1.20000005) * r4.zw;
  r4.zw = max(r4.zw, r3.xy);
  r4.zw = min(r4.zw, r3.zw);
  r2.xy = float2(1.20000005,1.20000005) * r2.xy;
  r2.xy = max(r2.xy, r3.xy);
  r2.xy = min(r2.xy, r3.zw);
  r2.zw = float2(1.20000005,1.20000005) * r2.zw;
  r2.zw = max(r2.zw, r3.xy);
  r2.zw = min(r2.zw, r3.zw);
  r3.xy = r4.xy + r0.yz;
  r0.w = inTexture.Sample(g_samplerLinear_Wrap_s, r3.xy).w;
  r3.xy = r4.zw + r0.yz;
  r3.x = inTexture.Sample(g_samplerLinear_Wrap_s, r3.xy).w;
  r2.xy = r2.xy + r0.yz;
  r2.x = inTexture.Sample(g_samplerLinear_Wrap_s, r2.xy).w;
  r0.yz = r2.zw + r0.yz;
  r0.y = inTexture.Sample(g_samplerLinear_Wrap_s, r0.yz).w;
  r0.z = cmp(0 < r0.w);
  if (r0.z != 0) {
    r0.z = 1;
  } else {
    r0.z = 0;
  }
  r2.y = cmp(0 < r3.x);
  if (r2.y != 0) {
    r0.z = 1 + r0.z;
  }
  r2.y = cmp(0 < r2.x);
  if (r2.y != 0) {
    r0.z = 1 + r0.z;
  }
  r2.y = cmp(0 < r0.y);
  if (r2.y != 0) {
    r0.z = 1 + r0.z;
  }
  r0.w = r3.x + r0.w;
  r0.w = r0.w + r2.x;
  r0.y = r0.w + r0.y;
  r0.w = r0.x + r0.y;
  r0.w = cmp(r0.w >= 9.99999975e-05);
  r0.w = r0.w ? 1 : 0;
  r0.y = r0.y / r0.z;
  r0.z = -r0.y;
  r0.y = max(r0.y, r0.z);
  r0.y = log2(r0.y);
  r0.y = 0.400000006 * r0.y;
  r0.y = exp2(r0.y);
  r0.x = cmp(0 < r0.x);
  r1.w = r0.x ? 1 : r0.y;
  r0.xyzw = r1.xyzw * r0.wwww;
  r0.xyzw = r0.xyzw;
  o0.xyzw = r0.xyzw;

  o0 = saturate(o0);
  return;
}