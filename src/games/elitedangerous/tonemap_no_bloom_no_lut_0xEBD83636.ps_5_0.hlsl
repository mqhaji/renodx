#include "./common.hlsli"

Texture2D<float4> t0 : register(t0);

SamplerState s0_s : register(s0);

cbuffer cb2 : register(b2) {
  float4 cb2[11];
}

// 3Dmigoto declarations
#define cmp -

void main(
    float2 v0: TEXCOORD1,
    out float4 o0: SV_TARGET0) {
  float4 r0, r1, r2, r3;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = t0.Sample(s0_s, v0.xy).xyzw;
  r0.xyzw = cb2[2].xxxx * r0.xyzw;
  r0.xyzw = max(float4(0, 0, 0, 0), r0.xyzw);
  r1.x = 64500 * cb2[2].x;

  float3 tonemapped;
  if (RENODX_TONE_MAP_TYPE == 0.f) {
    r0.xyzw = min(r1.xxxx, r0.xyzw);
    r1.xyz = cb2[7].xxx * r0.xyz;
    r1.xyz = lerp(1.f, cb2[6].yyy, CUSTOM_AUTO_EXPOSURE) * r1.xyz;  // r1.xyz = cb2[6].yyy * r1.xyz;
    r1.rgb = ApplyVanillaTonemap(r1.rgb);
    tonemapped = r1.rgb;
  } else {
    r1.xyz = cb2[7].xxx * r0.xyz;
    r1.xyz = lerp(1.f, cb2[6].yyy, CUSTOM_AUTO_EXPOSURE) * r1.xyz;  // r1.xyz = cb2[6].yyy * r1.xyz;
    ApplyUserToneMap(r1.rgb, tonemapped);
    tonemapped = renodx::color::gamma::EncodeSafe(r1.rgb);
  }

  r1.rgb = tonemapped;

  r1.rgb = renodx::math::SignPow(r1.rgb, 2.2f);
  r1.xyz = (r1.xyz * cb2[10].yyy + cb2[9].yyy);  // remove saturate
  r1.w = renodx::color::y::from::BT709(r1.xyz);  //  r1.w = dot(r1.xyz, float3(0.308600008, 0.609399974, 0.0820000023));
  r1.xyz = r1.xyz + -r1.www;
  r1.xyz = cb2[10].zzz * r1.xyz + r1.www;
  r0.xyz = cb2[10].xxx * r1.xyz;
  // r0 = clamp(r0, 0, 64500);
  o0 = r0;

  o0.rgb = GameScaleAndGrainAndDisplayMap(o0.rgb, v0.xy);

  return;
}
