#include "../material_capture.hlsli"

// clang-format off
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:20 2026

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

SamplerState g_sampler_diffuse_s : register(s0);
SamplerState g_sampler_normal_s : register(s1);
SamplerState g_sampler_srm_s : register(s2);
SamplerState g_sampler_subNormal_s : register(s4);
SamplerState g_sampler_subNormalMask_s : register(s5);
SamplerState g_samplerPoint_Wrap_s : register(s8);
SamplerState g_samplerLinear_Wrap_s : register(s10);
Texture2D<float4> g_tex_diffuse : register(t0);
Texture2D<float4> g_tex_normal : register(t1);
Texture2D<float4> g_tex_srm : register(t2);
Texture2D<float4> g_tex_materialmap : register(t3);
Texture2D<float4> g_tex_subNormal : register(t4);
Texture2D<float4> g_tex_subNormalMask : register(t5);
Texture2D<float4> g_MaterialTexture : register(t10);
TextureCube<float4> g_ReflectionTexture : register(t11);
Texture2D<float4> g_tex_mesh : register(t15);


// 3Dmigoto declarations
#define cmp -


void NativeMaterialMain(
  float4 v0 : SV_Position0,
  float4 v1 : COLOR0,
  float4 v2 : TEXCOORD0,
  float4 v3 : TEXCOORD4,
  float4 v4 : TEXCOORD5,
  float4 v5 : TEXCOORD6,
  float3 v6 : TEXCOORD7,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1,
  out float4 o2 : SV_Target2)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v0.xy;
  r0.zw = v2.xy;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r1.x = g_psMaterial.m_materials[0].x;
  r1.y = g_tex_materialmap.Sample(g_samplerPoint_Wrap_s, v2.xy).x;
  r1.y = r1.y;
  r1.y = cmp(r1.y >= 0.25);
  r1.y = r1.y ? 1 : 0;
  r1.z = -r1.y;
  r1.z = 1 + r1.z;
  r1.x = r1.z * r1.x;
  r1.z = g_psMaterial.m_materials[0].y;
  r1.y = r1.y * r1.z;
  r1.x = r1.x + r1.y;
  r1.x = r1.x;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r1.yz = g_psMaterial.m_materials[1].yz;
  r1.w = g_tex_subNormalMask.Sample(g_sampler_subNormalMask_s, v2.xy).x;
  r2.xy = r1.ww;
  r2.zw = g_psMaterial.m_materials[1].xx;
  r3.xyz = v4.xyz;
  r4.xyz = v5.xyz;
  r5.xyz = v6.xyz;
  r5.xyz = float3(1,1,1) * r5.xyz;
  r6.xy = g_tex_normal.Sample(g_sampler_normal_s, v2.xy).wy;
  r6.xy = r6.xy;
  r6.xy = float2(2,2) * r6.xy;
  r6.xy = float2(-1,-1) + r6.xy;
  r1.w = r6.x * r6.x;
  r1.w = -r1.w;
  r1.w = 1 + r1.w;
  r3.w = r6.y * r6.y;
  r3.w = -r3.w;
  r1.w = r3.w + r1.w;
  r1.w = max(0, r1.w);
  r1.w = min(1, r1.w);
  r1.w = 9.99999975e-05 + r1.w;
  r3.w = rsqrt(r1.w);
  r6.z = r3.w * r1.w;
  r6.xy = r6.xy;
  r6.z = r6.z;
  r6.xyz = r6.xyz;
  r6.xyz = r6.xyz;
  r0.zw = r1.yz * r0.zw;
  r0.zw = g_tex_subNormal.Sample(g_sampler_subNormal_s, r0.zw).wy;
  r0.zw = r0.zw;
  r0.zw = float2(2,2) * r0.zw;
  r0.zw = float2(-1,-1) + r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw;
  r0.zw = r0.zw * r2.xy;
  r0.zw = r0.zw * r2.zw;
  r6.xy = r6.xy + r0.zw;
  r0.z = dot(r6.xyz, r6.xyz);
  r0.z = rsqrt(r0.z);
  r1.yzw = r6.xyz * r0.zzz;
  r0.z = dot(r3.xyz, r3.xyz);
  r0.z = rsqrt(r0.z);
  r2.xyz = r3.xyz * r0.zzz;
  r3.xyz = r1.yyy;
  r2.xyz = r3.xyz * r2.xyz;
  r0.z = dot(r4.xyz, r4.xyz);
  r0.z = rsqrt(r0.z);
  r3.xyz = r4.xyz * r0.zzz;
  r4.xyz = r1.zzz;
  r3.xyz = r4.xyz * r3.xyz;
  r2.xyz = r3.xyz + r2.xyz;
  r0.z = dot(r5.xyz, r5.xyz);
  r0.z = rsqrt(r0.z);
  r3.xyz = r5.xyz * r0.zzz;
  r1.yzw = r1.www;
  r1.yzw = r3.xyz * r1.yzw;
  r1.yzw = r2.xyz + r1.yzw;
  r0.z = dot(r1.yzw, r1.yzw);
  r0.z = rsqrt(r0.z);
  r1.yzw = r1.yzw * r0.zzz;
  r1.yzw = r1.yzw;
  r1.yzw = r1.yzw;
  r1.yzw = r1.yzw;
  r2.xyz = g_tex_srm.Sample(g_sampler_srm_s, v2.xy).yxz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r2.y = r2.y;
  r2.y = r2.y;
  r2.y = r2.y;
  r2.y = r2.y;
  r2.y = r2.y;
  r2.y = r2.y;
  r2.y = r2.y;
  r2.y = r2.y;
  r3.xyzw = g_tex_diffuse.Sample(g_sampler_diffuse_s, v2.xy).xyzw;
  r3.xyzw = r3.xyzw;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r1.yzw = r1.yzw;
  r4.xyz = v3.xyz;
  r2.y = r2.y;
  r1.x = r1.x;
  r5.w = r2.z;
  r3.xyz = r3.xyz;
  r1.yzw = r1.yzw;
  r4.xyz = r4.xyz;
  r6.x = r2.y;
  r2.z = r1.x;
  r5.w = r5.w;
  r7.x = r2.z;
  r7.y = 0.25;
  r8.xyz = g_MaterialTexture.SampleLevel(g_samplerPoint_Wrap_s, r7.xy, 0).xyz;
  r8.xyz = r8.xyz;
  r7.z = 0.75;
  r0.zw = g_MaterialTexture.SampleLevel(g_samplerPoint_Wrap_s, r7.xz, 0).xz;
  r0.zw = r0.zw;
  r1.x = r5.w;
  r4.w = dot(r4.xyz, r4.xyz);
  r4.w = rsqrt(r4.w);
  r4.xyz = r4.xyz * r4.www;
  r4.xyz = -r4.xyz;
  r4.xyz = r4.xyz;
  r1.yzw = r1.yzw;
  r1.x = r1.x;
  r4.w = dot(r4.xyz, r1.yzw);
  r4.w = r4.w + r4.w;
  r4.w = -r4.w;
  r7.xyz = r4.www * r1.yzw;
  r4.xyz = r7.xyz + r4.xyz;
  r7.xyz = g_psScene.m_view._m00_m10_m20 * r4.xxx;
  r4.xyw = g_psScene.m_view._m01_m11_m21 * r4.yyy;
  r4.xyw = r7.xyz + r4.xyw;
  r7.xyz = g_psScene.m_view._m02_m12_m22 * r4.zzz;
  r4.xyz = r7.xyz + r4.xyw;
  r4.xyz = float3(0,0,0) + r4.xyz;
  r4.xyz = r4.xyz;
  r4.w = -r1.x;
  r4.w = 1 + r4.w;
  r6.z = 5 * r4.w;
  r6.z = 0 + r6.z;
  r4.xyz = SampleMaterialCubeLevel(g_ReflectionTexture, g_samplerLinear_Wrap_s, r4.xyz, r6.z).xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r6.z = r0.w;
  r4.w = r6.z * r4.w;
  r1.x = r4.w + r1.x;
  r0.z = r1.x * r0.z;
  r4.xyz = r4.xyz;
  r7.xy = r1.xx * r0.ww;
  r0.w = r6.x;
  r1.x = -r0.w;
  r1.x = 1 + r1.x;
  r1.x = r6.z * r1.x;
  r6.y = r1.x + r0.w;
  r6.xy = r7.xy * r6.xy;
  r3.xyz = r3.xyz;
  r7.xyz = cmp(float3(0.0392800011,0.0392800011,0.0392800011) >= r3.xyz);
  r7.xyz = r7.xyz ? float3(1,1,1) : float3(0,0,0);
  r9.xyz = r3.xyz / float3(12.9200001,12.9200001,12.9200001);
  r9.xyz = r9.xyz * r7.xyz;
  r7.xyz = -r7.xyz;
  r7.xyz = float3(1,1,1) + r7.xyz;
  r10.xyz = float3(0.0549999997,0.0549999997,0.0549999997) + r3.xyz;
  r10.xyz = r10.xyz / float3(1.05499995,1.05499995,1.05499995);
  r10.xyz = max(float3(9.99999975e-06,9.99999975e-06,9.99999975e-06), r10.xyz);
  r10.xyz = log2(r10.xyz);
  r10.xyz = float3(2.4000001,2.4000001,2.4000001) * r10.xyz;
  r10.xyz = exp2(r10.xyz);
  r7.xyz = r10.xyz * r7.xyz;
  r7.xyz = r9.xyz + r7.xyz;
  r7.xyz = r7.xyz;
  r6.yzw = -r6.yyy;
  r6.yzw = float3(1,1,1) + r6.yzw;
  r6.yzw = r7.xyz * r6.yzw;
  r4.xyz = r6.xxx * r4.xyz;
  r4.xyz = r4.xyz * r0.zzz;
  r4.xyz = r4.xyz * r8.xyz;
  r4.xyz = r6.yzw + r4.xyz;
  r4.xyz = r4.xyz;
  r6.xyz = cmp(float3(0.00313080009,0.00313080009,0.00313080009) >= r4.xyz);
  r6.xyz = r6.xyz ? float3(1,1,1) : float3(0,0,0);
  r7.xyz = float3(12.9200001,12.9200001,12.9200001) * r4.xyz;
  r7.xyz = r7.xyz * r6.xyz;
  r6.xyz = -r6.xyz;
  r6.xyz = float3(1,1,1) + r6.xyz;
  r4.xyz = max(float3(9.99999975e-06,9.99999975e-06,9.99999975e-06), r4.xyz);
  r4.xyz = log2(r4.xyz);
  r4.xyz = float3(0.416666657,0.416666657,0.416666657) * r4.xyz;
  r4.xyz = exp2(r4.xyz);
  r4.xyz = float3(1.05499995,1.05499995,1.05499995) * r4.xyz;
  r4.xyz = float3(-0.0549999997,-0.0549999997,-0.0549999997) + r4.xyz;
  r4.xyz = r6.xyz * r4.xyz;
  r4.xyz = r7.xyz + r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r2.x = r2.x;
  r4.xyz = r4.xyz;
  r2.x = r2.x;
  r4.xyz = r4.xyz;
  r2.x = r2.x;
  r4.xyz = r4.xyz;
  r2.x = r2.x;
  r3.w = r3.w;
  r3.w = r3.w;
  r3.w = r3.w;
  r3.w = r3.w;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r4.xyz = r4.xyz;
  r3.w = r3.w;
  r3.x = v1.w;
  r0.xy = r0.xy;
  r3.w = r3.w;
  r3.x = r3.x;
  r0.xy = r0.xy;
  r0.z = g_psSystem.m_param.w;
  r0.xy = float2(0.125,0.125) * r0.xy;
  r0.yw = g_tex_mesh.Sample(g_samplerPoint_Wrap_s, r0.xy).xw;
  r0.yw = r0.yw;
  r0.x = r0.w * r0.z;
  r0.xy = -r0.xy;
  r0.xy = r3.wx + r0.xy;
  r0.xy = cmp(r0.xy < float2(0,0));
  r0.x = (int)r0.y | (int)r0.x;
  r0.x = r0.x ? asfloat(0xFFFFFFFFu) : 0;
  if (r0.x != 0) discard;
  r0.xy = float2(0.5,0.5) * r1.yz;
  r5.xy = float2(0.5,0.5) + r0.xy;
  r0.x = 0.5 * r1.w;
  r0.x = 0.5 + r0.x;
  r5.z = sqrt(r0.x);
  r5.xy = r5.xy;
  r5.z = r5.z;
  r5.xyz = r5.xyz;
  r5.xyz = r5.xyz;
  r4.xyz = r4.xyz;
  r5.xyz = r5.xyz;
  r2.x = r2.x;
  r4.xyz = r4.xyz;
  r5.xyz = r5.xyz;
  r2.x = r2.x;
  r2.y = r2.y;
  r4.xyz = r4.xyz;
  r4.w = 1;
  r5.xyz = r5.xyz;
  r5.w = r5.w;
  r2.x = r2.x;
  r2.y = r2.y;
  r2.z = r2.z;
  r2.w = 0;
  r4.xyzw = r4.xyzw;
  r5.xyzw = r5.xyzw;
  r2.xyzw = r2.xyzw;
  o0.xyzw = r4.xyzw;
  o1.xyzw = r5.xyzw;
  o2.xyzw = r2.xyzw;
  return;
}
// clang-format on

void main(
    float4 v0 : SV_Position0,
    float4 v1 : COLOR0,
    float4 v2 : TEXCOORD0,
    float4 v3 : TEXCOORD4,
    float4 v4 : TEXCOORD5,
    float4 v5 : TEXCOORD6,
    float3 v6 : TEXCOORD7,
    out float4 o0 : SV_Target0,
    out float4 o1 : SV_Target1,
    out float4 o2 : SV_Target2,
    out float4 clean_material : SV_Target3,
    out uint4 material_owner : SV_Target4,
    out float4 specular_reset : SV_Target5,
    out float4 composition_reset : SV_Target6) {
  specular_reset = 0.0;
  composition_reset = 0.0;
  material_clean_pass = false;
  NativeMaterialMain(v0, v1, v2, v3, v4, v5, v6, o0, o1, o2);
  clean_material = float4(0.0, 0.0, 0.0, -1.0);
  material_owner = 0u;
  const uint capture = material_capture_parameters.Load(int3(0, 0, 0));
  if ((capture & 1u) == 0u) return;
  material_clean_pass = true;
  float4 clean_o0, clean_o1, clean_o2;
  NativeMaterialMain(v0, v1, v2, v3, v4, v5, v6, clean_o0, clean_o1, clean_o2);
  material_clean_pass = false;
  clean_material = float4(renodx::color::srgb::Decode(saturate(clean_o0.rgb)), 1.0);
  material_owner = uint4(PackMaterialUNorm(o0), PackMaterialUNorm(o1), PackMaterialUNorm(o2), asuint(v0.z));
}
