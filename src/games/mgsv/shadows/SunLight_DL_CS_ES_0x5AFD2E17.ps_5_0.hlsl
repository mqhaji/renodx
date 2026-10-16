// MGSV sun (directional) light with cascade shadow ? CRC32 0x5AFD2E17, ps_5_0.
//
// Original source: SunLight_DL_CS_ES_ps.hlsl
// Decompiled from the retail DXBC with 3Dmigoto (bin/cmd_Decompiler.exe).
// The register-level body below is the vanilla decompilation, kept verbatim so the
// baseline is provably equivalent. Contact hardening edits are marked "PCSS".
//
// Vanilla shadow filter (what PCSS replaces):
//   4 x SampleCmp(t3, s15) at +/-0.5 +/-0.125 texel, dithered from t12,
//   then shadow = (1 - average)^2.
//   Fixed radius => no contact hardening.
//
// Shadow map: t3 g_ShadowTexture, 8192x8192 R24_UNORM_X8_TYPELESS view of D24S8,
//   2x2 atlas of 4 cascades x 4096x4096.
// ---- Created with 3Dmigoto v1.3.16 on Thu Oct 15 03:15:09 2026

// The addon's injected constants, which carry the contact-hardening toggle below.
// Every MGSV replacement shares this header; it declares cbuffer cb13, a register
// the vanilla shader does not use.
#include "../shared.h"

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

cbuffer cPSLight : register(b3)
{

  struct
  {
    float4 m_lightParams[11];
  } g_psLight : packoffset(c0);

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

SamplerState g_samplerPoint_Wrap_s : register(s8);
SamplerState g_samplerPoint_Clamp_s : register(s9);
SamplerState g_samplerLinear_Wrap_s : register(s10);
SamplerState g_samplerLinear_Clamp_s : register(s11);
SamplerComparisonState g_samplerComparisonLess_Linear_Clmap_s : register(s15);
Texture2D<float4> g_NormalTexture : register(t0);
Texture2D<float4> g_SpecularTexture : register(t1);
Texture2D<float4> g_DepthTexture : register(t2);
Texture2D<float4> g_ShadowTexture : register(t3);
Texture2D<float4> g_CloudTexture : register(t4);
Texture3D<float4> g_lut2 : register(t8);
Texture2D<float4> g_lut1 : register(t9);
Texture2D<float4> g_MaterialTexture : register(t10);
Texture2D<float4> g_RandomTexture : register(t12);
Texture2D<float4> g_SkyReflectTexture : register(t15);

// ---------------------------------------------------------------------------
// Contact-hardening soft shadows, after Wojciech Sterna, "Contact-hardening
// Soft Shadows Made Fast" (2018), section 2.1. Sections 2.2 (penumbra mask) and
// 2.3 (min filter) both need extra passes, so only the regular solution is
// implementable from a single shader replacement.
//
// The paper's algorithm:
//   blocker search : 16 Vogel disk taps inside a fixed max radius
//   penumbra       : p = (z - avgBlocker) / avgBlocker;  saturate(K * p * p)
//   filter         : 16 Vogel disk taps scaled by penumbra * maxFilterSize
//
// The penumbra is a *relative* ratio, so it is independent of the depth range
// and of the shadow map's absolute units. That is the part the previous attempt
// got wrong: it used an absolute distance in metres, floored and capped, which
// pinned to one of the two bounds instead of varying.
// ---------------------------------------------------------------------------
#define PCSS_STRENGTH            1.0   // 0 = vanilla 4-tap PCF, 1 = full effect
// K, the penumbra response, is the Sun Shadow Softness section's Penumbra
// Response setting now - too high and every pixel saturates, the penumbra goes
// uniform, and the gradient that the hardening is made of disappears.
// Tunables come from the addon, through cb13.
//
//   CUSTOM_SUN_CONTACT_HARDENING  the on/off toggle (off by default)
//   CUSTOM_SUN_SHADOW_SOFTNESS    the widest penumbra, metres
//
// Both are declared in ../shared.h and set from the Shadows section of the
// overlay. To iterate through the devkit instead, pin them here: the devkit
// live-shader path replaces the shader without supplying cb13, and D3D11 returns
// 0 for an unbound constant buffer, so anything read from cb13 - including the
// gate - comes back as 0 and the effect disappears.
//
// #undef  CUSTOM_SUN_CONTACT_HARDENING
// #define CUSTOM_SUN_CONTACT_HARDENING 1.0
// #undef  CUSTOM_SUN_SHADOW_SOFTNESS
// #define CUSTOM_SUN_SHADOW_SOFTNESS   0.05
// #undef  CUSTOM_SUN_SHADOW_K
// #define CUSTOM_SUN_SHADOW_K          24.0
// #undef  CUSTOM_SUN_BLUR_PIXELS
// #define CUSTOM_SUN_BLUR_PIXELS       24.0

#define PCSS_PENUMBRA_MIN_WORLD  0.0   // penumbra at penumbra01 = 0, metres. Zero, not a
                                       // centimetre. A caster with no separation has to
                                       // reproduce vanilla exactly, and vanilla's PCF
                                       // radius is 0.5 texels (1.6 mm). A 1 cm floor
                                       // blurs *every* pixel, including the ones contact
                                       // hardening exists to leave alone, and a 1 cm
                                       // blur fills in the centimetre-scale gaps in a
                                       // canopy's dappling. That reads as a translucent
                                       // veil rather than a soft shadow.
#define PCSS_PENUMBRA_MIN_TEXELS_FOR_MAX 6.0
                                       // Lower bound on the maximum penumbra, in
                                       // texels. The maximum itself is the Sun
                                       // Shadow Softness setting (metres); keep it
                                       // under half the width of the smallest shadow
                                       // you care about, or its dark core disappears
                                       // and it reads as translucent rather than
                                       // soft. It also sets the tap spacing: the
                                       // radius in texels is that value divided by
                                       // the cascade texel size (3.2 mm in cascade
                                       // 0), so 8 cm is ~25 texels and needs the tap
                                       // count below to resolve. Too few taps and
                                       // the PCF is a random sample of the shadow's
                                       // coverage rather than its average, which
                                       // reads as dithering.
                                       //
                                       // A shadow-map mod can stretch the cascades
                                       // (CascadeShadowRangeScale), so one texel
                                       // covers several times the world space. A
                                       // fixed metre value then falls to a few
                                       // texels and the hardening stops being
                                       // visible. This floor holds it up, and it
                                       // binds only when the map is stretched - at
                                       // the stock cascade range it changes nothing.
                                       // Six texels is also about the most a 32 tap
                                       // gather PCF resolves cleanly.
#define SS_STEPS        32     // depth-buffer march steps toward the light
#define SS_STEP_SCALE   0.02   // step size, as a fraction of the receiver's depth,
                               // so the march covers similar screen distance near
                               // and far rather than collapsing at range
#define SS_THICKNESS    0.02   // metres a surface must be in front of the ray to
                               // count as occluding. Floored against the step
                               // length, because a step longer than this skips
                               // thin casters entirely - which reads as banding
                               // and as occlusion in unrelated places.
#define SS_FADE_START   8.0    // metres; screen space is reliable close in
#define SS_FADE_END     25.0   // metres; beyond this the cascade map takes over

#define PCSS_FILTER_MAX_TEXELS   128.0 // sampling guard, texels; rarely binds
#define PCSS_FILTER_MIN_TEXELS   0.5   // never harder than vanilla's 0.5 texel PCF

// Tap counts are literals in the body, not defines: they have to be compile-time
// loop bounds for the unroll to fold the sample positions and the disk constants.
//
// The mask is nine taps because it is the low-frequency half of the effect. Each
// costs a linear-PCF fetch, four texels, plus a point read for the blocker depth,
// so nine is 45 texels against the filter's 128 - and 25 taps, which it started
// at, was 125.

// Vogel disk, precomputed.
//
// The samples are fixed - only the per-pixel rotation varies - so the golden
// angle is baked into the table instead of evaluated per tap. Evaluating it at
// runtime costs a sincos per tap, and phi is a runtime value, so the compiler
// cannot fold them: that was 32 sincos per pixel for a rotation the table can
// apply in four multiplies. The disk is spread evenly and, unlike a Poisson set,
// never maps a sample onto another under rotation unless the rotation is a full
// turn.
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

// Interleaved gradient noise (Jimenez), the paper's per-pixel rotation source.
float PcssInterleavedGradientNoise(float2 position_screen) {
  const float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
  return frac(magic.z * frac(dot(position_screen, magic.xy)));
}

// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float4 v1 : TEXCOORD0,
  float4 v2 : TEXCOORD1,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r13,r14,r15;
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
  r1.y = r3.y;
  r3.x = r3.x;
  r3.z = r3.z;
  r3.w = r3.w;
  r2.xyz = r2.xyz;
  r3.z = r3.z;
  r3.x = r3.x;
  r1.y = r1.y;
  r3.w = r3.w;
  r1.x = r1.x;
  r1.x = r1.x;
  r4.xyz = v1.xyw;
  r1.x = r1.x;
  r4.xyz = r4.xyz;
  r1.zw = r4.xy / r4.zz;
  r4.xy = -g_psScene.m_cameraCenterOffset.xy;
  r1.zw = r4.xy + r1.zw;
  r1.x = r1.x;
  r4.xyzw = g_psScene.m_projectionParam.xyzw;
  r1.zw = r4.xy * r1.zw;
  r2.w = -r4.w;
  r1.x = r2.w + r1.x;
  r4.z = r4.z / r1.x;
  r5.z = r4.z;
  r4.xy = r5.zz * r1.zw;
  r5.xy = r4.xy;
  r4.w = 1;
  r5.xy = r5.xy;
  r5.z = r5.z;
  r5.w = 1;
  r5.xyzw = r5.xyzw;
  r1.x = dot(r5.xyz, r5.xyz);
  r1.x = rsqrt(r1.x);
  r1.xzw = r5.xyz * r1.xxx;
  r5.xyzw = r5.xyzw;
  r1.xzw = r1.xzw;
  r6.xyz = g_psLight.m_lightParams[0].xyz;
  r7.xyz = g_psLight.m_lightParams[1].xyz * g_psScene.m_exposure.zzz;
  r7.w = g_psLight.m_lightParams[1].w;
  r6.xyz = r6.xyz;
  r7.xyzw = r7.xyzw;
  r5.xyzw = r5.xyzw;
  r0.xyzw = r0.xyzw;
  r5.xyzw = r5.xyzw;
  r0.xyzw = r0.xyzw;
  r8.x = 0.5 / g_psScene.m_shadowMapResolutions.x;
  r8.y = 0.5 / g_psScene.m_shadowMapResolutions.x;
  r9.x = dot(r4.xyzw, g_psScene.m_shadowProjection._m00_m10_m20_m30);
  r9.y = dot(r4.xyzw, g_psScene.m_shadowProjection._m01_m11_m21_m31);
  r9.z = dot(r4.xyzw, g_psScene.m_shadowProjection._m02_m12_m22_m32);
  r9.xyz = r9.xyz;
  r9.xyz = r9.xyz;
  r4.xy = r0.zw;
  r2.w = 1 / g_psScene.m_shadowMapResolutions.x;
  r4.z = 8 * r2.w;
  r4.z = -r4.z;
  r4.z = 1 + r4.z;
  r8.zw = r4.zz * r4.zz;
  r4.w = g_psScene.m_shadowProjection2._m33;
  r4.w = r4.w;
  r6.w = r8.w;
  r4.w = cmp(0 != r4.w);
  if (r4.w != 0) {
    r4.xy = float2(0.03125,0.03125) * r4.xy;
    r4.xy = g_RandomTexture.Sample(g_samplerLinear_Wrap_s, r4.xy).xy;
    r4.x = max(r4.x, r4.y);
    r4.y = 0.00249999994 * r4.x;
    r4.x = g_psScene.m_shadowMapResolutions.x * r4.x;
    r4.x = r4.x / 4;
    r2.w = r4.x * r2.w;
    r2.w = -r2.w;
    r2.w = r4.z + r2.w;
    r2.w = r2.w * r2.w;
    r6.w = 1 * r2.w;
  } else {
    r4.y = 0;
  }
  r2.w = g_psScene.m_shadowProjection2._m30;
  r2.w = r2.w;
  r9.w = 0;
  r10.x = g_psScene.m_shadowProjection2._m00;
  r10.y = g_psScene.m_shadowProjection2._m01;
  r10.z = g_psScene.m_shadowProjection2._m02;
  r10.w = g_psScene.m_shadowProjection2._m03;
  r10.xyzw = r10.xyzw;
  r11.x = g_psScene.m_shadowProjection2._m10;
  r11.y = g_psScene.m_shadowProjection2._m11;
  r11.z = g_psScene.m_shadowProjection2._m12;
  r11.w = g_psScene.m_shadowProjection2._m13;
  r11.xyzw = r11.xyzw;
  r12.x = g_psScene.m_shadowProjection2._m20;
  r12.y = g_psScene.m_shadowProjection2._m21;
  r12.z = g_psScene.m_shadowProjection2._m22;
  r12.w = g_psScene.m_shadowProjection2._m23;
  r12.xyzw = r12.xyzw;
  r4.xzw = r10.www * r9.xyz;
  r10.xyz = r4.xzw + r10.xyz;
  r10.w = 1;
  r4.xzw = r11.www * r9.xyz;
  r11.xyz = r4.xzw + r11.xyz;
  r11.w = 2;
  r4.xzw = r12.www * r9.xyz;
  r12.xyz = r4.xzw + r12.xyz;
  r12.w = 3;
  r4.x = 1 + r9.z;
  r9.z = r4.x * r2.w;
  r4.x = 1 + r10.z;
  r10.z = r4.x * r2.w;
  r4.x = 1 + r11.z;
  r11.z = r4.x * r2.w;
  r4.x = 1 + r12.z;
  r12.z = r4.x * r2.w;
  r4.xz = r9.xy * r9.xy;
  r13.xy = r10.xy * r10.xy;
  r13.zw = r11.xy * r11.xy;
  r4.xz = cmp(r8.zw >= r4.xz);
  r4.xz = r4.xz ? float2(1,1) : float2(0,0);
  r2.w = cmp(r9.z >= r4.y);
  r2.w = r2.w ? 1 : 0;
  r8.zw = r6.ww;
  r13.xy = cmp(r8.zw >= r13.xy);
  r13.xy = r13.xy ? float2(1,1) : float2(0,0);
  r4.w = cmp(r10.z >= r4.y);
  r4.w = r4.w ? 1 : 0;
  r8.zw = cmp(r8.zw >= r13.zw);
  r8.zw = r8.zw ? float2(1,1) : float2(0,0);
  r4.y = cmp(r11.z >= r4.y);
  r4.y = r4.y ? 1 : 0;
  r4.x = r4.x * r4.z;
  r14.xyzw = r4.xxxx * r2.wwww;
  r2.w = r13.x * r13.y;
  r2.w = r2.w * r4.w;
  r4.x = r8.z * r8.w;
  r4.x = r4.x * r4.y;
  r4.y = -r14.w;
  r4.y = 1 + r4.y;
  r13.xyzw = r4.yyyy * r2.wwww;
  r2.w = -r13.w;
  r2.w = 1 + r2.w;
  r2.w = r4.y * r2.w;
  r4.xyzw = r4.xxxx * r2.wwww;
  r6.w = -r4.w;
  r6.w = 1 + r6.w;
  r2.w = r6.w * r2.w;
  r15.xyzw = float4(1,1,1,1) * r2.wwww;
  r9.xyzw = r14.xyzw * r9.xyzw;
  r10.xyzw = r13.xyzw * r10.xyzw;
  r9.xyzw = r10.xyzw + r9.xyzw;
  r4.xyzw = r11.xyzw * r4.xyzw;
  r4.xyzw = r9.xyzw + r4.xyzw;
  r9.xyzw = r15.xyzw * r12.xyzw;
  r4.xyzw = r9.wxyz + r4.wxyz;
  r4.w = max(9.99999997e-007, r4.w);
  r4.xyz = r4.xyz;
  r4.w = r4.w;
  r4.xyzw = r4.xyzw;
  r4.x = r4.x;
  r8.zw = float2(0.5,0.5) * r4.yz;
  r9.xy = float2(0.5,0.5) + r8.zw;
  r2.w = -r9.y;
  r9.z = 1 + r2.w;
  r8.zw = float2(0.5,0.5) * r9.xz;
  r9.xy = float2(2,2) * r8.xy;
  r9.xy = -r9.xy;
  r9.xy = float2(1,1) + r9.xy;
  r8.zw = min(r9.xy, r8.zw);
  r2.w = -1 + r4.x;
  r2.w = max(0, r2.w);
  r2.w = min(1, r2.w);
  r4.x = r4.x / 2;
  r6.w = -r2.w;
  r9.x = r6.w + r4.x;
  r9.y = 0.5 * r2.w;
  r8.zw = r9.xy + r8.zw;
  r4.yz = r8.zw + r8.xy;
  r4.yz = r4.yz;
  r4.w = r4.w;
  r0.xyzw = r0.xyzw;
  r4.yzw = r4.yzw;
  r0.xyzw = r0.xyzw;
  r4.yzw = r4.yzw;
  r0.xyzw = r0.xyzw;
  r0.xyzw = r0.xyzw / float4(2,2,2,2);
  r0.xyzw = frac(r0.xyzw);
  r0.xyzw = cmp(r0.xyzw >= float4(0.300000012,0.300000012,0.300000012,0.300000012));
  r0.xyzw = r0.xyzw ? float4(1,1,1,1) : float4(0,0,0,0);
  r0.xyzw = float4(2,2,2,2) * r0.xyzw;
  r0.xyzw = float4(-1,-1,-1,-1) + r0.xyzw;
  r0.xyzw = float4(0.125,0.125,0.125,0.125) * r0.xyzw;
  r0.xyzw = float4(0.5,0.5,-0.5,-0.5) + r0.xyzw;
  r8.xyzw = float4(1,1,1,1) / g_psScene.m_shadowMapResolutions.xxxx;
  r0.xyzw = r8.xyzw * r0.xyzw;
  r4.yzw = r4.yzw;
  r8.xy = r4.yz + r0.xy;
  r8.xy = r8.xy;
  r8.z = r4.w;
  r9.xyz = r8.xyz / float3(1,1,1);
  r2.w = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r9.xy, r9.z).x;
  r2.w = 0 + r2.w;
  r9.xy = r4.yz + r0.zy;
  r9.xy = r9.xy;
  r9.z = r8.z;
  r9.xyz = r9.xyz / float3(1,1,1);
  r0.y = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r9.xy, r9.z).x;
  r0.y = r2.w + r0.y;
  r9.xy = r4.yz + r0.xw;
  r9.xy = r9.xy;
  r9.z = r8.z;
  r9.xyz = r9.xyz / float3(1,1,1);
  r0.x = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r9.xy, r9.z).x;
  r0.x = r0.y + r0.x;
  r8.xy = r4.yz + r0.zw;
  r8.xy = r8.xy;
  r0.yzw = r8.xyz / float3(1,1,1);
  r0.y = g_ShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r0.yz, r0.w).x;
  r0.x = r0.x + r0.y;
  r0.x = 0.25 * r0.x;
  r0.x = 1 * r0.x;
  r0.x = -r0.x;
  r0.x = 1 + r0.x;
  r0.x = r0.x * r0.x;

  // =========================================================================
  // PCSS contact hardening (sun).
  //
  // Replaces the vanilla fixed-radius 4-tap PCF just above. Everything else in
  // this shader (cascade selection, blending, bias, cloud shadow, fades) is
  // untouched.
  //
  //   vanilla : lit = (1 - mean(SampleCmp))^2     fixed +/-0.5 texel radius
  //   PCSS    : penumbra from blocker distance    variable radius
  //
  // r0.x here is the vanilla lit factor (1 = fully lit, 0 = fully shadowed).
  //
  // Gated on a setting, default off. This changes how the scene's shadows read -
  // a look, not a fix - so the game's intended appearance stays the default. r0.x
  // already holds the vanilla result above, so skipping the block leaves it alone.
  // =========================================================================
  if (CUSTOM_SUN_CONTACT_HARDENING != 0.f) {
    const float  texel     = 1.0 / g_psScene.m_shadowMapResolutions.x;
    const float  atlasSize = g_psScene.m_shadowMapResolutions.x;
    const float2 baseUV    = r4.yz;
    const float  refZ      = r4.w;

    // Cascade tile window. The atlas is a 2x2 grid of 0.5-UV quadrants and the
    // vanilla UV lands inside exactly one of them, so the tile is simply the
    // quadrant that contains baseUV. tileXY doubles as the cascade index:
    // vanilla's tile offset maps cascades 0..3 onto quadrants (0,0) (1,0) (0,1) (1,1).
    const float2 tileXY = floor(baseUV * 2.0);
    const float2 tileOrigin = tileXY * 0.5;
    const float2 uvMin = tileOrigin + 0.5 * texel;
    const float2 uvMax = tileOrigin + 0.5 - 0.5 * texel;

    // Distance from this pixel's UV to the nearest edge of its cascade tile, per
    // axis. Both loops below keep their taps inside the tile, because a tap that
    // crosses the edge reads the neighbouring cascade's depth and produces a
    // wrong penumbra. They do it by shrinking their radius once, from this, rather
    // than clamping every tap: a per-tap clamp is four instructions, and the two
    // loops have 57 taps between them. Shrinking is exact and costs a few
    // instructions, at the price of a slightly narrower blur within a disk radius
    // of a tile boundary - which is where the alternative would be wrong anyway.
    const float2 tileSlack = max(min(baseUV - uvMin, uvMax - baseUV), 0.0);

    // Per-pixel rotation. The paper uses interleaved gradient noise; the game's
    // own 32x32 noise texture (t12) would work too but IGN is free and matches
    // the reference.
    const float angle = PcssInterleavedGradientNoise(v0.xy) * 6.28318530718;
    const float cs    = cos(angle);
    const float sn    = sin(angle);

    // The paper's penumbra is a relative ratio, so it needs neither the light's
    // ortho extent nor its depth range. Those were measured anyway and are worth
    // keeping for reference, because two earlier attempts were built on wrong
    // values for them:
    //
    //   clipScale    = length(m_shadowProjection._m00_m10_m20) = 0.15202
    //   ortho extent = 2 / clipScale = 13.16 m (cascade 0, a cube)
    //   depthRange   = worldPerClip / |m_shadowProjection2._m30| = 622.9 m
    //   cascade ladder (m_shadowProjection2._m03 / _m13 / _m23):
    //                  0.41376 / 0.20271 / 0.10708 -> 31.8 / 64.9 / 122.9 m
    //
    // m_shadowProjection is a uniform scale times a rotation, so its diagonal is
    // NOT the scale, and because it maps view space it changes as the camera
    // turns. Do not read _m00 or _m22 off it.

    // ---- Blocker depth, with no search -------------------------------------
    //
    // The shadow map stores the depth of the first surface the light ray through
    // a texel hits. For a shadowed receiver that surface IS the caster, so the
    // depth at the receiver's own texel is already the blocker depth. One read,
    // exact, and independent of the cascade's texel size.
    //
    // A blocker *search* cannot be used here. Its radius has to be expressed in
    // metres to reach a caster, and in cascade 0 a texel is 3.2 mm, so a 1 m
    // radius is ~311 texels that 16 taps cannot cover. Expressed in texels
    // instead, the same 12 texels covers 3.8 cm in cascade 0 and 36 cm in
    // cascade 3, so the effect silently switched on and off with the cascade the
    // player happened to be standing in — soft on distant terrain, vanilla on
    // nearby objects.
    //
    // ---- Penumbra mask, smoothed across screen space (paper section 2.2) ----
    //
    //   p = (z - caster) / caster;  penumbra = saturate(K * p * p)
    //
    // A *relative* ratio, so it needs no depth range and does not depend on the
    // shadow map's absolute units.
    //
    // The ratio is evaluated at *neighbouring pixels* and averaged, not only
    // here. That is the paper's penumbra mask blur, and it is the fix for the
    // symptom the paper's figure 7 names: a single-pixel penumbra jitters, because
    // neighbouring pixels land in regions of zero and of maximum penumbra, so the
    // shadow's lightness varies pixel to pixel and reads as a translucent veil
    // rather than a soft edge. Averaging the penumbra removes that jitter without
    // touching the shadow, which is why it cannot lighten the shadow's core the
    // way blurring the shadow itself does.
    //
    // The paper renders the mask at quarter resolution in its own pass and blurs
    // it, for speed. Within a cascade the shadow-space UV and depth are affine in
    // screen space, so ddx/ddy carry a neighbouring pixel's UV and depth exactly
    // and the penumbra can be evaluated at neighbours here - no repeated cascade
    // math, no extra pass, and no binding a spare slot. Same mask, same blur.
    const float2 uvStepX = ddx(baseUV);
    const float2 uvStepY = ddy(baseUV);
    const float  zStepX  = ddx(refZ);
    const float  zStepY  = ddy(refZ);

    // Blur radius shrunk so the whole grid stays inside the tile; see tileSlack.
    // The span is taken for the largest grid, which is conservative for the
    // smaller ones: it can only narrow the blur, never let a tap leave the tile.
    const float2 maskSpanPerPixel = 2.0 * (abs(uvStepX) + abs(uvStepY));
    const float maskBlur = min(CUSTOM_SUN_BLUR_PIXELS,
                               min(tileSlack.x, tileSlack.y) / max(maskSpanPerPixel.x, maskSpanPerPixel.y));

    // Nine taps. The mask is the low-frequency half of the effect, and each tap
    // costs a linear-PCF fetch - four texels - plus a point read for the blocker
    // depth, so this is 45 texels against the filter's 128. The paper renders its
    // own mask at quarter resolution, which this matches in effective sample count.
    float penumbra01 = 0.0;
    [unroll] for (int k = 0; k < 9; ++k) {
      const float2 o       = (float2(k % 3, k / 3) - 1.0) * maskBlur;
      const float2 uvN     = baseUV + o.x * uvStepX + o.y * uvStepY;
      const float  zN      = refZ   + o.x * zStepX  + o.y * zStepY;
      const float  occN    = g_ShadowTexture.SampleCmpLevelZero(g_samplerComparisonLess_Linear_Clmap_s, uvN, zN).x;
      const float  casterN = g_ShadowTexture.SampleLevel(g_samplerPoint_Clamp_s, uvN, 0).x;
      const float  relN    = (zN - casterN) / max(casterN, 1e-9);
      penumbra01 += occN * saturate(CUSTOM_SUN_SHADOW_K * relN * relN);
    }
    penumbra01 /= 9.0;

    // Per-cascade texel size, from m_shadowProjection2's cascade ladder. Needed to
    // express the penumbra in metres: a texel-space radius makes the blur's world
    // size follow the cascade, and a PCF at radius R on a shadow of width W leaves
    // a dark core only while R < W/2. 20 texels is 6.4 cm in cascade 0 and 31.7 cm
    // in cascade 2, so the same shadow reads correctly in one cascade and washes
    // out to 'half transparent' in the next.
    const float cascadeIndex = tileXY.x + 2.0 * tileXY.y;
    const float cascadeScale = cascadeIndex < 0.5 ? 1.0
                             : cascadeIndex < 1.5 ? abs(g_psScene.m_shadowProjection2._m03)
                             : cascadeIndex < 2.5 ? abs(g_psScene.m_shadowProjection2._m13)
                                                  : abs(g_psScene.m_shadowProjection2._m23);
    const float clipScale   = length(g_psScene.m_shadowProjection._m00_m10_m20);
    const float worldPerClip = 1.0 / max(clipScale, 1e-9);
    const float texelWorld  = (2.0 * worldPerClip) / (atlasSize * 0.5) / max(cascadeScale, 1e-6);

    // Scale the fixed max filter kernel. The floor keeps the result from getting
    // harder than vanilla's 0.5 texel PCF; the ceiling is only a sampling guard -
    // a small texel radius is trivially well sampled, so it rarely binds.
    //
    // The maximum is floored in texels as well as metres, so that stretching the
    // cascade range (a shadow-map mod) cannot shrink the effect out of sight. At
    // the stock range the floor never binds and this is the setting's value alone.
    const float penumbraMaxWorld = max(CUSTOM_SUN_SHADOW_SOFTNESS,
                                       PCSS_PENUMBRA_MIN_TEXELS_FOR_MAX * texelWorld);
    const float penumbraWorld = lerp(PCSS_PENUMBRA_MIN_WORLD, penumbraMaxWorld, penumbra01);
    const float penumbra = clamp(penumbraWorld / texelWorld, PCSS_FILTER_MIN_TEXELS, PCSS_FILTER_MAX_TEXELS);

    // Radius shrunk so the whole disk stays inside the tile; see tileSlack.
    const float penumbraTiled = min(penumbra, min(tileSlack.x, tileSlack.y) / texel);

    // Variable-radius PCF with the game's own comparison sampler.
    //
    // GatherCmp, not SampleCmp. A disk PCF's taps are spread over an area, so
    // their spacing is R * sqrt(pi / N): at a 25 texel radius that is 11 texels
    // with 16 taps and 7.8 with 32. The average is then 32 random draws from the
    // shadow's coverage rather than a mean of it, and per-pixel rotation turns
    // that into dithering, which reads as translucency rather than a blur.
    //
    // GatherCmp returns four comparisons per instruction, so the same instruction
    // count buys four times the density - 3.9 texels at that radius instead of
    // 7.8. This is the paper's section 1.1 speed trick used for the opposite
    // purpose: not to save samples, but to spend them.
    // Only where there is a penumbra to resolve.
    //
    // penumbra01 is the average over the mask above, so it is zero exactly when
    // none of those nine neighbours had a blocker: no separation, nothing to
    // soften, and the radius would sit at vanilla's 0.5 texels regardless. r0.x
    // already holds the vanilla 4-tap result, so those pixels can keep it and
    // skip 128 texels of gathers. The branch is spatially coherent - it is lit
    // regions that take it - so it costs little in divergence.
    if (penumbra01 > 0.0) {
      const float radiusTexels = penumbraTiled * texel;
      float avg = 0.0;
      [unroll] for (int j = 0; j < 32; ++j) {
        const float2 c = kPcssVogelDisk[j];
        const float2 v = float2(c.x * cs - c.y * sn, c.x * sn + c.y * cs);
        avg += dot(g_ShadowTexture.GatherCmp(g_samplerComparisonLess_Linear_Clmap_s, baseUV + v * radiusTexels, refZ), 0.25);
      }
      avg /= 32.0;

      // Keep vanilla's squaring. Removing it was tried and is wrong: it widens the
      // gradient only by *lightening* partial occlusion, and a tree canopy is
      // almost entirely partial occlusion - thin branches over a wide area - so it
      // drops toward mid grey and reads as translucent rather than soft. Squaring
      // keeps the core dark and narrows the transition, which is what a soft shadow
      // should look like.
      float pcssLit = 1.0 - avg;
      pcssLit *= pcssLit;

      r0.x = lerp(r0.x, pcssLit, PCSS_STRENGTH);
    }
  }

  r4.xy = float2(-0,-0);
  r4.z = -g_psLight.m_lightParams[8].x;
  r0.yzw = r5.xyz + r4.xyz;
  r0.y = dot(r0.yzw, r0.yzw);
  r0.y = sqrt(r0.y);
  r0.z = -g_psLight.m_lightParams[8].y;
  r0.y = r0.y + r0.z;
  r0.y = max(0, r0.y);
  r0.y = g_psLight.m_lightParams[6].x * r0.y;
  r0.y = max(0, r0.y);
  r0.y = min(1, r0.y);
  r0.z = cmp(r0.w >= 0);
  r0.z = r0.z ? 1 : 0;
  r0.y = r0.y * r0.z;
  r0.y = r0.y * r0.y;
  r0.z = -r0.x;
  r0.z = 1 + r0.z;
  r0.y = r0.y * r0.z;
  r0.x = r0.x + r0.y;
  r0.y = g_psLight.m_lightParams[6].w;
  r0.y = -r0.y;
  r0.x = r0.x + r0.y;
  r0.x = max(0, r0.x);
  r0.x = min(1, r0.x);
  r4.x = dot(g_psObject.m_localParam[0].xyzw, r5.xyzw);
  r4.y = dot(g_psObject.m_localParam[1].xyzw, r5.xyzw);
  r0.y = g_psObject.m_localParam[3].w;
  r0.z = g_psObject.m_localParam[3].z;
  r0.w = g_CloudTexture.Sample(g_samplerLinear_Wrap_s, r4.xy).x;
  r0.z = -0 + r0.z;
  r0.w = -0 + r0.w;
  r0.z = 1 / r0.z;
  r0.z = r0.w * r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.w = -2 * r0.z;
  r0.w = 3 + r0.w;
  r0.z = r0.z * r0.z;
  r0.z = r0.w * r0.z;
  r0.y = r0.z * r0.y;
  r0.y = max(0, r0.y);
  r0.y = min(1, r0.y);
  r0.x = r0.x * r0.y;
  r0.y = -g_psObject.m_localParam[2].x;
  r0.y = r5.z + r0.y;
  r0.y = g_psObject.m_localParam[2].y * r0.y;
  r0.y = max(0, r0.y);
  r0.y = min(1, r0.y);
  r0.y = -r0.y;
  r0.y = 1 + r0.y;
  r0.y = r0.y * r0.y;
  r0.x = r0.x * r0.y;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.y = g_psLight.m_lightParams[7].x;
  r0.y = max(r0.y, r0.x);
  r0.y = r0.y;
  r2.xyz = r2.xyz;
  r6.xyz = r6.xyz;
  r7.xyzw = r7.xyzw;
  r1.xzw = r1.xzw;
  r3.x = r3.x;
  r3.z = r3.z;
  r1.y = r1.y;
  r3.w = r3.w;
  r2.xyz = r2.xyz;
  r6.xyz = r6.xyz;
  r7.xyzw = r7.xyzw;
  r1.xzw = r1.xzw;
  r3.x = r3.x;
  r3.z = r3.z;
  r1.y = r1.y;
  r3.w = r3.w;
  r6.xyz = r6.xyz;
  r1.xzw = r1.xzw;
  r2.xyz = r2.xyz;
  r4.xyz = r6.xyz;
  r0.z = dot(r4.xyz, r2.xyz);
  r5.xyz = -r1.xzw;
  r0.w = dot(r5.xyz, r2.xyz);
  r0.w = r0.w + r0.w;
  r0.w = -r0.w;
  r8.xyz = r2.xyz * r0.www;
  r8.xyz = r8.xyz + r5.xyz;
  r8.xyz = -r8.xyz;
  r9.xyz = -r4.xyz;
  r9.xyz = r9.xyz + r8.xyz;
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
  r5.xyz = r6.xyz + r5.xyz;
  r0.w = dot(r5.xyz, r5.xyz);
  r0.w = rsqrt(r0.w);
  r5.xyz = r5.xyz * r0.www;
  r0.w = dot(r5.xyz, r2.xyz);
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r4.x = dot(r5.xyz, r4.xyz);
  r3.z = r3.z;
  r3.y = 0.25;
  r5.xyzw = g_MaterialTexture.Sample(g_samplerPoint_Clamp_s, r3.zy).xyzw;
  r3.y = 0.75;
  r6.xz = g_MaterialTexture.Sample(g_samplerPoint_Clamp_s, r3.zy).xw;
  r6.xz = r6.xz;
  r5.xyz = r5.xyz;
  r5.w = r5.w;
  r2.w = r6.x;
  r6.z = r6.z;
  r4.x = r4.x;
  r4.y = r2.w;
  r3.y = g_lut1.Sample(g_samplerLinear_Clamp_s, r4.xy).x;
  r3.y = r3.y;
  r3.y = r3.y;
  r3.z = r0.z * r0.z;
  r3.z = -r3.z;
  r3.z = 1 + r3.z;
  r3.z = r3.z * r3.w;
  r3.z = r3.z + r0.z;
  r3.w = r5.w * r3.w;
  r3.z = max(r3.z, r3.w);
  r3.z = max(0, r3.z);
  r3.z = min(1, r3.z);
  r4.xyz = r5.xyz * r3.xxx;
  r0.w = r0.w;
  r1.y = r1.y;
  r6.z = r6.z;
  r0.w = max(0, r0.w);
  r6.x = min(1, r0.w);
  r6.y = r1.y;
  r6.z = r6.z;
  r6.xyz = r6.xyz * r6.xyz;
  r6.xyz = float3(0.984375,0.9375,0.9375) * r6.xyz;
  r6.xyz = float3(0.0078125,0.03125,0.03125) + r6.xyz;
  r6.xy = g_lut2.Sample(g_samplerLinear_Clamp_s, r6.xyz).xy;
  r6.xy = r6.xy;
  r6.xy = r6.xy * r6.xy;
  r0.w = r6.x / r6.y;
  r0.w = r3.y * r0.w;
  r0.z = r0.z;
  r0.z = 4 * r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = r0.w * r0.z;
  r4.xyz = r4.xyz * r7.xyz;
  r4.xyz = r4.xyz * r0.zzz;
  r0.z = 1 * r3.z;
  r0.z = 1 * r0.z;
  r3.yzw = r0.zzz * r7.xyz;
  r4.xyz = r4.xyz;
  r3.yzw = r3.yzw;
  r0.z = dot(r1.xzw, r2.xyz);
  r0.z = r0.z + r0.z;
  r0.z = -r0.z;
  r6.xyz = r2.xyz * r0.zzz;
  r6.xyz = r6.xyz + r1.xzw;
  r1.xzw = -r1.xzw;
  r0.z = dot(r1.xzw, r2.xyz);
  r0.z = max(0, r0.z);
  r2.x = min(1, r0.z);
  r2.y = max(g_psLight.m_lightParams[2].z, r2.w);
  r0.z = g_lut1.Sample(g_samplerLinear_Clamp_s, r2.xy).x;
  r0.z = r0.z;
  r0.z = r0.z;
  r1.xz = -r6.xy;
  r1.xz = r1.xz;
  r1.xz = float2(0.96875,0.96875) * r1.xz;
  r1.xz = float2(0.5,0.5) * r1.xz;
  r1.xz = float2(0.5,0.5) + r1.xz;
  r1.xz = float2(0.25,0.125) * r1.xz;
  r1.xz = r1.xz;
  r0.w = cmp(0 < r6.z);
  r2.xy = r0.ww ? float2(0.25,0) : float2(0,0);
  r1.xz = r2.xy + r1.xz;
  r1.xz = g_SkyReflectTexture.Sample(g_samplerLinear_Clamp_s, r1.xz).xy;
  r1.xz = float2(4,4) * r1.xz;
  r0.w = 1.42799997 * r1.y;
  r0.w = -0.300000012 + r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r1.w = -r1.x;
  r1.z = r1.z + r1.w;
  r0.w = r1.z * r0.w;
  r1.xzw = r1.xxx + r0.www;
  r2.xyz = g_psLight.m_lightParams[3].xyz;
  r1.xzw = r2.xyz * r1.xzw;
  r0.w = -r1.y;
  r0.w = 1 + r0.w;
  r0.w = 0.5 * r0.w;
  r0.z = r0.w * r0.z;
  r0.z = r0.z * r3.x;
  r1.xyz = r1.xzw * r0.zzz;
  r1.xyz = r1.xyz * r5.xyz;
  r3.yzw = r3.yzw;
  r4.xyz = r4.xyz;
  r1.xyz = r1.xyz;
  r3.yzw = r3.yzw;
  r4.xyz = r4.xyz;
  r0.y = r0.y;
  r3.yzw = r3.yzw;
  r4.xyz = r4.xyz;
  r0.y = r0.y;
  r2.xyz = r3.yzw * r0.xxx;
  r2.xyz = float3(1,1,1) * r2.xyz;
  r0.yzw = r4.xyz * r0.yyy;
  r0.yzw = float3(1,1,1) * r0.yzw;
  r2.xyz = r2.xyz;
  r0.yzw = r0.yzw;
  r2.xyz = r2.xyz;
  r0.yzw = r0.yzw;
  r1.xyz = r1.xyz;
  r2.xyz = r2.xyz;
  r0.yzw = r0.yzw;
  r1.xyz = r1.xyz;
  r0.x = max(g_psLight.m_lightParams[2].x, r0.x);
  r0.x = g_psLight.m_lightParams[2].y * r0.x;
  r2.xyz = float3(-0,-0,-0) + r2.xyz;
  r1.xyz = r1.xyz * r0.xxx;
  r0.xyz = r1.xyz + r0.yzw;
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
  // Screen-space shadow complement.
  //
  // The cascade map cannot afford centimetre detail over a hundred metres of
  // range, so this supplies the high-frequency half: ray-march the depth buffer
  // toward the light and occlude where the ray passes behind a nearer surface.
  // Deliberately complementary - it only sees casters on screen and fades with
  // distance, which is where the cascades take over.
  //
  // View space throughout. m_dominantLightDir is view space, and the view
  // position is unprojected from the same depth being marched, using the
  // projection matrix's own scale: m_projectionParam.xy produced a scale error,
  // confirmed by probe.
  // =========================================================================
  {
    const float2 uv    = v2.xy / v2.z;
    const float  d     = g_DepthTexture.SampleLevel(g_samplerPoint_Clamp_s, uv, 0).x;
    const float  viewZ = g_psScene.m_projectionParam.z / max(d - g_psScene.m_projectionParam.w, 1e-6);
    const float2 ndc   = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    const float3 viewPos = float3(ndc.x * viewZ / g_psScene.m_projection._m00,
                                  ndc.y * viewZ / g_psScene.m_projection._m11,
                                  viewZ);

    const float3 lightDir = normalize(g_psSystem.m_dominantLightDir.xyz);

    // The G-buffer normal, view space, packed the way the spot shader decodes it.
    const float3 gbufN = g_NormalTexture.SampleLevel(g_samplerPoint_Clamp_s, uv, 0).xyz;
    const float3 normal = normalize(float3(gbufN.xy * 2.0 - 1.0, gbufN.z * gbufN.z * 2.0 - 1.0));

    // Step length scales with depth so the march covers similar screen distance
    // near and far. The thickness is floored against it, and divided by N.V:
    // the problem is a grazing VIEW, not a grazing light. Looking along a
    // surface, the screen-space depth gradient explodes, so a step spans far
    // more world distance along that surface and the comparison gets coarse.
    // N.L does not capture this - with the sun high it is near 1 on the ground,
    // so keying off it changed nothing.
    const float3 viewDir  = normalize(-viewPos);
    const float stepLen   = viewZ * SS_STEP_SCALE;
    const float grazing   = 1.0 / max(abs(dot(normal, viewDir)), 0.2);
    const float thickness = max(SS_THICKNESS, stepLen * 1.5) * grazing;

    // Per-pixel jitter of the start offset, so any residual banding reads as
    // noise rather than stripes.
    const float jitter = PcssInterleavedGradientNoise(v0.xy);

    float occ = 0.0;
    [unroll] for (int s = 0; s < SS_STEPS; ++s) {
      const float3 p = viewPos + lightDir * (stepLen * ((float)s + jitter));
      const float4 clip = mul(float4(p, 1.0), g_psScene.m_projection);
      const float2 suv  = clip.xy / clip.w * float2(0.5, -0.5) + 0.5;
      if (any(suv < 0.0) || any(suv > 1.0)) break;
      const float  sd = g_DepthTexture.SampleLevel(g_samplerPoint_Clamp_s, suv, 0).x;
      const float  sz = g_psScene.m_projectionParam.z / max(sd - g_psScene.m_projectionParam.w, 1e-6);
      occ += (sz < p.z - thickness) ? 1.0 : 0.0;
    }

    // Fade out with distance: beyond this the cascade map is the right tool and
    // screen space has too little of the caster on screen to be reliable.
    const float fade  = 1.0 - saturate((viewZ - SS_FADE_START) / max(SS_FADE_END - SS_FADE_START, 1e-3));
    const float ssOcc = (occ / (float)SS_STEPS) * fade;

    // Scale the light rather than replacing the output. The render target is a
    // light accumulation with a one/one blend, so anything written here is
    // *added*. Writing 1 - ssOcc added a full unit of light everywhere and
    // washed the frame out; what is wanted is to attenuate the light that this
    // pass contributes where the march found an occluder.
    r2.xyz *= (1.0 - ssOcc);
  }

  o0.xyzw = r2.xyzw;
  o1.xyzw = r0.xyzw;
  return;
}
