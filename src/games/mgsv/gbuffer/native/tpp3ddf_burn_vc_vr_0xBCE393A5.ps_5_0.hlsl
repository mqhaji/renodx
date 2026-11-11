#include "../material_capture.hlsli"

// clang-format off
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:31 2026

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
SamplerState g_sampler_SubNorm_s : register(s3);
SamplerState g_sampler_Burn_s : register(s4);
SamplerState g_sampler_Charcoal_s : register(s5);
SamplerState g_samplerPoint_Wrap_s : register(s8);
SamplerState g_samplerLinear_Wrap_s : register(s10);
Texture2D<float4> g_tex_diffuse : register(t0);
Texture2D<float4> g_tex_normal : register(t1);
Texture2D<float4> g_tex_srm : register(t2);
Texture2D<float4> g_tex_SubNorm : register(t3);
Texture2D<float4> g_tex_Burn : register(t4);
Texture2D<float4> g_tex_Charcoal : register(t5);
Texture2D<float4> g_MaterialTexture : register(t10);
TextureCube<float4> g_ReflectionTexture : register(t11);
Texture2D<float4> g_tex_mesh : register(t15);


// 3Dmigoto declarations
#define cmp -


void NativeMaterialMain(
  float4 v0 : SV_Position0,
  float4 v1 : COLOR0,
  float4 v2 : TEXCOORD0,
  float2 v3 : TEXCOORD1,
  float2 w3 : TEXCOORD9,
  float4 v4 : TEXCOORD4,
  float4 v5 : TEXCOORD5,
  float4 v6 : TEXCOORD6,
  float4 v7 : TEXCOORD7,
  float4 v8 : TEXCOORD8,
  out float4 o0 : SV_Target0,
  out float4 o1 : SV_Target1,
  out float4 o2 : SV_Target2)
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v0.xy;
  r1.z = g_psMaterial.m_materials[0].x;
  r1.z = r1.z;
  r2.xyz = g_tex_srm.Sample(g_sampler_srm_s, v2.xy).yxz;
  r2.xyz = r2.xyz;
  r2.xyz = r2.xyz;
  r3.xyzw = g_tex_diffuse.Sample(g_sampler_diffuse_s, v2.xy).xyzw;
  r3.xyzw = r3.xyzw;
  r3.xyz = r3.xyz;
  r2.x = r2.x;
  r0.z = v1.x;
  r3.xyz = r3.xyz;
  r2.x = r2.x;
  r0.z = r0.z;
  r4.xyzw = g_tex_Burn.Sample(g_sampler_Burn_s, v3.xy).xyzw;
  r5.xyz = g_tex_Charcoal.Sample(g_sampler_Charcoal_s, v3.xy).xyz;
  r5.xyz = r5.xyz;
  r0.w = -0.25 + r0.z;
  r0.w = 4 * r0.w;
  r0.w = r0.w + r4.w;
  r0.w = max(0, r0.w);
  r6.z = min(1, r0.w);
  r0.z = -0.75 + r0.z;
  r0.z = 4 * r0.z;
  r0.z = r0.z + r4.w;
  r0.z = max(0, r0.z);
  r7.xyz = min(float3(1,1,1), r0.zzz);
  r0.z = g_psMaterial.m_materials[2].y;
  r0.z = r0.z * r6.z;
  r0.z = r0.z + r2.x;
  r0.z = max(0, r0.z);
  r1.x = min(1, r0.z);
  r8.xyz = g_psMaterial.m_materials[2].xxx;
  r9.xyz = -r3.xyz;
  r4.xyz = r9.xyz + r4.xyz;
  r4.xyz = r8.xyz * r4.xyz;
  r4.xyz = r4.xyz + r3.xyz;
  r6.xyz = r6.zzz;
  r4.xyz = r4.xyz + r9.xyz;
  r4.xyz = r6.xyz * r4.xyz;
  r4.xyz = r4.xyz + r3.xyz;
  r6.yzw = -r4.xyz;
  r5.xyz = r6.yzw + r5.xyz;
  r5.xyz = r7.xyz * r5.xyz;
  r4.xyz = r5.xyz + r4.xyz;
  r4.xyz = r4.xyz;
  r1.x = r1.x;
  r6.x = r6.x;
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
  r0.xy = v2.zw;
  r6.x = r6.x;
  r0.xy = r0.xy;
  r6.x = r6.x;
  r0.xy = r0.xy;
  r0.zw = g_psMaterial.m_materials[1].yz;
  r3.xy = g_psMaterial.m_materials[1].xx * r6.xx;
  r5.xyz = v5.xyz;
  r6.xyz = v6.xyz;
  r7.xyz = v7.xyz;
  r7.xyz = float3(1,1,1) * r7.xyz;
  r3.zw = g_tex_normal.Sample(g_sampler_normal_s, v2.zw).wy;
  r3.zw = r3.zw;
  r3.zw = float2(2,2) * r3.zw;
  r8.xy = float2(-1,-1) + r3.zw;
  r3.z = r8.x * r8.x;
  r3.z = -r3.z;
  r3.z = 1 + r3.z;
  r3.w = r8.y * r8.y;
  r3.w = -r3.w;
  r3.z = r3.z + r3.w;
  r3.z = max(0, r3.z);
  r3.z = min(1, r3.z);
  r3.z = 9.99999975e-05 + r3.z;
  r3.w = rsqrt(r3.z);
  r8.z = r3.z * r3.w;
  r8.xy = r8.xy;
  r8.z = r8.z;
  r8.xyz = r8.xyz;
  r8.xyz = r8.xyz;
  r0.xy = r0.xy * r0.zw;
  r0.xy = g_tex_SubNorm.Sample(g_sampler_SubNorm_s, r0.xy).wy;
  r0.xy = r0.xy;
  r0.xy = float2(2,2) * r0.xy;
  r0.xy = float2(-1,-1) + r0.xy;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  r0.xy = float2(1,1) * r0.xy;
  r0.xy = r0.xy * r3.xy;
  r8.xy = r8.xy + r0.xy;
  r0.x = dot(r8.xyz, r8.xyz);
  r0.x = rsqrt(r0.x);
  r0.xyz = r8.xyz * r0.xxx;
  r0.w = dot(r5.xyz, r5.xyz);
  r0.w = rsqrt(r0.w);
  r3.xyz = r5.xyz * r0.www;
  r5.xyz = r0.xxx;
  r3.xyz = r5.xyz * r3.xyz;
  r0.x = dot(r6.xyz, r6.xyz);
  r0.x = rsqrt(r0.x);
  r5.xyz = r6.xyz * r0.xxx;
  r0.xyw = r0.yyy;
  r0.xyw = r5.xyz * r0.xyw;
  r0.xyw = r3.xyz + r0.xyw;
  r3.x = dot(r7.xyz, r7.xyz);
  r3.x = rsqrt(r3.x);
  r3.xyz = r7.xyz * r3.xxx;
  r5.xyz = r0.zzz;
  r3.xyz = r5.xyz * r3.xyz;
  r0.xyz = r3.xyz + r0.xyw;
  r0.w = dot(r0.xyz, r0.xyz);
  r0.w = rsqrt(r0.w);
  r0.xyz = r0.xyz * r0.www;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r0.xyz = r0.xyz;
  r4.xyz = r4.xyz;
  r0.xyz = r0.xyz;
  r3.xyz = v4.xyz;
  r1.y = r2.y;
  r1.z = r1.z;
  r2.z = r2.z;
  r4.xyz = r4.xyz;
  r0.xyz = r0.xyz;
  r3.xyz = r3.xyz;
  r5.x = r1.y;
  r1.z = r1.z;
  r2.z = r2.z;
  r6.x = r1.z;
  r6.y = 0.25;
  r7.xyz = g_MaterialTexture.SampleLevel(g_samplerPoint_Wrap_s, r6.xy, 0).xyz;
  r7.xyz = r7.xyz;
  r6.z = 0.75;
  r5.zw = g_MaterialTexture.SampleLevel(g_samplerPoint_Wrap_s, r6.xz, 0).xz;
  r5.zw = r5.zw;
  r0.w = r2.z;
  r3.w = dot(r3.xyz, r3.xyz);
  r3.w = rsqrt(r3.w);
  r3.xyz = r3.xyz * r3.www;
  r3.xyz = -r3.xyz;
  r3.xyz = r3.xyz;
  r0.xyz = r0.xyz;
  r0.w = r0.w;
  r3.w = dot(r3.xyz, r0.xyz);
  r3.w = r3.w + r3.w;
  r3.w = -r3.w;
  r6.xyz = r3.www * r0.xyz;
  r3.xyz = r6.xyz + r3.xyz;
  r6.xyz = g_psScene.m_view._m00_m10_m20 * r3.xxx;
  r3.xyw = g_psScene.m_view._m01_m11_m21 * r3.yyy;
  r3.xyw = r6.xyz + r3.xyw;
  r6.xyz = g_psScene.m_view._m02_m12_m22 * r3.zzz;
  r3.xyz = r6.xyz + r3.xyw;
  r3.xyz = float3(0,0,0) + r3.xyz;
  r3.xyz = r3.xyz;
  r3.w = -r0.w;
  r3.w = 1 + r3.w;
  r4.w = 5 * r3.w;
  r4.w = 0 + r4.w;
  r3.xyz = SampleMaterialCubeLevel(g_ReflectionTexture, g_samplerLinear_Wrap_s, r3.xyz, r4.w).xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r4.w = r5.w;
  r3.w = r4.w * r3.w;
  r0.w = r3.w + r0.w;
  r3.w = r0.w * r5.z;
  r3.xyz = r3.xyz;
  r5.zw = r0.ww * r5.ww;
  r0.w = r5.x;
  r6.x = -r0.w;
  r6.x = 1 + r6.x;
  r4.w = r6.x * r4.w;
  r5.y = r4.w + r0.w;
  r5.xy = r5.zw * r5.xy;
  r4.xyz = r4.xyz;
  r6.xyz = cmp(float3(0.0392800011,0.0392800011,0.0392800011) >= r4.xyz);
  r6.xyz = r6.xyz ? float3(1,1,1) : float3(0,0,0);
  r8.xyz = r4.xyz / float3(12.9200001,12.9200001,12.9200001);
  r8.xyz = r8.xyz * r6.xyz;
  r6.xyz = -r6.xyz;
  r6.xyz = float3(1,1,1) + r6.xyz;
  r4.xyz = float3(0.0549999997,0.0549999997,0.0549999997) + r4.xyz;
  r4.xyz = r4.xyz / float3(1.05499995,1.05499995,1.05499995);
  r4.xyz = max(float3(9.99999975e-06,9.99999975e-06,9.99999975e-06), r4.xyz);
  r4.xyz = log2(r4.xyz);
  r4.xyz = float3(2.4000001,2.4000001,2.4000001) * r4.xyz;
  r4.xyz = exp2(r4.xyz);
  r4.xyz = r6.xyz * r4.xyz;
  r4.xyz = r8.xyz + r4.xyz;
  r4.xyz = r4.xyz;
  r5.yzw = -r5.yyy;
  r5.yzw = float3(1,1,1) + r5.yzw;
  r4.xyz = r5.yzw * r4.xyz;
  r3.xyz = r5.xxx * r3.xyz;
  r3.xyz = r3.xyz * r3.www;
  r3.xyz = r3.xyz * r7.xyz;
  r3.xyz = r4.xyz + r3.xyz;
  r3.xyz = r3.xyz;
  r4.xyz = cmp(float3(0.00313080009,0.00313080009,0.00313080009) >= r3.xyz);
  r4.xyz = r4.xyz ? float3(1,1,1) : float3(0,0,0);
  r5.xyz = float3(12.9200001,12.9200001,12.9200001) * r3.xyz;
  r5.xyz = r5.xyz * r4.xyz;
  r4.xyz = -r4.xyz;
  r4.xyz = float3(1,1,1) + r4.xyz;
  r3.xyz = max(float3(9.99999975e-06,9.99999975e-06,9.99999975e-06), r3.xyz);
  r3.xyz = log2(r3.xyz);
  r3.xyz = float3(0.416666657,0.416666657,0.416666657) * r3.xyz;
  r3.xyz = exp2(r3.xyz);
  r3.xyz = float3(1.05499995,1.05499995,1.05499995) * r3.xyz;
  r3.xyz = float3(-0.0549999997,-0.0549999997,-0.0549999997) + r3.xyz;
  r3.xyz = r4.xyz * r3.xyz;
  r3.xyz = r5.xyz + r3.xyz;
  r3.xyz = r3.xyz;
  r3.xyz = r3.xyz;
  r0.xy = float2(0.5,0.5) * r0.xy;
  r2.xy = float2(0.5,0.5) + r0.yx;
  r0.x = 0.5 * r0.z;
  r0.x = 0.5 + r0.x;
  r2.w = sqrt(r0.x);
  r2.xy = r2.xy;
  r2.w = r2.w;
  r2.xyw = r2.xyw;
  r2.xyw = r2.xyw;
  r3.xyz = r3.xyz;
  r2.xyw = r2.xyw;
  r1.x = r1.x;
  r3.xyz = r3.xyz;
  r2.xyw = r2.xyw;
  r1.x = r1.x;
  r1.y = r1.y;
  r3.xyz = r3.xyz;
  r3.w = 1;
  r2.xyw = r2.xyw;
  r2.z = r2.z;
  r1.x = r1.x;
  r1.y = r1.y;
  r1.z = r1.z;
  r1.w = 0;
  r3.xyzw = r3.xyzw;
  r2.xyzw = r2.xyzw;
  r1.xyzw = r1.xyzw;
  o0.xyzw = r3.xyzw;
  o1.xyzw = r2.yxwz;
  o2.xyzw = r1.xyzw;
  return;
}
// clang-format on

void main(
    float4 v0 : SV_Position0,
    float4 v1 : COLOR0,
    float4 v2 : TEXCOORD0,
    float2 v3 : TEXCOORD1,
    float2 w3 : TEXCOORD9,
    float4 v4 : TEXCOORD4,
    float4 v5 : TEXCOORD5,
    float4 v6 : TEXCOORD6,
    float4 v7 : TEXCOORD7,
    float4 v8 : TEXCOORD8,
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
  NativeMaterialMain(v0, v1, v2, v3, w3, v4, v5, v6, v7, v8, o0, o1, o2);
  clean_material = float4(0.0, 0.0, 0.0, -1.0);
  material_owner = 0u;
  const uint capture = material_capture_parameters.Load(int3(0, 0, 0));
  if ((capture & 1u) == 0u) return;
  material_clean_pass = true;
  float4 clean_o0, clean_o1, clean_o2;
  NativeMaterialMain(v0, v1, v2, v3, w3, v4, v5, v6, v7, v8, clean_o0, clean_o1, clean_o2);
  material_clean_pass = false;
  clean_material = float4(renodx::color::srgb::Decode(saturate(clean_o0.rgb)), 1.0);
  material_owner = uint4(PackMaterialUNorm(o0), PackMaterialUNorm(o1), PackMaterialUNorm(o2), asuint(v0.z));
}
