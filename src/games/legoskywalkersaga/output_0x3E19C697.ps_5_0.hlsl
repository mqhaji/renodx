#include "./shared.h"
// ---- Created with 3Dmigoto v1.4.1 on Fri Aug 22 07:08:30 2025

// clang-format off
cbuffer g_ResolveParams_CB : register(b0) {
  struct
  {
    float4 neo_4k_params;
    float4 hdr_params;
  } g_ResolveParams: packoffset(c0);
}
// clang-format on

SamplerState texture0_ss_s : register(s0);
Texture2D<float4> texture0 : register(t0);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position0,
    float4 v1: TEXCOORD0,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2, r3, r4, r5;
  uint4 bitmask, uiDest;
  float4 fDest;

  o0.w = 0;

  r0.xyz = texture0.Sample(texture0_ss_s, v1.xy).xyz;

  o0.rgb = renodx::color::srgb::EncodeSafe(r0.rgb);
  return;
  // o0.rgb *= RENODX_GRAPHICS_WHITE_NITS / 80.f;

  r0.w = max(r0.x, r0.y);
  r0.w = max(r0.w, r0.z);
  r0.w = -2 + r0.w;
  r0.w = saturate(0.125 * r0.w);
  r1.x = dot(float3(0.753844976, 0.198593006, 0.0475619994), r0.xyz);
  r1.y = dot(float3(0.0457456, 0.941776991, 0.0124771995), r0.xyz);
  r1.z = dot(float3(-0.00121054996, 0.0176040996, 0.983606994), r0.xyz);
  r1.xyz = r1.xyz * r0.www;
  r0.w = 1 + -r0.w;
  r2.x = dot(float3(0.627403975, 0.329281986, 0.0433136001), r0.xyz);
  r2.y = dot(float3(0.0457456, 0.941776991, 0.0124771995), r0.xyz);
  r2.z = dot(float3(-0.00121054996, 0.0176040996, 0.983606994), r0.xyz);
  r1.xyz = r0.www * r2.xyz + r1.xyz;
  r1.xyz = g_ResolveParams.hdr_params.www * r1.xyz;
  r0.w = 1 + -g_ResolveParams.hdr_params.w;
  r2.x = dot(float3(0.627402008, 0.329291999, 0.0433060005), r0.xyz);
  r2.y = dot(float3(0.0690950006, 0.919543982, 0.0113599999), r0.xyz);
  r2.z = dot(float3(0.0163940005, 0.0880279988, 0.895578027), r0.xyz);
  r0.xyz = r0.www * r2.xyz + r1.xyz;
  r0.xyz = g_ResolveParams.hdr_params.xxx * r0.xyz;
  r0.xyz = float3(9.99999975e-05, 9.99999975e-05, 9.99999975e-05) * r0.xyz;
  r0.xyz = log2(abs(r0.xyz));
  r0.xyz = float3(0.159301758, 0.159301758, 0.159301758) * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  r1.xyz = r0.xyz * float3(18.8515625, 18.8515625, 18.8515625) + float3(0.8359375, 0.8359375, 0.8359375);
  r0.xyz = r0.xyz * float3(18.6875, 18.6875, 18.6875) + float3(1, 1, 1);
  r0.xyz = r1.xyz / r0.xyz;
  r0.xyz = log2(r0.xyz);
  r0.xyz = float3(78.84375, 78.84375, 78.84375) * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  r0.w = -20 + g_ResolveParams.hdr_params.y;
  r0.w = 9.99999975e-05 * r0.w;
  r0.w = log2(abs(r0.w));
  r0.w = 0.159301758 * r0.w;
  r0.w = exp2(r0.w);
  r1.xy = r0.ww * float2(18.8515625, 18.6875) + float2(0.8359375, 1);
  r0.w = r1.x / r1.y;
  r0.w = log2(r0.w);
  r0.w = 78.84375 * r0.w;
  r0.w = exp2(r0.w);
  r1.xyz = r0.xyz + -r0.www;
  r1.w = 0.771707892 + -r0.w;
  r1.xyz = saturate(r1.xyz / r1.www);
  r1.w = 9.99999975e-05 * g_ResolveParams.hdr_params.y;
  r1.w = log2(abs(r1.w));
  r1.w = 0.159301758 * r1.w;
  r1.w = exp2(r1.w);
  r2.xy = r1.ww * float2(18.8515625, 18.6875) + float2(0.8359375, 1);
  r1.w = r2.x / r2.y;
  r1.w = log2(r1.w);
  r1.w = 78.84375 * r1.w;
  r1.w = exp2(r1.w);
  r2.xyz = r1.www * r1.xyz;
  r3.xyz = float3(1, 1, 1) + -r1.xyz;
  r4.xyz = r1.www * r3.xyz + r2.xyz;
  r2.xyz = r0.www * r3.xyz + r2.xyz;
  r5.xyz = cmp(r0.www < r0.xyz);
  r1.xyz = r4.xyz * r1.xyz;
  r1.xyz = r2.xyz * r3.xyz + r1.xyz;
  r1.xyz = min(r1.xyz, r0.xyz);
  o0.xyz = r5.xyz ? r1.xyz : r0.xyz;

  // r0.rgb = renodx::color::gamma::EncodeSafe(r0.rgb);
  // o0.rgb = r0.rgb;
  // return;

  return;
}
