// ---- Created with 3Dmigoto v1.4.1 on Fri May 23 23:19:44 2025
Texture2D<uint4> t7 : register(t7);

Texture2D<float4> t6 : register(t6);

Texture2D<float4> t5 : register(t5);

Texture2D<float4> t4 : register(t4);

Texture2D<float4> t3 : register(t3);

Texture2D<float4> t2 : register(t2);

Texture3D<float4> t1 : register(t1);

Texture2D<float4> t0 : register(t0);

SamplerState s6_s : register(s6);

SamplerState s5_s : register(s5);

SamplerState s4_s : register(s4);

SamplerState s3_s : register(s3);

SamplerState s2_s : register(s2);

SamplerState s1_s : register(s1);

SamplerState s0_s : register(s0);

cbuffer cb0 : register(b0)
{
  float4 cb0[19];
}




// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float4 v1 : TEXCOORD0,
  float2 v2 : TEXCOORD1,
  out float4 o0 : SV_Target0,
  out uint2 o1 : SV_Target1)
{
  float4 r0,r1,r2,r3,r4;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = asint(cb0[0].xxxx) & int4(8,64,128,32);
  if (r0.x != 0) {
    r1.xy = float2(-0.5,-0.5) + v2.xy;
    r1.zw = cb0[4].zw * r1.xy;
    r1.zw = r1.zw * r1.zw;
    r0.x = r1.z + r1.w;
    r1.z = asint(cb0[0].x) & 16;
    if (r1.z != 0) {
      r1.z = r0.x * cb0[2].x + 1;
      r1.z = cb0[2].z * r1.z;
    } else {
      r1.w = sqrt(r0.x);
      r1.w = cb0[2].y * r1.w + cb0[2].x;
      r0.x = r0.x * r1.w + 1;
      r1.z = cb0[2].z * r0.x;
    }
    r1.xy = r1.xy * r1.zz + float2(0.5,0.5);
  } else {
    r1.xy = v2.xy;
  }
  if (r0.y != 0) {
    r2.xyz = t2.Sample(s2_s, r1.xy).xyz;
    r0.xy = r2.xy * cb0[3].xy + cb0[3].zw;
    r1.xy = r1.xy + r0.xy;
    r0.x = r2.z * r2.z;
    r0.x = r0.x * cb0[2].w + 1;
    r0.x = log2(r0.x);
  } else {
    r0.x = 0;
  }
  r2.xyz = t0.SampleLevel(s0_s, r1.xy, r0.x).xyz;
  r2.xyz = max(float3(0,0,0), r2.xyz);
  t7.GetDimensions(0, uiDest.x, uiDest.y, uiDest.z);
  r1.zw = uiDest.xy;
  r1.zw = (uint2)r1.zw;
  r1.zw = r1.xy * r1.zw;
  r1.zw = floor(r1.zw);
  r3.xy = (uint2)r1.zw;
  r3.zw = float2(0,0);
  r1.zw = t7.Load(r3.xyz).xy;
  if (r0.z != 0) {
    r3.xyzw = float4(-0.5,-0.5,-0.5,-0.5) + r1.xyxy;
    r3.xyzw = r3.xyzw + r3.xyzw;
    r3.xyzw = r3.xyzw * cb0[13].xyzw + float4(0.5,0.5,0.5,0.5);
    r2.x = t0.SampleLevel(s0_s, r3.xy, r0.x).x;
    r2.y = t0.SampleLevel(s0_s, r3.zw, r0.x).y;
  }
  if (r0.w != 0) {
    r3.xyzw = t5.Sample(s5_s, r1.xy).xyzw;
    r0.xyw = r3.xyz * r3.xyz;
    r3.x = cb0[4].x * r3.w;
    if (r0.z != 0) {
      r4.xyzw = float4(-0.5,-0.5,-0.5,-0.5) + r1.xyxy;
      r4.xyzw = r4.xyzw + r4.xyzw;
      r4.xyzw = r4.xyzw * cb0[13].xyzw + float4(0.5,0.5,0.5,0.5);
      r3.yz = t5.Sample(s5_s, r4.xy).xw;
      r0.z = r3.y * r3.y;
      r3.y = cb0[4].x * r3.z;
      r3.zw = t5.Sample(s5_s, r4.zw).yw;
      r3.z = r3.z * r3.z;
      r3.w = cb0[4].x * r3.w;
      r0.z = r0.z * cb0[4].y + -r2.x;
      r2.x = r3.y * r0.z + r2.x;
      r0.z = r3.z * cb0[4].y + -r2.y;
      r2.y = r3.w * r0.z + r2.y;
      r0.z = r0.w * cb0[4].y + -r2.z;
      r2.z = r3.x * r0.z + r2.z;
    } else {
      r2.w = r2.z;
      r0.xyz = r0.xyw * cb0[4].yyy + -r2.xyw;
      r2.xyz = r3.xxx * r0.xyz + r2.xyw;
    }
  }
  r0.xyzw = asint(cb0[0].xxxx) & int4(256,512,2,1);
  if (r0.x != 0) {
    r3.xyz = t3.Sample(s3_s, r1.xy).xyz;
    r3.xyz = cb0[5].xyz * r3.xyz;
    r0.x = asint(cb0[0].x) & 1024;
    if (r0.x != 0) {
      r4.xyz = t6.Sample(s6_s, r1.xy).xyz;
      r4.xyz = saturate(r4.xyz);
      r4.xyz = log2(r4.xyz);
      r4.xyz = cb0[16].xyz * r4.xyz;
      r4.xyz = exp2(r4.xyz);
      r4.xyz = r4.xyz * cb0[17].xyz + cb0[18].xyz;
      r3.xyz = r4.xyz * r3.xyz;
    }
    r2.xyz = r3.xyz + r2.xyz;
  }
  r2.xyz = -cb0[7].xyz + r2.xyz;
  r2.xyz = cb0[6].xyz * r2.xyz + cb0[7].xyz;
  if (r0.y != 0) {
    r0.xy = cb0[12].xy + v2.xy;
    r0.xy = cb0[10].xy * r0.xy;
    r0.x = dot(r0.xy, r0.xy);
    r0.x = saturate(-r0.x * cb0[11].w + 1);
    r0.x = log2(r0.x);
    r0.x = cb0[10].z * r0.x;
    r0.x = exp2(r0.x);
    r2.xyz = r2.xyz * r0.xxx;
  }
  if (r0.z != 0) {
    r0.xy = v2.xy * cb0[8].xy + cb0[8].zw;
    r0.xyz = t4.Sample(s4_s, r0.xy).xyz;
    r1.xy = asint(cb0[0].xx) & int2(8192,2048);
    if (r1.x != 0) {
      r1.x = dot(r0.xyz, float3(0.212599993,0.715200007,0.0722000003));
      r3.xyz = -r1.xxx + r0.xyz;
      r3.xyz = cb0[14].www * r3.xyz + r1.xxx;
    } else {
      r3.xyz = r0.xxx;
    }
    r0.xyz = float3(-0.5,-0.5,-0.5) + r3.xyz;
    if (r1.y != 0) {
      r1.x = dot(r2.xyz, float3(0.212599993,0.715200007,0.0722000003));
      r1.x = cb0[1].x * r1.x;
      r1.x = f32tof16(r1.x);
      r1.x = (uint)r1.x;
      r1.x = r1.x * cb0[1].y + -cb0[15].x;
      r3.xy = cb0[15].yw + -cb0[15].xz;
      r1.y = 0.00100000005 + r3.x;
      r1.x = saturate(r1.x / r1.y);
      r1.x = r1.x * r3.y + cb0[15].z;
      r3.xyz = cb0[9].xyz * r1.xxx;
    } else {
      r3.xyz = cb0[9].xyz;
    }
    r4.xyz = cb0[1].xxx * r2.xyz;
    r4.xyz = f32tof16(r4.xyz);
    r4.xyz = (uint3)r4.xyz;
    r0.xyz = r3.xyz * r0.xyz;
    r0.xyz = saturate(r4.xyz * cb0[1].yyy + r0.xyz);
    r0.xyz = cb0[1].www * r0.xyz;
    r0.xyz = (uint3)r0.xyz;
    r0.xyz = f16tof32(r0.xyz);
    r2.xyz = cb0[1].zzz * r0.xyz;
  }
  if (r0.w != 0) {

    // BT.709 -> ICTCP LMS
    r0.x = dot(float3(0.295775771,0.623078883,0.0811608657), r2.xyz);
    r0.y = dot(float3(0.156185895,0.727243543,0.116516791), r2.xyz);
    r0.z = dot(float3(0.0351171643,0.156598315,0.808349252), r2.xyz);
    r0.xyz = cb0[14].xyz * r0.xyz;
    // ICTCP LMS -> BT.709
    r2.x = dot(float3(6.17299318,-5.32050371,0.147119075), r0.xyz);
    r2.y = dot(float3(-1.32386422,2.5601418,-0.236103922), r0.xyz);
    r2.z = dot(float3(-0.0117062014,-0.264827281,1.27643752), r0.xyz);
  }
  r0.xyz = max(float3(0,0,0), r2.xyz);
  r1.xy = asint(cb0[0].xx) & int2(4,4096);
  if (r1.x != 0) {
    r2.xyz = cb0[1].xxx * r0.xyz;
    r2.xyz = f32tof16(r2.xyz);
    r2.xyz = (uint3)r2.xyz;
    r2.xyz = cb0[1].yyy * r2.xyz;
    r2.xyz = r2.xyz * float3(0.96875,0.96875,0.96875) + float3(0.015625,0.015625,0.015625);
    r0.xyz = t1.SampleLevel(s1_s, r2.xyz, 0).xyz;
  }
  if (r1.y != 0) {
    r1.x = dot(r0.xyz, float3(0.212599993,0.715200007,0.0722000003));
    r1.x = cb0[1].x * r1.x;
    r1.x = f32tof16(r1.x);
    r1.x = (uint)r1.x;
    r0.w = cb0[1].y * r1.x;
  } else {
    r0.w = 1;
  }
  o0.xyzw = r0.xyzw;
  o1.xy = r1.zw;
  return;
}