// ---- Created with 3Dmigoto v1.3.16 on Sat May 25 22:39:47 2024
Texture2D<float4> t15 : register(t15);

Texture2D<float4> t14 : register(t14);

Texture2D<float4> t13 : register(t13);

Texture2D<float4> t12 : register(t12);

Texture2D<float4> t11 : register(t11);

Texture2D<float4> t10 : register(t10);

Texture2D<float4> t9 : register(t9);

Texture2D<float4> t8 : register(t8);

Texture2D<float4> t7 : register(t7);

Texture2D<float4> t6 : register(t6);

Texture2D<float4> t5 : register(t5);

Texture2D<float4> t4 : register(t4);

Texture2D<float4> t3 : register(t3);

Texture2D<float4> t2 : register(t2);

Texture2D<float4> t1 : register(t1);

Texture2D<float4> t0 : register(t0);

SamplerState s15_s : register(s15);

SamplerState s14_s : register(s14);

SamplerState s13_s : register(s13);

SamplerState s12_s : register(s12);

SamplerState s11_s : register(s11);

SamplerState s10_s : register(s10);

SamplerState s9_s : register(s9);

SamplerState s8_s : register(s8);

SamplerState s7_s : register(s7);

SamplerState s6_s : register(s6);

SamplerState s5_s : register(s5);

SamplerState s4_s : register(s4);

SamplerState s3_s : register(s3);

SamplerState s2_s : register(s2);

SamplerState s1_s : register(s1);

SamplerState s0_s : register(s0);

cbuffer cb0 : register(b0)
{
  float4 cb0[4];
}




// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_POSITION0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_TARGET0)
{
  float4 r0,r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = t0.Sample(s0_s, v1.xy).xyzw;
  r0.xyzw = float4(1,1,1,1) + -r0.xyzw;
  r0.xyzw = -r0.xyzw * cb0[0].xxxx + float4(1,1,1,1);
  r1.xyzw = t1.Sample(s1_s, v1.xy).xyzw;
  r1.xyzw = float4(1,1,1,1) + -r1.xyzw;
  r1.xyzw = -r1.xyzw * cb0[0].yyyy + float4(1,1,1,1);
  r0.xyzw = r1.xyzw * r0.xyzw;
  r1.xyzw = t2.Sample(s2_s, v1.xy).xyzw;
  r1.xyzw = float4(1,1,1,1) + -r1.xyzw;
  r1.xyzw = -r1.xyzw * cb0[0].zzzz + float4(1,1,1,1);
  r0.xyzw = r1.xyzw * r0.xyzw;
  r1.xyzw = t3.Sample(s3_s, v1.xy).xyzw;
  r1.xyzw = float4(1,1,1,1) + -r1.xyzw;
  r1.xyzw = -r1.xyzw * cb0[0].wwww + float4(1,1,1,1);
  r0.xyzw = r1.xyzw * r0.xyzw;
  r1.xyzw = t4.Sample(s4_s, v1.xy).xyzw;
  r1.xyzw = float4(1,1,1,1) + -r1.xyzw;
  r1.xyzw = -r1.xyzw * cb0[1].xxxx + float4(1,1,1,1);
  r0.xyzw = r1.xyzw * r0.xyzw;
  r1.xyzw = t5.Sample(s5_s, v1.xy).xyzw;
  r1.xyzw = float4(1,1,1,1) + -r1.xyzw;
  r1.xyzw = -r1.xyzw * cb0[1].yyyy + float4(1,1,1,1);
  r0.xyzw = r1.xyzw * r0.xyzw;
  r1.xyzw = t6.Sample(s6_s, v1.xy).xyzw;
  r1.xyzw = float4(1,1,1,1) + -r1.xyzw;
  r1.xyzw = -r1.xyzw * cb0[1].zzzz + float4(1,1,1,1);
  r0.xyzw = r1.xyzw * r0.xyzw;
  r1.xyzw = t7.Sample(s7_s, v1.xy).xyzw;
  r1.xyzw = float4(1,1,1,1) + -r1.xyzw;
  r1.xyzw = -r1.xyzw * cb0[1].wwww + float4(1,1,1,1);
  r0.xyzw = r1.xyzw * r0.xyzw;
  r1.xyzw = t8.Sample(s8_s, v1.xy).xyzw;
  r1.xyzw = float4(1,1,1,1) + -r1.xyzw;
  r1.xyzw = -r1.xyzw * cb0[2].xxxx + float4(1,1,1,1);
  r0.xyzw = r1.xyzw * r0.xyzw;
  r1.xyzw = t9.Sample(s9_s, v1.xy).xyzw;
  r1.xyzw = float4(1,1,1,1) + -r1.xyzw;
  r1.xyzw = -r1.xyzw * cb0[2].yyyy + float4(1,1,1,1);
  r0.xyzw = r1.xyzw * r0.xyzw;
  r1.xyzw = t10.Sample(s10_s, v1.xy).xyzw;
  r1.xyzw = float4(1,1,1,1) + -r1.xyzw;
  r1.xyzw = -r1.xyzw * cb0[2].zzzz + float4(1,1,1,1);
  r0.xyzw = r1.xyzw * r0.xyzw;
  r1.xyzw = t11.Sample(s11_s, v1.xy).xyzw;
  r1.xyzw = float4(1,1,1,1) + -r1.xyzw;
  r1.xyzw = -r1.xyzw * cb0[2].wwww + float4(1,1,1,1);
  r0.xyzw = r1.xyzw * r0.xyzw;
  r1.xyzw = t12.Sample(s12_s, v1.xy).xyzw;
  r1.xyzw = float4(1,1,1,1) + -r1.xyzw;
  r1.xyzw = -r1.xyzw * cb0[3].xxxx + float4(1,1,1,1);
  r0.xyzw = r1.xyzw * r0.xyzw;
  r1.xyzw = t13.Sample(s13_s, v1.xy).xyzw;
  r1.xyzw = float4(1,1,1,1) + -r1.xyzw;
  r1.xyzw = -r1.xyzw * cb0[3].yyyy + float4(1,1,1,1);
  r0.xyzw = r1.xyzw * r0.xyzw;
  r1.xyzw = t14.Sample(s14_s, v1.xy).xyzw;
  r1.xyzw = float4(1,1,1,1) + -r1.xyzw;
  r1.xyzw = -r1.xyzw * cb0[3].zzzz + float4(1,1,1,1);
  r0.xyzw = r1.xyzw * r0.xyzw;
  r1.xyzw = t15.Sample(s15_s, v1.xy).xyzw;
  r1.xyzw = float4(1,1,1,1) + -r1.xyzw;
  r1.xyzw = -r1.xyzw * cb0[3].wwww + float4(1,1,1,1);
  o0.xyzw = r1.xyzw * r0.xyzw;
  return;
}