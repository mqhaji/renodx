// MGSV spot light with shadow — CRC32 0x08292A5E, ps_5_0.
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

// Tunables are pinned while iterating through the devkit: that path replaces the
// shader without supplying cb13, and D3D11 returns 0 for an unbound constant
// buffer, so anything read from it - including a gate - comes back as 0 and the
// effect disappears. Wire these to settings once the look is right.
#define SPOT_PCSS_ENABLED  1.0
#define SPOT_SOFTNESS_M    0.05  // widest penumbra, metres
#define SPOT_K             24.0  // how fast separation reaches full softness
#define SPOT_BLUR_PIXELS   24.0  // penumbra mask neighbour spacing, screen pixels
#define SPOT_MIN_TEXELS    6.0   // floor on the maximum, texels

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
  r4.w = r4.w * r4.w;
  r2.w = g_psLight.m_lightParams[6].w * r2.w;
  r2.w = -r2.w;
  r2.w = r4.w + r2.w;
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
  if (SPOT_PCSS_ENABLED != 0.f) {
    const float  shadowRes = max(g_psScene.m_shadowMapResolutions.y, 1.0);
    const float  texel     = 1.0 / shadowRes;
    const float2 baseUV    = r9.xy / r9.w;
    const float  refZ      = r9.z / r9.w;

    // World size of one texel at this receiver's depth. m_shadowProjection is a
    // perspective transform here, so its x scale converts clip units back to
    // world: r9.w / |_m00| is one UV unit, and a texel is that over the map
    // resolution. The sun reads this off the cascade ladder instead.
    const float texelWorld = r9.w / max(abs(g_psScene.m_shadowProjection._m00), 1e-9) / shadowRes;

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
    float penumbra01 = 0.0;
    [unroll] for (int k = 0; k < 9; ++k) {
      const float2 o       = (float2(k % 3, k / 3) - 1.0) * SPOT_BLUR_PIXELS;
      const float2 uvN     = baseUV + o.x * uvStepX + o.y * uvStepY;
      const float  zN      = refZ   + o.x * zStepX  + o.y * zStepY;
      const float  occN    = g_ShadowTexture.SampleCmpLevelZero(g_samplerComparisonLess_Linear_Clmap_s, uvN, zN).x;
      const float  casterN = g_ShadowTexture.SampleLevel(g_samplerPoint_Clamp_s, uvN, 0).x;
      const float  relN    = (zN - casterN) / max(casterN, 1e-9);
      penumbra01 += occN * saturate(SPOT_K * relN * relN);
    }
    penumbra01 /= 9.0;

    // Maximum penumbra, floored in texels so a coarse shadow map cannot shrink
    // the effect out of sight.
    const float penumbraMaxWorld = max(SPOT_SOFTNESS_M, SPOT_MIN_TEXELS * texelWorld);
    const float penumbraWorld    = penumbraMaxWorld * penumbra01;
    const float penumbra         = clamp(penumbraWorld / max(texelWorld, 1e-9), 0.5, 128.0);

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
  o0.xyzw = r2.xyzw;
  o1.xyzw = r0.xyzw;
  return;
}