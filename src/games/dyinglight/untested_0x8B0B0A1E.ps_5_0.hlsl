// ---- Created with 3Dmigoto v1.3.16 on Sat May 25 22:39:23 2024
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
  float2 v1 : TEXCOORD0,
  float2 w1 : TEXCOORD3,
  float4 v2 : TEXCOORD1,
  float3 v3 : TEXCOORD2,
  out float4 o0 : SV_TARGET0,
  out float4 o1 : SV_TARGET1,
  out float4 o2 : SV_TARGET2,
  out float2 o3 : SV_TARGET3)
{
  float4 r0,r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = t0.Sample(s0_s, v1.xy).wxyz;
  r1.x = dot(float3(0.212500006,0.715399981,0.0720999986), r0.yzw);
  r1.x = cb0[0].z * r1.x;
  r1.x = log2(abs(r1.x));
  r1.x = 0.454545468 * r1.x;
  o0.xyz = exp2(r1.xxx);
  r1.x = 1 + -r0.w;
  o0.w = max(0, r1.x * cb0[0].x + cb0[0].y);  //  o0.w = saturate(r1.x * cb0[0].x + cb0[0].y);
  r0.x = max(0, r0.x);  //  r0.x = saturate(r0.x);
  r0.x = r0.x * 2 + -1;
  r1.xyz = max(0, v3.xyz * r0.xxx + float3(1,1,1)); //  r1.xyz = saturate(v3.xyz * r0.xxx + float3(1,1,1));
  o1.xyz = r1.xyz * r0.yzw;
  o1.w = 0;
  o2.xyzw = v2.xyzw;
  o3.xy = w1.xy;
  return;
}