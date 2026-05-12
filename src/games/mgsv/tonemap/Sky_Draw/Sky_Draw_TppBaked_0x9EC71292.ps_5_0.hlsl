#include "../tonemap.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:30 2026

// clang-format off
cbuffer cPSScene : register(b2) {
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
  } g_psScene: packoffset(c0);
}

cbuffer cPSObject : register(b5) {
  struct
  {
    float4x4 m_viewWorld;
    float4x4 m_world;
    float4 m_useWeightCount;
    float4 m_localParam[4];
  } g_psObject: packoffset(c0);
}

cbuffer cPSMaterial : register(b4) {
  struct
  {
    float4 m_materials[8];
  } g_psMaterial: packoffset(c0);
}

cbuffer cPSSystem : register(b0) {
  struct
  {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem: packoffset(c0);
}
// clang-format on

SamplerState inMoonSampler_s : register(s0);
SamplerState g_samplerPoint_Wrap_s : register(s8);
SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inMoonTexture : register(t0);
Texture2D<float4> inInscattering : register(t3);
Texture2D<float4> inMesh : register(t8);
Texture2D<float4> g_tex_fog : register(t12);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position0,
    float4 v1: TEXCOORD0,
    float4 v2: TEXCOORD1,
    float4 v3: TEXCOORD2,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2, r3, r4, r5, r6, r7;
  uint4 bitmask, uiDest;
  float4 fDest;

  // NCalculateTextureCordinate: screen position, dither UV, and clip coordinate.
  r0.xy = float2(-0.5, -0.5) + v0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.zw = float2(0.125, 0.125) * r0.xy;
  r0.zw = frac(r0.zw);
  r0.zw = r0.zw;
  r0.xy = r0.xy / g_psSystem.m_renderInfo.xy;
  r0.xy = float2(2, -2) * r0.xy;
  r0.xy = float2(-1, 1) + r0.xy;
  r0.xy = r0.xy;

  // NViewRay: normalize sky view ray and apply camera-relative vertical offset.
  r1.xyz = v1.xyz;
  r1.xyz = r1.xyz;
  r1.w = g_psMaterial.m_materials[2].y;
  r2.x = g_psMaterial.m_materials[2].w;
  r2.y = g_psMaterial.m_materials[6].z;
  r2.z = dot(r1.xyz, r1.xyz);
  r2.z = rsqrt(r2.z);
  r1.xyz = r2.zzz * r1.xyz;
  r1.w = 1 / r1.w;
  r2.x = g_psScene.m_eyepos.y * r2.x;
  r2.y = -r2.y;
  r2.y = r2.x + r2.y;
  r2.xz = float2(0, 0);
  r2.xyz = r2.xyz * r1.www;
  r1.xyz = r2.xyz + r1.xyz;
  r1.w = dot(r1.xyz, r1.xyz);
  r1.w = rsqrt(r1.w);
  r1.xyz = r1.yxz * r1.www;
  r1.x = max(0, r1.x);
  r1.yz = r1.yz;
  r1.x = r1.x;

  // NSunDirectColor: sample moon texture and mask by UV visibility and view zenith.
  r2.xyzw = v3.xyzw;
  r1.w = r1.x;
  r3.xy = v2.zw;
  r2.xyzw = r2.xyzw;
  r1.w = r1.w;
  r3.xy = r3.xy;
  r4.xyzw = inMoonTexture.Sample(inMoonSampler_s, v2.zw).xyzw;

  if (CUSTOM_BOOST_SUN != 0.f) {
    r4.w /= 10.f;
  }

  r2.xyzw = r4.xyzw * r2.xyzw;
  r3.zw = cmp(float2(1, 1) >= r3.xy);
  r3.zw = r3.zw ? float2(1, 1) : float2(0, 0);
  r3.xy = cmp(r3.xy >= float2(0, 0));
  r3.xy = r3.xy ? float2(1, 1) : float2(0, 0);
  r4.xy = cmp(float2(0, 0) != r3.zw);
  r4.zw = cmp(float2(0, 0) != r3.xy);
  r3.xyzw = cmp((int4)r4.xyzw != int4(0, 0, 0, 0));
  r3.xy = r3.zw ? r3.xy : 0;
  r3.x = r3.y ? r3.x : 0;
  r3.x = r3.x ? 1.000000 : 0;
  r3.y = cmp(r1.w < 9.99999975e-05);
  r3.y = r3.y ? 0 : 1;
  r3.x = r3.x * r3.y;
  r3.yzw = r2.xyz * r2.www;
  r3.yzw = max(float3(0, 0, 0), r3.yzw);
  r2.xyz = min(float3(1, 1, 1), r3.yzw);
  r2.w = r2.w;
  r2.xyzw = r2.xyzw * r3.xxxx;
  r2.xyzw = r2.xyzw;

  // NLightSource2: prepare sun color and background sky intensity.
  r3.xyzw = g_psMaterial.m_materials[3].xyzw;
  r4.x = g_psScene.m_exposure.z;
  r3.w = r4.x * r3.w;
  r3.w = min(1000, r3.w);
  r3.xyz = r3.xyz;
  r4.y = g_psMaterial.m_materials[7].x;
  r4.yzw = float3(0.00328400009, 0.0041100001, 0.00624000002) * r4.yyy;
  r4.xyz = r4.yzw * r4.xxx;
  r3.xyz = r3.xyz;
  r4.xyz = r4.xyz;
  r5.xyz = r1.yxz;
  r3.xyz = r3.xyz;

  // NInscatterColor2: phase terms and 4D inscattering texture lookup.
  r5.xyz = r5.xyz;
  r3.xyz = r3.xyz;
  r1.y = dot(r5.xyz, r3.xyz);
  r1.z = r1.y * r1.y;
  r1.z = 1 + r1.z;
  r1.z = 0.0596831031 * r1.z;
  r1.z = r1.z;
  r3.x = g_psMaterial.m_materials[2].x;
  r3.y = r3.x * r3.x;
  r3.y = -r3.y;
  r3.y = 1 + r3.y;
  r3.y = 0.0795774683 * r3.y;
  r3.x = r3.x * r1.y;
  r3.x = -r3.x;
  r3.x = 1 + r3.x;
  r3.x = r3.x * r3.x;
  r3.x = r3.y / r3.x;
  r3.x = r3.x;
  r1.x = 6360.00977 * r1.x;
  r3.y = r1.x * r1.x;
  r3.y = -40449728 + r3.y;
  r3.y = 40449600 + r3.y;
  r1.x = -1 * r1.x;
  r3.y = 766800 + r3.y;
  r3.y = sqrt(r3.y);
  r1.x = r3.y + r1.x;
  r1.x = r1.x / 886.949463;
  r1.x = 0.4921875 * r1.x;
  r5.y = 0.50390625 + r1.x;
  r1.x = g_psMaterial.m_materials[3].y;
  r1.x = max(-0.197500005, r1.x);
  r1.x = 5.34962368 * r1.x;
  r3.y = -r1.x;
  r3.y = max(r3.y, r1.x);
  r3.z = min(1, r3.y);
  r4.w = max(1, r3.y);
  r4.w = 1 / r4.w;
  r3.z = r4.w * r3.z;
  r4.w = r3.z * r3.z;
  r5.z = 0.0208350997 * r4.w;
  r5.z = -0.0851330012 + r5.z;
  r5.z = r5.z * r4.w;
  r5.z = 0.180141002 + r5.z;
  r5.z = r5.z * r4.w;
  r5.z = -0.330299497 + r5.z;
  r4.w = r5.z * r4.w;
  r4.w = 0.999866009 + r4.w;
  r3.z = r4.w * r3.z;
  r3.y = cmp(1 < r3.y);
  r4.w = -2 * r3.z;
  r4.w = 1.57079637 + r4.w;
  r3.y = r3.y ? r4.w : 0;
  r3.y = r3.y + r3.z;
  r3.y = 0 + r3.y;
  r1.x = min(1, r1.x);
  r3.z = -r1.x;
  r1.x = cmp(r1.x < r3.z);
  r3.z = -r3.y;
  r1.x = r1.x ? r3.z : r3.y;
  r1.x = r1.x / 1.10000002;
  r1.x = 0.74000001 + r1.x;
  r1.x = 0.5 * r1.x;
  r1.x = 0.96875 * r1.x;
  r1.x = 0.015625 + r1.x;
  r1.y = 1 + r1.y;
  r1.y = r1.y / 2;
  r1.y = 7 * r1.y;
  r3.y = floor(r1.y);
  r3.z = -r3.y;
  r1.y = r3.z + r1.y;
  r1.x = r3.y + r1.x;
  r5.x = r1.x / 8;
  r5.y = r5.y;
  r1.x = 1 + r1.x;
  r6.x = r1.x / 8;
  r6.y = r5.y;
  r5.xyzw = inInscattering.Sample(g_samplerLinear_Clamp_s, r5.xy).xyzw;
  r1.x = -r1.y;
  r1.x = 1 + r1.x;
  r5.xyzw = r5.xyzw * r1.xxxx;
  r6.xyzw = inInscattering.Sample(g_samplerLinear_Clamp_s, r6.xy).xyzw;
  r6.xyzw = r6.xyzw * r1.yyyy;
  r5.xyzw = r6.xyzw + r5.xyzw;
  r5.xyzw = r5.xyzw;
  r5.xyzw = r5.xyzw;
  r5.xyzw = max(float4(0, 0, 0, 0), r5.xyzw);
  r5.xyzw = r5.xyzw;

  // getMie / inscatter approximation: derive mie and rayleigh contribution.
  r6.xyz = g_psMaterial.m_materials[0].xyz;
  r7.xyz = r5.xyz * r5.www;
  r1.x = max(9.99999975e-05, r5.x);
  r7.xyz = r7.xyz / r1.xxx;
  r6.xyz = r6.xxx / r6.xyz;
  r6.xyz = r7.xyz * r6.xyz;
  r6.xyz = r6.xyz;
  r5.xyz = r5.xyz;
  r1.x = dot(r5.xyz, r5.xyz);
  r1.x = sqrt(r1.x);
  r1.x = 0.800000012 * r1.x;
  r3.xyz = r6.xyz * r3.xxx;
  r6.xyz = float3(0.200000003, 0.200000003, 0.200000003) * r3.xyz;
  r6.xyz = r6.xyz + r1.xxx;
  r6.xyz = float3(0.100000001, 0.100000001, 0.100000001) * r6.xyz;
  r1.xyz = r5.xyz * r1.zzz;
  r4.w = g_psObject.m_localParam[1].x;
  r4.w = -r4.w;
  r5.xyz = float3(1, 1, 1) + r4.www;
  r1.xyz = r5.xyz * r1.xyz;
  r5.xyz = g_psObject.m_localParam[1].xxx;
  r5.xyz = r6.xyz * r5.xyz;
  r1.xyz = r5.xyz + r1.xyz;
  r1.xyz = r1.xyz + r3.xyz;
  r1.xyz = max(float3(0, 0, 0), r1.xyz);
  r1.xyz = r1.xyz * r3.www;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;

  // NAddColor4: add background light to inscattering.
  r4.xyz = r4.xyz;
  r1.xyz = r1.xyz;
  r4.xyz = r4.xyz;
  r1.xyz = r4.xyz + r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;

  // NLightContribution: blend direct moon/sun color into sky inscatter.
  r2.xyzw = r2.xyzw;
  r1.xyz = r1.xyz;
  r2.xyzw = r2.xyzw;
  r2.w = -r2.w;
  r2.w = 1 + r2.w;
  r1.xyz = r2.www * r1.xyz;
  r1.xyz = r2.xyz + r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;

  // NSkyColor2: apply sky color multipliers.
  r2.x = g_psMaterial.m_materials[0].w;
  r2.y = g_psMaterial.m_materials[4].w;
  r2.z = g_psMaterial.m_materials[6].w;
  r1.xyz = r2.xyz * r1.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;
  r0.xy = r0.xy;
  r1.xyz = r1.xyz;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;

  // NApplyFurthestFog2 / GetFurthestVolumetricFog2D: sample and apply far volumetric fog.
  r0.xy = float2(0.0146484375, 0.123046875) * r0.xy;
  r0.xy = float2(0.015625, 0.125) + r0.xy;
  r0.xy = float2(0.96875, 0.75) + r0.xy;
  r2.xyzw = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r0.xy).xyzw;
  r0.x = g_psScene.m_fogParam[1].y;
  r2.xyz = r2.xyz * r0.xxx;
  r2.xyz = r2.xyz;
  r2.w = r2.w;
  r2.xyzw = r2.xyzw;
  r2.xyzw = r2.xyzw;
  r0.x = g_psMaterial.m_materials[1].w;
  r0.y = g_psMaterial.m_materials[5].w;
  r3.x = g_psMaterial.m_materials[6].x;
  r3.y = g_psMaterial.m_materials[6].y;
  r1.w = -1 + r1.w;
  r0.x = r0.x;
  r0.y = r0.y;
  r3.x = r3.x;
  r3.y = r3.y;
  r1.w = r3.y * r1.w;
  r1.w = max(0, r1.w);
  r1.w = min(1, r1.w);
  r3.x = r3.x * r1.w;
  r1.w = r3.x + r1.w;
  r3.x = 1 + r3.x;
  r1.w = r1.w / r3.x;
  r0.y = r1.w * r0.y;
  r0.x = r0.x + r0.y;
  r0.x = r0.x;
  r0.x = r0.x;
  r2.xyz = r2.xyz * r0.xxx;
  r0.y = -r2.w;
  r0.y = 1 + r0.y;
  r0.x = r0.y * r0.x;
  r0.x = -r0.x;
  r0.x = 1 + r0.x;
  r1.xyz = r1.xyz * r0.xxx;
  r1.xyz = r1.xyz + r2.xyz;
  r1.xyz = r1.xyz;
  r1.xyz = r1.xyz;

  // NDithering2: add blue-noise dither from mesh texture.
  r0.zw = r0.zw;
  r1.xyz = r1.xyz;
  r0.zw = r0.zw;
  r0.x = inMesh.Sample(g_samplerPoint_Wrap_s, r0.zw).x;
  r0.x = -0.5 + r0.x;
  r0.x = r0.x / 128;
  r0.xyz = r1.xyz + r0.xxx;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;

  // NCalcurateOutputColor: TppTonemap curve.
  float3 untonemapped = r0.xyz;
  r1.xyz = g_psMaterial.m_materials[1].xyz;
  r1.xyz = r1.xyz;
  r0.xyz = r0.xyz;
  r2.xyz = r1.yyy;
  r2.xzw = cmp(r2.xyz >= r0.xyz);
  r2.xzw = r2.xzw ? float3(1, 1, 1) : float3(0, 0, 0);
  r3.xyz = r2.xzw * r0.xyz;
  r2.xzw = -r2.xzw;
  r2.xzw = float3(1, 1, 1) + r2.xzw;
  r0.xyz = r0.xyz + r1.zzz;
  r4.xyz = -r2.yyy;
  r0.xyz = r4.xyz + r0.xyz;
  r0.xyz = r1.xxx * r0.xyz;
  r0.xyz = float3(-1, -1, -1) / r0.xyz;
  r0.xyz = r0.xyz + r1.zzz;
  r0.xyz = r0.xyz + r2.yyy;
  r0.xyz = r2.xzw * r0.xyz;
  r0.xyz = r3.xyz + r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = max(float3(0, 0, 0), r0.xyz);
  r0.xyz = min(float3(1, 1, 1), r0.xyz);

  if (RENODX_TONE_MAP_TYPE != 0.f) {
    r0.rgb = untonemapped;
  }

  // GammaCorrection: linear RGB to sRGB output.
  r1.xyz = cmp(float3(0.00313080009, 0.00313080009, 0.00313080009) >= r0.xyz);
  r1.xyz = r1.xyz ? float3(1, 1, 1) : float3(0, 0, 0);
  r2.xyz = float3(12.9200001, 12.9200001, 12.9200001) * r0.xyz;
  r2.xyz = r2.xyz * r1.xyz;
  r1.xyz = -r1.xyz;
  r1.xyz = float3(1, 1, 1) + r1.xyz;
  r0.xyz = max(float3(9.99999975e-06, 9.99999975e-06, 9.99999975e-06), r0.xyz);
  r0.xyz = log2(r0.xyz);
  r0.xyz = float3(0.416666657, 0.416666657, 0.416666657) * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  r0.xyz = float3(1.05499995, 1.05499995, 1.05499995) * r0.xyz;
  r0.xyz = float3(-0.0549999997, -0.0549999997, -0.0549999997) + r0.xyz;
  r0.xyz = r1.xyz * r0.xyz;
  r0.xyz = r2.xyz + r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.w = 0;
  o0.xyzw = r0.xyzw;
  return;
}
