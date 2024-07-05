// ---- Created with 3Dmigoto v1.3.16 on Sat May 25 22:39:01 2024
Texture2D<float4> t1 : register(t1);

Texture2D<float4> t0 : register(t0);

SamplerState s1_s : register(s1);

SamplerState s0_s : register(s0);

cbuffer cb0 : register(b0)
{
  float4 cb0[2];
}




// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_POSITION0,
  float2 v1 : TEXCOORD0,
  float2 w1 : TEXCOORD2,
  float3 v2 : TEXCOORD1,
  uint v3 : SV_ISFRONTFACE0,
  out float4 o0 : SV_TARGET0,
  out float4 o1 : SV_TARGET1,
  out float4 o2 : SV_TARGET2,
  out float2 o3 : SV_TARGET3)
{
  float4 r0;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = t0.Sample(s0_s, v1.xy).xyzw;
  r0.w = -cb0[0].w + r0.w;
  o1.xyz = r0.xyz;
  r0.x = cmp(r0.w < 0);
  if (r0.x != 0) discard;
  r0.xyzw = t1.Sample(s1_s, v1.xy).xyzw;
  r0.x = dot(float3(0.212500006,0.715399981,0.0720999986), r0.xyz);
  o0.w = saturate(r0.w * cb0[0].x + cb0[0].y);
  r0.x = cb0[0].z * r0.x;
  r0.x = log2(abs(r0.x));
  r0.x = 0.454545468 * r0.x;
  o0.xyz = exp2(r0.xxx);
  o1.w = 0;
  r0.xyzw = v3.xxxx ? v2.xyzx : -v2.xyzx;
  o2.xyzw = r0.xyzw * cb0[1].xxxy + cb0[1].xxxz;
  o3.xy = w1.xy;
  return;
}