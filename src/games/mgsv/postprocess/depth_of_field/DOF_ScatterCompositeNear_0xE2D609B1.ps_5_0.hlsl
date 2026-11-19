// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:34 2026

cbuffer cPSObject : register(b5)
{

  struct
  {
    float4x4 m_viewWorld;
    float4x4 m_world;
    float4 m_useWeightCount;
    float4 m_localParam[4];
  } g_psObject : packoffset(c0);

}

cbuffer cPSSystem : register(b0)
{

  struct
  {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem : packoffset(c0);

}

SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inTexture0 : register(t0);
Texture2D<float4> inTexture1 : register(t1);
Texture2D<float4> inTexture2 : register(t2);
Texture2D<float4> inTexture3 : register(t3);
Texture2D<float4> inTexture4 : register(t6);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = v1.xy;
  r0.xy = r0.xy;
  r0.xy = g_psSystem.m_renderInfo.xy * r0.xy;
  r0.xy = g_psSystem.m_renderBuffer.zw * r0.xy;
  r0.zw = g_psSystem.m_renderBuffer.zw * g_psSystem.m_renderInfo.xy;
  r1.xyzw = inTexture0.Sample(g_samplerLinear_Clamp_s, r0.xy).wxyz;
  r2.xyzw = inTexture1.Sample(g_samplerLinear_Clamp_s, r0.xy).xyzw;
  r3.xyzw = inTexture2.Sample(g_samplerLinear_Clamp_s, r0.xy).xyzw;
  r4.xyzw = inTexture3.Sample(g_samplerLinear_Clamp_s, r0.xy).xyzw;

  r1 = max(0, r1);
  r2 = max(0, r2);
  r3 = max(0, r3);
  r4 = max(0, r4);

  r1.x = r1.x;
  r5.x = g_psObject.m_localParam[3].x;
  r5.y = g_psObject.m_localParam[3].y;
  r5.z = g_psObject.m_localParam[2].w;
  r5.w = g_psObject.m_localParam[2].z;
  r5.z = -r5.z;
  r1.x = max(r5.z, r1.x);
  r1.x = min(r1.x, r5.w);
  r5.z = -r1.x;
  r5.x = -r5.x;
  r5.w = r5.z + r5.x;
  r5.x = r5.y + r5.x;
  r5.x = 0.5 * r5.x;
  r5.w = r5.w / r5.x;
  r1.x = max(-2, r1.x);
  r1.x = 0.5 * r1.x;
  r1.x = 0.5 + r1.x;
  r1.x = 2 * r1.x;
  r1.x = min(0, r1.x);
  r6.xy = float2(0.866025388,0.5) * r1.xx;
  r0.zw = g_psSystem.m_renderBuffer.zw * r0.zw;
  r6.xy = r6.xy * r0.zw;
  r6.xy = r6.xy + r0.xy;
  r6.xyzw = inTexture0.Sample(g_samplerLinear_Clamp_s, r6.xy).wxyz;

  r6 = max(0, r6);

  r6.x = cmp(0 >= r6.x);
  r6.x = r6.x ? 1 : 0;
  r6.xyz = r6.yzw * r6.xxx;
  r6.xyz = float3(0,0,0) + r6.xyz;
  r7.xy = float2(-0.5,0.866025388) * r1.xx;
  r7.xy = r7.xy * r0.zw;
  r7.xy = r7.xy + r0.xy;
  r7.xyzw = inTexture0.Sample(g_samplerLinear_Clamp_s, r7.xy).wxyz;

  r7 = max(0, r7);

  r6.w = cmp(0 >= r7.x);
  r6.w = r6.w ? 1 : 0;
  r7.xyz = r7.yzw * r6.www;
  r6.xyz = r7.xyz + r6.xyz;
  r7.xy = float2(-0.866025388,-0.5) * r1.xx;
  r7.xy = r7.xy * r0.zw;
  r7.xy = r7.xy + r0.xy;
  r7.xyzw = inTexture0.Sample(g_samplerLinear_Clamp_s, r7.xy).wxyz;

  r7 = max(0, r7);

  r6.w = cmp(0 >= r7.x);
  r6.w = r6.w ? 1 : 0;
  r7.xyz = r7.yzw * r6.www;
  r6.xyz = r7.xyz + r6.xyz;
  r7.xy = float2(0.5,-0.866025388) * r1.xx;
  r0.zw = r7.xy * r0.zw;
  r0.zw = r0.xy + r0.zw;
  r7.xyzw = inTexture0.Sample(g_samplerLinear_Clamp_s, r0.zw).wxyz;

  r7 = max(0, r7);

  r0.z = cmp(0 >= r7.x);
  r0.z = r0.z ? 1 : 0;
  r7.xyz = r7.yzw * r0.zzz;
  r6.xyz = r7.xyz + r6.xyz;
  r1.xyz = r6.xyz + r1.yzw;
  r1.xyz = r1.xyz / float3(5,5,5);
  r0.z = max(0, r5.w);
  r0.z = min(1, r0.z);
  r0.w = -r5.y;
  r0.w = r5.z + r0.w;
  r0.w = r0.w / r5.x;
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r1.xyz = r1.xyz;
  r2.xyzw = r3.xyzw + r2.xyzw;
  r2.xyzw = r2.xyzw + r4.xyzw;
  r1.w = max(1, r2.w);
  r2.xyzw = r2.wxyz / r1.wwww;
  r2.x = r2.x;
  r0.w = r0.z * r0.w;
  r1.w = -r2.x;
  r0.w = r1.w + r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r1.xyz = r1.xyz * r0.www;
  r1.xyz = r1.xyz + r2.yzw;
  r0.w = r2.x + r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r2.xyz = inTexture4.Sample(g_samplerLinear_Clamp_s, r0.xy).xyz;

  r2 = max(0, r2);

  r2.xyz = r2.xyz;
  r0.x = -r0.w;
  r0.x = r0.z + r0.x;
  r0.x = max(0, r0.x);
  r0.x = min(1, r0.x);
  r2.xyz = r2.xyz * r0.xxx;
  r1.xyz = r2.xyz + r1.xyz;
  r0.x = r0.w + r0.x;
  r0.x = max(0, r0.x);
  r1.w = min(1, r0.x);
  r1.xyz = r1.xyz;
  r1.w = r1.w;
  r1.xyz = r1.xyz;
  r1.w = r1.w;
  o0.xyzw = r1.xyzw;

  o0 = max(0, o0); // fixes black dots
  return;
}