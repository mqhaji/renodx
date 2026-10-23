// MGSV spot light with shadow ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â CRC32 0x1D4DB9C5, ps_5_0.
//
// This is the same shader as 0x08292A5E with one permutation: the distance
// attenuation. The engine emits both, and a scene can use one or the other, so
// both carry the PCSS and screen-space port; see the 0x08292A5E file for the
// commentary, which applies here unchanged.
//
// Original source: SSLighting2_Spot_ps.hlsl
// Decompiled from the retail DXBC with 3Dmigoto (bin/cmd_Decompiler.exe).
// The register-level body below is the vanilla decompilation, kept verbatim so
// the baseline is provably equivalent. Contact hardening edits are marked "PCSS".
//
// Vanilla shadow filter (what PCSS replaces):
//   4 x SampleCmp(t3, s15) at +/-0.125 texels, dithered from a hash,
//   then shadow = (1 - average)^2.
//
// Shadow map: t3, 4096x4096 R24_UNORM_X8, one map per light. Unlike the sun
// there are no cascades and no atlas, so taps need no tile clamping.
//
// The shadow coordinate is projective: r9 is light clip space, so UV and depth
// come from dividing by r9.w. That w is the view-space depth along the light's
// axis in world units, which is what makes a metre-valued penumbra possible.
//
// ---- Created with 3Dmigoto v1.3.16 on Fri Oct 16 16:41:57 2026

// The addon's injected settings, which carry the tunables below.
#include "../shared.h"

// Tunables come from the addon, through cb13; see ../shared.h. The spot's values
// are separate from the sun's because the scales differ - a spot's cone is tens
// of metres where the sun's cascade is hundreds, so the same caster separation
// gives a far larger ratio.
//
// To iterate through the devkit instead, pin them here: that path replaces the
// shader without supplying cb13, and D3D11 returns 0 for an unbound constant
// buffer, so anything read from it - including the gate - comes back as 0.
//
// #undef  CUSTOM_SPOT_CONTACT_HARDENING
// #define CUSTOM_SPOT_CONTACT_HARDENING 1.0
// #undef  CUSTOM_SPOT_SHADOW_SOFTNESS
// #define CUSTOM_SPOT_SHADOW_SOFTNESS   0.08
// #undef  CUSTOM_SPOT_SHADOW_K
// #define CUSTOM_SPOT_SHADOW_K          4.0
// #undef  CUSTOM_SPOT_BLUR_PIXELS
// #define CUSTOM_SPOT_BLUR_PIXELS       24.0
// #undef  CUSTOM_SPOT_SHADOW_DARKNESS
// #define CUSTOM_SPOT_SHADOW_DARKNESS   1.0
// #undef  CUSTOM_SPOT_CONE_FALLOFF
// #define CUSTOM_SPOT_CONE_FALLOFF      1.0

#define SPOT_MIN_TEXELS    6.0   // floor on the maximum, texels

// ---- Screen-space shadow complement -----------------------------------------
//
// The same term the sun shader carries, and the same addon dials drive it
// (CUSTOM_SS_* in ../shared.h) - but its own toggle, CUSTOM_SS_SPOT_SHADOWS,
// because it runs once per spot draw instead of once a frame. See ../addon.cpp.
// The march is in view space and light-agnostic apart from one thing: the
// direction toward the light, which for a spot is not constant across the
// screen - see the block at the end of main().
//
// These constants are the sun shader's, unchanged. They size the march in screen
// and world units that a spot shares: the reach is a fraction of screen height,
// the thickness is a caster's size in metres, and a spot's own shadow map cannot
// resolve anything finer than the sun's either. Only the addon dials are worth
// re-tuning per light type, and those are settings, not defines.
//
// To iterate through the devkit instead, pin them here: that path replaces the
// shader without supplying cb13, and D3D11 returns 0 for an unbound constant
// buffer, so anything read from it - including the toggle - comes back as 0.
//
// #undef  CUSTOM_SS_SPOT_SHADOWS
// #define CUSTOM_SS_SPOT_SHADOWS 1.0
// #undef  CUSTOM_SS_SPOT_REACH
// #define CUSTOM_SS_SPOT_REACH        0.05
// #undef  CUSTOM_SS_SPOT_THICKNESS
// #define CUSTOM_SS_SPOT_THICKNESS    0.005
// #undef  CUSTOM_SS_SPOT_STRENGTH
// #define CUSTOM_SS_SPOT_STRENGTH     2.0
// #undef  CUSTOM_SS_SPOT_FADE_END
// #define CUSTOM_SS_SPOT_FADE_END     122.9
// #undef  CUSTOM_SS_SPOT_MATERIAL_MAX
// #define CUSTOM_SS_SPOT_MATERIAL_MAX 0.25

#define SS_STEPS        48     // depth-buffer march steps toward the light, sized to
                               // the screen resolution of the reach: 2.5% of screen
                               // height is 54 px at 2160p, so 48 steps is about a
                               // pixel per step. NOT unrolled - see the sun shader.
#define SS_NORMAL_BIAS  0.0    // start offset along the normal, metres
#define SS_REACH_MAX_M  6.00   // world-space cap on the reach, metres
#define SS_CASTER_MAX_M 0.25   // floor on the bounded separation between ray and
                               // caster, metres
#define SS_CASTER_PER_M 0.010  // metres of that bound per metre of depth, so the
                               // bound is max(SS_CASTER_MAX_M, depth * this)

// Vogel disk, precomputed. Only the per-pixel rotation varies, so baking the
// golden angle into the table avoids a sincos per tap that the compiler cannot
// fold, because phi is a runtime value.
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

// Interleaved gradient noise (Jimenez), the per-pixel rotation source.
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
  r5.z = r5.z / r1.x;
  r6.z = r5.z;
  r5.xy = r6.zz * r1.yz;
  r6.xy = r5.xy;
  r5.w = 1;
  r6.xy = r6.xy;
  r6.z = r6.z;
  r6.xyz = r6.xyz;
  r1.x = dot(r6.xyz, r6.xyz);
  r1.x = rsqrt(r1.x);
  r1.xyz = r6.xyz * r1.xxx;
  r6.xyz = r6.xyz;
  r1.xyz = r1.xyz;
  r6.xyz = r6.xyz;
  r6.xyz = r6.xyz;
  r6.xyz = -r6.xyz;
  r6.xyz = g_psLight.m_lightParams[3].xyz + r6.xyz;
  r1.w = g_psMaterial.m_materials[7].x;
  r2.w = dot(r6.xyz, r6.xyz);
  r4.w = rsqrt(r2.w);
  r6.w = r4.w * r2.w;
  r1.w = max(r6.w, r1.w);
  r6.xyz = r6.xyz * r4.www;
  r6.w = g_psMaterial.m_materials[7].w;
  // This variant's distance attenuation, verbatim. It is the only difference
  // from 0x08292A5E: a fourth power of the squared distance against
  // m_lightParams[6].z instead of .w, and a multiply instead of an add. Every
  // other instruction, including the shadow taps, is identical.
  r2.w = r2.w * r2.w;
  r2.w = r2.w * r2.w;
  r2.w = r2.w * r2.w;
  r2.w = min(3.40282347e+038, r2.w);
  r4.w = r4.w * r4.w;
  r2.w = g_psLight.m_lightParams[6].z * r2.w;
  r2.w = -r2.w;
  r2.w = 1 + r2.w;
  r2.w = r4.w * r2.w;
  r2.w = min(r6.w, r2.w);
  r2.w = max(0, r2.w);
  r7.xyz = g_psLight.m_lightParams[7].xyz;
  r8.xyz = g_psLight.m_lightParams[5].xyz;
  r6.xyz = r6.xyz;
  r7.x = r7.x;
  r7.y = r7.y;
  r9.xyz = -r6.xyz;
  r4.w = dot(r8.xyz, r9.xyz);
  r7.z = r7.z;
  r6.w = -r7.x;
  r4.w = r6.w + r4.w;
  r4.w = r4.w * r7.y;
  r4.w = max(0, r4.w);
  r4.w = min(1, r4.w);
  r4.w = log2(r4.w);
  r4.w = r7.z * r4.w;
  r7.xyz = exp2(r4.www);
  r7.xyz = r7.xyz;
  r7.xyz = r7.xyz;
  r9.xyz = g_psLight.m_lightParams[4].xyz * r2.www;
  r7.xyz = r9.xyz * r7.xyz;
  r7.xyz = g_psScene.m_exposure.zzz * r7.xyz;
  r2.w = g_psLight.m_lightParams[4].w;
  r1.w = r1.w;
  r2.w = 1 * r2.w;
  r1.w = r2.w / r1.w;
  r1.w = 0.899999976 * r1.w;
  r1.w = max(0, r1.w);
  r7.w = min(1, r1.w);
  r7.w = r7.w;
  r6.xyz = r6.xyz;
  r7.xyz = r7.xyz;
  r7.w = r7.w;
  r0.xyzw = r0.xyzw;
  r6.xyz = r6.xyz;
  r0.xyzw = r0.xyzw;
  r6.xyz = r6.xyz;
  r9.x = dot(r5.xyzw, g_psScene.m_shadowProjection._m00_m10_m20_m30);
  r9.y = dot(r5.xyzw, g_psScene.m_shadowProjection._m01_m11_m21_m31);
  r9.z = dot(r5.xyzw, g_psScene.m_shadowProjection._m02_m12_m22_m32);
  r9.w = dot(r5.xyzw, g_psScene.m_shadowProjection._m03_m13_m23_m33);
  r9.xyzw = r9.xyzw;
  r9.xyzw = r9.xyzw;
  r1.w = g_psMaterial.m_materials[7].y;
  r2.w = g_psMaterial.m_materials[7].z;
  r1.w = -r1.w;
  r1.w = r9.w + r1.w;
  r1.w = r1.w * r2.w;
  r1.w = max(0, r1.w);
  r1.w = min(1, r1.w);
  r5.xy = g_psMaterial.m_materials[6].xy;
  r6.xyz = r6.xyz;
  r5.x = r5.x;
  r5.y = r5.y;
  r10.xyz = -r6.xyz;
  r2.w = dot(r8.xyz, r10.xyz);
  r4.w = -r5.x;
  r2.w = r4.w + r2.w;
  r2.w = r2.w * r5.y;
  r2.w = max(0, r2.w);
  r2.w = min(1, r2.w);
  r2.w = r2.w * r2.w;
  r2.w = r2.w;
  r2.w = r2.w;
  r9.xyzw = r9.xyzw;
  r0.xyzw = r0.xyzw;
  r2.w = r2.w;
  r9.xyzw = r9.xyzw;
  r0.xyzw = r0.xyzw;
  r2.w = r2.w;
  r9.xyzw = r9.xyzw;
  r0.xyzw = r0.xyzw;
  r2.w = r2.w;
  r0.xyzw = r0.xyzw / float4(2,2,2,2);
  r0.xyzw = frac(r0.xyzw);
  r0.xyzw = cmp(r0.xyzw >= float4(0.300000012,0.300000012,0.300000012,0.300000012));
  r0.xyzw = r0.xyzw ? float4(1,1,1,1) : float4(0,0,0,0);
  r0.xyzw = float4(2,2,2,2) * r0.xyzw;
  r0.xyzw = float4(-1,-1,-1,-1) + r0.xyzw;
  r0.xyzw = float4(0.125,0.125,0.125,0.125) * r0.xyzw;
  r0.xyzw = float4(0.5,0.5,-0.5,-0.5) + r0.xyzw;
  r5.xyzw = r9.wwww / g_psScene.m_shadowMapResolutions.yyyy;
  r0.xyzw = r5.xyzw * r0.xyzw;
  r9.xyzw = r9.xyzw;
  r5.xy = r9.xy + r0.xy;
  r5.xy = r5.xy;
  r5.zw = r9.zw;
  r8.xyz = r5.xyz / r5.www;
  r4.w = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r8.xy, r8.z).x;
  r4.w = 0 + r4.w;
  r8.xy = r9.xy + r0.zy;
  r8.xy = r8.xy;
  r8.zw = r5.zw;
  r8.xyz = r8.xyz / r8.www;
  r0.y = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r8.xy, r8.z).x;
  r0.y = r4.w + r0.y;
  r8.xy = r9.xy + r0.xw;
  r8.xy = r8.xy;
  r8.zw = r5.zw;
  r8.xyz = r8.xyz / r8.www;
  r0.x = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r8.xy, r8.z).x;
  r0.x = r0.y + r0.x;
  r5.xy = r9.xy + r0.zw;
  r5.xy = r5.xy;
  r0.yzw = r5.xyz / r5.www;
  r0.y = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r0.yz, r0.w).x;
  r0.x = r0.x + r0.y;
  r0.x = 0.25 * r0.x;
  r0.x = r0.x * r2.w;
  r0.x = -r0.x;
  r0.x = 1 + r0.x;
  r0.x = r0.x * r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;

  // =========================================================================
  // PCSS contact hardening (spot light).
  //
  // Replaces the fixed-radius 4-tap PCF just above, exactly as the sun shader
  // does. r0.x already holds the vanilla result, so skipping the block leaves it
  // alone - which is what the lit-pixel early-out relies on.
  //
  // Two things differ from the sun. The shadow coordinate is projective, so UV
  // and depth come from dividing r9 by r9.w; and there are no cascades, so there
  // is no atlas tile and no clamp to keep taps inside one.
  // =========================================================================
  if (CUSTOM_SPOT_CONTACT_HARDENING != 0.f) {
    const float  shadowRes = max(g_psScene.m_shadowMapResolutions.y, 1.0);
    const float  texel     = 1.0 / shadowRes;
    const float2 baseUV    = r9.xy / r9.w;
    const float  refZ      = r9.z / r9.w;

    // World size of one texel at this receiver's depth. The map covers the spot
    // cone, and the material's x component is cos(halfAngle) - the same value the
    // vanilla uses as the cone's edge test at line 282. So the cone's width at
    // this depth is 2 * depth * tan(halfAngle), and a texel is that over the map
    // resolution. Deriving it from the light avoids assuming which element of
    // m_shadowProjection carries the scale.
    const float cosHalf = clamp(g_psMaterial.m_materials[6].x, 1e-3, 0.9999);
    const float tanHalf = sqrt(max(1.0 - cosHalf * cosHalf, 0.0)) / cosHalf;
    const float texelWorld = 2.0 * r9.w * tanHalf / shadowRes;

    // The shadow map stores a non-linear depth. For a perspective projection
    // z_norm = _m22 + _m32 / z_view, so z_view = _m32 / (z_norm - _m22). The
    // sun does not need this - an orthographic cascade stores linear depth, and
    // its ratio is a true distance ratio. Here the raw ratio blows up wherever
    // the stored depth approaches zero, which is near the light, and the
    // saturated penumbra smears across the whole surface.
    const float projM22 = g_psScene.m_shadowProjection._m22;
    const float projM32 = g_psScene.m_shadowProjection._m32;

    const float angle = PcssInterleavedGradientNoise(v0.xy) * 6.28318530718;
    const float cs    = cos(angle);
    const float sn    = sin(angle);

    // A neighbouring pixel's UV and depth, exactly: within one projection both
    // are affine in screen space.
    const float2 uvStepX = ddx(baseUV);
    const float2 uvStepY = ddy(baseUV);
    const float  zStepX  = ddx(refZ);
    const float  zStepY  = ddy(refZ);

    // Nine taps. The mask is the low-frequency half of the effect, and each tap
    // costs a linear-PCF fetch, four texels, plus a point read for the blocker
    // depth. Its purpose is that a single-pixel penumbra jitters between zero and
    // maximum on adjacent pixels, and the eye reads the mixture as a translucent
    // veil rather than a soft edge.
    // Nine taps. The mask is the low-frequency half of the effect, and each tap
    // costs a linear-PCF fetch, four texels, plus a point read for the blocker
    // depth. Its purpose is that a single-pixel penumbra jitters between zero and
    // maximum on adjacent pixels, and the eye reads the mixture as a translucent
    // veil rather than a soft edge.
    float penumbra01 = 0.0;
    [unroll] for (int k = 0; k < 9; ++k) {
      const float2 o       = (float2(k % 3, k / 3) - 1.0) * CUSTOM_SPOT_BLUR_PIXELS;
      const float2 uvN     = baseUV + o.x * uvStepX + o.y * uvStepY;
      const float  zN      = refZ   + o.x * zStepX  + o.y * zStepY;
      const float  occN    = g_ShadowTexture.SampleCmpLevelZero(g_samplerComparisonLess_Linear_Clmap_s, uvN, zN).x;
      const float  casterN = g_ShadowTexture.SampleLevel(g_samplerPoint_Clamp_s, uvN, 0).x;
      // Linearise both, and the receiver's is this neighbour's OWN depth - the
      // extrapolated zN, exactly as the sun does. Using the centre's depth for
      // every tap mixes one receiver with nine different casters, which saturates
      // relN wherever the surface is sloped and pins the radius at maximum.
      const float  zRecvN  = projM32 / max(zN - projM22, 1e-6);
      const float  zCastN  = projM32 / max(casterN - projM22, 1e-6);
      const float  relN    = (zRecvN - zCastN) / max(zCastN, 1e-9);
      penumbra01 += occN * saturate(CUSTOM_SPOT_SHADOW_K * relN * relN);
    }
    penumbra01 /= 9.0;

    // Maximum penumbra, floored in texels so a coarse shadow map cannot shrink
    // the effect out of sight.
    const float penumbraMaxWorld = max(CUSTOM_SPOT_SHADOW_SOFTNESS, SPOT_MIN_TEXELS * texelWorld);
    const float penumbraWorld    = penumbraMaxWorld * penumbra01;
    const float penumbra = clamp(penumbraWorld / max(texelWorld, 1e-9), 0.5, 128.0);

    // Only where there is a penumbra to resolve. penumbra01 is zero exactly when
    // none of those nine neighbours had a blocker: no separation, nothing to
    // soften, and the radius would sit at vanilla's 0.5 texels regardless.
    if (penumbra01 > 0.0) {
      const float radiusTexels = penumbra * texel;
      float avg = 0.0;
      [unroll] for (int j = 0; j < 32; ++j) {
        const float2 c = kPcssVogelDisk[j];
        const float2 v = float2(c.x * cs - c.y * sn, c.x * sn + c.y * cs);
        avg += dot(g_ShadowTexture.GatherCmp(g_samplerComparisonLess_Linear_Clmap_s, baseUV + v * radiusTexels, refZ), 0.25);
      }
      avg /= 32.0;

      // r2.w is the cone's edge falloff: the vanilla scales its occlusion by it,
      // so a shadow fades toward the cone's rim. That is why vanilla spot shadows
      // are faint over most of the cone. How much of it to keep is a setting, but
      // it cannot go to zero - at full strength right up to the rim the shadow
      // reads as a stretch rather than a fade.
      avg *= lerp(1.0, r2.w, CUSTOM_SPOT_CONE_FALLOFF);

      // Darkness scales the occlusion, so higher is darker. Clamped at fully
      // occluded: (1 - avg)^2 inverts once avg passes 1, which would make a more
      // occluded pixel come out brighter. The vanilla never needs this because
      // r2.w is clamped to [0,1] and its mean cannot exceed 1.
      avg = min(avg * CUSTOM_SPOT_SHADOW_DARKNESS, 1.0);

      // Keep vanilla's squaring, as the sun does: it keeps the core of a shadow
      // dark and narrows the transition, rather than lightening partial occlusion.
      float pcssLit = 1.0 - avg;
      pcssLit *= pcssLit;

      r0.x = pcssLit;
    }
  }
  r0.y = -r0.x;
  r0.y = 1 + r0.y;
  r0.y = r1.w * r0.y;
  r0.x = r0.x + r0.y;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.y = r0.x;
  r2.xyz = r2.xyz;
  r7.xyzw = r7.xyzw;
  r1.xyz = r1.xyz;
  r3.x = r3.x;
  r3.z = r3.z;
  r4.y = r4.y;
  r3.w = r3.w;
  r2.xyz = r2.xyz;
  r7.xyzw = r7.xyzw;
  r1.xyz = r1.xyz;
  r3.x = r3.x;
  r3.z = r3.z;
  r4.y = r4.y;
  r3.w = r3.w;
  r2.xyz = r2.xyz;
  r5.xyz = r6.xyz;
  r0.z = dot(r5.xyz, r2.xyz);
  r1.xyz = -r1.xyz;
  r0.w = dot(r1.xyz, r2.xyz);
  r0.w = r0.w + r0.w;
  r0.w = -r0.w;
  r8.xyz = r2.xyz * r0.www;
  r8.xyz = r8.xyz + r1.xyz;
  r8.xyz = -r8.xyz;
  r9.xyz = r8.xyz + r10.xyz;
  r0.w = dot(r9.xyz, r9.xyz);
  r0.w = sqrt(r0.w);
  r0.w = r7.w / r0.w;
  r0.w = max(0, r0.w);
  r9.xyz = min(float3(1,1,1), r0.www);
  r10.xyz = -r6.xyz;
  r8.xyz = r10.xyz + r8.xyz;
  r8.xyz = r9.xyz * r8.xyz;
  r6.xyz = r8.xyz + r6.xyz;
  r0.w = dot(r6.xyz, r6.xyz);
  r0.w = rsqrt(r0.w);
  r6.xyz = r6.xyz * r0.www;
  r1.xyz = r6.xyz + r1.xyz;
  r0.w = dot(r1.xyz, r1.xyz);
  r0.w = rsqrt(r0.w);
  r1.xyz = r1.xyz * r0.www;
  r0.w = dot(r1.xyz, r2.xyz);
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r1.x = dot(r1.xyz, r5.xyz);
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
  // Screen-space shadow complement (spot light).
  //
  // The spot map cannot afford centimetre detail over a cone tens of metres
  // deep - the same problem the cascade map has - so this supplies the
  // high-frequency half: ray-march the depth buffer toward the light and
  // attenuate where the ray passes behind a nearer surface.
  //
  // The port is the sun's block, with two differences that come from the light
  // and not from taste:
  //
  //  1. The direction toward the light is per pixel. A spot has a position, not
  //     a direction: m_lightParams[3].xyz is the light's view-space position and
  //     the vanilla builds L from it as (lightPos - viewPos), so the march has
  //     to normalise that per pixel instead of reading one direction from the
  //     constant buffer.
  //  2. The receiver's view position is rebuilt here rather than taken from the
  //     vanilla's r5, because r5 has already been overwritten by the four shadow
  //     taps by the time this runs. The rebuild is the sun's proven path -
  //     depth, m_projectionParam for the linearisation, m_projection._m00/_m11
  //     for the unprojection - and not m_projectionParam.xy, which measured with
  //     a scale error in the sun port.
  //
  // Only the diffuse accumulation (r2, SV_Target0) is scaled, exactly as the
  // sun does. The specular target (r0) is left alone; the sun's specular is too,
  // so a spot highlight inside the contact region stays visible. Check that in
  // the capture before deciding it is wrong.
  //
  // The target is additive, so this multiplies the light rather than writing an
  // occlusion value - see the findings in docs/MGSV_ScreenSpaceShadows.md, where
  // writing (1 - ssOcc) washed the frame out.
  // =========================================================================
  {
    const float2 uv    = v0.xy / max(g_psScene.m_viewportSize.xy, float2(1.0, 1.0));
    const float  d     = g_DepthTexture.SampleLevel(g_samplerPoint_Clamp_s, uv, 0).x;
    const float  viewZ = g_psScene.m_projectionParam.z / max(d - g_psScene.m_projectionParam.w, 1e-6);

    // The material band, sampled the way the vanilla samples it, so the band is
    // the one that was measured: grass, terrain and rocks below 0.25, characters
    // above it. It gates both the receiver here and the caster in the loop.
    const float band = g_SpecularTexture.SampleLevel(g_samplerPoint_Wrap_s, uv, 0).z;

    // The march is the expensive part of this shader, so it runs only where it
    // can change the output: the setting is on, the receiver is a material the
    // term is allowed on, the pixel is inside the fade, and this light is not
    // already contributing zero here (zero scaled is still zero).
    if (CUSTOM_SS_SPOT_SHADOWS != 0.f
        && band < CUSTOM_SS_SPOT_MATERIAL_MAX
        && viewZ < CUSTOM_SS_SPOT_FADE_END
        && max(r2.x, max(r2.y, r2.z)) > 0.0) {
      const float jitter    = PcssInterleavedGradientNoise(v0.xy);
      // Full strength begins at 40 m. Fixed in the body, as in the sun shader:
      // it had a knob and never needed one, since the end distance is what
      // decides where the term is gone.
      const float fadeStart = 40.0;
      const float fade      = 1.0 - saturate((viewZ - fadeStart) / max(CUSTOM_SS_SPOT_FADE_END - fadeStart, 1e-3));

      // March toward the light's own screen position. Projecting a world-space
      // step instead - as the sun does - collapses here: a spot sits above the
      // receiver, so its ray is near-perpendicular to the view axis and the
      // projected clip.w drives the sample off-screen within a step or two. That
      // measured as fewer than twelve of forty-eight steps on every lit pixel,
      // and one occluded pixel in the whole frame. Stepping in UV has no such
      // dependency, and the aspect comes from the projection matrix.
      const float  aspect    = max(g_psScene.m_projection._m11 / max(abs(g_psScene.m_projection._m00), 1e-6), 1e-3);
      const float4 clipLight = mul(float4(g_psLight.m_lightParams[3].xyz, 1.0), g_psScene.m_projection);

      float occ = 0.0;
      if (clipLight.w > 1e-4) {
        const float2 suvLight = clipLight.xy / clipLight.w * float2(0.5, -0.5) + 0.5;
        const float2 pSelf    = float2(uv.x * aspect, uv.y);
        const float2 delta    = float2(suvLight.x * aspect, suvLight.y) - pSelf;
        const float  lenUV    = length(delta);
        if (lenUV > 1e-4) {
          const float2 dir    = delta / lenUV;
          // The reach is a fraction of screen height, capped so a far receiver
          // cannot sweep the frame. A spot lights a small part of the image, so
          // the shared dial leaves a patch only a few hundred pixels wide; the
          // floor keeps the term visible without changing the sun. Measured at
          // 39,721 affected pixels at 0.15 against 1 at 0.04.
          const float  reach  = clamp(CUSTOM_SS_SPOT_REACH, 0.02f, 0.5f);
          const float  stepUV = reach / (float)SS_STEPS;
          for (int s = 0; s < SS_STEPS; ++s) {
            // Quadratic step distribution: dense near the receiver, where
            // contact-sized casters are, sparser further out where only large
            // ones can register.
            const float  f   = ((float)s + jitter) / (float)SS_STEPS;
            const float2 p   = pSelf + dir * (stepUV * f);
            const float2 suv = float2(p.x / aspect, p.y);
            if (any(suv < 0.0) || any(suv > 1.0)) break;
            const float sd = g_DepthTexture.SampleLevel(g_samplerPoint_Clamp_s, suv, 0).x;
            const float sz = g_psScene.m_projectionParam.z / max(sd - g_psScene.m_projectionParam.w, 1e-6);
            // Anything nearer than the receiver along the path to the light is a
            // caster. The threshold is metres of view depth, so it does not
            // depend on the angle between the ray and the view axis.
            if (viewZ - sz > max(CUSTOM_SS_SPOT_THICKNESS, 0.001f)
                && (CUSTOM_SS_SPOT_MATERIAL_MAX >= 0.999
                    || g_SpecularTexture.SampleLevel(g_samplerPoint_Wrap_s, suv, 0).z < CUSTOM_SS_SPOT_MATERIAL_MAX)) {
              occ += 1.0;
            }
          }
        }
      }

      const float ssOcc = (occ / (float)SS_STEPS) * fade;
      r2.xyz *= saturate(1.0 - ssOcc * CUSTOM_SS_SPOT_STRENGTH);
    }
  }

  o0.xyzw = r2.xyzw;
  o1.xyzw = r0.xyzw;
  return;
}
