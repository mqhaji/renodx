#include "./shared.h"
#include "../../shaders/color.hlsl"

// ---- Created with 3Dmigoto v1.3.16 on Sat May 18 21:43:01 2024

cbuffer cb_g_Pass : register(b2)
{

  struct
  {
    float4 m_EyePosition;
    float4 m_EyeDirection;
    float4x4 m_ViewToWorld;
    float4x4 m_WorldToView;
    float4x4 m_ProjMatrix;
    float4x4 m_ViewProj;
    float4x4 m_ViewNoTranslationProj;
    float4 m_ViewTranslation;

    struct
    {
      float4x4 clipXYZToViewPos;
      float4x4 clipXYZToWorldPos;
      float4 clipZToViewZ;
    } reverseProjParams;

    float4 m_VPosToUV;
    float4 m_ViewportScaleOffset;
    float4 m_ClipPlane;
    float4 m_GlobalLightingScale;
    float4 m_ViewSpaceLightingBackWS;
    float4 m_ThinGeomAAPixelScale;
  } g_Pass : packoffset(c0);

}

cbuffer cb_g_HDR : register(b5)
{

  struct
  {
    float4 m_LightingParams;
    float4 m_CameraParams;
    float4 m_FilmR0Params;
    float4 m_FilmR1Params;
    float4 m_FilmG0Params;
    float4 m_FilmG1Params;
    float4 m_FilmB0Params;
    float4 m_FilmB1Params;
    float4 m_ExposureParams;
    float4 m_ScreenExtents;
    float4 m_BloomParams;
  } g_HDR : packoffset(c0);

}

SamplerState s_Scene_s : register(s0);
SamplerState s_Exposure_s : register(s1);
SamplerState s_Bloom_s : register(s2);
SamplerState s_FilmLUT_s : register(s3);
SamplerState s_FilmLUTTarget_s : register(s4);
Texture2D<float4> t_Scene : register(t0);
Texture2D<float4> t_Exposure : register(t1);
Texture2D<float4> t_Bloom : register(t2);
Texture3D<float4> t_FilmLUT : register(t3);
Texture3D<float4> t_FilmLUTTarget : register(t4);


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

  r0.xyz = t_Scene.Sample(s_Scene_s, v1.xy).xyz;
  r0.w = t_Exposure.Sample(s_Exposure_s, float2(0,0)).x;
  r0.xyz = r0.xyz * r0.www * injectedData.colorGradeExposure;
  r1.xyzw = t_Bloom.Sample(s_Bloom_s, v1.xy).xyzw;
  r0.xyz = r0.xyz * g_Pass.m_GlobalLightingScale.yyy + r1.xyz;
  r0.w = r1.w * g_HDR.m_BloomParams.x * injectedData.fxBloom + 1;
  r0.xyz = r0.xyz / r0.www;
  r1.xy = cmp(float2(0,0) != g_HDR.m_CameraParams.xy);
  r1.zw = float2(-0.5,-0.5) + v1.xy;
  r0.w = dot(r1.zw, r1.zw);
  r0.w = 1 + -r0.w;
  r0.w = max(0, r0.w);
  r0.w = log2(r0.w);
  r0.w = g_HDR.m_CameraParams.x * r0.w;
  r0.w = exp2(r0.w);
  r2.xyz = r0.xyz * r0.www;
  r0.xyz = r1.xxx ? r2.xyz : r0.xyz;
  r1.xz = floor(v0.xy);
  r1.xz = (uint2)r1.xz;
  r0.w = (uint)g_HDR.m_LightingParams.x;
  r1.x = mad((int)r1.x, 0x0019660d, (int)r1.z);
  r0.w = (int)r1.x + (int)r0.w;
  r1.x = (int)r0.w ^ 61;
  r0.w = (uint)r0.w >> 16;
  r0.w = (int)r0.w ^ (int)r1.x;
  r0.w = (int)r0.w * 9;
  r1.x = (uint)r0.w >> 4;
  r0.w = (int)r0.w ^ (int)r1.x;
  r0.w = (int)r0.w * 0x27d4eb2d;
  r1.x = (uint)r0.w >> 15;
  r0.w = (int)r0.w ^ (int)r1.x;
  r0.w = (uint)r0.w;
  r0.w = r0.w * 4.65661287e-010 + -1;
  r1.xzw = r0.www * g_HDR.m_CameraParams.yyy + r0.xyz;
  r0.xyz = r1.yyy ? r1.xzw : r0.xyz;
  if (injectedData.toneMapType == 0) {  // Vanilla Tonemapper
  r0.xyz = max(float3(0,0,0), r0.xyz);
  r1.xyz = r0.xyz * float3(0.97709924,0.97709924,0.97709924) + float3(1.46564889,1.46564889,1.46564889);
  r0.xyz = r0.xyz / r1.xyz;

    //r0.xyz = min(float3(1,1,1), r0.xyz);  // clamp to SDR
    r0.xyz = max(float3(9.99999975e-006,9.99999975e-006,9.99999975e-006), r0.xyz);
    r0.xyz = log2(r0.xyz);
    r0.xyz = float3(0.454545468,0.454545468,0.454545468) * r0.xyz;
    r0.xyz = exp2(r0.xzy);
    r0.w = cmp(g_HDR.m_CameraParams.z == 1.000000);
    if (r0.w != 0) {
      r1.xyz = r0.xzy * float3(0.96875,0.96875,0.96875) + float3(0.015625,0.015625,0.015625);
      r2.xyz = r0.xzy * r0.xzy;
      r3.x = r2.x;
      r3.y = r0.x;
      r3.z = 1;
      r0.w = dot(r3.xyz, g_HDR.m_FilmR0Params.xyz);
      r1.w = dot(r3.xyz, g_HDR.m_FilmR1Params.xyz);
      r3.x = r0.w / r1.w;
      r4.x = r2.y;
      r4.y = r0.z;
      r4.z = 1;
      r0.w = dot(r4.xyz, g_HDR.m_FilmG0Params.xyz);
      r1.w = dot(r4.xyz, g_HDR.m_FilmG1Params.xyz);
      r3.y = r0.w / r1.w;
      r0.x = r2.z;
      r0.z = 1;
      r0.w = dot(r0.xyz, g_HDR.m_FilmB0Params.xyz);
      r1.w = dot(r0.xyz, g_HDR.m_FilmB1Params.xyz);
      r3.z = r0.w / r1.w;
      r1.xyz = t_FilmLUT.SampleLevel(s_FilmLUT_s, r1.xyz, 0).xyz;
      r2.xyz = r3.xyz + -r1.xyz;
      r1.xyz = g_HDR.m_CameraParams.www * r2.xyz + r1.xyz;
    } else {
      r0.w = cmp(g_HDR.m_CameraParams.z == 0.000000);
      if (r0.w != 0) {
        r2.xyz = r0.xzy * r0.xzy;
        r3.x = r2.x;
        r3.y = r0.x;
        r3.z = 1;
        r0.w = dot(r3.xyz, g_HDR.m_FilmR0Params.xyz);
        r1.w = dot(r3.xyz, g_HDR.m_FilmR1Params.xyz);
        r1.x = r0.w / r1.w;
        r3.x = r2.y;
        r3.y = r0.z;
        r3.z = 1;
        r0.w = dot(r3.xyz, g_HDR.m_FilmG0Params.xyz);
        r1.w = dot(r3.xyz, g_HDR.m_FilmG1Params.xyz);
        r1.y = r0.w / r1.w;
        r0.x = r2.z;
        r0.z = 1;
        r0.w = dot(r0.xyz, g_HDR.m_FilmB0Params.xyz);
        r1.w = dot(r0.xyz, g_HDR.m_FilmB1Params.xyz);
        r1.z = r0.w / r1.w;
      } else {
        r0.xyz = r0.xzy * float3(0.96875,0.96875,0.96875) + float3(0.015625,0.015625,0.015625);
        r2.xyz = t_FilmLUT.SampleLevel(s_FilmLUT_s, r0.xyz, 0).xyz;
        r0.xyz = t_FilmLUTTarget.SampleLevel(s_FilmLUTTarget_s, r0.xyz, 0).xyz;
        r0.xyz = r0.xyz + -r2.xyz;
        r1.xyz = g_HDR.m_CameraParams.www * r0.xyz + r2.xyz;
      }
    }
  o0.xyz = r1.xyz;
  }
  else {  // untonemapped
    o0.xyz = r0.xyz;
    o0.rgb = pow(o0.rgb, 1.f/2.2f);
  }
  o0.w = 1;
  return;
}