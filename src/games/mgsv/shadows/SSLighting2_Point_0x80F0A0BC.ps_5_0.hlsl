// MGSV point light with a dual-paraboloid shadow map - CRC32 0x80F0A0BC, ps_5_0.
//
// Original source: SSLighting2_Point_ps.hlsl
// Decompiled from the retail DXBC with 3Dmigoto (bin/cmd_Decompiler.exe) and
// augmented, like the sun and spot ports, with contact hardening and a
// screen-space shadow complement.
//
// Vanilla shadow filter (what PCSS replaces):
//   4 x SampleCmp(t3, s15) at +/-0.125 texels, dithered from the position hash,
//   then shadow = (1 - average)^2.
//   Fixed radius => no contact hardening.
//
// Shadow map: t3, 8192x4096 R24_UNORM_X8, two 4096x4096 dual-paraboloid
// hemispheres. The coordinate is the paraboloid projection of the direction to
// the light, and r9.w is the depth; there is no m_shadowProjection step here.
//
// ---- Created with 3Dmigoto v1.3.16 on Fri Oct 23 17:04:02 2026

// The addon's injected settings, which carry the tunables below.
#include "../shared.h"

// To iterate through the devkit instead, pin them here: that path replaces the
// shader without supplying cb13, and D3D11 returns 0 for an unbound constant
// buffer, so anything read from it - including the gate - comes back as 0.
//
// #undef  CUSTOM_POINT_CONTACT_HARDENING
// #define CUSTOM_POINT_CONTACT_HARDENING 1.0
// #undef  CUSTOM_POINT_SHADOW_SOFTNESS
// #define CUSTOM_POINT_SHADOW_SOFTNESS   0.10
// #undef  CUSTOM_POINT_SHADOW_K
// #define CUSTOM_POINT_SHADOW_K          20.0
// #undef  CUSTOM_POINT_BLUR_PIXELS
// #define CUSTOM_POINT_BLUR_PIXELS       24.0
// #undef  CUSTOM_POINT_SHADOW_DARKNESS
// #define CUSTOM_POINT_SHADOW_DARKNESS   1.0
// #undef  CUSTOM_SS_POINT_SHADOWS
// #define CUSTOM_SS_POINT_SHADOWS        1.0
// #undef  CUSTOM_SS_POINT_REACH
// #define CUSTOM_SS_POINT_REACH          0.05
// #undef  CUSTOM_SS_POINT_THICKNESS
// #define CUSTOM_SS_POINT_THICKNESS      0.005
// #undef  CUSTOM_SS_POINT_STRENGTH
// #define CUSTOM_SS_POINT_STRENGTH       2.0
// #undef  CUSTOM_SS_POINT_FADE_END
// #define CUSTOM_SS_POINT_FADE_END       40.0
// #undef  CUSTOM_SS_POINT_MATERIAL_MAX
// #define CUSTOM_SS_POINT_MATERIAL_MAX   0.25

#define POINT_MIN_TEXELS 6.0

// ---- Screen-space shadow complement -----------------------------------------
//
// Same march as the sun and spot, but once per point draw. The direction toward
// the light is per pixel and comes from m_lightParams[3].xyz, exactly as the spot
// block does; an omni light has a position, not a direction.
//
// A scene can hold several point lights and each draw runs this pass, so the
// term has its own toggle; it defaults on, like the spot term.
//
// These constants are the spot shader's, unchanged.
#define SS_STEPS        48
#define SS_NORMAL_BIAS  0.0
#define SS_REACH_MAX_M  6.00
#define SS_CASTER_MAX_M 0.25
#define SS_CASTER_PER_M 0.010

// Vogel disk, precomputed. Only the per-pixel rotation varies.
static const float2 kPcssVogelDisk[32] = {
  float2(0.125000000, 0.000000000),
  float2(-0.159645045, 0.146247939),
  float2(0.024436233, -0.278438271),
  float2(0.201222239, 0.262458779),
  float2(-0.369267557, -0.065318232),
  float2(0.349802466, -0.222515695),
  float2(-0.117002080, 0.435241902),
  float2(-0.223135653, -0.429634124),
  float2(0.484115115, 0.176798065),
  float2(-0.503641109, 0.207895727),
  float2(0.242788296, -0.518824482),
  float2(0.179414372, 0.572001296),
  float2(-0.540757005, -0.313379740),
  float2(0.634369524, -0.139464358),
  float2(-0.387145847, 0.550675125),
  float2(-0.089439652, -0.690199644),
  float2(0.549071755, 0.462758261),
  float2(-0.738878471, 0.030554941),
  float2(0.538955128, -0.536332332),
  float2(-0.036058190, 0.779791515),
  float2(-0.512817529, -0.614526796),
  float2(0.812359593, 0.109301839),
  float2(-0.688310643, 0.478908612),
  float2(0.188086063, -0.836061381),
  float2(0.435033261, 0.759191058),
  float2(-0.850448409, -0.271316244),
  float2(0.826102404, -0.381680257),
  float2(-0.357888208, 0.855155559),
  float2(-0.319407328, -0.888033760),
  float2(0.849908632, 0.446688166),
  float2(-0.944034648, 0.248844495),
  float2(0.536595815, -0.834529766),
};

float PcssInterleavedGradientNoise(float2 position_screen) {
  const float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
  return frac(magic.z * frac(dot(position_screen, magic.xy)));
}

cbuffer cPSScene : register(b2)
{

  struct
  {
    float4x4 m_projectionView;
    float4x4 m_projection;
    float4x4 m_view;
    float4x4 m_shadowProjection;
    float4x4 m_shadowProjection2;
    float4 m_eyepos;
    float4 m_projectionParam;
    float4 m_viewportSize;
    float4 m_exposure;
    float4 m_fogParam[3];
    float4 m_fogColor;
    float4 m_cameraCenterOffset;
    float4 m_shadowMapResolutions;
  } g_psScene : packoffset(c0);

}

cbuffer cPSLight : register(b3)
{

  struct
  {
    float4 m_lightParams[11];
  } g_psLight : packoffset(c0);

}

cbuffer cPSMaterial : register(b4)
{

  struct
  {
    float4 m_materials[8];
  } g_psMaterial : packoffset(c0);

}

SamplerState g_samplerPoint_Wrap_s : register(s8);
SamplerState g_samplerPoint_Clamp_s : register(s9);
SamplerState g_samplerLinear_Clamp_s : register(s11);
SamplerComparisonState g_samplerComparisonLess_Linear_Clmap_s : register(s15);
Texture2D<float4> g_NormalTexture : register(t0);
Texture2D<float4> g_SpecularTexture : register(t1);
Texture2D<float4> g_DepthTexture : register(t2);
Texture2D<float4> g_ShadowTexture : register(t3);
Texture3D<float4> g_lut2 : register(t8);
Texture2D<float4> g_lut1 : register(t9);
Texture2D<float4> g_MaterialTexture : register(t10);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float4 v1 : TEXCOORD0,
  float4 v2 : TEXCOORD1,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = float4(-0.5,-0.5,-0.5,-0.5) + v0.xyxy;
  r1.xyz = v2.xyw;
  r1.xyz = r1.xyz;
  r1.xy = r1.xy / r1.zz;
  r1.xy = r1.xy;
  r1.xy = r1.xy;
  r1.xy = r1.xy;
  r2.xyz = g_NormalTexture.Sample(g_samplerPoint_Wrap_s, r1.xy).xyz;
  r2.xyz = r2.xyz;
  r3.xyzw = g_SpecularTexture.Sample(g_samplerPoint_Wrap_s, r1.xy).yxzw;
  r1.x = g_DepthTexture.Sample(g_samplerPoint_Wrap_s, r1.xy).x;
  r1.x = r1.x;
  r2.xyz = r2.xyz;
  r1.yz = float2(2,2) * r2.xy;
  r1.yz = float2(-1,-1) + r1.yz;
  r1.w = r2.z * r2.z;
  r1.w = 2 * r1.w;
  r2.z = -1 + r1.w;
  r1.w = r2.z * r2.z;
  r1.w = -r1.w;
  r1.w = 1 + r1.w;
  r4.xy = r1.yz * r1.ww;
  r1.y = dot(r1.yz, r1.yz);
  r1.y = r1.w * r1.y;
  r1.y = 1.00000001e-007 + r1.y;
  r1.y = rsqrt(r1.y);
  r2.xy = r4.xy * r1.yy;
  r2.xy = r2.xy;
  r2.z = r2.z;
  r2.xyz = r2.xyz;
  r4.y = r3.y;
  r3.x = r3.x;
  r3.z = r3.z;
  r3.w = r3.w;
  r2.xyz = r2.xyz;
  r3.z = r3.z;
  r3.x = r3.x;
  r4.y = r4.y;
  r3.w = r3.w;
  r1.x = r1.x;
  r1.x = r1.x;
  r1.yzw = v1.xyw;
  r1.x = r1.x;
  r1.yzw = r1.yzw;
  r1.yz = r1.yz / r1.ww;
  r5.xy = -g_psScene.m_cameraCenterOffset.xy;
  r1.yz = r5.xy + r1.yz;
  r1.x = r1.x;
  r5.xyzw = g_psScene.m_projectionParam.xyzw;
  r1.yz = r5.xy * r1.yz;
  r1.w = -r5.w;
  r1.x = r1.x + r1.w;
  r5.x = r5.z / r1.x;
  r1.xy = r5.xx * r1.yz;
  r5.yz = r1.xy;
  r1.w = 1;
  r5.yz = r5.yz;
  r5.x = r5.x;
  r5.xyz = r5.xyz;
  r2.w = dot(r5.xyz, r5.xyz);
  r2.w = rsqrt(r2.w);
  r6.xyz = r5.yzx * r2.www;
  r5.xyz = r5.xyz;
  r6.xyz = r6.xyz;
  r5.xyz = r5.xyz;
  r5.yzw = r5.yzx;
  r5.yzw = -r5.yzw;
  r5.yzw = g_psLight.m_lightParams[3].xyz + r5.yzw;
  r2.w = g_psMaterial.m_materials[7].x;
  r4.w = dot(r5.yzw, r5.yzw);
  r4.w = sqrt(r4.w);
  r2.w = max(r4.w, r2.w);
  r6.w = g_psLight.m_lightParams[6].x;
  r5.yzw = r5.yzw / r4.www;
  r2.w = r6.w + r2.w;
  r4.w = r2.w * r2.w;
  r6.w = 1 / r4.w;
  r4.w = g_psLight.m_lightParams[6].w * r4.w;
  r4.w = -r4.w;
  r4.w = r6.w + r4.w;
  r4.w = max(0, r4.w);
  r5.yzw = r5.yzw;
  r7.xyz = g_psLight.m_lightParams[4].xyz * r4.www;
  r7.xyz = g_psScene.m_exposure.zzz * r7.xyz;
  r4.w = g_psLight.m_lightParams[4].w;
  r4.w = r4.w;
  r2.w = r2.w;
  r4.w = 1 * r4.w;
  r2.w = r4.w / r2.w;
  r2.w = 0.899999976 * r2.w;
  r2.w = max(0, r2.w);
  r7.w = min(1, r2.w);
  r7.w = r7.w;
  r5.yzw = r5.yzw;
  r7.xyz = r7.xyz;
  r7.w = r7.w;
  r5.x = r5.x;
  r0.xyzw = r0.xyzw;
  r5.x = r5.x;
  r0.xyzw = r0.xyzw;
  r2.w = g_psLight.m_lightParams[3].w;
  r2.w = g_psLight.m_lightParams[7].w * r2.w;
  r1.z = r5.x + r2.w;
  r8.x = dot(r1.xyzw, g_psScene.m_view._m00_m10_m20_m30);
  r8.y = dot(r1.xyzw, g_psScene.m_view._m01_m11_m21_m31);
  r8.z = dot(r1.xyzw, g_psScene.m_view._m02_m12_m22_m32);
  r8.w = dot(r1.xyzw, g_psScene.m_view._m03_m13_m23_m33);
  r8.xyzw = r8.xyzw;
  r1.xyz = g_psLight.m_lightParams[3].xyz;
  r1.w = 1;
  r9.x = dot(r1.xyzw, g_psScene.m_view._m00_m10_m20_m30);
  r9.y = dot(r1.xyzw, g_psScene.m_view._m01_m11_m21_m31);
  r9.z = dot(r1.xyzw, g_psScene.m_view._m02_m12_m22_m32);
  r9.xyz = r9.xyz;
  r1.x = -r8.x;
  r8.x = r1.x + r9.x;
  r1.x = -r9.y;
  r8.y = r8.y + r1.x;
  r1.x = -r9.z;
  r8.z = r8.z + r1.x;
  r8.w = r8.w;
  r1.x = dot(r8.xyz, r8.xyz);
  r1.x = sqrt(r1.x);
  r1.y = g_psLight.m_lightParams[7].z;
  r8.xyz = r8.xyz;
  r8.w = r8.w;
  r1.y = r1.y;
  r8.xyzw = r8.xyzw;
  r8.xyz = r8.xyz / r8.www;
  r1.z = cmp(0 >= r8.z);
  r9.x = r1.z ? -1 : 1;
  r8.w = r9.x * r8.z;
  r1.z = dot(r8.xyw, r8.xyw);
  r1.z = sqrt(r1.z);
  r8.xyz = r8.xyw / r1.zzz;
  r1.w = 1 + r8.z;
  r8.xy = r8.xy / r1.ww;
  r1.w = g_psScene.m_shadowMapResolutions.z / 2;
  r2.w = g_psScene.m_shadowMapResolutions.w;
  r4.w = -2 + r1.w;
  r10.x = r4.w / r1.w;
  r1.w = -2 + r2.w;
  r10.y = r1.w / r2.w;
  r9.yz = r10.xy * r8.xy;
  r1.y = r1.z * r1.y;
  r1.y = -r1.y;
  r9.w = 1 + r1.y;
  r9.x = r9.x;
  r9.yz = r9.yz;
  r9.w = r9.w;
  r9.x = r9.x;
  r9.xyzw = r9.xyzw;
  r9.x = r9.x;
  r1.y = r9.y + r9.x;
  r9.y = 0.5 * r1.y;
  r1.yz = float2(0.5,0.5) * r9.yz;
  r8.xy = float2(0.5,0.5) + r1.yz;
  r1.y = -r8.y;
  r8.z = 1 + r1.y;
  r10.x = -0.5 / g_psScene.m_shadowMapResolutions.z;
  r10.y = -0.5 / g_psScene.m_shadowMapResolutions.w;
  r9.yz = r10.xy + r8.xz;
  r9.yz = r9.yz;
  r9.w = r9.w;
  r0.xyzw = r0.xyzw;
  r9.yzw = r9.yzw;
  r0.xyzw = r0.xyzw;
  r9.yzw = r9.yzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw / float4(2,2,2,2);
  r0.xyzw = frac(r0.xyzw);
  r0.xyzw = cmp(r0.xyzw >= float4(0.300000012,0.300000012,0.300000012,0.300000012));
  r0.xyzw = r0.xyzw ? float4(1,1,1,1) : float4(0,0,0,0);
  r0.xyzw = float4(2,2,2,2) * r0.xyzw;
  r0.xyzw = float4(-1,-1,-1,-1) + r0.xyzw;
  r0.xyzw = float4(0.125,0.125,0.125,0.125) * r0.xyzw;
  r0.xyzw = float4(0.5,0.5,-0.5,-0.5) + r0.xyzw;
  r8.xyzw = float4(1,1,1,1) / g_psScene.m_shadowMapResolutions.zwzw;
  r0.xyzw = r8.xyzw * r0.xyzw;
  r9.yzw = r9.yzw;
  r8.xy = r9.yz + r0.xy;
  r8.xy = r8.xy;
  r8.z = r9.w;
  r1.yzw = r8.xyz / float3(1,1,1);
  r1.y = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r1.yz, r1.w).x;
  r1.y = 0 + r1.y;
  r10.xy = r9.yz + r0.zy;
  r10.xy = r10.xy;
  r10.z = r8.z;
  r10.xyz = r10.xyz / float3(1,1,1);
  r0.y = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r10.xy, r10.z).x;
  r0.y = r1.y + r0.y;
  r10.xy = r9.yz + r0.xw;
  r10.xy = r10.xy;
  r10.z = r8.z;
  r1.yzw = r10.xyz / float3(1,1,1);
  r0.x = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r1.yz, r1.w).x;
  r0.x = r0.y + r0.x;
  r8.xy = r9.yz + r0.zw;
  r8.xy = r8.xy;
  r0.yzw = r8.xyz / float3(1,1,1);
  r0.y = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r0.yz, r0.w).x;
  r0.x = r0.x + r0.y;
  r0.x = 0.25 * r0.x;
  r0.x = 1 * r0.x;
  r0.x = -r0.x;
  r0.x = 1 + r0.x;
  r0.x = r0.x * r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  // =========================================================================
  // PCSS contact hardening (point light).
  //
  // Replaces the fixed-radius 4-tap PCF just above. r0.x already holds the
  // vanilla result, so skipping the block leaves it alone.
  //
  // The point map is dual-paraboloid: r9.yz is the atlas UV and r9.w is
  // 1 - distance/range. Unlike the spot there is no m_shadowProjection step,
  // and a texel covers a different world size at the centre and the rim of a
  // hemisphere. 3 * distance / hemisphereRes is the compromise; the softness
  // setting still decides the final penumbra.
  // =========================================================================
  if (CUSTOM_POINT_CONTACT_HARDENING != 0.f) {
    const float atlasWidth = max(g_psScene.m_shadowMapResolutions.z, 1.0);
    const float texel = 1.0 / atlasWidth;
    const float hemisphereRes = max(atlasWidth * 0.5, 1.0);
    const float2 baseUV = r9.yz;
    const float refZ = r9.w;

    // m_lightParams[7].z is 1/range for this light, and the map stores
    // 1 - distance/range, so depth differences are already linear.
    const float range = 1.0 / max(g_psLight.m_lightParams[7].z, 1e-6);
    const float dist = max((1.0 - refZ) * range, 1e-4);
    const float texelWorld = 3.0 * dist / hemisphereRes;

    const float angle = PcssInterleavedGradientNoise(v0.xy) * 6.28318530718;
    const float cs = cos(angle);
    const float sn = sin(angle);

    const float2 uvStepX = ddx(baseUV);
    const float2 uvStepY = ddy(baseUV);
    const float zStepX = ddx(refZ);
    const float zStepY = ddy(refZ);

    float penumbra01 = 0.0;
    [unroll] for (int k = 0; k < 9; ++k) {
      const float2 o = (float2(k % 3, k / 3) - 1.0) * CUSTOM_POINT_BLUR_PIXELS;
      const float2 uvN = baseUV + o.x * uvStepX + o.y * uvStepY;
      const float zN = refZ + o.x * zStepX + o.y * zStepY;
      const float occN = g_ShadowTexture.SampleCmpLevelZero(g_samplerComparisonLess_Linear_Clmap_s, uvN, zN).x;
      const float casterN = g_ShadowTexture.SampleLevel(g_samplerPoint_Clamp_s, uvN, 0).x;
      const float zRecvN = (1.0 - zN) * range;
      const float zCastN = (1.0 - casterN) * range;
      const float relN = (zRecvN - zCastN) / max(zCastN, 1e-9);
      penumbra01 += occN * saturate(CUSTOM_POINT_SHADOW_K * relN * relN);
    }
    penumbra01 /= 9.0;

    const float penumbraMaxWorld = max(CUSTOM_POINT_SHADOW_SOFTNESS, POINT_MIN_TEXELS * texelWorld);
    const float penumbraWorld = penumbraMaxWorld * penumbra01;
    const float penumbra = clamp(penumbraWorld / max(texelWorld, 1e-9), 0.5, 128.0);

    if (penumbra01 > 0.0) {
      const float radiusTexels = penumbra * texel;
      float avg = 0.0;
      [unroll] for (int j = 0; j < 32; ++j) {
        const float2 c = kPcssVogelDisk[j];
        const float2 v = float2(c.x * cs - c.y * sn, c.x * sn + c.y * cs);
        avg += dot(g_ShadowTexture.GatherCmp(g_samplerComparisonLess_Linear_Clmap_s, baseUV + v * radiusTexels, refZ), 0.25);
      }
      avg /= 32.0;

      avg = min(avg * CUSTOM_POINT_SHADOW_DARKNESS, 1.0);

      float pcssLit = 1.0 - avg;
      pcssLit *= pcssLit;

      r0.x = pcssLit;
    }
  }
  r0.y = g_psMaterial.m_materials[7].y;
  r0.z = g_psMaterial.m_materials[7].z;
  r0.y = -r0.y;
  r0.y = r1.x + r0.y;
  r0.y = r0.y * r0.z;
  r0.y = max(0, r0.y);
  r0.y = min(1, r0.y);
  r0.z = -r0.x;
  r0.z = 1 + r0.z;
  r0.y = r0.y * r0.z;
  r0.x = r0.x + r0.y;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.y = r0.x;
  r2.xyz = r2.xyz;
  r5.yzw = r5.yzw;
  r7.xyzw = r7.xyzw;
  r6.xyz = r6.xyz;
  r3.x = r3.x;
  r3.z = r3.z;
  r4.y = r4.y;
  r3.w = r3.w;
  r2.xyz = r2.xyz;
  r5.yzw = r5.yzw;
  r7.xyzw = r7.xyzw;
  r6.xyz = r6.xyz;
  r3.x = r3.x;
  r3.z = r3.z;
  r4.y = r4.y;
  r3.w = r3.w;
  r5.yzw = r5.yzw;
  r2.xyz = r2.xyz;
  r1.xyz = r5.yzw;
  r0.z = dot(r1.xyz, r2.xyz);
  r6.xyz = -r6.xyz;
  r0.w = dot(r6.xyz, r2.xyz);
  r0.w = r0.w + r0.w;
  r0.w = -r0.w;
  r8.xyz = r2.xyz * r0.www;
  r8.xyz = r8.xyz + r6.xyz;
  r8.xyz = -r8.xyz;
  r9.xyz = -r1.xyz;
  r9.xyz = r9.xyz + r8.xyz;
  r0.w = dot(r9.xyz, r9.xyz);
  r0.w = sqrt(r0.w);
  r0.w = r7.w / r0.w;
  r0.w = max(0, r0.w);
  r9.xyz = min(float3(1,1,1), r0.www);
  r10.xyz = -r5.yzw;
  r8.xyz = r10.xyz + r8.xyz;
  r8.xyz = r9.xyz * r8.xyz;
  r5.xyz = r8.xyz + r5.yzw;
  r0.w = dot(r5.xyz, r5.xyz);
  r0.w = rsqrt(r0.w);
  r5.xyz = r5.xyz * r0.www;
  r5.xyz = r5.xyz + r6.xyz;
  r0.w = dot(r5.xyz, r5.xyz);
  r0.w = rsqrt(r0.w);
  r5.xyz = r5.xyz * r0.www;
  r0.w = dot(r5.xyz, r2.xyz);
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r1.x = dot(r5.xyz, r1.xyz);
  r3.z = r3.z;
  r3.y = 0.25;
  r2.xyzw = g_MaterialTexture.Sample(g_samplerPoint_Clamp_s, r3.zy).xyzw;
  r3.y = 0.75;
  r4.xz = g_MaterialTexture.Sample(g_samplerPoint_Clamp_s, r3.zy).xw;
  r4.xz = r4.xz;
  r2.xyz = r2.xyz;
  r2.w = r2.w;
  r1.y = r4.x;
  r4.z = r4.z;
  r1.x = r1.x;
  r1.y = r1.y;
  r1.x = g_lut1.Sample(g_samplerLinear_Clamp_s, r1.xy).x;
  r1.x = r1.x;
  r1.x = r1.x;
  r1.y = r0.z * r0.z;
  r1.y = -r1.y;
  r1.y = 1 + r1.y;
  r1.y = r1.y * r3.w;
  r1.y = r1.y + r0.z;
  r1.z = r3.w * r2.w;
  r1.y = max(r1.y, r1.z);
  r1.y = max(0, r1.y);
  r1.y = min(1, r1.y);
  r2.xyz = r2.xyz * r3.xxx;
  r0.w = r0.w;
  r4.y = r4.y;
  r4.z = r4.z;
  r0.w = max(0, r0.w);
  r4.x = min(1, r0.w);
  r4.y = r4.y;
  r4.z = r4.z;
  r3.xyz = r4.xyz * r4.xyz;
  r3.xyz = float3(0.984375,0.9375,0.9375) * r3.xyz;
  r3.xyz = float3(0.0078125,0.03125,0.03125) + r3.xyz;
  r1.zw = g_lut2.Sample(g_samplerLinear_Clamp_s, r3.xyz).xy;
  r1.zw = r1.zw;
  r1.zw = r1.zw * r1.zw;
  r0.w = r1.z / r1.w;
  r0.w = r1.x * r0.w;
  r0.z = r0.z;
  r0.z = 4 * r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = r0.w * r0.z;
  r1.xzw = r2.xyz * r7.xyz;
  r1.xzw = r1.xzw * r0.zzz;
  r0.z = 1 * r1.y;
  r0.z = 1 * r0.z;
  r2.xyz = r0.zzz * r7.xyz;
  r1.xzw = r1.xzw;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r1.xzw = r1.xzw;
  r2.xyz = r2.xyz;
  r1.xzw = r1.xzw;
  r0.y = r0.y;
  r2.xyz = r2.xyz;
  r1.xzw = r1.xzw;
  r0.y = r0.y;
  r0.x = -1 + r0.x;
  r0.x = g_psLight.m_lightParams[8].z * r0.x;
  r0.x = 1 + r0.x;
  r0.y = -1 + r0.y;
  r0.y = g_psLight.m_lightParams[8].z * r0.y;
  r0.y = 1 + r0.y;
  r0.xzw = r2.xyz * r0.xxx;
  r1.y = g_psLight.m_lightParams[8].y;
  r0.xzw = r1.yyy * r0.xzw;
  r2.xyz = float3(1,1,1) * r0.xzw;
  r0.xyz = r1.xzw * r0.yyy;
  r0.w = g_psLight.m_lightParams[8].x;
  r0.xyz = r0.xyz * r0.www;
  r0.xyz = float3(1,1,1) * r0.xyz;
  r2.xyz = r2.xyz;
  r0.xyz = r0.xyz;
  r2.xyz = r2.xyz;
  r0.xyz = r0.xyz;
  r2.xyz = r2.xyz;
  r0.xyz = r0.xyz;
  r2.xyz = r2.xyz;
  r0.xyz = r0.xyz;
  r2.xyz = r2.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = 1;
  r0.xyz = r0.xyz;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.w = 1;
  r2.xyz = r2.xyz;
  r2.xyzw = r2.xyzw;
  r2.xyzw = r2.xyzw;
  // =========================================================================
  // Screen-space shadow complement (point light).
  //
  // The point map cannot resolve centimetre contact detail either, so this
  // supplies the high-frequency half: march the depth buffer toward the light
  // and attenuate where the ray passes behind a nearer surface.
  //
  // The direction toward the light is per pixel: m_lightParams[3].xyz is the
  // light's view-space position, exactly as in the spot port. An omni light has
  // no cone constant, so this is the only light-specific input.
  //
  // Only the diffuse accumulation (r2, SV_Target0) is scaled, as in the sun and
  // spot shaders. The specular target (r0) is left alone.
  // =========================================================================
  {
    const float2 uv = v0.xy / max(g_psScene.m_viewportSize.xy, float2(1.0, 1.0));
    const float d = g_DepthTexture.SampleLevel(g_samplerPoint_Clamp_s, uv, 0).x;
    const float viewZ = g_psScene.m_projectionParam.z / max(d - g_psScene.m_projectionParam.w, 1e-6);

    const float band = g_SpecularTexture.SampleLevel(g_samplerPoint_Wrap_s, uv, 0).z;

    if (CUSTOM_SS_POINT_SHADOWS != 0.f
        && band < CUSTOM_SS_POINT_MATERIAL_MAX
        && viewZ < CUSTOM_SS_POINT_FADE_END
        && max(r2.x, max(r2.y, r2.z)) > 0.0) {
      const float jitter = PcssInterleavedGradientNoise(v0.xy);
      const float fadeStart = 40.0;
      const float fade = 1.0 - saturate((viewZ - fadeStart) / max(CUSTOM_SS_POINT_FADE_END - fadeStart, 1e-3));

      const float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
      const float3 viewPos = float3(ndc.x * viewZ / g_psScene.m_projection._m00,
                                    ndc.y * viewZ / g_psScene.m_projection._m11,
                                    viewZ);

      const float3 lightDir = normalize(g_psLight.m_lightParams[3].xyz - viewPos);

      const float3 gbufN = g_NormalTexture.SampleLevel(g_samplerPoint_Clamp_s, uv, 0).xyz;
      const float3 normal = normalize(float3(gbufN.xy * 2.0 - 1.0, gbufN.z * gbufN.z * 2.0 - 1.0));

      const float marchStep = min(CUSTOM_SS_POINT_REACH * viewZ / abs(g_psScene.m_projection._m11), SS_REACH_MAX_M) / (float)SS_STEPS;
      const float3 rayDir = lightDir * marchStep;
      const float3 rayStart = viewPos + normal * SS_NORMAL_BIAS;
      const float4 clipStart = mul(float4(rayStart, 1.0), g_psScene.m_projection);
      const float4 clipStep = mul(float4(rayDir, 0.0), g_psScene.m_projection);

      const float occSaturated = SS_STEPS / max(CUSTOM_SS_POINT_STRENGTH * fade, 1e-6);

      float occ = 0.0;
      for (int s = 0; s < SS_STEPS; ++s) {
        const float u = ((float)s + jitter) / (float)SS_STEPS;
        const float t = (0.25 * u + 0.75 * u * u) * (float)SS_STEPS;
        const float4 clip = clipStart + clipStep * t;
        if (clip.w <= 1e-6) break;
        const float2 suv = clip.xy / clip.w * float2(0.5, -0.5) + 0.5;
        if (any(suv < 0.0) || any(suv > 1.0)) break;
        if (occ >= occSaturated) break;
        const float sd = g_DepthTexture.SampleLevel(g_samplerPoint_Clamp_s, suv, 0).x;
        const float sz = g_psScene.m_projectionParam.z / max(sd - g_psScene.m_projectionParam.w, 1e-6);
        const float sep = rayStart.z + rayDir.z * t - sz;
        if (sep > CUSTOM_SS_POINT_THICKNESS
            && sep < max(SS_CASTER_MAX_M, viewZ * SS_CASTER_PER_M)) {
          if (CUSTOM_SS_POINT_MATERIAL_MAX >= 0.999
              || g_SpecularTexture.SampleLevel(g_samplerPoint_Wrap_s, suv, 0).z < CUSTOM_SS_POINT_MATERIAL_MAX) {
            occ += 1.0;
          }
        }
      }

      const float ssOcc = (occ / (float)SS_STEPS) * fade;
      r2.xyz *= saturate(1.0 - ssOcc * CUSTOM_SS_POINT_STRENGTH);
    }
  }

  o0.xyzw = r2.xyzw;
  o1.xyzw = r0.xyzw;
  return;
}