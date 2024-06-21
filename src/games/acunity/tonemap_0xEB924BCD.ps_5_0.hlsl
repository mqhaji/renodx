// ---- Created with 3Dmigoto v1.3.16 on Sat May 18 21:43:06 2024

struct AdaptLuminanceData
{
    float Exposure;                // Offset:    0
    float ExposureDelta;           // Offset:    4
    float LastExposureTarget;      // Offset:    8
    float AverageLuminance;        // Offset:   12
    float HistogramMax;            // Offset:   16
    float ToBeUsed1;               // Offset:   20
    float ToBeUsed2;               // Offset:   24
    float ToBeUsed3;               // Offset:   28
};

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

StructuredBuffer<AdaptLuminanceData> g_AdaptionData : register(t5);
Texture2D<int> g_HistogramTexture : register(t6);  // 3dmigoto says Texture2D<sint>, replaced with Texture2D<int>


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2,r3,r4,r5;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyz = v1.xyy * float3(1,4,4) + float3(0,-3,-3.88000011);
  r1.xy = cmp(float2(1,0) < r0.xz);
  r0.w = cmp(r0.y < -0.200000003);
  r0.w = (int)r0.w | (int)r1.x;
  if (r0.w != 0) discard;
  r0.w = g_AdaptionData[0].Exposure;
  r1.x = g_AdaptionData[0].LastExposureTarget;
  r1.z = g_AdaptionData[0].AverageLuminance;
  r1.w = g_AdaptionData[0].HistogramMax;
  r2.x = 1.17647064 * r0.y;
  r2.y = r0.x * 45 + -17.584259;
  r2.z = ddx_coarse(r2.y);
  r2.w = ddx_coarse(r0.x);
  r2.x = ddy_coarse(r2.x);
  if (r1.y != 0) {
    r1.y = round(r2.y);
    r1.y = r2.y + -r1.y;
    r3.x = 0.5 * abs(r2.z);
    r1.y = cmp(abs(r1.y) < r3.x);
    r3.z = r1.y ? 0.800000 : 0;
    r1.y = cmp(g_HDR.m_ExposureParams.y < r2.y);
    r3.w = cmp(r2.y < g_HDR.m_ExposureParams.x);
    r1.y = (int)r1.y | (int)r3.w;
    r3.x = r1.y ? 0.500000 : 0;
    r4.xy = -g_HDR.m_ExposureParams.yx + r2.yy;
    r4.xy = cmp(abs(r4.xy) < abs(r2.zz));
    r1.y = (int)r4.y | (int)r4.x;
    r3.y = 0;
    r3.xyz = r1.yyy ? float3(1,0,0) : r3.xyz;
    r1.x = 1 / r1.x;
    r1.y = cmp(0.0599999987 < r0.z);
    r1.x = 5464 * r1.x;
    r1.x = log2(r1.x);
    r1.x = r2.y + -r1.x;
    r1.x = cmp(abs(r1.x) < abs(r2.z));
    r1.x = r1.x ? r1.y : 0;
    r3.xyz = r1.xxx ? float3(0,1,0) : r3.xyz;
    r1.x = 1 / r0.w;
    r0.z = cmp(r0.z < 0.0599999987);
    r1.x = 5464 * r1.x;
    r1.x = log2(r1.x);
    r1.x = r2.y + -r1.x;
    r1.x = cmp(abs(r1.x) < abs(r2.z));
    r0.z = r0.z ? r1.x : 0;
    r3.xyz = r0.zzz ? float3(1,0,0) : r3.xyz;
  } else {
    r0.z = cmp(0.850000024 >= r0.y);
    if (r0.z != 0) {
      r0.z = 1024 * r2.w;
      r1.x = 1024 * r0.x;
      r4.x = (int)r1.x;
      r4.yzw = float3(4.20389539e-045,0,0);
      r1.x = g_HistogramTexture.Load(r4.xyz).x;
      r1.x = (int)r1.x;
      r1.x = max(0, r1.x);
      r4.yzw = float3(4.20389539e-045,0,0);
      r5.xyz = r1.xxx;
      r1.y = 1;
      while (true) {
        r3.w = cmp(r1.y >= r0.z);
        if (r3.w != 0) break;
        r3.w = r0.x * 1024 + r1.y;
        r4.x = (int)r3.w;
        r3.w = g_HistogramTexture.Load(r4.xyz).x;
        r3.w = (int)r3.w;
        r5.xyz = max(r5.xyz, r3.www);
        r1.y = 1 + r1.y;
      }
      r0.z = -r0.y * 1.17647064 + 1;
      r1.xyw = r5.xyz / r1.www;
      r1.xyw = cmp(r1.xyw >= r0.zzz);
      r4.xyz = r1.xyw ? float3(0.200000003,0.200000003,0.200000003) : 0;
      r1.x = r0.x * 45.0439873 + -30;
      r1.x = exp2(r1.x);
      r0.w = r1.x * r0.w;
      r1.x = r0.w * 0.97709924 + 1.46564889;
      r0.w = saturate(r0.w / r1.x);
      r0.z = cmp(r0.w >= r0.z);
      r0.z = r0.z ? 0.750000 : 0;
      r4.w = r4.y + r0.z;
      r0.z = cmp(r0.y < 0);
      r0.y = r0.y * 1.17647064 + r2.x;
      r0.y = cmp(0 < r0.y);
      r0.y = r0.y ? r0.z : 0;
      r1.xyw = float3(0.200000003,0.200000003,0.200000003) + r4.xwz;
      r0.yzw = r0.yyy ? r1.xyw : r4.xwz;
      r1.x = dot(r0.yzw, float3(1,1,1));
      r1.x = cmp(r1.x == 0.000000);
      r1.yw = cmp(abs(r2.yy) < float2(0.0222222228,0.5));
      r2.xz = float2(0.5,1.5) * abs(r2.zz);
      r1.y = r1.y ? r2.z : r2.x;
      r1.w = r1.w ? 0.600000024 : 0.300000012;
      r2.x = round(r2.y);
      r2.x = r2.y + -r2.x;
      r1.y = cmp(abs(r2.x) < r1.y);
      r1.y = r1.y ? 1.000000 : 0;
      r2.xyz = r1.www * r1.yyy + r0.yzw;
      r0.yzw = r1.xxx ? r2.xyz : r0.yzw;
      r1.x = log2(r1.z);
      r1.x = 30 + r1.x;
      r1.x = saturate(0.0222222228 * r1.x);
      r0.x = -r1.x * 0.999023438 + r0.x;
      r0.x = cmp(abs(r0.x) < abs(r2.w));
      r1.xyz = r0.yzw * float3(0.5,0.5,0.5) + float3(0.100000001,0.100000001,0.100000001);
      r3.xyz = r0.xxx ? r1.xyz : r0.yzw;
    } else {
      r3.xyz = float3(0,0,0);
    }
  }
  //r0.xyz = min(float3(1,1,1), r3.xyz);  // clamp to SDR
  r0.w = dot(r0.xyz, float3(1,1,1));
  r0.w = cmp(0 < r0.w);
  o0.w = r0.w ? 1 : 0.100000001;
  o0.xyz = r0.xyz;
  return;
}