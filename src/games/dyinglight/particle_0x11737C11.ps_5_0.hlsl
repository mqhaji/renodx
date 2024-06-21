#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Sat May 25 22:38:54 2024
Texture2D<float4> t3 : register(t3);

Texture2D<float4> t2 : register(t2);

Texture2D<float4> t1 : register(t1);

Texture2D<float4> t0 : register(t0);

SamplerState s3_s : register(s3);

SamplerState s2_s : register(s2);

SamplerState s1_s : register(s1);

SamplerState s0_s : register(s0);

cbuffer cb2 : register(b2)
{
  float4 cb2[15];
}




// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_POSITION0,
  float4 v1 : TEXCOORD0,
  out float4 o0 : SV_TARGET0)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = t1.Sample(s0_s, v1.zw).xy;
  r0.z = dot(r0.xy, r0.xy);
  r0.z = cmp(0.5 < r0.z);
  if (r0.z != 0) {
    r1.xyzw = t3.SampleLevel(s2_s, v1.zw, 0).xyzw;
    r0.zw = t2.Sample(s1_s, v1.zw).xy;
    r0.zw = cb2[14].zw * r0.zw;
    r0.z = dot(r0.zw, r0.zw);
    r0.w = sqrt(r0.z);
    r0.z = cmp(0 < r0.z);
    r2.y = r0.z ? r0.w : 9.99999975e-006;
    r3.y = 1 / r2.y;
    r4.xyz = r3.yyy * r1.xyz;
    r0.z = t0.SampleLevel(s3_s, v1.zw, 0).x;
    r0.zw = r0.zz * cb2[8].xy + cb2[8].zw;
    r0.z = r0.z / -r0.w;
    r2.zw = float2(-0.444444507,-0.444444507) * r0.xy;
    r5.xyz = r4.xyz;
    r3.zw = r2.zw;
    r0.w = r3.y;
    r4.w = 0;
    while (true) {
      r5.w = cmp((int)r4.w >= 8);
      if (r5.w != 0) break;
      r6.xy = v1.xy + r3.zw;
      r6.xy = round(r6.xy);
      r6.zw = cb2[14].xy * r6.xy;
      r7.xy = t2.SampleLevel(s1_s, r6.zw, 0).xy;
      r7.xy = cb2[14].zw * r7.xy;
      r5.w = dot(r7.xy, r7.xy);
      r7.x = sqrt(r5.w);
      r5.w = cmp(0 < r5.w);
      r2.x = r5.w ? r7.x : 9.99999975e-006;
      r3.x = 1 / r2.x;
      r6.xy = v1.xy + -r6.xy;
      r5.w = dot(r6.xy, r6.xy);
      r6.x = sqrt(r5.w);
      r5.w = cmp(0 < r5.w);
      r5.w = r5.w ? r6.x : 9.99999975e-006;
      r6.x = t0.SampleLevel(s3_s, r6.zw, 0).x;
      r6.xy = r6.xx * cb2[8].xy + cb2[8].zw;
      r6.x = r6.x / -r6.y;
      r6.y = abs(r6.x) + -abs(r0.z);
      r7.x = saturate(-r6.y * 100 + 1);
      r6.x = -abs(r6.x) + abs(r0.z);
      r7.y = saturate(-r6.x * 100 + 1);
      r6.xy = -r5.ww * r3.xy + float2(1,1);
      r6.xy = max(float2(0,0), r6.xy);
      r3.x = dot(r7.xy, r6.xy);
      r6.xy = float2(0.0999999642,0.0999999642) * r2.xy;
      r7.xy = -r2.xy * float2(0.949999988,0.949999988) + r5.ww;
      r6.xy = float2(1,1) / r6.xy;
      r6.xy = saturate(r7.xy * r6.xy);
      r7.xy = r6.xy * float2(-2,-2) + float2(3,3);
      r6.xy = r6.xy * r6.xy;
      r6.xy = -r7.xy * r6.xy + float2(1,1);
      r2.x = dot(r6.xx, r6.yy);
      r2.x = r3.x + r2.x;
      r2.x = max(0, r2.x);
      r0.w = r2.x + r0.w;
      r6.xyz = t3.SampleLevel(s2_s, r6.zw, 0).xyz;
      r5.xyz = r2.xxx * r6.xyz + r5.xyz;
      r3.zw = r0.xy * float2(0.111111,0.111111) + r3.zw;
      r4.w = (int)r4.w + 1;
    }
    r0.xyz = r5.xyz / r0.www;
    r0.w = min(1, r0.w);
    r0.xyz = r0.xyz + -r1.xyz;
    o0.xyz = r0.www * r0.xyz +  r1.xyz; // r1.xyz = particle effect
    o0.w = r1.w;
  } else {
    if (-1 != 0) discard;
    o0.xyzw = float4(0,0,0,0);
  }
  return;
}