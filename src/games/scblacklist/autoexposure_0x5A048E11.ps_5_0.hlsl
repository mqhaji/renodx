#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Thu May 23 03:04:28 2024

cbuffer CB_PS_LuminanceAdapt : register(b7)
{
  float4 cLumBlend : packoffset(c0);
  float3 cLumTerms : packoffset(c1);
}

SamplerState sLum1x1_s : register(s0);
SamplerState sTonemapOld_s : register(s1);
SamplerState sCumulative_s : register(s2);
Texture2D<float4> sLum1x1 : register(t0);
Texture2D<float4> sCumulative : register(t1);
Texture2D<float4> sTonemapOld : register(t2);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.x = sLum1x1.Sample(sLum1x1_s, float2(0,0)).z;
  r0.y = sCumulative.Sample(sCumulative_s, float2(1,0.5)).x;
  r0.y = cLumTerms.y * r0.y;
  r1.y = 0.5;
  r1.xzw = float3(0,1,0);
  r0.z = 0;
  while (true) {
    r0.w = cmp((int)r0.z >= 10);
    if (r0.w != 0) break;
    r0.w = r1.w + r1.z;
    r1.x = 0.5 * r0.w;
    r0.w = sCumulative.Sample(sCumulative_s, r1.xy).x;
    r0.w = cmp(r0.w < r0.y);
    r1.zw = r0.ww ? r1.zx : r1.xw;
    r0.z = (int)r0.z + 1;
  }
  r0.w = cLumTerms.z * r1.x;
  r0.x = min(r0.w, r0.x);
  r0.x = max(cLumBlend.x, r0.x);
  r0.x = min(cLumBlend.y, r0.x);
  r1.x = sTonemapOld.Sample(sTonemapOld_s, float2(0,0)).z;
  r0.x = -r1.x + r0.x;
  r0.yz = cLumBlend.zz * r0.xx + r1.xx;
  r0.x = -1 + r0.z;
  r0.x = cLumTerms.x * r0.x + 1;
  o0.x = 1 / r0.x;
  o0.yzw = r0.yzw;
  return;
}