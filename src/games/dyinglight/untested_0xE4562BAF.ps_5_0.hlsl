#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Sat May 25 22:39:45 2024
Texture2D<float4> t1 : register(t1);

Texture2D<float4> t0 : register(t0);

SamplerState s1_s : register(s1);

SamplerState s0_s : register(s0);

cbuffer cb0 : register(b0)
{
  float4 cb0[3];
}




// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_POSITION0,
  float2 v1 : TEXCOORD0,
  float2 w1 : TEXCOORD4,
  float4 v2 : TEXCOORD1,
  float4 v3 : TEXCOORD2,
  float3 v4 : TEXCOORD3,
  uint v5 : SV_ISFRONTFACE0,
  out float4 o0 : SV_TARGET0,
  out float4 o1 : SV_TARGET1,
  out float4 o2 : SV_TARGET2,
  out float2 o3 : SV_TARGET3)
{
  float4 r0,r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = t0.Sample(s0_s, v1.xy).xyzw;
  r0.w = -cb0[0].w + r0.w;
  r0.w = cmp(r0.w < 0);
  if (r0.w != 0) discard;
  r1.xyz = max(0, r0.xyz * float3(2,2,2) + float3(-1,-1,-1));
  o1.xyz = r0.xyz;
  r0.x = dot(float3(0.212500006,0.715399981,0.0720999986), r1.xyz);
  r0.x = cb0[0].z * r0.x;
  r0.x = log2(abs(r0.x));
  r0.x = 0.454545468 * r0.x;
  o0.xyz = exp2(r0.xxx);
  o0.w = max(0, cb0[0].x + cb0[0].y); //  o0.w = saturate(cb0[0].x + cb0[0].y); // not sure if it has an effect
  o1.w = 0;
  r0.xy = t1.Sample(s1_s, v1.xy).yw;
  r0.xy = r0.yx * cb0[2].xx + cb0[2].yy;
  r1.xyzw = v2.xyzx * r0.yyyy;
  r0.xyzw = r0.xxxx * v3.xyzx + r1.xyzw;
  r0.xyzw = v4.xyzx + r0.xyzw;
  r1.x = dot(r0.yzw, r0.yzw);
  r1.x = rsqrt(r1.x);
  r0.xyzw = r1.xxxx * r0.xyzw;
  r0.xyzw = v5.xxxx ? r0.xyzw : -r0.wyzw;
  o2.xyzw = r0.xyzw * cb0[1].xxxy + cb0[1].xxxz;
  o3.xy = w1.xy;
  return;
}