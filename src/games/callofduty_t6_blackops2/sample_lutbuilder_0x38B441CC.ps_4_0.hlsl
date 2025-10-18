#include "./shared.h"

// ---- Created with 3Dmigoto v1.4.1 on Sat Oct 18 15:10:49 2025

SamplerState codeTexture0_s : register(s0);
SamplerState codeTexture1_s : register(s1);
SamplerState codeTexture2_s : register(s2);
Texture2D<float4> codeTexture2 : register(t0);
Texture2D<float4> codeTexture0 : register(t1);
Texture2D<float4> codeTexture1 : register(t2);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position,
    float2 v1: TEXCOORD,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = codeTexture2.Sample(codeTexture2_s, v1.xy).xyzw;

  float3 color_input = r0.rgb;

  r0.xyz = r0.xyz * r0.xyz;
  r1.xyzw = codeTexture0.Sample(codeTexture0_s, v1.xy).xyzw;
  r0.xyz = r0.xyz * float3(4, 4, 4) + r1.xyz;
  r0.xyz = sqrt(r0.xyz);
  r1.xyz = r0.xyz * float3(-5.77078009, -5.77078009, -5.77078009) + float3(4.32808495, 4.32808495, 4.32808495);
  r1.xyz = exp2(r1.xyz);
  r1.xyz = float3(1, 1, 1) + -r1.xyz;
  r1.xyz = r1.xyz * float3(0.25, 0.25, 0.25) + float3(0.75, 0.75, 0.75);
  r2.xyz = cmp(float3(0.75, 0.75, 0.75) < r0.xyz);
  r0.xyz = r2.xyz ? r1.xyz : r0.xyz;
  r1.xyz = r0.xzy * float3(31, 31, 0.96875) + float3(0.5, 0, 0.015625);
  r0.x = 31 * r0.z;
  r0.x = frac(r0.x);
  r0.y = floor(r1.y);
  r1.w = 0;
  r0.yz = r0.yy * float2(32, 32) + r1.wx;
  r2.x = r1.x;
  r2.y = 32;
  r0.yz = r2.xy + r0.yz;
  r1.xy = float2(0.0009765625, 0.0009765625) * r0.yz;
  r2.xyzw = codeTexture1.Sample(codeTexture1_s, r1.yz).xyzw;
  r1.xyzw = codeTexture1.Sample(codeTexture1_s, r1.xz).xyzw;
  r0.yzw = r2.xyz + -r1.xyz;
  r0.xyz = r0.xxx * r0.yzw + r1.xyz;
  o0.w = dot(r0.xyz, float3(0.298999995, 0.587000012, 0.114));
  o0.xyz = r0.xyz;

  // o0.rgb = color_input;
  return;
}
