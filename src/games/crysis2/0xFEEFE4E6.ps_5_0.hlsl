// ---- Created with 3Dmigoto v1.3.16 on Thu Jun 06 03:06:21 2024

cbuffer PER_BATCH : register(b0)
{
  float4 HDRParams : packoffset(c0);
  float4 PS_ScreenSize : packoffset(c1);
  float Time : packoffset(c2);
}

SamplerState PostAA_GrainSS_s : register(s0);
Texture2D<float4> PAAComp_CurTarg : register(t0);
Texture3D<float4> PostAA_Grain : register(t1);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float4 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.x = PS_ScreenSize.x / PS_ScreenSize.y;
  r0.zw = float2(4,1) * v1.xy;
  r0.y = 4;
  r0.xy = r0.zw * r0.xy;
  r0.z = 3 * Time;
  r0.x = PostAA_Grain.Sample(PostAA_GrainSS_s, r0.xyz).x;
  r0.x = -0.5 + r0.x;
  r0.x = HDRParams.w * r0.x + 0.5;
  r0.y = 1 + -r0.x;
  r1.xy = (int2)v0.xy;
  r1.zw = float2(0,0);
  r1.xyzw = PAAComp_CurTarg.Load(r1.xyz).xyzw;
  r2.xyz = float3(1,1,1) + -r1.xyz;
  r2.xyz = r2.xyz + r2.xyz;
  r0.yzw = -r2.xyz * r0.yyy + float3(1,1,1);
  r2.xyz = r1.xyz * r0.xxx;
  r0.xyz = -r2.xyz * float3(2,2,2) + r0.yzw;
  r2.xyz = r2.xyz + r2.xyz;
  r1.xyz = cmp(r1.xyz >= float3(0.5,0.5,0.5));
  o0.w = saturate(r1.w);
  r1.xyz = r1.xyz ? float3(1,1,1) : 0;
  o0.xyz = max(0, r1.xyz * r0.xyz + r2.xyz);
  return;
}