#include "./common.hlsl"

// ---- Created with 3Dmigoto v1.4.1 on Wed Jan  8 23:24:07 2025

cbuffer _Globals : register(b1) {
  struct {
    float velocityMagnitudeOutputTexture;
    float3 outputValue0;
    float3 historyValue0;
    float outputValue1;
    float historyValue1;
  } psOutput:
  packoffset(c0);
}

cbuffer ShaderConstants : register(b0) {
  uint g_width : packoffset(c0);
  uint g_height : packoffset(c0.y);
  float g_invWidth : packoffset(c0.z);
  float g_invHeight : packoffset(c0.w);
  uint g_debugMode : packoffset(c1);
  float g_debugParams : packoffset(c1.y);
  float g_minHistoryBlendFactor : packoffset(c1.z);
  float g_maxHistoryBlendFactor : packoffset(c1.w);
  float g_exposureMultiplier : packoffset(c2);
  float g_unexposureMultiplier : packoffset(c2.y);
  float g_historyUnexposureMultiplier : packoffset(c2.z);
  float g_disocclusionRejectionFactor : packoffset(c2.w);
  float g_motionSharpeningFactor : packoffset(c3);
  float g_antiflickerMultiplier : packoffset(c3.y);
  float g_antiflickerInDistance : packoffset(c3.z);
  float g_antiflickerOutDistance : packoffset(c3.w);
  float4x4 g_currentToPrevFrameTransform : packoffset(c4);
  float4x4 g_invJitteredViewProjection : packoffset(c8);
  float4x4 g_invUnjitteredViewProjection : packoffset(c12);
  float4 g_neighborWeights[3] : packoffset(c16);
  float4 g_neighborLowWeights[3] : packoffset(c19);
  float4 g_outputQuantizationBias : packoffset(c22);
  float g_lumaContrastFactor : packoffset(c23);
  float3 pad : packoffset(c23.y);
}

SamplerState g_pointSampler_s : register(s0);
SamplerState g_linearSampler_s : register(s1);
Texture2D<float> g_depthTexture : register(t0);
Texture2D<float2> g_velocityTexture : register(t1);
Texture2D<float4> g_inputTexture0 : register(t3);
Texture2D<float4> g_historyInputTexture0 : register(t4);
Texture2D<float4> g_inputTexture1 : register(t5);
Texture2D<float4> g_historyInputTexture1 : register(t6);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position0,
    out float o0: SV_Target0,
    out float3 o1: SV_Target1,
    out float3 o2: SV_Target2,
    out float o3: SV_Target3,
    out float o4: SV_Target4) {
  float4 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15, r16, r17;

  r0.xy = (uint2)v0.xy;
  r1.xyzw = (int4)r0.xyxy;
  r1.xyzw = float4(0.5, 0.5, 0.5, 0.5) + r1.xyzw;
  r2.xyzw = g_invWidth * r1.xyzw;
  r0.zw = float2(0, 0);
  r1.x = g_depthTexture.Load(r0.xyw).x;
  r3.xyzw = (int4)r0.xyxy + int4(1, -1, -1, -1);
  r4.xy = r3.zw;
  r4.zw = float2(0, 0);
  r1.y = g_depthTexture.Load(r4.xyw).x;
  r3.zw = float2(0, 0);
  r5.x = g_depthTexture.Load(r3.xyw).x;
  r6.xyzw = (int4)r0.xyxy + int4(1, 1, -1, 1);
  r7.xy = r6.zw;
  r7.zw = float2(0, 0);
  r5.y = g_depthTexture.Load(r7.xyw).x;
  r6.zw = float2(0, 0);
  r5.z = g_depthTexture.Load(r6.xyw).x;
  r5.w = max(r5.x, r1.y);
  r8.x = max(r5.y, r5.z);
  r8.y = max(r8.x, r5.w);
  r1.x = cmp(r1.x < r8.y);
  if (r1.x != 0) {
    r9.xyzw = float4(1, -1, -1, 1) * g_invWidth;
    r1.x = cmp(r1.y < r5.x);
    r1.xy = r1.xx ? r9.xy : -g_invWidth;
    r5.x = cmp(r5.y < r5.z);
    r5.xy = r5.xx ? g_invWidth : r9.zw;
    r5.z = cmp(r5.w < r8.x);
    r1.xy = r5.zz ? r5.xy : r1.xy;
    r1.xy = r1.zw * g_invWidth + r1.xy;
    r5.xy = g_width;
    r1.xy = r5.xy * r1.xy;
    r5.xy = (int2)r1.xy;
  } else {
    r5.xy = r0.xy;
  }
  r5.z = 0;
  r1.xy = g_velocityTexture.Load(r5.xyz).xy;
  r1.xy = float2(0.5, -0.5) * r1.xy;
  r1.zw = r1.zw * g_invWidth + -r1.xy;
  r5.xy = r1.zw * float2(2, 2) + float2(-1, -1);
  r5.zw = g_width;
  r1.xy = r5.zw * -r1.xy;
  r5.xy = g_invWidth * float2(2, 2) + abs(r5.xy);
  r5.x = max(r5.x, r5.y);
  r5.x = cmp(r5.x >= 1);
  r1.x = abs(r1.x) + abs(r1.y);
  r1.x = saturate(g_motionSharpeningFactor * r1.x);
  r1.zw = saturate(r1.zw);
  r4.xyz = g_inputTexture0.Load(r4.xyz).xyz;
  r8.xyzw = (int4)r0.xyxy + int4(-1, 0, 0, -1);
  r9.xy = r8.zw;
  r9.zw = float2(0, 0);
  r9.xyz = g_inputTexture0.Load(r9.xyz).xyz;
  r3.xyz = g_inputTexture0.Load(r3.xyz).xyz;
  r8.zw = float2(0, 0);
  r8.xyz = g_inputTexture0.Load(r8.xyz).xyz;
  r10.xyz = g_inputTexture0.Load(r0.xyz).xyz;
  r0.xyzw = (int4)r0.xyxy + int4(0, 1, 1, 0);
  r11.xy = r0.zw;
  r11.zw = float2(0, 0);
  r11.xyz = g_inputTexture0.Load(r11.xyz).xyz;
  r7.xyz = g_inputTexture0.Load(r7.xyz).xyz;
  r0.zw = float2(0, 0);
  r0.xyz = g_inputTexture0.Load(r0.xyz).xyz;
  r6.xyz = g_inputTexture0.Load(r6.xyz).xyz;
  r0.w = renodx::color::y::from::BT709(r4.xyz);  // fix incorrect BT.601 luma coefficients
  r0.w = 1 + r0.w;
  r0.w = 1 / r0.w;
  r4.xyz = r4.xyz * r0.www;
  r0.w = renodx::color::y::from::BT709(r9.xyz);  // fix incorrect BT.601 luma coefficients
  r0.w = 1 + r0.w;
  r0.w = 1 / r0.w;
  r9.xyz = r9.xyz * r0.www;
  r0.w = renodx::color::y::from::BT709(r3.xyz);  // fix incorrect BT.601 luma coefficients
  r0.w = 1 + r0.w;
  r0.w = 1 / r0.w;
  r3.xyz = r3.xyz * r0.www;
  r0.w = renodx::color::y::from::BT709(r8.xyz);  // fix incorrect BT.601 luma coefficients
  r0.w = 1 + r0.w;
  r0.w = 1 / r0.w;
  r8.xyz = r8.xyz * r0.www;
  r0.w = renodx::color::y::from::BT709(r10.xyz);  // fix incorrect BT.601 luma coefficients
  r0.w = 1 + r0.w;
  r0.w = 1 / r0.w;
  r12.xyz = r10.xyz * r0.www;
  r1.y = renodx::color::y::from::BT709(r11.xyz);  // fix incorrect BT.601 luma coefficients
  r1.y = 1 + r1.y;
  r1.y = 1 / r1.y;
  r11.xyz = r11.xyz * r1.yyy;
  r1.y = renodx::color::y::from::BT709(r7.xyz);  // fix incorrect BT.601 luma coefficients
  r1.y = 1 + r1.y;
  r1.y = 1 / r1.y;
  r7.xyz = r7.xyz * r1.yyy;
  r1.y = renodx::color::y::from::BT709(r0.xyz);  // fix incorrect BT.601 luma coefficients
  r1.y = 1 + r1.y;
  r1.y = 1 / r1.y;
  r0.xyz = r1.yyy * r0.xyz;
  r1.y = renodx::color::y::from::BT709(r6.xyz);  // fix incorrect BT.601 luma coefficients
  r1.y = 1 + r1.y;
  r1.y = 1 / r1.y;
  r6.xyz = r6.xyz * r1.yyy;
  r13.xy = r1.zw * r5.zw + float2(-0.5, -0.5);
  r13.xy = floor(r13.xy);
  r14.xyzw = float4(0.5, 0.5, -0.5, -0.5) + r13.xyxy;
  r5.yz = r1.zw * r5.zw + -r14.xy;
  r13.zw = r5.yz * r5.yz;
  r15.xy = r13.zw * r5.yz;
  r15.zw = r13.zw * r5.yz + r5.yz;
  r15.zw = -r15.zw * float2(0.5, 0.5) + r13.zw;
  r16.xy = float2(2.5, 2.5) * r13.zw;
  r15.xy = r15.xy * float2(1.5, 1.5) + -r16.xy;
  r15.xy = float2(1, 1) + r15.xy;
  r5.yz = r13.zw * r5.yz + -r13.zw;
  r13.zw = float2(0.5, 0.5) * r5.yz;
  r16.xy = float2(1, 1) + -r15.zw;
  r16.xy = r16.xy + -r15.xy;
  r5.yz = -r5.yz * float2(0.5, 0.5) + r16.xy;
  r15.xy = r15.xy + r5.yz;
  r16.xw = g_invWidth * r14.zw;
  r5.yz = r5.yz / r15.xy;
  r5.yz = r14.xy + r5.yz;
  r16.yz = g_invHeight * r5.zy;
  r5.yz = float2(2.5, 2.5) + r13.xy;
  r14.xy = g_invWidth * r5.yz;
  r5.yzw = g_historyInputTexture0.SampleLevel(g_pointSampler_s, r16.xw, 0).xyz;
  r5.yzw = r5.yzw * r15.zzz;
  r17.xyz = g_historyInputTexture0.SampleLevel(g_linearSampler_s, r16.zw, 0).xyz;
  r17.xyz = r17.xyz * r15.xxx;
  r17.xyz = r17.xyz * r15.www;
  r5.yzw = r5.yzw * r15.www + r17.xyz;
  r14.zw = r16.wy;
  r17.xyz = g_historyInputTexture0.SampleLevel(g_pointSampler_s, r14.xz, 0).xyz;
  r17.xyz = r17.xyz * r13.zzz;
  r5.yzw = r17.xyz * r15.www + r5.yzw;
  r17.xyz = g_historyInputTexture0.SampleLevel(g_linearSampler_s, r16.xy, 0).xyz;
  r17.xyz = r17.xyz * r15.zzz;
  r5.yzw = r17.xyz * r15.yyy + r5.yzw;
  r17.xyz = g_historyInputTexture0.SampleLevel(g_linearSampler_s, r16.zy, 0).xyz;
  r17.xyz = r17.xyz * r15.xxx;
  r5.yzw = r17.xyz * r15.yyy + r5.yzw;
  r17.xyz = g_historyInputTexture0.SampleLevel(g_linearSampler_s, r14.xw, 0).xyz;
  r17.xyz = r17.xyz * r13.zzz;
  r5.yzw = r17.xyz * r15.yyy + r5.yzw;
  r16.y = r14.y;
  r17.xyz = g_historyInputTexture0.SampleLevel(g_pointSampler_s, r16.xy, 0).xyz;
  r15.yzw = r17.xyz * r15.zzz;
  r5.yzw = r15.yzw * r13.www + r5.yzw;
  r15.yzw = g_historyInputTexture0.SampleLevel(g_linearSampler_s, r16.zy, 0).xyz;
  r15.xyz = r15.yzw * r15.xxx;
  r5.yzw = r15.xyz * r13.www + r5.yzw;
  r14.xyz = g_historyInputTexture0.SampleLevel(g_pointSampler_s, r14.xy, 0).xyz;
  r13.xyz = r14.xyz * r13.zzz;
  r5.yzw = r13.xyz * r13.www + r5.yzw;
  r1.y = g_historyUnexposureMultiplier * g_exposureMultiplier;
  r5.yzw = r5.yzw * r1.yyy;
  r3.w = renodx::color::y::from::BT709(r5.yzw);  // fix incorrect BT.601 luma coefficients
  r3.w = 1 + r3.w;
  r3.w = 1 / r3.w;
  r13.xyz = r5.yzw * r3.www;
  if (r5.x != 0) {
    r14.xyz = r12.xyz;
    r15.xyz = r12.xyz;
  } else {
    r16.xyz = g_neighborWeights[0].yyy * r9.xyz;
    r16.xyz = r4.xyz * g_neighborWeights[0].xxx + r16.xyz;
    r16.xyz = r3.xyz * g_neighborWeights[0].zzz + r16.xyz;
    r16.xyz = r8.xyz * g_neighborWeights[0].www + r16.xyz;
    r16.xyz = r12.xyz * g_neighborWeights[1].xxx + r16.xyz;
    r16.xyz = r11.xyz * g_neighborWeights[1].yyy + r16.xyz;
    r16.xyz = r7.xyz * g_neighborWeights[1].zzz + r16.xyz;
    r16.xyz = r0.xyz * g_neighborWeights[1].www + r16.xyz;
    r14.xyz = r6.xyz * g_neighborWeights[2].xxx + r16.xyz;
    r16.xyz = g_neighborLowWeights[0].yyy * r9.xyz;
    r16.xyz = r4.xyz * g_neighborLowWeights[0].xxx + r16.xyz;
    r16.xyz = r3.xyz * g_neighborLowWeights[0].zzz + r16.xyz;
    r16.xyz = r8.xyz * g_neighborLowWeights[0].www + r16.xyz;
    r16.xyz = r12.xyz * g_neighborLowWeights[1].xxx + r16.xyz;
    r16.xyz = r11.xyz * g_neighborLowWeights[1].yyy + r16.xyz;
    r16.xyz = r7.xyz * g_neighborLowWeights[1].zzz + r16.xyz;
    r16.xyz = r0.xyz * g_neighborLowWeights[1].www + r16.xyz;
    r15.xyz = r6.xyz * g_neighborLowWeights[2].xxx + r16.xyz;
  }
  r16.xyz = min(r4.xyz, r3.xyz);
  r17.xyz = min(r7.xyz, r6.xyz);
  r16.xyz = min(r17.xyz, r16.xyz);
  r3.xyz = max(r4.xyz, r3.xyz);
  r4.xyz = max(r7.xyz, r6.xyz);
  r3.xyz = max(r4.xyz, r3.xyz);
  r4.xyz = min(r9.xyz, r0.xyz);
  r6.xyz = min(r11.xyz, r8.xyz);
  r4.xyz = min(r6.xyz, r4.xyz);
  r4.xyz = min(r4.xyz, r12.xyz);
  r0.xyz = max(r9.xyz, r0.xyz);
  r6.xyz = max(r11.xyz, r8.xyz);
  r0.xyz = max(r6.xyz, r0.xyz);
  r0.xyz = max(r0.xyz, r12.xyz);
  r6.xyz = min(r16.xyz, r4.xyz);
  r4.xyz = float3(0.5, 0.5, 0.5) * r4.xyz;
  r4.xyz = r6.xyz * float3(0.5, 0.5, 0.5) + r4.xyz;
  r3.xyz = max(r3.xyz, r0.xyz);
  r0.xyz = float3(0.5, 0.5, 0.5) * r0.xyz;
  r0.xyz = r3.xyz * float3(0.5, 0.5, 0.5) + r0.xyz;

  // fix incorrect BT.601 luma coefficients
  r3.x = renodx::color::y::from::BT709(r4.xyz);
  r3.y = renodx::color::y::from::BT709(r0.xyz);
  r3.z = renodx::color::y::from::BT709(r13.xyz);

  r4.w = r3.y + -r3.x;
  r4.w = max(0, r4.w);
  r4.w = r4.w + r4.w;
  r6.x = r3.y + r3.x;
  r6.x = max(9.99999975e-06, r6.x);
  r4.w = r4.w / r6.x;
  r6.x = r4.w * g_lumaContrastFactor + 1;
  r6.x = 1 / r6.x;
  r6.x = saturate(r1.x * 0.5 + r6.x);
  r6.yzw = r10.xyz * r0.www + -r14.xyz;
  r6.xyz = r6.xxx * r6.yzw + r14.xyz;
  r0.w = r3.x + -r3.z;
  r3.x = r3.y + -r3.z;
  r0.w = min(abs(r3.x), abs(r0.w));
  r3.x = g_antiflickerOutDistance + -g_antiflickerInDistance;
  r0.w = -g_antiflickerInDistance + r0.w;
  r3.x = 1 / r3.x;
  r0.w = saturate(r3.x * r0.w);
  r3.y = r0.w * -2 + 3;
  r0.w = r0.w * r0.w;
  r0.w = r3.y * r0.w + r1.x;
  r0.w = g_antiflickerMultiplier + r0.w;
  r0.w = 0.125 * r0.w;
  r3.y = r1.x * r0.w;
  r3.y = r3.y * 8 + 1;
  r0.w = r3.y * r0.w;
  r3.y = 1 + r4.w;
  r0.w = saturate(r0.w / r3.y);
  r0.w = max(g_minHistoryBlendFactor, r0.w);
  r0.w = min(g_maxHistoryBlendFactor, r0.w);
  if (r5.x != 0) {
    r7.xyz = r6.xyz;
  } else {
    r4.xyz = min(r15.xyz, r4.xyz);
    r0.xyz = max(r15.xyz, r0.xyz);
    r8.xyz = r0.xyz + -r4.xyz;
    r0.xyz = r4.xyz + r0.xyz;
    r3.yzw = -r5.yzw * r3.www + r15.xyz;
    r0.xyz = -r0.xyz * float3(0.5, 0.5, 0.5) + r13.xyz;
    r4.xyz = float3(1, 1, 1) / r3.yzw;
    r5.yzw = r8.xyz * float3(0.5, 0.5, 0.5) + -r0.xyz;
    r5.yzw = r5.yzw * r4.xyz;
    r0.xyz = -r8.xyz * float3(0.5, 0.5, 0.5) + -r0.xyz;
    r0.xyz = r0.xyz * r4.xyz;
    r0.xyz = min(r5.yzw, r0.xyz);
    r0.x = max(r0.x, r0.y);
    r0.x = saturate(max(r0.x, r0.z));
    r7.xyz = r0.xxx * r3.yzw + r13.xyz;
  }
  r0.xyz = -r7.xyz + r6.xyz;
  r0.xyz = r0.www * r0.xyz + r7.xyz;
  r0.w = renodx::color::y::from::BT709(r0.xyz);  // fix incorrect BT.601 luma coefficients
  r0.w = 1 + -r0.w;
  r0.w = 1 / r0.w;
  r0.xyz = r0.xyz * r0.www;
  r3.yzw = (int3)r0.xyz & int3(0x7f800000, 0x7f800000, 0x7f800000);
  r0.xyz = r3.yzw * g_outputQuantizationBias.xyz + r0.xyz;
  r3.yzw = cmp(r0.xyz != r0.xyz);
  r0.xyz = r3.yzw ? float3(0, 0, 0) : r0.xyz;
  r4.xyzw = g_invWidth * float4(-0.5, -0.5, 0.5, -0.5) + r2.zwzw;
  r3.yzw = g_inputTexture1.Gather(g_pointSampler_s, r4.xy).xyw;
  r4.xy = g_inputTexture1.Gather(g_pointSampler_s, r4.zw).zw;
  r2.xyzw = g_invWidth * float4(0.5, 0.5, -0.5, 0.5) + r2.xyzw;
  r2.xy = g_inputTexture1.Gather(g_pointSampler_s, r2.xy).yz;
  r2.zw = g_inputTexture1.Gather(g_pointSampler_s, r2.zw).xy;
  r0.w = g_historyInputTexture1.SampleLevel(g_linearSampler_s, r1.zw, 0).x;
  r1.z = r0.w * r1.y;
  if (r5.x != 0) {
    r1.w = r3.z;
  } else {
    r4.z = g_neighborWeights[0].y * r4.y;
    r4.z = r3.w * g_neighborWeights[0].x + r4.z;
    r4.z = r4.x * g_neighborWeights[0].z + r4.z;
    r4.z = r3.y * g_neighborWeights[0].w + r4.z;
    r4.z = r3.z * g_neighborWeights[1].x + r4.z;
    r4.z = r2.y * g_neighborWeights[1].y + r4.z;
    r4.z = r2.z * g_neighborWeights[1].z + r4.z;
    r4.z = r2.w * g_neighborWeights[1].w + r4.z;
    r1.w = r2.x * g_neighborWeights[2].x + r4.z;
  }
  r4.z = min(r4.x, r3.w);
  r4.w = min(r2.z, r2.x);
  r4.z = min(r4.z, r4.w);
  r3.w = max(r4.x, r3.w);
  r2.x = max(r2.z, r2.x);
  r2.x = max(r3.w, r2.x);
  r2.z = min(r4.y, r2.w);
  r3.w = min(r3.y, r2.y);
  r2.z = min(r3.w, r2.z);
  r2.z = min(r2.z, r3.z);
  r2.w = max(r4.y, r2.w);
  r2.y = max(r3.y, r2.y);
  r2.y = max(r2.w, r2.y);
  r2.y = max(r2.y, r3.z);
  r2.w = min(r4.z, r2.z);
  r2.z = 0.5 * r2.z;
  r2.z = r2.w * 0.5 + r2.z;
  r2.x = max(r2.x, r2.y);
  r2.y = 0.5 * r2.y;
  r2.x = r2.x * 0.5 + r2.y;
  r2.y = r2.x + -r2.z;
  r2.y = max(0, r2.y);
  r2.yw = r2.yx + r2.yz;
  r2.w = max(9.99999975e-06, r2.w);
  r2.y = r2.y / r2.w;
  r2.w = -r0.w * r1.y + r2.z;
  r0.w = -r0.w * r1.y + r2.x;
  r0.w = min(abs(r2.w), abs(r0.w));
  r0.w = -g_antiflickerInDistance + r0.w;
  r0.w = saturate(r0.w * r3.x);
  r1.y = r0.w * -2 + 3;
  r0.w = r0.w * r0.w;
  r0.w = r1.y * r0.w + r1.x;
  r0.w = g_antiflickerMultiplier + r0.w;
  r0.w = 0.125 * r0.w;
  r1.x = r1.x * r0.w;
  r1.x = r1.x * 8 + 1;
  r0.w = r1.x * r0.w;
  r1.x = 1 + r2.y;
  r0.w = saturate(r0.w / r1.x);
  r0.w = max(g_minHistoryBlendFactor, r0.w);
  r0.w = min(g_maxHistoryBlendFactor, r0.w);
  if (r5.x != 0) {
    r1.x = r1.w;
  } else {
    r1.y = max(r2.z, r1.z);
    r1.x = min(r1.y, r2.x);
  }
  r1.y = r1.w + -r1.x;
  r0.w = r0.w * r1.y + r1.x;
  r1.x = cmp(r0.w != r0.w);
  r0.w = r1.x ? 0 : r0.w;
  o1.xyz = r0.xyz;
  o2.xyz = r0.xyz;
  o0.x = psOutput.velocityMagnitudeOutputTexture;
  o3.x = r0.w;
  o4.x = r0.w;
  return;
}
