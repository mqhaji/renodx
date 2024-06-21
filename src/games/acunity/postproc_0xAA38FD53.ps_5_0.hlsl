// ---- Created with 3Dmigoto v1.3.16 on Sat May 18 21:43:01 2024
Texture2D<float4> t36 : register(t36);

Texture2D<float4> t35 : register(t35);

TextureCube<float4> t34 : register(t34);

Texture2D<float4> t33 : register(t33);

Texture2D<float4> t32 : register(t32);

Texture2D<float4> t26 : register(t26);

Texture2D<float4> t17 : register(t17);

Texture2D<float4> t12 : register(t12);

SamplerState s14_s : register(s14);

SamplerState s12_s : register(s12);

SamplerState s7_s : register(s7);

cbuffer cb3 : register(b3)
{
  float4 cb3[36];
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
  float4 r0,r1,r2,r3,r4,r5;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = t33.Sample(s14_s, v1.zw).xyzw;
  r0.x = r0.x + r0.y;
  r0.x = r0.x + r0.z;
  r0.x = r0.x * 0.333332986 + -r0.w;
  r0.x = cb3[20].x * r0.x + r0.w;
  r1.xyzw = t32.Sample(s14_s, v1.xy).xyzw;
  r0.y = -1 + r1.w;
  r0.y = cb3[32].w * r0.y + 1;
  r0.z = r0.x + -r0.y;
  r0.y = cb3[23].x * r0.z + r0.y;
  r0.x = r0.x + -r0.y;
  r0.x = saturate(cb3[22].x * r0.x + r0.y);
  r0.z = 1 + -r0.x;
  r0.x = -cb3[19].x + r0.x;
  r0.w = 1 + -cb3[21].x;
  r0.z = r0.z + -r0.w;
  r0.w = cmp(0 != cb3[27].x);
  r0.w = r0.w ? cb3[27].x : 9.99999997e-007;
  r0.xz = saturate(r0.xz / r0.ww);
  r0.x = max(r0.x, r0.z);
  r0.x = 1 + -r0.x;
  r0.z = -1 + v2.w;
  r0.z = cb3[26].x * r0.z + 1;
  r0.y = r0.y * r0.z;
  r0.x = r0.x * r0.y;
  r0.x = saturate(cb3[18].x * r0.x);
  r0.y = -cb3[0].z + r0.x;
  o0.w = r0.x;
  r0.x = cmp(r0.y < 0);
  if (r0.x != 0) discard;
  r0.x = dot(v4.xyz, v4.xyz);
  r0.x = rsqrt(r0.x);
  r0.xyz = v4.xyz * r0.xxx;
  r0.w = dot(v5.xyz, v5.xyz);
  r0.w = rsqrt(r0.w);
  r2.xyz = v5.xyz * r0.www;
  r3.xy = t35.Sample(s14_s, v1.xy).xy;
  r3.zw = r3.xy * float2(2,2) + float2(-1,-1);
  r4.xy = r3.xy + r3.xy;
  r0.w = dot(r3.zw, r3.zw);
  r0.w = 1 + -r0.w;
  r0.w = max(0, r0.w);
  r4.z = sqrt(r0.w);
  r4.xyz = float3(-1,-1,-1) + r4.xyz;
  r4.xyz = cb3[28].xxx * r4.xyz + float3(0,0,1);
  r2.xyz = r4.yyy * r2.xyz;
  r0.xyz = r4.xxx * r0.xyz + r2.xyz;
  r0.w = dot(v6.xyz, v6.xyz);
  r0.w = rsqrt(r0.w);
  r2.xyz = v6.xyz * r0.www;
  r0.xyz = r4.zzz * r2.xyz + r0.xyz;
  r0.w = cb3[30].x * r4.x;
  r0.w = 5 * abs(r0.w);
  r0.w = min(1, r0.w);
  r2.w = dot(r0.xyz, r0.xyz);
  r2.w = rsqrt(r2.w);
  r0.xyz = r2.www * r0.xyz;
  r4.xyz = v3.xyz;
  r4.w = 0;
  r4.xyzw = -cb2[0].xyzw + r4.xyzw;
  r2.w = dot(r4.xyzw, r4.xyzw);
  r3.x = rsqrt(r2.w);
  r2.w = sqrt(r2.w);
  r2.w = cb3[24].x / r2.w;
  r3.yz = r3.zw * r2.ww;
  r3.yz = saturate(v0.xy * cb2[32].xy + r3.yz);
  r5.xyz = r4.xyz * r3.xxx;
  r2.w = dot(r5.xyz, r0.xyz);
  r2.w = -0.600000024 + abs(r2.w);
  r2.w = saturate(3.33333325 * r2.w);
  r2.w = 1 + -r2.w;
  r2.w = log2(r2.w);
  r2.w = cb3[29].x * r2.w;
  r2.w = exp2(r2.w);
  r5.xyz = t26.Sample(s7_s, r3.yz).xyz;
  r3.xyz = t12.Sample(s7_s, r3.yz).xyz;
  r5.xyz = r5.xyz + -r3.xyz;
  r3.xyz = r0.www * r5.xyz + r3.xyz;
  r5.xyz = cb3[33].xyz * r3.xyz;
  r3.xyz = cb3[34].xyz * r3.xyz + -r5.xyz;
  r3.xyz = r2.www * r3.xyz + r5.xyz;
  r3.xyz = float3(1.5,1.5,1.5) * r3.xyz;
  r5.xyz = cb3[32].xyz * r1.xyz + -r3.xyz;
  r5.xyz = r1.www * r5.xyz + r3.xyz;
  r1.xyz = cb3[32].xyz * r1.xyz;
  r1.xyz = r1.xyz * r3.xyz + -r5.xyz;
  r1.xyz = cb3[31].xxx * r1.xyz + r5.xyz;
  r0.w = dot(r4.xyz, r4.xyz);
  r0.w = rsqrt(r0.w);
  r3.xyz = r4.xyz * r0.www;
  r0.w = dot(r3.xyz, r0.xyz);
  r0.w = r0.w + r0.w;
  r0.xyz = r0.xyz * -r0.www + r3.xyz;
  r0.xyz = t34.Sample(s12_s, r0.xyz).xyz;
  r0.xyz = r0.xyz * cb3[35].xyz + r1.xyz;
  r0.w = dot(r2.xyz, cb2[6].xyz);
  r1.x = saturate(r0.w);
  r0.w = saturate(-r0.w);
  r1.xyz = cb1[413].xyz * r1.xxx;
  r1.xyz = r0.www * cb1[412].xyz + r1.xyz;
  r0.w = dot(r2.xyz, cb2[7].xyz);
  r1.w = dot(r2.xyz, cb2[8].xyz);
  r2.x = saturate(-r0.w);
  r0.w = saturate(r0.w);
  r1.xyz = r2.xxx * cb1[414].xyz + r1.xyz;
  r1.xyz = r0.www * cb1[415].xyz + r1.xyz;
  r0.w = saturate(-r1.w);
  r1.w = saturate(r1.w);
  r1.xyz = r0.www * cb1[416].xyz + r1.xyz;
  r1.xyz = r1.www * cb1[417].xyz + r1.xyz;
  r1.xyz = r0.xyz * float3(1.5,1.5,1.5) + r1.xyz;
  r1.xyz = r1.xyz + -r0.xyz;
  r2.xyz = t36.Sample(s14_s, v1.xy).xyz;
  r0.w = r2.x + r2.y;
  r0.w = r0.w + r2.z;
  r0.w = saturate(0.333332986 * r0.w);
  r1.xyz = r0.www * r1.xyz;
  r0.xyz = cb3[25].xxx * r1.xyz + r0.xyz;
  r0.w = t17.Load(float4(0,0,0,0)).x;
  r0.xyz = r0.xyz / r0.www;
  o0.xyz = cb2[35].xxx * r0.xyz;
  return;
}