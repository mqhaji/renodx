#include "../tonemap.hlsli"

// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:21 2026

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

cbuffer cPSMaterial : register(b4)
{

  struct
  {
    float4 m_materials[8];
  } g_psMaterial : packoffset(c0);

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

SamplerState g_samplerPoint_Clamp_s : register(s9);
SamplerState g_samplerLinear_Wrap_s : register(s10);
SamplerState g_samplerLinear_Clamp_s : register(s11);
SamplerState g_samplerAnisotropic_Wrap_s : register(s12);
SamplerComparisonState g_samplerComparisonLess_Linear_Clmap_s : register(s15);
Texture2D<float4> inGradientTexture : register(t1);
Texture2D<float4> inWhitecapTexture1 : register(t2);
Texture2D<float4> inFoamTexture : register(t3);
Texture2D<float4> inNormalDetailTexture : register(t4);
Texture2D<float4> inFoamTexture2 : register(t5);
Texture2D<float4> g_tex_fog : register(t12);
Texture2D<float4> inDepthTexture : register(t13);
Texture2D<float4> inShadowTexture : register(t14);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float4 v1 : TEXCOORD0,
  float4 v2 : TEXCOORD1,
  float4 v3 : TEXCOORD2,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r13,r14,r15,r16,r17,r18;
  uint4 bitmask, uiDest;
  float4 fDest;

  // Pixel position setup.
  r0.xy = float2(-0.5,-0.5) + v0.xy;

  // NGetShadow_getShadow: cascade selection, shadow-map comparison, and distance fade.
  r1.xyz = v3.xyz;
  r0.z = v1.z;
  r1.xyz = r1.xyz;
  r0.z = r0.z;
  r0.w = g_psLight.m_lightParams[8].x;
  r0.w = r0.w;
  r2.x = g_psLight.m_lightParams[8].y;
  r2.x = r2.x;
  r3.x = 0.5 / g_psScene.m_shadowMapResolutions.x;
  r3.y = 0.5 / g_psScene.m_shadowMapResolutions.x;
  r1.xyz = r1.xyz;
  r2.y = g_psScene.m_shadowProjection2._m30;
  r2.y = r2.y;
  r2.z = 1 / g_psScene.m_shadowMapResolutions.x;
  r2.z = 4 * r2.z;
  r2.z = -r2.z;
  r2.z = 1 + r2.z;
  r2.z = r2.z * r2.z;
  r2.z = 1 * r2.z;
  r1.w = 0;

  // CalcCascadeShadowUV: choose the active shadow cascade and build projected shadow UV.
  r4.x = g_psScene.m_shadowProjection2._m00;
  r4.y = g_psScene.m_shadowProjection2._m01;
  r4.z = g_psScene.m_shadowProjection2._m02;
  r4.w = g_psScene.m_shadowProjection2._m03;
  r4.xyzw = r4.xyzw;
  r5.x = g_psScene.m_shadowProjection2._m10;
  r5.y = g_psScene.m_shadowProjection2._m11;
  r5.z = g_psScene.m_shadowProjection2._m12;
  r5.w = g_psScene.m_shadowProjection2._m13;
  r5.xyzw = r5.xyzw;
  r6.x = g_psScene.m_shadowProjection2._m20;
  r6.y = g_psScene.m_shadowProjection2._m21;
  r6.z = g_psScene.m_shadowProjection2._m22;
  r6.w = g_psScene.m_shadowProjection2._m23;
  r6.xyzw = r6.xyzw;
  r7.xyz = r4.www * r1.xyz;
  r4.xyz = r7.xyz + r4.xyz;
  r4.w = 1;
  r7.xyz = r5.www * r1.xyz;
  r5.xyz = r7.xyz + r5.xyz;
  r5.w = 2;
  r7.xyz = r6.www * r1.xyz;
  r6.xyz = r7.xyz + r6.xyz;
  r6.w = 3;
  r2.w = 1 + r1.z;
  r1.z = r2.w * r2.y;
  r2.w = 1 + r4.z;
  r4.z = r2.w * r2.y;
  r2.w = 1 + r5.z;
  r5.z = r2.w * r2.y;
  r2.w = 1 + r6.z;
  r6.z = r2.w * r2.y;
  r2.yw = r1.xy * r1.xy;
  r2.yw = cmp(r2.zz >= r2.yw);
  r2.yw = r2.yw ? float2(1,1) : float2(0,0);
  r3.z = cmp(r1.z >= 0);
  r3.z = r3.z ? 1 : 0;
  r7.xy = r4.xy * r4.xy;
  r7.xy = cmp(r2.zz >= r7.xy);
  r7.xy = r7.xy ? float2(1,1) : float2(0,0);
  r3.w = cmp(r4.z >= 0);
  r3.w = r3.w ? 1 : 0;
  r7.zw = r5.xy * r5.xy;
  r7.zw = cmp(r2.zz >= r7.zw);
  r7.zw = r7.zw ? float2(1,1) : float2(0,0);
  r2.z = cmp(r5.z >= 0);
  r2.z = r2.z ? 1 : 0;
  r2.y = r2.y * r2.w;
  r8.xyzw = r2.yyyy * r3.zzzz;
  r2.y = r7.x * r7.y;
  r2.y = r2.y * r3.w;
  r2.w = r7.z * r7.w;
  r2.z = r2.w * r2.z;
  r2.w = -r8.w;
  r2.w = 1 + r2.w;
  r7.xyzw = r2.yyyy * r2.wwww;
  r2.y = -r7.w;
  r2.y = 1 + r2.y;
  r2.y = r2.w * r2.y;
  r9.xyzw = r2.zzzz * r2.yyyy;
  r2.z = -r9.w;
  r2.z = 1 + r2.z;
  r2.y = r2.y * r2.z;
  r10.xyzw = float4(1,1,1,1) * r2.yyyy;
  r1.xyzw = r8.xyzw * r1.xyzw;
  r4.xyzw = r7.xyzw * r4.xyzw;
  r1.xyzw = r4.xyzw + r1.xyzw;
  r4.xyzw = r9.xyzw * r5.xyzw;
  r1.xyzw = r4.xyzw + r1.xyzw;
  r4.xyzw = r10.xyzw * r6.xyzw;
  r1.xyzw = r4.wxyz + r1.wxyz;
  r1.w = max(9.99999997e-07, r1.w);
  r1.xyz = r1.xyz;
  r1.w = r1.w;
  r1.xyzw = r1.xyzw;
  r1.x = r1.x;
  r2.yz = float2(0.5,0.5) * r1.yz;
  r4.xy = float2(0.5,0.5) + r2.yz;
  r2.y = -r4.y;
  r4.z = 1 + r2.y;
  r2.yz = float2(0.5,0.5) * r4.xz;
  r3.zw = float2(2,2) * r3.xy;
  r3.zw = -r3.zw;
  r3.zw = float2(1,1) + r3.zw;
  r2.yz = min(r3.zw, r2.yz);
  r2.w = -1 + r1.x;
  r2.w = max(0, r2.w);
  r2.w = min(1, r2.w);
  r1.x = r1.x / 2;
  r3.z = -r2.w;
  r4.x = r3.z + r1.x;
  r4.y = 0.5 * r2.w;
  r2.yz = r4.xy + r2.yz;
  r1.yz = r2.yz + r3.xy;
  r1.yz = r1.yz;
  r1.w = r1.w;
  r1.yzw = r1.yzw;
  r1.xyz = r1.yzw / float3(1,1,1);

  // ShadowComparisonFiltered / TFetch2DProjCmp: sample comparison shadow texture.
  r1.x = inShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r1.xy, r1.z).x;
  r1.x = r1.x;
  r1.x = 1 * r1.x;
  r1.x = -r1.x;
  r1.x = 1 + r1.x;
  r1.x = r1.x * r1.x;
  r1.x = r1.x;
  r1.x = r1.x;
  r1.y = r2.x + r0.z;
  r0.w = r1.y * r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r1.y = -r1.x;
  r1.y = 1 + r1.y;
  r0.w = r1.y * r0.w;
  r0.w = r1.x + r0.w;
  r1.x = g_psLight.m_lightParams[8].w;
  r1.x = -r1.x;
  r0.w = r1.x + r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r0.w = r0.w;

  // NScreenToClipCoordinate + NFetchVolFog_fetchFog: screen position to clip space.
  r1.xyzw = r0.xyxy;
  r2.xyzw = r1.xyzw;
  r2.xyzw = r2.xyzw / g_psSystem.m_renderInfo.xyxy;
  r2.xyzw = float4(2,-2,2,-2) * r2.xyzw;
  r2.xyzw = float4(-1,1,-1,1) + r2.xyzw;
  r2.xyzw = r2.xyzw;
  r2.xyzw = r2.xyzw;
  r2.xyzw = r2.xyzw;
  r2.xyzw = r2.xyzw;
  r0.z = r0.z;
  r2.xyzw = r2.xyzw;
  r0.x = r0.z;
  r0.x = r0.x;
  r0.y = g_psScene.m_fogParam[1].x;
  r0.x = log2(r0.x);
  r0.x = r0.y * r0.x;
  r0.x = r0.x;

  // GetVolumetricFog2D: encode view Z, address the fog atlas layers, and interpolate.
  r0.x = max(0, r0.x);
  r0.x = min(1, r0.x);
  r0.x = 127 * r0.x;
  r2.xyzw = float4(0.0146484375,0.123046875,0.0146484375,0.123046875) * r2.xyzw;
  r2.xyzw = float4(0.015625,0.125,0.015625,0.125) + r2.xyzw;
  r0.y = 1 + r0.x;
  r0.y = max(0, r0.y);
  r3.w = min(127, r0.y);
  r3.y = r0.x;
  r1.zw = floor(r3.yw);
  r1.zw = r1.zw / float2(32,32);
  r3.xy = frac(r1.zw);
  r3.xz = float2(32,32) * r3.xy;
  r3.yw = floor(r1.zw);
  r3.xyzw = float4(0.03125,0.25,0.03125,0.25) * r3.xyzw;
  r2.xyzw = r3.xyzw + r2.xyzw;
  r3.xyzw = frac(r0.xxxx);
  r4.xyzw = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r2.xy).xyzw;
  r2.xyzw = g_tex_fog.Sample(g_samplerLinear_Clamp_s, r2.zw).xyzw;
  r5.xyzw = -r3.xyzw;
  r5.xyzw = float4(1,1,1,1) + r5.xyzw;
  r4.xyzw = r5.xyzw * r4.xyzw;
  r2.xyzw = r3.xyzw * r2.xyzw;
  r2.xyzw = r4.xyzw + r2.xyzw;
  r0.x = g_psScene.m_fogParam[1].y;
  r2.xyz = r2.xyz * r0.xxx;
  r2.xyz = r2.xyz;
  r2.w = r2.w;
  r2.xyzw = r2.xyzw;
  r2.xyzw = r2.xyzw;
  r2.xyzw = r2.xyzw;

  // NGetSunLightParam_sun: fetch sun/light direction for ocean lighting.
  r3.xyz = g_psObject.m_localParam[0].xzy;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r1.xy = r1.xy;
  r1.xy = r1.xy;

  // NScreenToTextureCoordinate + NGetPrimitiveDepthFactor_fetchViewZ: sample scene depth and reconstruct view Z.
  r0.xy = float2(0.49609375,0.49609375) + r1.xy;
  r0.xy = g_psSystem.m_renderBuffer.zw * r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r1.xy = g_psScene.m_projectionParam.zw;
  r1.z = inDepthTexture.Sample(g_samplerPoint_Clamp_s, r0.xy).x;
  r1.z = r1.z;
  r1.z = r1.z;
  r1.xy = r1.xy;
  r1.y = -r1.y;
  r1.z = r1.z + r1.y;
  r1.z = r1.x / r1.z;
  r1.z = r1.z;
  r1.z = r1.z;
  r1.z = r1.z;

  // NCalculate_computePrimitiveColor: main ocean shading.
  r4.xy = v1.xy;
  r5.xyz = v2.xyz;
  r1.w = v2.w;
  r1.z = r1.z;
  r3.xyz = r3.xyz;
  r2.xyzw = r2.xyzw;
  r0.w = r0.w;
  r4.xy = r4.xy;
  r5.xyz = r5.xyz;
  r1.w = r1.w;
  r1.z = r1.z;
  r3.xyz = r3.xyz;
  r2.xyzw = r2.xyzw;
  r0.w = r0.w;

  // Ocean material parameters, sun color, and view direction.
  r6.w = 78.125;
  r4.zw = g_psMaterial.m_materials[0].xy;
  r3.w = g_psMaterial.m_materials[0].w;
  r5.w = g_psMaterial.m_materials[1].z;
  r7.xy = float2(0.620000005,0.889999986) * g_psMaterial.m_materials[2].yy;
  r8.xyz = g_psMaterial.m_materials[3].xyz;
  r9.xyz = g_psMaterial.m_materials[4].xyz;
  r7.z = g_psMaterial.m_materials[4].w;
  r7.w = g_psMaterial.m_materials[5].x;
  r8.w = g_psMaterial.m_materials[5].y;
  r10.x = g_psMaterial.m_materials[5].z;
  r9.w = g_psMaterial.m_materials[5].w;
  r10.w = g_psMaterial.m_materials[6].x;
  r10.y = g_psMaterial.m_materials[6].y;
  r10.z = g_psMaterial.m_materials[6].z;
  r11.x = g_psMaterial.m_materials[6].w;
  r11.yzw = g_psMaterial.m_materials[7].xyz * g_psScene.m_exposure.zzz;
  r12.xyz = g_psScene.m_eyepos.xyz;
  r12.xyz = -r12.xyz;
  r5.xyz = r12.xyz + r5.xyz;
  r12.x = dot(r5.xyz, r5.xyz);
  r12.x = rsqrt(r12.x);
  r5.xyz = r12.xxx * r5.xyz;

  // Patch blend and far-range blend controls.
  r7.w = -r7.w;
  r7.w = r7.w + r0.z;
  r7.w = r7.w / r8.w;
  r7.w = -r7.w;
  r7.w = 1 + r7.w;
  r7.w = max(0, r7.w);
  r7.w = min(1, r7.w);
  r8.w = 0.649999976 * r1.w;
  r8.w = 0.349999994 + r8.w;
  r7.w = r8.w * r7.w;
  r8.w = r0.z / 5000;
  r8.w = -r8.w;
  r8.w = 1 + r8.w;

  // Gradient sampling: wave gradient, specular gradient, blurred gradient, and base normal.
  r12.xy = r7.xx * r4.xy;
  r7.xy = r7.yy * r4.yx;
  r12.zw = inGradientTexture.Sample(g_samplerLinear_Wrap_s, r12.xy).xy;
  r12.zw = r12.zw;
  r7.xy = inGradientTexture.Sample(g_samplerLinear_Wrap_s, r7.xy).xy;
  r7.xy = r7.xy;
  r12.zw = float2(0.649999976,0.649999976) * r12.zw;
  r7.xy = float2(0.349999994,0.349999994) * r7.xy;
  r7.xy = r12.zw + r7.xy;
  r13.xyzw = inGradientTexture.Sample(g_samplerLinear_Wrap_s, v1.xy).xyzw;
  r12.zw = g_psMaterial.m_materials[2].zz * r7.xy;
  r14.xy = -r12.zw;
  r14.xy = r14.xy + r13.xy;
  r14.xy = r14.xy * r7.ww;
  r14.xz = r14.xy + r12.zw;
  r7.xy = float2(0.100000001,0.100000001) * r7.xy;
  r15.xy = r7.xy * r8.ww;
  r15.z = 0;
  r16.xyz = -r15.xyz;
  r13.xyz = r16.xyz + r13.xyz;
  r13.xyz = r13.xyz * r7.www;
  r6.xyz = r15.xyz + r13.xyz;
  r7.xy = float2(0.015625,0.015625) + r4.xy;
  r7.x = inGradientTexture.Sample(g_samplerLinear_Wrap_s, r7.xy).z;
  r7.x = r7.x + r6.z;
  r7.x = r7.x / 2;
  r7.x = r7.x * r7.w;
  r7.y = dot(r6.xyw, r6.xyw);
  r7.y = rsqrt(r7.y);
  r13.xyz = r7.yyy * r6.xwy;
  r14.y = 78.125;
  r6.w = dot(r14.xyz, r14.xyz);
  r6.w = rsqrt(r6.w);
  r14.xyz = r14.xyz * r6.www;

  // DecodeNormalTexture: far/detail normal from anisotropic normal texture.
  r7.yw = r12.xy + r4.zw;
  r7.yw = float2(0.400000006,0.400000006) * r7.yw;
  r7.yw = inNormalDetailTexture.Sample(g_samplerAnisotropic_Wrap_s, r7.yw).yw;
  r7.yw = r7.yw;
  r7.yw = float2(2,2) * r7.yw;
  r12.xz = float2(-1,-1) + r7.yw;
  r6.w = r12.z * r12.z;
  r6.w = -r6.w;
  r6.w = 1 + r6.w;
  r7.y = r12.x * r12.x;
  r7.y = -r7.y;
  r6.w = r7.y + r6.w;
  r6.w = max(0, r6.w);
  r6.w = min(1, r6.w);
  r6.w = 9.99999975e-05 + r6.w;
  r7.y = rsqrt(r6.w);
  r12.y = r7.y * r6.w;
  r12.xz = r12.xz;
  r12.y = r12.y;
  r12.xyz = r12.xyz;
  r6.w = 0.649999976 * r8.w;
  r6.w = max(0, r6.w);
  r6.w = min(1, r6.w);
  r15.xyz = -r12.xyz;
  r14.xyz = r15.xyz + r14.xyz;
  r14.xyz = r14.xyz * r6.www;
  r12.xyz = r14.xyz + r12.xyz;

  // GetNormalFromTexture: secondary perturbed normal and blend with gradient normal.
  r7.yw = r4.xy + r4.zw;
  r7.yw = float2(4,4) * r7.yw;
  r7.yw = inNormalDetailTexture.Sample(g_samplerLinear_Wrap_s, r7.yw).yw;
  r7.yw = r7.yw;
  r7.yw = float2(2,2) * r7.yw;
  r14.xz = float2(-1,-1) + r7.yw;
  r6.w = r14.z * r14.z;
  r6.w = -r6.w;
  r6.w = 1 + r6.w;
  r7.y = r14.x * r14.x;
  r7.y = -r7.y;
  r6.w = r7.y + r6.w;
  r6.w = max(0, r6.w);
  r6.w = min(1, r6.w);
  r6.w = 9.99999975e-05 + r6.w;
  r7.y = rsqrt(r6.w);
  r14.y = r7.y * r6.w;
  r14.xz = r14.xz;
  r14.y = r14.y;
  r14.xyz = r14.xyz;
  r14.xyz = r14.xyz;
  r13.xyz = float3(0.980000019,0.980000019,0.980000019) * r13.xyz;
  r14.xyz = float3(0.0199999996,0.0199999996,0.0199999996) * r14.xyz;
  r13.xyz = r14.xyz + r13.xyz;

  // Fresnel, reflection vector, foam noise, and sun specular response.
  r6.w = -100 + r0.z;
  r7.y = r6.w / 100;
  r7.y = max(0, r7.y);
  r7.y = min(1, r7.y);
  r14.xyz = -r5.xyz;
  r7.w = dot(r13.xyz, r14.xyz);
  r12.w = dot(r5.xyz, r12.xyz);
  r12.w = r12.w + r12.w;
  r12.w = -r12.w;
  r15.xyz = r12.xyz * r12.www;
  r15.xyz = r15.xyz + r5.xyz;
  r16.xy = float2(0.200000003,0.200000003) * r4.xy;
  r16.xy = r16.xy + r4.zw;
  r16.xyz = inFoamTexture.Sample(g_samplerAnisotropic_Wrap_s, r16.xy).xyz;
  r16.xyz = r16.xyz;
  r12.w = -r7.w;
  r7.w = max(r12.w, r7.w);
  r7.w = r8.w * r7.w;
  r7.w = -r7.w;
  r7.w = 1.00010002 + r7.w;
  r12.w = 8 * r8.w;
  r7.w = log2(r7.w);
  r7.w = r12.w * r7.w;
  r7.w = exp2(r7.w);
  r7.w = 0.949999988 * r7.w;
  r7.w = 0.0500000007 + r7.w;
  r7.w = max(0, r7.w);
  r17.xyz = min(float3(1,1,1), r7.www);
  r7.w = dot(r3.xzy, r15.xyz);
  r7.w = max(0, r7.w);
  r7.w = min(1, r7.w);
  r12.w = 2 * r10.x;
  r10.xyz = r10.xyz;
  r7.w = log2(r7.w);
  r10.x = r10.x * r7.w;
  r15.x = exp2(r10.x);
  r7.w = r12.w * r7.w;
  r15.y = exp2(r7.w);
  r7.w = dot(r10.yz, r15.xy);
  r10.x = r6.w / 500;
  r10.x = max(0, r10.x);
  r10.x = min(1, r10.x);
  r10.y = -r11.x;
  r10.y = max(r11.x, r10.y);
  r10.y = 1000 * r10.y;
  r10.y = -r10.y;
  r10.y = 1 + r10.y;
  r10.y = max(0.5, r10.y);
  r10.z = 0.5 * r16.z;
  r10.x = r10.z * r10.x;
  r10.x = r10.x * r10.y;
  r7.w = r10.x * r7.w;
  r10.x = r0.z / 2500;
  r10.x = -r10.x;
  r10.x = 1 + r10.x;
  r10.x = max(0, r10.x);
  r10.x = min(1, r10.x);
  r10.y = 0.200000003 * r10.x;
  r15.xyz = -r8.xyz;
  r15.xyz = r15.xyz + r3.xzy;
  r15.xyz = r15.xyz * r10.yyy;
  r15.xyz = r15.xyz + r8.xyz;
  r8.x = dot(r15.xyz, r15.xyz);
  r8.x = rsqrt(r8.x);
  r15.xyz = r15.xyz * r8.xxx;
  r8.x = 0.150000006 * r8.w;
  r8.x = 0.0250000004 + r8.x;
  r12.xyz = float3(-0,-1,-0) + r12.xyz;
  r8.xzw = r12.xyz * r8.xxx;
  r8.xzw = float3(0,1,0) + r8.xzw;
  r10.y = dot(r8.xzw, r8.xzw);
  r10.y = rsqrt(r10.y);
  r8.xzw = r10.yyy * r8.xzw;
  r10.y = dot(r5.xyz, r8.xzw);
  r10.y = r10.y + r10.y;
  r10.y = -r10.y;
  r8.xzw = r10.yyy * r8.xzw;
  r8.xzw = r8.xzw + r5.xyz;
  r8.z = dot(r15.xyz, r8.xzw);
  r8.z = max(0, r8.z);
  r8.z = min(1, r8.z);
  r10.y = r8.z * r8.z;
  r10.z = 1 * r10.y;
  r10.y = r10.y * r10.y;
  r10.y = r10.z * r10.y;
  r10.y = 1.27323997 * r10.y;
  r10.y = r10.y * r10.w;
  r7.w = r10.y + r7.w;
  r8.z = log2(r8.z);
  r8.z = 5000 * r8.z;
  r8.z = exp2(r8.z);
  r8.z = 796.093323 * r8.z;
  r8.z = r8.z * r10.w;
  r8.z = 0.25 * r8.z;
  r7.w = r8.z + r7.w;
  r10.yzw = r11.yzw * r7.www;
  r7.w = dot(r10.yzw, float3(0.333000004,0.333000004,0.333000004));
  r7.w = r7.w * r7.w;
  r7.w = max(0, r7.w);
  r7.w = min(1, r7.w);
  r10.yzw = r10.yzw * r7.www;
  r12.xyz = r17.zzz;
  r10.yzw = r12.xyz * r10.yzw;
  r10.yzw = r10.yzw * r9.www;

  // Specular contribution shaping: shadow influence and day/night transition.
  r12.xyz = float3(0.0500000007,0.0500000007,0.0500000007) + r0.www;
  r10.yzw = r12.xyz * r10.yzw;
  r7.w = -0.0399999991 + r8.y;
  r7.w = 14 * r7.w;
  r7.w = 1 + r7.w;
  r7.w = max(0, r7.w);
  r7.w = min(1, r7.w);
  r8.y = 0.239999995 + r8.y;
  r8.y = 14 * r8.y;
  r8.y = -r8.y;
  r8.y = 1 + r8.y;
  r8.y = max(0, r8.y);
  r8.y = min(1, r8.y);
  r8.y = 0.5 * r8.y;
  r7.w = r8.y + r7.w;
  r7.w = r7.w * r7.w;
  r10.yzw = r10.yzw * r7.www;

  // Ambient and sky-reflection color from exposure, sun factor, and water depth.
  r7.w = 0.5 * r16.x;
  r7.w = max(0, r7.w);
  r7.w = min(1, r7.w);
  r12.xyz = float3(-0.075000003,-0.0900000036,-0.0599999987) * r7.www;
  r12.xyz = float3(0.135000005,0.194999993,0.254999995) + r12.xyz;
  r7.w = 9.99999975e-05 / g_psScene.m_exposure.z;
  r7.w = max(0, r7.w);
  r7.w = min(1, r7.w);
  r7.w = max(0.349999994, r7.w);
  r12.xyz = r12.xyz * r7.www;
  r15.xz = r5.xz;
  r15.y = r3.z;
  r5.x = dot(r15.xyz, r15.xyz);
  r5.x = rsqrt(r5.x);
  r15.xyz = r15.xyz * r5.xxx;
  r5.x = dot(r15.xzy, r3.xyz);
  r5.x = max(0, r5.x);
  r15.xyz = min(float3(1,1,1), r5.xxx);
  r5.x = dot(r11.yzw, r11.yzw);
  r5.x = rsqrt(r5.x);
  r18.xyz = r11.yzw * r5.xxx;
  r18.xyz = float3(-0.349999994,-0.349999994,-0.349999994) + r18.xyz;
  r15.xyz = r18.xyz * r15.xyz;
  r15.xyz = float3(0.349999994,0.349999994,0.349999994) + r15.xyz;
  r12.xyz = r15.xyz * r12.xyz;
  r12.xyz = float3(0.0960000008,0.200000003,0.379999995) * r12.xyz;
  r18.xyz = float3(-0.075000003,0.0250000004,0.0250000004) * r7.yyy;
  r18.xyz = float3(0.135000005,0.194999993,0.254999995) + r18.xyz;
  r5.xz = r4.xy / float2(10,10);
  r8.yz = r13.xz * r0.zz;
  r8.yz = r8.yz / float2(75,75);
  r5.xz = r8.yz + r5.xz;
  r5.x = inWhitecapTexture1.Sample(g_samplerLinear_Wrap_s, r5.xz).z;
  r5.x = r5.x * r10.x;
  r5.x = r5.x * r5.x;
  r5.x = 400 * r5.x;
  r5.x = max(0, r5.x);
  r5.x = min(1, r5.x);
  r18.xyz = float3(-0.0599999987,-0.104999997,-0.194999993) + r18.xyz;
  r18.xyz = r18.xyz * r5.xxx;
  r18.xyz = float3(0.0599999987,0.104999997,0.194999993) + r18.xyz;
  r18.xyz = r18.xyz * r7.www;
  r15.xyz = r18.xyz * r15.xyz;
  r5.x = dot(float3(0,1,0), r14.xyz);
  r5.x = 5 * r5.x;
  r5.x = 3 + r5.x;

  // Water distortion: bend screen UV, fetch distorted depth, and derive blend factors.
  r5.z = 2 * r0.z;
  r3.w = r5.z / r3.w;
  r3.w = r3.w / 600;
  r3.w = max(0, r3.w);
  r3.w = min(1, r3.w);
  r3.w = -r3.w;
  r3.w = 1 + r3.w;
  r3.w = 0.949999988 * r3.w;
  r8.yz = float2(0.0500000007,0.0500000007) + r3.ww;
  r3.w = 0.0399999991 * r10.x;
  r3.w = -r3.w;
  r3.w = 0.0405000001 + r3.w;
  r6.xy = r6.xy * r3.ww;
  r6.xy = float2(0,0.0399999991) + r6.xy;
  r6.xy = r8.yz * r6.xy;
  r6.xy = -r6.xy;
  r0.xy = r6.xy + r0.xy;
  r0.xy = max(float2(0,0), r0.xy);
  r0.xy = min(float2(1,1), r0.xy);
  r0.xy = r0.xy;
  r0.x = inDepthTexture.Sample(g_samplerPoint_Clamp_s, r0.xy).x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x + r1.y;
  r0.x = r1.x / r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.x = r0.x;
  r0.y = cmp(r0.x < r0.z);
  r1.x = -r0.x;
  r1.x = r1.x + r0.z;
  r1.y = r1.x / 50;
  r1.y = max(0, r1.y);
  r1.y = min(1, r1.y);
  r1.y = -r1.y;
  r1.y = 1 + r1.y;
  r3.w = -r0.z;
  r0.x = r3.w + r0.x;
  r5.y = 650 * r5.y;
  r5.y = -r5.y;
  r5.y = 700 + r5.y;
  r0.x = r0.x / r5.y;
  r0.x = max(0, r0.x);
  r0.x = min(1, r0.x);
  r0.x = -r0.x;
  r0.x = 1 + r0.x;
  r0.x = r0.y ? r1.y : r0.x;
  r0.y = r0.z / 250;
  r0.y = max(0, r0.y);
  r0.y = min(1, r0.y);
  r0.x = r0.x * r0.y;
  r0.y = -r1.x;
  r0.y = max(r1.x, r0.y);
  r0.y = r0.y / 10;
  r0.y = -r0.y;
  r0.y = 1 + r0.y;
  r0.y = max(0, r0.y);
  r0.y = min(1, r0.y);
  r0.z = -50 + r0.z;
  r0.z = r0.z / 50;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.y = r0.y * r0.z;

  // Foam: sample foam mask and combine far, close, crest, and pillar foam.
  r0.z = -r7.z;
  r0.z = 1 + r0.z;
  r0.z = max(0, r0.z);
  r0.z = min(1, r0.z);
  r0.z = 10 * r0.z;
  r0.z = 1 + r0.z;
  r1.xy = r5.ww * r4.zw;
  r1.xy = float2(2,2) * r1.xy;
  r1.xy = r4.xy + r1.xy;
  r4.xyz = inFoamTexture2.Sample(g_samplerLinear_Wrap_s, r1.xy).xyz;
  r4.xyz = r4.xyz;
  r1.x = r6.w / 1500;
  r1.x = max(0, r1.x);
  r1.x = min(1, r1.x);
  r1.y = r7.z + r1.x;
  r1.y = 0.100000001 + r1.y;
  r1.y = max(0, r1.y);
  r1.y = min(1, r1.y);
  r4.w = -1 + r16.y;
  r1.y = r4.w * r1.y;
  r1.y = 1 + r1.y;
  r4.w = r13.w;
  r5.y = r6.z * r6.z;
  r5.y = r5.y * r5.y;
  r5.z = r1.y * r16.x;
  r5.z = r5.z * r4.y;
  r5.z = r5.z * r4.w;
  r5.z = 2 * r5.z;
  r1.x = r5.z * r1.x;
  r1.x = -r1.x;
  r1.x = r5.z + r1.x;
  r5.z = r13.w * r13.w;
  r5.w = r5.z * r5.z;
  r6.x = r5.y * r4.z;
  r6.x = r6.x * r16.y;
  r1.w = -r1.w;
  r1.w = 1 + r1.w;
  r1.w = max(0, r1.w);
  r1.w = min(1, r1.w);
  r1.w = 0.5 * r1.w;
  r1.w = 0.125 + r1.w;
  r1.w = r6.x * r1.w;
  r4.z = r5.w * r4.z;
  r5.z = r5.z * r4.y;
  r4.z = r5.z + r4.z;
  r4.x = r4.x * r4.w;
  r4.x = r4.z + r4.x;
  r4.x = r4.x * r16.y;
  r1.y = r4.x * r1.y;
  r1.y = r1.y + r1.w;
  r1.w = cmp(r1.y < 0.00499999989);
  r4.x = log2(r1.y);
  r4.x = 1.5 * r4.x;
  r4.x = exp2(r4.x);
  r1.y = r1.w ? r4.x : r1.y;
  r1.w = -r1.y;
  r1.x = r1.x + r1.w;
  r1.x = r7.y * r1.x;
  r1.x = r1.y + r1.x;
  r0.z = r1.x * r0.z;
  r1.x = r4.y * r0.y;
  r0.z = r1.x + r0.z;
  r0.z = 0.5 * r0.z;
  r1.xyw = float3(0.150000006,0.150000006,0.150000006) * r11.yzw;
  r1.xyw = r1.xyw + r12.xyz;
  r1.xyw = float3(0.0500000007,0.0500000007,0.0500000007) + r1.xyw;

  // Subsurface scattering contribution and shadow shaping.
  r3.z = dot(r13.xzy, r3.xyz);
  r4.x = -r3.z;
  r3.z = max(r4.x, r3.z);
  r3.x = dot(r3.xy, r8.xw);
  r3.y = -r3.x;
  r3.x = max(r3.x, r3.y);
  r3.x = r3.z + r3.x;
  r3.x = 0.5 * r3.x;
  r0.y = 2 * r0.y;
  r0.y = r3.x + r0.y;
  r3.x = 0.25 * r5.y;
  r3.x = r3.x + r7.x;
  r0.y = r3.x * r0.y;
  r3.xyz = r0.yyy * r11.yzw;
  r3.xyz = float3(0.00875000004,0.0315000005,0.0157500003) * r3.xyz;
  r0.y = 0.25 * r0.w;
  r0.y = 0.75 + r0.y;

  // Final ocean color composite: specular, reflection, foam, SSS, shadow, and volumetric fog.
  r0.w = 12 * r0.z;
  r0.w = -r0.w;
  r0.w = 1 + r0.w;
  r0.w = max(0, r0.w);
  r0.w = min(1, r0.w);
  r4.xyz = r10.yzw * r0.www;
  r5.yzw = float3(1.85000002,1.85000002,1.85000002) * r12.xyz;
  r6.xyz = r15.xyz * r5.xxx;
  r7.xyz = -r5.yzw;
  r6.xyz = r7.xyz + r6.xyz;
  r6.xyz = r17.xyz * r6.xyz;
  r5.xyz = r6.xyz + r5.yzw;
  r0.x = -0.5 + r0.x;
  r0.x = max(0, r0.x);
  r0.x = min(1, r0.x);
  r6.xyz = -r5.xyz;
  r6.xyz = float3(0,0,0) + r6.xyz;
  r6.xyz = r6.xyz * r0.xxx;
  r5.xyz = r6.xyz + r5.xyz;
  r3.xyz = r5.xyz + r3.xyz;
  r0.xzw = r1.xyw * r0.zzz;
  r0.xzw = r3.xyz + r0.xzw;
  r0.xyz = r0.xzw * r0.yyy;
  r0.xyz = r4.xyz + r0.xyz;
  r0.xyz = r0.xyz * r2.www;
  r0.xyz = r0.xyz + r2.xyz;

  // TppTonemap.
  r9.xyz = r9.xyz;
  r0.xyz = r0.xyz;

  float3 untonemapped = r0.rgb;

  r1.xyw = r9.yyy;
  r2.xyz = cmp(r1.xyw >= r0.xyz);
  r2.xyz = r2.xyz ? float3(1,1,1) : float3(0,0,0);
  r3.xyz = r2.xyz * r0.xyz;
  r2.xyz = -r2.xyz;
  r2.xyz = float3(1,1,1) + r2.xyz;
  r0.xyz = r0.xyz + r9.zzz;
  r4.xyz = -r1.yyy;
  r0.xyz = r4.xyz + r0.xyz;
  r0.xyz = r9.xxx * r0.xyz;
  r0.xyz = float3(-1,-1,-1) / r0.xyz;
  r0.xyz = r0.xyz + r9.zzz;
  r0.xyz = r0.xyz + r1.yyy;
  r0.xyz = r2.xyz * r0.xyz;
  r0.xyz = r3.xyz + r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;

  if (RENODX_TONE_MAP_TYPE != 0.f) {
    r0.rgb = untonemapped;
  }

  // GammaCorrection.
  r1.xyw = cmp(float3(0.00313080009,0.00313080009,0.00313080009) >= r0.xyz);
  r1.xyw = r1.xyw ? float3(1,1,1) : float3(0,0,0);
  r2.xyz = float3(12.9200001,12.9200001,12.9200001) * r0.xyz;
  r2.xyz = r2.xyz * r1.xyw;
  r1.xyw = -r1.xyw;
  r1.xyw = float3(1,1,1) + r1.xyw;
  r0.xyz = max(float3(9.99999975e-06,9.99999975e-06,9.99999975e-06), r0.xyz);
  r0.xyz = log2(r0.xyz);
  r0.xyz = float3(0.416666657,0.416666657,0.416666657) * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  r0.xyz = float3(1.05499995,1.05499995,1.05499995) * r0.xyz;
  r0.xyz = float3(-0.0549999997,-0.0549999997,-0.0549999997) + r0.xyz;
  r0.xyz = r1.xyw * r0.xyz;
  r0.xyz = r2.xyz + r0.xyz;
  r0.xyz = r0.xyz;

  // Output alpha and duplicate color to both render targets.
  r0.w = r3.w + r1.z;
  r0.w = r0.w / 2;
  r0.w = max(0, r0.w);
  r1.w = min(1, r0.w);
  r1.xyz = r1.www * r0.xyz;
  r1.xyz = r1.xyz;
  r1.w = r1.w;
  o0.xyzw = r1.xyzw;
  o1.xyzw = r1.xyzw;
  return;
}
