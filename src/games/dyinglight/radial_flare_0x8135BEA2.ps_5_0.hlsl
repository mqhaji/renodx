#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Sat May 25 22:39:21 2024
Texture2D<float4> t0 : register(t0);

SamplerState s0_s : register(s0);

cbuffer cb0 : register(b0)
{
  float4 cb0[1];
}




// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_POSITION0,
  float4 v1 : TEXCOORD0,
  float4 v2 : TEXCOORD1,
  out float4 o0 : SV_TARGET0)
{
  float4 r0,r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = v2.zw * v2.zw;
  r0.xy = -r0.xy * r0.xy + float2(1,1);
  r0.x = r0.x * r0.y;
  r0.y = cmp(r0.x < 0);
  if (r0.y != 0) discard;
  r0.y = dot(v2.xy, v2.xy);
  r0.z = sqrt(r0.y);
  r0.y = rsqrt(r0.y);
  r1.xyzw = v2.xyxx * r0.yyyy;
  r1.xyzw = r1.xyzw * float4(0.414213568,0.414213568,0.414213568,0.414213568) + v1.xyzw;
  r0.y = r0.z * 2 + -1;
  r0.y = 1 + -abs(r0.y);
  r0.y = max(0, r0.y);
  r0.z = r0.y * -2 + 3;
  r0.y = r0.y * r0.y;
  r0.y = r0.z * r0.y;
  r0.y = min(1, r0.y);
  r0.y = r0.y * r0.y;
  r0.y = r0.y * r0.y;
  r0.y = r0.y * r0.y;
  r0.x = r0.x * r0.y;
  r0.y = t0.Sample(s0_s, r1.zy).x;
  r0.z = t0.Sample(s0_s, r1.xy).y;
  r0.w = t0.Sample(s0_s, r1.wy).z;
  r0.xyz = r0.yzw * r0.xxx;
  o0.w = r0.w;
  o0.xyz = (cb0[0].xyz) * r0.xyz;
  return;
}