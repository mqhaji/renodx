#include "./shared.h"

// ---- Created with 3Dmigoto v1.4.1 on Fri May 23 23:01:17 2025
Texture2D<float4> t10 : register(t10);

Texture3D<float4> t9 : register(t9);

Texture2D<float4> t1 : register(t1);

Texture2D<float4> t0 : register(t0);

SamplerState s2_s : register(s2);

SamplerState s1_s : register(s1);

SamplerState s0_s : register(s0);

cbuffer cb0 : register(b0)
{
  float4 cb0[9];
}




// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2,r3,r4;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyz = t0.SampleLevel(s0_s, v1.xy, 0).xyz;


  // r0.xyz = max(float3(0,0,0), r0.xyz);
  r0.xyz = float3(100,100,100) * r0.xyz;

  o0.rgb = r0.rgb;
  o0.w = 1;
  return;

  r1.xyzw = asint(cb0[4].yyyy) & int4(8,1,16,2);
  if (r1.x != 0) {
    r2.xyzw = t1.SampleLevel(s2_s, v1.xy, 0).xyzw;
    r2.xyz = cb0[3].yyy * r2.xyz;
    r0.w = -r2.w * cb0[3].z + 1;
    r0.xyz = r0.xyz * r0.www + r2.xyz;
  }
  if (r1.y != 0) {
    r2.xyz = float3(0.00999999978,0.00999999978,0.00999999978) * r0.xyz;
    r2.xyz = log2(r2.xyz);
    r2.xyz = float3(0.159301758,0.159301758,0.159301758) * r2.xyz;
    r2.xyz = exp2(r2.xyz);
    r3.xyz = r2.xyz * float3(18.8515625,18.8515625,18.8515625) + float3(0.8359375,0.8359375,0.8359375);
    r2.xyz = r2.xyz * float3(18.6875,18.6875,18.6875) + float3(1,1,1);
    r2.xyz = r3.xyz / r2.xyz;
    r3.xyz = float3(-0.8359375,-0.8359375,-0.8359375) + r2.xyz;
    r3.xyz = max(float3(0,0,0), r3.xyz);


    r2.xyz = -r2.xyz * float3(18.6875,18.6875,18.6875) + float3(18.8515625,18.8515625,18.8515625);
    r2.xyz = r3.xyz / r2.xyz;
    r2.xyz = log2(r2.xyz);
    r2.xyz = float3(6.27739477,6.27739477,6.27739477) * r2.xyz;
    r2.xyz = exp2(r2.xyz);

    // r2.rgb = 0.01 * r0.rgb;
    // r2.rgb = renodx::color::pq::EncodeSafe(r2.rgb, 1.f);

    r3.xyz = float3(100,100,100) * r2.xyz;
    r4.xyz = float3(1292,1292,1292) * r2.xyz;
    r3.xyz = log2(r3.xyz);
    r3.xyz = float3(0.416666657,0.416666657,0.416666657) * r3.xyz;
    r3.xyz = exp2(r3.xyz);
    r3.xyz = r3.xyz * float3(1.05499995,1.05499995,1.05499995) + float3(-0.0549999997,-0.0549999997,-0.0549999997);
    r2.xyz = cmp(float3(3.13080018e-05,3.13080018e-05,3.13080018e-05) >= r2.xyz);
    r2.xyz = r2.xyz ? r4.xyz : r3.xyz;
    r2.xyz = float3(-0.5,-0.5,-0.5) + r2.xyz;
    r0.w = cb0[6].w * cb0[7].z + 1;
    r2.xyz = r2.xyz * r0.www + float3(0.5,0.5,0.5);
    r2.xyz = -cb0[6].www * cb0[7].www + r2.xyz;
    r0.w = dot(r2.xyz, float3(17.8824005,43.5161018,4.11934996));
    r1.x = dot(r2.xyz, float3(3.45565009,27.1553993,3.86714005));
    r1.y = dot(r2.xyz, float3(0.0299565997,0.184309006,1.46709001));
    r2.w = 0.801109016 * r1.x;
    r3.xy = float2(2.52810001,1.24827003) * r1.yy;
    r3.x = r1.x * 2.02343988 + -r3.x;
    r4.xyzw = float4(1,1,1,1) + -cb0[6].xyzw;
    r3.z = r4.x * r0.w;
    r3.x = r3.x * cb0[6].x + r3.z;
    r3.y = r0.w * 0.494206995 + r3.y;
    r1.xy = r4.yz * r1.xy;
    r1.x = r3.y * cb0[6].y + r1.x;
    r0.w = r0.w * -0.395913005 + r2.w;
    r0.w = r0.w * cb0[6].z + r1.y;
    r3.yzw = float3(-0.130504414,0.0540193282,-0.00412161462) * r1.xxx;
    r3.xyz = r3.xxx * float3(0.0809444487,-0.0102485335,-0.000365296932) + r3.yzw;
    r3.xyz = r0.www * float3(0.116721064,-0.113614708,0.693511426) + r3.xyz;
    r3.xyz = -r3.xyz + r2.xyz;
    r3.yz = r3.xx * float2(0.699999988,0.699999988) + r3.yz;
    r3.x = 0;
    r3.xyz = saturate(r3.xyz + r2.xyz);
    r2.xyz = r4.www * r2.xyz;
    r2.xyz = r3.xyz * cb0[6].www + r2.xyz;
    r2.xyz = float3(-0.5,-0.5,-0.5) + r2.xyz;
    r0.w = 1 + cb0[7].y;
    r2.xyz = r2.xyz * r0.www + float3(0.5,0.5,0.5);
    r0.w = cb0[8].x * cb0[6].w + cb0[7].x;
    r2.xyz = r2.xyz + r0.www;
    r3.xyz = float3(0.0773993805,0.0773993805,0.0773993805) * r2.xyz;
    r4.xyz = float3(0.0549999997,0.0549999997,0.0549999997) + r2.xyz;
    r4.xyz = float3(0.947867334,0.947867334,0.947867334) * r4.xyz;
    r4.xyz = log2(r4.xyz);
    r4.xyz = float3(2.4000001,2.4000001,2.4000001) * r4.xyz;
    r4.xyz = exp2(r4.xyz);
    r2.xyz = cmp(float3(0.0404499993,0.0404499993,0.0404499993) >= r2.xyz);
    r2.xyz = r2.xyz ? r3.xyz : r4.xyz;
    r2.xyz = float3(0.00999999978,0.00999999978,0.00999999978) * r2.xyz;
    r2.xyz = log2(r2.xyz);
    r2.xyz = float3(0.159301758,0.159301758,0.159301758) * r2.xyz;
    r2.xyz = exp2(r2.xyz);
    r3.xyz = r2.xyz * float3(18.8515625,18.8515625,18.8515625) + float3(0.8359375,0.8359375,0.8359375);
    r2.xyz = r2.xyz * float3(18.6875,18.6875,18.6875) + float3(1,1,1);
    r2.xyz = r3.xyz / r2.xyz;
    r3.xyz = float3(-0.8359375,-0.8359375,-0.8359375) + r2.xyz;
    r3.xyz = max(float3(0,0,0), r3.xyz);
    r2.xyz = -r2.xyz * float3(18.6875,18.6875,18.6875) + float3(18.8515625,18.8515625,18.8515625);
    r2.xyz = r3.xyz / r2.xyz;
    r2.xyz = log2(r2.xyz);
    r2.xyz = float3(6.27739477,6.27739477,6.27739477) * r2.xyz;
    r2.xyz = exp2(r2.xyz);
    r0.xyz = float3(100,100,100) * r2.xyz;
  }
  if (r1.z != 0) {
    r1.xy = cb0[2].xy * v1.xy;
    r2.xyz = float3(12.9200001,12.9200001,12.9200001) * r0.xyz;
    r3.xyz = log2(abs(r0.xyz));
    r3.xyz = float3(0.416666657,0.416666657,0.416666657) * r3.xyz;
    r3.xyz = exp2(r3.xyz);
    r3.xyz = r3.xyz * float3(1.05499995,1.05499995,1.05499995) + float3(-0.0549999997,-0.0549999997,-0.0549999997);
    r4.xyz = cmp(float3(0.00313080009,0.00313080009,0.00313080009) >= r0.xyz);
    r2.xyz = r4.xyz ? r2.xyz : r3.xyz;
    r1.xy = trunc(r1.xy);
    r1.xy = cb0[2].zw * r1.xy;
    r1.xyz = t10.SampleLevel(s1_s, r1.xy, 0).xyz;
    r3.x = dot(r2.xyz, float3(0.298999995,0.587000012,0.114));
    r0.w = dot(r2.xyz, float3(-0.168699995,-0.33129999,0.5));
    r2.w = dot(r2.xyz, float3(0.5,-0.41870001,-0.0812999979));
    r1.x = r1.x + r1.y;
    r1.x = saturate(r1.x + r1.z);
    r3.w = 18 * r0.w;
    r3.w = frac(r3.w);
    r1.y = r1.y * 0.5 + -r3.w;
    r1.y = r1.y * r1.x;
    r3.y = r1.y * 0.055555556 + r0.w;
    r0.w = 18 * r2.w;
    r0.w = frac(r0.w);
    r0.w = r1.z * 0.5 + -r0.w;
    r0.w = r0.w * r1.x;
    r3.z = r0.w * 0.055555556 + r2.w;
    r0.w = 1 + -r1.x;
    r4.x = dot(r3.xz, float2(1,1.40199995));
    r4.y = dot(r3.xyz, float3(1,-0.344139993,-0.714139998));
    r4.z = dot(r3.xy, float2(1,1.77199996));
    r1.xyz = r4.xyz * r1.xxx;
    r1.xyz = r2.xyz * r0.www + r1.xyz;
    r2.xyz = float3(0.0773993805,0.0773993805,0.0773993805) * r1.xyz;
    r3.xyz = float3(0.0549999997,0.0549999997,0.0549999997) + r1.xyz;
    r3.xyz = float3(0.947867334,0.947867334,0.947867334) * r3.xyz;
    r3.xyz = log2(r3.xyz);
    r3.xyz = float3(2.4000001,2.4000001,2.4000001) * r3.xyz;
    r3.xyz = exp2(r3.xyz);
    r1.xyz = cmp(float3(0.0404499993,0.0404499993,0.0404499993) >= r1.xyz);
    r0.xyz = r1.xyz ? r2.xyz : r3.xyz;
  }
  if (r1.w != 0) {
    r1.xyz = f32tof16(r0.xyz);
    r1.xyz = (uint3)r1.xyz;
    r1.xyz = cb0[4].zzz * r1.xyz;
    r1.xyz = r1.xyz * float3(0.96875,0.96875,0.96875) + float3(0.015625,0.015625,0.015625);
    r0.xyz = t9.SampleLevel(s1_s, r1.xyz, 0).xyz;
  }
  o0.xyz = float3(1.25,1.25,1.25) * r0.xyz;
  o0.w = 1;
  return;
}