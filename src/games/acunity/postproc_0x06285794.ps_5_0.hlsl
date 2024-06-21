// ---- Created with 3Dmigoto v1.3.16 on Sat May 18 21:42:47 2024
Texture2D<float4> t34 : register(t34);

Texture2D<float4> t33 : register(t33);

Texture2D<float4> t32 : register(t32);

Texture2D<float4> t17 : register(t17);

SamplerState s14_s : register(s14);

cbuffer cb3 : register(b3)
{
  float4 cb3[25];
}

cbuffer cb2 : register(b2)
{
  float4 cb2[36];
}

cbuffer cb1 : register(b1)
{
  float4 cb1[418];
}




// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float4 v1 : TEXCOORD0,
  float4 v2 : COLOR0,
  float4 v3 : TEXCOORD1,
  float4 v4 : TEXCOORD2,
  float4 v5 : TEXCOORD3,
  float4 v6 : TEXCOORD4,
  uint v7 : InstanceID0,
  uint v8 : SV_IsFrontFace0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyz = v3.xyz;
  r0.w = 0;
  r0.xyzw = -cb2[0].xyzw + r0.xyzw;
  r0.w = dot(r0.xyzw, r0.xyzw);
  r0.w = rsqrt(r0.w);
  r0.xyz = r0.xyz * r0.www;
  r0.w = dot(v6.xyz, v6.xyz);
  r0.w = rsqrt(r0.w);
  r1.xyz = v6.xyz * r0.www;
  r0.x = dot(r1.xyz, r0.xyz);
  r0.x = log2(abs(r0.x));
  r0.x = cb3[12].x * r0.x;
  r0.x = exp2(r0.x);
  r0.x = -1 + r0.x;
  r0.x = cb3[13].x * r0.x + 1;
  r0.y = saturate(r0.x);
  r0.y = 1 + -r0.y;
  r0.y = r0.y + -r0.x;
  r0.x = cb3[19].x * r0.y + r0.x;
  r2.xyzw = t33.Sample(s14_s, v1.zw).xyzw;
  r0.y = r2.x + r2.y;
  r0.y = r0.y + r2.z;
  r0.y = r0.y * 0.333332986 + -r2.w;
  r0.y = cb3[15].x * r0.y + r2.w;
  r2.xyzw = t32.Sample(s14_s, v1.xy).xyzw;
  r0.z = -r2.w + r0.y;
  r0.z = cb3[18].x * r0.z + r2.w;
  r0.y = -r0.x * r0.z + r0.y;
  r0.x = r0.x * r0.z;
  r0.y = saturate(cb3[17].x * r0.y + r0.x);
  r0.z = 1 + -r0.y;
  r0.y = -cb3[14].x + r0.y;
  r0.w = 1 + -cb3[16].x;
  r0.z = r0.z + -r0.w;
  r0.w = cmp(0 != cb3[20].x);
  r0.w = r0.w ? cb3[20].x : 9.99999997e-007;
  r0.yz = saturate(r0.yz / r0.ww);
  r0.y = max(r0.y, r0.z);
  r0.y = 1 + -r0.y;
  r0.z = -1 + v2.w;
  r0.z = cb3[21].x * r0.z + 1;
  r0.x = r0.x * r0.z;
  r0.x = r0.y * r0.x;
  r0.y = cb3[24].x + cb3[24].y;
  r0.y = cb3[24].z + r0.y;
  r0.y = cb3[24].w + r0.y;
  r0.y = saturate(1 + -r0.y);
  r0.z = 1 + -r0.y;
  r0.w = dot(r2.xyzw, cb3[24].xyzw);
  r0.w = r0.w * r0.z;
  r2.xyz = r2.xyz * r0.yyy + r0.zzz;
  r0.y = r2.w * r0.y;
  r0.x = r0.x * r0.y + r0.w;
  r0.x = saturate(cb3[11].x * r0.x);
  r0.y = -cb3[0].z + r0.x;
  o0.w = r0.x;
  r0.x = cmp(r0.y < 0);
  if (r0.x != 0) discard;
  r0.x = dot(r1.xyz, cb2[6].xyz);
  r0.y = saturate(r0.x);
  r0.x = saturate(-r0.x);
  r0.yzw = cb1[413].xyz * r0.yyy;
  r0.xyz = r0.xxx * cb1[412].xyz + r0.yzw;
  r0.w = dot(r1.xyz, cb2[7].xyz);
  r1.x = dot(r1.xyz, cb2[8].xyz);
  r1.y = saturate(-r0.w);
  r0.w = saturate(r0.w);
  r0.xyz = r1.yyy * cb1[414].xyz + r0.xyz;
  r0.xyz = r0.www * cb1[415].xyz + r0.xyz;
  r0.w = saturate(-r1.x);
  r1.x = saturate(r1.x);
  r0.xyz = r0.www * cb1[416].xyz + r0.xyz;
  r0.xyz = r1.xxx * cb1[417].xyz + r0.xyz;
  r1.xyz = cb3[23].xyz * r2.xyz;
  r0.xyz = r1.xyz * float3(1.5,1.5,1.5) + r0.xyz;
  r0.xyz = -r2.xyz * cb3[23].xyz + r0.xyz;
  r2.xyz = t34.Sample(s14_s, v1.xy).xyz;
  r0.w = r2.x + r2.y;
  r0.w = r0.w + r2.z;
  r0.w = saturate(0.333332986 * r0.w);
  r0.xyz = r0.www * r0.xyz;
  r0.xyz = cb3[22].xxx * r0.xyz + r1.xyz;
  r0.w = t17.Load(float4(0,0,0,0)).x;
  r0.xyz = r0.xyz / r0.www;
  o0.xyz = cb2[35].xxx * r0.xyz;
  return;
}