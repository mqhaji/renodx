// ---- Created with 3Dmigoto v1.3.16 on Sat May 18 21:42:59 2024

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

cbuffer cb_g_AdaptLuminance : register(b5)
{

  struct
  {
    float4 m_Params;
    float4 m_AdaptionParams;
    float4 m_EVAdjustments[32];
    int4 m_ScreenWidthHeight;
  } g_AdaptLuminance : packoffset(c0);

}

SamplerState s_TrilinearClamp_s : register(s12);
Texture2D<float4> t_Tex0 : register(t0);
RWStructuredBuffer<AdaptLuminanceData> g_AdaptionData : register(u1);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.x = g_AdaptionData[0].Exposure;
  r0.y = g_AdaptionData[0].ExposureDelta;
  r0.z = g_AdaptionData[0].Exposure;
  r0.z = 9.99999994e-009 + r0.z;
  r0.z = log2(r0.z);
  r1.xy = t_Tex0.SampleLevel(s_TrilinearClamp_s, float2(0.5,0.5), 10).xw;
  r0.w = r1.x / r1.y;
  r1.w = exp2(r0.w);
  r0.w = 5464 * r1.w;
  r0.w = log2(r0.w);
  r0.w = max(g_AdaptLuminance.m_Params.z, r0.w);
  r0.w = min(g_AdaptLuminance.m_Params.w, r0.w);
  r0.w = exp2(r0.w);
  r0.w = 0.000183016105 * r0.w;
  r1.z = 1 / r0.w;
  r0.w = 9.99999994e-009 + r1.z;
  r0.w = log2(r0.w);
  r0.w = r0.w + -r0.z;
  r0.x = cmp(r1.z < r0.x);
  r0.x = r0.x ? g_AdaptLuminance.m_AdaptionParams.x : g_AdaptLuminance.m_AdaptionParams.y;
  r2.x = 11.6999998 / r0.x;
  r0.x = 1 / r0.x;
  r0.x = r0.x * r0.x;
  r0.x = 36 * r0.x;
  r2.y = r0.x * g_AdaptLuminance.m_Params.x + r2.x;
  r2.x = r2.x * g_AdaptLuminance.m_Params.x + 1;
  r2.y = r2.y * -r0.y;
  r0.w = r0.x * r0.w + r2.y;
  r0.x = g_AdaptLuminance.m_Params.x * r0.x;
  r0.x = r0.x * g_AdaptLuminance.m_Params.x + r2.x;
  r0.x = r0.w / r0.x;
  r0.y = r0.x * g_AdaptLuminance.m_Params.x + r0.y;
  r0.z = r0.y * g_AdaptLuminance.m_Params.x + r0.z;
  r0.z = exp2(r0.z);
  r0.x = -9.99999994e-009 + r0.z;
  r0.z = cmp(g_AdaptLuminance.m_Params.y == 1.000000);
  r1.x = 0;
  r1.xy = r0.zz ? r0.xy : r1.zx;
  g_AdaptionData[0].Exposure = r1.x;
  g_AdaptionData[0].ExposureDelta = r1.y;
  g_AdaptionData[0].LastExposureTarget = r1.z;
  g_AdaptionData[0].AverageLuminance = r1.w;
  r0.x = 1 / r1.x;
  r0.x = 5464 * r0.x;
  r0.x = log2(r0.x);
  r0.y = max(-10, r0.x);
  r0.y = min(21, r0.y);
  r0.z = floor(r0.x);
  r0.x = ceil(r0.x);
  r0.xz = (int2)r0.xz;
  r0.xz = max(int2(-10,-10), (int2)r0.xz);
  r0.xz = min(int2(21,21), (int2)r0.xz);
  r0.w = (int)r0.z;
  r0.xz = (int2)r0.xz + int2(10,10);
  r0.y = r0.y + -r0.w;
  r0.x = g_AdaptLuminance.m_EVAdjustments[r0.x].x + -g_AdaptLuminance.m_EVAdjustments[r0.z].x;
  r0.x = r0.y * r0.x + g_AdaptLuminance.m_EVAdjustments[r0.z].x;
  r0.x = exp2(r0.x);
  o0.xyz = r1.xxx * r0.xxx;
  o0.w = 1;
  return;
}