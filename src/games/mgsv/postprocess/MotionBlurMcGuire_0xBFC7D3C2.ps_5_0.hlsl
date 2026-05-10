// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:32 2026

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

SamplerState g_samplerPoint_Clamp_s : register(s9);
SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inImage : register(t0);
Texture2D<float4> inVelocityDepth : register(t5);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = v1.xy;
  r0.xy = r0.xy;
  r0.z = g_psObject.m_localParam[2].y;
  r0.w = g_psObject.m_localParam[2].w;
  r1.xy = g_psSystem.m_renderBuffer.zw * r0.ww;
  r0.xy = g_psSystem.m_renderInfo.xy * r0.xy;
  r0.xy = g_psSystem.m_renderBuffer.zw * r0.xy;
  r1.zw = g_psObject.m_localParam[1].zw;
  r0.w = g_psObject.m_localParam[0].w;
  r2.x = g_psObject.m_localParam[0].z;
  r2.yz = float2(64,64) * r0.ww;
  r2.yz = g_psObject.m_localParam[0].xy * r2.yz;
  r3.xyzw = inVelocityDepth.SampleLevel(g_samplerPoint_Clamp_s, r0.xy, 0).xyzw;
  r3.zw = float2(2,2) * r3.zw;
  r3.zw = float2(-1,-1) + r3.zw;
  r2.yz = r3.zw * r2.yz;
  r0.w = dot(r2.yz, r2.yz);
  r0.z = cmp(r0.w < r0.z);
  if (r0.z != 0) {
    r4.xyzw = inImage.SampleLevel(g_samplerPoint_Clamp_s, r0.xy, 0).xyzw;
    r0.z = cmp(r2.x >= 0.99000001);
    if (r0.z != 0) {
      r4.w = 0;
    }
    r4.xyzw = r4.xyzw;
  } else {
    r0.z = 0.166666672 * r2.x;
    r2.yz = r2.yz * r0.zz;
    r1.xy = r2.yz * r1.xy;
    r2.yz = float2(3,3) * r1.xy;
    r2.yz = -r2.yz;
    r2.yz = r2.yz + r0.xy;
    r0.w = sqrt(r0.w);
    r0.z = r0.w * r0.z;
    r0.z = r0.z / 64;
    r0.w = -3 * r0.z;
    r3.xy = r3.xy;
    r2.w = r3.y * r3.y;
    r3.y = max(9.99999975e-05, r3.x);
    r3.y = 1 / r3.y;
    r3.y = r3.y;
    r5.xyzw = inImage.SampleLevel(g_samplerLinear_Clamp_s, r0.xy, 0).wxyz;
    r5.yzw = r5.yzw * r3.yyy;
    r5.x = r5.x;
    r0.x = 0;
    r6.xyz = r5.yzw;
    r0.y = r3.y;
    r3.z = r0.x;
    while (true) {
      r3.w = cmp((int)r3.z < 7);
      if (r3.w == 0) break;
      r3.w = cmp((int)r3.z != 3);
      if (r3.w != 0) {
        r7.xy = (int2)r3.zz;
        r7.xy = r7.xy * r1.xy;
        r7.xy = r7.xy + r2.yz;
        r7.xy = min(r7.xy, r1.zw);
        r3.w = (int)r3.z;
        r3.w = r3.w * r0.z;
        r3.w = r3.w + r0.w;
        r6.w = -r3.w;
        r3.w = max(r6.w, r3.w);
        r7.zw = inVelocityDepth.SampleLevel(g_samplerPoint_Clamp_s, r7.xy, 0).xy;
        r7.zw = r7.zw;
        r8.x = r7.w * r7.w;
        r9.x = r2.w;
        r9.y = r8.x;
        r8.x = r8.x;
        r8.y = r2.w;
        r8.xy = -r8.xy;
        r8.xy = r9.xy + r8.xy;
        r8.xy = float2(256,256) * r8.xy;
        r8.xy = float2(1,1) + r8.xy;
        r8.xy = max(float2(0,0), r8.xy);
        r8.xy = min(float2(1,1), r8.xy);
        r8.xy = r8.xy;
        r3.w = r3.w;
        r9.xz = r7.zz;
        r9.yw = r3.xx;
        r9.xyzw = rcp(r9.xyzw);
        r9.xyzw = r9.xyzw * r3.wwww;
        r9.xyzw = -r9.xyzw;
        r9.xyzw = float4(1,1,1,1) + r9.xyzw;
        r9.xyzw = float4(0,0,0.949999988,0.949999988) + r9.xyzw;
        r9.xyzw = max(float4(0,0,0,0), r9.xyzw);
        r9.xyzw = min(float4(1,1,1,1), r9.xyzw);
        r9.xyzw = r9.xyzw;
        r3.w = dot(r8.xy, r9.xy);
        r6.w = r9.z * r9.w;
        r6.w = 2 * r6.w;
        r3.w = r6.w + r3.w;
        r7.xyz = inImage.SampleLevel(g_samplerPoint_Clamp_s, r7.xy, 0).xyz;
        r7.xyz = r7.xyz * r3.www;
        r6.xyz = r7.xyz + r6.xyz;
        r0.y = r3.w + r0.y;
      }
      r3.z = (int)r3.z + 1;
    }
    r4.xyz = r6.xyz / r0.yyy;
    r0.x = r3.y / r0.y;
    r0.x = -r0.x;
    r0.x = 1 + r0.x;
    r0.y = cmp(r2.x >= 0.99000001);
    r0.x = r0.x;
    r0.z = r5.x * r0.x;
    r0.z = r0.z / 0.125;
    r0.z = -1 + r0.z;
    r0.z = max(0, r0.z);
    r0.z = min(1, r0.z);
    r4.w = r0.y ? r0.x : r0.z;
  }
  r4.xyzw = r4.xyzw;
  o0.xyzw = r4.xyzw;
  return;
}