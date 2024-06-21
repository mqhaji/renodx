#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Sat May 25 22:39:25 2024
Texture2D<float4> t1 : register(t1);

Texture2D<float4> t0 : register(t0);

SamplerState s1_s : register(s1);

SamplerState s0_s : register(s0);




// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_POSITION0,
  float4 v1 : TEXCOORD0,
  float2 v2 : TEXCOORD1,
  out float4 o0 : SV_TARGET0)
{
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = t0.Sample(s0_s, v1.xy).xyzw;
  r1.xyzw = t0.Sample(s0_s, v1.zy).xyzw;
  r2.x = t1.SampleLevel(s1_s, float2(0,0), 0).x;
  r2.x = 0.25 * r2.x; // strength of dirty lens effect
  r1.xyzw = r2.xxxx * r1.xyzw;
  r0.xyzw = r0.xyzw * r2.xxxx + r1.xyzw;
  r1.xyzw = t0.Sample(s0_s, v1.xw).xyzw;
  r0.xyzw = r1.xyzw * r2.xxxx + r0.xyzw;
  r1.xyzw = t0.Sample(s0_s, v1.zw).xyzw;
  r0.xyzw = r1.xyzw * r2.xxxx + r0.xyzw;
  r0.xyzw = r0.xyzw * float4(2,2,2,2) + float4(-2,-2,-2,-2);
  r0.xyzw = r0.xyzw;  //  r0.xyzw = max(float4(0,0,0,0), r0.xyzw);
  r1.xy = v2.xy * v2.xy;
  r1.xy = -r1.xy * r1.xy + float2(1,1);
  r1.x = r1.x * r1.y;
  o0.xyzw = r1.xxxx * r0.xyzw;
  return;
}